import os

from .HAKCSymbolOrigin import HAKCSymbolOrigin

_redefinable_symbol_names = {'init_module', 'cleanup_module', '__this_module'}


class SymbolInfo:
    def __init__(self):
        self.name = None
        self.types = set()
        self.hash = None
        self.direct_calls = set()
        self.indirect_calls = dict()
        self.escape_to_symbols = set()
        self.definition_file = None
        self.definition_line = None
        self.is_global = False
        self.user_compilation_units = set()
        self.valid_decl_linkage = True
        self.declaration_sites = set()

    @classmethod
    def from_yaml(cls, yaml_def: dict, compilation_unit: str):
        new_symbol = cls()
        new_symbol.set_name(yaml_def['name'])
        new_symbol.set_hash(yaml_def['type'])
        new_symbol.set_valid_decl_linkage('valid-decl-linkage' in yaml_def and \
                                          yaml_def['valid-decl-linkage'] == 'y')
        new_symbol.set_is_global('is-global' in yaml_def and yaml_def['is-global'] == 'y')

        declaration_site = None
        if 'file' in yaml_def and \
                'line' in yaml_def:
            new_symbol.definition_file = yaml_def['file']
            new_symbol.definition_line = int(yaml_def['line'])
            declaration_site = new_symbol.get_definition_site()
            new_symbol.add_declaration_site(declaration_site)
            if yaml_def['is-defined'] == 'n':
                new_symbol.definition_file = None
                new_symbol.definition_line = None

        if yaml_def['is-defined'] == 'y':
            if declaration_site is None:
                raise RuntimeError(f'Symbol {new_symbol.name} from {compilation_unit} is defined but has no '
                                   f'definition site.'
                                   f'{new_symbol.definition_file} {new_symbol.definition_line}')
            if new_symbol.name in _redefinable_symbol_names:
                new_symbol.set_name(
                    new_symbol.get_name() + "_" + new_symbol.get_definition_site().replace(os.sep, '_'))

        new_symbol.add_user_compilation_unit(compilation_unit)
        if 'direct-calls' in yaml_def and yaml_def['direct-calls']:
            for direct_call in yaml_def['direct-calls']:
                new_symbol.add_direct_call(direct_call)
        if 'indirect-calls' in yaml_def and yaml_def['indirect-calls']:
            for indirect_call in yaml_def['indirect-calls']:
                origin = HAKCSymbolOrigin.from_yaml(indirect_call['origin'])
                new_symbol.add_indirect_call(indirect_call['type'], origin)
        if 'escapes-to' in yaml_def and yaml_def['escapes-to']:
            for escape_symbol in yaml_def['escapes-to']:
                origin = HAKCSymbolOrigin.from_yaml(escape_symbol)
                new_symbol.add_escaping_symbol(origin)

        return new_symbol

    def set_name(self, name: str):
        self.name = name

    def set_hash(self, newhash: str):
        self.hash = newhash

    def add_user_compilation_unit(self, cu):
        self.user_compilation_units.add(cu)

    def add_direct_call(self, call: str):
        self.direct_calls.add(call)

    def add_indirect_call(self, call, origin: HAKCSymbolOrigin):
        if call not in self.indirect_calls:
            self.indirect_calls[call] = set()
        self.indirect_calls[call].add(origin)

    def add_escaping_symbol(self, escape_symbol: HAKCSymbolOrigin):
        self.escape_to_symbols.add(escape_symbol)

    def add_type(self, type_used):
        self.types.add(type_used)

    def get_name(self):
        return self.name

    def get_hash(self):
        return self.hash

    def set_is_global(self, is_global: bool):
        self.is_global = is_global

    def is_global_variable(self) -> bool:
        return self.is_global

    def is_function(self) -> bool:
        return not self.is_global_variable()

    def get_types(self):
        return self.types

    def get_direct_calls(self):
        return self.direct_calls

    def get_indirect_calls(self):
        return self.indirect_calls

    def get_full_definition_file(self):
        return self.definition_file

    def get_definition_site(self):
        declaration_site = None
        if self.definition_file:
            declaration_site = self.get_full_definition_file() + ":" + str(self.definition_line)
        return declaration_site

    def get_definition_file(self):
        return self.definition_file

    def set_definition_file(self, definition_file):
        self.definition_file = definition_file

    def get_definition_line(self):
        return self.definition_line

    def set_definition_line(self, definition_line):
        self.definition_line = definition_line

    def get_declaration_sites(self):
        return self.declaration_sites

    def add_declaration_site(self, declaration_site):
        self.declaration_sites.add(declaration_site)

    def get_user_compilation_units(self):
        return self.user_compilation_units

    def get_escapes(self):
        return self.escape_to_symbols

    def set_valid_decl_linkage(self, valid_decl_linkage: bool):
        self.valid_decl_linkage = valid_decl_linkage

    def is_valid_decl_linkage(self):
        return self.valid_decl_linkage

    def is_inline_like_function(self):
        return self.is_function() and \
            not self.is_valid_decl_linkage()

    def merge_symbol_data(self, symbol_info: 'SymbolInfo') -> None:
        for declaration_site in symbol_info.get_declaration_sites():
            self.add_declaration_site(declaration_site)
        if self.name is None and symbol_info.get_name() is not None:
            self.set_name(symbol_info.get_name())
        if self.hash is None and symbol_info.get_hash() is not None:
            self.set_hash(symbol_info.get_hash())
        for t in symbol_info.get_types():
            self.add_type(t)
        for dc in symbol_info.get_direct_calls():
            self.add_direct_call(dc)
        for call, origins in symbol_info.get_indirect_calls().items():
            for origin in origins:
                self.add_indirect_call(call, origin)
        for escape in symbol_info.get_escapes():
            self.add_escaping_symbol(escape)
        for cu in symbol_info.get_user_compilation_units():
            self.add_user_compilation_unit(cu)
        if symbol_info.get_definition_file() and self.definition_file is None:
            self.set_definition_file(symbol_info.get_definition_file())
        if symbol_info.get_definition_line() and self.definition_line is None:
            self.set_definition_line(symbol_info.get_definition_line())

    def __hash__(self):
        if self.hash is None or self.name is None:
            raise RuntimeError("Invalid object")
        objhash = hash((self.hash, self.name, self.is_global, self.is_valid_decl_linkage()))
        return objhash

    def __eq__(self, other):
        other_eq = self.hash == other.hash and self.name == other.name and \
                   self.is_global == other.is_global and self.is_valid_decl_linkage() == other.is_valid_decl_linkage()
        return other_eq

    def __str__(self):
        result = "----------------------------------\n"
        result += "Name:         {}\n".format(self.name)
        result += "Hash:         {}\n".format(self.hash)
        result += "Definition:   {}\n".format(self.get_definition_site())
        result += "------------------------------------\n"
        return result
