import argparse
import concurrent.futures
import os
import statistics
from multiprocessing import cpu_count

import matplotlib.pyplot as plt
import pandas as pd
import tqdm
import yaml


def compute_plot_data(filename):
    plot_data = dict()

    with open(filename, 'r') as f:
        data = yaml.safe_load(f)
        max_vulnerable_size = -1

        compartment_sizes = list()
        total_compartments = 0
        for compartment_id, compartment_info in data['compartmentalization-info'].items():
            compartment_size = len(compartment_info['filtered-symbol-names']['data'])
            compartment_sizes.append(compartment_size)
            total_compartments += 1

        total_symbols = sum(compartment_sizes)

        for escalation_compartment_id in data['allowable-escalation-compartments']['data']:
            size = len(
                data['compartmentalization-info'][escalation_compartment_id]['filtered-symbol-names']['data'])
            if size > max_vulnerable_size:
                max_vulnerable_size = size
        plot_data['max-normalized-vulnerable-compartment-size'] = max_vulnerable_size / total_symbols

        plot_data['average-compartment-size'] = statistics.mean(compartment_sizes)
        plot_data['total-compartments'] = total_compartments

    return plot_data


def main():
    parser = argparse.ArgumentParser(description='FLEXC Plot Creator')

    parser.add_argument('--analysis-dir', dest='analysis_dir', required=True)
    parser.add_argument('--core-count', dest='core_count', default=cpu_count())
    args = parser.parse_args()

    plot_data = {
        'max-normalized-vulnerable-compartment-size': [],
        'average-compartment-size': [],
        'total-compartments': [],
    }

    filenames = set()
    for root, _, files in os.walk(args.analysis_dir):
        for file in files:
            if file.endswith(".yml"):
                filenames.add(os.path.join(root, file))

    with concurrent.futures.ProcessPoolExecutor(max_workers=args.core_count) as executor:
        futures_to_files = {}
        for filename in filenames:
            futures_to_files[executor.submit(compute_plot_data, filename)] = filename

        with tqdm.tqdm(total=len(filenames)) as pbar:
            for future in concurrent.futures.as_completed(futures_to_files):
                filename = futures_to_files[future]
                try:
                    result = future.result()
                    for name, value in result.items():
                        plot_data[name].append(value)
                except Exception as e:
                    print(f"Error processing {filename}: {e}")
                pbar.update(1)

    df = pd.DataFrame(plot_data)
    df.plot.scatter(x='max-normalized-vulnerable-compartment-size', y='average-compartment-size')
    df.plot.scatter(x='total-compartments', y='max-normalized-vulnerable-compartment-size')

    plt.xlim(left=0)
    plt.ylim(bottom=0)
    plt.show()


if __name__ == "__main__":
    main()
