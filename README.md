# Overview
This is the artifact that is used to evaluate OrbitCache, as described in the paper Pushing the Limits of In-Network Caching for Key-Value Stores" in USENIX NSDI 2025.

# Contents

This repository contains the following code segments:

1. Switch data plane code
2. Switch control plane code
3. Client and server applications

# Contents

# Hardware dependencies

- To run experiments using the artifact, at least 2 nodes (1 client and 1 server) are required. However, it is recommended to use more nodes. 
- Nodes should be equipped with an Nvidia ConnectX-5 NIC or similar NIC supporting Nvidia VMA for kernel-bypass networking. Experiments can still be run without the VMA-capable NICs, but this may result in increased latency and decreased throughput due to the application's reliance on a legacy network stack. 
- A programmable switch with Intel Tofino1 ASIC is needed.

Our artifact for a minimal working example is tested on:
- 8 nodes (4 clients and 4 servers) with single-port Nvidia 100GbE MCX515A-CCAT ConnectX-5 NIC
- APS BF6064XT switch with Intel Tofino1 ASIC

# Software dependencies
Our artifact is tested on:

**Clients and servers:**
- Ubuntu 24.04.2 LTS with Linux kernel 6.8.
- Mellanox OFED NIC drivers 25.01-0.6.0
- gcc 13.3.0.
- VMA libvma 9.8.40-1.
- TommyDS (https://www.tommyds.it/)

**Switch:**
- Ubuntu 20.04 LTS with Linux kernel 5.4.
- python 3.8.10
- Intel P4 Studio SDE 9.7.0 and BSP 9.7.0. 


# Installation

## Client/Server-side
1. Place `client.c`, `server.c`, `header.h`, and `Makefile` in the home directory (We used `/home/orbitcache` in the paper).
2. Place `tommyds` in the home directory as well
3. Configure cluster-related details in `header.h`, such as IP and MAC addresses. Note that IP configuration is important in this artifact. Each node should have a linearly-increasing IP address. For example, we use 10.0.1.101 for node1, 10.0.1.102 for node2, and 10.0.1.103 for node3. This is because the server program automatically assigns the server ID based on the last digit of the IP address.

   `header.h`
   - Please carefuilly check Lines 36 - 77, and configure them for your cluster

4. Compile `client.c`, `server.c`, and `header.h` using `make`.

## Switch-side
1. Place `controller.py` and `orbitcache.p4` in the SDE directory.
2. Carefully read the code and configure cluster-related information in the `controller.py`. This includes IP and MAC addresses, and port-related information. This requires advanced domain knowledge of Intel Tofino and P4 programming. This might be hard to understand first. (I also spent tons of time to understand and configure these things.)
3. Compile `orbitcache.p4` using the P4 compiler (we used `p4build.sh` provided by Intel). You can compile it manually with the following commands.
   - `cmake ${SDE}/p4studio -DCMAKE_INSTALL_PREFIX=${SDE_INSTALL} -DCMAKE_MODULE_PATH=${SDE}/cmake -DP4_NAME=orbitcache -DP4_PATH=${SDE}/orbitcache.p4`
   - `make`
   - `make install`
   - `${SDE}` and `${SDE_INSTALL}` are path to the SDE. In our testbed, SDE = `/home/admin/bf-sde-9.7.0`  and SDE_INSTALL = `/home/admin/bf-sde-9.7.0/install`.
   - If done well, you should see the following outputs
```
root@tofino:/home/tofino/bf-sde-9.7.0# cmake ${SDE}/p4studio -DCMAKE_INSTALL_PREFIX=${SDE_INSTALL} -DCMAKE_MODULE_PATH=${SDE}/cmake -DP4_NAME=orbitcache -DP4_PATH=${SDE}/orbitcache.p4
-- 
P4_LANG: p4-16
P4C: /home/tofino/bf-sde-9.7.0/install/bin/bf-p4c
P4C-GEN_BRFT-CONF: /home/tofino/bf-sde-9.7.0/install/bin/p4c-gen-bfrt-conf
P4C-MANIFEST-CONFIG: /home/tofino/bf-sde-9.7.0/install/bin/p4c-manifest-config
-- 
P4_NAME: orbitcache
-- 
P4_PATH: /home/tofino/bf-sde-9.7.0/orbitcache.p4
-- Configuring done
-- Generating done
-- Build files have been written to: /home/tofino/bf-sde-9.7.0
root@tofino:/home/tofino/bf-sde-9.7.0# make
[  0%] Built target bf-p4c
[  0%] Built target driver
[100%] Generating orbitcache/tofino/bf-rt.json
/home/tofino/bf-sde-9.7.0/orbitcache.p4(681): [--Wwarn=unused] warning: Table get_acked_table is not used; removing
    table get_acked_table{
          ^^^^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(700): [--Wwarn=unused] warning: Table update_acked_table is not used; removing
    table update_acked_table{
          ^^^^^^^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(732): [--Wwarn=unused] warning: Table inc_total_overflow_table is not used; removing
    table inc_total_overflow_table{
          ^^^^^^^^^^^^^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(661): [--Wwarn=unused] warning: get_acked: unused instance
    RegisterAction<bit<8>, _, bit<8>>(acked) get_acked = {
                                             ^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(689): [--Wwarn=unused] warning: update_acked: unused instance
     RegisterAction<bit<8>, _, bit<8>>(acked) update_acked = {
                                              ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(721): [--Wwarn=unused] warning: inc_total_overflow: unused instance
    RegisterAction<bit<32>, _, bit<32>>(total_overflow) inc_total_overflow = {
                                                        ^^^^^^^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(170): [--Wwarn=uninitialized_out_param] warning: out parameter 'ig_md' may be uninitialized when 'SwitchIngressParser' terminates
        out metadata_t ig_md,
                       ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(167)
parser SwitchIngressParser(
       ^^^^^^^^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(284): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                                                        ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(284)
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(406): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
                                                      ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(406)
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(412): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
                                                      ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(412)
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(509): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                                                        ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(509)
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(552): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<16> reg_value, out bit<16> return_value) {
                                                        ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(552)
        void apply(inout bit<16> reg_value, out bit<16> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(573): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                                                        ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(573)
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(592): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                                                        ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(592)
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(635): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                                                        ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(635)
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(654): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                                                        ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(654)
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(690): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
                                                      ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(690)
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
             ^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(722): [--Wwarn=uninitialized_out_param] warning: out parameter 'return_value' may be uninitialized when 'apply' terminates
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                                                        ^^^^^^^^^^^^
/home/tofino/bf-sde-9.7.0/orbitcache.p4(722)
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
             ^^^^^
[100%] Built target orbitcache-tofino
[100%] Built target orbitcache
root@tofino:/home/tofino/bf-sde-9.7.0# make install
[  0%] Built target bf-p4c
[  0%] Built target driver
[100%] Built target orbitcache-tofino
[100%] Built target orbitcache
Install the project...
-- Install configuration: "RelWithDebInfo"
-- Up-to-date: /home/tofino/bf-sde-9.7.0/install/share/p4/targets/tofino
-- Installing: /home/tofino/bf-sde-9.7.0/install/share/p4/targets/tofino/orbitcache.conf
-- Up-to-date: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache
-- Up-to-date: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache/pipe
-- Installing: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache/pipe/tofino.bin
-- Installing: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache/pipe/context.json
-- Installing: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache/events.json
-- Installing: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache/source.json
-- Installing: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache/bf-rt.json


   ```
# Experiment workflow
## Switch-side
1. Open three terminals for the switch control plane. We need them for 1) starting the switch program, 2) port configuration, 3) rule configuration by controller
2. In terminal 1, run Orbitcache program using `run_switchd.sh -p orbitcache` in the SDE directory. `run_switch.sh` is included in the SDE by default.
- The output should be like...
```
Using SDE /home/tofino/bf-sde-9.7.0
Using SDE_INSTALL /home/tofino/bf-sde-9.7.0/install
Setting up DMA Memory Pool
Using TARGET_CONFIG_FILE /home/tofino/bf-sde-9.7.0/install/share/p4/targets/tofino/orbitcache.conf
Using PATH /home/tofino/bf-sde-9.7.0/install/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/home/tofino/bf-sde-9.7.0/install/bin
Using LD_LIBRARY_PATH /usr/local/lib:/home/tofino/bf-sde-9.7.0/install/lib::/home/tofino/bf-sde-9.7.0/install/lib
bf_sysfs_fname /sys/class/bf/bf0/device/dev_add
Install dir: /home/tofino/bf-sde-9.7.0/install (0x559518f54bd0)
bf_switchd: system services initialized
bf_switchd: loading conf_file /home/tofino/bf-sde-9.7.0/install/share/p4/targets/tofino/orbitcache.conf...
bf_switchd: processing device configuration...
Configuration for dev_id 0
  Family        : tofino
  pci_sysfs_str : /sys/devices/pci0000:00/0000:00:03.0/0000:05:00.0
  pci_domain    : 0
  pci_bus       : 5
  pci_fn        : 0
  pci_dev       : 0
  pci_int_mode  : 1
  sbus_master_fw: /home/tofino/bf-sde-9.7.0/install/
  pcie_fw       : /home/tofino/bf-sde-9.7.0/install/
  serdes_fw     : /home/tofino/bf-sde-9.7.0/install/
  sds_fw_path   : /home/tofino/bf-sde-9.7.0/install/share/tofino_sds_fw/avago/firmware
  microp_fw_path: 
bf_switchd: processing P4 configuration...
P4 profile for dev_id 0
num P4 programs 1
  p4_name: orbitcache
  p4_pipeline_name: pipe
    libpd: 
    libpdthrift: 
    context: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache/pipe/context.json
    config: /home/tofino/bf-sde-9.7.0/install/share/tofinopd/orbitcache/pipe/tofino.bin
  Pipes in scope [0 1 2 3 ]
  diag: 
  accton diag: 
  Agent[0]: /home/tofino/bf-sde-9.7.0/install/lib/libpltfm_mgr.so
  non_default_port_ppgs: 0
  SAI default initialize: 1 
bf_switchd: library /home/tofino/bf-sde-9.7.0/install/lib/libpltfm_mgr.so loaded
bf_switchd: agent[0] initialized
Health monitor started 
Operational mode set to ASIC
Initialized the device types using platforms infra API
ASIC detected at PCI /sys/class/bf/bf0/device
ASIC pci device id is 16
Starting PD-API RPC server on port 9090
bf_switchd: drivers initialized
Setting core_pll_ctrl0=cd44cbfe
\
bf_switchd: dev_id 0 initialized

bf_switchd: initialized 1 devices
Adding Thrift service for bf-platforms to server
bf_switchd: thrift initialized for agent : 0
bf_switchd: spawning cli server thread
bf_switchd: spawning driver shell
bf_switchd: server started - listening on port 9999
bfruntime gRPC server started on 0.0.0.0:50052

        ********************************************
        *      WARNING: Authorised Access Only     *
        ********************************************
    

bfshell> Starting UCLI from bf-shell 

```
3. In terminal 2, configure ports manually or `run_bfshell.sh`. It is recommended to configure ports to 100Gbps.
 - After starting the switch program, run `./run_bfshell.sh` and type `ucli` and `pm`.
 - You can create ports like `port-add #/- 100G NONE` and `port-enb #/-`. It is recommended to turn off auto-negotiation using `an-set -/- 2`. This part requires knowledge of Intel Tofino-related stuff. You can find more information in the switch manual or on Intel websites. I uploaded a sample configuration file. Note that you can set a front port to a loopback port for better debugging (instead of using internation recirculatio port where we cannot see packets). 
