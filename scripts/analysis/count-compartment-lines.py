import argparse
import yaml
import os


def main():
    parser = argparse.ArgumentParser(description='Count compartmentalization '
                                                 'lines')
    parser.add_argument("-i", "--input", help="Input Compartmentalization "
                                              "YML", required=True)
    parser.add_argument('-r', "--root", help="Kernel source root",
                        required=True)
    args = parser.parse_args()

    with open(args.input, 'r') as f:
        compartmentalization = yaml.safe_load(f)

    total_lines = 0
    compartmentalized_lines = 0
    for entry in compartmentalization['FILES']:
        file_path = os.path.join(args.root, entry['PATH'])
        print(f"Counting {file_path}")
        if os.path.exists(file_path):
            with open(file_path, 'r') as f:
                line_count = len(f.readlines())
                total_lines += line_count
                for symbol in entry['SYMBOLS']:
                    if symbol['COMPARTMENT'] > 0:
                        compartmentalized_lines += line_count
                        break
    print(f"{compartmentalized_lines} / {total_lines} = "
          f"{compartmentalized_lines/total_lines}")


if __name__ == "__main__":
    main()
