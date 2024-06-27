import argparse
import logging
import os
from enum import Enum
from typing import Type

import yaml
from yaml import SafeLoader

from hakc.yaml.HAKCDagObjects import HAKCCompartmentalization
from hakc.yaml.HAKCObjects import HAKCGlobalVariable, HAKCFunction, HAKCIndirectCallSource, HAKCIndirectSourceLink, \
    HAKCType

logger = logging.getLogger('hakc-dag')


class LoggingLevelEnum(Enum):
    CRITICAL = logging.CRITICAL
    ERROR = logging.ERROR
    WARNING = logging.WARNING
    INFO = logging.INFO
    DEBUG = logging.DEBUG


def construct_global_variable(loader: yaml.SafeLoader, node: yaml.nodes.MappingNode) -> HAKCGlobalVariable:
    return HAKCGlobalVariable(**loader.construct_mapping(node))


def get_loader() -> Type[SafeLoader]:
    loader = yaml.SafeLoader

    constructors = {
        HAKCGlobalVariable.yaml_tag: HAKCGlobalVariable.from_yaml,
        HAKCFunction.yaml_tag: HAKCFunction.from_yaml,
        HAKCIndirectCallSource.yaml_tag: HAKCIndirectCallSource.from_yaml,
        HAKCIndirectSourceLink.yaml_tag: HAKCIndirectSourceLink.from_yaml,
        HAKCType.yaml_tag: HAKCType.from_yaml
    }

    for yaml_tag, ctor in constructors.items():
        loader.add_constructor(yaml_tag, ctor)

    return loader


def create_new_dag(analysis_root: str):
    logger.info(f'Finding DAG files starting from {os.path.abspath(analysis_root)}')
    compartmentalization = HAKCCompartmentalization()
    yaml_loader = get_loader()
    for root, subdirs, files in os.walk(analysis_root):
        for f in files:
            filename = os.path.join(root, f)
            if not filename.endswith(".dag.yml"):
                continue
            with open(filename, 'rb') as f:
                parsed_yaml = yaml.load(f, Loader=yaml_loader)
                compilation_unit = parsed_yaml["CU"]
                functions = parsed_yaml['functions'] if 'functions' in parsed_yaml else set()
                global_variables = parsed_yaml['globals'] if 'globals' in parsed_yaml else set()

            logger.debug(
                f'{compilation_unit} found {len(functions)} functions and {len(global_variables)} globals')

            for func in functions:
                symbol = compartmentalization.add_symbol(func)
                symbol.add_compilation_unit(compilation_unit)

            for glob in global_variables:
                symbol = compartmentalization.add_symbol(glob)
                symbol.add_compilation_unit(compilation_unit)

    logger.info(f'Finished creating compartmentalization.')
    logger.info(f'    Type Count: {len(compartmentalization.get_types())}')
    logger.info(f'  Global Count: {len(compartmentalization.get_global_variables())}')
    logger.info(f'Function Count: {len(compartmentalization.get_functions())}')


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

    args = parser.parse_args()

    setup_logging(log_file=args.log_path, log_level=args.log_level)
    if args.dag_files_root:
        create_new_dag(args.dag_files_root)


if __name__ == "__main__":
    main()
