# utils.py 
import json
import sys
import time
import socket
import threading
from logger import *

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


