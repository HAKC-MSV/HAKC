import argparse
import logging
import shutil

from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger
from hakc.HAKCObjects import HAKCSymbol, HAKCType, HAKCDefinitionLocation

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

    edges_to_add = dict()
    existing_primary_key_map = dict()
    hakc_objects = [HAKCSymbol, HAKCType, HAKCDefinitionLocation]
    for _, row in symbols_to_duplicate.iterrows():
        symbol_hash = row['SymbolHash']
        duplicated_symbol_name = row['Name']
        incoming_nodes = db.execute(
            f'MATCH (n)-[e]->(s0:{HAKCSymbol.get_table_name()}) '
            f'WHERE s0.{str(HAKCSymbol.get_primary_key())} = {symbol_hash} '
            f'RETURN n AS Node, Label(e) AS EdgeType')
        outgoing_nodes = db.execute(
            f'MATCH (s0:{HAKCSymbol.get_table_name()})-[e]->(n) '
            f'WHERE s0.{str(HAKCSymbol.get_primary_key())} = {symbol_hash} '
            f'RETURN n AS Node, e AS edge')
        logger.info(f'Incoming to {row['Name']}\n{incoming_nodes}')
        logger.info(f'Outgoing from {row['Name']}\n{outgoing_nodes}')
        current_id = 0
        for _, row in outgoing_nodes.iterrows():
            edge_data = row['edge']
            logger.info(f'edge_data={edge_data}')
            for column in HAKCSymbol.get_db_relations():
                if column.relation_name == edge_data['_label']:
                    data = row['Node']
                    logger.info(f'Creating a {column.to_class.__name__} object from {data}')
                    node = column.to_class(**data)
                    logger.info(f'Created {node}')

        for _, data in incoming_nodes.iterrows():
            node_info = data['Node']
            edge_type = data['EdgeType']
            node_primary_key = None
            for hakc_obj in hakc_objects:
                if hakc_obj.__name__ == node_info['_label']:
                    node_primary_key = node_info[str(hakc_obj.get_primary_key())]
                    break
            if node_primary_key:
                if node_primary_key not in existing_primary_key_map:
                    new_name = f"{duplicated_symbol_name}_{current_id}"
                    current_id += 1


if __name__ == "__main__":
    main()
