import argparse
import concurrent.futures
import itertools
import logging
import multiprocessing as mp
import os
import pickle
import time
from enum import Enum
from typing import Type

import tqdm
import yaml

from hakc.yaml.HAKCDagObjects import HAKCCompartmentalization
from hakc.yaml.HAKCObjects import HAKCObject_constructors, HAKCFunction, HAKCGlobalVariable, HAKCSymbol

logger = logging.getLogger('hakc-dag')


class IndentingEmitter(yaml.emitter.Emitter):
    def increase_indent(self, flow=False, indentless=False):
        """Ensure that lists items are always indented."""
        return super().increase_indent(
            flow=False,
            indentless=False,
        )


class PrettyDumper(
    IndentingEmitter,
    yaml.serializer.Serializer,
    yaml.representer.Representer,
    yaml.resolver.Resolver,
):
    def __init__(
            self,
            stream,
            default_style=None,
            default_flow_style=False,
            canonical=None,
            indent=None,
            width=None,
            allow_unicode=None,
            line_break=None,
            encoding=None,
            explicit_start=None,
            explicit_end=None,
            version=None,
            tags=None,
            sort_keys=True,
    ):
        IndentingEmitter.__init__(
            self,
            stream,
            canonical=canonical,
            indent=indent,
            width=width,
            allow_unicode=allow_unicode,
            line_break=line_break,
        )
        yaml.serializer.Serializer.__init__(
            self,
            encoding=encoding,
            explicit_start=explicit_start,
            explicit_end=explicit_end,
            version=version,
            tags=tags,
        )
        yaml.representer.Representer.__init__(
            self,
            default_style=default_style,
            default_flow_style=default_flow_style,
            sort_keys=sort_keys,
        )
        yaml.resolver.Resolver.__init__(self)


class LoggingLevelEnum(Enum):
    CRITICAL = logging.CRITICAL
    ERROR = logging.ERROR
    WARNING = logging.WARNING
    INFO = logging.INFO
    DEBUG = logging.DEBUG


def get_loader() -> Type[yaml.SafeLoader]:
    loader = yaml.SafeLoader

    for yaml_tag, ctor in HAKCObject_constructors.items():
        loader.add_constructor(yaml_tag, ctor)

    return loader


def parse_yaml(filename: str):
    with open(filename, 'rb') as f:
        parsed_yaml = yaml.load(f, Loader=get_loader())
        compilation_unit = parsed_yaml["CU"]
        functions = parsed_yaml['functions'] if 'functions' in parsed_yaml else set()
        global_variables = parsed_yaml['globals'] if 'globals' in parsed_yaml else set()
    return compilation_unit, functions, global_variables


def add_symbols(compartmentalization: HAKCCompartmentalization, compilation_unit: str, functions: set[HAKCFunction],
                global_variables: set[HAKCGlobalVariable]) -> None:
    for func in functions:
        compartmentalization.add_symbol(func, compilation_unit)

    for glob in global_variables:
        compartmentalization.add_symbol(glob, compilation_unit)


def add_dag_edges(compartmentalization: HAKCCompartmentalization):
    logger.info(f'Starting DAG edge computation')
    symbols = compartmentalization.get_symbols()
    with tqdm.tqdm(total=len(symbols) * (len(symbols) - 1)) as pbar:
        for head, tail in itertools.permutations(symbols, 2):
            dag_edge_weight = add_dag_edge(compartmentalization, head, tail)
            pbar.update(1)
            logger.debug(f'Adding DAG Edge between {head.name} -> {tail.name} with weight {dag_edge_weight}')
            compartmentalization.add_dag_edge(head, tail, dag_edge_weight)


def create_dag_single_thread(files: set[str]) -> HAKCCompartmentalization:
    compartmentalization = HAKCCompartmentalization()
    with tqdm.tqdm(total=len(files)) as pbar:
        for filename in files:
            compilation_unit, functions, global_variables = parse_yaml(filename)
            pbar.update(1)
            logger.debug(
                f'{compilation_unit} found {len(functions)} functions and {len(global_variables)} globals')
            add_symbols(compartmentalization, compilation_unit, functions, global_variables)

    add_dag_edges(compartmentalization)

    return compartmentalization


def adjust_compartmentalization(compartmentalization: HAKCCompartmentalization, adjustments):
    logger.info(f'Adjusting DAG')

    if 'kernel' in adjustments:
        for symbol in compartmentalization.get_symbols():
            defining_unit = symbol.defining_file
            compartment_id = compartmentalization.get_compartment_id(symbol)
            original_compartment_id = compartment_id
            color = compartmentalization.get_color(symbol)

            change = defining_unit is None
            if defining_unit:
                for kernel_path in adjustments['kernel']:
                    if kernel_path in defining_unit:
                        compartment_id = HAKCCompartmentalization.kernel_compartment_id
                        color = HAKCCompartmentalization.kernel_color
                        change = True

                if change and 'compartmentalize' in adjustments and adjustments['compartmentalize'] is not None:
                    for compartmentalize_path in adjustments['compartmentalize']:
                        if compartmentalize_path in defining_unit:
                            change = False
                            break
            else:
                compartment_id = HAKCCompartmentalization.kernel_compartment_id
                color = HAKCCompartmentalization.kernel_color

            if change:
                logger.debug(
                    f'Changing Symbol {symbol.name} Compartment from {original_compartment_id} to {compartment_id}')
                compartmentalization.set_compartment_id(symbol, compartment_id)
                compartmentalization.set_color(symbol, color)
            else:
                logger.info(f'Not changing Symbol {symbol.name}')


