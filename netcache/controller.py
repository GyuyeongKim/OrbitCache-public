#!/usr/bin/env python3
import sys
import os
sys.path.append(os.path.expandvars('$SDE/install/lib/python3.8/site-packages/tofino/'))
sys.path.append(os.path.expandvars('$SDE/install/lib/python3.8/site-packages/tofino/bfrt_grpc'))
sys.path.append(os.path.expandvars('$SDE/install/lib/python3.8/site-packages/'))
sys.path.append(os.path.expandvars('$SDE/install/lib/python3.8/site-packages/tofinopd/'))
sys.path.append(os.path.expandvars('$SDE/install/lib/python3.8/site-packages/tofino_pd_api/'))
sys.path.append(os.path.expandvars('$SDE/install/lib/python3.8/site-packages/p4testutils'))
import time
import datetime
import grpc
import bfrt_grpc.bfruntime_pb2_grpc as bfruntime_pb2_grpc
import bfrt_grpc.bfruntime_pb2 as bfruntime_pb2

import bfrt_grpc.client as gc
import port_mgr_pd_rpc as mr
from time import sleep
import socket, struct
import binascii

# Cluster configuration
NUM_SRV_CTRL = 10
KEY_SIZE = 16
# Other stuff
FNV_16_PRIME = 0x0101
FNV_16_INIT = 0x811c
CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
CHARSET_SIZE = len(CHARSET)
CPU_DEVPORT = 64
def complex_transform(number):

    transformed = number
    for _ in range(5):
        transformed = ((transformed << 3) - transformed + 7) & 0xFFFFFFFFFFFFFFFF
    return transformed

def tostring_key(number):
    key_chars = [''] * (KEY_SIZE - 1)
    transformed_number = complex_transform(number)

    for i in range(KEY_SIZE - 2, -1, -1):
        index = transformed_number % CHARSET_SIZE
        key_chars[i] = CHARSET[index]
        transformed_number //= CHARSET_SIZE

    return ''.join(key_chars)

def hash64_str2(s: str) -> int:
    value = 0xABCDEF
    for char in s:

        value = (value * 33 + ord(char) + 0x45) & 0xFFFFFFFFFFFFFFFF


    value = (((value >> 30) ^ value) * 0x123456789ABCDEF1) & 0xFFFFFFFFFFFFFFFF
    value = (((value >> 33) ^ value) * 0xFEDCBA9876543210) & 0xFFFFFFFFFFFFFFFF
    value = (value >> 35) ^ value

    return value & 0xFFFFFFFFFFFFFFFF

def hash64_str(s: str) -> int:
    value = 0
    for char in s:
        value = (value * 31 + ord(char)) & 0xFFFFFFFFFFFFFFFF


    value = (((value >> 32) ^ value) * 0xc4ceb9fe1a85ec53) & 0xFFFFFFFFFFFFFFFF
    value = (((value >> 32) ^ value) * 0xc4ceb9fe1a85ec53) & 0xFFFFFFFFFFFFFFFF
    value = (value >> 32) ^ value
    return value & 0xFFFFFFFFFFFFFFFF

def fnv1_16_hash(value):
    hash_ = FNV_16_INIT
    for i in range(4):
        hash_ *= FNV_16_PRIME
        hash_ ^= (value & (0xFF << (i * 8))) >> (i * 8)
    return hash_ & 0xFFFF  # Ensure it stays a 16-bit value

def table_add(target, table, keys, action_name, action_data=[]):
    keys = [table.make_key([gc.KeyTuple(*f) for f in keys])]
    datas = [table.make_data([gc.DataTuple(*p) for p in action_data], action_name)]
    table.entry_add(target, keys, datas)

def table_mod(target, table, keys, action_name, action_data=[]):
    keys = [table.make_key([gc.KeyTuple(*f) for f in keys])]
    datas = [table.make_data([gc.DataTuple(*p) for p in action_data], action_name)]
    table.entry_mod(target, keys, datas)

def table_del(target, table, keys):
    table.entry_del(target, keys)

def table_clear(target, table):
    keys = []
    for data, key in table.entry_get(target):
        if key is not None:
            keys.append(key)
    if keys:
        table.entry_del(target, keys)

