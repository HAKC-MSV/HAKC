# Packages to install: pip3 install gremlinpython docker
# On Ubuntu, follow the directions at the following URLs to install Docker
# https://docs.docker.com/engine/install/ubuntu/
# https://docs.docker.com/engine/install/linux-postinstall/
# https://docs.docker.com/config/daemon/systemd/

import argparse
import concurrent.futures
import logging
import multiprocessing as mp
import os
import subprocess
import time
from enum import Enum

import docker
import yaml
from gremlin_python import statics
from gremlin_python.driver.client import Client
from gremlin_python.driver.driver_remote_connection import DriverRemoteConnection
from gremlin_python.process.anonymous_traversal import traversal

from hakc.HAKCCompartment import HAKCCompartment
from hakc.StructInfo import StructInfo
from hakc.SymbolInfo import SymbolInfo

logger = logging.getLogger('hakc-dag')


class LoggingLevelEnum(Enum):
    CRITICAL = logging.CRITICAL
    ERROR = logging.ERROR
    WARNING = logging.WARNING
    INFO = logging.INFO
    DEBUG = logging.DEBUG


class HAKCSessionNotStartedError(RuntimeError):
    pass


class JGCarginalityType(Enum):
    Single = "SINGLE"
    List = "LIST"
    Set = "SET"


class JGDataType(Enum):
    String = "String"
    Character = "Character"
    Boolean = "Boolean"
    Byte = "Byte"
    Short = "Short"
    Integer = "Integer"
    Long = "Long"
    Float = "Float"
    Double = "Double"
    Date = "Date"
    Geoshape = "Geoshape"
    Uuid = "UUID"


class JGSchemaProperty:
    def __init__(self, name: str, data_type: JGDataType, cardinality: JGCarginalityType, mgmt_name="mgmt"):
        self.data_type = data_type
        self.cardinality = cardinality
        self.name = name
        self.mgmt_name = mgmt_name

    def make_property_str(self) -> str:
        return (f'{self.mgmt_name}.makePropertyKey(\'{self.name}\')'
                f'.dataType({self.data_type.value})'
                f'.cardinality({self.cardinality.value})'
                f'.make();')


HAKCIdJGProperty = JGSchemaProperty('compartment-id', JGDataType.Integer, JGCarginalityType.Single)

GraphSchema = [
    HAKCIdJGProperty
]


