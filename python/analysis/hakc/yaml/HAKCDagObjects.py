import networkx as nx
from typing import Collection
from hakc.yaml.HAKCObjects import HAKCSymbol, HAKCType


class HAKCCompartmentalization(nx.DiGraph):

    def add_symbol(self, symbol: HAKCSymbol) -> HAKCSymbol:
        self.add_edge(symbol, symbol.type)
        for used_symbol in symbol.used_symbols:
            self.add_edge(symbol, used_symbol)
        return self.nodes[symbol]

    def get_types(self) -> Collection[HAKCType]:
        node_filter = lambda n: n.is_type()
        return nx.subgraph_view(self, filter_node=node_filter).nodes

    def get_functions(self) -> Collection[HAKCSymbol]:
        node_filter = lambda n: n.is_function()
        return nx.subgraph_view(self, filter_node=node_filter).nodes

    def get_global_variables(self) -> Collection[HAKCSymbol]:
        node_filter = lambda n: n.is_global_variable()
        return nx.subgraph_view(self, filter_node=node_filter).nodes
