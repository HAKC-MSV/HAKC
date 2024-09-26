import argparse
import cProfile
import concurrent.futures
import io
import logging
import multiprocessing as mp
import os
import pstats
import shutil
import time
from enum import Enum
from typing import Type

import kuzu
import tqdm
import yaml

from hakc.yaml.HAKCDagObjects import HAKCCompartmentalization
from hakc.yaml.HAKCObjects import HAKCObject_constructors, HAKCFunction, HAKCGlobalVariable, HAKCSymbol, HAKCType, \
    QuotedString, HAKCScope

logger = logging.getLogger('hakc-dag')


def quoted_presenter(dumper, data):
    return dumper.represent_scalar('tag:yaml.org,2002:str', data, style='"')


class LoggingLevelEnum(Enum):
    CRITICAL = logging.CRITICAL
    ERROR = logging.ERROR
    WARNING = logging.WARNING
    INFO = logging.INFO
    DEBUG = logging.DEBUG


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False):
        self.db_dir = db_dir
        self.open(read_only=read_only)

    def close(self):
        self.conn.close()
        self.database.close()

    def open(self, read_only: bool = False):
        self.database = kuzu.Database(self.db_dir, read_only=read_only, max_num_threads=mp.cpu_count())
        self.conn = kuzu.Connection(self.database)

    def persist_compartmentalization(self, compartmentalization: HAKCCompartmentalization, create_schema: bool = False):
        if create_schema:
            logger.info(f'Creating schema')
            compartmentalization.create_schema(self.conn)

        logger.info(f'Persisting compartmentalization to database.')
        compartmentalization.persist_to_database(self.conn)
        logger.info(f'Done.')

    def get_symbol_by_hash(self, symbol_hash: int) -> HAKCSymbol | None:
        result = self._get_symbols(where_clause=f'WHERE sym.symbol_hash = {symbol_hash}')
        if len(result) == 0:
            return None
        if len(result) > 1:
            logger.error(f'Found {len(result)} symbols with hash {symbol_hash}')
            for sym in result:
                logger.error(f'{sym}')
            raise RuntimeError(f'Found {len(result)} symbols with hash {symbol_hash}')
        return result[0]

    def _execute_prepared_stmt(self, prepared_stmt: str, **kwargs):
        response = self.conn.execute(prepared_stmt, parameters=kwargs)
        return response

    def _get_symbols(self, where_clause: None | str = None, return_count: bool = False) -> list[HAKCSymbol] | int:
        cmd = [f"""
        MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.HasScopeTable}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.IsTypeTable}]->(ty:{HAKCType.get_table_name()})
        """]
        if where_clause is not None:
            cmd.append(where_clause)

        cmd.append("RETURN")
        return_str = """
            sym.Name, sym.DefiningFile, sym.DefiningLine, sym.is_function AS is_function, scope.Scope, 
            scope.LocalScopeName, ty.DebugType, ty.LLVMType
            """
        if return_count:
            cmd.append(f'COUNT(*)')
        else:
            cmd.append(f'{return_str}')
            cmd.append(f"""
                ORDER BY sym.Name, ty.DebugType, scope.Scope, scope.LocalScopeName;
            """)
        symbols = list()
        response = self._execute_prepared_stmt(prepared_stmt=" ".join(cmd))
        if response.has_next():
            if return_count:
                return int(response.get_next()[0])
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                symbol = self._create_symbol_from_response(**data)
                symbols.append(symbol)

        return symbols

    def _create_type_from_response(self, type_prefix: str = "ty.", **kwargs) -> HAKCType:
        type_data = {key.removeprefix(type_prefix): val for key, val in kwargs.items() if key.startswith(type_prefix)}
        if len(type_data) == 0:
            raise RuntimeError('No type data provided')
        ty = HAKCType(**type_data)
        return ty

    def _create_symbol_from_response(self, is_function: bool, type_prefix: str = "ty.", scope_prefix: str = "scope.",
                                     symbol_prefix: str = "sym.", **kwargs) -> HAKCSymbol:
        ty = self._create_type_from_response(type_prefix=type_prefix, **kwargs)
        scope_data = {key.removeprefix(scope_prefix): val for key, val in kwargs.items() if
                      key.startswith(scope_prefix)}
        if len(scope_data) == 0:
            raise RuntimeError('No scope data provided')
        scope = HAKCScope(**scope_data)
        symbol_data = {key.removeprefix(symbol_prefix): val for key, val in kwargs.items() if
                       key.startswith(symbol_prefix)}
        if len(symbol_data) == 0:
            raise RuntimeError('No symbol data provided')
        symbol_data['Type'] = ty
        symbol_data['Scope'] = scope

        if is_function:
            symbol = HAKCFunction(**symbol_data)
        else:
            symbol = HAKCGlobalVariable(**symbol_data)

        return symbol

    def get_indirect_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        cmd = f"""
            MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.IndirectCallTable}]->(ty: {HAKCType.get_table_name()})
            WHERE head.symbol_hash = $symbol_hash
            RETURN ty.DebugType, ty.LLVMType
            ORDER BY ty.DebugType, ty.LLVMType;
        """
        response = self._execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
        types = []
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                ty = self._create_type_from_response(**data)
                types.append(ty)
        return types

    def get_used_symbols(self, symbol: HAKCSymbol):
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.UsesSymbolTable}]->(tail:{HAKCSymbol.get_table_name()}),
            (sc:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.HasScopeTable}]-(tail)-[:{HAKCSymbol.IsTypeTable}]->(ty:{HAKCType.get_table_name()})
            WHERE head.symbol_hash=$symbol_hash
            RETURN tail.Name, tail.DefiningFile, tail.DefiningLine, tail.is_function AS is_function, sc.Scope,
            sc.LocalScopeName, ty.DebugType AS DebugType, ty.LLVMType AS LLVMType;
        """
        response = self._execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
        used_symbols = []
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                symbol = self._create_symbol_from_response(symbol_prefix='tail.', scope_prefix='sc.',
                                                           type_prefix='ty.', **data)
                used_symbols.append(symbol)

        return used_symbols

    def get_symbol_count(self):
        return self._get_symbols(return_count=True)

    def get_symbols(self):
        return self._get_symbols()


def get_loader() -> Type[yaml.SafeLoader]:
    loader = yaml.SafeLoader

    for yaml_tag, ctor in HAKCObject_constructors.items():
        loader.add_constructor(yaml_tag, ctor)

    return loader


def parse_yaml(filename: str):
    with open(filename, 'rb') as f:
        parsed_yaml = yaml.load(f, Loader=get_loader())
        compilation_unit = parsed_yaml["CU"]
        functions = parsed_yaml['functions'] if 'functions' in parsed_yaml else list()
        global_variables = parsed_yaml['globals'] if 'globals' in parsed_yaml else list()
    return compilation_unit, functions, global_variables


def compute_dag_edge_weight(head: HAKCSymbol, tail: HAKCSymbol, head_uses_tail: bool,
                            indirect_calls: list[HAKCType]) -> int:
    edge_weight = 0

    if head_uses_tail:
        edge_weight += 1

    if head.is_function():
        for indirect_call in indirect_calls:
            if indirect_call == tail.type:
                edge_weight += 1

    return edge_weight


def compute_dag_edges_for_symbol_with_conn(conn: HAKCDatabase, symbol_hash: int, symbol_hashes: list[int]):
    results = list()
    head = conn.get_symbol_by_hash(symbol_hash)
    if head is None:
        symbol_count = conn.get_symbol_count()
        raise RuntimeError(f'No symbol with hash {symbol_hash} found from {symbol_count} symbols')
    indirect_calls = conn.get_indirect_calls(head)
    used_symbols = conn.get_used_symbols(head)
    for tail_hash in symbol_hashes:
        tail = conn.get_symbol_by_hash(tail_hash)
        try:
            if symbol_hash != tail_hash:
                edge_weight = compute_dag_edge_weight(head, tail, tail in used_symbols, indirect_calls)
                if edge_weight > 0:
                    results.append((symbol_hash, tail_hash, edge_weight))
        except Exception as e:
            logger.error(f'Error computing edge weight between {head.name} and {tail.name}: {str(e)}')
    return results


def compute_dag_edges_for_symbol(db_dir: str, symbol_hash: int, symbol_hashes: list[int]):
    conn = HAKCDatabase(db_dir, read_only=True)

    try:
        results = compute_dag_edges_for_symbol_with_conn(conn, symbol_hash, symbol_hashes)
    except Exception as e:
        conn.close()
        raise e

    conn.close()
    return results


def add_dag_edges(compartmentalization: HAKCCompartmentalization):
    logger.info(f'Starting DAG edge computation')
    dag_edge_count = 0
    symbols = compartmentalization.get_symbols()
    with tqdm.tqdm(total=len(symbols)) as pbar:
        for head in symbols:
            indirect_calls = compartmentalization.get_indirect_calls(head)
            for tail in compartmentalization.get_symbols():
                dag_edge_weight = compute_dag_edge_weight(head, tail, compartmentalization.has_edge(head, tail),
                                                          indirect_calls)
                if dag_edge_weight > 0:
                    logger.debug(f'Adding DAG Edge between {head} -> {tail} with weight {dag_edge_weight}')
                    dag_edge_count += 1
                    compartmentalization.add_dag_edge(head, tail, dag_edge_weight)
            pbar.update(1)
    logger.info(f'Finished adding {dag_edge_count} DAG edges')


def add_symbols(compartmentalization: HAKCCompartmentalization, compilation_unit: str, functions: set[HAKCFunction],
                global_variables: set[HAKCGlobalVariable]):
    for func in functions:
        compartmentalization.add_symbol(func, compilation_unit)
    for glob in global_variables:
        compartmentalization.add_symbol(glob, compilation_unit)


def create_dag_single_thread(files: set[str], db_dir: str) -> HAKCCompartmentalization:
    compartmentalization = HAKCCompartmentalization()
    with tqdm.tqdm(total=len(files)) as pbar:
        for filename in sorted(files):
            compilation_unit, functions, global_variables = parse_yaml(filename)
            logger.debug(
                f'{compilation_unit} found {len(functions)} functions and {len(global_variables)} globals')
            add_symbols(compartmentalization, compilation_unit, functions, global_variables)
            pbar.update(1)

    add_dag_edges(compartmentalization)
    conn = HAKCDatabase(db_dir)
    conn.persist_compartmentalization(compartmentalization, create_schema=True)
    conn.close()
    return compartmentalization


def adjust_compartmentalization(compartmentalization: HAKCCompartmentalization, adjustments):
    logger.info(f'Adjusting DAG')

    if 'kernel' in adjustments:
        for symbol in compartmentalization.get_symbols():
            defining_unit = symbol.defining_file
            compartment_id = compartmentalization.get_compartment_id(symbol)
            original_compartment_id = compartment_id
            division_id = compartmentalization.get_division_id(symbol)

            change = defining_unit is None
            if defining_unit:
                for kernel_path in adjustments['kernel']:
                    if kernel_path in defining_unit:
                        compartment_id = HAKCCompartmentalization.kernel_compartment_id
                        division_id = HAKCCompartmentalization.kernel_division
                        change = True

                if change and 'compartmentalize' in adjustments and adjustments['compartmentalize'] is not None:
                    for compartmentalize_path in adjustments['compartmentalize']:
                        if compartmentalize_path in defining_unit:
                            change = False
                            break
            else:
                compartment_id = HAKCCompartmentalization.kernel_compartment_id
                division_id = HAKCCompartmentalization.kernel_division

            if change:
                logger.debug(
                    f'Changing Symbol {symbol} Compartment from {original_compartment_id} to {compartment_id}')
                compartmentalization.set_division(symbol, division_id, compartment_id)
            else:
                logger.info(f'Not changing Symbol {symbol}')


def create_dag_multithread(files: set[str], core_count: int, db_dir: str) -> HAKCCompartmentalization:
    logger.info(f'Starting multiprocess DAG creation using {core_count} cores')
    with concurrent.futures.ProcessPoolExecutor(max_workers=core_count) as executor:
        logger.info(f'Submitting {len(files)} parsing yaml tasks')
        futures_to_files = {}
        with tqdm.tqdm(total=len(files)) as pbar:
            for file in sorted(files):
                futures_to_files[executor.submit(parse_yaml, file)] = file
                pbar.update(1)
        logger.info("Completed")
        compartmentalization = HAKCCompartmentalization()
        conn = HAKCDatabase(db_dir)
        compartmentalization.create_schema(conn.conn)
        with tqdm.tqdm(total=len(files)) as pbar:
            for future in concurrent.futures.as_completed(futures_to_files):
                pbar.update(1)
                file = futures_to_files[future]
                try:
                    compilation_unit, functions, global_variables = future.result()
                    logger.debug(
                        f'{compilation_unit} found {len(functions)} functions and {len(global_variables)} globals')
                    add_symbols(compartmentalization, compilation_unit, functions, global_variables)
                except Exception as e:
                    logger.error(f'Error parsing {file}: {str(e)}')

        conn.persist_compartmentalization(compartmentalization)
        logger.info(f'Total symbols {len(conn.get_symbols())}')
        conn.close()
        logger.info(f'Starting DAG edge computation')

        dag_edges_added = 0
        futures_to_symbol = dict()
        symbol_hashes = compartmentalization.get_symbol_hashes()

        with tqdm.tqdm(total=len(symbol_hashes)) as pbar:
            try:
                for symbol_hash in symbol_hashes:
                    future = executor.submit(compute_dag_edges_for_symbol, db_dir, symbol_hash, symbol_hashes)
                    futures_to_symbol[future] = symbol_hash
                    pbar.update(1)
            except Exception as e:
                logger.error(f'Error submitting tasks: {str(e)}')
                executor.shutdown(wait=False, cancel_futures=True)
                raise e

        with tqdm.tqdm(total=len(futures_to_symbol)) as pbar:
            try:
                for future in concurrent.futures.as_completed(futures_to_symbol):
                    pbar.update(1)
                    symbol_hash = futures_to_symbol[future]
                    try:
                        edge_weights = future.result()
                        for (head_hash, tail_hash, dag_edge_weight) in edge_weights:
                            head = compartmentalization.get_symbol_by_hash(head_hash)
                            tail = compartmentalization.get_symbol_by_hash(tail_hash)
                            logger.debug(
                                f'Adding DAG Edge between {head} -> {tail} with weight {dag_edge_weight}')
                            compartmentalization.add_dag_edge(head, tail, dag_edge_weight)
                            dag_edges_added += 1
                    except Exception as e:
                        symbol = compartmentalization.get_symbol_by_hash(symbol_hash)
                        if symbol is None:
                            symbol = symbol_hash
                        logger.error(f'Error computing DAG edge for symbol {symbol}: {str(e)}')
            except KeyboardInterrupt as ki:
                logger.info(f'Stopping edge computation')
                executor.shutdown(wait=False, cancel_futures=True)
                raise ki

    logger.info(f'Finished adding {dag_edges_added} DAG edges')
    conn.open()
    conn.persist_compartmentalization(compartmentalization)
    conn.close()

    return compartmentalization


def create_new_dag(analysis_root: str, single_thread: bool, core_count: int, db_dir: str,
                   delete_existing_db: bool) -> HAKCCompartmentalization:
    logger.info(f'Finding DAG files starting from {os.path.abspath(analysis_root)}')
    filenames = set()
    for root, subdirs, files in os.walk(analysis_root):
        for f in files:
            filename = os.path.join(root, f)
            if not filename.endswith(".dag.yml"):
                continue
            filenames.add(str(filename))

    if delete_existing_db:
        if os.path.exists(db_dir) and os.path.isdir(db_dir):
            logger.info(f'Removing existing database at {db_dir}')
            shutil.rmtree(db_dir)

    logger.info(f'Starting DAG construction from {len(filenames)} files')
    start = time.time()
    if single_thread:
        compartmentalization = create_dag_single_thread(filenames, db_dir)
    else:
        core_count = max(1, core_count)
        compartmentalization = create_dag_multithread(filenames, core_count, db_dir)

    end = time.time()
    logger.info(f'Finished creating DAG.')
    logger.info(f'    Total Time: {end - start} seconds')
    logger.info(f'    Type Count: {len(compartmentalization.get_types())}')
    logger.info(f'  Global Count: {len(compartmentalization.get_global_variables())}')
    logger.info(f'Function Count: {len(compartmentalization.get_functions())}')
    return compartmentalization


def parse_log_level(level_string: str):
    for level in LoggingLevelEnum:
        if level.name == level_string.upper():
            return level
    raise RuntimeError(f'Invalid log level {level_string}')


def setup_logging(log_file: str, log_level: LoggingLevelEnum, log_mode: str):
    log_formatter = logging.Formatter("%(asctime)s [%(threadName)-12.12s] [%(levelname)-5.5s]  %(message)s")
    logger.setLevel(log_level.value)
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(log_formatter)
    logger.addHandler(console_handler)
    if log_file is not None:
        file_handler = logging.FileHandler(log_file, mode=log_mode)
        file_handler.setFormatter(log_formatter)
        logger.addHandler(file_handler)


def output_profile_stats(profile):
    s = io.StringIO()
    ps = pstats.Stats(profile, stream=s).sort_stats('tottime')
    ps.print_stats()
    logger.info(s.getvalue())


def main():
    parser = argparse.ArgumentParser(description='Kernel Data Access Analysis')
    parser.add_argument('--dag-files-root', help='Root of DAG Yaml files', dest='dag_files_root')
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-mode', default='w', dest='log_mode')
    parser.add_argument('--single-thread', dest='single_thread', action='store_true',
                        help='Run analysis without multiprocessing')
    parser.add_argument('--output-yaml', dest='output_yaml', action='store_true',
                        help='Output compartmentalization YAML')
    parser.add_argument('--output-yaml-path', dest='output_yaml_path', help='Path to output DAG YAML')
    parser.add_argument('--create-dag', dest='create_dag', action='store_true', help='Create new DAG')
    parser.add_argument("--adjust", help='Adjust compartmentalization', action='store_true')
    parser.add_argument('--adjust-path', dest='adjust_path', help='Path to adjustment YAML')
    parser.add_argument('--profile', dest='profile', action='store_true')
    parser.add_argument('--core-count', help='Max cores to use for analysis', dest='core_count',
                        default=mp.cpu_count() - 1, type=int)
    parser.add_argument('--db-dir', help='Directory to use for the kuzu database', dest='kuzu_dir',
                        default=None)
    parser.add_argument('--delete-existing-db', action='store_true',
                        help='Deletes existing database when creating a new compartmentalization', dest='delete_db',
                        default=True)

    args = parser.parse_args()

    profile = None
    setup_logging(log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)

    if args.kuzu_dir is None or len(args.kuzu_dir) == 0:
        raise RuntimeError(f'Must specify a kuzu directory')

    logger.info(f'Using kuzu database at {args.kuzu_dir}')
    compartmentalization = None

    if args.profile:
        profile = cProfile.Profile()

    if args.create_dag:
        if profile:
            profile.enable()
        compartmentalization = create_new_dag(args.dag_files_root, args.single_thread, args.core_count, args.kuzu_dir,
                                              args.delete_db)
        if profile:
            profile.disable()
            output_profile_stats(profile)

    if args.adjust:
        logger.info(f'Adjusting compartmentalization based on {args.adjust_path}')
        with open(args.adjust_path, 'r') as f:
            adjustments = yaml.safe_load(f)
        compartmentalization = HAKCCompartmentalization.CreateFromDatabase(args.kuzu_dir)
        adjust_compartmentalization(compartmentalization, adjustments)
        logger.info("Done")

    if args.output_yaml:
        if compartmentalization is None:
            raise RuntimeError(f'No compartmentalization')
        with open(args.output_yaml_path, 'w') as f:
            logger.info(f"Outputting YAML to {args.output_yaml_path}")
            yaml.add_representer(QuotedString, quoted_presenter)
            yaml.dump(compartmentalization.to_yaml(), f, width=float("inf"))
            logger.info('Done')


if __name__ == "__main__":
    main()