4. In terminal 3, run the controller using `python3 controller.py` in the SDE directory. By default, we preload 128 keys to the cache lookup table.
- The output should be ...
```
root@tofino:/home/admin/bf-sde-9.7.0# python3 controller.py
Binding with p4_name orbitcache
Binding with p4_name orbitcache successful!!
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
Received orbitcache on GetForwarding on client 0, device 0
0 AAAAAAAAAAAAFGP 13833500700839145416 305149963468443528 0
1 AAAAAAAAAAAAJdU 9606265380191604036 10868894308282838393 1
2 AAAAAAAAAAAAN0Z 13559153852650052292 8982862755689546034 2
3 AAAAAAAAAAAASNe 9394920547967376517 1693438258242181827 3
4 AAAAAAAAAAAAWkj 9665396339512089011 13138618137277545506 4
5 AAAAAAAAAAAAa7o 3346621490298314385 14146857955682963419 5
6 AAAAAAAAAAAAfUt 5569646748052919891 3786802104300928702 6
7 AAAAAAAAAAAAjry 2641648143767124611 6078388740900987211 7
8 AAAAAAAAAAAAoE3 13880863324388004929 11101004450512041063 8
9 AAAAAAAAAAAAsb8 11389212948037033212 18418121837775530055 9
10 AAAAAAAAAAAAwzD 11247800725610099082 11830129090728851207 10
11 AAAAAAAAAAAA1MI 17012916684340159612 14628204108108529588 11
12 AAAAAAAAAAAA5jN 6026599716181990072 14484638336226771578 12
13 AAAAAAAAAAAA96S 8220871549642060575 344503017595879157 13
14 AAAAAAAAAAABETX 2488839082870395196 8910351109806539124 14
15 AAAAAAAAAAABIqc 10423801463781430683 4202195452343776511 15
16 AAAAAAAAAAABNDh 2524464840051382919 17597902166443442185 16
17 AAAAAAAAAAABRam 6916822242761979009 1467158197677201523 17
18 AAAAAAAAAAABVxr 6049861293158258372 2217781265217515737 18
19 AAAAAAAAAAABaKw 6798469928478916934 3446835443131714354 19
20 AAAAAAAAAAABeh1 4403946934657747958 11384856572064374183 20
21 AAAAAAAAAAABi46 15623848636337751770 5235577500436637935 21
22 AAAAAAAAAAABnSB 15739654825143674398 13072665333042684555 22
23 AAAAAAAAAAABrpG 2921870352558144258 4914922986401518093 23
24 AAAAAAAAAAABwCL 2188105367596445277 13420283977503348661 24
25 AAAAAAAAAAAB0ZQ 10349004630290632419 5392260956423238232 25
26 AAAAAAAAAAAB4wV 11348396399710893364 8630507170284937985 26
27 AAAAAAAAAAAB9Ja 10694897896246659063 7848911791771176582 27
28 AAAAAAAAAAACDgf 8972707111496423962 14438362060472170217 28
29 AAAAAAAAAAACH3k 10265311071527116693 15391944860018294633 29
30 AAAAAAAAAAACMQp 9763315331683831947 2834312444690326964 30
31 AAAAAAAAAAACQnu 14057742362279507131 2055267727118338387 31
32 AAAAAAAAAAACVAz 16479923734311759448 3837815322351221942 32
33 AAAAAAAAAAACZX4 14388333624433547607 13708508659860099759 33
34 AAAAAAAAAAACdu9 4047706269679131389 13111654007635244843 34
35 AAAAAAAAAAACiIE 619894091278481385 1921904599674182337 35
36 AAAAAAAAAAACmfJ 14509252022542917520 212233030393009514 36
37 AAAAAAAAAAACq2O 2846614828655105668 1919718318980912292 37
38 AAAAAAAAAAACvPT 14143875142389480842 18426502581624046847 38
39 AAAAAAAAAAACzmY 17815436960849820605 565681756455285457 39
40 AAAAAAAAAAAC39d 6644724565397910427 4210576195286853127 40
41 AAAAAAAAAAAC8Wi 17594085416495182077 13848066249978513788 41
42 AAAAAAAAAAADCtn 11736642518954197621 8100698461551985578 42
43 AAAAAAAAAAADHGs 2873539001283143255 6411067799583850598 43
44 AAAAAAAAAAADLdx 16594636474091762981 5193309405662767740 44
45 AAAAAAAAAAADP02 12644640015403057085 7636478175891586014 45
46 AAAAAAAAAAADUN7 4896417930644460299 17584420101112232173 46
47 AAAAAAAAAAADYlC 13835960897699437465 7060393190989098854 47
48 AAAAAAAAAAADc8H 14084880736427699173 5415216904512133378 48
49 AAAAAAAAAAADhVM 9421976853288656691 18355084075792847814 49
50 AAAAAAAAAAADlsR 3660114764238008493 7992113182956669811 50
51 AAAAAAAAAAADqFW 6269254437642972512 18137184758225979102 51
52 AAAAAAAAAAADucb 3016813360573777469 826577929369721895 52
53 AAAAAAAAAAADyzg 5883869635685781854 8047134582429313208 53
54 AAAAAAAAAAAD3Ml 17439123688403716318 7662713545332967947 54
55 AAAAAAAAAAAD7jq 3077475446324613790 4029479270577092577 55
56 AAAAAAAAAAAEB6v 8650372957295745751 4605199875976596748 56
57 AAAAAAAAAAAEGT0 9034625895971155061 10298275024321492130 57
58 AAAAAAAAAAAEKq5 10500383439815604895 1411043657568541255 58
59 AAAAAAAAAAAEPEA 16567255161884858756 6802776439200109844 59
60 AAAAAAAAAAAETbF 10418010548676341005 17510086554468724798 60
61 AAAAAAAAAAAEXyK 8387789072576808548 2512564790037732139 61
62 AAAAAAAAAAAEcLP 9159771720821502019 15209026035131301747 62
63 AAAAAAAAAAAEgiU 2727379648156057034 9326473218497212815 63
64 AAAAAAAAAAAEk5Z 9250516675345327121 14550955520359471774 64
65 AAAAAAAAAAAEpSe 12572204937874367806 8332444224014512955 65
66 AAAAAAAAAAAEtpj 7539886512809365021 15887137456545894970 66
67 AAAAAAAAAAAEyCo 4184129359620650234 6244910460308700255 67
68 AAAAAAAAAAAE2Zt 18077946884836637972 833136771689660351 68
69 AAAAAAAAAAAE6wy 830669285642671951 8462892310592190454 69
70 AAAAAAAAAAAFBJ3 8601536829229076115 17669320671081221873 70
71 AAAAAAAAAAAFFg8 1337045002693677340 3340800825241641204 71
72 AAAAAAAAAAAFJ4D 12826526440638857538 12355200857803408071 72
73 AAAAAAAAAAAFORI 2573600937202445315 5749353483804864145 73
74 AAAAAAAAAAAFSoN 12668686090034950855 14296253809543025843 74
75 AAAAAAAAAAAFXBS 1916030278395427482 8990879118623658253 75
76 AAAAAAAAAAAFbYX 10753975369391717810 16244594363446458432 76
77 AAAAAAAAAAAFfvc 7688356086534730210 15278987019903191767 77
78 AAAAAAAAAAAFkIh 6692978241883427731 9026952751696825942 78
79 AAAAAAAAAAAFofm 9857283545566166632 6280255332499124670 79
80 AAAAAAAAAAAFs2r 8828356291868998690 7942921865476792443 80
81 AAAAAAAAAAAFxPw 13140592301099892188 13397692408870016600 81
82 AAAAAAAAAAAF1m1 8410569091064468942 1081279640083506051 82
83 AAAAAAAAAAAF596 566938175261548434 11614416053476463067 83
84 AAAAAAAAAAAGAXB 15075750792656449171 6021181060272644360 84
85 AAAAAAAAAAAGEuG 16248398774554555210 7530807938280313261 85
86 AAAAAAAAAAAGJHL 9270906442696023934 950831553590791976 86
87 AAAAAAAAAAAGNeQ 12177821001022400515 6320701527018579889 87
88 AAAAAAAAAAAGR1V 7573382869238406090 11426760286562982542 88
89 AAAAAAAAAAAGWOa 16761350350353062776 17596809025556154498 89
90 AAAAAAAAAAAGalf 17274396474903471395 17873009164005294580 90
91 AAAAAAAAAAAGe8k 14229209127338518612 7818303861017325246 91
92 AAAAAAAAAAAGjVp 7829544237076878609 743134879512385959 92
93 AAAAAAAAAAAGnsu 9675469503288653886 1471530759155055245 93
94 AAAAAAAAAAAGsFz 5824522092730892137 5878708429783355093 94
95 AAAAAAAAAAAGwc4 9570944245227570287 8897597805529632674 95
96 AAAAAAAAAAAG0z9 16976182599024491038 812002723999804293 96
97 AAAAAAAAAAAG5NE 3750696477951019410 9700327231283558919 97
98 AAAAAAAAAAAG9kJ 2948936875028494751 1326871847673616461 98
99 AAAAAAAAAAAHD7O 8318878866327026964 6637347860011287174 99
100 AAAAAAAAAAAHIUT 4493658800989766313 9557490220954806310 100
101 AAAAAAAAAAAHMrY 5361287716456145688 14629661628019570647 101
102 AAAAAAAAAAAHREd 13001302657817344326 13564942889038602997 102
103 AAAAAAAAAAAHVbi 15753536039572664630 17906167755535216038 103
104 AAAAAAAAAAAHZyn 9018418340024607645 2498718345305567251 104
105 AAAAAAAAAAAHeLs 5453468790031890785 4851156463383429283 105
106 AAAAAAAAAAAHiix 4366851561535117802 3752550372050394245 106
107 AAAAAAAAAAAHm52 7970532138823548150 8058065986485779658 107
108 AAAAAAAAAAAHrS7 4702413082541343634 12991044184640673835 108
109 AAAAAAAAAAAHvqC 14168789186814066820 10046852734783106954 109
110 AAAAAAAAAAAH0DH 9068833839575475066 13592271398716663674 110
111 AAAAAAAAAAAH4aM 7705310256709308644 6105717250394285071 111
112 AAAAAAAAAAAH8xR 8518930030700389944 14367672314424706251 112
113 AAAAAAAAAAAIDKW 2524243063202496196 18426138201394144962 113
114 AAAAAAAAAAAIHhb 9414741122469907678 8189242833342455870 114
115 AAAAAAAAAAAIL4g 8716525367814875026 453452676256452793 115
116 AAAAAAAAAAAIQRl 14494567919299885721 5714372991173311630 116
117 AAAAAAAAAAAIUoq 16431236772111117611 13962117230739088948 117
118 AAAAAAAAAAAIZBv 5800348378716280961 12563261911589836324 118
119 AAAAAAAAAAAIdY0 7379091384865260189 10617472018769478433 119
120 AAAAAAAAAAAIhv5 18194847532315879664 2273895805064415189 120
121 AAAAAAAAAAAImJA 16216581375592386269 2644106016955773741 121
122 AAAAAAAAAAAIqgF 8083016374920471105 3861500030745770010 122
123 AAAAAAAAAAAIu3K 4890928446081004387 16145118588603135642 123
124 AAAAAAAAAAAIzQP 7887883444072982160 8818527317537702283 124
125 AAAAAAAAAAAI3nU 9489175113843625722 10186774705208881971 125
126 AAAAAAAAAAAI8AZ 17514683886146426319 12111430550645952845 126
127 AAAAAAAAAAAJCXe 15573036772515772950 14982745974005649426 127
128 Keys are inserted to the lookup table.
```

