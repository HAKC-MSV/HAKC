import abc
import argparse
import logging

from hakc.HAKCDatabase import HAKCDatabase
from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('flexc')


class FlexCAlgorithm(abc.ABC):

    def __init__(self, parser, **kwargs):
        algo_parser = parser.add_parser(self.title, aliases=self.aliases, help=self.help)
        self.add_command_line_arguments(algo_parser)

    @property
    @abc.abstractmethod
    def title(self) -> str:
        raise NotImplementedError

    @property
    def aliases(self) -> list[str]:
        return []

    @property
    @abc.abstractmethod
    def help(self) -> str:
        raise NotImplementedError

    def add_command_line_arguments(self, parser):
        pass

    @abc.abstractmethod
    def run(self, arguments: argparse.Namespace, db: HAKCDatabase):
        raise NotImplementedError

    def __str__(self):
        return f'Algorithm {self.title}'

    def __eq__(self, other):
        if isinstance(other, str):
            return other in self.aliases or other == self.title
        elif isinstance(other, FlexCAlgorithm):
            return other.title == self.title
        return False


class GreedyAlgorithm(FlexCAlgorithm):
    title = "greedy"
    aliases = ['g']
    help = "Partition the DAG greedily"

    def add_command_line_arguments(self, parser):
        parser.add_argument('--max-compartments', type=int, help='Max resultant compartments',
                            dest="max_compartments", required=True)

    def run(self, arguments: argparse.Namespace, db: HAKCDatabase):
        logger.info(f'Creating at most {arguments.max_compartments} compartments')


def main():
    parser = argparse.ArgumentParser(description='FLEXC Compartment Analysis')
    parser.add_argument('--db-dir', help='Directory to use for the kuzu database', dest='db_dir',
                        required=True)
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-mode', default='w', dest='log_mode')

    algo_parser = parser.add_subparsers(title='algo', dest='algo', help='Which algorithm to use')
    algos = list()

    algos.append(GreedyAlgorithm(algo_parser))

    args = parser.parse_args()

    setup_logging(logger, log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)

    logger.info(f'Opening database at {args.db_dir}')
    database = HAKCDatabase(args.db_dir, read_only=True)

    for algo in algos:
        if algo == args.algo:
            logger.info(f'Running {algo}')
            algo.run(args, database)


if __name__ == "__main__":
    main()