mp_compartmentalization = None


def add_dag_edge(compartmentalization: HAKCCompartmentalization, head: HAKCSymbol, tail: HAKCSymbol) -> int:
    edge_weight = 0

    if compartmentalization.has_edge(head, tail):
        edge_weight += 1

    if head.is_function():
        for indirect_call in head.indirect_calls:
            if indirect_call == tail.type:
                edge_weight += 1

    return edge_weight


def mp_add_edge(head: HAKCSymbol):
    global mp_compartmentalization
    results = list()

    for tail in mp_compartmentalization.get_symbols():
        try:
            if tail is not head:
                edge_weight = add_dag_edge(mp_compartmentalization, head, tail)
                if edge_weight > 0:
                    results.append((head, tail, edge_weight))
        except Exception as e:
            logger.error(f'Error computing edge weight between {head.name}  and {tail.name}: {str(e)}')

    return results


def create_dag_multithread(files: set[str]) -> HAKCCompartmentalization:
    max_workers = mp.cpu_count() - 1
    compartmentalization = HAKCCompartmentalization()
    tasks_per_worker = 1000
    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        with tqdm.tqdm(total=len(files)) as pbar:
            for compilation_unit, functions, global_variables in executor.map(parse_yaml, files):
                pbar.update(1)
                logger.debug(
                    f'{compilation_unit} found {len(functions)} functions and {len(global_variables)} globals')
                add_symbols(compartmentalization, compilation_unit, functions, global_variables)

    global mp_compartmentalization
    mp_compartmentalization = compartmentalization
    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        logger.info(f'Starting DAG edge computation')

        symbols = compartmentalization.get_symbols()
        dag_edges_added = 0
        with tqdm.tqdm(total=len(symbols)) as pbar:
            for edge_weights in executor.map(mp_add_edge, symbols):
                pbar.update(1)
                try:
                    for (head, tail, dag_edge_weight) in edge_weights:
                        logger.debug(
                            f'Adding DAG Edge between {head.name} -> {tail.name} with weight {dag_edge_weight}')
                        dag_edges_added += 1
                        compartmentalization.add_dag_edge(head, tail, dag_edge_weight)
                except Exception as e:
                    logger.error(f'Error computing DAG edge: {str(e)}')

    logger.info(f'Finished adding {dag_edges_added} DAG edges')
    return compartmentalization


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


def parse_log_level(level_string: str):
    for level in LoggingLevelEnum:
        if level.name == level_string.upper():
            return level
    raise RuntimeError(f'Invalid log level {level_string}')


def setup_logging(log_file: str, log_level: LoggingLevelEnum):
    log_formatter = logging.Formatter("%(asctime)s [%(threadName)-12.12s] [%(levelname)-5.5s]  %(message)s")
    logger.setLevel(log_level.value)
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(log_formatter)
    logger.addHandler(console_handler)
    if log_file is not None:
        file_handler = logging.FileHandler(log_file)
        file_handler.setFormatter(log_formatter)
        logger.addHandler(file_handler)


def main():
    parser = argparse.ArgumentParser(description='Kernel Data Access Analysis')
    parser.add_argument('--c-in', help='Input compartment path', dest='c_in')
    parser.add_argument('--c-out', help='Output compartment path', dest='c_out')
    parser.add_argument('--dag-files-root', help='Root of DAG Yaml files', dest='dag_files_root')
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--single-thread', dest='single_thread', action='store_true',
                        help='Run analysis without multiprocessing')
    parser.add_argument('--output-yaml', dest='output_yaml', action='store_true',
                        help='Output compartmentalization YAML')
    parser.add_argument('--output-yaml-path', dest='output_yaml_path', help='Path to output DAG YAML')
    parser.add_argument('--create-dag', dest='create_dag', action='store_true', help='Create new DAG')
    parser.add_argument("--adjust", help='Adjust compartmentalization', action='store_true')
    parser.add_argument('--adjust-path', dest='adjust_path', help='Path to adjustment YAML')

    args = parser.parse_args()

    setup_logging(log_file=args.log_path, log_level=args.log_level)
    compartmentalization = None
    if args.c_in:
        with open(args.c_in, 'rb') as f:
            logger.info(f'Reading compartmentalization from {args.c_in}')
            compartmentalization = pickle.load(f)
            logger.info('Done')

    if args.create_dag:
        compartmentalization = create_new_dag(args.dag_files_root, args.single_thread)
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

    if args.output_yaml:
        if compartmentalization is None:
            raise RuntimeError("No input compartmentalization")
        with open(args.output_yaml_path, 'w') as f:
            logger.info(f"Outputting YAML to {args.output_yaml_path}")
            yaml.dump(compartmentalization.to_yaml(), f, Dumper=PrettyDumper)
            logger.info('Done')


if __name__ == "__main__":
    main()
