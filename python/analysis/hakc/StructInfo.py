import os


class StructInfo:
    def __init__(self):
        self.name = None
        self.path = None
        self.line = -1
        self.type_hash = None
        self.compilation_units = set()
        self.users = set()
        self.escaping_members = dict()
        self.members = dict()

    @classmethod
    def from_yaml(cls, yaml_def: dict, compilation_unit: str):
        new_type = cls()
        new_type.set_name(yaml_def['name'])
        new_type.set_type_hash(yaml_def['type'])
        new_type.add_compilation_unit(compilation_unit)
        if yaml_def['file']:
            new_type.set_path(yaml_def['file'])
            new_type.set_line(int(yaml_def['line']))
        if yaml_def['Users']:
            for user in yaml_def['Users']:
                new_type.add_user(user)
        if yaml_def['EscapingMembers']:
            for escape_info in yaml_def['EscapingMembers']:
                new_type.add_escaping_member(escape_info['escaper'],
                                             escape_info['offset'])

        if 'MemberOffsets' in yaml_def and \
                yaml_def['MemberOffsets'] is not None:
            for member_info in yaml_def['MemberOffsets']:
                new_type.add_member(member_info['offset'], member_info['type'])
        return new_type

    def add_escaping_member(self, escaper, member_offset):
        if member_offset not in self.escaping_members:
            self.escaping_members[member_offset] = set()
        self.escaping_members[member_offset].add(escaper)

    def add_member(self, member_offset, member_type):
        self.members[member_offset] = member_type

    def add_compilation_unit(self, compilation_unit):
        self.compilation_units.add(compilation_unit)

    def add_user(self, user: str):
        self.users.add(user)

    def get_users(self):
        return self.users

    def get_type(self):
        return self.type_hash

    def get_escaping_members(self):
        return self.escaping_members

    def get_members(self):
        return self.members

    def set_name(self, name: str):
        self.name = name

    def get_name(self):
        return self.name

    def set_path(self, path: str):
        self.path = path

    def get_path(self):
        return self.path

    def set_type_hash(self, type_hash: str):
        self.type_hash = type_hash

    def get_type_hash(self):
        return self.type_hash

    def set_line(self, line: int):
        self.line = line

    def get_line(self):
        return self.line

    def merge_struct_info(self, struct_info: 'StructInfo') -> None:
        if self.type_hash is None and struct_info.get_type() is not None:
            self.type_hash = struct_info.get_type()
        if self.name is None and struct_info.name is not None:
            self.name = struct_info.name
        for cu in struct_info.compilation_units:
            self.add_compilation_unit(cu)
        for user in struct_info.users:
            self.add_user(user)
        for offset, escapers in struct_info.escaping_members.items():
            for escaper in escapers:
                self.add_escaping_member(escaper, offset)
        for offset, member_type in struct_info.members.items():
            self.add_member(offset, member_type)

    def __eq__(self, other):
        if not isinstance(other, StructInfo):
            return False

        return self.type_hash == other.type_hash and self.path == other.path \
            and self.line == other.line

    def __hash__(self):
        return hash((self.type_hash, self.path, self.line))

    def __str__(self):
        result = "Name:           {}\n".format(self.name)
        result += "Path:           {}\n".format(self.path)
        result += "Line:           {}\n".format(self.line)
        result += "Type:           {}\n".format(self.type_hash)
        result += "CU:             {}\n".format(self.compilation_units)
        result += "Users:          {}\n".format(self.users)
        result += "Escapers:\n"
        for offset, escapers in sorted(self.escaping_members.items(),
                                       key=lambda x: x[0]):
            result += "\t{}: {}\n".format(offset, escapers)
        result += "Members:\n"
        for offset, type_hash in sorted(self.members.items(),
                                        key=lambda x: x[0]):
            result += "\t{}: {}\n".format(offset, type_hash)

        return result
