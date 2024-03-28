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
from collections.abc import Mapping, Iterable
from enum import Enum

import docker
import yaml
from gremlin_python import statics
from gremlin_python.driver.client import Client
from gremlin_python.driver.driver_remote_connection import DriverRemoteConnection
from gremlin_python.driver.protocol import GremlinServerError
from gremlin_python.driver.serializer import GraphSONSerializersV3d0
from gremlin_python.process.anonymous_traversal import traversal
from gremlin_python.process.traversal import within
from gremlin_python.structure.graph import Vertex

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


class JGCardinalityType(Enum):
    Single = "SINGLE"
    List = "LIST"
    Set = "SET"


class JGEdgeMultiplicity(Enum):
    Mutli = "MULTI"
    Simple = "SIMPLE"
    ManyToOne = "MANY2ONE"
    OneToMany = "ONE2MANY"
    OneToOne = "ONE2ONE"


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


class JGEdgeLabel(Enum):
    DirectCall = 'direct-call'
    Uses = 'uses'
    IndirectCall = 'indirect-call'
    InCompartment = 'in-compartment'


class JGVertexLabel(Enum):
    DataType = 'Type'
    Symbol = 'Symbol'
    Compartment = 'Compartment'


class JGSchemaElement:
    @staticmethod
    def get_default_management_name() -> str:
        return "mgmt"

    def __init__(self, name: str, mgmt_name: str):
        self.name = name
        self.mgmt_name = mgmt_name

    def make_gremlin_str(self) -> str:
        raise NotImplementedError

    def __eq__(self, other):
        if isinstance(other, JGSchemaElement):
            return other.name == self.name
        return False

    def __hash__(self):
        return hash(self.name)


class JGSchemaProperty(JGSchemaElement):
    def __init__(self, name: str, data_type: JGDataType, cardinality: JGCardinalityType,
                 mgmt_name=JGSchemaElement.get_default_management_name()):
        super().__init__(name, mgmt_name)
        self.data_type = data_type
        self.cardinality = cardinality

    def make_property_str(self) -> str:
        return (f'{self.mgmt_name}.makePropertyKey(\'{self.name}\')'
                f'.dataType({self.data_type.value})'
                f'.cardinality({self.cardinality.value})'
                f'.make();')

    def make_gremlin_str(self) -> str:
        return self.make_property_str()

    def __repr__(self):
        return f'{self.name} [{self.data_type.value} {self.cardinality.value}]'


class JGSchemaVertexLabel(JGSchemaElement):
    def __init__(self, vertex_label: JGVertexLabel, mgmt_name=JGSchemaElement.get_default_management_name()):
        super().__init__(vertex_label.value, mgmt_name)
        self.vertex_label = vertex_label

    def make_vertex_label_str(self) -> str:
        return f'{self.mgmt_name}.makeVertexLabel(\'{self.name}\').make();'

    def make_gremlin_str(self) -> str:
        return self.make_vertex_label_str()

    def __repr__(self):
        return f'Vertex Label {self.name}'


class JGSchemaEdgeLabel(JGSchemaElement):
    def __init__(self, name: str, edge_multiplicity: JGEdgeMultiplicity,
                 mgmt_name=JGSchemaElement.get_default_management_name()):
        super().__init__(name, mgmt_name)
        self.edge_multiplicity = edge_multiplicity

    def make_edge_label_str(self) -> str:
        return (f'{self.mgmt_name}.makeEdgeLabel(\'{self.name}\')'
                f'.multiplicity({self.edge_multiplicity.value}).make();')

    def make_gremlin_str(self) -> str:
        return self.make_edge_label_str()

    def __repr__(self):
        return f'{self.name} [{self.edge_multiplicity.value}]'


