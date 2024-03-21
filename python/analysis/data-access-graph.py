# Packages to install: pip3 install gremlinpython docker
# On Ubuntu, follow the directions at the following URLs to install Docker
# https://docs.docker.com/engine/install/ubuntu/
# https://docs.docker.com/engine/install/linux-postinstall/
# https://docs.docker.com/config/daemon/systemd/

import argparse
import logging
import os

import docker

logging_level_map = {
    "CRITICAL": logging.CRITICAL,
    "ERROR": logging.ERROR,
    "WARNING": logging.WARNING,
    "INFO": logging.INFO,
    "DEBUG": logging.DEBUG
}


class HAKCDagStore:
    def __init__(self, working_dir: str, logger: logging.Logger):
        self.storage_backend = None
        self.logger = logger

        self.storage_img_name = 'cassandra'
        self.storage_img_tag = '5.0'
        self.container_name = f'{self.storage_img_name}:{self.storage_img_tag}'

        os.makedirs(working_dir, exist_ok=True)
        self.working_dir = os.path.abspath(working_dir)

        self.logger.info(f'Starting Docker')
        self.client = docker.from_env()

        self.logger.info(f'Pulling Docker Image {self.container_name}')
        self.storage_image = self.client.images.pull(self.storage_img_name, tag=self.storage_img_tag)
        self.logger.info(f'Finished retrieving Docker Image {self.container_name}')

        storage_container_options = {
            "detach": True,
            "environment": ['CASSANDRA_START_RPC=true'],
            "ports": {
                9160: 9160,
                9042: 9042,
                7199: 7199,
                7001: 7001,
                7000: 7000
            },
            "name": f'jg-{self.storage_img_name}',
            "working_dir": self.working_dir
        }
        self.logger.info(f'Starting {self.container_name} in directory {self.working_dir}')
        self.storage_backend = self.client.containers.run(self.container_name, **storage_container_options)
        self.logger.info(f'Container {self.container_name} (id: {str(self.storage_backend.short_id)}) started '
                         f'successfully')

    def shutdown(self):
        self.logger.info(f'HAKCDagStore shutting down')
        if self.storage_backend is not None:
            try:
                self.logger.info(f'Stopping Container {self.container_name} (id: {str(self.storage_backend.short_id)})')
                self.storage_backend.stop()
            except docker.errors.APIError as docker_error:
                self.logger.error(f'Failed to stop Container {self.container_name}!')
                self.logger.error(f'{str(docker_error)}')

    def __del__(self):
        self.shutdown()


class HAKCDagSession:
    def __init__(self, storage_working_dir: str, log_file: str, log_level: type(logging.DEBUG)):
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

        self.dag_store = HAKCDagStore(working_dir=storage_working_dir, logger=self.logger)

    def shutdown(self):
        self.logger.info(f'HAKCDagSession shutting down')
        if self.dag_store is not None:
            self.dag_store.shutdown()
            self.dag_store = None

    def start(self):
        self.logger.info(f'HAKCDagSession is starting')

    def __del__(self):
        self.shutdown()


def parse_log_level(level_string: str):
    return logging_level_map[level_string.capitalize()]


def main():
    default_path = os.path.join(os.curdir, 'hakc-jg-store')
    parser = argparse.ArgumentParser(description='HAKC DAG generation and manipulation')
    parser.add_argument('-d', '--working-dir', default=str(default_path), dest='working_dir')
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-level', required=False, dest='log_level', default=logging.INFO,
                        help='Log level to display, can be lower case', type=parse_log_level,
                        choices=sorted(logging_level_map.keys()))

    args = parser.parse_args()

    dag_session = HAKCDagSession(storage_working_dir=args.working_dir,
                                 log_file=args.log_path, log_level=args.log_level)

    dag_session.start()
    dag_session.shutdown()


if __name__ == '__main__':
    main()
