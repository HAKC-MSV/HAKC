import sys
import kuzu
import pandas as pd

conn = None 
# https://docs.kuzudb.com/cypher/query-clauses/call/
def main(kuzuin, outfolder):
    global conn 
    # open db 
    print("Start main")
    max_num_threads = 1
    database = kuzu.Database(kuzuin, read_only=True, max_num_threads=max_num_threads)
    print("Got database")
    conn = kuzu.Connection(database) # main connection 
    print("Got connection")
    # MATCH (n) RETURN distinct labels(n)
    response = conn.execute(f"CALL SHOW_TABLES() return *;")
    print(response.get_as_df())
    # column names: ['id', 'name', 'type', 'database name', 'comment']
    # loop through all table names
    for table in response.get_as_df()["name"]:
        print(table)
        # resp = conn.execute(f"MATCH (head:{table}) return *")
        resp = conn.execute(f"CALL TABLE_INFO('{table}') RETURN *;")
        print(resp.get_as_df())
        if(table.startswith("HAKC")):
            resp = conn.execute(f"MATCH (head:{table}) return *").get_as_networkx()
            print(resp.nodes)
            fname = outfolder + table + ".csv"
            conn.execute(f"COPY (MATCH (head:{table}) return *) TO '{fname}' (header=true);")
    
    # response = conn.execute(f"CALL SHOW_ATTACHED_DATABASES() return *;")
    # response = conn.execute(f"CALL SHOW_FUNCTIONS() return *;")
    # conn.execute("MATCH (head:HAKCSymbol) return *").get_as_networkx().nodes
    # conn.execute("MATCH (head:HAKCSymbol) return *").get_as_networkx()._node["HAKCSymbol_6588985453060915670"]
    # conn.execute("MATCH (head:HAKCSymbol) return *").get_as_networkx()._node["HAKCSymbol_5954552900367763080"]
    # conn.execute("MATCH (head:HAKCDivision) return *").get_as_networkx().nodes
    # print(response.get_as_df())
    # CALL TABLE_INFO('HAKCCompilationUnit') RETURN *;"
    # conn.execute("MATCH (head:HAKCCompartment) return *").get_as_networkx().nodes
    # response = conn.execute(f"COPY (CALL SHOW_TABLES() return *) TO '{csvout}' (header=true);")
    # response = conn.execute(f"COPY (CALL SHOW_FUNCTIONS() RETURN *) TO '{csvout}' (header=true);")
    # print(response.get_as_df())
    print("Executed")

# python3 -i kuzu_export_to_csv.py /home/al32163/explorer/hakc-db /home/al32163/explorer/hakc-db/
if __name__ == "__main__":
    print(f"{sys.argv}")
    if len(sys.argv) == 3:
        kuzuin = sys.argv[1]
        outfolder= sys.argv[2]
        main(kuzuin, outfolder)
    else:
        print("Needs kuzuin, then csvout folder")