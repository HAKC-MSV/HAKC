import argparse
import cProfile
import concurrent.futures
import io
import logging
import multiprocessing as mp
import os
import pickle
import pstats
import time
from enum import Enum
from typing import Type

import kuzu
import tqdm
import yaml

from hakc.yaml.HAKCDagObjects import HAKCCompartmentalization
from hakc.yaml.HAKCObjects import HAKCObject_constructors, HAKCFunction, HAKCGlobalVariable, HAKCSymbol, HAKCType, \
    QuotedString
from python.analysis.hakc.yaml.HAKCObjects import HAKCScope

logger = logging.getLogger('hakc-dag')


def quoted_presenter(dumper, data):
    return dumper.represent_scalar('tag:yaml.org,2002:str', data, style='"')


class LoggingLevelEnum(Enum):
    CRITICAL = logging.CRITICAL
    ERROR = logging.ERROR
    WARNING = logging.WARNING
    INFO = logging.INFO
    DEBUG = logging.DEBUG


shared_state = None


class HAKCDagState:
    def __init__(self):
        self.global_dict = dict()
        self.symbol_count = 0

    def initialize_state(self):
        pass

    def initialize_new_compartmentalization(self):
        pass

    def track_global(self, symbol: HAKCSymbol, compilation_unit: str) -> HAKCSymbol:
        if len(compilation_unit) > 0:
            symbol.compilation_units.add(compilation_unit)
        if symbol.name not in self.global_dict:
            self.global_dict[symbol.name] = dict()
            self.global_dict[symbol.name][symbol.scope] = dict()
            self.global_dict[symbol.name][symbol.scope][symbol.is_function()] = symbol
            self.symbol_count += 1
            result = symbol
        else:
            if symbol.scope in self.global_dict[symbol.name]:
                if symbol.is_function() in self.global_dict[symbol.name][symbol.scope]:
                    self.global_dict[symbol.name][symbol.scope][symbol.is_function()].merge_symbol(symbol)
                    result = self.global_dict[symbol.name][symbol.scope][symbol.is_function()]
                else:
                    self.global_dict[symbol.name][symbol.scope][symbol.is_function()] = symbol
                    self.symbol_count += 1
                    result = symbol
            else:
                self.global_dict[symbol.name][symbol.scope] = dict()
                self.global_dict[symbol.name][symbol.scope][symbol.is_function()] = symbol
                self.symbol_count += 1
                result = symbol
        return result

    def add_symbol(self, symbol: HAKCSymbol):
        raise NotImplementedError

    def get_symbols(self):
        raise NotImplementedError

    def finalize_compartmentalization(self):
        raise NotImplementedError

    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int):
        raise NotImplementedError

    def persist_compartmentalization(self, path: str):
        raise NotImplementedError

    def get_compartment_id(self, symbol: HAKCSymbol) -> int:
        raise NotImplementedError

    def get_division_id(self, symbol: HAKCSymbol) -> int:
        raise NotImplementedError

    def set_compartment_id(self, symbol: HAKCSymbol, compartment_id: int):
        raise NotImplementedError

    def set_division_id(self, symbol: HAKCSymbol, division_id: int):
        raise NotImplementedError

    def get_types(self):
        raise NotImplementedError

    def get_global_variables(self):
        raise NotImplementedError

    def get_functions(self):
        raise NotImplementedError

    def to_yaml(self) -> dict:
        raise NotImplementedError

    def has_edge(self, head: HAKCSymbol, tail: HAKCSymbol) -> bool:
        raise NotImplementedError

    def get_indirect_calls(self, symbol: HAKCSymbol) -> set[HAKCType]:
        raise NotImplementedError

    def finalize_symbols(self):
        symbols = set()

        for name, scope_dict in self.global_dict.items():
            logger.debug(f'Symbol {name} has {len(scope_dict)} scoped symbols')
            for scope, symbol_dict in scope_dict.items():
                for _, symbol in symbol_dict.items():
                    symbols.add(symbol)

        # Ensure that types match for correct hash computations
        for symbol in symbols:
            for used_symbol in symbol.used_symbols:
                tracked_symbol = self.track_global(used_symbol, "")
                used_symbol.type = tracked_symbol.type

        for symbol in symbols:
            self.add_symbol(symbol)

        self.global_dict.clear()

        self.finalize_compartmentalization()

        for symbol in self.get_symbols():
            symbol.clear()

    def __len__(self):
        return self.symbol_count


