import re
from enum import Enum
import hashlib

import yaml


class QuotedString(str):
    pass


class CliqueColors(Enum):
    NO_CLIQUE = 0
    SILVER_CLIQUE = 1  # SIL
    GREEN_CLIQUE = 2  # GRN
    RED_CLIQUE = 3  # RED
    ORANGE_CLIQUE = 4  # ORN
    YELLOW_CLIQUE = 5  # YEL
    PURPLE_CLIQUE = 6  # PUR
    BLUE_CLIQUE = 7  # BLU
    GREY_CLIQUE = 8  # GRY
    PINK_CLIQUE = 9  # PNK
    BROWN_CLIQUE = 10  # BWN
    WHITE_CLIQUE = 11  # WHI
    BLACK_CLIQUE = 12  # BLK
    TEAL_CLIQUE = 13  # TEA
    VIOLET_CLIQUE = 14  # VLT
    CRIMSON_CLIQUE = 15  # CRI
    GOLD_CLIQUE = 16  # GLD


def get_bytes(value) -> bytes:
    if isinstance(value, str):
        return value.encode('utf-8')
    elif isinstance(value, int):
        return value.to_bytes(8, byteorder='big')
    else:
        hash_value = hash(value)
        return hash_value.to_bytes(8, byteorder='big')

def hash_values(values: list) -> int:
    h = hashlib.sha256(get_bytes(values[0]))
    if len(values) > 1:
        for value in values[1:]:
            h.update(get_bytes(value))
    return int.from_bytes(h.digest()[:8], 'big')

class HAKCPrintableObj:
    def __init__(self, **kwargs):
        self.computed_hash = None

    def __str__(self):
        cls = self.__class__.__name__
        inside_strings = [f'{key}={str(value)}' for key, value in self.get_info_tokens().items()]
        return f'{cls}({", ".join(sorted(inside_strings))})'

    def get_info_tokens(self) -> dict[str, object]:
        raise NotImplementedError

    def to_yaml_dict(self) -> dict[str, object]:
        result = dict()
        for key, value in self.get_info_tokens().items():
            if isinstance(value, HAKCPrintableObj):
                result[key] = value.to_yaml_dict()
            elif isinstance(value, str):
                result[key] = QuotedString(value)
            else:
                result[key] = value
        return result

    def __hash__(self):
        if self.computed_hash is not None:
            return self.computed_hash
        self.computed_hash = hash_values(self.get_hash_inputs())
        return self.computed_hash

    def get_hash_inputs(self):
        raise NotImplementedError


class HAKCDBRelation:
    EdgePropertyName = 'EdgeData'

    def __init__(self, relation_name: str, from_class, to_class, **kwargs):
        self.relation_name = relation_name
        self.from_class = from_class
        self.to_class = to_class
        self.properties = None
        if len(kwargs) > 0:
            struct_def = ",".join([" ".join([name, db_type]) for name, db_type in kwargs.items()])
            self.properties = struct_def
            # self.properties = f'{HAKCDBRelation.EdgePropertyName} STRUCT({struct_def})'


class HAKCDBColumn:
    def __init__(self, column_name: str, db_type: str):
        self.column_name = column_name
        self.db_type = db_type

    def __hash__(self):
        return hash_values([self.column_name, self.db_type])

    def __str__(self):
        return f'{self.column_name}'

    def __eq__(self, other):
        if isinstance(other, HAKCDBColumn):
            return self.column_name == other.column_name
        return False


