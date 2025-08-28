import argparse
import logging
import shutil
import time
from multiprocessing import cpu_count

from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger
from hakc.HAKCObjects import HAKCCompartment, HAKCDivision, HAKCSymbol

from flexc_algos import GreedyAlgorithm, FilesystemAlgorithm
from flexc_algos.FlexCAlgorithm import FlexCAlgorithm

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')


def setup_algorithms(parser) -> list[FlexCAlgorithm]:
    algo_parser = parser.add_subparsers(title='algo', dest='algo', help='Which algorithm to use')
    algos = [GreedyAlgorithm.GreedyAlgorithm(algo_parser), FilesystemAlgorithm.FilesystemAlgorithm(algo_parser)]

    return algos


def main():
    parser = argparse.ArgumentParser(description='FLEXC Compartment Analysis')
    parser.add_argument('--db-dir', help='Directory to use for the kuzu database', dest='db_dir',
                        required=True)
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-mode', default='w', dest='log_mode')
    parser.add_argument('--core-count', dest='core_count', help='Number of cores to use', default=cpu_count())
    parser.add_argument('--output-db-dir', help='Directory to store compartmentalization policy', default=None,
                        dest='output_db_dir')

    algos = setup_algorithms(parser)

    args = parser.parse_args()

    setup_logging(logger, log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)

    db_dir_to_use = args.db_dir
    if args.output_db_dir is not None:
        logger.info(f'Copying database from {args.db_dir} to {args.output_db_dir}')
        shutil.copytree(args.db_dir, args.output_db_dir, dirs_exist_ok=True)
        db_dir_to_use = args.output_db_dir

    logger.info(f'Opening database at {db_dir_to_use}')
    database = HAKCDatabase(db_dir_to_use, read_only=True)

    for algo in algos:
        if algo == args.algo:
            logger.info(f'Running {algo}')
            start = time.time()
            algo.run(database, **vars(args))
            end = time.time()
            logger.info(f'{algo} took {end - start} seconds')


if __name__ == "__main__":
    main()
