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

import tqdm
import yaml

from hakc.yaml.HAKCDagObjects import HAKCCompartmentalization
from hakc.yaml.HAKCObjects import HAKCObject_constructors, HAKCFunction, HAKCGlobalVariable, HAKCSymbol, HAKCType, QuotedString

logger = logging.getLogger('hakc-dag')


def quoted_presenter(dumper, data):
    return dumper.represent_scalar('tag:yaml.org,2002:str', data, style='"')


class LoggingLevelEnum(Enum):
    CRITICAL = logging.CRITICAL
    ERROR = logging.ERROR
    WARNING = logging.WARNING
    INFO = logging.INFO
    DEBUG = logging.DEBUG


mp_compartmentalization = None


class HAKCDagState:
    def __init__(self):
        self.compartmentalization = HAKCCompartmentalization()
        self.global_dict = dict()
        self.symbol_count = 0

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
            self.compartmentalization.add_symbol(symbol)

        self.global_dict.clear()

        self.compartmentalization.finalize_symbols()

        for symbol in self.compartmentalization.get_symbols():
            symbol.clear()

    def __len__(self):
        return self.symbol_count


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


def compute_dag_edge_weight(compartmentalization: HAKCCompartmentalization, head: HAKCSymbol, tail: HAKCSymbol,
                            indirect_calls: set[HAKCType]) -> int:
    edge_weight = 0

    if compartmentalization.has_edge(head, tail):
        edge_weight += 1

    if head.is_function():
        for indirect_call in indirect_calls:
            if indirect_call == tail.type:
                edge_weight += 1

    return edge_weight


def compute_dag_edges_for_symbol(head: HAKCSymbol):
    global mp_compartmentalization
    results = list()

    indirect_calls = mp_compartmentalization.get_indirect_calls(head)
    for tail in mp_compartmentalization.get_symbols():
        try:
            if tail is not head:
                edge_weight = compute_dag_edge_weight(mp_compartmentalization, head, tail, indirect_calls)
                if edge_weight > 0:
                    results.append((head, tail, edge_weight))
        except Exception as e:
            logger.error(f'Error computing edge weight between {head.name}  and {tail.name}: {str(e)}')

    return results


def add_dag_edges(compartmentalization: HAKCCompartmentalization):
    logger.info(f'Starting DAG edge computation')
    symbols = compartmentalization.get_symbols()
    dag_edge_count = 0
    with tqdm.tqdm(total=len(symbols)) as pbar:
        for symbol in symbols:
            edge_weights = compute_dag_edges_for_symbol(symbol)
            pbar.update(1)
            for (head, tail, dag_edge_weight) in edge_weights:
                logger.debug(f'Adding DAG Edge between {head} -> {tail} with weight {dag_edge_weight}')
                dag_edge_count += 1
                compartmentalization.add_dag_edge(head, tail, dag_edge_weight)
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


def create_dag_single_thread(files: set[str]) -> HAKCCompartmentalization:
    state = HAKCDagState()
    with tqdm.tqdm(total=len(files)) as pbar:
        for filename in sorted(files):
            compilation_unit, functions, global_variables = parse_yaml(filename)
            pbar.update(1)
            logger.debug(
                f'{compilation_unit} found {len(functions)} functions and {len(global_variables)} globals')
            add_symbols(state, compilation_unit, functions, global_variables)

    finalize_symbols(state)
    global mp_compartmentalization
    mp_compartmentalization = state.compartmentalization
    add_dag_edges(state.compartmentalization)

    return state.compartmentalization


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
                compartmentalization.set_compartment_id(symbol, compartment_id)
                compartmentalization.set_division_id(symbol, division_id)
            else:
                logger.info(f'Not changing Symbol {symbol}')