class HAKCInfo(HAKCPrintableObj):
    def __init__(self, Name: str, **kwargs):
        HAKCPrintableObj.__init__(self, **kwargs)
        self.name = str(Name)

    def __eq__(self, other):
        if isinstance(other, HAKCInfo):
            return other.name == self.name

        return False

    def __hash__(self):
        return HAKCPrintableObj.__hash__(self)

    def get_hash_inputs(self) -> list[object]:
        return [self.name]

    def is_function(self) -> bool:
        return False

    def is_global_variable(self) -> bool:
        return False

    def is_type(self) -> bool:
        return False

    def is_scope(self) -> bool:
        return False

    def is_compilation_unit(self) -> bool:
        return False

    def is_symbol(self) -> bool:
        return self.is_function() or self.is_global_variable()

    def get_info_tokens(self) -> dict[str, object]:
        return {'name': f'{self.name}'}

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        raise NotImplementedError

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        raise NotImplementedError

    @staticmethod
    def get_db_table_schema() -> list[HAKCDBColumn]:
        raise NotImplementedError

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return []

    @staticmethod
    def get_table_name() -> str:
        raise NotImplementedError

    def get_primary_key_data(self) -> object:
        primary_key = self.get_primary_key()
        for column, data in self.get_db_data().items():
            if column == primary_key:
                return data
        raise RuntimeError(
            f'Could not find data for primary key {primary_key} in object for table {self.get_table_name()}')


class HAKCCompilationUnit(HAKCInfo, yaml.YAMLObject):
    yaml_tag = "!HAKCCompilationUnit"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCInfo.__init__(self, **kwargs)

    def is_compilation_unit(self) -> bool:
        return True

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('filename', "STRING")

    @staticmethod
    def get_db_table_schema() -> list[HAKCDBColumn]:
        return [HAKCCompilationUnit.get_primary_key()]

    @staticmethod
    def get_table_name() -> str:
        return HAKCCompilationUnit.yaml_tag[1:]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        return {
            HAKCCompilationUnit.get_primary_key(): self.name
        }


class HAKCCompartment(yaml.YAMLObject, HAKCInfo):
    yaml_tag = u'!HAKCCompartment'
    DefaultDivisionCount = 16
    TargetTable = 'Target'

    def __init__(self, compartment_id: int, division_count: int, **kwargs):
        yaml.YAMLObject.__init__(self)
        if 'Name' not in kwargs:
            kwargs['Name'] = str(compartment_id)
        HAKCInfo.__init__(self, **kwargs)
        self.compartment_id = compartment_id
        self.division_count = division_count
        self.targets = set()
        self.divisions = dict()
        self.entry_token = self.compute_entry_token()

    def __eq__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id == other.compartment_id
        return False

    def __hash__(self):
        return HAKCInfo.__hash__(self)

    def get_hash_inputs(self) -> list[object]:
        return [self.compartment_id]

    def __lt__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id < other.compartment_id
        raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')

    @staticmethod
    def compute_access_token(division_id: int, compartment_id: int, division_count: int) -> int:
        if division_id != CliqueColors.NO_CLIQUE.value:
            access_token = (compartment_id << division_count) | (1 << division_id)
        else:
            access_token = 0xFFFF
        return access_token

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

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('CompartmentID', 'INT32')

    @staticmethod
    def get_db_table_schema() -> list[HAKCDBColumn]:
        return [HAKCCompartment.get_primary_key(), HAKCDBColumn('EntryToken', 'INT64')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCCompartment.yaml_tag[1:]

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCCompartment.TargetTable, HAKCCompartment, HAKCCompartment)
        ]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCCompartment.get_db_table_schema()
        return {
            schema[0]: self.compartment_id,
            schema[1]: self.entry_token
        }


class HAKCDivision(yaml.YAMLObject, HAKCInfo):
    yaml_tag = u'!HAKCDivision'
    InDivisionTable = "InDivision"

    def __init__(self, division_id: int, compartment_id: int,
                 division_count: int = HAKCCompartment.DefaultDivisionCount, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCPrintableObj.__init__(self, **kwargs)
        self.division_id = division_id
        self.compartment_id = compartment_id
        self.division_count = division_count

    def get_hash_inputs(self) -> list[object]:
        return [self.division_id, self.compartment_id]

    def __eq__(self, other):
        if isinstance(other, HAKCDivision):
            return self.compartment_id == other.compartment_id and self.division_id == other.division_id
        return False

    def __hash__(self):
        return HAKCInfo.__hash__(self)

    def __lt__(self, other):
        if isinstance(other, HAKCDivision):
            return self.compartment_id < other.compartment_id and self.division_id < other.division_id
        raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')

    def get_info_tokens(self) -> dict[str, object]:
        result = dict()
        result['division_id'] = self.division_id
        result['compartment_id'] = self.compartment_id
        result['access_token'] = HAKCCompartment.compute_access_token(self.division_id, self.compartment_id,
                                                                      self.division_count)
        return result

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('AccessToken', 'UINT64')

    @staticmethod
    def get_db_table_schema() -> list[HAKCDBColumn]:
        return [HAKCDivision.get_primary_key(), HAKCDBColumn('DivisionID', 'UINT32')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCDivision.yaml_tag[1:]

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCDivision.InDivisionTable, HAKCDivision, HAKCCompartment)
        ]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCDivision.get_db_table_schema()
        return {
            schema[0]: HAKCCompartment.compute_access_token(self.division_id, self.compartment_id, self.division_count),
            schema[1]: self.division_id
        }


