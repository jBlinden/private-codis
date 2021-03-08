#!/usr/bin/python3

import random
import subprocess
import argparse
import os

steps_per_round_fsm1 = 2
steps_per_round_fsm2 = 2

def format_read_var(var_name, line):
    l = line.split(" ")
    variable_name = l[0]
    equals = l[1]
    value = int(l[2])
    assert equals == "=", "incorrect config format"
    assert var_name == variable_name, "incorrect config format with variable " + var_name 
    return value

def format_read_arr(arr_name, line, C):
    l = line.split(" ")
    variable_name = l[0]
    equals = l[1]
    assert equals == "=", "incorrect config format"
    assert arr_name == variable_name, "incorrect config format with variable " + var_name 
    
    arr = []
    for i in range(C):
        arr.append(int(l[2+i]))

    return arr

def read_config_file(config_file):
    with open(config_file, "r") as f:
        C = format_read_var("C", f.readline())
        Bits = format_read_arr("Bits", f.readline(), C)
    return C, Bits

def generateEntry(C, Bits):
    return [random.randint(0, 2**bit - 1) for bit in Bits]

def format(entry):
    s = ", ".join(str(component) for component in entry)
    return s + "\n"

def GenerateTestCase(N, C, Bits, IN_DB, config_file, verbose = False):
    if verbose:
        print("Creating following test case:")
        print("N =", N)
        print("C =", C)
        print("Config file =", config_file)
        print("Bits per component", Bits)
        print("In DB =", IN_DB)

    
    subprocess.run(["./tests/GenerateOTs", str(N), config_file])
    
    query = ""
    entry = random.randint(0, N - 1) if IN_DB else -1
    
    if not IN_DB:
        query = generateEntry(C, Bits)
        

    if verbose:
        print("Generating database")

    with open('./data/server/db.txt', 'w') as db_file:
        for i in range(N):
            if i == entry:
                query = generateEntry(C, Bits)
                db_file.write(format(query))
            else:
                db_file.write(format(generateEntry(C, Bits)))

    if verbose:
        print("Creating Query")

    if IN_DB:
        print("Query is in database at location: %d" % entry)

    with open('./data/client/query.txt', 'w') as query_file:
        query_file.write(format(query))
    
    return entry