/* orbitcache.p4 */
#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif

/* Operation type */
#define OP_READ 0
#define OP_R_REPLY 1
#define OP_WRITE 2
#define OP_W_REPLY 3
#define OP_CRCTN 4
#define OP_FETCH 5
#define OP_F_REPLY 6

/* MACRO for better readabtiliy */
#define YES 1
#define NO 0
#define INVALID 0
#define VALID 1
#define SUCCESS 1
#define FAIL 0
#define EMPTY 0
#define NONE 0
#define INIT 0
#define MISSED 2
#define FETCHING 1
#define NOFETCH 0
#define LASTACK 255 // To indicate the current packet is the last value part for a multi-packet item value.

/* Fixed configuration. For artifact evaluation, you do not have to motidfy this */
#define MAX_QUEUE_SIZE_PER_KEY 8 // Max. queue size per each key in the request table
#define NUM_BITSHIFT 3 // 2^n where n is NUM_BITSHIFT and 2^n = MAX_QUEUE_SIZE_PER_KEY; Tofino loves bitwise operations.
#define MAX_CACHE_SIZE 1024 // Numebr of table entry size.
#define TOTAL_QUEUE_SIZE MAX_CACHE_SIZE*MAX_QUEUE_SIZE_PER_KEY
#define MAX_SRV 64 // To determine the maximum number of entries for forwarding tables like IPv4 and dstMAC.
#define ORBITCACHE_PORT 1000 // L4 Port number for OrbitCache
//#define RECIRC_PORT 68 // recirculation port for pipeline 0

/*************************************************************************
*********************** H E A D E R S  ***********************************
*************************************************************************/

typedef bit<16> ether_type_t;
const ether_type_t TYPE_IPV4 = 0x800;
typedef bit<8> trans_protocol_t;
const trans_protocol_t TYPE_TCP = 6;
const trans_protocol_t TYPE_UDP = 17;

header ethernet_h {
    bit<48>   dstAddr;
    bit<48>   srcAddr;
    bit<16>   etherType;
}

header ORBITCACHE_h {
    bit<8> op; // operation type
    bit<32> seq; // sequence number
    bit<64> hkey_hi; // first part for 128-bit key
    bit<64> hkey_lo; // second part for 128-bit key
    bit<8> flag; // Versatile flag
    bit<8> cached; // For stats only
    bit<32> latency; // For stats only

}

header ipv4_h {
    bit<4>   version;
    bit<4>   ihl;
    bit<6>   dscp;
    bit<2>   ecn;
    bit<16>  totalLen;
    bit<16>  identification;
    bit<3>   flags;
    bit<13>  frag_offset;
    bit<8>   ttl;
    bit<8>   protocol;
    bit<16>  hdrChecksum;
    bit<32>  srcAddr;
    bit<32>  dstAddr;
}

header tcp_h {
    bit<16> srcport;
    bit<16> dstport;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4> dataOffset;
    bit<3>  res;
    bit<3>  ecn;
    bit<6>  ctrl;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}

header udp_h {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> len;
    bit<16> checksum;
}

struct header_t {
    ethernet_h ethernet;
    ipv4_h ipv4;
    tcp_h tcp;
    udp_h udp;
    ORBITCACHE_h orbitcache;
}

struct metadata_t {
    bit<32> isCached;
    bit<8> cacheState;
    bit<32> requestIdx;
    bit<32> offset;
    bit<32> cacheIdx;
    bit<16> Crecirc_id; // Group Id for Multicasts
    bit<9> CacheRecirculationPort;
    bit<4> pipeNum; // Pipeline number
    bit<16> qlen;
    bit<8> readyToCommit;

}

struct custom_metadata_t {

}

struct empty_header_t {
    ethernet_h ethernet;
    ipv4_h ipv4;
    tcp_h tcp;
    udp_h udp;
    ORBITCACHE_h orbitcache;
}

struct empty_metadata_t {
    custom_metadata_t custom_metadata;
}