class HAKCType(HAKCInfo, yaml.YAMLObject):
    yaml_tag = "!HAKCType"
    unknown_type = "@UNKNOWN@"

    def __init__(self, DebugType: str, LLVMType: str, **kwargs):
        yaml.YAMLObject.__init__(self)
        if 'Name' not in kwargs:
            kwargs['Name'] = DebugType if DebugType is not None and DebugType != HAKCType.unknown_type else LLVMType
        HAKCInfo.__init__(self, **kwargs)
        self.debug_type = DebugType
        self.llvm_type = LLVMType
        self._debug_type_transformed = HAKCType.transform_type_str(self.debug_type)
        self._debug_type_is_known = self.debug_type != HAKCType.unknown_type
        self._llvm_type_is_known = self.llvm_type != HAKCType.unknown_type

    def __eq__(self, other):
        if isinstance(other, HAKCType):
            if self._debug_type_is_known and other._debug_type_is_known:
                return self._debug_type_transformed == other._debug_type_transformed
            elif self._llvm_type_is_known and other._llvm_type_is_known:
                return self.llvm_type == other.llvm_type
            else:
                return False
        return False

    def get_hash_inputs(self) -> list[object]:
        if self._debug_type_is_known:
            return [self._debug_type_transformed]
        else:
            return [self.llvm_type]

    def __hash__(self):
        return HAKCInfo.__hash__(self)

    def get_info_tokens(self) -> dict[str, object]:
        return {'debug_type': f'{self.debug_type}', 'llvm_type': f'{self.llvm_type}'}

    def is_type(self) -> bool:
        return True

    @staticmethod
    def transform_type_str(type_str: str) -> str:
        transforms = {
            "struct anon.[0-9]*": "struct anon.#",
        }

        result = type_str
        for regex, sub in transforms.items():
            result = re.sub(regex, sub, result)

        return result

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('type_hash', 'INT64')

    @staticmethod
    def get_db_table_schema() -> list[HAKCDBColumn]:
        return [HAKCType.get_primary_key(), HAKCDBColumn('DebugType', "STRING"), HAKCDBColumn('LLVMType', 'STRING')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCType.yaml_tag[1:]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCType.get_db_table_schema()
        return {
            schema[0]: hash(self),
            schema[1]: self.debug_type,
            schema[2]: self.llvm_type
        }


class HAKCScope(yaml.YAMLObject, HAKCInfo):
    yaml_tag = "!HAKCScope"
    global_scope = "global"
    local_scope = "local"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        if 'Name' not in kwargs:
            name = kwargs['LocalScopeName'] if 'LocalScopeName' in kwargs else kwargs['Scope']
            kwargs['Name'] = name
        HAKCPrintableObj.__init__(self, **kwargs)
        self.scope = kwargs['Scope'] if 'Scope' in kwargs else None
        self.local_scope_name = kwargs['LocalScopeName'] if 'LocalScopeName' in kwargs else HAKCScope.global_scope
        self.is_global_scope = self.scope == HAKCScope.global_scope
        self.is_local_scope = self.scope == HAKCScope.local_scope

    def __eq__(self, other):
        if isinstance(other, HAKCScope):
            if self.is_local_scope:
                return other.is_local_scope and self.local_scope_name == other.local_scope_name
            else:
                return other.is_global_scope
        return False

    def __hash__(self):
        return HAKCInfo.__hash__(self)

    def __lt__(self, other):
        if isinstance(other, HAKCScope):
            if self.is_local_scope and other.is_local_scope:
                return self.local_scope_name < other.local_scope_name
            else:
                return self.scope < other.scope
        raise RuntimeError(f'{other} is not a {self.__class__.__name__}')

    def get_hash_inputs(self) -> list[object]:
        if self.is_global_scope:
            return [self.scope]
        else:
            return [self.scope, self.local_scope_name]

    def get_info_tokens(self) -> dict[str, object]:
        result = {'scope': f'{self.scope}'}
        if self.is_local_scope:
            result['local_scope_name'] = self.local_scope_name
        return result

    def is_scope(self) -> bool:
        return True

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('scope_hash', 'INT64')

    @staticmethod
    def get_db_table_schema() -> list[HAKCDBColumn]:
        return [HAKCScope.get_primary_key(), HAKCDBColumn('Scope', "STRING"), HAKCDBColumn('LocalScopeName', "STRING")]

    @staticmethod
    def get_table_name() -> str:
        return HAKCScope.yaml_tag[1:]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCScope.get_db_table_schema()
        return {
            schema[0]: hash(self),
            schema[1]: self.scope,
            schema[2]: self.local_scope_name
        }


class HAKCSymbol(HAKCInfo):
    IsTypeTable = "IsType"
    HasScopeTable = "HasScope"
    UsesSymbolTable = "UsesSymbol"
    SymbolCompilationUnitTable = "UsedInCompilationUnit"
    DagEdgeTable = "DagEdge"
    InDivisionTable = "InDivision"
    DefinedInTable = "DefinedIn"

    def __init__(self, Type: HAKCType, Scope: HAKCScope, **kwargs):
        HAKCInfo.__init__(self, **kwargs)
        self.type = Type
        self.scope = Scope
        self.defining_file = None
        self.defining_line = None

        self.defining_file = kwargs['DefiningFile'] if 'DefiningFile' in kwargs else None
        self.defining_line = kwargs['DefiningLine'] if 'DefiningLine' in kwargs else None

        self.used_symbols = kwargs['UsedSymbols'] if 'UsedSymbols' in kwargs else list()

    def __eq__(self, other):
        if isinstance(other, HAKCSymbol):
            return hash(self) == hash(other)
        return False

    def __hash__(self):
        return HAKCInfo.__hash__(self)

    def get_hash_inputs(self) -> list[object]:
        return [self.name, self.type, self.scope]

    def get_info_tokens(self) -> dict[str, object]:
        result = HAKCInfo.get_info_tokens(self)
        result['type'] = self.type
        result['scope'] = self.scope
        result['definition'] = f'{self.defining_file + ":" + str(self.defining_line) if self.is_definition else "None"}'
        return result

    @property
    def is_definition(self):
        return self.defining_file is not None

    def clear(self):
        self.used_symbols.clear()

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('symbol_hash', 'INT64')

    @staticmethod
    def get_db_table_schema() -> list[HAKCDBColumn]:
        return [HAKCSymbol.get_primary_key(), HAKCDBColumn('DefiningFile', 'STRING'),
                HAKCDBColumn('DefiningLine', 'INT32'), HAKCDBColumn('is_function', 'BOOL'),
                HAKCDBColumn('Name', 'STRING')]

    @staticmethod
    def get_table_name() -> str:
        return "HAKCSymbol"

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCSymbol.IsTypeTable, HAKCSymbol, HAKCType),
            HAKCDBRelation(HAKCSymbol.HasScopeTable, HAKCSymbol, HAKCScope),
            HAKCDBRelation(HAKCSymbol.UsesSymbolTable, HAKCSymbol, HAKCSymbol),
            HAKCDBRelation(HAKCSymbol.SymbolCompilationUnitTable, HAKCSymbol, HAKCCompilationUnit),
            HAKCDBRelation(HAKCSymbol.DagEdgeTable, HAKCSymbol, HAKCSymbol, weight="INT32"),
            HAKCDBRelation(HAKCSymbol.InDivisionTable, HAKCSymbol, HAKCDivision),
            HAKCDBRelation(HAKCSymbol.DefinedInTable, HAKCSymbol, HAKCCompilationUnit, line="INT64")
        ]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCSymbol.get_db_table_schema()
        return {
            schema[0]: hash(self),
            schema[1]: self.defining_file,
            schema[2]: self.defining_line,
            schema[3]: self.is_function(),
            schema[4]: self.name
        }