class JGPropertyIndex(JGSchemaElement):
    def __init__(self, name: str, schema_properties: list[JGSchemaProperty], is_vertex_index: bool = True):
        super().__init__(name, schema_properties[0].mgmt_name)
        self.is_vertex_index = is_vertex_index
        self.schema_properties = schema_properties

    def make_index_str(self) -> str:
        gremlin_ver_names = {schema_property: schema_property.name.replace('-', '_') for schema_property in
                             self.schema_properties}
        index_cmds = [f'graph.tx().rollback();\n',
                      f'{self.mgmt_name} = graph.openManagement();\n']

        for schema_property in self.schema_properties:
            index_cmds.append(f'{gremlin_ver_names[schema_property]} = {self.mgmt_name}')
            index_cmds.append(f'.getPropertyKey(\'{schema_property.name}\');\n', )

        index_cmds.append(f'{self.mgmt_name}.buildIndex(\'{self.name}\', ')
        index_cmds.append(f'{"Vertex" if self.is_vertex_index else "Edge"}.class)')

        for schema_property in self.schema_properties:
            index_cmds.append(f'.addKey({gremlin_ver_names[schema_property]})')

        index_cmds.append(f'.buildCompositeIndex();\n')
        index_cmds.append(f'{self.mgmt_name}.commit();\n')
        index_cmds.append(f'ManagementSystem.awaitGraphIndexStatus(graph, \'{self.name}\').call();\n')
        index_cmds.append(f'{self.mgmt_name} = graph.openManagement();\n')
        index_cmds.append(f'{self.mgmt_name}.updateIndex({self.mgmt_name}.getGraphIndex(\"{self.name}\"), ')
        index_cmds.append(f'SchemaAction.REINDEX).get();\n')
        index_cmds.append(f'{self.mgmt_name}.commit();\n')
        return "".join(index_cmds)

    def make_gremlin_str(self) -> str:
        return self.make_index_str()

    def __repr__(self):
        return f'Index {self.name} for properties {self.schema_properties}'


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

        logger.info(f'Containers started successfully')
        self.connection = DriverRemoteConnection('ws://localhost:8182/gremlin', 'g',
                                                 message_serializer=GraphSONSerializersV3d0())
        self.gremlin_client = Client(self.connection.url, self.connection.traversal_source)
        self.create_schema()
        self.create_indices()
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
        result = self.gremlin_client.submit(command_string).all().result()
        logger.debug(f'Got Gremlin server response {result}')
        return result

    def create_schema(self):
        logger.info(f'Creating schema')
        # There is no Python API for constructing the schema, so we must do this using Gremlin commands
        schema_commands = [f'{JGSchemaProperty.get_default_management_name()} = graph.openManagement();']

        for jg_property in HAKCDagStore.vertex_properties():
            schema_commands.append(jg_property.make_property_str())
        for jg_property in HAKCDagStore.edge_properties():
            schema_commands.append(jg_property.make_property_str())
        for jg_edge_label in HAKCDagStore.edge_labels():
            schema_commands.append(jg_edge_label.make_edge_label_str())
        for jg_vertex_label in HAKCDagStore.vertex_labels():
            schema_commands.append(jg_vertex_label.make_vertex_label_str())
        schema_commands.append(f'{JGSchemaProperty.get_default_management_name()}.commit();')
        try:
            self._send_gremlin_cmd_str(schema_commands)
        except Exception as e:
            logger.error(f'Received error creating schema: {e}')

    def create_indices(self):
        logger.info(f'Creating indices')
        indices_to_create = [
            JGPropertyIndex('byNameAndTypeComposite', [HAKCDagStore.name_property(), HAKCDagStore.type_property()]),
            JGPropertyIndex('byCompartmentIdComposite', [HAKCDagStore.compartment_id_property()])]

        for index_to_create in indices_to_create:
            logger.debug(f'Creating {index_to_create}')
            try:
                self._send_gremlin_cmd_str([index_to_create.make_gremlin_str()])
            except Exception as e:
                logger.error(f'Received error creating index: {e}')
                continue
            logger.debug(f'Finished index creation')

    @staticmethod
    def compartment_id_property() -> JGSchemaProperty:
        return JGSchemaProperty('compartment-id', JGDataType.Integer, JGCardinalityType.Single)

    @staticmethod
    def label_property() -> JGSchemaProperty:
        return JGSchemaProperty('label', JGDataType.String, JGCardinalityType.Single)

    @staticmethod
    def name_property() -> JGSchemaProperty:
        return JGSchemaProperty('name', JGDataType.String, JGCardinalityType.Single)

    @staticmethod
    def type_property() -> JGSchemaProperty:
        return JGSchemaProperty('type', JGDataType.String, JGCardinalityType.Single)

    @staticmethod
    def edge_weight_property() -> JGSchemaProperty:
        return JGSchemaProperty('weight', JGDataType.Float, JGCardinalityType.Single)

    @staticmethod
    def vertex_properties() -> set[JGSchemaProperty]:
        return {HAKCDagStore.compartment_id_property(),
                HAKCDagStore.name_property(),
                HAKCDagStore.type_property()}

    @staticmethod
    def edge_properties() -> set[JGSchemaProperty]:
        return {HAKCDagStore.edge_weight_property()}

    @staticmethod
    def edge_labels() -> set[JGSchemaEdgeLabel]:
        return set(HAKCDagStore.edge_label_map().values())

    @staticmethod
    def vertex_labels() -> set[JGSchemaVertexLabel]:
        return set(HAKCDagStore.vertex_label_map().values())

    @staticmethod
    def vertex_label_map() -> Mapping[JGVertexLabel, JGSchemaVertexLabel]:
        return {VertexLabel: JGSchemaVertexLabel(VertexLabel) for VertexLabel in JGVertexLabel}

    @staticmethod
    def edge_label_map() -> Mapping[JGEdgeLabel, JGSchemaEdgeLabel]:
        return {EdgeLabel: JGSchemaEdgeLabel(EdgeLabel.value, JGEdgeMultiplicity.Simple) for EdgeLabel in JGEdgeLabel}

    def add_vertex(self, properties: Mapping[JGSchemaProperty, int | str | float | Iterable],
                   check_for_existing: bool = True) -> int:
        if check_for_existing:
            existing_vertex = self.get_vertex_id(properties)
            if existing_vertex is not None:
                logger.debug(f'Vertex with properties {properties} is already in the graph')
                return existing_vertex
        else:
            logger.debug(f'Not checking for existing vertex')

        return self.add_vertex_no_check(properties)

    @staticmethod
    def _handle_properties(graph_element, properties: Mapping[JGSchemaProperty, int | str | float | Iterable],
                           setting_properties: bool, use_label_in_query: bool = False):
        for jg_property, property_value in properties.items():
            if property_value is None:
                raise RuntimeError(f'Value for property `{jg_property}` is None')
            value_is_iterable = isinstance(property_value, Iterable) and not isinstance(property_value, str)

            if setting_properties:
                if value_is_iterable:
                    for v in property_value:
                        graph_element = graph_element.property(jg_property.name, v)
                else:
                    if jg_property != HAKCDagStore.label_property():
                        graph_element = graph_element.property(jg_property.name, property_value)
            else:
                if value_is_iterable:
                    graph_element = graph_element.has(jg_property.name, within(*property_value))
                else:
                    if jg_property == HAKCDagStore.label_property():
                        if use_label_in_query:
                            graph_element = graph_element.has_label(property_value)
                    else:
                        graph_element = graph_element.has(jg_property.name, property_value)

        return graph_element

    def _search_properties(self, graph_element, properties: Mapping[JGSchemaProperty, int | str | float | Iterable],
                           use_label_in_query: bool = False):
        return self._handle_properties(graph_element, properties, False, use_label_in_query)

    def _set_properties(self, graph_element, properties: Mapping[JGSchemaProperty, int | str | float | Iterable]):
        return self._handle_properties(graph_element, properties, True)

    def add_vertex_no_check(self, properties: Mapping[JGSchemaProperty, int | str | float | Iterable]) -> int:
        logger.debug(f"Adding new vertex with properties {properties}")
        label = properties[HAKCDagStore.label_property()]
        jg_vertex = self.graph_traversal.add_v(label).next()
        property_traversal = self.graph_traversal.V(jg_vertex)
        property_traversal = self._set_properties(property_traversal, properties)
        try:
            property_traversal.iterate()
        except GremlinServerError as gremlin_error:
            logger.error(f'Error setting vertex properties: {gremlin_error}')
            raise gremlin_error
        logger.debug(f'Finished adding vertex {self.graph_traversal.V(jg_vertex).next()}')
        return jg_vertex.id

    def get_vertex_id(self, properties: Mapping[JGSchemaProperty, int | str | float],
                      use_label_in_query: bool = False) -> int | None:
        vertex_set = self.get_vertex_ids(properties, use_label_in_query)
        if len(vertex_set) == 0:
            logger.debug(f'No vertex found with properties {properties}')
            return None

        result = vertex_set.pop()
        logger.debug(f'Found result {result}')
        return result

    def get_vertex_ids(self, properties: Mapping[JGSchemaProperty, int | str | float | Iterable],
                       use_label_in_query: bool = False) -> set[int]:
        logger.debug(f'Finding vertices with properties {properties}')
        vertex_traversal = self.graph_traversal.V()
        vertex_traversal = self._search_properties(vertex_traversal, properties, use_label_in_query)
        initial_results = vertex_traversal.to_set()
        if HAKCDagStore.label_property() in properties and not use_label_in_query:
            result_set = set()
            label = properties[HAKCDagStore.label_property()]
            for result in initial_results:
                if result.label == label:
                    result_set.add(result)
        else:
            result_set = initial_results

        logger.debug(f'Found {len(result_set)} results')
        ids_to_return = {result.id for result in result_set}
        return ids_to_return

    def get_vertex(self, vertex_id: int) -> object | None:
        vertex_list = self.graph_traversal.V(vertex_id).to_list()
        if vertex_list and len(vertex_list) == 1:
            return vertex_list[0]
        return None

    def add_edge(self, from_vertex_id: int, to_vertex_id: int, edge_label: JGSchemaEdgeLabel,
                 properties: Mapping[JGSchemaProperty, int | str | float | Iterable]):
        if from_vertex_id is None:
            raise RuntimeError(f'from_vertex is None')
        if to_vertex_id is None:
            raise RuntimeError(f'to_vertex is None')
        if edge_label is None:
            raise RuntimeError(f'edge_label is None')

        from_vertex = self.get_vertex(from_vertex_id)
        to_vertex = self.get_vertex(to_vertex_id)

        existing_edge = self.graph_traversal.V(from_vertex).out(edge_label.name).has_id(to_vertex_id).to_list()
        if existing_edge and len(existing_edge) > 0:
            logger.debug(f'{edge_label} edge from {from_vertex_id} to {to_vertex_id} already exists')
            return

        logger.debug(f'Adding {edge_label} edge from {from_vertex_id} to {to_vertex_id} with properties {properties}')
        new_edge = self.graph_traversal.add_e(edge_label.name).from_(from_vertex).to(to_vertex)
        new_edge = self._set_properties(new_edge, properties)
        new_edge.iterate()
        logger.debug(f'Finished adding edge')

    def _get_vertices_by_label(self, vertex_label: JGVertexLabel) -> set[Vertex]:
        return self.graph_traversal.V().has_label(vertex_label.value).to_set()

    def _get_vertex_ids_by_label(self, vertex_label: JGVertexLabel) -> set[int]:
        vertices = self._get_vertices_by_label(vertex_label)
        return {vertex.id for vertex in vertices}

    def get_compartment_vertex_ids(self) -> set[int]:
        return self._get_vertex_ids_by_label(JGVertexLabel.Compartment)


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

    def get_vertex_ids(self, properties: Mapping[JGSchemaProperty, int | str | float | Iterable],
                       use_label_in_query: bool = False) -> set[int]:
        if not self.started:
            raise HAKCSessionNotStartedError
        return self.dag_store.get_vertex_ids(properties, use_label_in_query)

    def add_vertex(self, properties: Mapping[JGSchemaProperty, int | str | float | Iterable],
                   check_for_existing: bool = True) -> int:
        if not self.started:
            raise HAKCSessionNotStartedError
        return self.dag_store.add_vertex(properties, check_for_existing=check_for_existing)

    def add_edge(self, from_vertex: int, to_vertex: int, edge_label: JGSchemaEdgeLabel,
                 properties: Mapping[JGSchemaProperty, int | str | float | Iterable]):
        if not self.started:
            raise HAKCSessionNotStartedError
        self.dag_store.add_edge(from_vertex, to_vertex, edge_label, properties)

    def get_compartment_vertex_ids(self) -> set[int]:
        if not self.started:
            raise HAKCSessionNotStartedError
        return self.dag_store.get_compartment_vertex_ids()

    def __del__(self):
        self.shutdown()


