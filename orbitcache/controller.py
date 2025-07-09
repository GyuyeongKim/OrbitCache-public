# controller.py Switch Control Plane for cache updates
#!/usr/bin/env python3
DYNAMIC = 0 # Enable dynamic workloads?
NUM_HOTKEY = 128 # How many keys will be cached?
NUM_SRV_WORKERS = 8 # Number of partitions per server
MAX_HOTKEY = 1024
MIN_HOTKEY = 0
KEY_SIZE = 16  # Unit: Byte. OrbitCache, original key
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
import heapq
import binascii
import threading
import random
# Cluster configuration
fetching_lock = threading.Lock()
fetching_tracking_lock = threading.Lock()
fetching_cnt = 0

NUM_SRV = 4 # Number of connected storage servers

NUM_SRV_CTRL = 15 # Total number of nodes (including switch control plane). This is for the whole cluster.
REPORT_PORT = 4321
CPU_DEVPORT = 64
WEIGHT_FOR_CACHED_KEY = 1
C_RECIRC_PRT_0 = 56 # 68 pipeline 0 // 56 (FP 34/0) when debugging
C_RECIRC_PRT_1 = 128 # 192 Pipeline 1 // Debugging 128 (FP 57/0)
C_RECIRC_PRT_2 = 316 # 324 Pipeline 2 // Debugging 316 (FP 63/0)
C_RECIRC_PRT_3 = 384 # 452 Pipeline 3 // Debugging 384 (FP 47/0)
C_RECIRC_PRT_CURRENT = C_RECIRC_PRT_0
MAX_QUEUE_SIZE_PER_KEY = 8
HOTKEY_REPORT_INTERVAL = 1
ORBITCACHE_BASE_PORT = 1000
# MACRO for fetching packet generation
OP_FETCH = 5
OP_F_REPLY = 6
SEQ = 0
HKEY_HI = 0x1234567890ABCDEF  # 64-bit hash key high
HKEY_LO = 0xFEDCBA0987654321  # 64-bit hash key low
CACHED = 1
FLAG = 0
LATENCY = 0
PAR_ID = 0

CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
CHARSET_SIZE = len(CHARSET)
available_indexes = set(range(MAX_HOTKEY))
fetching_keys = set()  #

def get_next_index():
    global available_indexes
    if available_indexes:
        next_index = min(available_indexes)
        available_indexes.remove(next_index)
        return next_index
    else:
        raise Exception("No available indexes")

def release_index(index):
    global available_indexes
    if index < MAX_HOTKEY:
        available_indexes.add(index)
    else:
        raise ValueError("Index out of allowed range")

def calculate_miss_rate(total_cache, total_overflow):
    """
    Calculate the miss rate as total_overflow / (total_cache + total_overflow).

    :param total_cache: The total cache hits.
    :param total_overflow: The total cache misses.
    :return: The miss rate rounded to two decimal places.
    """
    if total_cache + total_overflow == 0:
        return 0.0  # Avoid division by zero
    miss_rate = total_overflow / (total_cache + total_overflow)
    return round(miss_rate, 2)


class ReportHeader:
    def __init__(self, key_size):

        self.struct_format = f'{key_size}sI'
        self.struct = struct.Struct(self.struct_format)
    def unpack(self, buf):
        return self.struct.unpack(buf)

class OrbitCacheHeader:
    def __init__(self, op, seq, hkey_hi, hkey_lo, flag, cached, latency, par_id, org_key):
        self.op = op
        self.seq = seq
        self.hkey_hi = hkey_hi
        self.hkey_lo = hkey_lo
        self.flag = flag
        self.cached = cached
        self.latency = latency
        self.par_id = par_id
        self.org_key = org_key

    def pack(self):
        # Fixed structure format: 44 bytes excluding value
        struct_format = f"<B I Q Q B B I B {KEY_SIZE}s"
        packed_data = struct.pack(
            struct_format,
            self.op,
            self.seq,
            self.hkey_hi,
            self.hkey_lo,
            self.flag,
            self.cached,
            self.latency,
            self.par_id,
            self.org_key.encode()[:KEY_SIZE]  # Ensure org_key is exactly 16 bytes
        )
        return packed_data

    @classmethod
    def unpack(cls, data):
        # Unpack the fixed 44 bytes part of the structure
        struct_format = f"<B I Q Q B B I B {KEY_SIZE}s"
        unpacked_data = struct.unpack(struct_format, data[:struct.calcsize(struct_format)])

        # Ensure org_key is decoded and stripped of null characters properly
        org_key = unpacked_data[8].decode('utf-8', errors='ignore').rstrip('\x00')

        return cls(
            unpacked_data[0],  # op
            unpacked_data[1],  # seq
            unpacked_data[2],  # hkey_hi
            unpacked_data[3],  # hkey_lo
            unpacked_data[4],  # flag
            unpacked_data[5],  # cached
            unpacked_data[6],  # latency
            unpacked_data[7],  # par_id
            org_key            # org_key (decoded properly)
        )