class HAKCIndirectSourceLink(yaml.YAMLObject, HAKCInfo):
    yaml_tag = "!HAKCIndirectSourceLink"

    def __init__(self, LinkType: str, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCInfo.__init__(self, **kwargs)
        self.link_type = LinkType
        self.type = kwargs['Type'] if 'Type' in kwargs else None
        self.global_name = kwargs['GlobalName'] if 'GlobalName' in kwargs else None
        self.offset = kwargs['Offset'] if 'Offset' in kwargs else None
        self.arg_num = kwargs['ArgNumber'] if 'ArgNumber' in kwargs else None
        self.function_name = kwargs['Function'] if 'Function' in kwargs else None

    def get_hash_inputs(self) -> list[object]:
        result = [self.link_type]
        if self.type:
            result.append(self.type)
        if self.global_name:
            result.append(self.global_name)
        if self.offset:
            result.append(self.offset)
        if self.arg_num:
            result.append(self.arg_num)
        if self.function_name:
            result.append(self.function_name)
        return result


class HAKCIndirectCallSource(yaml.YAMLObject, HAKCInfo):
    yaml_tag = "!HAKCIndirectSource"

    def __init__(self, Type: HAKCType, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCInfo.__init__(self, **kwargs)
        self.source = kwargs['Source'] if 'Source' in kwargs else list()
        self.type = Type

    def get_hash_inputs(self) -> list[object]:
        result = [self.type]
        for link in self.source:
            result.append(link)
        return result


class HAKCFunction(yaml.YAMLObject, HAKCSymbol):
    yaml_tag = "!HAKCFunction"
    IndirectCallTable = "IndirectCall"
    DirectCallTable = "DirectCall"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCSymbol.__init__(self, **kwargs)
        self.direct_calls = kwargs['DirectCalls'] if 'DirectCalls' in kwargs else list()
        self.indirect_calls = kwargs['IndirectCalls'] if 'IndirectCalls' in kwargs else list()

    def is_function(self) -> bool:
        return True

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCFunction.IndirectCallTable, HAKCFunction, HAKCType),
            HAKCDBRelation(HAKCFunction.DirectCallTable, HAKCFunction, HAKCSymbol)
        ]