class NetworkXDagState(HAKCDagState):
    def __init__(self, compartmentalization: HAKCCompartmentalization):
        HAKCDagState.__init__(self)
        if compartmentalization is None:
            self._compartmentalization = HAKCCompartmentalization()
        else:
            self._compartmentalization = compartmentalization

    def add_symbol(self, symbol: HAKCSymbol):
        self._compartmentalization.add_symbol(symbol)

    def get_symbols(self):
        return self._compartmentalization.get_symbols()

    def finalize_compartmentalization(self):
        self._compartmentalization.finalize_symbols()

    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int):
        self._compartmentalization.add_dag_edge(head, tail, dag_edge_weight)

    def persist_compartmentalization(self, path: str):
        with open(path, 'wb') as f:
            pickle.dump(self._compartmentalization, f)

    def get_compartment_id(self, symbol: HAKCSymbol) -> int:
        return self._compartmentalization.get_compartment_id(symbol)

    def get_division_id(self, symbol: HAKCSymbol) -> int:
        return self._compartmentalization.get_division_id(symbol)

    def set_compartment_id(self, symbol: HAKCSymbol, compartment_id: int):
        self._compartmentalization.set_compartment_id(symbol, compartment_id)

    def set_division_id(self, symbol: HAKCSymbol, division_id: int):
        self._compartmentalization.set_division_id(symbol, division_id)

    def get_types(self):
        return self._compartmentalization.get_types()

    def get_global_variables(self):
        return self._compartmentalization.get_global_variables()

    def get_functions(self):
        return self._compartmentalization.get_functions()

    def to_yaml(self) -> dict:
        return self._compartmentalization.to_yaml()

    def get_indirect_calls(self, symbol: HAKCSymbol) -> set[HAKCType]:
        return self._compartmentalization.get_indirect_calls(symbol)

    def has_edge(self, head: HAKCSymbol, tail: HAKCSymbol) -> bool:
        return self._compartmentalization.has_edge(head, tail)


class KuzuDagState(HAKCDagState):
    IsTypeTable = "IsType"
    HasScopeTable = "HasScope"

    def __init__(self, kuzu_db_dir: str):
        HAKCDagState.__init__(self)
        self._kuzu_db = kuzu.Database(kuzu_db_dir)
        self._kuzu_conn = None

    def initialize_state(self):
        self._kuzu_conn = kuzu.Connection(self._kuzu_db)

    def _create_table(self, table_name: str, cmd: str):
        try:
            self._kuzu_conn.execute(cmd)
        except RuntimeError:
            self._kuzu_conn.execute(f'DROP TABLE {table_name}')
            self._kuzu_conn.execute(cmd)

    def create_node_table(self, obj_name: str, primary_key: str, **kwargs):
        if primary_key not in kwargs:
            raise RuntimeError(f'Primary key {primary_key} not provided')
        member_str = ",".join([" ".join([key, val]) for key, val in kwargs.items()])
        create_cmd = f'CREATE NODE TABLE {obj_name}({member_str}, PRIMARY KEY ({primary_key}))'
        self._create_table(obj_name, create_cmd)

    def create_rel_table(self, table_name: str, from_name: str, to_name: str):
        create_cmd = f'CREATE REL TABLE {table_name}(FROM {from_name} TO {to_name})'
        self._create_table(table_name, create_cmd)

    def initialize_new_compartmentalization(self):
        self._kuzu_conn = kuzu.Connection(self._kuzu_db)
        self.create_node_table(HAKCType.__class__.__name__, primary_key="type_hash",
                               debug_type="STRING", llvm_type="STRING", type_hash="STRING")
        self.create_node_table(HAKCScope.__class__.__name__, primary_key="scope_hash", scope="STRING",
                               local_scope="STRING", scope_hash="STRING")
        self.create_node_table(HAKCSymbol.__class__.__name__, primary_key="symbol_hash", name="STRING",
                               defining_file="STRING", defining_line="INT32", compilation_units="STRING[]",
                               symbol_hash="STRING")

        self.create_rel_table(KuzuDagState.IsTypeTable, HAKCSymbol.__class__.__name__, HAKCType.__class__.__name__)
        self.create_rel_table(KuzuDagState.HasScopeTable, HAKCSymbol.__class__.__name__, HAKCScope.__class__.__name__)

        self._kuzu_conn.close()
        self._kuzu_conn = None

    def add_symbol(self, symbol: HAKCSymbol):


    def get_symbols(self):
        return []

    def finalize_compartmentalization(self):
        pass

    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int):
        pass

    def get_compartment_id(self, symbol: HAKCSymbol) -> int:
        return HAKCCompartmentalization.kernel_compartment_id

    def get_division_id(self, symbol: HAKCSymbol) -> int:
        return HAKCCompartmentalization.kernel_division

    def set_division_id(self, symbol: HAKCSymbol, division_id: int):
        pass

    def set_compartment_id(self, symbol: HAKCSymbol, compartment_id: int):
        pass

    def get_types(self):
        return []

    def get_global_variables(self):
        return []

    def get_functions(self):
        return []

    def to_yaml(self) -> dict:
        return dict()

    def get_indirect_calls(self, symbol: HAKCSymbol) -> set[HAKCType]:
        return set()


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


