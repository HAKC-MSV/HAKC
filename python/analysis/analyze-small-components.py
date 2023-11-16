import argparse
import pickle

import networkx as nx

scc_size = 1


def main():
    parser = argparse.ArgumentParser(
        description='Analyzes SCCs smaller than {}'.format(scc_size + 1))
    # parser.add_argument('-i', '--input', help='Path to output structures', required=True)
    parser.add_argument('-c', '--compartments', required=True, help='Path to existing '
                                                                    'compartmentalization')
    args = parser.parse_args()

    # with open(args.input, 'rb') as f:
    #     struct_list, symbol_list = pickle.load(f)

    with open(args.compartments, 'rb') as f:
        compartmentalization = pickle.load(f)

    sccs = nx.strongly_connected_components(
        compartmentalization.compartment_topo)
    incoming_sizes = dict()
    outgoing_sizes = dict()
    for c in filter(lambda x: len(x) <= scc_size, sccs):
        incoming_nodes = set()
        outgoing_nodes = set()
        for u, v, edge_capacity in compartmentalization.compartment_topo.edges(
                data="capacity"):
            if u in c:
                outgoing_nodes.add((v, edge_capacity))
            if v in c:
                incoming_nodes.add((u, edge_capacity))
        if len(incoming_nodes) not in incoming_sizes:
            incoming_sizes[len(incoming_nodes)] = list()
        incoming_sizes[len(incoming_nodes)].append(c)

        if len(outgoing_nodes) not in outgoing_sizes:
            outgoing_sizes[len(outgoing_nodes)] = list()
        outgoing_sizes[len(outgoing_nodes)].append(c)

    print("Incoming nodes:")
    for size, component_list in sorted(list(incoming_sizes.items()), reverse=True,
                                       key=lambda x: x[0]):
        print("{} ({}):".format(size, len(component_list)))
        for c in component_list:
            for component in c:
                for clique in component.get_cliques():
                    print("\t{}".format(clique.get_data()))

    print("Outgoing nodes:")
    for size, component_list in sorted(list(outgoing_sizes.items()), reverse=True,
                                       key=lambda x: x[0]):
        print("{} ({}):".format(size, len(component_list)))
        for c in component_list:
            for component in c:
                for clique in component.get_cliques():
                    print("\t{}".format(clique.get_data()))


if __name__ == "__main__":
    main()
