# client.py
# this client is only used for testing purposes 
# https://realpython.com/python-sockets/

import socket
from utils import *


thread_id = -1
msg_type = "QUERY"
if len(sys.argv) == 2:
    logger.debug(f"{sys.argv[0]} {sys.argv[1]}")
    thread_id = sys.argv[1]

# (sys.argv[1], int(sys.argv[2])
# Bind the socket to a path name
socket_path = "/tmp/my_socket"

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(10000)
# try to connect to server
while True:
    try:
        sock.connect(socket_path)
        logger.debug(f"Connected to {socket_path}\t")
        break
    except Exception as err:
        logger.error(str(err))
        time.sleep(timeout)
        continue
# generate query 
while True:
    query_msg = Message(time.time_ns(), thread_id, "QUERY", {"payload":"SOME_QUERY_PAYLOAD"})
    try:
        sock.sendall(query_msg.bytes())
    except Exception as err:
        logger.error("FAILED to send; " + str(err))
        break
    try:
        data = sock.recv(1024).decode('utf-8')
    except Exception as err:
        logger.error("FAILED to recv; " + str(err))
        break
    try: 
        reply_msg = strToMsg(data)
        logger.debug(f"SUCCESSFULLY QUERIED: {query_msg} with response {reply_msg}")
        # break
    except Exception as err:
        logger.error("strToMsg FAILED; " + str(err) + " " + str(data))
        break

    time.sleep(timeout)