## Client/Server-side
1. Open terminals for each node. For example, we open 8 terminals for 8 nodes (4 clients and 4 servers).
2. Make sure your ARP table and IP configuration are correct. The provided switch code does not concern the network setup of hosts. Therefore, you should do network configuration in hosts manually. Also, please double-check check the cluster-related information in the codes is configured correctly.
   - You can set the arp rule using `arp -s IP_ADDRESS MAC_ADDRESS`. For example, type `arp -s 10.0.1.101 0c:42:a1:2f:12:e6` in node 2 for node 1.
3. Make sure each node can communicate by using tools like `ping`. e.g., `ping 10.0.1.101` in other nodes.
4. Configure VMA-related stuffs like socket buffers, hugepages, etc. The following commands must be executed in all nodes. <br>
`sysctl -w net.core.rmem_max=104857600 && sysctl -w net.core.rmem_default=104857600` <br>
`echo 2000000000 > /proc/sys/kernel/shmmax` <br>
`echo 2048 > /proc/sys/vm/nr_hugepages` <br>
`ulimit -l unlimited` <br>

5. Turn on the server program in server nodes by typing the following command<br>
`LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./server.out NUM_WORKERS PROTOCOL_ID` <br>
`PROTOCOL_ID`: The ID of protocols to use. 0 is NoCache, 1 is NotUsed, 2 is NetCache, 3 is OrbitCache.<br>


