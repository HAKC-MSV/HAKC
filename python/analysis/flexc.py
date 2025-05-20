import argparse
import logging
import multiprocessing as mp
import time

from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger
from hakc.HAKCObjects import HAKCCompartment, HAKCDivision, HAKCSymbol

from flexc_algos import GreedyAlgorithm

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')


def main():
    parser = argparse.ArgumentParser(description='FLEXC Compartment Analysis')
    parser.add_argument('--db-dir', help='Directory to use for the kuzu database', dest='db_dir',
                        required=True)
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-mode', default='w', dest='log_mode')
    parser.add_argument('--core-count', dest='core_count', help='Number of cores to use', default=mp.cpu_count())

    algo_parser = parser.add_subparsers(title='algo', dest='algo', help='Which algorithm to use')
    algos = list()

    algos.append(GreedyAlgorithm.GreedyAlgorithm(algo_parser))

    args = parser.parse_args()

    setup_logging(logger, log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)

    logger.info(f'Opening database at {args.db_dir}')
    database = HAKCDatabase(args.db_dir, read_only=True)

    for algo in algos:
        if algo == args.algo:
            logger.info(f'Running {algo}')
            start = time.time()
            algo.run(database, **vars(args))
            end = time.time()
            logger.info(f'Algorithm {algo} took {end - start} seconds')


if __name__ == "__main__":
    main()
