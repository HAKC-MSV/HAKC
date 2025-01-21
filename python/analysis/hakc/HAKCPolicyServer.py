import json
import logging
import socketserver
from pathlib import Path
from typing import Optional

from .HAKCBase import HAKCPrintableObj
from .HAKCLogger import setup_logging, LoggingLevelEnum
from .HAKCObjects import HAKCCompartment, HAKCDivision

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


class HAKCRequestHandler(socketserver.DatagramRequestHandler):

    def handle(self):
        logger.debug(f'Handling request')
        try:
            json_request = json.load(self.rfile)
            hakc_request = HAKCDataRequest(**json_request)
            data = self.server.backing_store.handle_request(hakc_request)
            response_data = json.dumps(data.to_yaml_dict())
        except Exception as e:
            logger.error(f"Error handling request: {e}")
            response_data = json.dumps({'error': str(e)})
        finally:
            bytes_to_write = bytes(response_data.encode('utf-8'))
            self.wfile.write(bytes_to_write)


class HAKCPolicyServer(socketserver.ThreadingUnixDatagramServer):
    def __init__(self, socket_path: Path, backing_store: HAKCPolicyDataSource, log_level=LoggingLevelEnum.INFO,
                 log_file: str = "", log_mode: str = 'w', **kwargs):
        self.backing_store = backing_store
        setup_logging(logger, log_level=log_level, log_file=log_file, log_mode=log_mode)
        logger.info(f'Starting Socket Server at {socket_path}')
        socketserver.ThreadingUnixDatagramServer.__init__(self, str(socket_path), HAKCRequestHandler)