For our example, use the command as follows:<br>
`LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./server.out 3` <br>
If done well, the output should be as follows.<br>

```
root@node5:/home/orbitcache# LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./server.out 3
 VMA INFO: ---------------------------------------------------------------------------
 VMA INFO: VMA_VERSION: 9.8.40-1 Release built on Sep 12 2023 10:31:00
 VMA INFO: Cmd Line: ./server.out 3
 VMA INFO: OFED Version: OFED-internal-25.01-0.6.0:
 VMA INFO: ---------------------------------------------------------------------------
 VMA INFO: Log Level                      INFO                       [VMA_TRACELEVEL]
 VMA INFO: Thread mode                    Multi mutex lock           [VMA_THREAD_MODE]
 VMA INFO: ---------------------------------------------------------------------------
Server 1 is running
OrbitCache is running
Tx/Rx Worker 0,Global Partition 0 is running with Socket 19  
Tx/Rx Worker 1,Global Partition 1 is running with Socket 23  
Tx/Rx Worker 2,Global Partition 2 is running with Socket 24  
Tx/Rx Worker 3,Global Partition 3 is running with Socket 28  
Tx/Rx Worker 4,Global Partition 4 is running with Socket 29  
Tx/Rx Worker 5,Global Partition 5 is running with Socket 31  
Tx/Rx Worker 6,Global Partition 6 is running with Socket 33  
Tx/Rx Worker 7,Global Partition 7 is running with Socket 35  
Data preparation done
Data preparation done
Data preparation done
Data preparation done
Data preparation done
Data preparation done
Data preparation done
Data preparation done

``` 
   
