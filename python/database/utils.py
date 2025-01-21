# utils.py 
import json
import sys
import time
import socket
import threading
import selectors
import types
import errno
import os
import kuzu
import time 
import multiprocessing as mp 
from logger import *
import HAKCDatabase


HOST = "127.0.0.1"  # The server's hostname or IP address
PORT = 5353  # The port used by the server
timeout = 5
kuzu_database = "somedb"

class Message:
    def __init__(self, timestamp : int, thread_id : str = None, type : str = None , payload : dict = {}):
        self.data = {"timestamp" : timestamp, "thread_id" : thread_id, "type" : type, "payload" : payload}
        self.timestamp = self.data["timestamp"]
        self.thread_id = self.data["thread_id"]
        self.type = self.data["type"]
        self.payload = self.data["payload"]

    def __str__(self):
        # TODO: add prettier printing for each type 
        if(len(self.payload) == 0):
            return f"<{int(self.timestamp/1000000000)}, {self.thread_id}, {self.type}>"
        else:
            return f"<{int(self.timestamp/1000000000)}, {self.thread_id}, {self.type}, ({self.payload})>"
    
    def toStr(self):
        return json.dumps(self.data)

    def bytes(self):
        return bytes(self.toStr(),'utf-8')
    
def strToMsg(data : str):
    if(data == None):
        raise Exception("data is None")
    if(not(isinstance(data, str))):
        raise Exception("data is not string type")
    jsonData = json.loads(data)
    return Message(jsonData["timestamp"], jsonData["thread_id"], jsonData["type"], jsonData["payload"])


class ServerThread:
    def __init__(self, sel : selectors, HAKC_Database_Obj):
        self.sel = sel 
        self.server_id = threading.get_ident()
        self.MSGS = list()
        self.KILLED = False
        self.conn = HAKC_Database_Obj.new_conn()
        # int(mp.cpu_count() / 2)

    def __del__(self):
        # if(self.SOCK):
        #     self.SOCK.close()
        if(not(self.KILLED)):
            logger.error(f"{self} killed IMPROPERLY!")
        else:
            logger.debug(f"{self} killed successfully!")


    # def checkMessageValidity(self, msg : Message):
    #     # ensures that the message replica_id matches the current replica_id
    #     if(msg.server_id == self.server_id):
    #         return None
    #     raise Exception("FAIL: Message sent to wrong server (this should NEVER be called); msg server: " + msg.server_id + " != object server: " + self.server_id)
    
    def __str__(self):
        return f"server_id: {self.server_id})"

    def kill(self):
        # mark the thread as killed gracefully (not unexpectedly died)
        self.KILLED = True

    def debug(self):
        # report the name of all active threads
        for thread in threads:
            logger.debug("thread: " + thread.name)
    
    def service_connection(self, conn, mask):
        # conn is of type socket.socket 
        # print( 'active threads', threading.active_count(), " ", len(ClientThreads))
        data_buff = None 
        if mask & selectors.EVENT_READ:
            try:
                recv_data = conn.recv(1024).decode('utf-8')  # Should be ready to read
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
                # logger.receive(f"Received {MSG.type} from {conn.getsockname()} \t{MSG}")
                data_buff = self.createReply(MSG)
            else:
                print(f"Closing connection to {conn.getsockname()}, recv got {recv_data}")
                self.sel.unregister(sock)
                sock.close()
        if mask & selectors.EVENT_WRITE:
            if data_buff:
                # print(f"Acking {conn.getsockname()}")
                sent = conn.sendall(data_buff.bytes())
    def service_connection_wrapper(self, conn, mask):
        # while 1:
        #     print(threading.active_count())
        self.sel.register(conn, selectors.EVENT_READ | selectors.EVENT_WRITE, self.service_connection)

    def createReply(self, msg : Message):
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
