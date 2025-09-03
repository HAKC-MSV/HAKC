import itertools
import logging

import pandas as pd
from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import HAKCLogger
from hakc.HAKCObjects import HAKCCompartment, HAKCDivision, HAKCSymbol

from . import FlexCAlgorithm

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')


class EqualCompartmentSizeAlgorithm(FlexCAlgorithm.FlexCAlgorithm):
    title = "ecs"
    aliases = ['e']
    help = "Place symbols in equally sized compartments"

    def __init__(self, parser, **kwargs):
        super().__init__(parser, **kwargs)

    def add_command_line_arguments(self, parser):
        parser.add_argument('--symbols-per-compartment', type=int, help='Max number of symbols in a compartment',
                            dest="count", required=True)

    def run(self, db: HAKCDatabase, **kwargs):
        symbols_per_compartment = kwargs.get('count')
        db.close()
        db.open()

        symbol_hashes = db.get_all_symbol_hashes()
        logger.info(f'Separating {len(symbol_hashes)} into compartments of size {symbols_per_compartment}')

        current_compartment_id = 1
        compartment_map = dict()
        for batched_symbols in logger.progress_bar(iterable=itertools.batched(symbol_hashes, symbols_per_compartment),
                                                   desc="Compartmentalizing symbols"):
            for symbol_hash in batched_symbols:
                compartment_map[symbol_hash] = current_compartment_id
            current_compartment_id += 1

        logger.info(f'Deleting compartments')
        div = HAKCDivision.get_table_name()
        comp = HAKCCompartment.get_table_name()
        div_comp_edge = HAKCDivision.relation_compartment
        cmd = f"""
        MATCH (d:{div})-[:{div_comp_edge}]->(c:{comp})
        DETACH DELETE d, c;
        """
        db.execute_prepared_stmt(cmd)

        compartment_ids = list(range(1, current_compartment_id))
        division_hashes = [hash(compartment_id) for compartment_id in compartment_ids]
        access_tokens = [compartment_id << 16 & 1 for compartment_id in compartment_ids]
        logger.info(f'Inserting {current_compartment_id} new divisions')
        df = pd.DataFrame({
            'division_hash': division_hashes,
            'AccessToken': access_tokens,
            'DivisionID': list(itertools.repeat(1, len(compartment_ids))),
        })
        logger.info(f'{df}')
        db.insert_from_dataframe(HAKCDivision.get_table_name(), df)

        logger.info(f'Inserting compartments')
        df = pd.DataFrame({
            'CompartmentID': compartment_ids,
            'EntryToken': access_tokens
        })
        db.insert_from_dataframe(HAKCCompartment.get_table_name(), df)

        logger.info(f'Adding division compartment edges')
        df = pd.DataFrame({
            'from': division_hashes,
            'to': compartment_ids
        })
        db.insert_from_dataframe(HAKCDivision.relation_compartment, df)

        logger.info(f'Compartmentalization now has {len(db.get_all_compartments())} compartments')
