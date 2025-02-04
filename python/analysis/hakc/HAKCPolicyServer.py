import json
import logging
import socketserver
import struct
from pathlib import Path
from typing import Optional
import yaml

from .HAKCBase import HAKCPrintableObj
from .HAKCLogger import setup_logging, LoggingLevelEnum
from .HAKCObjects import HAKCSymbol, HAKCCompilationUnit, HAKCFunction, HAKCType, HAKCCompartment, HAKCDivision, \
    HAKCScope, HAKCGlobalVariable, HAKCObject_constructors
from .HAKCCompartmentalization import HAKCCompartmentalization
from .HAKCDatabase import HAKCDatabase
import networkx as nx

logger = logging.getLogger('hakc-policy-server')


class TimeoutException(Exception):
    pass

class HAKCDataRequest:
    def __init__(self, Endpoint: str, **kwargs):
        self.endpoint = Endpoint
        self.parameters = kwargs.get('Parameters', dict())


class HAKCPolicyDataSource:
    def __init__(self, **kwargs):
        self.get_compartment_endpoint = kwargs.get('get_compartment_endpoint', 'get-compartment')
        self.get_division_endpoint = kwargs.get('get_division_endpoint', 'get-division')

    def _get_default_division(self) -> HAKCDivision:
        raise NotImplementedError

    def _get_default_compartment(self) -> HAKCCompartment:
        raise NotImplementedError

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        raise NotImplementedError

    def _get_division_from_backing_store(self, division_id: int) -> Optional[HAKCDivision]:
        raise NotImplementedError

    def get_division_by_id(self, compartment_id: int, division_id: int) -> HAKCDivision:
        # TODO: Q: is there any need to get the compartment too?
        # compartment_entry_token = self.get_compartment_by_id(compartment_id)
        division = self._get_division_from_backing_store(division_id)
        if division is None:
            return self.default_division
        logger.debug(f"Returning Division {division} from (compartment_id, division_id): ({compartment_id}, {division_id})")
        return division

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
        elif request.endpoint == self.get_division_endpoint:
            return self.get_division_by_id(int(request.parameters['compartment-id']), int(request.parameters['division-id']))
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

    def _get_division_from_backing_store(self, division_id: int) -> Optional[HAKCDivision]:
        return self._get_default_division()

class YAMLHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, yamlin: str, default_compartment_id: int, default_division_id: int, **kwargs):
        HAKCPolicyDataSource.__init__(self, **kwargs)
        self.compartmentalization = None 
        self.deserialize_compartmentalization(yamlin)
        self.default_compartment = HAKCCompartment(default_compartment_id)
        self.default_division = HAKCDivision(default_division_id, default_compartment_id)
        
    def _get_default_compartment(self) -> HAKCCompartment:
        return self.default_compartment

    def _get_default_division(self) -> HAKCDivision:
        return self.default_division

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self.compartmentalization.get_compartment_node(compartment_id)

    def _get_division_from_backing_store(self, division_id: int) -> Optional[HAKCDivision]:
        return self.compartmentalization.get_division_node(division_id)

    def deserialize_compartmentalization(self, yamlin):
        G = None
        # add custom constructors to yaml loader 
        # I guess networkx constructor stuff is not in safeloader, but is in loader?
        loader = yaml.Loader
        for yaml_tag, ctor in HAKCObject_constructors.items():
            loader.add_constructor(yaml_tag, ctor)

        if(yamlin == None):
            raise RuntimeError(f'yamlin is None')
        with open(yamlin, 'r') as file:
            G = yaml.load(file, Loader=loader)
        if(G == None):
            raise RuntimeError(f'Graph from yamlin is empty')
        
        # print(G)
        self.compartmentalization = G
        logger.debug(f'Successfully deserialized compartmentalization info! {self.compartmentalization}')


class KUZUHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, kuzuin: str, default_compartment_id: int, default_division_id: int, **kwargs):
        HAKCPolicyDataSource.__init__(self, **kwargs)
        self.database = None
        self.compartmentalization = None
        self.connect(kuzuin)
        self.default_compartment = HAKCCompartment(default_compartment_id)
        self.default_division = HAKCDivision(default_division_id, default_compartment_id)

    def _get_default_compartment(self) -> HAKCCompartment:
        return self.default_compartment

    def _get_default_division(self) -> HAKCDivision:
        return self.default_division

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        logger.debug(f"Trying to get compartment_id: {compartment_id} from backing store")
        return self.database.get_compartment_node(compartment_id)


    def _get_division_from_backing_store(self, division_id: int) -> Optional[HAKCDivision]:
        logger.debug(f"Trying to get division_id: {division_id} from backing store")
        return self.database.get_division_node(division_id)


    def connect(self, kuzuin):
        self.database = HAKCDatabase(kuzuin)
        self.database.open(True)

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

                if not(isinstance(data, HAKCPrintableObj)):
                    logger.error(f"Data received is not a HAKCPrintableObj, and is invalid: {data}")
                    raise Exception
                response_data = json.dumps(data.to_yaml_dict())
                encoded_data = response_data.encode('utf-8')

                self.write_raw_bytes(struct.pack(HAKCRequestHandler.size_fmt, len(encoded_data)))
                self.write_raw_bytes(encoded_data)
        # the 'raise' will call 'handle_error' in HAKCPolicyServer 
        except ConnectionAbortedError:
            logger.debug(f'Client Aborted Connection')
            raise
        except ConnectionResetError:
            logger.debug(f'Client Reset Connection')
            raise
        except TimeoutException:
            logger.debug(f'Timeout received')
            return
        except Exception as e:
            logger.error(f"Error handling request: {e}")
    

class HAKCPolicyServer(socketserver.ThreadingUnixStreamServer):
    def __init__(self, socket_path: Path, backing_store: HAKCPolicyDataSource, log_level=LoggingLevelEnum.INFO,
                 log_file: str = "", log_mode: str = 'w', **kwargs):
        self.backing_store = backing_store
        setup_logging(logger, log_level=log_level, log_file=log_file, log_mode=log_mode)
        logger.debug(f'Starting Socket Server at {socket_path}')
        socketserver.ThreadingUnixStreamServer.__init__(self, str(socket_path), RequestHandlerClass=HAKCRequestHandler)

    def handle_error(self, _a, _b):
        logger.error(f"Shutting down server")
        # do a server shutdown, rather than a server_close()
        self.shutdown()