def hex_to_stringIP(hex_ip):
    # Convert the hex IP to a 4-byte packed representation
    packed_ip = struct.pack('>I', hex_ip)
    # Convert the packed IP to a dotted string
    dotted_ip = socket.inet_ntoa(packed_ip)
    return dotted_ip

def print_heap(heap, num_elements):

    top_elements = heapq.nlargest(num_elements, heap)


    for popularity, key in top_elements:
        print(f"Key: {key}, Popularity: {popularity}")




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

def hash64_lo(s: str) -> int:
    value = 0xABCDEF
    for char in s:

        value = (value * 33 + ord(char) + 0x45) & 0xFFFFFFFFFFFFFFFF


    value = (((value >> 30) ^ value) * 0x123456789ABCDEF1) & 0xFFFFFFFFFFFFFFFF
    value = (((value >> 33) ^ value) * 0xFEDCBA9876543210) & 0xFFFFFFFFFFFFFFFF
    value = (value >> 35) ^ value

    return value & 0xFFFFFFFFFFFFFFFF

def hash64_hi(s: str) -> int:
    value = 0
    for char in s:
        value = (value * 31 + ord(char)) & 0xFFFFFFFFFFFFFFFF


    value = (((value >> 32) ^ value) * 0xc4ceb9fe1a85ec53) & 0xFFFFFFFFFFFFFFFF
    value = (((value >> 32) ^ value) * 0xc4ceb9fe1a85ec53) & 0xFFFFFFFFFFFFFFFF
    value = (value >> 32) ^ value

    return value & 0xFFFFFFFFFFFFFFFF

FNV_16_PRIME = 0x0101
FNV_16_INIT = 0x811c
def fnv1_16_hash(value):
    hash_ = FNV_16_INIT
    for i in range(4):
        hash_ *= FNV_16_PRIME
        hash_ ^= (value & (0xFF << (i * 8))) >> (i * 8)
    return hash_ & 0xFFFF  # Ensure it stays a 16-bit value

def fnv1_16_str(value):
    hash = FNV_16_INIT
    for char in value:
        hash *= FNV_16_PRIME
        hash ^= ord(char)  # 문자를 정수로 변환
        hash &= 0xFFFF  # 16비트 해시 유지

    return hash
def reg_get(target, table, index, pipe, field_name):
    """
    Get a value from a specified register table.

    :param target: Target device.
    :param table: Register table object.
    :param index: Index of the register to read.
    :param pipe: The pipeline number.
    :param field_name: The name of the field to read from the register.
    :return: The value of the specified field at the given index and pipeline, or None if the value is not found.
    """
    keys = [table.make_key([gc.KeyTuple('$REGISTER_INDEX', index)])]
    for data, _ in table.entry_get(target, keys):
        data_dict = data.to_dict()
        if field_name in data_dict:
            return data_dict[field_name][pipe]
    return None


def reg_mod(target, table, index, data_tuples):
    """
    Modify a register entry in a table.
    :param target: Target device.
    :param table: Register table object.
    :param index: Index of the register to modify.
    :param data_tuples: List of tuples, where each tuple contains a data field name and its value.

    사용 예:
    SRV2 = client.bfrt_info_get().table_get("pipe.srv2")
    for i in range(NUM_SRV):
        reg_mod(target, SRV2, i, [('srv2.f1', 0)])
    """
    keys = [table.make_key([gc.KeyTuple('$REGISTER_INDEX', index)])]
    datas = [table.make_data([gc.DataTuple(field, value) for field, value in data_tuples])]
    table.entry_mod(target, keys, datas)