Register<bit<32>,_>(1,0) total_cache; // Stats for total cache hits
Register<bit<32>,_>(1,0) total_overflow; //  Stats for total


Register<bit<8>,_>(MAX_CACHE_SIZE,0) cacheState; // per cached key
Register<bit<32>,_>(1,0) miss_counter; // We need only one cuz this is a global counter
Register<bit<32>,_>(MAX_CACHE_SIZE,0) cache_counter; // per cached key
Register<bit<32>,_>(MAX_CACHE_SIZE,0) ptr_rear; // for circular queue
Register<bit<32>,_>(MAX_CACHE_SIZE,0) ptr_front; // for circular queue
Register<bit<16>,_>(MAX_CACHE_SIZE,0) qlen; // for circular queue

// TOTAL_QUEUE_SIZE  = MAX_CACHE_SIZE*MAX_QUEUE_SIZE_PER_KEY //
Register<bit<32>,_>(TOTAL_QUEUE_SIZE,0) clientIP;
Register<bit<16>,_>(TOTAL_QUEUE_SIZE,0) UDPPort;
Register<bit<32>,_>(TOTAL_QUEUE_SIZE,0) seq;
Register<bit<32>,_>(TOTAL_QUEUE_SIZE,0) latency; // for stats
Register<bit<8>,_>(TOTAL_QUEUE_SIZE,0) acked;

/*************************************************************************
*********************** P A R S E R  ***********************************
*************************************************************************/

parser SwitchIngressParser(
        packet_in pkt,
        out header_t hdr,
        out metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {

    state start {
        pkt.extract(ig_intr_md);
        pkt.advance(PORT_METADATA_SIZE);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            TYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            TYPE_UDP: parse_udp;
            default: accept;
        }
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        //transition parse_orbitcache;
        transition select(hdr.udp.dstPort){
        ORBITCACHE_PORT: parse_orbitcache;
        default: parse_udp2;
        }
    }

    state parse_udp2 {
        //transition parse_orbitcache;
        transition select(hdr.udp.srcPort){
        ORBITCACHE_PORT: parse_orbitcache;
        default: accept;
        }
    }
        state parse_orbitcache {
        pkt.extract(hdr.orbitcache);
        transition accept;
    }

}

