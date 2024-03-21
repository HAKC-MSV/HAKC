# Packages to install: pip3 install gremlinpython docker
# On Ubuntu, follow the directions at the following URLs to install Docker
# https://docs.docker.com/engine/install/ubuntu/
# https://docs.docker.com/engine/install/linux-postinstall/
# https://docs.docker.com/config/daemon/systemd/

import argparse
import logging
import os
import subprocess
import docker


from gremlin_python import statics
from gremlin_python.structure.graph import Graph
from gremlin_python.process.graph_traversal import __
from gremlin_python.driver.driver_remote_connection import DriverRemoteConnection
from gremlin_python.process.anonymous_traversal import traversal


logging_level_map = {
    "CRITICAL": logging.CRITICAL,
    "ERROR": logging.ERROR,
    "WARNING": logging.WARNING,
    "INFO": logging.INFO,
    "DEBUG": logging.DEBUG
}


class HAKCDagStore:
    def __init__(self, docker_file: str, docker_output: str, logger: logging.Logger):
        self.logger = logger
        self.docker_output_file = None
        self.docker_process = None
        self.gremlin_conn = None
        if not os.path.exists(docker_file):
            self.logger.error(f'Dockerfile {docker_file} does not exist!')
            raise FileNotFoundError

        self.docker_file = os.path.abspath(docker_file)
        self.docker_output_file_path = os.path.abspath(docker_output)
        os.makedirs(os.path.dirname(self.docker_output_file_path), exist_ok=True)
        self.docker_output_file = open(self.docker_output_file_path, 'w')

        self.logger.info(f'Starting Docker from Docker file {self.docker_file}')
        self.docker_watcher = docker.from_env()
        running_containers = set(self.docker_watcher.containers.list(all=True))
        self.docker_process = subprocess.Popen(['docker', 'compose', '-f', self.docker_file, 'up'],
                                               stdout=self.docker_output_file, stderr=self.docker_output_file)
        if self.docker_process.poll() is not None:
            self.logger.error(f"Docker process has stopped!")
            raise RuntimeError
        self.docker_containers = [container for container in self.docker_watcher.containers.list(all=True) if container not in running_containers]
        self.logger.info(f'Docker started {len(self.docker_containers)} containers')
        self.gremlin_conn = DriverRemoteConnection('ws://localhost:8182/gremlin', 'g')
        self.graph = traversal().withRemote(self.gremlin_conn)

    def shutdown(self):
        if self.docker_process is not None:
            self.logger.info("HAKCDagStore is shutting down")
            try:
                self.docker_process.terminate()
            except Exception as e:
                self.logger.error(f'Error terminating Docker process: {e}')
            self.docker_process = None

        if self.docker_output_file is not None:
            try:
                self.docker_output_file.close()
            except Exception as e:
                self.logger.error(f'Error closing {self.docker_output_file_path}: {e}')
            self.docker_output_file = None

        if self.gremlin_conn is not None:
            try:
                self.gremlin_conn.close()
            except Exception as e:
                self.logger.error(f'Error closing gremlin connection: {e}')
            self.gremlin_conn = None

    def __del__(self):
        self.shutdown()


class HAKCDagSession:
    def __init__(self, docker_file: str, docker_output_file_path: str, log_file: str, log_level: type(logging.DEBUG)):
        self.dag_store = None

        log_formatter = logging.Formatter("%(asctime)s [%(threadName)-12.12s] [%(levelname)-5.5s]  %(message)s")
        self.logger = logging.getLogger()
        self.logger.setLevel(log_level)
        console_handler = logging.StreamHandler()
        console_handler.setFormatter(log_formatter)
        self.logger.addHandler(console_handler)
        if log_file is not None:
            file_handler = logging.FileHandler(log_file)
            file_handler.setFormatter(log_formatter)
            self.logger.addHandler(file_handler)

        self.dag_store = HAKCDagStore(docker_file=docker_file, docker_output=docker_output_file_path,
                                      logger=self.logger)

    def shutdown(self):
        if self.dag_store is not None:
            self.logger.info(f'HAKCDagSession shutting down')
            self.dag_store.shutdown()
            self.dag_store = None

    def start(self):
        self.logger.info(f'HAKCDagSession is starting')

    def __del__(self):
        self.shutdown()


def parse_log_level(level_string: str):
    return logging_level_map[level_string.capitalize()]


def main():
    parser = argparse.ArgumentParser(description='HAKC DAG generation and manipulation')
    parser.add_argument('-f', '--docker-file', required=True, dest='docker_file')
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('-o', '--docker-output', default='docker.out', dest='docker_out')
    parser.add_argument('--log-level', required=False, dest='log_level', default=logging.INFO,
                        help='Log level to display, can be lower case', type=parse_log_level,
                        choices=sorted(logging_level_map.keys()))

    args = parser.parse_args()

    dag_session = HAKCDagSession(docker_file=args.docker_file, docker_output_file_path=args.docker_out,
                                 log_file=args.log_path, log_level=args.log_level)

    dag_session.start()


if __name__ == '__main__':
    main()
