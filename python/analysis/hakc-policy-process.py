import argparse
import json
import logging
import signal
from enum import Enum
from pathlib import Path
import subprocess
import os 

from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging
from hakc.HAKCPolicyServer import HAKCPolicyServer, NullHAKCPolicyDataStore, HAKCPolicyDataSource, \
    JSONHAKCPolicyDataStore, TimeoutException

logger = logging.getLogger('hakc-policy-process')


class SupportedBackingStore(Enum):
    NULL = "null",
    JSON = "json"


class HAKCPolicyProcessConfig:
    def __init__(self, socket_path: str, backing_store: dict[str, str], **kwargs):
        self.backing_store = backing_store
        self.socket_path = Path(socket_path)
        self.reuse_path = kwargs.get('reuse_path', False)
        self.log_path = kwargs.get('log_path', None)
        self.server_timeout = int(kwargs.get('server_timeout', -1))
        if self.reuse_path and self.socket_path.exists():
            self.socket_path.unlink()


def init_data_source(config: HAKCPolicyProcessConfig) -> HAKCPolicyDataSource:
    if config.backing_store['type'] == SupportedBackingStore.NULL.value:
        logger.debug(f'Creating NullHAKCPolicyDataStore')
        return NullHAKCPolicyDataStore()
    elif config.backing_store['type'] == SupportedBackingStore.JSON.value:
        logger.debug(f'Creating JSONPolicyDataStore')
        return JSONHAKCPolicyDataStore(jsonin=config.backing_store['path'],default_compartment=config.backing_store['default_compartment'],default_division=config.backing_store['default_division'])
    raise RuntimeError(f"Unsupported data store type: {config.backing_store['type']}")


def timeout_handler(signum, frame):
    raise TimeoutException


def main():
    parser = argparse.ArgumentParser(description='HAKC Policy Process')
    parser.add_argument('--config', help='Path to config file', required=True)
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-mode', default='w', dest='log_mode')

    args = parser.parse_args()
    setup_logging(logger, log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)
    with open(args.config, 'r') as f:
        parsed_config = json.load(f)
        config = HAKCPolicyProcessConfig(**parsed_config)

    # create subprocess to allow server to run in the background, while allowing parent to die (workaround for llvm lit requiring all programs terminate before finishing tests)
    print(f"Parent PID: {os.getpid()}")
    try: 
        pid = os.fork() 
    except OSError: 
        exit("Could not create a child process") 
    
    if(pid > 0):
        print(f"Parent PID: {os.getpid()}, killing self")
        os.kill(os.getpid(), signal.SIGTERM)
    else:
        print(f"Child PID: {os.getpid()}")

    data_source = init_data_source(config)
    with HAKCPolicyServer(backing_store=data_source, socket_path=config.socket_path, log_level=args.log_level,
                          log_file=config.log_path, log_mode=args.log_mode) as server:
        try:
            if config.server_timeout > 0:
                signal.signal(signal.SIGALRM, timeout_handler)
                signal.alarm(config.server_timeout)

            server.serve_forever()
        except KeyboardInterrupt:
            logger.info('User requested to stop server')
        except TimeoutException:
            logger.info(f'Timeout received')
        except Exception as e:
            logger.error(f'Error: {e}')
        finally:
            server.server_close()


if __name__ == "__main__":
    main()