def reg_del(target, table, index, data_field_names):
    """
    Delete (reset to zero) a register entry in a table.
    :param target: Target device.
    :param table: Register table object.
    :param index: Index of the register to delete.
    :param data_field_names: List of data field names in the register to reset.

    usage:
    SRV2 = client.bfrt_info_get().table_get("pipe.srv2")
	index = 0  # 예시 인덱스
	data_field_names = ['srv2.f1']  # 예시 데이터 필드 이름
	reg_del(target, SRV2, index, data_field_names)
    """
    data_tuples = [(field, 0) for field in data_field_names]
    reg_mod(target, table, index, data_tuples)


def table_get(target, table, keys):
    """
    Get entries from the table.
    :param target: Target device.
    :param table: Table object from which to get the entries.
    :param keys: List of keys to get from the table. Each key is a list of gc.KeyTuple.
    :return: A list of tuples, each containing the data and key for an entry.
    """
    keys = [table.make_key([gc.KeyTuple(*f) for f in keys])]
    entries = []
    for data, key in table.entry_get(target, keys):
        key_fields = key.to_dict()
        data_fields = data.to_dict()
        entries.append((data_fields, key_fields))
    return entries


def table_add(target, table, keys, action_name, action_data=[]):
	keys = [table.make_key([gc.KeyTuple(*f)   for f in keys])]
	datas = [table.make_data([gc.DataTuple(*p) for p in action_data],
								  action_name)]
	table.entry_add(target, keys, datas)

def table_mod(target, table, keys, action_name, action_data=[]):
	keys = [table.make_key([gc.KeyTuple(*f)   for f in keys])]
	datas = [table.make_data([gc.DataTuple(*p) for p in action_data],
								  action_name)]
	table.entry_mod(target, keys, datas)

def table_del(target, table, keys):
	table.entry_del(target, keys)


def get_port_status(target, table, keys):
	keys = [table.make_key([gc.KeyTuple(*f)   for f in keys])]
	for data,key in table.entry_get(target,keys):
		key_fields = key.to_dict()
		data_fields = data.to_dict()
		return data_fields[b'$PORT_UP']

def table_clear(target, table):
	keys = []
	for data,key in table.entry_get(target):
		if key is not None:
			keys.append(key)
	if keys:
		table.entry_del(target, keys)


def to_network_order_64bit(value):

    high = (value >> 32) & 0xFFFFFFFF
    low = value & 0xFFFFFFFF


    high_net = socket.htonl(high)
    low_net = socket.htonl(low)


    return (high_net << 32) | low_net

def send_udp_message(host, port, message, max_retries):
    global fetching_cnt, fetching_keys  # fetching_keys를 추가로 사용
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    timeout_interval = 1
    max_timeout_interval = 1
    packed_message = message.pack()
    retries = 0
    try:
        while retries < max_retries:
            try:

                sock.sendto(packed_message, (host, port))
                sock.settimeout(timeout_interval)


                received_data, addr = sock.recvfrom(4096)  # 최대 4096 바이트 수신


                received_message = OrbitCacheHeader.unpack(received_data)


                sent_key = message.org_key
                received_key = received_message.org_key
                # print(f"Sent org_key: {sent_key}, Received org_key: {received_key}")


                if sent_key != received_key:
                    print(f"org_key mismatch! Sent: {sent_key}, Received: {received_key}")


                with fetching_tracking_lock:
                    fetching_cnt += 1
                    fetching_keys.add(message.org_key)
                return True

            except socket.timeout:
                retries += 1
                timeout_interval = min(timeout_interval * 2, max_timeout_interval)
                #print(f"Timeout, {retries}th retransmit with timeout {timeout_interval}s for key: {message.org_key}")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        sock.close()


def handle_client(conn):
    with conn:
        global active_conn
        try:
            while True:

                fixed_header_size = KEY_SIZE + 4
                data_buffer = b''
                while len(data_buffer) < fixed_header_size:
                    more_data = conn.recv(fixed_header_size - len(data_buffer))
                    if not more_data:
                        return
                    data_buffer += more_data

                if len(data_buffer) < fixed_header_size:
                    return

                # unpack the fixed part of the header
                key, popularity = struct.unpack(f'{KEY_SIZE}sI', data_buffer[:fixed_header_size])
                key = key.decode('utf-8').rstrip('\x00')

                #print(len(value_data),len(value))

                unCachedKeyPopularity[key] = popularity
                #print(key,popularity)

        except ConnectionResetError:
            active_conn = active_conn - 1
            print(f"Connection disconnected We have {active_conn} connections.")

