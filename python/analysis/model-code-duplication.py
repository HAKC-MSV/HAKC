import argparse
import logging
import shutil

import pandas as pd
from hakc.HAKCCompartmentalization import HAKCCompartmentalization
from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger
from hakc.HAKCObjects import HAKCSymbol, HAKCType, HAKCDefinitionLocation, HAKCScope, HAKCFunction, HAKCDivision, \
    HAKCCompartment

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')


def construct_node_dataframe(hakc_objects: list) -> pd.DataFrame:
    symbol_data_to_persist = dict()
    for hakc_object in hakc_objects:
        for column, data in hakc_object.get_db_data().items():
            if data is None:
                logger.debug(f'Node {hakc_object} has None for column {column.column_name}')
                data = column.column_type.default_value
            if column.column_name not in symbol_data_to_persist:
                symbol_data_to_persist[column.column_name] = list()
            symbol_data_to_persist[column.column_name].append(data)
    return pd.DataFrame(symbol_data_to_persist)


def main():
    parser = argparse.ArgumentParser(description='Models code duplication in Compartmentalization')
    parser.add_argument('--db-dir', help='Directory to use for the kuzu database', dest='db_dir',
                        required=True)
    parser.add_argument('--duplication-count', '-c', help='Number of functions to duplicate', dest='duplication_count',
                        required=True)
    parser.add_argument('--dest-db-dir', help='Path to store new database', dest='dest_db_dir')
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-mode', default='w', dest='log_mode')
    args = parser.parse_args()
    setup_logging(logger, log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)

    dest_db_dir = args.dest_db_dir if args.dest_db_dir else args.db_dir + f"-duplication-{args.duplication_count}"
    shutil.copytree(args.db_dir, dest_db_dir, dirs_exist_ok=True)
    db = HAKCDatabase(dest_db_dir)

    symbols_to_duplicate = db.execute(
        f'MATCH (s:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_dag}]->(s0:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->(t:{HAKCType.get_table_name()}) '
        f'WHERE NOT (s0)-[:{HAKCSymbol.relation_dag}]->() AND s0.IsFunction AND t.DebugType <> "{HAKCType.unknown_type}" '
        f'WITH s0, count(e) AS DagCount '
        f'RETURN DISTINCT s0.Name AS Name, s0.symbol_hash as SymbolHash, s0.IsFunction AS IsFunction, DagCount '
        f'ORDER BY DagCount DESC LIMIT {args.duplication_count}')

    logger.info(f'Duplicating\n{symbols_to_duplicate}')

    hakc_objects = [HAKCSymbol]
    for _, row in symbols_to_duplicate.iterrows():
        existing_primary_key_map = dict()
        symbol_hash = row['SymbolHash']
        duplicated_symbol_name = row['Name']
        incoming_nodes = db.execute(
            f'MATCH (n)-[e]->(s0:{HAKCSymbol.get_table_name()}) '
            f'WHERE s0.{str(HAKCSymbol.get_primary_key())} = {symbol_hash} '
            f'RETURN n AS Node, e AS edge')
        outgoing_nodes = db.execute(
            f'MATCH (s0:{HAKCSymbol.get_table_name()})-[e]->(n) '
            f'WHERE s0.{str(HAKCSymbol.get_primary_key())} = {symbol_hash} '
            f'RETURN n AS Node, e AS edge')
        logger.debug(f'Incoming to {row['Name']}\n{incoming_nodes}')
        logger.debug(f'Outgoing from {row['Name']}\n{outgoing_nodes}')
        current_id = 0

        edges_to_add = dict()
        # Tuple of (HAKCFunction Parameter name, Default Value, Edge Data Name dict)
        hakc_symbol_input_map = {HAKCFunction.relation_scope: ("Scope", None, {}),
                                 HAKCFunction.relation_type: ("Type", None, {}),
                                 HAKCFunction.relation_definition_location: ("DefinitionLocation", None,
                                                                             {'DefiningLine': None}),
                                 HAKCFunction.relation_symbol: ("UsedSymbols", [], {}),
                                 HAKCFunction.relation_direct_calls: ("DirectCalls", [], {}),
                                 HAKCFunction.relation_indirect_calls: ("IndirectCalls", [], {})}

        for relation, _ in hakc_symbol_input_map.items():
            edges_to_add[relation] = {"from": [], "to": []}

        for _, row in outgoing_nodes.iterrows():
            edge_data = row['edge']
            logger.debug(f'edge_data={edge_data}')
            for relation in HAKCSymbol.get_db_relations():
                edge_type = edge_data['_label']
                if relation.relation_name == edge_type and edge_type in hakc_symbol_input_map:
                    data = row['Node']
                    logger.debug(f'Creating a {relation.to_class.__name__} object from {data}')
                    node = relation.to_class(**data)
                    logger.debug(f'Created {node}')
                    input_name, default_value, edge_values = hakc_symbol_input_map[edge_type]
                    new_edge_values = {}
                    for edge_value_name, edge_value in edge_values.items():
                        if edge_value_name in edge_data:
                            new_edge_values[edge_value_name] = edge_data[edge_value_name]

                    if default_value is None:
                        hakc_symbol_input_map[edge_type] = (input_name, node, new_edge_values)
                    else:
                        hakc_symbol_input_map[edge_type] = (input_name, default_value.append(node), new_edge_values)

        logger.info(f'Handling {len(incoming_nodes)} incoming nodes to {duplicated_symbol_name}')
        for node_info, edge_data in zip(incoming_nodes['Node'], incoming_nodes['edge']):
            node_primary_key = None
            for hakc_obj in hakc_objects:
                if hakc_obj.__name__ == node_info['_label']:
                    node_primary_key = node_info[str(hakc_obj.get_primary_key())]
                    break
            if node_primary_key:
                if node_primary_key not in existing_primary_key_map:
                    new_name = f"{duplicated_symbol_name}_{current_id}"
                    current_id += 1
                    hakc_symbol_inputs = {t[0]: t[1] for _, t in hakc_symbol_input_map.items()}
                    hakc_symbol_inputs["Name"] = new_name
                    logger.debug(f"Creating duplicated HAKCFunction with inputs {hakc_symbol_inputs}")
                    duplicated_symbol = HAKCFunction(**hakc_symbol_inputs)
                    logger.debug(f"Created duplicated symbol {duplicated_symbol}")
                    existing_primary_key_map[node_primary_key] = duplicated_symbol

                    for relation, relation_data in hakc_symbol_input_map.items():
                        target_object = relation_data[1]
                        if target_object is not None:
                            if isinstance(target_object, list):
                                for item in target_object:
                                    edges_to_add[relation]['from'].append(duplicated_symbol.get_primary_key_data())
                                    edges_to_add[relation]['to'].append(item.get_primary_key_data())
                            else:
                                edges_to_add[relation]['from'].append(duplicated_symbol.get_primary_key_data())
                                edges_to_add[relation]["to"].append(target_object.get_primary_key_data())
                        for key, value in relation_data[2].items():
                            if key not in edges_to_add[relation]:
                                edges_to_add[relation][key] = []
                            edges_to_add[relation][key].append(value)
                duplicated_symbol = existing_primary_key_map[node_primary_key]
                edge_type = edge_data['_label']
                if edge_type not in edges_to_add:
                    edges_to_add[edge_type] = {"from": [], "to": []}
                edges_to_add[edge_type]["from"].append(node_primary_key)
                edges_to_add[edge_type]["to"].append(duplicated_symbol.get_primary_key_data())
                for edge_data_name, edge_data_value in edge_data.items():
                    if edge_data_name[0] != '_' and edge_data_value is not None:
                        edges_to_add[edge_type][edge_data_name] = edge_data_value

        duplicated_symbols = [duplicated_symbol for _, duplicated_symbol in existing_primary_key_map.items()]
        logger.info(f'Deleting {duplicated_symbol_name} from database')
        delete_query = f'MATCH (s:{HAKCSymbol.get_table_name()}) WHERE s.{str(HAKCSymbol.get_primary_key())} = $symbol_hash DETACH DELETE s'
        db.execute(delete_query, symbol_hash=symbol_hash)
        logger.info(f'Persisting {len(existing_primary_key_map)} duplicated symbols')
        df = construct_node_dataframe(duplicated_symbols)
        db.insert_from_dataframe(HAKCSymbol.get_table_name(), df)
        logger.info(f'Persisting {len(edges_to_add)} edge set')
        for relation, relation_data in edges_to_add.items():
            df = pd.DataFrame(relation_data)
            try:
                db.insert_from_dataframe(relation, df)
            except Exception as e:
                logger.error(f'Failed to persist {relation}: {e}')
                raise e

    logger.info(f'There are {len(db.get_all_compartments())} Compartments prior to deletion')
    logger.info(f'Deleting all divisions and compartments')
    db.delete_all_compartments()
    logger.info(f'Creating default compartmentalization')
    compartment_id = 1
    symbol_to_division_edges = {"from": [], "to": []}
    division_to_compartment_edges = {"from": [], "to": []}
    divisions = list()
    compartments = list()
    for symbol_hash in db.get_all_symbol_hashes():
        division = HAKCDivision(DivisionID=1)
        divisions.append(division)
        compartment = HAKCCompartment(CompartmentID=compartment_id)
        compartment_id += 1
        compartments.append(compartment)
        symbol_to_division_edges['from'].append(symbol_hash)
        symbol_to_division_edges['to'].append(division.get_primary_key_data())
        division_to_compartment_edges['from'].append(division.get_primary_key_data())
        division_to_compartment_edges['to'].append(compartment.get_primary_key_data())
    df = construct_node_dataframe(compartments)
    logger.info(f'Creating {len(compartments)} compartments')
    db.insert_from_dataframe(HAKCCompartment.get_table_name(), df)
    df = construct_node_dataframe(divisions)
    logger.info(f'Creating {len(divisions)} divisions')
    db.insert_from_dataframe(HAKCDivision.get_table_name(), df)
    df = pd.DataFrame(symbol_to_division_edges)
    logger.info(f'Adding {len(symbol_to_division_edges['from'])} symbol to division edges')
    db.insert_from_dataframe(HAKCSymbol.relation_division, df)
    df = pd.DataFrame(division_to_compartment_edges)
    logger.info(f'Adding {len(division_to_compartment_edges['from'])} division to compartment edges')
    db.insert_from_dataframe(HAKCDivision.relation_compartment, df)
    logger.info(
        f'Finished persisting database to {dest_db_dir}. There are now {len(db.get_all_compartments())} compartments.')


if __name__ == "__main__":
    main()
