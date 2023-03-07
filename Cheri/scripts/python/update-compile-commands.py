import argparse
import json


def main():
    parser = argparse.ArgumentParser(description='Update Compilation Database to specific compiler')
    parser.add_argument('--cdb', required=True, help="/path/to/compile_commands.json", type=str)
    parser.add_argument('--out', required=True, help='/path/to/output/compile_commands.json', type=str)
    parser.add_argument('--use-cc', dest='use_cc', required=True, help='/path/to/replacement/cc/compiler', type=str)
    parser.add_argument('--cc', default='cc', help='Compiler string to find', type=str)

    args = parser.parse_args()

    with open(args.cdb, 'r') as f:
        print(f'Loading {args.cdb}...', end='', flush=True)
        compile_commands = json.load(f)
        print('Done!')

    print(f"Editing compile_commands...", end='', flush=True)
    replace_count = 0
    for entry in compile_commands:
        if entry['command']:
            command_toks = entry['command'].split()
            for i in range(len(command_toks)):
                tok = command_toks[i]
                if tok == args.cc:
                    command_toks[i] = args.use_cc
                    replace_count += 1
            entry['command'] = ' '.join(command_toks)
    print(f'Done! Replaced {replace_count} tokens.')

    with open(args.out, 'w') as f:
        print(f'Outputting modified compilation database to {args.out}...', end='', flush=True)
        json.dump(compile_commands, f)
        print(f'Done!')


if __name__ == "__main__":
    main()