import argparse
import concurrent.futures
import os
import statistics
from multiprocessing import cpu_count

import matplotlib.patches as mpatches
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

    parser.add_argument('--analysis-dir', dest='analysis_dirs', nargs='+')
    parser.add_argument('--core-count', dest='core_count', default=cpu_count())
    parser.add_argument('--categories', dest='categories', nargs='+')
    parser.add_argument('--apply-labels', dest='label_categories', nargs='+', default=set())
    parser.add_argument('--output-dataframe', dest='output_dataframe')
    args = parser.parse_args()

    if len(args.categories) != len(args.analysis_dirs):
        raise ValueError('Number of categories and analysis directories must match')

    for label_category in args.label_categories:
        if label_category not in args.categories:
            raise ValueError(f'Label {label_category} is not in category list {args.categories}')

    plot_data = {
        'max-normalized-vulnerable-compartment-size': [],
        'average-compartment-size': [],
        'total-compartments': [],
        'colors': []
    }

    filenames = set()
    colors = ['red', 'green', 'blue']
    filename_map = dict()
    label_map = dict()
    for i in range(len(args.analysis_dirs)):
        analysis_dir = args.analysis_dirs[i]
        for root, _, files in os.walk(analysis_dir):
            for file in files:
                if file.endswith(".yml"):
                    filename = os.path.join(root, file)
                    filenames.add(filename)
                    filename_map[filename] = colors[i]
                    if args.categories[i] in args.label_categories:
                        label_map[filename] = filename

    with concurrent.futures.ProcessPoolExecutor(max_workers=args.core_count) as executor:
        futures_to_files = {}
        for filename in filenames:
            futures_to_files[executor.submit(compute_plot_data, filename)] = filename

        with tqdm.tqdm(total=len(filenames)) as pbar:
            for future in concurrent.futures.as_completed(futures_to_files):
                filename = futures_to_files[future]
                try:
                    result = future.result()
                    plot_data['colors'].append(filename_map[filename])
                    label = label_map.get(filename, "")
                    plot_data['label'] = label
                    for name, value in result.items():
                        plot_data[name].append(value)
                except Exception as e:
                    print(f"Error processing {filename}: {e}")
                pbar.update(1)

    legend_handles = [mpatches.Patch(color=colors[i], label=args.categories[i]) for i in range(len(args.analysis_dirs))]

    df = pd.DataFrame(plot_data)
    if args.output_dataframe:
        df.to_csv(args.output_dataframe)
    fig1, ax1 = plt.subplots(figsize=(10, 6))
    df.plot.scatter(x='max-normalized-vulnerable-compartment-size', y='average-compartment-size', c='colors', ax=ax1)
    ax1.legend(handles=legend_handles, loc='upper left', bbox_to_anchor=(1, 1), title='Compartment Strategy')
    fig1.tight_layout(rect=[0, 0, 0.85, 1])

    fig2, ax2 = plt.subplots(figsize=(10, 6))
    df.plot.scatter(x='total-compartments', y='max-normalized-vulnerable-compartment-size', c='colors', ax=ax2)
    ax2.legend(handles=legend_handles, loc='upper left', bbox_to_anchor=(1, 1), title='Compartment Strategy')
    for i, label in enumerate(plot_data['label']):
        if len(label) > 0:
            ax2.annotate(label, (df['total-compartments'][i], df['max-normalized-vulnerable-compartment-size'][i]))

    fig2.tight_layout(rect=[0, 0, 0.85, 1])

    plt.xlim(left=0)
    plt.ylim(bottom=0)

    plt.show()


if __name__ == "__main__":
    main()