def compute_dag_edge_weight(state: HAKCDagState, head: HAKCSymbol, tail: HAKCSymbol,
                            indirect_calls: set[HAKCType]) -> int:
    edge_weight = 0

    if state.has_edge(head, tail):
        edge_weight += 1

    if head.is_function():
        for indirect_call in indirect_calls:
            if indirect_call == tail.type:
                edge_weight += 1

    return edge_weight


def compute_dag_edges_for_symbol(head: HAKCSymbol):
    global shared_state
    results = list()

    indirect_calls = shared_state.get_indirect_calls(head)
    for tail in shared_state.get_symbols():
        try:
            if tail is not head:
                edge_weight = compute_dag_edge_weight(shared_state, head, tail, indirect_calls)
                if edge_weight > 0:
                    results.append((head, tail, edge_weight))
        except Exception as e:
            logger.error(f'Error computing edge weight between {head.name}  and {tail.name}: {str(e)}')

    return results


def add_dag_edges(state: HAKCDagState):
    logger.info(f'Starting DAG edge computation')
    symbols = state.get_symbols()
    dag_edge_count = 0
    with tqdm.tqdm(total=len(symbols)) as pbar:
        for symbol in symbols:
            edge_weights = compute_dag_edges_for_symbol(symbol)
            pbar.update(1)
            for (head, tail, dag_edge_weight) in edge_weights:
                logger.debug(f'Adding DAG Edge between {head} -> {tail} with weight {dag_edge_weight}')
                dag_edge_count += 1
                state.add_dag_edge(head, tail, dag_edge_weight)
    logger.info(f'Finished adding {dag_edge_count} DAG edges')


def finalize_symbols(state: HAKCDagState):
    logger.info(f'Finalizing {len(state)} symbols')
    state.finalize_symbols()


def add_symbols(state: HAKCDagState, compilation_unit: str, functions: set[HAKCFunction],
                global_variables: set[HAKCGlobalVariable]):
    for func in functions:
        state.track_global(func, compilation_unit)
    for glob in global_variables:
        state.track_global(glob, compilation_unit)


def create_dag_single_thread(files: set[str], state: HAKCDagState) -> None:
    with tqdm.tqdm(total=len(files)) as pbar:
        for filename in sorted(files):
            compilation_unit, functions, global_variables = parse_yaml(filename)
            pbar.update(1)
            logger.debug(
                f'{compilation_unit} found {len(functions)} functions and {len(global_variables)} globals')
            add_symbols(state, compilation_unit, functions, global_variables)

    finalize_symbols(state)
    add_dag_edges(state)


