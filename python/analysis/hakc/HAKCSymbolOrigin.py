from typing import Optional


class HAKCSymbolOrigin:
    def __init__(self):
        self.type = None
        self.offset = None
        self.argument = None
        self.global_name = None
        self.function_name = None
        self.is_unknown = None

    @classmethod
    def from_yaml(cls, yaml_def):
        new_origin = cls()
        if yaml_def == 'unknown-origin':
            new_origin.is_unknown = True
        if 'type' in yaml_def and yaml_def['type'] is not None:
            new_origin.type = yaml_def['type']
        if 'offset' in yaml_def and yaml_def['offset'] is not None:
            new_origin.offset = int(yaml_def['offset'])
        if 'argument' in yaml_def and yaml_def['argument'] is not None:
            new_origin.argument = int(yaml_def['argument'])
        if 'global-name' in yaml_def and yaml_def['global-name'] is not None:
            new_origin.global_name = yaml_def['global-name']
        if 'function-name' in yaml_def and yaml_def['function-name'] is not None:
            new_origin.function_name = yaml_def['function-name']
        return new_origin

    def __eq__(self, __o: object) -> bool:
        if not isinstance(__o, HAKCSymbolOrigin):
            return False
        return hash(self) == hash(__o)

    def __hash__(self) -> int:
        return hash((self.type, self.offset, self.argument, self.global_name,
                     self.function_name))

    def __str__(self):
        result = "type: {}\n".format(self.type)
        if self.offset:
            result += "\toffset: {}\n".format(self.offset)
        if self.argument:
            result += "\targument: {}\n".format(self.argument)
        if self.global_name:
            result += "\tglobal_name: {}\n".format(self.global_name)
        if self.function_name:
            result += "\tfunction_name: {}\n".format(self.function_name)
        if self.is_unknown:
            result += "\tis_unknown: {}\n".format(self.is_unknown)

        return result

    def get_type(self) -> Optional[str]:
        return self.type

    def get_offset(self) -> Optional[int]:
        return self.offset

    def get_argument(self) -> Optional[int]:
        return self.argument

    def get_global_name(self) -> Optional[str]:
        return self.global_name

    def get_function_name(self) -> Optional[str]:
        return self.function_name
