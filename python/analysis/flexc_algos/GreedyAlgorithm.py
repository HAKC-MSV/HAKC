import concurrent.futures
import logging
import os

import networkx as nx
import yaml
from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import HAKCLogger
from hakc.HAKCObjects import HAKCCompartment, HAKCDivision, HAKCSymbol

from . import FlexCAlgorithm

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')

mp_conn = None
mp_graph = None


def init_mp_database(db_dir: str):
    global mp_conn
    logger.debug(f'Creating database at {db_dir}')
    mp_conn = HAKCDatabase(db_dir, read_only=True)


def compute_greedy_interaction(compartment_id: int):
    global mp_conn
    valid_targets = mp_conn.get_valid_targets_from_compartment_id(compartment_id)
    compartment_interaction_levels = GreedyAlgorithm.compute_compartment_interaction(mp_conn, compartment_id,
                                                                                     valid_targets)
    return compartment_interaction_levels


def merge_compartment(graph):
    resulting_compartments = dict()
    for compartment_id in graph.nodes:
        resulting_compartments[compartment_id] = compartment_id

    max_edge = max(graph.edges(data=True), key=lambda edge: edge[2].get(GreedyAlgorithm.weight_attr, 0))
    original_max_weight = max_edge[2].get(GreedyAlgorithm.weight_attr, 0)

    while len(graph.edges) > 0:
        max_edge = max(graph.edges(data=True), key=lambda edge: edge[2].get(GreedyAlgorithm.weight_attr, 0))
        if max_edge[2].get(GreedyAlgorithm.weight_attr, 0) < original_max_weight:
            break
        head_compartment_id = max_edge[0]
        tail_compartment_id = max_edge[1]
        logger.debug(
            f'Merging {head_compartment_id} -> {tail_compartment_id} with interaction {max_edge[2].get(GreedyAlgorithm.weight_attr, 0)}')
        compartments_to_change = set()
        for compartment_id, result in resulting_compartments.items():
            if result == head_compartment_id:
                compartments_to_change.add(compartment_id)

        for compartment_id in compartments_to_change:
            resulting_compartments[compartment_id] = tail_compartment_id

        resulting_compartments[head_compartment_id] = tail_compartment_id
        GreedyAlgorithm.remove_compartment(head_compartment_id, tail_compartment_id, graph)

    return resulting_compartments


