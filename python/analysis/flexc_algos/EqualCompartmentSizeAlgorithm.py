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

        logger.info(f"Getting all ordered symbols")
        all_symbol_hashes = db.get_all_symbol_hashes()

        symbol_hashes = list()
        added_hashes = set()
        s0hashes = set()
        s1hashes = set()
        remaining_hashes = set()
        cmd0 = f"""
        MATCH (s0:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_dag}]->(s1:{HAKCSymbol.get_table_name()})
        RETURN s0.{str(HAKCSymbol.get_primary_key())} as SymbolHash0, 
        s1.{str(HAKCSymbol.get_primary_key())} as SymbolHash1, 
        e.weight as Weight
        ORDER BY Weight DESC, SymbolHash0 DESC
        """
        data = db.execute_prepared_stmt(cmd0).get_as_df().astype(
            {'SymbolHash0': int, 'SymbolHash1': int, 'Weight': int})
        for _, row in data.iterrows():
            s0hash = int(row['SymbolHash0'])
            s1hash = int(row['SymbolHash1'])
            if s0hash not in added_hashes:
                symbol_hashes.append(s0hash)
                added_hashes.add(s0hash)
            s0hashes.add(s0hash)

            if s1hash not in added_hashes:
                symbol_hashes.append(s1hash)
                added_hashes.add(s1hash)
            s1hashes.add(s1hash)

        for symbol_hash in all_symbol_hashes:
            symbol_hash = int(symbol_hash)
            if symbol_hash not in added_hashes:
                symbol_hashes.append(symbol_hash)
                added_hashes.add(symbol_hash)
                remaining_hashes.add(symbol_hash)
        del data

        logger.info(f'Separating {len(symbol_hashes)} symbols into compartments of size {symbols_per_compartment}')

        current_compartment_id = 1
        compartment_map = dict()
        for batched_symbols in logger.progress_bar(iterable=itertools.batched(symbol_hashes, symbols_per_compartment),
                                                   desc="Compartmentalizing symbols"):
            for symbol_hash in batched_symbols:
                compartment_map[int(symbol_hash)] = current_compartment_id
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
        access_tokens = [compartment_id << 16 | 1 for compartment_id in compartment_ids]
        logger.info(f'Inserting {len(division_hashes)} new divisions')
        df = pd.DataFrame({
            'division_hash': division_hashes,
            'AccessToken': access_tokens,
            'DivisionID': list(itertools.repeat(1, len(compartment_ids))),
        })
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

        logger.info(f'Adding symbol division edges')
        symbols = list()
        divisions = list()
        for symbol_hash, compartment_id in compartment_map.items():
            symbols.append(int(symbol_hash))
            divisions.append(hash(compartment_id))

        df = pd.DataFrame({
            'from': symbols,
            'to': divisions
        })
        db.insert_from_dataframe(HAKCSymbol.relation_division, df)

        logger.info(f'Compartmentalization now has {len(db.get_all_compartments())} compartments')
        for compartment_id in [compartment_ids[0], compartment_ids[-1]]:
            logger.info(
                f'Compartment {compartment_id} has {len(db.get_all_symbol_hashes_in_compartment(compartment_id))} symbols')