def create_dag_multithread(files: set[str]) -> HAKCCompartmentalization:
    max_workers = mp.cpu_count() - 1
    state = HAKCDagState()

    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
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
    global mp_compartmentalization
    mp_compartmentalization = state.compartmentalization
    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        logger.info(f'Starting DAG edge computation')

        symbols = state.compartmentalization.get_symbols()
        dag_edges_added = 0
        try:
            futures_to_symbol = {executor.submit(compute_dag_edges_for_symbol, symbol): symbol for symbol in symbols}
            logger.info(f'{len(futures_to_symbol)} tasks submitted')
        except Exception as e:
            logger.error(f'Error submitting tasks: {str(e)}')
            executor.shutdown(wait=False, cancel_futures=True)
            raise e

        with tqdm.tqdm(total=len(futures_to_symbol)) as pbar:
            for future in concurrent.futures.as_completed(futures_to_symbol):
                pbar.update(1)
                symbol = futures_to_symbol[future]
                try:
                    edge_weights = future.result()
                    for (head, tail, dag_edge_weight) in edge_weights:
                        logger.debug(
                            f'Adding DAG Edge between {head} -> {tail} with weight {dag_edge_weight}')
                        dag_edges_added += 1
                        state.compartmentalization.add_dag_edge(head, tail, dag_edge_weight)
                except Exception as e:
                    logger.error(f'Error computing DAG edge for symbol {symbol}: {str(e)}')

    logger.info(f'Finished adding {dag_edges_added} DAG edges')
    return state.compartmentalization


def create_new_dag(analysis_root: str, single_thread: bool) -> HAKCCompartmentalization:
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
    if single_thread:
        compartmentalization = create_dag_single_thread(filenames)
    else:
        compartmentalization = create_dag_multithread(filenames)

    end = time.time()
    logger.info(f'Finished creating DAG.')
    logger.info(f'    Total Time: {end - start} seconds')
    logger.info(f'    Type Count: {len(compartmentalization.get_types())}')
    logger.info(f'  Global Count: {len(compartmentalization.get_global_variables())}')
    logger.info(f'Function Count: {len(compartmentalization.get_functions())}')

    return compartmentalization


def print_symbols(compartmentalization: HAKCCompartmentalization):
    for symbol in sorted(compartmentalization.get_symbols(), key=lambda node: node.name):
        logger.info(f'{symbol}')


def print_symbols_with_same_name(compartmentalization: HAKCCompartmentalization):
    name_dict = dict()
    for symbol in compartmentalization.get_symbols():
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
    parser.add_argument('--output-symbol-dir', dest='output_symbol_dir', help='Directory to output DAG Symbol YAML')
    parser.add_argument('--create-dag', dest='create_dag', action='store_true', help='Create new DAG')
    parser.add_argument("--adjust", help='Adjust compartmentalization', action='store_true')
    parser.add_argument('--adjust-path', dest='adjust_path', help='Path to adjustment YAML')
    parser.add_argument('--print-symbols', dest='print_symbols', action='store_true')
    parser.add_argument('--profile', dest='profile', action='store_true')
    parser.add_argument('--print-symbols-with-same-name', dest='print_symbols_with_same_name', action='store_true')

    args = parser.parse_args()

    profile = None
    setup_logging(log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)
    compartmentalization = None
    if args.c_in:
        with open(args.c_in, 'rb') as f:
            logger.info(f'Reading compartmentalization from {args.c_in}')
            compartmentalization = pickle.load(f)
            logger.info('Done')

    if args.profile:
        profile = cProfile.Profile()

    if args.create_dag:
        if profile:
            profile.enable()
        compartmentalization = create_new_dag(args.dag_files_root, args.single_thread)
        if profile:
            profile.disable()
            output_profile_stats(profile)

        if args.c_out:
            with open(args.c_out, 'wb') as f:
                logger.info(f'Writing compartmentalization to {args.c_out}')
                pickle.dump(compartmentalization, f)
                logger.info(f'Done')

    if args.adjust:
        if compartmentalization is None:
            raise RuntimeError("No compartmentalization")
        logger.info(f'Adjusting compartmentalization based on {args.adjust_path}')
        with open(args.adjust_path, 'r') as f:
            adjustments = yaml.safe_load(f)
        adjust_compartmentalization(compartmentalization, adjustments)
        logger.info("Done")

    if args.print_symbols:
        if compartmentalization is None:
            raise RuntimeError("No compartmentalization")
        print_symbols(compartmentalization)

    if args.print_symbols_with_same_name:
        if compartmentalization is None:
            raise RuntimeError("No compartmentalization")
        print_symbols_with_same_name(compartmentalization)

    if args.output_yaml:
        if compartmentalization is None:
            raise RuntimeError("No input compartmentalization")
        with open(args.output_yaml_path, 'w') as f:
            logger.info(f"Outputting YAML to {args.output_yaml_path}")
            yaml.add_representer(QuotedString, quoted_presenter)
            yaml.dump(compartmentalization.to_yaml(), f, width=float("inf"))
            logger.info('Done')


if __name__ == "__main__":
    main()
