# server.py 
# https://realpython.com/python-sockets/

import sys
import socket
import selectors
import types
import errno
import os
import kuzu
from utils import *


def createReply(msg : Message):
    # get current state so the client knows the correct count 
    # create a REPLY message and ensures that all incoming messages are "REQUEST"
    if(msg.type != "QUERY"):
        raise Exception("FAIL: Message is not of type QUERY: {msg}")
    lk = threading.Lock()
    with lk:
        t0 = time.time_ns()
        # self.database = kuzu.Database(self.db_dir, read_only=read_only, max_num_threads=max_num_threads)
        # self.conn = kuzu.Connection(self.database)
        # put HAKCDatabase Here 
        # kuzu_conn = KuzuConnection(kuzu_database)
        # result = kuzu_conn.execute(msg.payload)
        # for row in result:
        #     print(row)
        # kuzu_conn.close()
        return Message(t0, -1, "REPLY", {"payload":"result"})

def accept_wrapper(sock):
    # create new thread for every client connection 
    conn, addr = sock.accept()  # Should be ready to read
    print(f"Accepted connection from {conn}")
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
            data.outb = createReply(MSG)
        else:
            print(f"Closing connection to {data.addr}, recv got {recv_data}")
            sel.unregister(sock)
            sock.close()
    elif mask & selectors.EVENT_WRITE:
        if data.outb:
            print(f"Acking {data.addr}")
            sent = sock.sendall(data.outb.bytes())
    else:
        logger.error("service_connection do nothing\n")

# Bind the socket to a path name
socket_path = "/tmp/my_socket"

sel = selectors.DefaultSelector()

def main():
    # Create a socket
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    # sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try: 
        # sock.bind((HOST,PORT))
        sock.bind(socket_path)
    except Exception as err:
        if err.errno == errno.EADDRINUSE:
            # Handle the error (e.g., remove the socket file)
            logger.error(str(err) + "; unlinking and rebinding " + socket_path + " FD")
            os.unlink(socket_path)
            sock.bind(socket_path)
        else:
            logger.error(str(err) + " ")
    sock.listen()
    # logger.debug(f"Listening on {(HOST, PORT)}")
    logger.debug(f"Listening on {socket_path}")
    sock.setblocking(False)
    sel.register(sock, selectors.EVENT_READ, data=None)

    try:
        while True:
            events = sel.select(timeout=None)
            for key, mask in events:
                print(f"key {key[0]}\n")
                if key.data is None:
                    accept_wrapper(key.fileobj)
                else:
                    service_connection(key, mask)
    except KeyboardInterrupt:
        logger.error("Caught keyboard interrupt, exiting")
    finally:
        sock.close()
        sel.close()
        os.unlink(socket_path)

if __name__ == "__main__":
    main()