/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control SwitchIngress(
        inout header_t hdr,
        inout metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    action drop() {
        ig_intr_dprsr_md.drop_ctl=1;
    }

    action ipv4_forward(bit<9> port) {
        ig_tm_md.ucast_egress_port = port;
    }


    table ipv4_exact {
        key = {
            hdr.ipv4.dstAddr: exact;
        }
        actions = {
            ipv4_forward;
            drop;
            NoAction;
        }
        size = MAX_SRV;
       default_action = drop();
    }

    action msg_cloning_action(){
        ig_intr_dprsr_md.drop_ctl=0;
        ig_tm_md.rid = 1;
        ig_tm_md.mcast_grp_a = ig_md.Crecirc_id;

    }

    table msg_cloning_table{
        actions = {
            msg_cloning_action;
        }
        size = 1;
        default_action = msg_cloning_action;
    }

    table msg_cloning_table2{
        actions = {
            msg_cloning_action;
        }
        size = 1;
        default_action = msg_cloning_action;
    }

    RegisterAction<bit<32>,_,bit<32>>(clientIP) update_clientIP = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                reg_value = hdr.ipv4.srcAddr;
        }
    };


    action update_clientIP_action(){
        update_clientIP.execute(ig_md.requestIdx);

    }

    table update_clientIP_table{
        actions = {
            update_clientIP_action;
        }
        size = 1;
        default_action = update_clientIP_action;
    }

    RegisterAction<bit<32>,_,bit<32>>(clientIP) get_clientIP = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
             bit<32> temp = reg_value;
              reg_value = EMPTY;
             return_value = temp;

        }
    };

    action get_clientIP_action(){
        hdr.ipv4.dstAddr = get_clientIP.execute(ig_md.requestIdx);


    }

    table get_clientIP_table{
        actions = {
            get_clientIP_action;
        }
        size = 1;
        default_action = get_clientIP_action;
    }

    action read_cache_hit_action(bit<32> idx){
        ig_md.isCached = YES;
        ig_md.cacheIdx = idx;
    }

    action read_cache_miss_action(){
        ig_md.isCached = NO;
    }

    table cacheLookup_table{
        key = {
            hdr.orbitcache.hkey_hi: exact;
            hdr.orbitcache.hkey_lo: exact;
        }
        actions = {
            read_cache_hit_action;
            read_cache_miss_action;
            NoAction;
            drop;
        }
        size = MAX_CACHE_SIZE; // Max # of cache entries.
        default_action = read_cache_miss_action;
    }

    action read_CacheRecirculationPort_action(bit<9> port,bit<4> pnum){
        ig_md.CacheRecirculationPort = port;
        ig_md.pipeNum = pnum;
    }

    table read_CacheRecirculationPort_table{
        key = {
            ig_intr_md.ingress_port: exact; // Assigns recirculation port by looking up the Input port
        }
        actions = {
            read_CacheRecirculationPort_action;
            drop;
        }
        size = 512;
        default_action = drop();
    }


    action get_Crecirc_id_action(bit<16> recirc_id){
        ig_md.Crecirc_id = recirc_id ; // This is no server ID
    }

    table get_Crecirc_id_table{
        key = {
            hdr.ipv4.dstAddr: exact; // To know which Port is directed to the client
            ig_md.pipeNum: exact; // To know which recirculation port is correct for the current pipeline
        }
        actions = {
            get_Crecirc_id_action;
            drop;
        }
        size = 256;
        default_action = drop();
    }

    table get_Crecirc_id_table2{
        key = {
            hdr.ipv4.dstAddr: exact;
            ig_md.pipeNum: exact;
        }
        actions = {
            get_Crecirc_id_action;
            drop;
        }
        size = 256;
        default_action = drop();
    }

     RegisterAction<bit<8>, _, bit<8>>(cacheState) valid_cacheState = {
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
            reg_value = VALID;
        }
    };

     RegisterAction<bit<8>, _, bit<8>>(cacheState) invalid_cacheState = {
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
            reg_value = INVALID;
        }
    };

     RegisterAction<bit<8>, _, bit<8>>(cacheState) read_cacheState = {
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
            return_value = reg_value;
        }
    };


     action read_cacheState_action(){
        ig_md.cacheState = read_cacheState.execute(ig_md.cacheIdx);
    }

    table read_cacheState_table{
        actions = {
            read_cacheState_action;
        }
        size = 1;
        default_action = read_cacheState_action;
    }


    action update_dstMAC_action(bit<48> dst_mac){
        hdr.ethernet.dstAddr = dst_mac;
    }

    table update_dstMAC_table{
        key = {
            hdr.ipv4.dstAddr: exact;
        }
        actions = {
            update_dstMAC_action;
            drop;
            NoAction;
        }
        size = MAX_SRV;
        default_action = NoAction;
    }


    action invalid_cacheState_action(){
        invalid_cacheState.execute(ig_md.cacheIdx);

    }

    table invalid_cacheState_table {
        actions = {
            invalid_cacheState_action;
        }
        size = 1;
        default_action = invalid_cacheState_action;
    }

    action valid_cacheState_action(){
        valid_cacheState.execute(ig_md.cacheIdx);
    }

    table valid_cacheState_table {
        actions = {
            valid_cacheState_action;
        }
        size = 1;
        default_action = valid_cacheState_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(latency) get_latency = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
             reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_latency_action(){
        hdr.orbitcache.latency = get_latency.execute(ig_md.requestIdx);
    }

    table get_latency_table{
        actions = {
            get_latency_action;
        }
        size = 1;
        default_action = get_latency_action;
    }

     RegisterAction<bit<32>, _, bit<32>>(latency) update_latency = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                reg_value = hdr.orbitcache.latency;
        }
    };

    action update_latency_action(){
        update_latency.execute(ig_md.requestIdx);
    }

    table update_latency_table{
        actions = {
            update_latency_action;
        }
        size = 1;
        default_action = update_latency_action;
    }

    RegisterAction<bit<16>, _, bit<16>>(UDPPort) get_UDPPort = {
        void apply(inout bit<16> reg_value, out bit<16> return_value) {
            bit<16> temp = reg_value;
             reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_UDPPort_action(){
        hdr.udp.dstPort = get_UDPPort.execute(ig_md.requestIdx);
    }
    table get_UDPPort_table{
        actions = {
            get_UDPPort_action;
        }
        size = 1;
        default_action = get_UDPPort_action;
    }

     RegisterAction<bit<16>, _, bit<16>>(UDPPort) update_UDPPort = {
        void apply(inout bit<16> reg_value, out bit<16> return_value) {
            reg_value = hdr.udp.srcPort;
        }
    };

    action update_UDPPort_action(){
        update_UDPPort.execute(ig_md.requestIdx);
    }

    table update_UDPPort_table{
        actions = {
            update_UDPPort_action;
        }
        size = 1;
        default_action = update_UDPPort_action;
    }


    RegisterAction<bit<32>, _, bit<32>>(cache_counter) inc_cache_counter = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                reg_value = reg_value + 1;

        }
    };

    action inc_cache_counter_action(){
        inc_cache_counter.execute(ig_md.cacheIdx);
    }

    table inc_cache_counter_table{
        actions = {
            inc_cache_counter_action;
        }
        size = 1;
        default_action = inc_cache_counter_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(miss_counter) inc_miss_counter = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                reg_value = reg_value + 1;

        }
    };

    action inc_miss_counter_action(){
        inc_miss_counter.execute(0);
        hdr.orbitcache.cached = MISSED;
    }

    table inc_miss_counter_table{
        actions = {
            inc_miss_counter_action;
        }
        size = 1;
        default_action = inc_miss_counter_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(seq) get_seq = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_seq_action(){
        hdr.orbitcache.seq = get_seq.execute(ig_md.requestIdx);
    }

    table get_seq_table{
        actions = {
            get_seq_action;
        }
        size = 1;
        default_action = get_seq_action;
    }

     RegisterAction<bit<32>, _, bit<32>>(seq) update_seq = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.orbitcache.seq;
        }
    };

    action update_seq_action(){
        update_seq.execute(ig_md.requestIdx);
    }

    table update_seq_table{
        actions = {
            update_seq_action;
        }
        size = 1;
        default_action = update_seq_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(total_cache) inc_total_cache = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = reg_value + 1;

        }
    };


    RegisterAction<bit<8>, _, bit<8>>(acked) get_acked = {
        void apply(inout bit<8> reg_value, out bit<8> return_value) {

            if(reg_value == hdr.orbitcache.flag){
                reg_value = 1;  // 초기값이 1이다.
                return_value = YES;
            }
            else{
                reg_value = reg_value + 1;
                return_value = NO;
            }

        }
    };

    action get_acked_action(){
        ig_md.readyToCommit = get_acked.execute(ig_md.requestIdx);
    }


    table get_acked_table{
        actions = {
            get_acked_action;
        }
        size = 1;
        default_action = get_acked_action;
    }

     RegisterAction<bit<8>, _, bit<8>>(acked) update_acked = {
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
                reg_value = 1;
        }
    };

    action update_acked_action(){
        update_acked.execute(ig_md.requestIdx);
    }


    table update_acked_table{
        actions = {
            update_acked_action;
        }
        size = 1;
        default_action = update_acked_action;
    }


    action inc_total_cache_action(){
        inc_total_cache.execute(0);
    }

    table inc_total_cache_table{
        actions = {
            inc_total_cache_action;
        }
        size = 1;
        default_action = inc_total_cache_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(total_overflow) inc_total_overflow = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = reg_value + 1;

        }
    };

    action inc_total_overflow_action(){
        inc_total_overflow.execute(0);
    }

    table inc_total_overflow_table{
        actions = {
            inc_total_overflow_action;
        }
        size = 1;
        default_action = inc_total_overflow_action;
    }



    RegisterAction<bit<32>, _, bit<32>>(ptr_rear) incr_ptr_rear = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            //bit<16> temp = reg_value;
            return_value = reg_value;
            if(reg_value == MAX_QUEUE_SIZE_PER_KEY-1){
                reg_value = 0;
            }
            else{
                reg_value = reg_value + 1;
            }
        }
    };

    action incr_ptr_rear_action(){
        ig_md.offset = incr_ptr_rear.execute(ig_md.cacheIdx);
        ig_md.requestIdx = ig_md.cacheIdx<<NUM_BITSHIFT;

    }

    table incr_ptr_rear_table{
        actions = {
            incr_ptr_rear_action;
        }
        size = 1;
        default_action = incr_ptr_rear_action;
    }



    RegisterAction<bit<32>, _, bit<32>>(ptr_front) incr_ptr_front = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            return_value = temp;
            if(reg_value == MAX_QUEUE_SIZE_PER_KEY-1){
                reg_value = 0;
            }
            else reg_value = reg_value + 1;

        }
    };

    action incr_ptr_front_action(){
        ig_md.offset = incr_ptr_front.execute(ig_md.cacheIdx);
        ig_md.requestIdx = ig_md.cacheIdx<<NUM_BITSHIFT;

    }

    table incr_ptr_front_table{
        actions = {
            incr_ptr_front_action;
        }
        size = 1;
        default_action = incr_ptr_front_action;
    }

   RegisterAction<bit<16>, _, bit<16>>(qlen) incr_qlen = {
        void apply(inout bit<16> reg_value, out bit<16> return_value) {
            if(reg_value == MAX_QUEUE_SIZE_PER_KEY)
                return_value = 0;

            else{
                reg_value = reg_value + 1;
                return_value = reg_value;
            }


        }
    };

    action incr_qlen_action(){
        ig_md.qlen = incr_qlen.execute(ig_md.cacheIdx);

    }

    table incr_qlen_table{
        actions = {
            incr_qlen_action;
        }
        size = 1;
        default_action = incr_qlen_action;
    }



    RegisterAction<bit<16>, _, bit<16>>(qlen) decr_qlen = {
        void apply(inout bit<16> reg_value, out bit<16> return_value) {
            bit<16> temp = reg_value;
            if(reg_value > 0) reg_value = reg_value - 1;
            return_value = temp;
        }
    };

    action decr_qlen_action(){
        ig_md.qlen = decr_qlen.execute(ig_md.cacheIdx);

    }

    table decr_qlen_table{
        actions = {
            decr_qlen_action;
        }
        size = 1;
        default_action = decr_qlen_action;
    }

    apply {
        /*************** OrbitCache Block START *****************************/
            if (hdr.orbitcache.isValid()) { //
                ig_md.cacheState = INVALID;
                hdr.udp.checksum = 0; // Disable UDP checksum.
                read_CacheRecirculationPort_table.apply();
                cacheLookup_table.apply();

                if(hdr.orbitcache.op != OP_R_REPLY && (ig_intr_md.ingress_port == ig_md.CacheRecirculationPort)){
                    hdr.orbitcache.op = OP_R_REPLY;
                }

                if(hdr.orbitcache.op == OP_CRCTN || hdr.orbitcache.op == OP_FETCH){ // 정정하러 돌아감
                    ipv4_exact.apply();
                }
                else if(hdr.orbitcache.op == OP_READ || hdr.orbitcache.op == OP_R_REPLY){
                    if(ig_md.isCached == YES) read_cacheState_table.apply();
                    if(hdr.orbitcache.op == OP_READ){
                            if(ig_md.isCached == YES) {
                                inc_cache_counter_table.apply();
                                inc_total_cache_table.apply();
                            }
                            if(ig_md.cacheState == VALID){
                                incr_qlen_table.apply();
                                if(ig_md.qlen > 0){
                                    incr_ptr_rear_table.apply();
                                    ig_md.requestIdx = ig_md.requestIdx + ig_md.offset;
                                    //update_acked_table.apply();
                                    update_clientIP_table.apply();
                                    update_UDPPort_table.apply();
                                    update_seq_table.apply();
                                    update_latency_table.apply();

                                    drop();
                                }
                                else{
                                    inc_miss_counter_table.apply();
                                    ipv4_exact.apply();
                                }
                            }
                            else if(ig_md.cacheState == INVALID){
                                ig_intr_dprsr_md.drop_ctl=0;
                                ipv4_exact.apply();
                            }
                    }
                    else if (hdr.orbitcache.op == OP_R_REPLY) {
                        hdr.udp.checksum = 0; // Disable UDP checksum.
                        if(ig_intr_md.ingress_port == ig_md.CacheRecirculationPort){
                            if(ig_md.cacheState == VALID){
                                decr_qlen_table.apply();
                                if(ig_md.qlen> 0){
                                    incr_ptr_front_table.apply();
                                    ig_md.requestIdx = ig_md.requestIdx + ig_md.offset;

                                    //get_acked_table.apply();
                                    get_clientIP_table.apply();
                                    get_UDPPort_table.apply();
                                    get_seq_table.apply();
                                    get_latency_table.apply();
                                    update_dstMAC_table.apply();

                                    get_Crecirc_id_table.apply();
                                    msg_cloning_table.apply();
                                }
                                else{
                                    ig_tm_md.ucast_egress_port = ig_intr_md.ingress_port;
                                }
                            }
                            else if (ig_md.cacheState == INVALID) drop();
                        }
                        else ipv4_exact.apply();
                    }
                }
                else if(hdr.orbitcache.op == OP_WRITE){
                    if(ig_md.isCached == YES){
                        invalid_cacheState_table.apply();
                        hdr.orbitcache.flag = FETCHING;
                    }
                    ipv4_exact.apply();
                }
                else if(hdr.orbitcache.op == OP_W_REPLY || hdr.orbitcache.op == OP_F_REPLY){
                    if(ig_md.isCached == YES){
                        valid_cacheState_table.apply();
                        get_Crecirc_id_table2.apply();
                        msg_cloning_table2.apply();
                    }
                    else ipv4_exact.apply();
                }
                else ipv4_exact.apply();
            }
            else ipv4_exact.apply(); // Normal, not orbitcache packets


    }
}

