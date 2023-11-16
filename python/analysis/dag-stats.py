import argparse
import pickle
import pandas as pd
import networkx as nx
import os
import sys

sys.path.append(os.path.join('HAKC-common', 'python'))
from hakc.HAKCCompartmentalization import HAKCCompartmentalization

def compartment_degree_computation(c: HAKCCompartmentalization) -> pd.DataFrame:
    compartment_ids = list()
    compartment_data = dict()
    compartment_data['degree'] = list()

    for compartment in c.get_compartments():
        degree = c.compartment_topo.degree(compartment)
        compartment_ids.append(compartment.get_compartment_id())
        compartment_data['degree'].append(degree)

    return pd.DataFrame(compartment_data, index=compartment_ids)


def compartment_connected_component_computation(c: HAKCCompartmentalization) -> pd.DataFrame:
    compartment_ids = list()
    compartment_data = dict()
    compartment_data['scc-size'] = list()

    for scc in nx.strongly_connected_components(c.compartment_topo):
        scc_size = len(scc)
        for compartment in scc:
            compartment_ids.append(compartment.get_compartment_id())
            compartment_data['scc-size'].append(scc_size)
    return pd.DataFrame(compartment_data, index=compartment_ids)


def compartment_hist(df_series: pd.Series):
    print(df_series.value_counts().sort_index())


def compartment_degree_print(degree_df: pd.DataFrame,
                             c: HAKCCompartmentalization,
                             n_degree: int,
                             comparison_str: str):
    total_count = 0
    for compartment_id, _ in degree_df.iterrows():
        compartment = c.get_compartment(compartment_id)
        print(compartment)
        total_count += 1
    percent_total = total_count / c.compartment_topo.number_of_nodes() * 100
    print(f'Total compartments with degree {comparison_str} {n_degree}: '
          f'{total_count} ({percent_total:.3g}%)')


def compartment_degree_ge(c: HAKCCompartmentalization,
                          n_degree: int) -> pd.DataFrame:
    df = compartment_degree_computation(c)
    gte = df[df['degree'] >= n_degree]
    compartment_degree_print(gte, c, n_degree, ">=")
    return gte


def compartment_degree_eq(c: HAKCCompartmentalization,
                          n_degree: int) -> pd.DataFrame:
    df = compartment_degree_computation(c)
    eq = df[df['degree'] == n_degree]
    compartment_degree_print(eq, c, n_degree, "==")
    return eq


def compartment_degree_quantiles(c: HAKCCompartmentalization,
                                 n_quantiles: int) -> pd.DataFrame:
    df = compartment_degree_computation(c)
    df['degree-quantiles'] = pd.qcut(df['degree'], q=n_quantiles, precision=0)
    return df


def compartment_scc(c: HAKCCompartmentalization):
    df = compartment_connected_component_computation(c)
    df_counts = df['scc-size'].value_counts()
    print(df_counts.div(df_counts.index).astype(int).sort_index())


def compartment_degree_quantile_hist(c: HAKCCompartmentalization,
                                     n_quantiles: int) -> pd.DataFrame:
    df = compartment_degree_quantiles(c, n_quantiles)
    compartment_hist(df['degree-quantiles'])
    return df


def compartment_degree_bins(c: HAKCCompartmentalization, 
                            n_bins: int) -> pd.DataFrame:
    df = compartment_degree_computation(c)
    df['degree-bins'] = pd.cut(df['degree'], bins=n_bins, precision=0)
    return df


def compartment_degree_bin_hist(c: HAKCCompartmentalization,
                                n_bins: int) -> pd.DataFrame:
    df = compartment_degree_bins(c, n_bins)
    compartment_hist(df['degree-bins'])
    return df


def compute_page_rank(c: HAKCCompartmentalization) -> pd.DataFrame:
    compartment_ids = list()
    pageranks = dict()
    pageranks['page-rank'] = list()
    print(f'Adding pagerank weights...', end='', flush=True)
    c.add_weight_edges()
    print(f'Done')
    print(f'Computing pageranks...', end='', flush=True)
    for compartment, pagerank in nx.pagerank(c.compartment_topo).items():
        compartment_ids.append(compartment.get_compartment_id())
        pageranks['page-rank'].append(pagerank)
    print(f'Done')

    return pd.DataFrame(pageranks, index=compartment_ids)


output_count = 0
def output_csv(df: pd.DataFrame,
               base_path: str):
    global output_count
    filename, ext = os.path.splitext(base_path)
    df.to_csv(filename + str(output_count) + ext)
    output_count += 1

def compute_page_rank(c: HAKCCompartmentalization) -> pd.DataFrame:
    compartment_ids = list()
    pageranks = dict()
    pageranks['page-rank'] = list()
    print(f'Adding pagerank weights...', end='', flush=True)
    c.add_weight_edges()
    print(f'Done')
    print(f'Computing pageranks...', end='', flush=True)
    for compartment, pagerank in nx.pagerank(c.compartment_topo).items():
        compartment_ids.append(compartment.get_compartment_id())
        pageranks['page-rank'].append(pagerank)
    print(f'Done')

    return pd.DataFrame(pageranks, index=compartment_ids)


def main():
    parser = argparse.ArgumentParser(description='Data Access Graph stats')
    parser.add_argument('--dag', help='/path/to/dag', required=True)
    parser.add_argument('--degree_bins', 
                        const=10, 
                        type=int,
                        nargs='?')
    parser.add_argument('--degree_bins_hist', 
                        const=10, 
                        type=int,
                        nargs='?')
    parser.add_argument('--degree_quants', 
                        const=10, 
                        type=int,
                        nargs='?')
    parser.add_argument('--degree_quants_hist', 
                        const=10, 
                        type=int,
                        nargs='?')
    parser.add_argument('--degree_ge', type=int)
    parser.add_argument('--degree_eq', type=int)
    parser.add_argument('--scc_sizes', action='store_true')
    parser.add_argument('--page_rank', action='store_true')
    parser.add_argument('--to_csv',
                        type=str)

    args = parser.parse_args()

    with open(args.dag, 'rb') as f:
        print(f'Reading in DAG {args.dag}', end='...', flush=True)
        compartmentalization = pickle.load(f)
        print('Done')

    if args.degree_bins:
        df = compartment_degree_bins(compartmentalization,
                                args.degree_bins)
        print(df)
        if args.to_csv:
            output_csv(df, args.to_csv)
    
    if args.degree_bins_hist:
        df = compartment_degree_bin_hist(compartmentalization, args.degree_bins_hist)
        if args.to_csv:
            output_csv(df, args.to_csv)
    
    if args.degree_quants:
        df = compartment_degree_quantiles(compartmentalization,
                                args.degree_quants)
        print(df)
        if args.to_csv:
            output_csv(df, args.to_csv)
    
    if args.degree_quants_hist:
        df = compartment_degree_quantile_hist(compartmentalization, args.degree_quants_hist)
        if args.to_csv:
            output_csv(df, args.to_csv)

    if args.degree_ge:
        df = compartment_degree_ge(compartmentalization, args.degree_ge)
        if args.to_csv:
            output_csv(df, args.to_csv)
    
    if args.degree_eq:
        df = compartment_degree_eq(compartmentalization, args.degree_eq)
        if args.to_csv:
            output_csv(df, args.to_csv)

    if args.scc_sizes:
        compartment_scc(compartmentalization)

    if args.page_rank:
        pageranks = compute_page_rank(compartmentalization)
        print(pageranks.sort_values('page-rank'))
        if args.to_csv:
            output_csv(pageranks, args.to_csv)


if __name__ == "__main__":
    main()