DAGSession = HAKCDagSession()


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


def process_yaml_file(filename) -> YamlResult:
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


def populate_graph_from_yaml(filename: str, compartment_id: int):
    try:
        yaml_result = process_yaml_file(filename)
        compartment = create_compartment(compartment_id=compartment_id, yaml_result=yaml_result)

        type_vertices = dict()
        symbol_vertices = dict()
        logger.debug(f'Adding accessed types for {compartment_id}')
        for accessed_type in compartment.get_accessed_types():
            type_name = accessed_type.get_name()
            if type_name is None:
                type_name = f'type_{accessed_type.get_type_hash()}'

            properties = {HAKCDagStore.name_property(): type_name,
                          HAKCDagStore.type_property(): accessed_type.get_type_hash(),
                          HAKCDagStore.label_property(): JGVertexLabel.DataType.value,
                          }
            jg_vertex = DAGSession.add_vertex(properties)
            type_vertices[accessed_type] = jg_vertex

        logger.debug(f'Finished adding types for {compartment_id}')
        logger.debug(f'Adding accessed symbols for {compartment_id}')
        properties = {HAKCDagStore.compartment_id_property(): compartment_id,
                      HAKCDagStore.label_property(): JGVertexLabel.Compartment.value}
        compartment_vertex = DAGSession.add_vertex(properties)
        for accessed_symbol in compartment.get_accessed_symbols():
            properties = {HAKCDagStore.name_property(): accessed_symbol.get_name(),
                          HAKCDagStore.type_property(): accessed_symbol.get_hash(),
                          HAKCDagStore.label_property(): JGVertexLabel.Symbol.value}
            jg_vertex = DAGSession.add_vertex(properties)
            symbol_vertices[accessed_symbol] = jg_vertex
            DAGSession.add_edge(from_vertex=compartment_vertex,
                                to_vertex=jg_vertex,
                                edge_label=HAKCDagStore.edge_label_map()[JGEdgeLabel.InCompartment],
                                properties={})

        logger.debug(f'Finished adding accessed symbols for {compartment_id}')
        logger.debug(f'Adding symbol-type mapping for {compartment_id}')
        for accessed_symbol, symbol_vertex in symbol_vertices.items():
            for accessed_type in accessed_symbol.get_types():
                type_vertex = type_vertices.get(accessed_type)
                if type_vertex is not None:
                    DAGSession.add_edge(from_vertex=type_vertex,
                                        to_vertex=symbol_vertex,
                                        edge_label=HAKCDagStore.edge_label_map()[JGEdgeLabel.Uses],
                                        properties={})
                else:
                    logger.error(f'Could not find type vertex for {accessed_type}!')
        logger.debug(f'Finished symbol-type mapping for {compartment_id}')

        return compartment_id
    except Exception as e:
        raise RuntimeError(f'Error creating {compartment_id} from file {filename}: {e}')


