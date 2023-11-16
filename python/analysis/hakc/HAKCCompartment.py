import os
from enum import Enum

from .StructInfo import StructInfo
from .SymbolInfo import SymbolInfo


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


class HAKCCompartment:
    def __init__(self):
        self.compartment_id = None
        self.symbols_accessed = dict()
        self.types_accessed = set()
        self.color = CliqueColors.CRIMSON_CLIQUE

    def get_compartment_id(self) -> int:
        # This attempts to fix a bug that set the compartment_id to be an HAKCCompartment
        if not isinstance(self.compartment_id, int):
            self.compartment_id = self.compartment_id.get_compartment_id()
        return self.compartment_id

    def set_compartment_id(self, uid: int):
        self.compartment_id = uid

    def defines_symbol(self, symbol: SymbolInfo):
        if symbol in self.symbols_accessed:
            return self.symbols_accessed[symbol]
        return False

    def accessed_type(self, struct_type: StructInfo):
        return struct_type in self.types_accessed

    def accesses_symbol(self, symbol: SymbolInfo):
        return symbol in self.symbols_accessed

    def add_defined_symbol(self, symbol: SymbolInfo):
        self.symbols_accessed[symbol] = True

    def add_symbol_access(self, symbol: SymbolInfo):
        self.symbols_accessed[symbol] = False

    def add_type_access(self, struct_type: StructInfo):
        self.types_accessed.add(struct_type)

    def get_accessed_symbols(self) -> set:
        result = set()
        for symbol, _ in self.symbols_accessed.items():
            result.add(symbol)
        return result

    def get_accessed_types(self) -> set:
        return self.types_accessed

    def get_definition_sources(self) -> set:
        result = set()
        for symbol, is_defined_here in self.symbols_accessed.items():
            if is_defined_here:
                full_path = symbol.get_definition_file()
                result.add(full_path)
        return result

    def contains_symbol_defined_in_path(self, path):
        for definition_src in self.get_definition_sources():
            if path in definition_src:
                return True
        return False

    def __hash__(self):
        return hash(self.compartment_id)

    def __eq__(self, other):
        if not isinstance(other, HAKCCompartment):
            return False

        return self.compartment_id == other.get_compartment_id()

    def __str__(self):
        clique_data = dict()
        result = "===========================================\n"
        result += f"ID: {self.compartment_id}\n"
        result += "Definition Sources:\n"
        for source in sorted(self.get_definition_sources()):
            result += f"\t{source}\n"
        result += "==========================================="
        return result

    def merge_compartment_data(self, compartment: 'HAKCCompartment'):
        if compartment is self:
            return
        for symbol, is_defined_in_compartment in compartment.symbols_accessed.items():
            is_already_defined = False
            if symbol in self.symbols_accessed:
                for existing_symbol, is_already_defined in self.symbols_accessed.items():
                    if existing_symbol == symbol:
                        break
                existing_symbol.merge_symbol_data(symbol)
                symbol = existing_symbol
            self.symbols_accessed[symbol] = is_defined_in_compartment or is_already_defined
        for ty in compartment.types_accessed:
            if ty in self.types_accessed:
                for existing_ty in self.types_accessed:
                    if existing_ty == ty:
                        break
                existing_ty.merge_struct_info(ty)
                self.types_accessed.remove(existing_ty)
                ty = existing_ty
            self.types_accessed.add(ty)

    def to_yaml(self) -> tuple:
        compartment_yaml = dict()
        files_yaml = list()
        clique_info = dict()

        if self.get_compartment_id() > 0:
            entry_token = (self.get_compartment_id() << 16) | (1 << self.color.value)
            clique_info[self.color] = entry_token
        else:
            entry_token = 0xFFFF
            clique_info[self.color] = entry_token

        compartment_yaml['ENTRY_TOKEN'] = entry_token
        compartment_yaml['ID'] = self.get_compartment_id()
        compartment_yaml['CLIQUES'] = list()
        for color, access_token in clique_info.items():
            compartment_yaml['CLIQUES'].append({'COLOR': color.name,
                                                'ACCESS_TOKEN': access_token})

        file_info_map = dict()
        definition_sources = self.get_definition_sources()
        for symbol in [s for s, is_defined_here in self.symbols_accessed.items() if is_defined_here]:
            src = symbol.get_full_definition_file()
            if src in definition_sources:
                if src not in file_info_map:
                    file_info = dict()
                    file_info['PATH'] = src
                    file_info['GUID'] = 0
                    file_info['SYMBOLS'] = list()
                    file_info_map[src] = file_info
                    files_yaml.append(file_info)
                file_info_map[src]['SYMBOLS'].append({
                    'CLIQUE': self.color.name,
                    'COMPARTMENT': self.compartment_id,
                    'NAME': symbol.get_name(),
                    'IS_GLOBAL': 'Yes' if symbol.is_valid_decl_linkage() else "No"
                })

        return compartment_yaml, sorted(files_yaml, key=lambda x: x['PATH'])