6. Turn on the client program in client nodes by using the following command. <br>
`Usage: ./client.out PROTOCOL_ID KEY_DIST TIME_EXP TARGET_QPS WRITE_RATIO`<br>
`PROTOCOL_ID`: The ID of protocols to use. Same as in the server-side one.<br>
`KEY_DIST`: 0 Uniform 1 zipf-0.9 2 zipf-0.95 3 zipf-0.99.<br>
`TIME_EXP`: The experiment time. Set this at least more than 5 because there is a warm-up effect at the early phase of the experiment. <br>
`TARGET_QPS`: The target throughput (=Tx throughput). Goodput(Rx throughput) can be different. <br>
`WRITE_RATIO`: The ratio of write requests.
 
For example, use the command as follows:<br>
`LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./client.out 3 3 10 1000000 0` <br>
The output should be like ... <br>

```
root@node2:/home/orbitcache# LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./client.out 3 3 10 1000000 0
 VMA INFO: ---------------------------------------------------------------------------
 VMA INFO: VMA_VERSION: 9.8.40-1 Release built on Sep 12 2023 10:31:00
 VMA INFO: Cmd Line: ./client.out 3 3 10 1000000 0
 VMA INFO: OFED Version: OFED-internal-25.01-0.6.0:
 VMA INFO: ---------------------------------------------------------------------------
 VMA INFO: Log Level                      INFO                       [VMA_TRACELEVEL]
 VMA INFO: Thread mode                    Multi mutex lock           [VMA_THREAD_MODE]
 VMA INFO: ---------------------------------------------------------------------------
Client 2 is running 
Collision detection table done
Query PMF setup done
Tx Worker 0 is running with Socket 19, CPU 0
Rx Worker 0 is running with Socket 19, CPU 1
1 1000912
2 1000055
3 999162
4 999570
5 1000670
6 1000407
7 1000921
8 1001788
9 1000486
Tx Worker 0 done with 10000000 reqs, Tx throughput: 1000440 RPS 
10 996029
Rx Worker 0 finished with 0 hash collisions
10.063744
10000000
3069176
4850
6930824
0.157774
0.000000
993665
304973
688692
23398
23295
22784
22490
22176
22122
21970
21907
21844
21842
21819
21806
21682
21569
21554
21476
21452
21443
21392
21300
21196
21166
21114
21082
21036
21029
20768
20757
20669
20354
20242
19944
```

