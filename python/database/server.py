# server.py 
# https://realpython.com/python-sockets/

import sys
import socket
import selectors
import types
from kuzu import KuzuConnection
from utils import *

#     updateState(MSG)
#     reply = createReply(MSG)
#     # send reply 
#     try:
#         conn.sendall(reply.bytes())
#     except Exception as err:
#         logger.error("LFD failed to send all data " + self + " " + err)
#         break
#     logger.send(f"{server_id} sends MEMBERSHIP_TS (ACK) to {client_id}\t{reply}")
#     time.sleep(timeout)



# Bind the socket to a path name
path_name = "/tmp/my_socket"

def createReply(msg : Message):
    # get current state so the client knows the correct count 
    # create a REPLY message and ensures that all incoming messages are "REQUEST"
    if(msg.type != "QUERY"):
        raise Exception("FAIL: Message is not of type QUERY: {msg}")
    lk = threading.Lock()
    with lk:
        t0 = time.time_ns()
        kuzu_conn = KuzuConnection(kuzu_database)
        result = kuzu_conn.execute(msg.payload)
        for row in result:
            print(row)
        kuzu_conn.close()
        return Message(t0, -1, "REPLY", {"payload":result})

def accept_wrapper(sock):
    conn, addr = sock.accept()  # Should be ready to read
    print(f"Accepted connection from {addr}")
    conn.setblocking(False)
    data = types.SimpleNamespace(addr=addr, inb=b"", outb=b"")
    events = selectors.EVENT_READ | selectors.EVENT_WRITE
    sel.register(conn, events, data=data)

def service_connection(key, mask):
    sock = key.fileobj
    data = key.data
    data.outb = None 
    # need to put this in a loop?
    if mask & selectors.EVENT_READ:
        try:
            recv_data = sock.recv(1024).decode('utf-8')  # Should be ready to read
        except Exception as err:
            logger.error("recv failed - " + str(err))
            return
        if recv_data:
            MSG = None
            try:
                MSG = strToMsg(recv_data)
            except Exception as err:
                logger.error("strToMsg FAILED; " + str(err) + " " + str(recv_data))
                return
            logger.receive(f"Received {MSG.type} from {data.addr} \t{MSG}")
            # generate reply 
            # data.outb += "ACK"
            data.outb = createReply(MSG)
        else:
            print(f"Closing connection to {data.addr}, recv got {recv_data}")
            sel.unregister(sock)
            sock.close()
    if mask & selectors.EVENT_WRITE:
        if data.outb:
            # print(f"Acking {data.outb!r} to {data.addr}")
            print(f"Acking {data.addr}")
            sent = sock.sendall(data.outb.bytes())
            # sent = sock.send(data.outb)  # Should be ready to write
            # data.outb = data.outb[sent:]

sel = selectors.DefaultSelector()

# Create a socket
srvSock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
srvSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srvSock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try: 
    srvSock.bind((HOST,PORT))
    # srvSock.bind(path_name)
except Exception as err:
    logger.error(str(err) + " ")
srvSock.listen()
logger.debug(f"Listening on {(HOST, PORT)}")
srvSock.setblocking(False)
sel.register(srvSock, selectors.EVENT_READ, data=None)

try:
    while True:
        events = sel.select(timeout=None)
        for key, mask in events:
            if key.data is None:
                accept_wrapper(key.fileobj)
            else:
                service_connection(key, mask)
except KeyboardInterrupt:
    print("Caught keyboard interrupt, exiting")
finally:
    sel.close()





# # Get the file descriptor
# fd = srvSock.fileno()

# Close the socket
srvSock.close()

# # Now you can use the file descriptor
# print(fd)
