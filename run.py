#!/usr/bin/python3

import util
from subprocess import Popen, PIPE, run, DEVNULL
import argparse

if __name__ == "__main__":

    N = 1000
    config = "./configs/US-Current.config"
    indb = 1
    port = 8000
    local = 1

    parser = argparse.ArgumentParser(description="Protocol Arguments")
    parser.add_argument("-N", type=int, dest="N", help="Database size")
    parser.add_argument("-config", type=str, dest="config", help="Database size")
    parser.add_argument("-in", type=int, dest="indb", help="(optional) 0 if query is not in the database, 1 if the query is in in the database. Defaults to 1")
    parser.add_argument("-p", type=int, dest="port", help="(optional) port that will be used for communication, defaults to 8000")
    parser.add_argument("-local", type=int, dest="local", help="(optional) 1 if test is performed locally, 0 if the test is performed in a network environment. Defaults to 1 (local test)")
    
    values = parser.parse_args()

    if values.N is not None:
        N = values.N
    if values.config is not None:
        config = values.config
    if values.indb is not None:
        indb = values.indb
    if values.port is not None:
        port = values.port
    if values.local is not None:
        local = values.local

    print("Collecting protocol parameters")
    C, Bits = util.read_config_file(config)
    
    print("Generating test case")
    util.GenerateTestCase(N, C, Bits, indb, config)

    if local == 1:
        print("Running local test")
        client = Popen(["./tests/ProtocolClient", "./data/info.txt", "./data/client/", "127.0.0.1", str(port)])
        server = Popen(["./tests/ProtocolServer", "./data/info.txt", "./data/server/", str(port)], stdout=PIPE, stderr=PIPE)

        client_stdout, client_stderr = client.communicate()
    