class HAKCDagStore:
    def __init__(self, docker_file: str, docker_output: str):
        self.docker_output_file = None
        self.docker_process = None
        self.gremlin_client = None
        self.graph_traversal = None
        self.connection = None

        if not os.path.exists(docker_file):
            logger.error(f'Dockerfile {docker_file} does not exist!')
            raise FileNotFoundError

        self.docker_file = os.path.abspath(docker_file)
        self.docker_output_file_path = os.path.abspath(docker_output)
        os.makedirs(os.path.dirname(self.docker_output_file_path), exist_ok=True)
        self.docker_output_file = open(self.docker_output_file_path, 'w')

        logger.info(f'Starting Docker from Docker file {self.docker_file}')
        self.docker_watcher = docker.from_env()
        running_containers = set(self.docker_watcher.containers.list())
        self.docker_process = subprocess.Popen(['docker', 'compose', '-f', self.docker_file, 'up'],
                                               stdout=self.docker_output_file, stderr=self.docker_output_file)
        time.sleep(5)
        if self.docker_process.poll() is not None:
            logger.error(f"Docker process has stopped!")
            raise RuntimeError
        self.docker_containers = set(self.docker_watcher.containers.list()) - running_containers
        if len(self.docker_containers) == 0:
            raise RuntimeError("Docker started no containers!")

        logger.info(f'Docker started {len(self.docker_containers)} containers')
        logger.info(f'Waiting for containers to finish starting')
        all_containers_started = False
        while not all_containers_started:
            all_containers_started = True
            for container in self.docker_containers:
                container.reload()
                container_state = container.attrs.get('State')
                if container_state and container_state.get('Running') is True:
                    if container_state.get('Health') and container_state.get('Health').get('Status') == 'starting':
                        logger.debug(f'Container {container.short_id} is still starting')
                        all_containers_started = False
                elif container_state and container_state.get('Running') is False:
                    raise RuntimeError(f"Container {container.short_id} is not running!")
            if not all_containers_started:
                time.sleep(5)

        self.connection = DriverRemoteConnection('ws://localhost:8182/gremlin', 'g')
        self.gremlin_client = Client(self.connection.url, self.connection.traversal_source)
        self.graph_traversal = traversal().with_remote(self.connection)
        statics.load_statics(globals())

    def shutdown(self):
        if self.docker_process is not None:
            logger.info("HAKCDagStore is shutting down")
            try:
                self.docker_process.terminate()
            except Exception as e:
                logger.error(f'Error terminating Docker process: {e}')
            self.docker_process = None

        if self.docker_output_file is not None:
            try:
                self.docker_output_file.close()
            except Exception as e:
                logger.error(f'Error closing {self.docker_output_file_path}: {e}')
            self.docker_output_file = None

        if self.gremlin_client is not None:
            try:
                self.gremlin_client.close()
            except Exception as e:
                logger.error(f'Error closing gremlin connection: {e}')
            self.gremlin_client = None

        if self.connection is not None:
            try:
                self.connection.close()
            except Exception as e:
                logger.error(f'Error closing connection: {e}')
            self.connection = None

    def __del__(self):
        self.shutdown()

    def _send_gremlin_cmd_str(self, cmds: list[str]):
        command_string = "\n".join(cmds)
        logger.debug(f"Sending Gremlin command string:\n{command_string}")
        result = self.gremlin_client.submit(command_string)
        logger.debug(f'Got Gremlin server response {result}')
        return result

    def create_schema(self):
        # There is no Python API for constructing the schema, so we must do this using Gremlin commands
        schema_commands = ['mgmt = graph.openManagement();']
        for jg_property in GraphSchema:
            schema_commands.append(jg_property.make_property_str())
        schema_commands.append('mgmt.commit();')
        self._send_gremlin_cmd_str(schema_commands)

    def add_vertex(self, vertex: HAKCCompartment):
        if self.get_vertex(vertex.get_compartment_id()) is not None:
            logger.debug(f'Compartment {vertex.get_compartment_id()} is already in the graph')
            return

        self.add_vertex_no_check(vertex)

    def add_vertex_no_check(self, vertex: HAKCCompartment):
        logger.debug(f"Adding new vertex\n{vertex}")
        tx = self.graph_traversal.tx()
        tx.begin()
        jg_vertex = self.graph_traversal.add_v()
        jg_vertex.property(HAKCIdJGProperty.name, vertex.get_compartment_id())
        jg_vertex.iterate()
        tx.commit()

    def get_vertex(self, compartment_id: int):
        logger.debug(f'Getting vertex {compartment_id}')
        vertex_set = self.get_vertices([compartment_id])
        if len(vertex_set) == 0:
            return None
        return vertex_set.pop()

    def get_vertices(self, compartments: set[int]) -> set:
        return self.graph_traversal.V().has(HAKCIdJGProperty.name, within(compartments)).to_set()


