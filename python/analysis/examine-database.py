import argparse

from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCObjects import HAKCSymbol, HAKCType


def main():
    parser = argparse.ArgumentParser(description='Database Explorer')
    parser.add_argument('--db-dir', help='Directory to use for the kuzu database', dest='db_dir',
                        required=True)
    parser.add_argument('--type-limit', help='Number of types to retrieve from the database', dest='type_limit',
                        default=20)

    args = parser.parse_args()

    db = HAKCDatabase(args.db_dir, read_only=True)

    info_to_print = dict()

    info_to_print['Code Duplication Candidates'] = db.execute(
        f'MATCH (s:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_dag}]->(s0:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->(t:{HAKCType.get_table_name()}) '
        f'WHERE NOT (s0)-[:{HAKCSymbol.relation_dag}]->() AND s0.IsFunction AND t.DebugType <> "{HAKCType.unknown_type}" '
        f'RETURN DISTINCT s0.Name, count(e) AS DagCount ORDER BY DagCount DESC')

    for title, data in info_to_print.items():
        print(f'{title}:\n{data}')


if __name__ == "__main__":
    main()