def adjust_compartmentalization(state: HAKCDagState, adjustments):
    logger.info(f'Adjusting DAG')

    if 'kernel' in adjustments:
        for symbol in state.get_symbols():
            defining_unit = symbol.defining_file
            compartment_id = state.get_compartment_id(symbol)
            original_compartment_id = compartment_id
            division_id = state.get_division_id(symbol)

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
                state.set_compartment_id(symbol, compartment_id)
                state.set_division_id(symbol, division_id)
            else:
                logger.info(f'Not changing Symbol {symbol}')


def initialize_multithread_worker():
    global shared_state
    shared_state.initialize_state()


def create_dag_multithread(files: set[str], core_count: int, state: HAKCDagState) -> None:
    logger.info(f'Starting multiprocess DAG creation using {core_count} cores')
    with concurrent.futures.ProcessPoolExecutor(max_workers=core_count,
                                                initializer=initialize_multithread_worker) as executor:
        with tqdm.tqdm(total=len(files)) as pbar:
            futures_to_files = {executor.submit(parse_yaml, file): file for file in sorted(files)}
            for future in concurrent.futures.as_completed(futures_to_files):
                pbar.update(1)
                file = futures_to_files[future]
                try:
                    compilation_unit, functions, global_variables = future.result()
                    logger.debug(
                        f'{compilation_unit} found {len(functions)} functions and {len(global_variables)} globals')
                    add_symbols(state, compilation_unit, functions, global_variables)
                except Exception as e:
                    logger.error(f'Error parsing {file}: {str(e)}')

    finalize_symbols(state)
    with concurrent.futures.ProcessPoolExecutor(max_workers=core_count,
                                                initializer=initialize_multithread_worker) as executor:
        logger.info(f'Starting DAG edge computation')

        symbols = state.get_symbols()
        dag_edges_added = 0
        futures_to_symbol = dict()
        with tqdm.tqdm(total=len(symbols)) as pbar:
            try:
                for symbol in symbols:
                    future = executor.submit(compute_dag_edges_for_symbol, symbol)
                    futures_to_symbol[future] = symbol
                    pbar.update(1)
            except Exception as e:
                logger.error(f'Error submitting tasks: {str(e)}')
                executor.shutdown(wait=False, cancel_futures=True)
                raise e

        with tqdm.tqdm(total=len(futures_to_symbol)) as pbar:
            try:
                for future in concurrent.futures.as_completed(futures_to_symbol):
                    pbar.update(1)
                    symbol = futures_to_symbol[future]
                    try:
                        edge_weights = future.result()
                        for (head, tail, dag_edge_weight) in edge_weights:
                            logger.debug(
                                f'Adding DAG Edge between {head} -> {tail} with weight {dag_edge_weight}')
                            dag_edges_added += 1
                            state.add_dag_edge(head, tail, dag_edge_weight)
                    except Exception as e:
                        logger.error(f'Error computing DAG edge for symbol {symbol}: {str(e)}')
            except KeyboardInterrupt as ki:
                logger.info(f'Stopping edge computation')
                executor.shutdown(wait=False, cancel_futures=True)
                raise ki

    logger.info(f'Finished adding {dag_edges_added} DAG edges')


def create_new_dag(analysis_root: str, single_thread: bool, core_count: int, state: HAKCDagState) -> None:
    core_count = max(1, core_count)
    logger.info(f'Finding DAG files starting from {os.path.abspath(analysis_root)}')
    filenames = set()
    for root, subdirs, files in os.walk(analysis_root):
        for f in files:
            filename = os.path.join(root, f)
            if not filename.endswith(".dag.yml"):
                continue
            filenames.add(str(filename))

    logger.info(f'Starting DAG construction from {len(filenames)} files')
    start = time.time()
    state.initialize_new_compartmentalization()
    if single_thread:
        create_dag_single_thread(filenames, state)
    else:
        create_dag_multithread(filenames, core_count, state)

    end = time.time()
    logger.info(f'Finished creating DAG.')
    logger.info(f'    Total Time: {end - start} seconds')
    logger.info(f'    Type Count: {len(state.get_types())}')
    logger.info(f'  Global Count: {len(state.get_global_variables())}')
    logger.info(f'Function Count: {len(state.get_functions())}')


