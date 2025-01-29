import re

import yaml

from .HAKCBase import HAKCDivisionEnum, HAKCDBColumn, HAKCDBRelation, HAKCDBNode, HAKCPrintableObj


class HAKCCompilationUnit(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCCompilationUnit"
    
    def __init__(self, filename: str, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCDBNode.__init__(self, **kwargs)
        self.filename = filename

    def __eq__(self, other):
        if isinstance(other, HAKCCompilationUnit):
            return self.filename == other.filename
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def get_info_tokens(self) -> dict[str, object]:
        result = dict()
        result['filename'] = self.filename
        return result

    def get_hash_inputs(self) -> list[object]:
        return [self.filename]

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('filename', "STRING")

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return []

    @staticmethod
    def get_table_name() -> str:
        return HAKCCompilationUnit.yaml_tag[1:]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        return {
            HAKCCompilationUnit.get_primary_key(): self.filename
        }


class HAKCDivision(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = u'!HAKCDivision'
    InCompartmentTable = 'InCompartment'

    def __init__(self, division_id: int, compartment_id: int,
                 division_count: int = len(HAKCDivisionEnum) - 1, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCDBNode.__init__(self, **kwargs)
        self.division_id = division_id
        self.compartment_id = compartment_id
        self.division_count = division_count
        if 'AccessToken' in kwargs:
            self.access_token = kwargs["AccessToken"]
        else:
            self.access_token = self.compute_access_token([])

    def __eq__(self, other):
        if isinstance(other, HAKCDivision):
            return self.compartment_id == other.compartment_id and self.division_id == other.division_id
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def __lt__(self, other):
        if isinstance(other, HAKCDivision):
            return self.compartment_id < other.compartment_id and self.division_id < other.division_id
        raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')

    def get_hash_inputs(self) -> list[object]:
        return [self.division_id, self.compartment_id]

    def get_info_tokens(self) -> dict[str, object]:
        result = dict()
        result['division_id'] = self.division_id
        result['compartment_id'] = self.compartment_id
        result['access_token'] = self.access_token
        return result

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('division_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('DivisionID', 'UINT64'),
                HAKCDBColumn('AccessToken', 'UINT64')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCDivision.yaml_tag[1:]

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCDivision.InCompartmentTable, HAKCDivision, HAKCCompartment)
        ]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCDivision.get_db_table_columns()
        return {
            schema[0]: hash(self),
            schema[1]: self.division_id,
            schema[2]: self.access_token
        }

    def compute_access_token(self, allowable_accesses: list['HAKCDivision']) -> int:
        if self.division_id != HAKCDivisionEnum.NO_DIVISION.value:
            access_token = (self.compartment_id << self.division_count) | (1 << self.division_id)
            for division in allowable_accesses:
                if division.compartment_id != self.compartment_id:
                    raise RuntimeError(f'Trying to add access to Compartment {division.compartment_id} to {self}')
                access_token |= (1 << division.division_id)
        else:
            access_token = 0xFFFF
        return access_token


class HAKCCompartment(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = u'!HAKCCompartment'

    def __init__(self, compartment_id: int, division_count: int = len(HAKCDivisionEnum) - 1, **kwargs):
        yaml.YAMLObject.__init__(self)
        # should this be converted to kwargs.get?
        if 'Name' not in kwargs:
            kwargs['Name'] = str(compartment_id)
        HAKCDBNode.__init__(self, **kwargs)
        self.compartment_id = compartment_id
        self.division_count = division_count
        self.divisions = kwargs.get("Divisions", set())
        self.entry_token = self.compute_entry_token()

    def __eq__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id == other.compartment_id
        return False

    def __hash__(self):
        return self.compartment_id

    def add_division(self, division: HAKCDivision):
        self.divisions.add(division)
        self.entry_token = self.compute_entry_token()

    def __lt__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id < other.compartment_id
        raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')

    @staticmethod
    def compute_access_token(division_id: int, compartment_id: int, division_count: int) -> int:
        if division_id != HAKCDivisionEnum.NO_DIVISION.value:
            access_token = (compartment_id << division_count) | (1 << division_id)
        else:
            access_token = 0xFFFF
        return access_token

    def compute_entry_token(self) -> int:
        token = self.compartment_id << self.division_count
        for division in self.divisions:
            token |= (1 << division.division_id)
        return token

    def get_info_tokens(self) -> dict[str, object]:
        result = dict()
        result['compartment_id'] = self.compartment_id
        result['divisions'] = list()
        result['entry_token'] = self.entry_token

        for division in sorted(self.divisions):
            result['divisions'].append(division.get_info_tokens())
        return result

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('CompartmentID', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('EntryToken', 'UINT64')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCCompartment.yaml_tag[1:]

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return []

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCCompartment.get_db_table_columns()
        return {
            schema[0]: self.compartment_id,
            schema[1]: self.entry_token
        }

class HAKCType(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCType"
    unknown_type = "@UNKNOWN@"

    def __init__(self, DebugType: str, LLVMType: str, **kwargs):
        yaml.YAMLObject.__init__(self)
        if 'Name' not in kwargs:
            kwargs['Name'] = DebugType if DebugType is not None and DebugType != HAKCType.unknown_type else LLVMType
        HAKCDBNode.__init__(self, **kwargs)
        self.DebugType = DebugType
        self.LLVMType = LLVMType
        self._debug_type_transformed = HAKCType.transform_type_str(self.DebugType)
        self._debug_type_is_known = self.DebugType != HAKCType.unknown_type
        self._llvm_type_is_known = self.LLVMType != HAKCType.unknown_type

    def __eq__(self, other):
        if isinstance(other, HAKCType):
            if self._debug_type_is_known and other._debug_type_is_known:
                return self._debug_type_transformed == other._debug_type_transformed
            elif self._llvm_type_is_known and other._llvm_type_is_known:
                return self.llvm_type == other.llvm_type
            else:
                return False
        return False

    def __lt__(self, other):
        return True

    def get_hash_inputs(self) -> list[object]:
        if self._debug_type_is_known:
            return [self._debug_type_transformed]
        else:
            return [self.llvm_type]

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def get_info_tokens(self) -> dict[str, object]:
        return {'debug_type': f'{self.debug_type}', 'llvm_type': f'{self.llvm_type}'}

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
        return HAKCDBColumn('type_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('DebugType', "STRING"), HAKCDBColumn('LLVMType', 'STRING')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCType.yaml_tag[1:]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCType.get_db_table_columns()
        return {
            schema[0]: hash(self),
            schema[1]: self.DebugType,
            schema[2]: self.LLVMType
        }


class HAKCScope(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCScope"
    global_scope = "global"
    local_scope = "local"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        self.scope = kwargs['Scope'] if 'Scope' in kwargs else None
        if 'Name' not in kwargs:
            name = kwargs['LocalScopeName'] if 'LocalScopeName' in kwargs else self.scope
            kwargs['Name'] = name
        HAKCDBNode.__init__(self, **kwargs)
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
        return HAKCDBNode.__hash__(self)

    def __lt__(self, other):
        if isinstance(other, HAKCScope):
            if self.is_local_scope and other.is_local_scope:
                return self.local_scope_name < other.local_scope_name
            else:
                return self.scope < other.scope
        elif isinstance(other, HAKCType):
            return True
        # I guess at some point a HAKCType is being compared to HAKCScope
        print(f"{other} is of type: {type(other)}")
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

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('scope_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('Scope', "STRING"), HAKCDBColumn('LocalScopeName', "STRING")]

    @staticmethod
    def get_table_name() -> str:
        return HAKCScope.yaml_tag[1:]

    def get_db_data(self) -> dict[HAKCDBColumn, object]:
        schema = HAKCScope.get_db_table_columns()
        return {
            schema[0]: hash(self),
            schema[1]: self.scope,
            schema[2]: self.local_scope_name
        }


class HAKCSymbol(HAKCDBNode):
    yaml_tag = "!HAKCSymbol"

    IsTypeTable = "IsType"
    HasScopeTable = "HasScope"
    UsesSymbolTable = "UsesSymbol"
    SymbolCompilationUnitTable = "UsedInCompilationUnit"
    DagEdgeTable = "DagEdge"
    InDivisionTable = "InDivision"
    DefinedInTable = "DefinedIn"

    def __init__(self, Name: str, Type: HAKCType, Scope: HAKCScope, **kwargs):
        HAKCDBNode.__init__(self, **kwargs)
        self.Name = Name
        self.Type = Type
        self.Scope = Scope
        self.DefiningFile = kwargs['DefiningFile'] if 'DefiningFile' in kwargs else None
        self.DefiningLine = kwargs['DefiningLine'] if 'DefiningLine' in kwargs else None

        self.UsedSymbols = kwargs['UsedSymbols'] if 'UsedSymbols' in kwargs else list()

    def __eq__(self, other):
        if isinstance(other, HAKCSymbol):
            return hash(self) == hash(other)
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def get_hash_inputs(self) -> list[object]:
        return [self.Name, self.Type, self.Scope]

    def get_info_tokens(self) -> dict[str, object]:
        return {
            'name': self.Name,
            'type': self.Type,
            'scope': self.Scope,
            'definition': f'{self.defining_file + ":" + str(self.defining_line) if self.is_definition else "None"}'
        }

    @property
    def is_definition(self):
        return self.defining_file is not None

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('symbol_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('DefiningFile', 'STRING'),
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
        schema = HAKCSymbol.get_db_table_columns()
        return {
            schema[0]: hash(self),
            schema[1]: self.DefiningFile,
            schema[2]: self.DefiningLine,
            schema[3]: isinstance(self, HAKCFunction),
            # schema[4]: self.name
            schema[4]: self.Name
        }


class HAKCIndirectSourceLink(HAKCPrintableObj, yaml.YAMLObject):
    yaml_tag = "!HAKCIndirectSourceLink"

    def __init__(self, LinkType: str, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCPrintableObj.__init__(self, **kwargs)
        self.LinkType = LinkType
        self.Type = kwargs['Type'] if 'Type' in kwargs else None
        self.GlobalName = kwargs['GlobalName'] if 'GlobalName' in kwargs else None
        self.Offset = kwargs['Offset'] if 'Offset' in kwargs else None
        self.ArgNumber = kwargs['ArgNumber'] if 'ArgNumber' in kwargs else None
        self.FunctionName = kwargs['Function'] if 'Function' in kwargs else None

    def get_hash_inputs(self) -> list[object]:
        result = [self.LinkType]
        if self.Type:
            result.append(self.Type)
        if self.GlobalName:
            result.append(self.GlobalName)
        if self.Offset:
            result.append(self.Offset)
        if self.ArgNumber:
            result.append(self.ArgNumber)
        if self.FunctionName:
            result.append(self.FunctionName)
        return result


class HAKCIndirectCallSource(HAKCPrintableObj, yaml.YAMLObject):
    yaml_tag = "!HAKCIndirectSource"

    def __init__(self, Type: HAKCType, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCPrintableObj.__init__(self, **kwargs)
        self.Source = kwargs['Source'] if 'Source' in kwargs else list()
        self.Type = Type

    def get_hash_inputs(self) -> list[object]:
        result = [self.Type]
        for link in self.Source:
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

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCFunction.IndirectCallTable, HAKCFunction, HAKCType),
            HAKCDBRelation(HAKCFunction.DirectCallTable, HAKCFunction, HAKCSymbol)
        ]

    def get_info_tokens(self) -> dict[str, object]:
        tokens = HAKCSymbol.get_info_tokens(self)
        tokens["directCall"] = self.direct_calls
        tokens["indirectCall"] = self.indirect_calls
        return tokens


class HAKCGlobalVariable(yaml.YAMLObject, HAKCSymbol):
    yaml_tag = "!HAKCGlobalVariable"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCSymbol.__init__(self, **kwargs)

    def get_info_tokens(self) -> dict[str, object]:
        tokens = HAKCSymbol.get_info_tokens(self)
        tokens["is_global"] = True
        return tokens


class HAKCAdjustment(yaml.YAMLObject):
    yaml_tag = "!HAKCAdjustment"

    def __init__(self, path: str, division_id: int, compartment_id: int):
        yaml.YAMLObject.__init__(self)
        self.path = path
        self.division = HAKCDivision(division_id, compartment_id)


class HAKCCompartmentalizationAdjustment(yaml.YAMLObject):
    yaml_tag = "!HAKCAdjustments"
    compartmentalize_entry = 'compartmentalize'
    add_kernel_compartment_entry = 'add-kernel-division'

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        self.adjustment_regexes = dict()
        self.add_kernel_division = kwargs.get(HAKCCompartmentalizationAdjustment.add_kernel_compartment_entry, False)
        for adjustment in sorted(kwargs.get(HAKCCompartmentalizationAdjustment.compartmentalize_entry, set()),
                                 key=lambda e: e.path):
            escaped_path = re.escape(adjustment.path)
            self.adjustment_regexes[re.compile(escaped_path)] = adjustment

    def get_adjusted_compartment(self, defining_path: str) -> HAKCDivision | None:
        if defining_path is None:
            return None

        adjusted_division = None
        for adjustment_regex, adjustment in self.adjustment_regexes.items():
            match = adjustment_regex.search(defining_path)
            if match:
                adjusted_division = adjustment.division

        return adjusted_division


def construct_adjustment(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCAdjustment:
    return HAKCAdjustment(**loader.construct_mapping(node, deep=True))


def construct_adjustments(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCCompartmentalizationAdjustment:
    return HAKCCompartmentalizationAdjustment(**loader.construct_mapping(node, deep=True))


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


def construct_compilation_unit(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCCompilationUnit:
    return HAKCCompilationUnit(**loader.construct_mapping(node, deep=True))


def construct_division(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCDivision:
    return HAKCDivision(**loader.construct_mapping(node, deep=True))


def construct_compartment(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCCompartment:
    return HAKCCompartment(**loader.construct_mapping(node, deep=True))


def construct_symbol(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCSymbol:
    return HAKCSymbol(**loader.construct_mapping(node, deep=True))

HAKCObject_constructors = {
    HAKCGlobalVariable.yaml_tag: construct_global_variable,
    HAKCFunction.yaml_tag: construct_function,
    HAKCIndirectCallSource.yaml_tag: construct_indirect_call_source,
    HAKCIndirectSourceLink.yaml_tag: construct_indirect_call_source_link,
    HAKCType.yaml_tag: construct_type,
    HAKCScope.yaml_tag: construct_scope,
    HAKCAdjustment.yaml_tag: construct_adjustment,
    HAKCCompartmentalizationAdjustment.yaml_tag: construct_adjustments,
    HAKCCompilationUnit.yaml_tag: construct_compilation_unit,
    HAKCDivision.yaml_tag: construct_division,
    HAKCCompartment.yaml_tag: construct_compartment,
    HAKCSymbol.yaml_tag: construct_symbol
}
