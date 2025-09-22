import logging
import os

import numpy as np
import pandas as pd
from hakc.HAKCBase import HAKCDBNode
from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import HAKCLogger
from hakc.HAKCObjects import HAKCCompartment, HAKCDefinitionLocation, HAKCSymbol, HAKCDivision

from . import FlexCAlgorithm

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')


class FilesystemAlgorithm(FlexCAlgorithm.FlexCAlgorithm):
    title = "filesystem"
    aliases = ['f']
    help = "Partition the DAG by filesystem"

    def __init__(self, parser, **kwargs):
        super().__init__(parser, **kwargs)

    def add_command_line_arguments(self, parser):
        parser.add_argument('--max-depth', type=int, help='Max directory depth to compartmentalize',
                            dest="max_depth", required=True)

    def get_all_symbol_hashes_and_files(self, db: HAKCDatabase) -> dict[int, str]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})
        OPTIONAL MATCH (sym)-[:{HAKCSymbol.DefinedInTable}]->(cu:{HAKCDefinitionLocation.get_table_name()})
        RETURN sym.{str(HAKCSymbol.get_primary_key())} AS symbol_hash, cu.{str(HAKCDefinitionLocation.get_primary_key())} AS filename
        """

        response = db.execute_prepared_stmt(cmd)
        data = response.get_as_df()
        symbols_and_filenames = data.set_index('symbol_hash')['filename'].replace({np.nan: None}).to_dict()

        return symbols_and_filenames

    def find_longest_path_prefix(self, filenames: set[str]) -> str:
        prefix = None

        for filename in filenames:
            if prefix is None:
                prefix = filename
                continue

            split_prefix = prefix.split(os.path.sep)
            split_filename = filename.split(os.path.sep)

            for i in range(len(split_prefix)):
                if split_prefix[i] != split_filename[i]:
                    prefix = os.path.sep.join(split_prefix[:i])
        if prefix is None:
            prefix = ""
        return prefix

    def get_max_depth_filename(self, filename: str, prefix_length: int, max_depth: int):
        split_filename = filename.split(os.path.sep)
        file_path_tokens = split_filename[min(prefix_length, len(split_filename)):]
        compartment_path = file_path_tokens[:min(len(file_path_tokens), max_depth)]
        return os.path.sep.join(compartment_path)

    def construct_node_dataframe(self, objs: set[HAKCDBNode]) -> pd.DataFrame:
        data_to_persist = dict()
        for obj in objs:
            db_data = obj.get_db_data()
            if len(data_to_persist) > 0 and len(db_data) != len(data_to_persist):
                logger.error(
                    f'Object {obj} does not have all the data needed. Data needed is {" ".join(sorted(data_to_persist.keys()))} and data provided is {" ".join(sorted([column.column_name for column in db_data.keys()]))}')
            for column, data in db_data.items():
                if data is None:
                    logger.debug(f'Object {obj} has None for column {column.column_name}')
                    data = column.column_type.default_value
                if column.column_name not in data_to_persist:
                    data_to_persist[column.column_name] = list()
                data_to_persist[column.column_name].append(data)
        return pd.DataFrame(data_to_persist)

    def construct_edge_dataframe(self, edge_dict: dict[HAKCDBNode, HAKCDBNode]) -> pd.DataFrame:
        head_primary_keys = list()
        tail_primary_keys = list()
        for head, tail in edge_dict.items():
            head_primary_key = head.get_primary_key_data()
            tail_primary_key = tail.get_primary_key_data()
            head_primary_keys.append(head_primary_key)
            tail_primary_keys.append(tail_primary_key)
        return pd.DataFrame({
            'from': head_primary_keys,
            'to': tail_primary_keys
        })

    def run(self, db: HAKCDatabase, **kwargs):
        db.close()
        db.open(read_only=False)
        symbols_and_filenames = self.get_all_symbol_hashes_and_files(db)
        max_depth = kwargs['max_depth']

        filenames = set()
        symbols_missing_definition = set()
        for symbol_hash, filename in symbols_and_filenames.items():
            if filename is not None:
                filenames.add(os.path.abspath(filename))
            else:
                symbols_missing_definition.add(symbol_hash)
        logger.info(
            f"Found {len(symbols_and_filenames)} symbols with {len(symbols_missing_definition)} missing definition files and "
            f"{len(filenames)} unique filenames")

        longest_common_prefix = self.find_longest_path_prefix(filenames)
        logger.info(f"Longest common prefix is {longest_common_prefix}")
        split_prefix = longest_common_prefix.split(os.path.sep)
        division_filename_map = dict()
        division_compartment_map = dict()
        compartment_id = 1

        for filename in filenames:
            max_depth_filename = self.get_max_depth_filename(filename, len(split_prefix), max_depth)
            if max_depth_filename not in division_filename_map:
                division = HAKCDivision(DivisionID=1, compartment_id=compartment_id)
                compartment = HAKCCompartment(CompartmentID=compartment_id, Divisions=[division])
                division_filename_map[max_depth_filename] = division
                division_compartment_map[division] = compartment
                compartment_id += 1

        missing_cu_division = HAKCDivision(DivisionID=1, compartment_id=compartment_id)
        missing_cu_compartment = HAKCCompartment(CompartmentID=compartment_id, Divisions=[missing_cu_division])
        division_compartment_map[missing_cu_division] = missing_cu_compartment

        logger.info(f"Removing existing compartments")
        db.delete_all_compartments()
        logger.info(f"Creating {len(division_compartment_map.values())} compartments")
        df = self.construct_node_dataframe(set(division_compartment_map.values()))
        db.insert_from_dataframe(table_name=HAKCCompartment.get_table_name(), df=df)
        divisions = set(division_compartment_map.keys())
        logger.info(f'Creating {len(divisions)} divisions')
        df = self.construct_node_dataframe(divisions)
        db.insert_from_dataframe(table_name=HAKCDivision.get_table_name(), df=df)
        logger.info(f"Creating Division->Compartment edges")
        df = self.construct_edge_dataframe(division_compartment_map)
        db.insert_from_dataframe(table_name=HAKCDivision.InCompartmentTable, df=df)
        logger.info(f'Creating Symbol->Division edges')
        symbol_keys = list()
        division_keys = list()
        for symbol_hash, filename in symbols_and_filenames.items():
            symbol_keys.append(symbol_hash)
            if filename is not None:
                max_depth_filename = self.get_max_depth_filename(os.path.abspath(filename), len(split_prefix),
                                                                 max_depth)
                division = division_filename_map[max_depth_filename]
            else:
                division = missing_cu_division
            division_keys.append(division.get_primary_key_data())
        df = pd.DataFrame({
            'from': symbol_keys,
            'to': division_keys
        })
        db.insert_from_dataframe(table_name=HAKCSymbol.InDivisionTable, df=df)
        logger.info(f"The compartmentalization now has {len(db.get_all_compartments())} Compartments")