/*************************************************************************
***********************  D E P A R S E R  *******************************
*************************************************************************/
control SwitchIngressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {

    Checksum() ipv4_checksum;

    apply {

        hdr.ipv4.hdrChecksum = ipv4_checksum.update(
                        {hdr.ipv4.version,
                         hdr.ipv4.ihl,
                         hdr.ipv4.dscp,
                         hdr.ipv4.ecn,
                         hdr.ipv4.totalLen,
                         hdr.ipv4.identification,
                         hdr.ipv4.flags,
                         hdr.ipv4.frag_offset,
                         hdr.ipv4.ttl,
                         hdr.ipv4.protocol,
                         hdr.ipv4.srcAddr,
                         hdr.ipv4.dstAddr});


        pkt.emit(hdr);
    }
}

/*************************************************************************
****************  E G R E S S   P R O C E S S I N G   *******************
*************************************************************************/
parser SwitchEgressParser(
        packet_in pkt,
        out empty_header_t hdr,
        out empty_metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {
    state start {
        pkt.extract(eg_intr_md);
        transition accept;
    }
}

control SwitchEgressDeparser(
        packet_out pkt,
        inout empty_header_t hdr,
        in empty_metadata_t eg_md,
        in egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md) {
    apply {
        pkt.emit(hdr);
    }
}

control SwitchEgress(
        inout empty_header_t hdr,
        inout empty_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {
    apply {

        hdr.udp.checksum = 0; // Disable UDP checksum.

    }
}
/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/
Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(),
         SwitchEgressDeparser()) pipe;

Switch(pipe) main;
