# OrbitCache

Artifact for evaluating OrbitCache, as described in the paper "Pushing the Limits of In-Network Caching for Key-Value Stores" (USENIX NSDI 2025).

## Contents

This repository contains the following code segments:

1. Switch data plane code
2. Switch control plane code
3. Client and server applications

## Hardware Dependencies

- At least 2 nodes (1 client and 1 server) are required. More nodes are recommended.
- Nodes should be equipped with an Nvidia ConnectX-5 NIC or similar NIC supporting Nvidia VMA for kernel-bypass networking. Experiments can still be run without VMA-capable NICs, but this may result in increased latency and decreased throughput.
- A programmable switch with Intel Tofino1 ASIC is needed.

**Tested on:**
- 8 nodes (4 clients and 4 servers) with single-port Nvidia 100GbE MCX515A-CCAT ConnectX-5 NIC
- APS BF6064XT switch with Intel Tofino1 ASIC

## Software Dependencies

**Clients and servers:**
- Ubuntu 24.04.2 LTS with Linux kernel 6.8
- Mellanox OFED NIC drivers 25.01-0.6.0
- gcc 13.3.0
- VMA libvma 9.8.40-1
- [TommyDS](https://www.tommyds.it/)

**Switch:**
- Ubuntu 20.04 LTS with Linux kernel 5.4
- Python 3.8.10
- Intel P4 Studio SDE 9.7.0 and BSP 9.7.0

## Installation

### Client/Server-side

1. Place `client.c`, `server.c`, `preload.c`, `header.h`, and `Makefile` in the home directory (e.g., `/home/orbitcache`).
2. Place `tommyds` in the home directory as well.
3. Configure cluster-related details in `header.h`, such as IP and MAC addresses. Each node should have a linearly-increasing IP address (e.g., `10.0.1.101` for node1, `10.0.1.102` for node2). The server program automatically assigns the server ID based on the last digit of the IP address.
   - Carefully check Lines 36–77 in `header.h` and configure them for your cluster.
4. Compile using `make`. This builds `client.out`, `server.out`, and `preload.out`.

### Switch-side

1. Place `controller.py` and `orbitcache.p4` in the SDE directory.
2. Read the code and configure cluster-related information in `controller.py` (IP/MAC addresses, port information). This requires domain knowledge of Intel Tofino and P4 programming.
3. Compile `orbitcache.p4` using the P4 compiler (e.g., `p4build.sh` provided by Intel). Manual compilation:
   ```bash
   cmake ${SDE}/p4studio \
     -DCMAKE_INSTALL_PREFIX=${SDE_INSTALL} \
     -DCMAKE_MODULE_PATH=${SDE}/cmake \
     -DP4_NAME=orbitcache \
     -DP4_PATH=${SDE}/orbitcache.p4
   make
   make install
   ```
   - `${SDE}` and `${SDE_INSTALL}` are paths to the SDE (e.g., `SDE=/home/admin/bf-sde-9.7.0`, `SDE_INSTALL=/home/admin/bf-sde-9.7.0/install`).

## Experiment Workflow

### Switch-side

1. Open three terminals for the switch control plane: (1) starting the switch program, (2) port configuration, (3) rule configuration by controller.

2. **Terminal 1:** Run the OrbitCache program:
   ```bash
   run_switchd.sh -p orbitcache
   ```
   Expected output:
   ```
   Using SDE /home/tofino/bf-sde-9.7.0
   Using SDE_INSTALL /home/tofino/bf-sde-9.7.0/install
   Setting up DMA Memory Pool
   Using TARGET_CONFIG_FILE /home/tofino/bf-sde-9.7.0/install/share/p4/targets/tofino/orbitcache.conf
   ...
   bf_switchd: dev_id 0 initialized
   bf_switchd: initialized 1 devices
   ...
   bf_switchd: server started - listening on port 9999
   bfruntime gRPC server started on 0.0.0.0:50052
   ```

3. **Terminal 2:** Configure ports using `run_bfshell.sh`. After starting, type `ucli` and `pm`.
   - Create ports: `port-add #/- 100G NONE` and `port-enb #/-`
   - Disable auto-negotiation: `an-set -/- 2`
   - A sample configuration file is provided. You can also set a front port to loopback for debugging.

4. **Terminal 3:** Run the controller:
   ```bash
   python3 controller.py
   ```
   By default, 128 keys are preloaded to the cache lookup table. Expected output:
   ```
   Binding with p4_name orbitcache
   Binding with p4_name orbitcache successful!!
   ...
   0 AAAAAAAAAAAAFGP 13833500700839145416 305149963468443528 0
   1 AAAAAAAAAAAAJdU 9606265380191604036 10868894308282838393 1
   2 AAAAAAAAAAAAN0Z 13559153852650052292 8982862755689546034 2
   ...
   127 AAAAAAAAAAAJCXe 15573036772515772950 14982745974005649426 127
   128 Keys are inserted to the lookup table.
   ```

### Client/Server-side

1. Open terminals for each node (e.g., 8 terminals for 4 clients and 4 servers).

2. Verify ARP table and IP configuration. The switch code does not handle host network setup, so configure it manually.
   ```bash
   # Example: set ARP rule on node2 for node1
   arp -s 10.0.1.101 0c:42:a1:2f:12:e6
   ```

3. Verify connectivity using `ping`.

4. Configure VMA settings on all nodes:
   ```bash
   sysctl -w net.core.rmem_max=104857600
   sysctl -w net.core.rmem_default=104857600
   echo 2000000000 > /proc/sys/kernel/shmmax
   echo 2048 > /proc/sys/vm/nr_hugepages
   ulimit -l unlimited
   ```

5. Start the server program on server nodes:
   ```bash
   LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./server.out NUM_WORKERS PROTOCOL_ID
   ```
   - `PROTOCOL_ID`: `0` = NoCache, `2` = NetCache, `3` = OrbitCache

   Example:
   ```bash
   LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./server.out 3
   ```
   Expected output:
   ```
   VMA INFO: VMA_VERSION: 9.8.40-1 Release built on Sep 12 2023 10:31:00
   ...
   Server 1 is running
   OrbitCache is running
   Tx/Rx Worker 0,Global Partition 0 is running with Socket 19
   Tx/Rx Worker 1,Global Partition 1 is running with Socket 23
   ...
   Data preparation done
   ```

6. Start the client program on client nodes:
   ```bash
   LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./client.out PROTOCOL_ID KEY_DIST TIME_EXP TARGET_QPS WRITE_RATIO
   ```
   - `PROTOCOL_ID`: Same as server-side
   - `KEY_DIST`: `0` = Uniform, `1` = Zipf-0.9, `2` = Zipf-0.95, `3` = Zipf-0.99
   - `TIME_EXP`: Experiment duration in seconds (use at least 5 due to warm-up)
   - `TARGET_QPS`: Target Tx throughput (Rx/goodput may differ)
   - `WRITE_RATIO`: Ratio of write requests

   Example:
   ```bash
   LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./client.out 3 3 10 1000000 0
   ```
   Expected output:
   ```
   VMA INFO: VMA_VERSION: 9.8.40-1 Release built on Sep 12 2023 10:31:00
   ...
   Client 2 is running
   Collision detection table done
   Query PMF setup done
   Tx Worker 0 is running with Socket 19, CPU 0
   Rx Worker 0 is running with Socket 19, CPU 1
   1 1000912
   2 1000055
   ...
   Tx Worker 0 done with 10000000 reqs, Tx throughput: 1000440 RPS
   ```

7. Before starting clients, seed the switch cache by running the standalone preload tool once from any node on the data network:
   ```bash
   ./preload.out
   # Or: ./preload.out N
   # Or: ./preload.out N target_ip
   ```
   With no arguments, it installs `NUM_HOTKEY` items (128 by default) sized according to `LRATIO` in `header.h`, sending to `dst_ip[0]`. Expected output:
   ```
   [preload] installing 128 items (small=64B, large=1024B, LRATIO=18%) via 10.0.1.108:3001
   [preload] done: 128 items (small=103, large=25)
   ```
   Re-run `preload.out` whenever you change `NUM_HOTKEY` or `LRATIO`, or whenever the controller reinitialises cache state.

8. When the experiment finishes, clients report Tx/Rx throughput, experiment time, and other metrics. Request latency (in microseconds) is logged as a text file. The last line of the log contains the total experiment time — exclude it when analyzing latency data. See `client.c` for details on the output format.

## Citation

If you use any part of this artifact in your research, please cite:

```bibtex
@inproceedings{orbitcache,
  author = {Gyuyeong Kim},
  title = {Pushing the Limits of {In-Network} Caching for {Key-Value} Stores},
  booktitle = {Proc. of USENIX NSDI},
  year = {2025},
  isbn = {978-1-939133-46-5},
  address = {Philadelphia, PA},
  pages = {1155--1168},
  url = {https://www.usenix.org/conference/nsdi25/presentation/kim},
  publisher = {USENIX Association},
  month = apr
}
```
