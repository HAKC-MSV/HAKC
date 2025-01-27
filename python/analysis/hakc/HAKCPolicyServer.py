import json
import logging
import socketserver
import struct
from pathlib import Path
from typing import Optional

from .HAKCBase import HAKCPrintableObj
from .HAKCLogger import setup_logging, LoggingLevelEnum
from .HAKCObjects import HAKCSymbol, HAKCCompilationUnit, HAKCFunction, HAKCType, HAKCCompartment, HAKCDivision, \
    HAKCScope, HAKCGlobalVariable
from .HAKCCompartmentalization import HAKCCompartmentalization
import networkx as nx
from networkx.readwrite import json_graph

logger = logging.getLogger('hakc-policy-server')


class HAKCDataRequest:
    def __init__(self, Endpoint: str, **kwargs):
        self.endpoint = Endpoint
        self.parameters = kwargs.get('Parameters', dict())


class HAKCPolicyDataSource:
    def __init__(self, **kwargs):
        self.get_compartment_endpoint = kwargs.get('get_compartment_endpoint', 'get-compartment')

    def _get_default_division(self) -> HAKCDivision:
        raise NotImplementedError

    def _get_default_compartment(self) -> HAKCCompartment:
        raise NotImplementedError

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        raise NotImplementedError

    def get_compartment_by_id(self, compartment_id: int) -> HAKCCompartment:
        compartment = self._get_compartment_from_backing_store(compartment_id)
        if compartment is None:
            compartment = self._get_default_compartment()
        logger.debug(f"Returning Compartment {compartment} from input compartment {compartment_id}")
        return compartment

    def close(self) -> None:
        pass

    def handle_request(self, request: HAKCDataRequest) -> HAKCPrintableObj:
        if request.endpoint == self.get_compartment_endpoint:
            return self.get_compartment_by_id(int(request.parameters['compartment-id']))

        raise RuntimeError(f'Invalid Endpoint {request.endpoint}')


class NullHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, **kwargs):
        HAKCPolicyDataSource.__init__(self, **kwargs)
        self.default_compartment = HAKCCompartment(0, **kwargs)
        self.default_division = HAKCDivision(0, self.default_compartment.compartment_id, **kwargs)

    def _get_default_compartment(self) -> HAKCCompartment:
        return self.default_compartment

    def _get_default_division(self) -> HAKCDivision:
        return self.default_division

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self._get_default_compartment()


class JSONHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, **kwargs):
        HAKCPolicyDataSource.__init__(self, **kwargs)
        self.compartmentalization = None 
        self.deserialize_compartmentalization_init(**kwargs)
        self.default_compartment = kwargs['default_comparment']
        self.default_division = kwargs['default_division']
        
    def _get_default_compartment(self) -> HAKCCompartment:
        return self.default_compartment

    def _get_default_division(self) -> HAKCDivision:
        return self.default_division

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self.compartmentalization.get_compartment_node(compartment_id)

    def get_compartment_by_id(self, compartment_id: int) -> HAKCCompartment:
        return self._get_compartment_from_backing_store(compartment_id)

    def deserialize_compartmentalization_init(self, **kwargs):
        jsonin = kwargs['jsonin']
        # print(jsonin)
        if(jsonin == None):
            raise RuntimeError(f'jsonin is NULL: {jsonin}')
        with open(jsonin, 'r') as file:
            data = json.load(file)
        if(data == None):
            raise RuntimeError(f'jsonin is empty: {jsonin}')
        abc = self.deserialize_compartmentalization(data)
        # print(abc)
        deser = json_graph.node_link_graph(abc, directed=True, multigraph=True)
        # print(deser)
        self.compartmentalization = HAKCCompartmentalization(deser)
        print(self.compartmentalization)

    def deserialize_compartmentalization(self, data):
        # turn the json back into HAKCCompartmentalization object 
        # need to recurse and instantiate objects for each HAKC*
        # print(data)
        if isinstance(data, bool):
            return data
        elif isinstance(data, str):
            return data 
        elif isinstance(data, int):
            return data 
        elif isinstance(data, list):
            for i in range(len(data)):
                data[i] = self.deserialize_compartmentalization(data[i])
        elif isinstance(data, dict):
            # need to distinguish what object to create based on dictionary keys
            # print()
            # print("dict keys: " , data.keys())
            # print()
            keys = data.keys()
            # Root NetworkX 
            if ("directed" in keys and 
                "multigraph" in keys and 
                "graph" in keys and 
                "links" in keys and 
                "nodes" in keys):
                # print("At root!")
                data["directed"] = self.deserialize_compartmentalization(data["directed"])
                data["multigraph"] = self.deserialize_compartmentalization(data["multigraph"])
                data["graph"] = self.deserialize_compartmentalization(data["graph"])
                data["links"] = self.deserialize_compartmentalization(data["links"])
                data["nodes"] = self.deserialize_compartmentalization(data["nodes"])
            # NetworkX Links data structutre
            elif ("key" in keys and 
                  "persisted" in keys and 
                  "source" in keys and 
                  "target" in keys):
                # print()
                # print(data)
                # print("Found nx Link ds")
                # print()
                data["key"] = self.deserialize_compartmentalization(data["key"])
                data["persisted"] = self.deserialize_compartmentalization(data["persisted"])
                data["source"] = self.deserialize_compartmentalization(data["source"])
                data["target"] = self.deserialize_compartmentalization(data["target"])
            # NetworkX Nodes data structutre 
            elif ("id" in keys and 
                  "persisted" in keys):
                # print()
                # print(data)
                # print("Found nx Node ds")
                # print()
                data["id"] = self.deserialize_compartmentalization(data["id"])
                data["persisted"] = self.deserialize_compartmentalization(data["persisted"])
            # HAKCFunction
            elif ("name" in keys and 
                  "type" in keys and 
                  "scope" in keys and 
                  "definition" in keys and 
                  "indirectCall" in keys and
                  "directCall" in keys):
                data["Name"] = self.deserialize_compartmentalization(data["name"])
                data["Type"] = HAKCType(data["type"]["debug_type"], data["type"]["llvm_type"])
                data["Scope"] = self.deserialize_compartmentalization(data["scope"])
                data["DefiningFile"] = self.deserialize_compartmentalization(data["definition"])
                data["DirectCalls"] = self.deserialize_compartmentalization(data["directCall"])
                data["IndirectCalls"] = self.deserialize_compartmentalization(data["indirectCall"])
                return HAKCFunction(**data)
            # HAKCGlobalVariable is HAKCSymbol, with is_global 
            elif ("definition" in keys and 
                  "name" in keys and 
                  "scope" in keys and 
                  "type" in keys and 
                  "is_global" in keys):
                data["Name"] = self.deserialize_compartmentalization(data["name"])
                data["Type"] = HAKCType(data["type"]["debug_type"], data["type"]["llvm_type"])
                data["Scope"] = self.deserialize_compartmentalization(data["scope"])
                data["DefiningFile"] = self.deserialize_compartmentalization(data["definition"])
                return HAKCGlobalVariable(**data)
            # HAKCSymbol
            elif ("definition" in keys and 
                  "name" in keys and 
                  "scope" in keys and 
                  "type" in keys):
                data["Name"] = self.deserialize_compartmentalization(data["name"])
                data["Type"] = HAKCType(data["type"]["debug_type"], data["type"]["llvm_type"])
                data["Scope"] = self.deserialize_compartmentalization(data["scope"])
                data["DefiningFile"] = self.deserialize_compartmentalization(data["definition"])
                return HAKCSymbol(**data)
            # HAKCCompartment
            elif ("compartment_id" in keys and 
                  "divisions" in keys):
                data["compartment_id"] = self.deserialize_compartmentalization(data["compartment_id"])
                data["divisions"] = self.deserialize_compartmentalization(data["divisions"])
                return HAKCCompartment(data["compartment_id"], Divisions=data["divisions"])
            # HAKCDivision
            elif ("access_token" in keys and 
                  "compartment_id" in keys and
                  "division_id" in keys):
                return HAKCDivision(data["division_id"], data["compartment_id"], AccessToken=data["access_token"])
            # HAKCCompilationUnit
            elif ("filename" in keys):
                return HAKCCompilationUnit(data["filename"])
            # HAKCScope
            elif ("scope" in keys):
                if ("local_scope_name" in keys):
                    # TODO: change this to accept multiple local scope names? 
                    return HAKCScope(Name=data["scope"], LocalScopeName=data["local_scope_name"])
                else:
                    return HAKCScope(Name=data["scope"])
            # HAKCType 
            elif ("debug_type" in keys and 
                  "llvm_type" in keys):
                return HAKCType(data["debug_type"], data["llvm_type"])
            elif data == dict():
                return data
            else:
                print("____ UNKNOWN DICT KEYS TYPE ____ : " , type(data), " " ,data)
                return data 
        else:
            logger.info(f"Invalid key type: {type(data)}")
        return data


class HAKCRequestHandler(socketserver.StreamRequestHandler):
    size_fmt = "@L"

    def read_raw_bytes(self, size: int) -> bytes:
        logger.debug(f'Trying to read {size} bytes')
        raw_bytes = self.rfile.read(size)
        if len(raw_bytes) != size:
            raise ConnectionAbortedError
        return raw_bytes

    def write_raw_bytes(self, raw_bytes: bytes):
        try:
            self.wfile.write(raw_bytes)
        except OSError:
            raise ConnectionAbortedError

    def handle(self):
        logger.debug(f'Handling request')
        size_in_bytes = struct.calcsize(HAKCRequestHandler.size_fmt)
        try:
            while True:
                raw_size_bytes = self.read_raw_bytes(size_in_bytes)
                msg_size = struct.unpack(HAKCRequestHandler.size_fmt, raw_size_bytes)[0]
                logger.debug(f'Received msg_size {msg_size}')
                raw_msg_bytes = self.read_raw_bytes(msg_size)
                logger.debug(f'Received {len(raw_msg_bytes)} bytes')

                json_request = json.loads(raw_msg_bytes)
                hakc_request = HAKCDataRequest(**json_request)
                data = self.server.backing_store.handle_request(hakc_request)
                response_data = json.dumps(data.to_yaml_dict())
                encoded_data = response_data.encode('utf-8')

                self.write_raw_bytes(struct.pack(HAKCRequestHandler.size_fmt, len(encoded_data)))
                self.write_raw_bytes(encoded_data)
        except ConnectionAbortedError or ConnectionResetError:
            logger.debug(f'Client Disconnected')
            return
        except Exception as e:
            logger.error(f"Error handling request: {e}")


class HAKCPolicyServer(socketserver.ThreadingUnixStreamServer):
    def __init__(self, socket_path: Path, backing_store: HAKCPolicyDataSource, log_level=LoggingLevelEnum.INFO,
                 log_file: str = "", log_mode: str = 'w', **kwargs):
        self.backing_store = backing_store
        setup_logging(logger, log_level=log_level, log_file=log_file, log_mode=log_mode)
        logger.info(f'Starting Socket Server at {socket_path}')
        socketserver.ThreadingUnixStreamServer.__init__(self, str(socket_path), RequestHandlerClass=HAKCRequestHandler)
