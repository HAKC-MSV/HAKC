import argparse
import logging

from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger

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


if __name__ == "__main__":
    main()