def print_symbols(state: HAKCDagState):
    for symbol in sorted(state.get_symbols(), key=lambda node: node.name):
        logger.info(f'{symbol}')


def print_symbols_with_same_name(state: HAKCDagState):
    name_dict = dict()
    for symbol in state.get_symbols():
        if symbol.name not in name_dict:
            name_dict[symbol.name] = set()
        name_dict[symbol.name].add(symbol)

    for name, symbols in name_dict.items():
        if len(symbols) > 1:
            for symbol in symbols:
                logger.info(f'{symbol}')
            logger.info("")


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
    parser.add_argument('--networkx', help='Use NetworkX implementation', dest='use_networkx',
                        action='store_true')
    parser.add_argument('--c-in', help='Input compartment path', dest='c_in')
    parser.add_argument('--c-out', help='Output compartment path', dest='c_out')
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
    parser.add_argument('--output-symbol-dir', dest='output_symbol_dir',
                        help='Directory to output DAG Symbol YAML')
    parser.add_argument('--create-dag', dest='create_dag', action='store_true', help='Create new DAG')
    parser.add_argument("--adjust", help='Adjust compartmentalization', action='store_true')
    parser.add_argument('--adjust-path', dest='adjust_path', help='Path to adjustment YAML')
    parser.add_argument('--print-symbols', dest='print_symbols', action='store_true')
    parser.add_argument('--profile', dest='profile', action='store_true')
    parser.add_argument('--print-symbols-with-same-name', dest='print_symbols_with_same_name',
                        action='store_true')
    parser.add_argument('--core-count', help='Max cores to use for analysis', dest='core_count',
                        default=mp.cpu_count() - 1, type=int)
    parser.add_argument('--kuzu', help='Use kuzu', dest='use_kuzu', action='store_true')
    parser.add_argument('--kuzu-db-dir', help='Directory to use for the kuzu database', dest='kuzu_dir',
                        default=None)

    args = parser.parse_args()

    profile = None
    setup_logging(log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)

    if args.use_kuzu and args.use_networkx:
        raise RuntimeError(f'Cannot use both kuzu and NetworkX implementations')

    if not args.use_kuzu and not args.use_networkx:
        raise RuntimeError(f'Must use either kuzu or Networkx')

    global shared_state
    if args.use_kuzu:
        if args.kuzu_dir is None or len(args.kuzu_dir) == 0:
            raise RuntimeError(f'Must specify a kuzu directory')

        logger.info(f'Using kuzu database at {args.kuzu_dir}')
        shared_state = KuzuDagState(args.kuzu_dir)

    if args.use_networkx:
        compartmentalization = None
        if args.c_in:
            with open(args.c_in, 'rb') as f:
                logger.info(f'Reading compartmentalization from {args.c_in}')
                compartmentalization = pickle.load(f)
                logger.info('Done')
        shared_state = NetworkXDagState(compartmentalization)

    if args.profile:
        profile = cProfile.Profile()

    if args.create_dag:
        if profile:
            profile.enable()
        create_new_dag(args.dag_files_root, args.single_thread, args.core_count, shared_state)
        if profile:
            profile.disable()
            output_profile_stats(profile)

        if args.c_out:
            with open(args.c_out, 'wb') as f:
                logger.info(f'Writing compartmentalization to {args.c_out}')
                shared_state.persist_compartmentalization(args.c_out)
                logger.info(f'Done')

    if args.adjust:
        logger.info(f'Adjusting compartmentalization based on {args.adjust_path}')
        with open(args.adjust_path, 'r') as f:
            adjustments = yaml.safe_load(f)
        adjust_compartmentalization(shared_state, adjustments)
        logger.info("Done")

    if args.print_symbols:
        print_symbols(shared_state)

    if args.print_symbols_with_same_name:
        print_symbols_with_same_name(shared_state)

    if args.output_yaml:
        with open(args.output_yaml_path, 'w') as f:
            logger.info(f"Outputting YAML to {args.output_yaml_path}")
            yaml.add_representer(QuotedString, quoted_presenter)
            yaml.dump(shared_state.to_yaml(), f, width=float("inf"))
            logger.info('Done')


if __name__ == "__main__":
    main()