def process_dag_files_multithreaded(filename_compartment_map: dict[int, str]):
    max_workers = max(1, int(mp.cpu_count() / 2))
    # max_workers = 1
    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        logger.info(f'Starting DAG file processing using {max_workers} workers')
        while len(filename_compartment_map) > 0:
            logger.info(f'Starting {len(filename_compartment_map)} jobs')
            futures = [
                executor.submit(populate_graph_from_yaml, filename_compartment_map[compartment_id], compartment_id)
                for compartment_id in filename_compartment_map.keys()]
            for future in concurrent.futures.as_completed(futures):
                try:
                    compartment_id = future.result()
                    del filename_compartment_map[compartment_id]
                    logger.info(f'Completed creating compartment {compartment_id}')
                except Exception as e:
                    logger.error(f'Error handling: {e}')


def process_dag_files_singlethread(filename_compartment_map: dict[int, str]):
    logger.info(f'Starting DAG file processing in single thread mode')
    for compartment_id, filename in filename_compartment_map.items():
        populate_graph_from_yaml(filename, compartment_id)
        logger.info(f'Completed creating compartment {compartment_id} from {filename}')


def create_new_dag(analysis_root: str, single_thread: bool):
    filename_compartment_map = dict()
    compartment_id = 1
    logger.info(f'Finding DAG files starting from {os.path.abspath(analysis_root)}')
    for root, subdirs, files in os.walk(analysis_root):
        for f in files:
            filename = os.path.join(root, f)
            if not filename.endswith(".dag.yml"):
                continue
            filename_compartment_map[compartment_id] = str(filename)
            compartment_id += 1

    logger.info(f'Found {len(filename_compartment_map)} DAG files')
    if single_thread:
        process_dag_files_singlethread(filename_compartment_map)
    else:
        process_dag_files_multithreaded(filename_compartment_map)
    logger.info(f'Completed compartment creation')
    compartment_ids = DAGSession.get_compartment_vertex_ids()
    logger.info(f'Created {len(compartment_ids)} compartments')


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


def parse_log_level(level_string: str):
    for level in LoggingLevelEnum:
        if level.name == level_string.upper():
            return level
    raise RuntimeError(f'Invalid log level {level_string}')


def main():
    parser = argparse.ArgumentParser(description='HAKC DAG generation and manipulation')
    parser.add_argument('-f', '--docker-file', required=True, dest='docker_file')
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('-o', '--docker-output', default='docker.out', dest='docker_out')
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('--create-new-dag', dest='dag_yaml_root', type=str,
                        help='/path/to/root/build/dir')
    parser.add_argument('--single-thread', dest='single_thread', action='store_true',
                        help='Run analysis without multiprocessing')

    args = parser.parse_args()

    setup_logging(log_file=args.log_path, log_level=args.log_level)

    DAGSession.start(docker_file=args.docker_file, docker_output_file_path=args.docker_out)

    try:
        if args.dag_yaml_root:
            create_new_dag(analysis_root=args.dag_yaml_root, single_thread=args.single_thread)
    finally:
        DAGSession.shutdown()


if __name__ == '__main__':
    main()