7. If you run the client first, the server 1 will fetch the (preloaded) cached key-value pairs. Therefore, the actual experiment is from the second run. The output of the server 1 is liks as follows.
```
0 AAAAAAAAAAAAFGP 13833500700839145416 305149963468443528
1 AAAAAAAAAAAAJdU 9606265380191604036 10868894308282838393
2 AAAAAAAAAAAAN0Z 13559153852650052292 8982862755689546034
3 AAAAAAAAAAAASNe 9394920547967376517 1693438258242181827
4 AAAAAAAAAAAAWkj 9665396339512089011 13138618137277545506
5 AAAAAAAAAAAAa7o 3346621490298314385 14146857955682963419
6 AAAAAAAAAAAAfUt 5569646748052919891 3786802104300928702
7 AAAAAAAAAAAAjry 2641648143767124611 6078388740900987211
8 AAAAAAAAAAAAoE3 13880863324388004929 11101004450512041063
9 AAAAAAAAAAAAsb8 11389212948037033212 18418121837775530055
10 AAAAAAAAAAAAwzD 11247800725610099082 11830129090728851207
11 AAAAAAAAAAAA1MI 17012916684340159612 14628204108108529588
12 AAAAAAAAAAAA5jN 6026599716181990072 14484638336226771578
13 AAAAAAAAAAAA96S 8220871549642060575 344503017595879157
14 AAAAAAAAAAABETX 2488839082870395196 8910351109806539124
15 AAAAAAAAAAABIqc 10423801463781430683 4202195452343776511
16 AAAAAAAAAAABNDh 2524464840051382919 17597902166443442185
17 AAAAAAAAAAABRam 6916822242761979009 1467158197677201523
18 AAAAAAAAAAABVxr 6049861293158258372 2217781265217515737
19 AAAAAAAAAAABaKw 6798469928478916934 3446835443131714354
20 AAAAAAAAAAABeh1 4403946934657747958 11384856572064374183
21 AAAAAAAAAAABi46 15623848636337751770 5235577500436637935
22 AAAAAAAAAAABnSB 15739654825143674398 13072665333042684555
23 AAAAAAAAAAABrpG 2921870352558144258 4914922986401518093
24 AAAAAAAAAAABwCL 2188105367596445277 13420283977503348661
25 AAAAAAAAAAAB0ZQ 10349004630290632419 5392260956423238232
26 AAAAAAAAAAAB4wV 11348396399710893364 8630507170284937985
27 AAAAAAAAAAAB9Ja 10694897896246659063 7848911791771176582
28 AAAAAAAAAAACDgf 8972707111496423962 14438362060472170217
29 AAAAAAAAAAACH3k 10265311071527116693 15391944860018294633
30 AAAAAAAAAAACMQp 9763315331683831947 2834312444690326964
31 AAAAAAAAAAACQnu 14057742362279507131 2055267727118338387
32 AAAAAAAAAAACVAz 16479923734311759448 3837815322351221942
33 AAAAAAAAAAACZX4 14388333624433547607 13708508659860099759
34 AAAAAAAAAAACdu9 4047706269679131389 13111654007635244843
35 AAAAAAAAAAACiIE 619894091278481385 1921904599674182337
36 AAAAAAAAAAACmfJ 14509252022542917520 212233030393009514
37 AAAAAAAAAAACq2O 2846614828655105668 1919718318980912292
38 AAAAAAAAAAACvPT 14143875142389480842 18426502581624046847
39 AAAAAAAAAAACzmY 17815436960849820605 565681756455285457
40 AAAAAAAAAAAC39d 6644724565397910427 4210576195286853127
41 AAAAAAAAAAAC8Wi 17594085416495182077 13848066249978513788
42 AAAAAAAAAAADCtn 11736642518954197621 8100698461551985578
43 AAAAAAAAAAADHGs 2873539001283143255 6411067799583850598
44 AAAAAAAAAAADLdx 16594636474091762981 5193309405662767740
45 AAAAAAAAAAADP02 12644640015403057085 7636478175891586014
46 AAAAAAAAAAADUN7 4896417930644460299 17584420101112232173
47 AAAAAAAAAAADYlC 13835960897699437465 7060393190989098854
48 AAAAAAAAAAADc8H 14084880736427699173 5415216904512133378
49 AAAAAAAAAAADhVM 9421976853288656691 18355084075792847814
50 AAAAAAAAAAADlsR 3660114764238008493 7992113182956669811
51 AAAAAAAAAAADqFW 6269254437642972512 18137184758225979102
52 AAAAAAAAAAADucb 3016813360573777469 826577929369721895
53 AAAAAAAAAAADyzg 5883869635685781854 8047134582429313208
54 AAAAAAAAAAAD3Ml 17439123688403716318 7662713545332967947
55 AAAAAAAAAAAD7jq 3077475446324613790 4029479270577092577
56 AAAAAAAAAAAEB6v 8650372957295745751 4605199875976596748
57 AAAAAAAAAAAEGT0 9034625895971155061 10298275024321492130
58 AAAAAAAAAAAEKq5 10500383439815604895 1411043657568541255
59 AAAAAAAAAAAEPEA 16567255161884858756 6802776439200109844
60 AAAAAAAAAAAETbF 10418010548676341005 17510086554468724798
61 AAAAAAAAAAAEXyK 8387789072576808548 2512564790037732139
62 AAAAAAAAAAAEcLP 9159771720821502019 15209026035131301747
63 AAAAAAAAAAAEgiU 2727379648156057034 9326473218497212815
64 AAAAAAAAAAAEk5Z 9250516675345327121 14550955520359471774
65 AAAAAAAAAAAEpSe 12572204937874367806 8332444224014512955
66 AAAAAAAAAAAEtpj 7539886512809365021 15887137456545894970
67 AAAAAAAAAAAEyCo 4184129359620650234 6244910460308700255
68 AAAAAAAAAAAE2Zt 18077946884836637972 833136771689660351
69 AAAAAAAAAAAE6wy 830669285642671951 8462892310592190454
70 AAAAAAAAAAAFBJ3 8601536829229076115 17669320671081221873
71 AAAAAAAAAAAFFg8 1337045002693677340 3340800825241641204
72 AAAAAAAAAAAFJ4D 12826526440638857538 12355200857803408071
73 AAAAAAAAAAAFORI 2573600937202445315 5749353483804864145
74 AAAAAAAAAAAFSoN 12668686090034950855 14296253809543025843
75 AAAAAAAAAAAFXBS 1916030278395427482 8990879118623658253
76 AAAAAAAAAAAFbYX 10753975369391717810 16244594363446458432
77 AAAAAAAAAAAFfvc 7688356086534730210 15278987019903191767
78 AAAAAAAAAAAFkIh 6692978241883427731 9026952751696825942
79 AAAAAAAAAAAFofm 9857283545566166632 6280255332499124670
80 AAAAAAAAAAAFs2r 8828356291868998690 7942921865476792443
81 AAAAAAAAAAAFxPw 13140592301099892188 13397692408870016600
82 AAAAAAAAAAAF1m1 8410569091064468942 1081279640083506051
83 AAAAAAAAAAAF596 566938175261548434 11614416053476463067
84 AAAAAAAAAAAGAXB 15075750792656449171 6021181060272644360
85 AAAAAAAAAAAGEuG 16248398774554555210 7530807938280313261
86 AAAAAAAAAAAGJHL 9270906442696023934 950831553590791976
87 AAAAAAAAAAAGNeQ 12177821001022400515 6320701527018579889
88 AAAAAAAAAAAGR1V 7573382869238406090 11426760286562982542
89 AAAAAAAAAAAGWOa 16761350350353062776 17596809025556154498
90 AAAAAAAAAAAGalf 17274396474903471395 17873009164005294580
91 AAAAAAAAAAAGe8k 14229209127338518612 7818303861017325246
92 AAAAAAAAAAAGjVp 7829544237076878609 743134879512385959
93 AAAAAAAAAAAGnsu 9675469503288653886 1471530759155055245
94 AAAAAAAAAAAGsFz 5824522092730892137 5878708429783355093
95 AAAAAAAAAAAGwc4 9570944245227570287 8897597805529632674
96 AAAAAAAAAAAG0z9 16976182599024491038 812002723999804293
97 AAAAAAAAAAAG5NE 3750696477951019410 9700327231283558919
98 AAAAAAAAAAAG9kJ 2948936875028494751 1326871847673616461
99 AAAAAAAAAAAHD7O 8318878866327026964 6637347860011287174
100 AAAAAAAAAAAHIUT 4493658800989766313 9557490220954806310
101 AAAAAAAAAAAHMrY 5361287716456145688 14629661628019570647
102 AAAAAAAAAAAHREd 13001302657817344326 13564942889038602997
103 AAAAAAAAAAAHVbi 15753536039572664630 17906167755535216038
104 AAAAAAAAAAAHZyn 9018418340024607645 2498718345305567251
105 AAAAAAAAAAAHeLs 5453468790031890785 4851156463383429283
106 AAAAAAAAAAAHiix 4366851561535117802 3752550372050394245
107 AAAAAAAAAAAHm52 7970532138823548150 8058065986485779658
108 AAAAAAAAAAAHrS7 4702413082541343634 12991044184640673835
109 AAAAAAAAAAAHvqC 14168789186814066820 10046852734783106954
110 AAAAAAAAAAAH0DH 9068833839575475066 13592271398716663674
111 AAAAAAAAAAAH4aM 7705310256709308644 6105717250394285071
112 AAAAAAAAAAAH8xR 8518930030700389944 14367672314424706251
113 AAAAAAAAAAAIDKW 2524243063202496196 18426138201394144962
114 AAAAAAAAAAAIHhb 9414741122469907678 8189242833342455870
115 AAAAAAAAAAAIL4g 8716525367814875026 453452676256452793
116 AAAAAAAAAAAIQRl 14494567919299885721 5714372991173311630
117 AAAAAAAAAAAIUoq 16431236772111117611 13962117230739088948
118 AAAAAAAAAAAIZBv 5800348378716280961 12563261911589836324
119 AAAAAAAAAAAIdY0 7379091384865260189 10617472018769478433
120 AAAAAAAAAAAIhv5 18194847532315879664 2273895805064415189
121 AAAAAAAAAAAImJA 16216581375592386269 2644106016955773741
122 AAAAAAAAAAAIqgF 8083016374920471105 3861500030745770010
123 AAAAAAAAAAAIu3K 4890928446081004387 16145118588603135642
124 AAAAAAAAAAAIzQP 7887883444072982160 8818527317537702283
125 AAAAAAAAAAAI3nU 9489175113843625722 10186774705208881971
126 AAAAAAAAAAAI8AZ 17514683886146426319 12111430550645952845
127 AAAAAAAAAAAJCXe 15573036772515772950 14982745974005649426
The preload of 128 cache items are done.
```

8. When the experiment is finished, the clients report Tx/Rx throughput, experiment time, and other related information. Currently, output contains no comments and only numbers exist for logging. However, you can check what they are in the code of client.c. Request latency in microseconds is logged as a text file. The end line of the log contains the total experiment time. Therefore, when you analyze the log, you should be careful. 

# Citation

Please cite this work if you refer to or use any part of this artifact for your research. 

BibTex:
```
@inproceedings {orbitcache,
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