if __name__ == "__main__":


    if len(sys.argv) != 3:
        NUM_HOTKEY = 10000
        LRATIO = 88
    else:
        try:
            NUM_HOTKEY = int(sys.argv[1])
            LRATIO = int(sys.argv[2])
            if LRATIO < 0 or LRATIO > 100:
                raise ValueError("LRATIO must be between 0 and 100.")
        except ValueError as e:
            print(f"Error: {e}")
            print("Both NUM_HOTKEY and LRATIO must be integers.")
            sys.exit(1)



    INPUT_FILE = f'gen_keys_{NUM_HOTKEY}_{LRATIO}.txt'

    try:
        grpc_addr = "localhost:50052"
        client_id = 0
        device_id = 0
        pipe_id = 0xFFFF
        is_master = True
        #client = gc.ClientInterface(grpc_addr, client_id, device_id,is_master)
        client = gc.ClientInterface(grpc_addr, client_id, device_id)
        target = gc.Target(device_id, pipe_id)
        client.bind_pipeline_config("netcache")

        ip_list = [
            0x0A000165, # 10.0.1.101
            0x0A000166, # 10.0.1.102
            0x0A000167, # 10.0.1.103
            0x0A000168, # 10.0.1.104
            0x0A000169, # 10.0.1.105
            0x0A00016A, # 10.0.1.106
            0x0A00016B, # 10.0.1.107
            0x0A00016C, # 10.0.1.108
            0x0A000179, # 10.0.1.121
            #0x0A00016D, # 10.0.1.109
            #0x0A00016E, # 10.0.1.110
            0x0A00016F, # 10.0.1.111
            #0x0A000170, # 10.0.1.112
            ]
        port_list = [
            48,
            52,
            0,
            4,
            16,
            20,
            32,
            36,
            40,
            #388,
            CPU_DEVPORT
        ]
        pipe_list = [
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            #0,
            #0,
            0
        ] #
        mac_list = [
            0x0c42a12f12e6, # 101
            0x0c42a12f11c6, # 102
            0x1070fd1cd4b8, # 103
            0x1070fd0dd54c, # 104
            0x1070fd1ccab8, # 105
            0x1070fd1ccab4, # 106
            0x1070fd1cc2c8, # 107
            0x1070fd1cd40c, # 108
            0x1070fd1ccabc, # 121
            #0xb83fd2d9983e, # 109
            #0xb83fd2d9983f, # 110
            0x84c78f036d82, # 111
            #0x84c78f036d83
        ]

        read_cache_table = client.bfrt_info_get().table_get("pipe.SwitchIngress.read_cache_table")
        table_clear(target, read_cache_table)
        cache_state = client.bfrt_info_get().table_get("pipe.cache_state")
        num_cached_key = 0 #


        with open(INPUT_FILE, 'r') as f:
            sampled_keys = [int(line.strip()) for line in f]


        num_cached_key = 0

        #for i in range(NUM_HOTKEY):
        for i in sampled_keys:
            num_cached_key += 1
            key = tostring_key(i)
            table_add(target, read_cache_table, [("hdr.netcache.hkey_hi", hash64_str(key)), ("hdr.netcache.hkey_lo", hash64_str2(key))], "read_cache_hit_action", [("idx", i)])
            cache_state.entry_mod(
                target,
                [cache_state.make_key([gc.KeyTuple('$REGISTER_INDEX', i)])],
                [cache_state.make_data(
                    [gc.DataTuple('cache_state.f1', 1)])])

        print(num_cached_key)
        ipv4_exact = client.bfrt_info_get().table_get("pipe.SwitchIngress.ipv4_exact")
        table_clear(target, ipv4_exact)
        for i in range(NUM_SRV_CTRL):
            table_add(target, ipv4_exact,[("hdr.ipv4.dstAddr", ip_list[i])],"ipv4_forward",[("port",port_list[i])]) # 101

        #print port_list
        port_table = client.bfrt_info_get().table_get("$PORT")
        print("Done!")
    finally:
        client.tear_down_stream()
