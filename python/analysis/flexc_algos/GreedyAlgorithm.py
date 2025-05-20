import concurrent.futures
import itertools
import logging
import math

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
    compartment_interactions = dict()
    for compartment_2 in valid_targets:
        compartment_interaction_level = GreedyAlgorithm.compute_compartment_interaction(mp_conn, compartment_id,
                                                                                        compartment_2)
        logger.debug(
            f'{compartment_id} -> {compartment_2}: {compartment_interaction_level}')
        compartment_interactions[compartment_2] = compartment_interaction_level
    return compartment_interactions


class GreedyAlgorithm(FlexCAlgorithm.FlexCAlgorithm):
    title = "greedy"
    aliases = ['g']
    help = "Partition the DAG greedily"

    def __init__(self, parser, **kwargs):
        super().__init__(parser, **kwargs)
        self.compartment_interactions = list()

    def add_command_line_arguments(self, parser):
        parser.add_argument('--max-compartments', type=int, help='Max resultant compartments',
                            dest="max_compartments", required=True)

    @staticmethod
    def compute_compartment_interaction(db: HAKCDatabase, compartment_id_1: int,
                                        compartment_id_2: int) -> int:
        cmd = f"""
        MATCH
        (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.InCompartmentTable}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.InDivisionTable}]-(sym1:{HAKCSymbol.get_table_name()})-[dag:{HAKCSymbol.DagEdgeTable}]->(sym2:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.InDivisionTable}]->(div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(comp2:{HAKCCompartment.get_table_name()})
        WHERE comp1.{str(HAKCCompartment.get_primary_key())} = $comp1_id AND comp2.{str(HAKCCompartment.get_primary_key())} = $comp2_id
        RETURN sum(dag.weight) AS interaction_level
        """
        response = db.execute_prepared_stmt(cmd, comp1_id=compartment_id_1, comp2_id=compartment_id_2)
        data = response.get_as_df()
        if data.empty:
            return 0
        else:
            info = data.to_dict(orient='records')[0]
            result = info['interaction_level']
            if math.isnan(result):
                result = 0
            return result

    def get_compartments_to_merge(self, db: HAKCDatabase):
        cmd = f"""
        MATCH 
        (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.InCompartmentTable}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.InDivisionTable}]-(sym1:{HAKCSymbol.get_table_name()})-[dag:{HAKCSymbol.DagEdgeTable}]->(sym2:{HAKCSymbol.get_table_name()})
        RETURN DISTINCT comp1.{str(HAKCCompartment.get_primary_key())} AS CompartmentID
        ORDER BY CompartmentID
        """
        response = db.execute_prepared_stmt(cmd)
        compartments = response.get_as_df()['CompartmentID'].tolist()[:20]

        return compartments

    def handle_interaction_result(self, head_compartment_id: int, compartment_interactions: dict):
        for compartment_id, interaction in compartment_interactions.items():
            self.compartment_interactions.append((head_compartment_id, compartment_id, interaction))

    def reset_interactions(self):
        self.compartment_interactions.clear()

    def merge_compartments(self, db: HAKCDatabase, max_compartments: int):
        db.close()
        db.open()
        resulting_compartment_map = dict()
        for head_compartment_id, tail_compartment_id, max_interaction in itertools.islice(
                sorted(self.compartment_interactions, key=lambda interaction: interaction[2],
                       reverse=True), 0,
                max(len(self.compartment_interactions) - max_compartments, max_compartments)):
            logger.debug(f'Merging {head_compartment_id} -> {tail_compartment_id} with interaction {max_interaction}')
            if head_compartment_id not in resulting_compartment_map:
                resulting_compartment_map[tail_compartment_id] = head_compartment_id
            else:
                resulting_compartment_map[tail_compartment_id] = resulting_compartment_map[head_compartment_id]

        with logger.progress_bar(total=len(resulting_compartment_map),
                                 desc="Merging Compartments") as pbar:
            for tail_compartment_id, head_compartment_id in resulting_compartment_map.items():
                db.merge_compartments(head_compartment_id, tail_compartment_id)
                pbar.update(1)

        logger.info(f'Removed {len(resulting_compartment_map)} compartments')
        db.close()
        db.open(read_only=True)

    def run(self, db: HAKCDatabase, **kwargs):
        max_compartments: int = kwargs.get('max_compartments')
        core_count: int = int(kwargs.get('core_count', 1))
        compartments = self.get_compartments_to_merge(db)

        while len(compartments) > max_compartments:
            self.reset_interactions()
            logger.info(f'Creating at most {max_compartments} compartments from {len(compartments)} compartments')

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
            compartments = self.get_compartments_to_merge(db)
