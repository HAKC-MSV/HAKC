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
import polars as pl

import kuzu
import tqdm
import yaml
import itertools

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

def batched(iterable, n):
    if n < 1:
        raise ValueError('n must be at least one')
    iterator = iter(iterable)
    while batch := tuple(itertools.islice(iterator, n)):
        yield batch


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.db_dir = db_dir
        self.open(read_only=read_only, max_num_threads=max_num_threads)

    def close(self):
        self.conn.close()
        self.database.close()

    def open(self, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.database = kuzu.Database(self.db_dir, read_only=read_only, max_num_threads=max_num_threads)
        self.conn = kuzu.Connection(self.database)

    def persist_compartmentalization(self, compartmentalization: HAKCCompartmentalization, create_schema: bool = False):
        if create_schema:
            logger.info(f'Creating schema')
            compartmentalization.create_schema(self.conn)

        logger.info(f'Persisting compartmentalization to database.')
        compartmentalization.persist_to_database(self.conn)
        logger.info(f'Done.')


    def persist_dag_edges(self, dag_edge_data):
        head_hashes = list()
        tail_hashes = list()
        edge_weights = list()

        for head_hash, edge_data in dag_edge_data.items():
            for tail_hash, edge_weight in edge_data.items():
                head_hashes.append(head_hash)
                tail_hashes.append(tail_hash)
                edge_weights.append(edge_weight)

        df = pl.DataFrame({
            "from": head_hashes,
            "to": tail_hashes,
            "weight": edge_weights
        })
        self.conn.execute(f'COPY {HAKCSymbol.DagEdgeTable} FROM df')

    def get_symbol_by_hash(self, symbol_hashes: list[int]) -> list[HAKCSymbol]:
        try:
            result = self._get_symbols(where_clause=f'WHERE sym.symbol_hash in [{", ".join([str(sh) for sh in symbol_hashes])}]')
            return result
        except Exception as e:
            logger.error(f'get_symbol_by_hash failed')
            raise e

    def get_dag_edges(self, symbol_hash: int) -> dict[int]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.DirectCallTable}]->(direct:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key().column_name} = $symbol_hash
        RETURN direct.{HAKCSymbol.get_primary_key().column_name} AS {HAKCFunction.DirectCallTable}
        UNION ALL
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.UsesSymbolTable}]->(uses:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key().column_name} = $symbol_hash
        RETURN uses.{HAKCSymbol.get_primary_key().column_name} AS {HAKCSymbol.UsesSymbolTable}
        UNION ALL
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.IndirectCallTable}]->(:{HAKCType.get_table_name()})<-[:{HAKCSymbol.IsTypeTable}]-(indirect:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key().column_name} = $symbol_hash
        RETURN indirect.{HAKCSymbol.get_primary_key().column_name} AS {HAKCFunction.IndirectCallTable}
        ;
        """

        response = self._execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        if response.has_next():
            return response.get_as_df().to_dict()
        else:
            return {}

    def _execute_prepared_stmt(self, prepared_stmt: str, **kwargs):
        response = self.conn.execute(prepared_stmt, parameters=kwargs)
        return response

    def _get_symbols(self, where_clause: None | str = None, return_count: bool = False, limit: int = 0) -> list[HAKCSymbol] | int:
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
                ORDER BY sym.Name, ty.DebugType, scope.Scope, scope.LocalScopeName
            """)
        if limit > 0:
            cmd.append(f'LIMIT {limit}')
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
        try:
            response = self._execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
            types = []
            if response.has_next():
                info = response.get_as_df()
                for data in info.to_dict(orient='records'):
                    ty = self._create_type_from_response(**data)
                    types.append(ty)
        except Exception as e:
            logger.error(f'get_indirect_calls failed')
            raise e
        return types

    def get_used_symbols(self, symbol: HAKCSymbol):
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.UsesSymbolTable}]->(tail:{HAKCSymbol.get_table_name()}),
            (sc:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.HasScopeTable}]-(tail)-[:{HAKCSymbol.IsTypeTable}]->(ty:{HAKCType.get_table_name()})
            WHERE head.symbol_hash=$symbol_hash
            RETURN tail.Name, tail.DefiningFile, tail.DefiningLine, tail.is_function AS is_function, sc.Scope,
            sc.LocalScopeName, ty.DebugType, ty.LLVMType;
        """
        try:
            response = self._execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
            used_symbols = []
            if response.has_next():
                info = response.get_as_df()
                for data in info.to_dict(orient='records'):
                    symbol = self._create_symbol_from_response(symbol_prefix='tail.', scope_prefix='sc.',
                                                               type_prefix='ty.', **data)
                    used_symbols.append(symbol)
        except Exception as e:
            logger.error(f'get_used_symbols failed')
            raise e

        return used_symbols

    def get_symbol_count(self):
        return self._get_symbols(return_count=True)

    def get_symbols(self, limit: int = 0):
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


def compute_dag_edge_weight(**kwargs) -> int:
    edge_weight = 0

    if kwargs.get(HAKCFunction.DirectCallTable, False):
        edge_weight += 1

    if kwargs.get(HAKCSymbol.UsesSymbolTable, False):
        edge_weight += 1

    if kwargs.get(HAKCFunction.IndirectCallTable, False):
        edge_weight += 1

    return edge_weight


def compute_dag_edges_for_symbol_with_conn(conn: HAKCDatabase, symbol_hash: int):
    results = list()
    tail_hash_types = conn.get_dag_edges(symbol_hash)
    dag_info = dict()

    for dag_edge_type, tail_hashes in tail_hash_types.items():
        if dag_edge_type == HAKCFunction.IndirectCallTable:
            logger.info(f'{symbol_hash}: {tail_hash_types}')
        for _, tail_hash in tail_hashes.items():
            if tail_hash not in dag_info:
                dag_info[tail_hash] = dict()
            dag_info[tail_hash][dag_edge_type] = True
    for tail_hash, tail_info in dag_info.items():
        dag_weight = compute_dag_edge_weight(**tail_info)
        if dag_weight > 0:
            results.append((symbol_hash, tail_hash, dag_weight))

    return results


mp_conn : HAKCDatabase | None = None

def init_mp_database(db_dir: str):
    global mp_conn
    logger.debug(f'Creating database at {db_dir}')
    mp_conn = HAKCDatabase(db_dir, read_only=True)

def compute_dag_edges_for_symbol(symbol_hashes):
    global mp_conn
    conn = mp_conn
    results = list()
    for symbol_hash in symbol_hashes:
        try:
            for t in compute_dag_edges_for_symbol_with_conn(conn, symbol_hash):
                results.append(t)
            logger.debug(f'Finished computing DAG edges for {symbol_hash}')
        except Exception as e:
            logger.error(f'compute_dag_edges_for_symbol failed for {symbol_hash}: {str(e)}')

    return results


def add_dag_edges(compartmentalization: HAKCCompartmentalization):
    logger.info(f'Starting DAG edge computation')
    dag_edge_count = 0
    symbols = compartmentalization.get_symbols()
    with tqdm.tqdm(total=len(symbols)) as pbar:
        for head in symbols:
            for tail in compartmentalization.get_symbols():
                edge_weight_info = dict()
                edge_weight_info[HAKCFunction.DirectCallTable] = compartmentalization.has_edge(head, tail, key=HAKCFunction.DirectCallTable)
                edge_weight_info[HAKCSymbol.UsesSymbolTable] = compartmentalization.has_edge(head, tail, key=HAKCSymbol.UsesSymbolTable)
                edge_weight_info[HAKCFunction.IndirectCallTable] = compartmentalization.has_edge(head, tail.type, key=HAKCFunction.IndirectCallTable)
                dag_edge_weight = compute_dag_edge_weight(**edge_weight_info)
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
        conn = HAKCDatabase(db_dir, max_num_threads=core_count)
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
    with concurrent.futures.ProcessPoolExecutor(max_workers=core_count, initializer=init_mp_database,
                                                initargs=(db_dir,)) as executor:
        dag_edges_added = 0
        symbol_hashes = compartmentalization.get_symbol_hashes()
        logger.info(f'Starting DAG edge computation')
        batch_size = 100
        futures = list()
        with tqdm.tqdm(total=int(len(symbol_hashes) / batch_size) + 1) as pbar:
            try:
                for symbol_hash_batch in batched(symbol_hashes, batch_size):
                    future = executor.submit(compute_dag_edges_for_symbol, symbol_hash_batch)
                    futures.append(future)
                    pbar.update(1)
            except Exception as e:
                logger.error(f'Error submitting tasks: {str(e)}')
                executor.shutdown(wait=False, cancel_futures=True)
                raise e

        with tqdm.tqdm(total=len(futures)) as pbar:
            try:
                dag_edges = dict()
                for future in concurrent.futures.as_completed(futures):
                    pbar.update(1)
                    try:
                        edge_weights = future.result()
                        for (head_hash, tail_hash, dag_edge_weight) in edge_weights:
                            if head_hash not in dag_edges:
                                dag_edges[head_hash] = dict()
                            dag_edges[head_hash][tail_hash] = dag_edge_weight
                            dag_edges_added += 1
                    except Exception as e:
                        logger.error(f'Error computing DAG edge: {str(e)}')
            except KeyboardInterrupt as ki:
                logger.info(f'Stopping edge computation')
                executor.shutdown(wait=False, cancel_futures=True)
                raise ki

    logger.info(f'Adding {dag_edges_added} DAG edges to compartmentalization')
    conn.open(max_num_threads=core_count)
    conn.persist_dag_edges(dag_edges)
    logger.info(f'Done')
    conn.close()

    # with tqdm.tqdm(total=dag_edges_added) as pbar:
    #     symbol_hash_map = dict()
    #     for head_hash, edge_data in dag_edges.items():
    #         if head_hash not in symbol_hash_map:
    #             symbol_hash_map[head_hash] = compartmentalization.get_symbol_by_hash(head_hash)
    #         head = symbol_hash_map[head_hash]
    #         for tail_hash, dag_edge_weight in edge_data.items():
    #             if tail_hash not in symbol_hash_map:
    #                 symbol_hash_map[tail_hash] = compartmentalization.get_symbol_by_hash(tail_hash)
    #             tail = symbol_hash_map[tail_hash]
    #             logger.debug(
    #                 f'Adding DAG Edge between {head} -> {tail} with weight {dag_edge_weight}')
    #             compartmentalization.add_dag_edge(head, tail, dag_edge_weight, add_nodes=False)
    #             pbar.update(1)
    #
    # logger.info(f'Done')
    # conn.persist_compartmentalization(compartmentalization)
    # conn.close()

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
