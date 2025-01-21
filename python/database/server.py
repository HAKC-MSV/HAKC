# server.py 
# https://realpython.com/python-sockets/
from utils import *

def accept(sock, mask):
    conn, addr = sock.accept()  # Should be ready
    conn.setblocking(False)
    print('accepted', conn, 'from', addr, 'active threads', threading.active_count(), " ", len(ClientThreads))
    if(len(ClientThreads) < max_num_threads=int(mp.cpu_count() / 2)){
        t = ServerThread(sel)
        thread = threading.Thread(target=t.service_connection_wrapper, args=(conn, mask))
        ClientThreads.append(thread) # apparently this is thread safe 
        thread.start()
    }
    else{
        logger.error("Trying to make too many threads!\n")
    }
    # threads = threading.enumerate()

# Bind the socket to a path name
socket_path = "/tmp/my_socket"

sel = selectors.DefaultSelector()
ClientThreads = list()

def main():
    # https://docs.python.org/3/library/selectors.html
    # if the socket already exists, then remove it (likely left over from crash)
    if os.path.exists(socket_path):
        os.unlink(socket_path)
        logger.debug("Unlinking and rebinding " + socket_path + " FD")
    # Create a socket
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try: 
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
    logger.debug(f"Listening on {socket_path}")
    sock.setblocking(False)
    sel.register(sock, selectors.EVENT_READ, accept)

    # create HAKCDatabase
    db_dir = ""
    logger.debug(f'Creating database at {db_dir}')
    mp_conn = HAKCDatabase(db_dir, read_only=True)


    try:
        while True:
            events = sel.select()
            for key, mask in events:
                callback = key.data
                callback(key.fileobj, mask)
    except KeyboardInterrupt:
        logger.error("Caught keyboard interrupt, exiting")
    finally:
        for index, thread in enumerate(ClientThreads):
            thread.join()
        sock.close()
        sel.close()
        os.unlink(socket_path)

if __name__ == "__main__":
    main()

