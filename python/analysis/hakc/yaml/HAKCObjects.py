import yaml


class HAKCInfo(object):
    def __init__(self, Name: str, **kwargs):
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


class HAKCType(HAKCInfo, yaml.YAMLObject):
    yaml_tag = "!HAKCType"
    unknown_type = "@UNKNOWN@"

    def __init__(self, DebugType: str, LLVMType: str, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCInfo.__init__(self, **kwargs)
        self.debug_type = DebugType
        self.llvm_type = LLVMType

    def __eq__(self, other):
        if isinstance(other, HAKCType):
            if self.debug_type != HAKCType.unknown_type and other.debug_type != HAKCType.unknown_type:
                return self.debug_type == other.debug_type
            elif self.llvm_type != HAKCType.unknown_type and other.llvm_type != HAKCType.unknown_type:
                return self.llvm_type == other.llvm_type

        return False

    def __hash__(self):
        if self.debug_type != HAKCType.unknown_type:
            return hash(self.debug_type)
        else:
            return hash(self.llvm_type)

    def is_type(self) -> bool:
        return True


class HAKCSymbol(HAKCInfo):
    def __init__(self, Type: HAKCType, **kwargs):
        HAKCInfo.__init__(self, **kwargs)
        self.type = Type
        self.defining_file = kwargs['DefiningFile'] if 'DefiningFile' in kwargs else None
        self.defining_line = kwargs['DefiningLine'] if 'DefiningLine' in kwargs else None
        self.used_symbols = kwargs['UsedSymbols'] if 'UsedSymbols' in kwargs else list()

    def __eq__(self, other):
        if isinstance(other, HAKCSymbol):
            return self.type == other.type and self.name == other.name
        return False

    def __hash__(self):
        return hash((HAKCInfo.__hash__(self), self.type))


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
        self.direct_calls = kwargs['DirectCalls'] if 'DirectCalls' in kwargs else set()
        self.indirect_calls = kwargs['IndirectCalls'] if 'IndirectCalls' in kwargs else set()

    def is_function(self) -> bool:
        return True


class HAKCGlobalVariable(yaml.YAMLObject, HAKCSymbol):
    yaml_tag = "!HAKCGlobalVariable"

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCSymbol.__init__(self, **kwargs)

    def is_global_variable(self) -> bool:
        return True


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
}