class GreedyAlgorithm(FlexCAlgorithm.FlexCAlgorithm):
    title = "greedy"
    aliases = ['g']
    help = "Partition the DAG greedily"
    merged_compartment_attr = 'merged_compartments'
    weight_attr = 'weight'

    def __init__(self, parser, **kwargs):
        super().__init__(parser, **kwargs)
        self.compartment_interactions = nx.DiGraph()

    def add_command_line_arguments(self, parser):
        parser.add_argument('--max-compartments', type=int, help='Max resultant compartments',
                            dest="max_compartments", required=True)
        parser.add_argument('--start-compartment-count', type=int,
                            help='Number of compartments to start with (for debugging)', dest='start_count', default=-1)

    @staticmethod
    def mp_merge_compartment(subgraph_nodes):
        global mp_graph
        subgraph = mp_graph.subgraph(subgraph_nodes).copy()
        return merge_compartment(subgraph)

    @staticmethod
    def remove_compartment(compartment_id: int, resulting_compartment_id: int, graph):
        edges_to_add = list()
        edges_to_adjust = list()
        for in_edge in graph.in_edges(compartment_id, data=True):
            head = in_edge[0]
            if head == resulting_compartment_id:
                continue

            weight = in_edge[2].get(GreedyAlgorithm.weight_attr, 0)
            if graph.has_edge(head, resulting_compartment_id):
                edges_to_adjust.append((head, resulting_compartment_id, weight))
            else:
                edges_to_add.append((head, resulting_compartment_id, weight))

        for out_edge in graph.out_edges(compartment_id, data=True):
            tail = out_edge[1]
            if tail == resulting_compartment_id:
                continue

            weight = out_edge[2].get(GreedyAlgorithm.weight_attr, 0)
            if graph.has_edge(resulting_compartment_id, tail):
                edges_to_adjust.append((resulting_compartment_id, tail, weight))
            else:
                edges_to_add.append((resulting_compartment_id, tail, weight))

        graph.remove_node(compartment_id)
        for (head, tail, weight) in edges_to_add:
            graph.add_edge(head, tail, **{GreedyAlgorithm.weight_attr: weight})
        for (head, tail, weight) in edges_to_adjust:
            graph[head][tail][GreedyAlgorithm.weight_attr] += weight

    @staticmethod
    def compute_compartment_interaction(db: HAKCDatabase, compartment_id_1: int,
                                        target_compartments: list[int]) -> dict[int, int]:
        cmd = f"""
        MATCH
        (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.InCompartmentTable}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.InDivisionTable}]-(sym1:{HAKCSymbol.get_table_name()})-[dag:{HAKCSymbol.DagEdgeTable}]->(sym2:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.InDivisionTable}]->(div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(comp2:{HAKCCompartment.get_table_name()})
        WHERE comp1.{str(HAKCCompartment.get_primary_key())} = $comp1_id AND comp2.{str(HAKCCompartment.get_primary_key())} IN [{','.join([str(i) for i in target_compartments])}]
        RETURN comp2.{str(HAKCCompartment.get_primary_key())} AS CompartmentID, dag.weight AS Weight
        """
        response = db.execute_prepared_stmt(cmd, comp1_id=compartment_id_1)
        data = response.get_as_df()
        weights = dict()
        for _, row in data.iterrows():
            compartment_id = int(row['CompartmentID'].item())
            if compartment_id == compartment_id_1:
                continue
            weight = row['Weight'].item()
            if compartment_id not in weights:
                weights[compartment_id] = weight
            else:
                weights[compartment_id] += weight
        return weights

    @staticmethod
    def get_compartments_to_merge(db: HAKCDatabase):
        cmd = f"""
        MATCH 
        (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.InCompartmentTable}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.InDivisionTable}]-(sym1:{HAKCSymbol.get_table_name()})-[dag:{HAKCSymbol.DagEdgeTable}]->(sym2:{HAKCSymbol.get_table_name()})
        RETURN DISTINCT comp1.{str(HAKCCompartment.get_primary_key())} AS CompartmentID
        ORDER BY CompartmentID
        """
        response = db.execute_prepared_stmt(cmd)
        compartments = response.get_as_df()['CompartmentID'].tolist()

        return compartments

    def insert_compartment(self, compartment_id: int):
        if compartment_id not in self.compartment_interactions:
            self.compartment_interactions.add_node(compartment_id, **{GreedyAlgorithm.merged_compartment_attr: set()})

    def handle_interaction_result(self, head_compartment_id: int, compartment_interactions: dict):
        self.insert_compartment(head_compartment_id)
        for compartment_id, interaction in compartment_interactions.items():
            self.insert_compartment(compartment_id)
            self.compartment_interactions.add_edge(head_compartment_id, compartment_id,
                                                   **{GreedyAlgorithm.weight_attr: interaction})

    def reset_interactions(self):
        self.compartment_interactions.clear()

    @staticmethod
    def handle_merge(compartment_interactions: nx.DiGraph, resulting_compartments: dict, max_compartments: int):
        for compartment_id, resulting_compartment in resulting_compartments.items():
            if len(compartment_interactions) <= max_compartments:
                break
            if compartment_id != resulting_compartment:
                existing_merged_nodes = compartment_interactions.nodes[compartment_id][
                    GreedyAlgorithm.merged_compartment_attr]
                for merged_node in existing_merged_nodes:
                    compartment_interactions.nodes[resulting_compartment][
                        GreedyAlgorithm.merged_compartment_attr].add(merged_node)
                compartment_interactions.nodes[resulting_compartment][GreedyAlgorithm.merged_compartment_attr].add(
                    compartment_id)
                GreedyAlgorithm.remove_compartment(compartment_id, resulting_compartment, compartment_interactions)

    @staticmethod
    def compute_subgraphs(compartment_interactions: nx.DiGraph) -> list[set[int]]:
        independent_graphs = list()
        graph_indicies = dict()
        resulting_indicies = dict()
        current_idx = 0
        for sorted_edge in logger.progress_bar(
                iterable=sorted(compartment_interactions.edges(data=True), reverse=True,
                                key=lambda edge: edge[2].get(GreedyAlgorithm.weight_attr, 0)),
                desc="Creating subgraphs"):
            compartment_to_be_removed = sorted_edge[0]
            remaining_compartment = sorted_edge[1]

            head_idx = graph_indicies.get(compartment_to_be_removed, None)
            tail_idx = graph_indicies.get(remaining_compartment, None)

            if head_idx is None and tail_idx is None:
                graph_indicies[compartment_to_be_removed] = current_idx
                graph_indicies[remaining_compartment] = current_idx
                resulting_indicies[current_idx] = current_idx
                current_idx += 1

            if head_idx is not None and tail_idx is not None:
                if head_idx < tail_idx:
                    resulting_indicies[tail_idx] = head_idx
                if tail_idx < head_idx:
                    resulting_indicies[head_idx] = tail_idx

            if head_idx is not None and tail_idx is None:
                graph_indicies[remaining_compartment] = head_idx
            if tail_idx is not None and head_idx is None:
                graph_indicies[compartment_to_be_removed] = tail_idx

        indicies = list(sorted(set(resulting_indicies.values())))
        normalized_indicies = dict()
        for i in range(len(indicies)):
            normalized_indicies[indicies[i]] = i
            independent_graphs.append(set())

        for compartment_id, index in graph_indicies.items():
            normalized_index = normalized_indicies[resulting_indicies[index]]
            independent_graphs[normalized_index].add(compartment_id)

        return independent_graphs

    def merge_compartments(self, db: HAKCDatabase, max_compartments: int, core_count: int):
        db.close()
        db.open()

        final_compartment_map = dict()

        try:
            while (len(self.compartment_interactions) > max_compartments) and len(
                    self.compartment_interactions.edges) > 0:
                edge_count = len(self.compartment_interactions.edges)
                compartment_count = len(self.compartment_interactions)

                independent_graphs = self.compute_subgraphs(self.compartment_interactions)

                global mp_graph
                mp_graph = self.compartment_interactions
                if core_count == 1 or len(independent_graphs) == 1:
                    for subgraph_nodes in logger.progress_bar(iterable=independent_graphs,
                                                              desc='Merging Compartments'):
                        resulting_compartments = GreedyAlgorithm.mp_merge_compartment(subgraph_nodes)
                        GreedyAlgorithm.handle_merge(self.compartment_interactions, resulting_compartments,
                                                     max_compartments)
                else:
                    with concurrent.futures.ProcessPoolExecutor(max_workers=core_count) as executor:
                        futures_to_edge = dict()
                        for subgraph_nodes in logger.progress_bar(iterable=independent_graphs,
                                                                  desc="Compartment Merge Scheduling"):
                            futures_to_edge[
                                executor.submit(GreedyAlgorithm.mp_merge_compartment, subgraph_nodes)] = subgraph_nodes
                        with logger.progress_bar(total=len(futures_to_edge),
                                                 desc="Merging Compartments") as pbar:
                            for future in concurrent.futures.as_completed(futures_to_edge):
                                pbar.update(1)
                                resulting_compartments = future.result()
                                GreedyAlgorithm.handle_merge(self.compartment_interactions, resulting_compartments,
                                                             max_compartments)
                if len(self.compartment_interactions.edges) == edge_count:
                    logger.info(f'Edge count unchanged. Stopping')
                    break
                else:
                    logger.info(
                        f'Removed {edge_count - len(self.compartment_interactions.edges)} edges and {compartment_count - len(self.compartment_interactions)} compartments. There are {len(self.compartment_interactions)} compartments remaining')
        except KeyboardInterrupt:
            logger.info(f'User stopped analysis')

        for compartment_id in self.compartment_interactions.nodes():
            for merged_compartment_id in self.compartment_interactions.nodes[compartment_id][
                GreedyAlgorithm.merged_compartment_attr]:
                final_compartment_map[merged_compartment_id] = compartment_id

        db.merge_compartments(final_compartment_map)
        remaining_compartments = db.get_all_compartments()
        logger.info(f'Initial merging complete. There are {len(remaining_compartments)} compartments')

        if len(remaining_compartments) > max_compartments:
            compartment_sizes = db.get_compartment_symbol_count()
            cleanup_compartment_map = dict()
            compartments_remaining = len(remaining_compartments) - max_compartments
            with logger.progress_bar(total=compartments_remaining, desc='Merging remaining compartments') as pbar:
                compartments_sorted_by_size = [compartment[0] for compartment in
                                               sorted(compartment_sizes.items(), key=lambda size: size[1])]
                for i in range(compartments_remaining):
                    target_compartment_idx = (i % max_compartments) + 1
                    target_compartment = compartments_sorted_by_size[-target_compartment_idx]
                    source_compartment = compartments_sorted_by_size[i]
                    cleanup_compartment_map[source_compartment] = target_compartment
                    final_compartment_map[source_compartment] = target_compartment

            db.merge_compartments(cleanup_compartment_map)

        merge_manifest_path = os.path.join(db.db_dir, 'merge-manifest.yml')
        logger.info(f'Writing merge manifest to {merge_manifest_path}')
        with open(merge_manifest_path, 'w') as f:
            yaml.dump(final_compartment_map, f, indent=2)

        db.close()
        db.open(read_only=True)

    def run(self, db: HAKCDatabase, **kwargs):
        max_compartments: int = kwargs.get('max_compartments')
        core_count: int = int(kwargs.get('core_count', 1))
        starting_compartment_count: int = kwargs.get('start_count', -1)
        compartments = self.get_compartments_to_merge(db)
        if starting_compartment_count > 0:
            compartments = compartments[:min(len(compartments), starting_compartment_count)]

        self.reset_interactions()
        logger.info(f'Creating at most {max_compartments} compartments')

        if core_count > 1:
            with concurrent.futures.ProcessPoolExecutor(max_workers=core_count, initializer=init_mp_database,
                                                        initargs=(db.db_dir,)) as executor:
                compartment_to_futures = dict()
                for compartment_id in logger.progress_bar(iterable=compartments,
                                                          desc="Compartment Interaction Scheduling"):
                    compartment_to_futures[
                        executor.submit(compute_greedy_interaction, compartment_id)] = compartment_id

                with logger.progress_bar(total=len(compartment_to_futures),
                                         desc="Compartment Interaction Computation") as pbar:
                    for future in concurrent.futures.as_completed(compartment_to_futures):
                        compartment_id = compartment_to_futures[future]
                        pbar.update(1)
                        compartment_interaction_levels = future.result()
                        self.handle_interaction_result(compartment_id, compartment_interaction_levels)
        else:
            init_mp_database(db.db_dir)
            for compartment_id in logger.progress_bar(iterable=compartments,
                                                      desc='Compartment Interaction Computation'):
                compartment_interaction_levels = compute_greedy_interaction(compartment_id)
                self.handle_interaction_result(compartment_id, compartment_interaction_levels)

        self.merge_compartments(db, max_compartments, core_count)