class HAKCDagSession:
    def __init__(self):
        self.dag_store = None

    def shutdown(self):
        if self.started:
            logger.info(f'HAKCDagSession shutting down')
            self.dag_store.shutdown()
            self.dag_store = None

    def start(self, docker_file, docker_output_file_path):
        logger.info(f'HAKCDagSession is starting')
        self.dag_store = HAKCDagStore(docker_file=docker_file, docker_output=docker_output_file_path)

    @property
    def started(self) -> bool:
        return self.dag_store is not None

    def add_vertex(self, vertex: HAKCCompartment):
        if not self.started:
            raise HAKCSessionNotStartedError
        self.dag_store.add_vertex(vertex=vertex)

    def get_vertex(self, compartment_id: int):
        if not self.started:
            raise HAKCSessionNotStartedError
        return self.dag_store.get_vertex(compartment_id=compartment_id)

    def get_vertices(self, compartment_ids: set[int]):
        if not self.started:
            raise HAKCSessionNotStartedError
        return self.dag_store.get_vertices(compartment_ids)

    def create_schema(self):
        if not self.started:
            raise HAKCSessionNotStartedError
        self.dag_store.create_schema()

    def __del__(self):
        self.shutdown()


DAGSession = HAKCDagSession()


def parse_log_level(level_string: str):
    for level in LoggingLevelEnum:
        if level.name == level_string.upper():
            return level
    raise RuntimeError(f'Invalid log level {level_string}')


class YamlResult:
    def __init__(self):
        self.struct_dict = dict()
        self.symbol_dict = dict()
        self.symbol_name_map = dict()
        self.compilation_unit = None

    @staticmethod
    def _create_list(d: dict):
        result = list()
        for _, it in d.items():
            for i in it:
                result.append(i)
        return result

    @property
    def symbol_list(self):
        return self._create_list(self.symbol_dict)

    @property
    def struct_list(self):
        return self._create_list(self.struct_dict)

    def handle_symbol(self, symbol_info: SymbolInfo):
        append_to_symbol_list = True
        if symbol_info in self.symbol_dict:
            for existing_symbol in self.symbol_dict[symbol_info]:
                # Ensure two symbols with the same name and type from two different compilation units are treated
                # separately. This can happen if two globals are declared static.
                if existing_symbol.get_definition_site() is None or symbol_info.get_definition_site() is None:
                    # We have matched name and type but different declaration locations. This symbol must refer
                    # to the same variable, but we may not know where the definition is. Merge all the information
                    # we know so far.
                    existing_symbol.merge_symbol_data(symbol_info)
                    append_to_symbol_list = False
                    break

        if append_to_symbol_list:
            if symbol_info not in self.symbol_dict:
                self.symbol_dict[symbol_info] = list()
                self.symbol_name_map[symbol_info.get_name()] = list()

            self.symbol_dict[symbol_info].append(symbol_info)
            self.symbol_name_map[symbol_info.get_name()].append(symbol_info)

    def handle_type(self, struct_info: 'StructInfo'):
        append_to_struct_list = True
        if struct_info in self.struct_dict:
            for existing_type in self.struct_dict[struct_info]:
                existing_type.merge_struct_info(struct_info)
                struct_info = existing_type
                append_to_struct_list = False
                break

        for user in struct_info.get_users():
            if user not in self.symbol_name_map:
                continue
            symbol_list = self.symbol_name_map[user]
            for symbol in symbol_list:
                symbol.add_type(struct_info)

        if append_to_struct_list:
            if struct_info not in self.struct_dict:
                self.struct_dict[struct_info] = list()
            self.struct_dict[struct_info].append(struct_info)


def process_yaml_file(filename):
    result = YamlResult()
    with open(filename, 'r') as yml:
        data = yaml.safe_load(yml)
        if not data:
            raise RuntimeError(f"No data for {filename}")

    result.compilation_unit = data['CU']
    if data['symbols'] is not None:
        for symbol_info in data['symbols']:
            info = SymbolInfo.from_yaml(symbol_info, result.compilation_unit)
            result.handle_symbol(info)
    if data['types'] is not None:
        for struct_info in data['types']:
            info = StructInfo.from_yaml(struct_info, result.compilation_unit)
            result.handle_type(info)
    return result


