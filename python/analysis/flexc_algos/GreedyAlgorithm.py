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

    def get_compartments_to_merge(self, db: HAKCDatabase):
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

    def merge_compartment(self, edge):
        head_compartment_id = edge[0]
        tail_compartment_id = edge[1]
        logger.debug(
            f'Merging {head_compartment_id} -> {tail_compartment_id} with interaction {edge[2].get(GreedyAlgorithm.weight_attr, 0)}')
        existing_merged_nodes = self.compartment_interactions.nodes[head_compartment_id][
            GreedyAlgorithm.merged_compartment_attr]
        for merged_node in existing_merged_nodes:
            self.compartment_interactions.nodes[tail_compartment_id][GreedyAlgorithm.merged_compartment_attr].add(
                merged_node)
        self.compartment_interactions.nodes[tail_compartment_id][GreedyAlgorithm.merged_compartment_attr].add(
            head_compartment_id)
        edges_to_add = list()
        edges_to_adjust = list()
        edges_removed = list()
        for in_edge in self.compartment_interactions.in_edges(head_compartment_id, data=True):
            edges_removed.append(in_edge)
            head = in_edge[0]
            if head == tail_compartment_id:
                continue

            weight = in_edge[2].get(GreedyAlgorithm.weight_attr, 0)
            if self.compartment_interactions.has_edge(head, tail_compartment_id):
                edges_to_adjust.append((head, tail_compartment_id, weight))
            else:
                edges_to_add.append((head, tail_compartment_id, weight))

        for out_edge in self.compartment_interactions.out_edges(head_compartment_id, data=True):
            edges_removed.append(out_edge)
            tail = out_edge[1]
            if tail == tail_compartment_id:
                continue

            weight = out_edge[2].get(GreedyAlgorithm.weight_attr, 0)
            if self.compartment_interactions.has_edge(tail_compartment_id, tail):
                edges_to_adjust.append((tail_compartment_id, tail, weight))
            else:
                edges_to_add.append((tail_compartment_id, tail, weight))

        self.compartment_interactions.remove_node(head_compartment_id)

        edges_added = list()
        for head, tail, weight in edges_to_add:
            self.compartment_interactions.add_edge(head, tail, **{GreedyAlgorithm.weight_attr: weight})
            edges_added.append((head, tail, {GreedyAlgorithm.weight_attr: weight}))

        edges_adjusted = list()
        for head, tail, weight in edges_to_adjust:
            if not self.compartment_interactions.has_edge(head, tail):
                logger.error(f'{head} -> {tail} does not exist')
                continue
            new_weight = self.compartment_interactions[head][tail][GreedyAlgorithm.weight_attr] + weight
            self.compartment_interactions[head][tail][GreedyAlgorithm.weight_attr] = new_weight

            edges_adjusted.append(
                (head, tail, {GreedyAlgorithm.weight_attr: new_weight, 'old_weight': new_weight - weight}))

        return edges_added, edges_adjusted, edges_removed

    def add_edge_to_weights(self, edge, weights):
        weight = edge[2].get(GreedyAlgorithm.weight_attr, 0)
        if weight not in weights:
            weights[weight] = dict()
        if edge[0] not in weights[weight]:
            weights[weight][edge[0]] = list()
        weights[weight][edge[0]].append(edge[1])

    def remove_edge_from_weights(self, edge, weights):
        weight = edge[2].get(GreedyAlgorithm.weight_attr, 0)
        if weight in weights:
            if edge[0] in weights[weight]:
                try:
                    weights[weight][edge[0]].remove(edge[1])
                except ValueError:
                    pass
                if len(weights[weight][edge[0]]) == 0:
                    del weights[weight][edge[0]]
            if len(weights[weight]) == 0:
                del weights[weight]

    def merge_compartments(self, db: HAKCDatabase, max_compartments: int):
        db.close()
        db.open()

        with logger.progress_bar(total=len(self.compartment_interactions.edges), desc="Organizing Weights") as pbar:
            weights = dict()

            for sorted_edge in sorted(self.compartment_interactions.edges(data=True),
                                      key=lambda edge: edge[2].get(GreedyAlgorithm.weight_attr, 0)):
                self.add_edge_to_weights(sorted_edge, weights)
                pbar.update(1)

        with logger.progress_bar(total=len(self.compartment_interactions.edges),
                                 desc="Merging Compartments") as pbar:
            while len(self.compartment_interactions) > max_compartments and len(weights) > 0:
                max_weight = max(weights.keys())
                head, tails = next(iter(weights[max_weight].items()))
                max_edge = (head, tails.pop(), {GreedyAlgorithm.weight_attr: max_weight})
                if self.compartment_interactions.has_node(max_edge[0]) and self.compartment_interactions.has_node(
                        max_edge[1]):
                    edges_added, edges_adjusted, edges_removed = self.merge_compartment(max_edge)
                    for removed_edge in edges_removed:
                        self.remove_edge_from_weights(removed_edge, weights)
                    for added_edge in edges_added:
                        self.add_edge_to_weights(added_edge, weights)
                    for adjusted_edge in edges_adjusted:
                        new_weight = adjusted_edge[2].get(GreedyAlgorithm.weight_attr, 0)
                        old_weight = adjusted_edge[2].get('old_weight', 0)
                        adjusted_edge[2][GreedyAlgorithm.weight_attr] = old_weight
                        self.remove_edge_from_weights(adjusted_edge, weights)
                        adjusted_edge[2][GreedyAlgorithm.weight_attr] = new_weight
                        self.add_edge_to_weights(adjusted_edge, weights)

                pbar.update(1)
                if len(self.compartment_interactions.edges()) == 0:
                    logger.info(
                        f"No more edges to merge. Compartment count is {len(self.compartment_interactions)}")
                    break

        final_compartment_map = dict()

        for compartment_id in self.compartment_interactions.nodes():
            for merged_compartment_id in self.compartment_interactions.nodes[compartment_id][
                GreedyAlgorithm.merged_compartment_attr]:
                final_compartment_map[merged_compartment_id] = compartment_id

        merge_manifest_path = os.path.join(db.db_dir, 'merge-manifest.yml')
        logger.info(f'Writing merge manifest to {merge_manifest_path}')
        with open(merge_manifest_path, 'w') as f:
            yaml.dump(final_compartment_map, f, indent=2)

        db.merge_compartments(final_compartment_map)

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

        self.merge_compartments(db, max_compartments)
