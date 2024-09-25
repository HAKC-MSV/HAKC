import logging

import kuzu
import networkx as nx
import polars as pl
import tqdm
from hakc.yaml.HAKCObjects import HAKCSymbol, HAKCType, CliqueColors, HAKCDivision, \
    HAKCCompartment, HAKCInfo, HAKCScope, HAKCCompilationUnit, HAKCFunction, HAKCDBColumn

logger = logging.getLogger('hakc-dag')


class HAKCCompartmentalization(nx.MultiDiGraph):
    kernel_compartment_id = 0
    kernel_division = CliqueColors.NO_CLIQUE.value
    default_division = CliqueColors.TEAL_CLIQUE.value
    persisted_attr = 'persisted'

    def __init__(self, division_count=16):
        super().__init__(self)
        self.division_count = division_count

    @classmethod
    def CreateFromDatabase(cls, db_dir: str):
        compartmentalization = cls.__init__()

        return compartmentalization

    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int):
        if dag_edge_weight > 0:
            self.add_persistent_edge(head, tail, key=HAKCSymbol.DagEdgeTable, weight=dag_edge_weight)

    def add_persistent_node(self, node):
        if not self.has_node(node):
            attrs = {HAKCCompartmentalization.persisted_attr: False}
            self.add_node(node, **attrs)

    def add_persistent_edge(self, u_for_edge, v_for_edge, key=None, **attr):
        self.add_persistent_node(u_for_edge)
        self.add_persistent_node(v_for_edge)
        if not self.has_edge(u_for_edge, v_for_edge, key):
            attr[HAKCCompartmentalization.persisted_attr] = False
            self.add_edge(u_for_edge, v_for_edge, key, **attr)

    def add_symbol(self, symbol: HAKCSymbol, compilation_unit: str):
        self.add_persistent_edge(symbol, symbol.type, key=HAKCSymbol.IsTypeTable)
        self.add_persistent_edge(symbol, symbol.scope, key=HAKCSymbol.HasScopeTable)
        self.add_persistent_edge(symbol, HAKCCompilationUnit(Name=compilation_unit),
                                 key=HAKCSymbol.SymbolCompilationUnitTable)
        if symbol.defining_file is not None:
            self.add_persistent_edge(symbol, HAKCCompilationUnit(Name=symbol.defining_file),
                                     key=HAKCSymbol.DefinedInTable, line=symbol.defining_line)

        for used_symbol in symbol.used_symbols:
            self.add_persistent_edge(symbol, used_symbol, key=HAKCSymbol.UsesSymbolTable)

        if symbol.is_function():
            for indirect_call in symbol.indirect_calls:
                self.add_persistent_edge(symbol, indirect_call.type, key=HAKCSymbol.IndirectCallsTable)

    def _get_neighbors(self, symbol: HAKCSymbol, edge_key: str) -> list:
        nbrs = list()
        for nbr, edges in self.adj[symbol].items():
            if edge_key in edges:
                nbrs.append(nbr)
        return nbrs

    def get_indirect_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        return self._get_neighbors(symbol, HAKCSymbol.IndirectCallsTable)

    def set_division(self, symbol: HAKCSymbol, division_id: int, compartment_id: int):
        division = HAKCDivision(division_id, compartment_id)
        compartment = HAKCCompartment(compartment_id)
        self.add_persistent_edge(division, compartment, key=HAKCDivision.DivisionCompartmentTable)
        self.add_persistent_edge(symbol, division, key=HAKCSymbol.InDivisionTable)

    def get_division(self, symbol: HAKCSymbol) -> HAKCDivision:
        nbrs = self._get_neighbors(symbol, HAKCSymbol.InDivisionTable)
        if len(nbrs) == 0:
            return None
        elif len(nbrs) > 1:
            logger.error(f'Symbol {symbol} is in {len(nbrs)} divisions.')
            for division in nbrs:
                logger.error(f'{division}')
            raise RuntimeError(f'Symbol {symbol} is in {len(nbrs)} divisions.')
        else:
            return nbrs[0]

    def get_types(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_type()).nodes

    def get_functions(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_function()).nodes

    def get_global_variables(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_global_variable()).nodes

    def get_symbols(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_symbol())

    def get_scopes(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_scope())

    def get_compilation_units(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_compilation_unit())

    def get_unpersisted_nodes(self) -> dict[str, list[HAKCInfo]]:
        result = dict()
        for node, is_persisted in self.nodes(data=HAKCCompartmentalization.persisted_attr):
            if not is_persisted:
                table_name = node.get_table_name()
                if table_name not in result:
                    result[table_name] = list()
                result[table_name].append(node)
        return result

    def get_unpersisted_edges(self) -> dict[str, list[tuple[HAKCInfo, HAKCInfo, dict]]]:
        result = dict()
        for head, tail, table_name, attrs in self.edges(data=True, keys=True):
            if not attrs[HAKCCompartmentalization.persisted_attr]:
                del attrs[HAKCCompartmentalization.persisted_attr]
                if table_name not in result:
                    result[table_name] = list()
                result[table_name].append((head, tail, attrs))

        return result

    def _persist_nodes(self, conn: kuzu.Connection):
        unpersisted_nodes = self.get_unpersisted_nodes()
        for table_name, nodes in unpersisted_nodes.items():
            data_to_persist = dict()
            for node in nodes:
                for column, data in node.get_db_data().items():
                    if column.column_name not in data_to_persist:
                        data_to_persist[column.column_name] = list()
                    data_to_persist[column.column_name].append(data)
            df = pl.DataFrame(data_to_persist)
            logger.info(f'Persisting {len(nodes)} Nodes to {table_name}')
            try:
                conn.execute(f'COPY {table_name} FROM df')
                for node in nodes:
                    self.nodes[node][HAKCCompartmentalization.persisted_attr] = True
            except Exception as e:
                logger.error(f'Failed to persist to {table_name}: {str(e)}')
            logger.info('Done')

    def _persist_edges(self, conn: kuzu.Connection):
        unpersisted_edges = self.get_unpersisted_edges()
        for table_name, edge_data in unpersisted_edges:
            for head, tail, attrs in edge_data:



    def persist_to_database(self, conn: kuzu.Connection):
        self._persist_nodes(conn)
        self._persist_edges(conn)


    def _execute_prepared_stmt(self, conn: kuzu.Connection, prepared_stmt: str, **kwargs):
        response = conn.execute(prepared_stmt, parameters=kwargs)
        return response

    def create_node_table(self, conn: kuzu.Connection, table_name: str, primary_key: HAKCDBColumn,
                          columns: list[HAKCDBColumn]):
        if primary_key not in columns:
            raise RuntimeError(f'Primary key {primary_key} not provided')
        member_str = ",".join([" ".join([column.column_name, column.db_type]) for column in columns])
        create_cmd = f'CREATE NODE TABLE IF NOT EXISTS {table_name}({member_str}, PRIMARY KEY ({primary_key.column_name}))'
        self._execute_prepared_stmt(conn, create_cmd)

    def create_rel_table(self, conn: kuzu.Connection, table_name: str, from_name: str, to_name: str, **kwargs):
        create_cmd = f'CREATE REL TABLE IF NOT EXISTS {table_name}(FROM {from_name} TO {to_name}'
        prop_str = ",".join([" ".join([key, val]) for key, val in kwargs.items()])
        if len(prop_str) != 0:
            create_cmd = ",".join([create_cmd, prop_str])
        create_cmd += ")"

        self._execute_prepared_stmt(conn, create_cmd)

    def create_schema(self, conn: kuzu.Connection):
        node_tables = [
            HAKCType,
            HAKCScope,
            HAKCSymbol,
            HAKCCompartment,
            HAKCDivision,
            HAKCCompilationUnit,
            HAKCFunction
        ]
        for cls in node_tables:
            primary_key = cls.get_primary_key()
            schema = cls.get_db_table_schema()
            self.create_node_table(conn, table_name=cls.get_table_name(), primary_key=primary_key, columns=schema)

        for cls in node_tables:
            db_relations = cls.get_db_relations()
            for db_relation in db_relations:
                self.create_rel_table(conn, db_relation.relation_name, db_relation.from_class.get_table_name(),
                                      db_relation.to_class.get_table_name(), **db_relation.properties)

    def get_symbol_hashes(self) -> list[int]:
        symbol_hashes = []
        for symbol in self.get_symbols():
            symbol_hashes.append(hash(symbol))
        return sorted(symbol_hashes)

    def get_symbol_by_hash(self, symbol_hash: int) -> HAKCSymbol | None:
        for symbol in self.get_symbols():
            if hash(symbol) == symbol_hash:
                return symbol

        return None

    def to_yaml(self):
        raise NotImplementedError