def create_compartment(compartment_id: int, yaml_result: YamlResult):
    compartment = HAKCCompartment()
    compartment.set_compartment_id(compartment_id)
    for symbol in yaml_result.symbol_list:
        if symbol.get_definition_site() and yaml_result.compilation_unit in symbol.get_definition_site():
            compartment.add_defined_symbol(symbol)
        else:
            compartment.add_symbol_access(symbol)
    for ty in yaml_result.struct_list:
        compartment.add_type_access(ty)
    return compartment


def construct_compartment(filename: str, compartment_id: int):
    yaml_result = process_yaml_file(filename)
    compartment = create_compartment(compartment_id=compartment_id, yaml_result=yaml_result)

    DAGSession.add_vertex(compartment)


def process_dag_files_multithreaded(filename_compartment_map: dict):
    max_workers = mp.cpu_count() - 1
    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        logger.info(f'Starting DAG file processing using {max_workers} workers')
        future_to_compartment_id = {
            executor.submit(construct_compartment, filename, filename_compartment_map[filename]): filename for filename
            in filename_compartment_map.keys()}
        for future in concurrent.futures.as_completed(future_to_compartment_id):
            filename = future_to_compartment_id[future]
            compartment_id = filename_compartment_map[filename]
            try:
                future.result()
                logger.info(f'Completed creating compartment {compartment_id} from {filename}')
            except Exception as e:
                logger.error(f'Error handling {filename}: {e}')


def process_dag_files_singlethread(filename_compartment_map: dict):
    logger.info(f'Starting DAG file processing in single thread mode')
    for filename, compartment_id in filename_compartment_map.items():
        construct_compartment(filename, compartment_id)
        logger.info(f'Completed creating compartment {compartment_id} from {filename}')


def filter_existing_vertices(filename_compartment_map: dict):
    compartment_ids = set(filename_compartment_map.values())
    existing_compartments = DAGSession.get_vertices(compartment_ids=compartment_ids)

    filenames_to_remove = set()
    for filename, compartment_id in filename_compartment_map.items():
        if compartment_id in existing_compartments:
            filenames_to_remove.add(filename)

    for filename in filenames_to_remove:
        filename_compartment_map.pop(filename)

    return filename_compartment_map

def create_new_dag(analysis_root: str, single_thread: bool):
    filename_compartment_map = dict()
    compartment_id = 1
    logger.info(f'Finding DAG files starting from {os.path.abspath(analysis_root)}')
    for root, subdirs, files in os.walk(analysis_root):
        for f in files:
            filename = os.path.join(root, f)
            if not filename.endswith(".dag.yml"):
                continue
            filename_compartment_map[filename] = compartment_id
            compartment_id += 1

    filename_compartment_map = filter_existing_vertices(filename_compartment_map)
    logger.info(f'Found {len(filename_compartment_map)} DAG files')
    if single_thread:
        process_dag_files_singlethread(filename_compartment_map)
    else:
        process_dag_files_multithreaded(filename_compartment_map)
    logger.info(f'Completed compartment creation')


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
    parser = argparse.ArgumentParser(description='HAKC DAG generation and manipulation')
    parser.add_argument('-f', '--docker-file', required=True, dest='docker_file')
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('-o', '--docker-output', default='docker.out', dest='docker_out')
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('--create-schema', default=False, action='store_true', dest="create_schema",
                        help="Create the schema on the database")
    parser.add_argument('--create-new-dag', dest='dag_yaml_root', type=str,
                        help='/path/to/root/build/dir')
    parser.add_argument('--single-thread', dest='single_thread', action='store_true',
                        help='Run analysis without multiprocessing')

    args = parser.parse_args()

    setup_logging(log_file=args.log_path, log_level=args.log_level)

    DAGSession.start(docker_file=args.docker_file, docker_output_file_path=args.docker_out)

    try:
        if args.create_schema:
            DAGSession.create_schema()

        if args.dag_yaml_root:
            create_new_dag(analysis_root=args.dag_yaml_root, single_thread=args.single_thread)
        time.sleep(30)
    finally:
        DAGSession.shutdown()


if __name__ == '__main__':
    main()
