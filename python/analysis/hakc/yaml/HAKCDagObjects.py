import logging
from enum import Enum

import networkx as nx
import yaml
from hakc.yaml.HAKCObjects import HAKCSymbol, HAKCType, HAKCPrintableObj

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


class HAKCCompartment(yaml.YAMLObject, HAKCPrintableObj):
    yaml_tag = u'!HAKCCompartment'

    def __init__(self, compartment_id: int, division_count: int, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCPrintableObj.__init__(self, **kwargs)
        self.compartment_id = compartment_id
        self.division_count = division_count
        self.targets = set()
        self.divisions = dict()
        self.entry_token = self.compute_entry_token()

    def __eq__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id == other.compartment_id
        raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')

    def __hash__(self):
        return hash(self.compartment_id)

    def add_target(self, target_compartment: int):
        self.targets.add(target_compartment)

    def add_division(self, division_id: int):
        if division_id >= self.division_count:
            raise RuntimeError(
                f'Attempted to add an invalid division id {division_id} when the max division id is {self.division_count - 1}')

        if division_id != CliqueColors.NO_CLIQUE.value:
            access_token = (self.compartment_id << self.division_count) | (1 << division_id)
        else:
            access_token = 0xFFFF

        self.divisions[division_id] = access_token
        self.entry_token = self.compute_entry_token()

    def compute_entry_token(self) -> int:
        token = self.compartment_id << self.division_count
        for division_id in self.divisions.keys():
            token |= (1 << division_id)
        return token

    def get_info_tokens(self) -> dict[str, object]:
        result = dict()
        result['compartment_id'] = self.compartment_id
        result['targets'] = sorted(list(self.targets))
        result['divisions'] = list()
        result['entry_token'] = self.entry_token

        for division in sorted(self.divisions.keys()):
            access_token = self.divisions[division]
            division_dict = dict()
            division_dict['division_id'] = division
            division_dict['access_token'] = access_token
            result['divisions'].append(division_dict)
        return result


class HAKCCompartmentalization(nx.DiGraph):
    # Node attributes
    compartment_id_attr = 'compartment-id'
    division_attr = 'division-id'
    defining_compilation_unit_attr = 'defining-compilation-unit'

    # Edge attributes
    indirect_call_attr = 'indirect-call'
    isa_attr = 'is-a'
    uses_attr = 'uses'
    dag_attr = 'dag'

    kernel_compartment_id = 0
    kernel_division = CliqueColors.NO_CLIQUE.value
    default_division = CliqueColors.TEAL_CLIQUE.value

    def __init__(self, division_count=16):
        super().__init__(self)
        self.division_count = division_count

    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int):
        if dag_edge_weight > 0:
            edge_attrs = {HAKCCompartmentalization.dag_attr: dag_edge_weight}
            self.add_edge(head, tail, **edge_attrs)

    def has_symbol_name(self, symbol: HAKCSymbol) -> bool:
        same_name_graph = self.get_symbols_by_name(symbol.name)
        return len(same_name_graph) > 0

    def get_symbols_by_name(self, name: str):
        return nx.subgraph_view(self, filter_node=lambda n, sym_name=name: n.is_symbol() and n.name == sym_name)

    def add_symbol(self, symbol: HAKCSymbol):
        type_attrs = {HAKCCompartmentalization.isa_attr: True}
        self.add_edge(symbol, symbol.type, **type_attrs)
        self.nodes[symbol][HAKCCompartmentalization.division_attr] = HAKCCompartmentalization.default_division

    def get_indirect_calls(self, symbol: HAKCSymbol) -> set[HAKCType]:
        indirect_calls = set()
        for nbr, edgeattr in self.adj[symbol].items():
            if HAKCCompartmentalization.indirect_call_attr in edgeattr:
                indirect_calls.add(nbr)
        return indirect_calls

    def finalize_symbols(self):
        compartment_id = 0
        indirect_edge_attrs = {HAKCCompartmentalization.indirect_call_attr: True}
        symbol_attrs = {HAKCCompartmentalization.uses_attr: True}

        symbols = set()
        for symbol in self.get_symbols():
            symbols.add(symbol)

        for symbol in symbols:
            for used_symbol in symbol.used_symbols:
                if not self.has_node(used_symbol):
                    self.add_symbol(used_symbol)
                self.add_edge(symbol, used_symbol, **symbol_attrs)

            if symbol.is_function():
                for indirect_call in symbol.indirect_calls:
                    self.add_edge(symbol, indirect_call.type, **indirect_edge_attrs)

        for symbol in self.get_symbols():
            compartment_id += 1
            self.nodes[symbol][HAKCCompartmentalization.compartment_id_attr] = compartment_id

    def set_compartment_id(self, symbol: HAKCSymbol, compartment_id: int):
        self.nodes[symbol][HAKCCompartmentalization.compartment_id_attr] = compartment_id

    def get_compartment_id(self, symbol: HAKCSymbol) -> int:
        return self.nodes[symbol][HAKCCompartmentalization.compartment_id_attr]

    def set_division_id(self, symbol: HAKCSymbol, division_id: int):
        self.nodes[symbol][HAKCCompartmentalization.division_attr] = division_id

    def get_division_id(self, symbol: HAKCSymbol) -> int:
        return self.nodes[symbol][HAKCCompartmentalization.division_attr]

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
        result['FILES'] = list()

        compartments = dict()
        compilation_unit_symbols = dict()
        for symbol in self.get_symbols():
            compartment_id = self.nodes[symbol][HAKCCompartmentalization.compartment_id_attr]
            color = self.nodes[symbol][HAKCCompartmentalization.division_attr]
            if compartment_id not in compartments:
                compartments[compartment_id] = HAKCCompartment(compartment_id, self.division_count)

            current_compartment = compartments[compartment_id]
            current_compartment.add_division(color)

            for nbr, _ in self.adj[symbol].items():
                if not nbr.is_type():
                    if HAKCCompartmentalization.compartment_id_attr not in self.nodes[nbr]:
                        error_message = f'Neighbor {nbr} of {symbol} does not have a compartment ID'
                        logger.error(error_message)
                        logger.error("Symbols with the same name:")
                        for sym in self.get_symbols_by_name(nbr.name):
                            logger.error(f'\t{sym}')
                        raise ValueError(error_message)
                    target_compartment = self.nodes[nbr][HAKCCompartmentalization.compartment_id_attr]
                    current_compartment.add_target(target_compartment)

            for compilation_unit in symbol.compilation_units:
                if compilation_unit not in compilation_unit_symbols:
                    compilation_unit_symbols[compilation_unit] = set()

                compilation_unit_symbols[compilation_unit].add(symbol)

        for compartment_id in sorted(compartments.keys()):
            current_compartment = compartments[compartment_id]
            result['COMPARTMENTS'].append(current_compartment.to_yaml_dict())

        for compilation_unit in sorted(compilation_unit_symbols.keys()):
            compilation_unit_dict = dict()
            compilation_unit_dict['file'] = compilation_unit
            compilation_unit_dict['symbols'] = list()
            for symbol in sorted(list(compilation_unit_symbols[compilation_unit]), key=lambda s: s.name):
                symbol_dict = symbol.to_yaml_dict()
                symbol_dict['compartment_id'] = self.nodes[symbol][HAKCCompartmentalization.compartment_id_attr]
                symbol_dict['division_id'] = self.nodes[symbol][HAKCCompartmentalization.division_attr]
                compilation_unit_dict['symbols'].append(symbol_dict)
            result['FILES'].append(compilation_unit_dict)

        return result
