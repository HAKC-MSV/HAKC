import argparse
import os

import yaml


def main():
    parser = argparse.ArgumentParser(description='FLEXC Plot Creator')

    parser.add_argument('--analysis-dir', dest='analysis_dir', required=True)
    args = parser.parse_args()

    plot_data = {
        'vulnerable-compartment-size': [],
    }
    for root, _, files in os.walk(args.analysis_dir):
        for file in files:
            if file.endswith(".yml"):
                with open(os.path.join(root, file), "r") as f:
                    data = yaml.safe_load(f)


if __name__ == "__main__":
    main()
