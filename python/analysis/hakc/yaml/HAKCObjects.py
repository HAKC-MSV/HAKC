import re

import yaml


class HAKCPrintableObj:
    def __init__(self, **kwargs):
        pass

    def __str__(self):
        cls = self.__class__.__name__
        return f'{cls}({", ".join(self.get_info_tokens())})'

    def get_info_tokens(self) -> list[str]:
        raise NotImplementedError


class HAKCInfo(HAKCPrintableObj):
    def __init__(self, Name: str, **kwargs):
        HAKCPrintableObj.__init__(self, **kwargs)
        self.name = Name

    def __eq__(self, other):
        if isinstance(other, HAKCInfo):
            return other.name == self.name

        return False

    def __hash__(self):
        return hash(self.name)

    def is_function(self) -> bool:
        return False

    def is_global_variable(self) -> bool:
        return False

    def is_type(self) -> bool:
        return False

    def is_symbol(self) -> bool:
        return self.is_function() or self.is_global_variable()

    def get_info_tokens(self) -> list[str]:
        return [f'name={self.name}']


class HAKCType(HAKCInfo, yaml.YAMLObject):
    yaml_tag = "!HAKCType"
    unknown_type = "@UNKNOWN@"

    def __init__(self, DebugType: str, LLVMType: str, **kwargs):
        yaml.YAMLObject.__init__(self)
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
        raise RuntimeError(f'{other} is not a {self.__class__.__name__}!')

    def __hash__(self):
        if self._debug_type_is_known:
            return hash(self._debug_type_transformed)
        else:
            return hash(self.llvm_type)

    def get_info_tokens(self):
        return [f'debug_type={self.debug_type}', f'llvm_type={self.llvm_type}']

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


class HAKCScope(yaml.YAMLObject, HAKCPrintableObj):
    yaml_tag = "!HAKCScope"
    global_scope = "global"
    local_scope = "local"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCPrintableObj.__init__(self, **kwargs)
        self.scope = kwargs['Scope'] if 'Scope' in kwargs else None
        self.local_scope_name = kwargs['LocalScopeName'] if 'LocalScopeName' in kwargs else None
        self.is_global_scope = self.scope == HAKCScope.global_scope
        self.is_local_scope = self.scope == HAKCScope.local_scope

    def __eq__(self, other):
        if isinstance(other, HAKCScope):
            if self.is_local_scope:
                return other.is_local_scope and self.local_scope_name == other.local_scope_name
            else:
                return other.is_global_scope
        raise RuntimeError(f'{other} is not a {self.__class__.__name__}')

    def __hash__(self):
        if self.is_global_scope:
            return hash(self.scope)
        else:
            return hash((self.scope, self.local_scope_name))

    def get_info_tokens(self):
        result = [f'scope={self.scope}']
        if self.is_local_scope:
            result.append(f'local_scope_name={self.local_scope_name}')
        return result


class HAKCSymbol(HAKCInfo):
    def __init__(self, Type: HAKCType, Scope: HAKCScope, IsDefinition: bool, **kwargs):
        HAKCInfo.__init__(self, **kwargs)
        self.type = Type
        self.scope = Scope
        self.defining_file = None
        self.defining_line = None
        self.compilation_units = set()

        if IsDefinition:
            self.defining_file = kwargs['DefiningFile'] if 'DefiningFile' in kwargs else None
            self.defining_line = kwargs['DefiningLine'] if 'DefiningLine' in kwargs else None

        self.used_symbols = set(kwargs['UsedSymbols']) if 'UsedSymbols' in kwargs else set()

    def __eq__(self, other):
        if isinstance(other, HAKCSymbol):
            return hash(self) == hash(other)
        raise RuntimeError(f'{other} is not a {self.__class__.__name__}')

    def __hash__(self):
        return hash((self.name, self.type, self.scope))

    def get_info_tokens(self):
        result = HAKCInfo.get_info_tokens(self)
        result.extend([f'type={str(self.type)}', f'scope={str(self.scope)}',
                       f'definition={self.defining_file + ":" + str(self.defining_line) if self.is_definition else "None"}'])
        return result

    @property
    def is_definition(self):
        return self.defining_file is not None

    def merge_symbol(self, other):
        for cu in other.compilation_units:
            self.compilation_units.add(cu)

        if other.is_definition:
            self.defining_file = other.defining_file
            self.defining_line = other.defining_line
            self.type = other.type
            for symbol in other.used_symbols:
                self.used_symbols.add(symbol)

    def clear(self):
        self.used_symbols.clear()


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

    def __hash__(self):
        result = hash(self.link_type)
        if self.type:
            result = hash((result, self.type))
        if self.global_name:
            result = hash((result, self.global_name))
        if self.offset:
            result = hash((result, self.offset))
        if self.arg_num:
            result = hash((result, self.arg_num))
        if self.function_name:
            result = hash((result, self.function_name))
        return result


class HAKCIndirectCallSource(yaml.YAMLObject, HAKCInfo):
    yaml_tag = "!HAKCIndirectSource"

    def __init__(self, Type: HAKCType, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCInfo.__init__(self, **kwargs)
        self.source = kwargs['Source'] if 'Source' in kwargs else list()
        self.type = Type

    def __hash__(self):
        result = hash(self.type)
        for link in self.source:
            result = hash((result, link))
        return result


class HAKCFunction(yaml.YAMLObject, HAKCSymbol):
    yaml_tag = "!HAKCFunction"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCSymbol.__init__(self, **kwargs)
        self.direct_calls = set(kwargs['DirectCalls']) if 'DirectCalls' in kwargs else set()
        self.indirect_calls = set(kwargs['IndirectCalls']) if 'IndirectCalls' in kwargs else set()

    def is_function(self) -> bool:
        return True

    def merge_symbol(self, other):
        if not isinstance(other, HAKCFunction):
            raise RuntimeError(f'Tried to merge {other} with {self}')
        HAKCSymbol.merge_symbol(self, other)
        for direct_call in other.direct_calls:
            self.direct_calls.add(direct_call)
        for indirect_call in other.indirect_calls:
            self.indirect_calls.add(indirect_call)

    def clear(self):
        HAKCSymbol.clear(self)
        self.direct_calls.clear()
        self.indirect_calls.clear()


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
