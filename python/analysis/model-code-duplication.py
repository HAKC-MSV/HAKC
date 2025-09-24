import argparse
import logging
import shutil

from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger
from hakc.HAKCObjects import HAKCSymbol, HAKCType, HAKCDefinitionLocation, HAKCScope, HAKCFunction

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')


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

    existing_primary_key_map = dict()
    hakc_objects = [HAKCSymbol]
    for _, row in symbols_to_duplicate.iterrows():
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
        outgoing_primary_keys = dict()
        edges_to_add = dict()
        # Tuple of (HAKCFunction Parameter name, Default Value, Edge Data Name List)
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

        for _, data in incoming_nodes.iterrows():
            node_info = data['Node']
            edge_data = data['edge']
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
                    logger.info(f"Creating duplicated HAKCFunction with inputs {hakc_symbol_inputs}")
                    duplicated_symbol = HAKCFunction(**hakc_symbol_inputs)
                    logger.info(f"Created duplicated symbol {duplicated_symbol}")
                    existing_primary_key_map[node_primary_key] = duplicated_symbol

                    for relation, relation_data in hakc_symbol_input_map.items():
                        target_object = relation_data[1]
                        if target_object is not None:
                            if isinstance(target_object, list):
                                for item in target_object:
                                    edges_to_add[relation]['from'].append(hash(duplicated_symbol))
                                    edges_to_add[relation]['to'].append(hash(item))
                            else:
                                edges_to_add[relation]['from'].append(hash(duplicated_symbol))
                                edges_to_add[relation]["to"].append(hash(target_object))


if __name__ == "__main__":
    main()
