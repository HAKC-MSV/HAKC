import logging

import numpy as np
from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import HAKCLogger
from hakc.HAKCObjects import HAKCCompartment, HAKCCompilationUnit, HAKCSymbol

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
        pass

    def get_all_symbol_hashes_and_files(self, db: HAKCDatabase) -> dict[int, str]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})
        OPTIONAL MATCH (sym)-[:{HAKCSymbol.DefinedInTable}]->(cu:{HAKCCompilationUnit.get_table_name()})
        RETURN sym.{str(HAKCSymbol.get_primary_key())} AS symbol_hash, cu.{str(HAKCCompilationUnit.get_primary_key())} AS filename
        """

        response = db.execute_prepared_stmt(cmd)
        data = response.get_as_df()
        symbols_and_filenames = data.set_index('symbol_hash')['filename'].replace({np.nan: None}).to_dict()

        return symbols_and_filenames

    def run(self, db: HAKCDatabase, **kwargs):
        symbols_and_filenames = self.get_all_symbol_hashes_and_files(db)

        for symbol_hash, filename in symbols_and_filenames.items():
            print(f'{symbol_hash}: {filename}')