def accept_connections(sock):
    global active_conn
    while True:
        try:
            conn, addr = sock.accept()
            active_conn = active_conn + 1
            print(f"Connection Established. We have {active_conn} connections.")
            client_thread = threading.Thread(target=handle_client, args=(conn,))
            client_thread.start()
        except socket.error as e:
            print(f"Accept failed: {e}")

try:
	grpc_addr = "localhost:50052"
	client_id = 0
	device_id = 0
	pipe_id = 0xFFFF
	client = gc.ClientInterface(grpc_addr, client_id, device_id)
	target = gc.Target(device_id, pipe_id)
	client.bind_pipeline_config("orbitcache")

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
        0x0A00017A, # 10.0.1.122
        0x0A00017B, # 10.0.1.123
        0x0A000183, # 10.0.1.131
        0x0A000184, # 10.0.1.132
        0x0A000185, # 10.0.1.133
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
        40, # 10.0.1.121 / 46
        24, # 10.0.1.122 / 43
        28, # 10.0.1.123 / 44
        12, # 10.0.1.131 / 41
        8, # 10.0.1.132 / 42
        44, # 10.0.1.133 / 45
        #388, # 듀얼포트 중 포트 하나나
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
        0,
        0,
		0,
        0,
        0,
        0
	]
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
        0x1070fd31e86c, # 122
        0x1070fd0dd4bc, # 123
        0x1070fde55ac0, # 131
        0x1070fde7bdc4, # 132
        0x1070fde55b20, # 133
        #0xb83fd2d9983e, # 109
        #0xb83fd2d9983f, # 110
		0x84c78f036d82, # 111
        #0x84c78f036d83
	]

	dst_ip = [None] * 4
	#dst_ip[5] = ip_list[2]  # ip_list의 5번째 값 (0x0A000169)에 접근
    # The reason why the IP list is put in reverse order here is because if you look at header.h, it was originally arranged in reverse order, so it is convenient when adjusting the number of servers.
	#dst_ip[4] = ip_list[3]
	dst_ip[0] = ip_list[7]
	dst_ip[1] = ip_list[6]
	dst_ip[2] = ip_list[5]
	dst_ip[3] = ip_list[4]

	ipv4_exact = client.bfrt_info_get().table_get("pipe.SwitchIngress.ipv4_exact")
	table_clear(target, ipv4_exact)
	for i in range(NUM_SRV_CTRL):
		table_add(target, ipv4_exact,[("hdr.ipv4.dstAddr", ip_list[i])],"ipv4_forward",[("port",port_list[i])]) # 101

	read_CacheRecirculationPort_table = client.bfrt_info_get().table_get("pipe.SwitchIngress.read_CacheRecirculationPort_table")
	table_clear(target, read_CacheRecirculationPort_table)
	dps_0 = [48,52,0,4,16,20,32,36,60,12,8,28,24,44,40,56,68] # dps => Device PortS
	dps_0.append(CPU_DEVPORT) # Controller port added
	for dp in dps_0:
		table_add(target, read_CacheRecirculationPort_table, [("ig_intr_md.ingress_port", dp)], "read_CacheRecirculationPort_action", [("port", C_RECIRC_PRT_0),("pnum", 0)])
	#print(len(dps_0))

	#CPU는 제외됨.. recirculation에선.

	get_Crecirc_id_table = client.bfrt_info_get().table_get("pipe.SwitchIngress.get_Crecirc_id_table")
	get_Crecirc_id_table2 = client.bfrt_info_get().table_get("pipe.SwitchIngress.get_Crecirc_id_table2")
	table_clear(target, get_Crecirc_id_table)
	table_clear(target, get_Crecirc_id_table2)
	recirc_id_idx = 1
	for i in range(NUM_SRV_CTRL):
		table_add(target, get_Crecirc_id_table,[("hdr.ipv4.dstAddr", ip_list[i]),("ig_md.pipeNum", 0)],"get_Crecirc_id_action",[("recirc_id",recirc_id_idx)]) # 101
		table_add(target, get_Crecirc_id_table2,[("hdr.ipv4.dstAddr", ip_list[i]),("ig_md.pipeNum", 0)],"get_Crecirc_id_action",[("recirc_id",recirc_id_idx)]) # 101
		recirc_id_idx+=1


	update_dstMAC_table = client.bfrt_info_get().table_get("pipe.SwitchIngress.update_dstMAC_table")
	table_clear(target, update_dstMAC_table)
	for i in range(NUM_SRV_CTRL):
		table_add(target, update_dstMAC_table,[("hdr.ipv4.dstAddr", ip_list[i])],"update_dstMAC_action",[("dst_mac",mac_list[i])])

    #print port_list
	port_table = client.bfrt_info_get().table_get("$PORT")


	node_table = client.bfrt_info_get().table_get("$pre.node")
	table_clear(target, node_table)
	for i in range(1,NUM_SRV_CTRL+1):
        # Specifically, to send a write reply to the client while fetching and validating a new cache packet to all pipelines including itself.
		node_table.entry_add(
			target,
			[node_table.make_key([
				gc.KeyTuple('$MULTICAST_NODE_ID', i)])],
			[node_table.make_data([
				gc.DataTuple('$MULTICAST_RID', 1),
				gc.DataTuple('$MULTICAST_LAG_ID', int_arr_val=[]),
				#gc.DataTuple('$DEV_PORT', int_arr_val=[port_list[i-1]])
				gc.DataTuple('$DEV_PORT', int_arr_val=[port_list[i-1],C_RECIRC_PRT_CURRENT])
                        ])] #
		)

	mgid_table = client.bfrt_info_get().table_get("$pre.mgid")
	table_clear(target, mgid_table)
	for i in range(1,NUM_SRV_CTRL+1):
		mgid_table.entry_add(
		target,
		[mgid_table.make_key([
			gc.KeyTuple('$MGID', i)])],
		[mgid_table.make_data([
			gc.DataTuple('$MULTICAST_NODE_ID',  int_arr_val=[i]),
			gc.DataTuple('$MULTICAST_NODE_L1_XID_VALID', bool_arr_val=[0]),
			gc.DataTuple('$MULTICAST_NODE_L1_XID', int_arr_val=[0])])]
	)

	cache_counter_table = client.bfrt_info_get().table_get("pipe.cache_counter")
	total_cache_table = client.bfrt_info_get().table_get("pipe.total_cache")
	total_overflow_table = client.bfrt_info_get().table_get("pipe.miss_counter")


	# Initialize an array to store the generated keys
	cached_key = []
	cache_idx = {}
	# A dictionary to store the mapping between keys and indices.
	key_index_mapping = {}

	# Index to be used when a new hotkey is added.
	next_index = 0
	active_conn = 0
	cachedKeyPopularity = {}
	total_overflow = 0
	total_cache_hit = 0
	hotkey_list = []
	previous_hotkey_list = []
	unCachedKeyPopularity = {}
	# Creating and binding a TCP socket

	cacheLookup_table = client.bfrt_info_get().table_get("pipe.SwitchIngress.cacheLookup_table")
	table_clear(target, cacheLookup_table)

	# get the register table
	cacheState_table = client.bfrt_info_get().table_get("pipe.cacheState")
	ptr_rear_table = client.bfrt_info_get().table_get("pipe.ptr_rear")
	ptr_front_table = client.bfrt_info_get().table_get("pipe.ptr_front")
	qlen_table = client.bfrt_info_get().table_get("pipe.qlen")

	clientIP_table = client.bfrt_info_get().table_get("pipe.clientIP")
	UDPPort_table = client.bfrt_info_get().table_get("pipe.UDPPort")
	seq_table = client.bfrt_info_get().table_get("pipe.seq")
	latency_table = client.bfrt_info_get().table_get("pipe.latency")

	for i in range(NUM_HOTKEY):


		# qlen_table
		reg_mod(target, qlen_table, i, [('qlen.f1', 0)])

		# ptr_rear_table
		reg_mod(target, ptr_rear_table, i, [('ptr_rear.f1', 0)])

		# ptr_front_table
		reg_mod(target, ptr_front_table, i, [('ptr_front.f1', 0)])

		# Example of modifying values ​​in cacheState_table
		reg_mod(target, cacheState_table, i, [('cacheState.f1', 0)])
	for i in range(MAX_QUEUE_SIZE_PER_KEY*NUM_HOTKEY):
		reg_mod(target, clientIP_table, i, [('clientIP.f1', 0)])
		reg_mod(target, UDPPort_table, i, [('UDPPort.f1', 0)])
		reg_mod(target, seq_table, i, [('seq.f1', 0)])
		reg_mod(target, latency_table, i, [('latency.f1', 0)])

	if DYNAMIC == 0:
		for i in range(NUM_HOTKEY):
			key = tostring_key(i)
			cached_key.append(key) # Tracks the initial list of Cached keys.
			cache_idx[key] = get_next_index()
			print(i,key, hash64_hi(key),hash64_lo(key),cache_idx[key])

			table_add(target, cacheLookup_table,[("hdr.orbitcache.hkey_hi", hash64_hi(key)),("hdr.orbitcache.hkey_lo", hash64_lo(key))],"read_cache_hit_action",[("idx",cache_idx[key])]) # 101
		fetching_cnt += 1
		print(f"{NUM_HOTKEY} Keys are inserted to the lookup table.")
	else:
		sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		sock.bind(('', REPORT_PORT))
		sock.listen()
		# Start a thread to receive connections
		accept_thread = threading.Thread(target=accept_connections, args=(sock,))
		accept_thread.start()
		#sock.settimeout(0.1)  # Set a short timeout for non-blocking
		last_execution_time = time.time()
		total_cache = 0
		total_overflow = 0
		exp_time = 1
		while True:
			if time.time() - last_execution_time >= HOTKEY_REPORT_INTERVAL: # Every second...
				# Read the popularity of the currently cached keys and put it in cachedKeyPopularity, then initialize the counter.
				for _, key in previous_hotkey_list:
					cachedKeyPopularity[key] = 0  # 초기화
					keyPopularity = reg_get(target, cache_counter_table, cache_idx[key], 0,'cache_counter.f1')
					cachedKeyPopularity[key] = keyPopularity
					reg_mod(target, cache_counter_table, int(cache_idx[key]), [('cache_counter.f1', 0)])

				# Read the cache hit counter.
				total_cache_local = reg_get(target, total_cache_table, 0, 0,'total_cache.f1')
				total_cache = total_cache_local
				reg_mod(target, total_cache_table, 0, [('total_cache.f1', 0)])

				# Read the overflow counter.
				total_overflow_local = reg_get(target, total_overflow_table, 0, 0,'miss_counter.f1')
				total_overflow = total_overflow_local
				reg_mod(target, total_overflow_table, 0, [('miss_counter.f1', 0)])

				# Prints statistics on caches and misses.
				miss_rate = calculate_miss_rate(total_cache, total_overflow)
				if DYNAMIC == 1:
					#print(total_cache, total_overflow, miss_rate)
					print(exp_time, miss_rate*100)
					exp_time+=1
				total_cache = 0 # Since it was printed, I'll initialize it
				total_overflow = 0 # Since it was printed, I'll initialize it


				# 1. Read the popularity of the cached keys cached in the switch and add them to the hotkey list.

				# hotkey_list update logic
				for key, popularity in cachedKeyPopularity.items(): # Now put the list of items that are already cached into the heap.

					if len(hotkey_list) < NUM_HOTKEY: # < The reason is, the moment this is added at the end, it reaches NUM_HOTKEy.
						# If the heap size is less than NUM_HOTKEY, add the key to the heap.
						heapq.heappush(hotkey_list, (popularity, key))

					# Since this is the first step to construct a heap, it will never be rejected or exceed NUM_HOTKEY.
                    # hotkey_list is also initialized and reconfigured every second.
					elif len(hotkey_list) > NUM_HOTKEY:
						print("This should not happen!") #


				# 2 . Now we are going to add the Uncached keys reported from the server to the heap by considering their key and popularity.
				unCachedKeyPopularity_copy =  unCachedKeyPopularity.copy()

				for key, popularity in unCachedKeyPopularity_copy.items():
					if key not in [k for _, k in previous_hotkey_list]:
						if len(hotkey_list) < NUM_HOTKEY: # have free slots. just put in the heap
							heapq.heappush(hotkey_list, (popularity, key))  # put in hotkey_list
							#print(hotkey_list)
							cache_idx[key] = get_next_index()  # new cahce idx
							#print(cache_idx[key])

							# put in the Cache lookup table
							key_hi = hash64_hi(key)
							key_lo = hash64_lo(key)
							table_add(target, cacheLookup_table,[("hdr.orbitcache.hkey_hi", key_hi),("hdr.orbitcache.hkey_lo", key_lo)],"read_cache_hit_action",[("idx",cache_idx[key])])

							# fetch the item
							DSTADDR = fnv1_16_str(key)%NUM_SRV
							PAR_ID = (key_hi + 0x12345678)% NUM_SRV_WORKERS
							orbitcache_header = OrbitCacheHeader(OP_FETCH, 0, key_hi, key_lo, 0, 0, 0, PAR_ID, key)
							PORT_NUM = ORBITCACHE_BASE_PORT+PAR_ID
							packed_header = orbitcache_header.pack()
							host = hex_to_stringIP(dst_ip[DSTADDR])
							port = PORT_NUM
							message = orbitcache_header
							max_retries = 10
							thread = threading.Thread(target=send_udp_message, args=(host, port, message, max_retries))
							thread.start()
						else: # no vacancy, popularity check
							if popularity > hotkey_list[0][0]: # uncached key's popularity is higher
								# evict existing Key
								_, evicted_key = heapq.heappop(hotkey_list)
								temp_idx = cache_idx[evicted_key]
								release_index(cache_idx[evicted_key]) # return cache idx

								# remove from Cache lookup table
								key_hi = hash64_hi(evicted_key)
								key_lo = hash64_lo(evicted_key)
								key_tuples = [gc.KeyTuple("hdr.orbitcache.hkey_hi", key_hi), gc.KeyTuple("hdr.orbitcache.hkey_lo", key_lo)]
								keys = [cacheLookup_table.make_key(key_tuples)]
								#print(evicted_key)
								table_del(target, cacheLookup_table, keys)

								# remove from Fetching list
								with fetching_tracking_lock:
									if evicted_key in fetching_keys:
										fetching_keys.remove(evicted_key)

								# put new cahced key into the hotkey list
								heapq.heappush(hotkey_list, (popularity, key))
								cache_idx[key] = temp_idx
								#print(temp_idx,cache_idx[key])
								# put in the Cache lookup table
								key_hi = hash64_hi(key)
								key_lo = hash64_lo(key)
								#print(key)
								table_add(target, cacheLookup_table,[("hdr.orbitcache.hkey_hi", key_hi),("hdr.orbitcache.hkey_lo", key_lo)],"read_cache_hit_action",[("idx",cache_idx[key])])

								# fetching
								DSTADDR = fnv1_16_str(key)%NUM_SRV
								PAR_ID = (key_hi + 0x12345678)% NUM_SRV_WORKERS
								orbitcache_header = OrbitCacheHeader(OP_FETCH, 0, key_hi, key_lo, 0, 0, 0, PAR_ID, key)
								PORT_NUM = ORBITCACHE_BASE_PORT+PAR_ID
								packed_header = orbitcache_header.pack()
								host = hex_to_stringIP(dst_ip[DSTADDR])
								port = PORT_NUM
								message = orbitcache_header
								max_retries = 5
								thread = threading.Thread(target=send_udp_message, args=(host, port, message, max_retries))
								thread.start()

				sorted_hotkey_list = sorted(hotkey_list, key=lambda x: x[0], reverse=False)

				if DYNAMIC == 0:
					# Print the sorted hotkeys
					print("***Current hot key list with Fetching Status***")
					# print hot keys
					for pop, key in sorted_hotkey_list:
						fetching_status = "1" if key in fetching_keys else "0"
						print(f"Key: {key} Load: {pop} Hi {hash64_hi(key):x} Lo {hash64_lo(key):x} Fetching: {fetching_status}")
				#print(len(fetching_keys))
				# fetching_keys에 있는 원소들을 출력
				#fetching_keys_copy = fetching_keys.copy()  # fetching_keys의 복사본 생성
				#print(f"Fetching keys ({len(fetching_keys_copy)}):")
				#for fetching_key in fetching_keys_copy:
					#print(f"Fetching key: {fetching_key}")
				# unCachedKeyPopularity 초기화
				# print(fetching_cnt)
				temp = {}
				previous_hotkey_list = hotkey_list.copy()
				hotkey_list = []
				unCachedKeyPopularity = {}
				cachedKeyPopularity = {}
				last_execution_time = time.time()


except KeyboardInterrupt:
    # This block will execute when Ctrl+C is pressed.
    print("Program interrupted by user. Closing socket.")
    sock.close()
finally:
    # This block will execute regardless of how the try block exits.
    # Ensure resources are released properly.

	if 'client' in locals() or 'client' in globals():
		client.tear_down_stream()
