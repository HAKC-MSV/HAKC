import argparse
import pickle


def main():
    parser = argparse.ArgumentParser(description='Finds struct and symbol '
                                                 'faults')
    parser.add_argument('-i', '--input', help='Path to output structures',
                        required=True)
    args = parser.parse_args()
    with open(args.input, 'rb') as f:
        struct_list, symbol_list = pickle.load(f)

    print("len(struct_list) = {}".format(len(struct_list)))
    print("len(symbol_list) = {}".format(len(symbol_list)))

    for cu, symbols in symbol_list.items():
        for symbol in symbols:
            if symbol.name is None or \
                    symbol.types is None or \
                    symbol.hash is None or \
                    symbol.direct_calls is None or \
                    symbol.indirect_calls is None or \
                    symbol.escape_to_symbols is None or \
                    symbol.compilation_unit is None or \
                    symbol.user_compilation_units is None:
                print("Symbol in {} is invalid".format(cu))
                if symbol.name is not None:
                    print("Symbol name: {}".format(symbol.name))
                break

    for cu, structs in struct_list.items():
        for struct in structs:
            if struct.name is None or \
                    struct.path is None or \
                    struct.line is None or \
                    struct.type_hash is None or \
                    struct.compilation_units is None or \
                    struct.users is None or \
                    struct.escaping_members is None or \
                    struct.members is None:
                print("Struct in {} is invalid".format(cu))
                if struct.name is not None:
                    print("Struct name: {}".format(struct.name))
                break


if __name__ == "__main__":
    main()
