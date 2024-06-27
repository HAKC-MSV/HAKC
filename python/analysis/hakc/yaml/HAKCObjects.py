from typing import Optional, List

import yaml


class HAKCInfo:
    def __init__(self, Name: str, **kwargs):
        self.name = Name

    def __eq__(self, other):
        if isinstance(other, HAKCInfo):
            return other.name == self.name

        return False

    def is_function(self) -> bool:
        return False

    def is_global_variable(self) -> bool:
        return False

    def is_type(self) -> bool:
        return False


class HAKCType(yaml.YAMLObject, HAKCInfo):
    yaml_tag = "!HAKCType"
    unknown_type = "@UNKNOWN@"

    def __init__(self, DebugType: str, LLVMType: str, **kwargs):
        super(HAKCInfo, self).__init__(**kwargs)
        self.debug_type = DebugType
        self.llvm_type = LLVMType

    def __eq__(self, other):
        if isinstance(other, HAKCType):
            if self.debug_type != HAKCType.unknown_type and other.debug_type != HAKCType.unknown_type:
                return self.debug_type == other.debug_type
            elif self.llvm_type != HAKCType.unknown_type and other.llvm_type != HAKCType.unknown_type:
                return self.llvm_type == other.llvm_type

        return False

    def is_type(self) -> bool:
        return True


class HAKCSymbol(HAKCInfo):
    def __init__(self, Type: HAKCType, UsedSymbols: Optional[List], **kwargs):
        super(HAKCInfo, self).__init__(**kwargs)
        self.type = Type
        self.used_symbols = UsedSymbols if UsedSymbols is not None else list()
        self.compilation_units = set()

    def __eq__(self, other):
        if isinstance(other, HAKCSymbol):
            return self.type == other.type and self.name == other.name
        return False

    def add_compilation_unit(self, compilation_unit: str) -> None:
        self.compilation_units.add(compilation_unit)


class HAKCIndirectSourceLink(yaml.YAMLObject, HAKCInfo):
    yaml_tag = "!HAKCIndirectSourceLink"

    def __init__(self, LinkType: str, Type: Optional[HAKCType], GlobalName: Optional[str], Offset: Optional[int],
                 ArgNumber: Optional[int], Function: Optional[str], **kwargs):
        super(HAKCInfo, self).__init__(**kwargs)
        self.link_type = LinkType
        self.type = Type
        self.global_name = GlobalName
        self.offset = Offset
        self.arg_num = ArgNumber
        self.function_name = Function


class HAKCIndirectCallSource(yaml.YAMLObject, HAKCInfo):
    yaml_tag = "!HAKCIndirectSource"

    def __init__(self, Source: Optional[List[HAKCIndirectSourceLink]], Type: HAKCType, **kwargs):
        super(HAKCInfo, self).__init__(**kwargs)
        self.source = Source if Source is not None else list()
        self.type = Type


class HAKCFunction(yaml.YAMLObject, HAKCSymbol):
    yaml_tag = "!HAKCFunction"

    def __init__(self, DirectCalls: Optional[List[HAKCSymbol]], IndirectCalls: Optional[List[HAKCIndirectCallSource]],
                 **kwargs):
        super(HAKCSymbol, self).__init__(**kwargs)
        self.direct_calls = DirectCalls if DirectCalls is not None else set()
        self.indirect_calls = IndirectCalls if IndirectCalls is not None else set()

    def is_function(self) -> bool:
        return True


class HAKCGlobalVariable(yaml.YAMLObject, HAKCSymbol):
    yaml_tag = "!HAKCGlobalVariable"

    def __init__(self, **kwargs):
        super(HAKCSymbol, self).__init__(**kwargs)

    def is_global_variable(self) -> bool:
        return True
