import logging
from enum import Enum

import networkx as nx
from hakc.yaml.HAKCObjects import HAKCSymbol, HAKCType

logger = logging.getLogger('hakc-dag')


class CliqueColors(Enum):
    NO_CLIQUE = -1
    SILVER_CLIQUE = 0  # SIL
    GREEN_CLIQUE = 1  # GRN
    RED_CLIQUE = 2  # RED
    ORANGE_CLIQUE = 3  # ORN
    YELLOW_CLIQUE = 4  # YEL
    PURPLE_CLIQUE = 5  # PUR
    BLUE_CLIQUE = 6  # BLU
    GREY_CLIQUE = 7  # GRY
    PINK_CLIQUE = 8  # PNK
    BROWN_CLIQUE = 9  # BWN
    WHITE_CLIQUE = 10  # WHI
    BLACK_CLIQUE = 11  # BLK
    TEAL_CLIQUE = 12  # TEA
    VIOLET_CLIQUE = 13  # VLT
    CRIMSON_CLIQUE = 14  # CRI
    GOLD_CLIQUE = 15  # GLD


class HAKCCompartmentalization(nx.DiGraph):
    # Node attributes
    cu_attr = 'compilation-units'
    compartment_id_attr = 'compartment-id'
    color_attr = 'color'
    defining_compilation_unit_attr = 'defining-compilation-unit'

    # Edge attributes
    indirect_call_attr = 'indirect-call'
    isa_attr = 'is-a'
    uses_attr = 'uses'
    dag_attr = 'dag'

    kernel_compartment_id = 0
    kernel_color = CliqueColors.SILVER_CLIQUE

    def __init__(self):
        super().__init__(self)

    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int):
        if dag_edge_weight > 0:
            edge_attrs = {HAKCCompartmentalization.dag_attr: dag_edge_weight}
            self.add_edge(head, tail, **edge_attrs)

    def has_symbol_name(self, symbol: HAKCSymbol) -> bool:
        same_name_graph = self.get_symbols_by_name(symbol.name)
        return len(same_name_graph) > 0

    def get_symbols_by_name(self, name: str):
        return nx.subgraph_view(self, filter_node=lambda n, sym_name=name: n.is_symbol() and n.name == sym_name)

    def get_symbol(self, symbol: HAKCSymbol) -> HAKCSymbol:
        symbol_graph = nx.subgraph_view(self, filter_node=lambda n, s=symbol: n == s)

        for sym in symbol_graph:
            return sym

        return None

    def merge_symbol(self, symbol: HAKCSymbol):
        if symbol.is_definition:
            existing_symbol = self.get_symbol(symbol)
            existing_symbol.merge_symbol(symbol)

    def add_symbol(self, symbol: HAKCSymbol, compilation_unit: str):
        if self.has_node(symbol):
            self.merge_symbol(symbol)
        else:
            type_attrs = {HAKCCompartmentalization.isa_attr: True}
            self.add_edge(symbol, symbol.type, **type_attrs)
            self.nodes[symbol][HAKCCompartmentalization.color_attr] = CliqueColors.NO_CLIQUE

        if HAKCCompartmentalization.cu_attr not in self.nodes[symbol]:
            self.nodes[symbol][HAKCCompartmentalization.cu_attr] = set()
        self.nodes[symbol][HAKCCompartmentalization.cu_attr].add(compilation_unit)

    def finalize_symbols(self):
        compartment_id = 0
        indirect_edge_attrs = {HAKCCompartmentalization.indirect_call_attr: True}
        symbol_attrs = {HAKCCompartmentalization.uses_attr: True}

        symbols = set()
        for symbol in self.get_symbols():
            symbols.add(symbol)

        for symbol in symbols:
            compartment_id += 1

            self.nodes[symbol][HAKCCompartmentalization.compartment_id_attr] = compartment_id

            for used_symbol in symbol.used_symbols:
                self.add_edge(symbol, used_symbol, **symbol_attrs)

            if symbol.is_function():
                for indirect_call in symbol.indirect_calls:
                    self.add_edge(symbol, indirect_call.type, **indirect_edge_attrs)

    def set_compartment_id(self, symbol: HAKCSymbol, compartment_id: int):
        self.nodes[symbol][HAKCCompartmentalization.compartment_id_attr] = compartment_id

    def get_compartment_id(self, symbol: HAKCSymbol) -> int:
        return self.nodes[symbol][HAKCCompartmentalization.compartment_id_attr]

    def set_color(self, symbol: HAKCSymbol, color: CliqueColors):
        self.nodes[symbol][HAKCCompartmentalization.color_attr] = color

    def get_color(self, symbol: HAKCSymbol) -> CliqueColors:
        return self.nodes[symbol][HAKCCompartmentalization.color_attr]

    def get_referenced_compilation_units(self, symbol: HAKCSymbol) -> set[str]:
        return self.nodes[symbol][HAKCCompartmentalization.cu_attr]

    def get_types(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_type()).nodes

    def get_functions(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_function()).nodes

    def get_global_variables(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_global_variable()).nodes

    def get_symbols(self):
        return nx.subgraph_view(self, filter_node=lambda n: n.is_symbol())

    def to_yaml(self) -> dict:
        result = dict()
        result['COMPARTMENTS'] = list()
        result['SYMBOLS'] = list()

        compartment_targets = dict()
        compartment_colors = dict()
        compartment_ids = set()
        compartment_symbols = dict()
        for node, node_attrs in self.nodes.items():
            if not node.is_type():
                compartment_id = node_attrs[HAKCCompartmentalization.compartment_id_attr]
                compartment_ids.add(compartment_id)
                color = node_attrs[HAKCCompartmentalization.color_attr]
                if compartment_id not in compartment_targets:
                    compartment_targets[compartment_id] = set()
                if compartment_id not in compartment_colors:
                    compartment_colors[compartment_id] = set()
                compartment_colors[compartment_id].add(color)
                if compartment_id not in compartment_symbols:
                    compartment_symbols[compartment_id] = list()

                compartment_symbol = dict()
                compartment_symbol['NAME'] = node.name
                compartment_symbol['CLIQUE'] = color.name
                compartment_symbol['COMPARTMENT'] = compartment_id
                compartment_symbols[compartment_id].append(compartment_symbol)

                for nbr, _ in self.adj[node].items():
                    if not nbr.is_type():
                        target_compartment = self.nodes[nbr][HAKCCompartmentalization.compartment_id_attr]
                        compartment_targets[compartment_id].add(target_compartment)

        for compartment_id in sorted(compartment_ids):
            compartment_yaml = dict()
            compartment_yaml['ID'] = compartment_id
            compartment_yaml['CLIQUES'] = list()
            for color in sorted(compartment_colors[compartment_id]):
                if color is not CliqueColors.NO_CLIQUE:
                    access_token = (compartment_id << 16) | (1 << color.value)
                else:
                    access_token = 0xFFFF
                compartment_yaml['CLIQUES'].append({'COLOR': color.name,
                                                    'ACCESS_TOKEN': access_token})
            compartment_yaml['TARGETS'] = list()
            for target in sorted(compartment_targets[compartment_id]):
                compartment_yaml['TARGETS'].append(target)

            result['COMPARTMENTS'].append(compartment_yaml)

            for compartment_symbol in sorted(compartment_symbols[compartment_id], key=lambda x: x['NAME']):
                result['SYMBOLS'].append(compartment_symbol)

        return result