class HAKCGlobalVariable(yaml.YAMLObject, HAKCSymbol):
    yaml_tag = "!HAKCGlobalVariable"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCSymbol.__init__(self, **kwargs)

    def is_global_variable(self) -> bool:
        return True


def construct_scope(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCScope:
    return HAKCScope(**loader.construct_mapping(node, deep=True))


def construct_global_variable(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCGlobalVariable:
    return HAKCGlobalVariable(**loader.construct_mapping(node, deep=True))


def construct_function(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCFunction:
    return HAKCFunction(**loader.construct_mapping(node, deep=True))


def construct_indirect_call_source(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCIndirectCallSource:
    return HAKCIndirectCallSource(**loader.construct_mapping(node, deep=True))


def construct_indirect_call_source_link(loader: yaml.SafeLoader,
                                        node: yaml.nodes.MappingNode) -> HAKCIndirectSourceLink:
    return HAKCIndirectSourceLink(**loader.construct_mapping(node, deep=True))


def construct_type(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCType:
    return HAKCType(**loader.construct_mapping(node, deep=True))


HAKCObject_constructors = {
    HAKCGlobalVariable.yaml_tag: construct_global_variable,
    HAKCFunction.yaml_tag: construct_function,
    HAKCIndirectCallSource.yaml_tag: construct_indirect_call_source,
    HAKCIndirectSourceLink.yaml_tag: construct_indirect_call_source_link,
    HAKCType.yaml_tag: construct_type,
    HAKCScope.yaml_tag: construct_scope,
}
