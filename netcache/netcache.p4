#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif
#define OP_READ 0
#define OP_R_REPLY 1
#define OP_WRITE 2
#define OP_W_REPLY 3
#define OP_CRCTN 4
#define OP_FETCH 5
#define OP_F_REPLY 6
#define MAX_OBJ 65536
#define MAX_SRV 64
#define YES 1
#define NO 0
#define INVALID 0
#define VALID 1
#define SUCCESS 1
#define FAIL 0
#define EMPTY 0
#define NONE 0
#define CONTROLLER_PORT 64
#define NETCACHE_PORT0 1000
#define NETCACHE_PORT1 1001
#define NETCACHE_PORT2 1002
#define NETCACHE_PORT3 1003
#define NETCACHE_PORT4 1004
#define NETCACHE_PORT5 1005
#define NETCACHE_PORT6 1006
#define NETCACHE_PORT7 1007
#define NETCACHE_PORT8 1008
#define NETCACHE_PORT9 1009
#define NETCACHE_PORT10 1010
#define NETCACHE_PORT11 1011
#define NETCACHE_PORT12 1012
#define NETCACHE_PORT13 1013
#define NETCACHE_PORT14 1014
#define NETCACHE_PORT15 1015

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

header NETCACHE_h {
    bit<8> op;
    bit<32> seq;
    bit<64> hkey_hi;
    bit<64> hkey_lo;
    bit<8> cached; // For stats
    bit<32> latency; // For stats

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

header value_h { //16바이트
    bit<32> val1;
    bit<32> val2;
    bit<32> val3;
    bit<32> val4;
}


struct header_t {
    ethernet_h ethernet;
    ipv4_h ipv4;
    tcp_h tcp;
    udp_h udp;
    NETCACHE_h netcache;
    value_h value1;
    value_h value2;
    value_h value3;
    value_h value4;
    value_h value5;
    value_h value6;
    value_h value7;
    value_h value8;
}

struct metadata_t {
    bit<32> isCached;
    bit<8> cache_state;
    bit<32> clientIP;
    bit<32> clientIP2;
    bit<8> isOK;
    bit<8> UpdateNow;
    bit<2> notforward;
    bit<8> tIDX;
    bit<32> cache_idx;
}

struct pair {
    bit<32>     hi; // High
    bit<32>     lo; // Low
}

struct custom_metadata_t {

}

struct empty_header_t {
    ethernet_h ethernet;
    ipv4_h ipv4;
    tcp_h tcp;
    udp_h udp;
    NETCACHE_h netcache;
}

struct empty_metadata_t {
    custom_metadata_t custom_metadata;
}

Register<bit<8>,_>(MAX_OBJ,0) cache_state;
//Register<pair,bit<32>>(size=MAX_OBJ,initial_value ={0,0}) clientIP;
Register<bit<32>,_>(MAX_OBJ,0) cache_counter;
Register<bit<32>,_>(MAX_OBJ,1) Value1;
Register<bit<32>,_>(MAX_OBJ,1) Value2;
Register<bit<32>,_>(MAX_OBJ,1) Value3;
Register<bit<32>,_>(MAX_OBJ,1) Value4;

Register<bit<32>,_>(MAX_OBJ,1) Value5;
Register<bit<32>,_>(MAX_OBJ,1) Value6;
Register<bit<32>,_>(MAX_OBJ,1) Value7;
Register<bit<32>,_>(MAX_OBJ,1) Value8;


Register<bit<32>,_>(MAX_OBJ,1) Value9;
Register<bit<32>,_>(MAX_OBJ,1) Value10;
Register<bit<32>,_>(MAX_OBJ,1) Value11;
Register<bit<32>,_>(MAX_OBJ,1) Value12;

Register<bit<32>,_>(MAX_OBJ,1) Value13;
Register<bit<32>,_>(MAX_OBJ,1) Value14;
Register<bit<32>,_>(MAX_OBJ,1) Value15;
Register<bit<32>,_>(MAX_OBJ,1) Value16;


//Register<bit<32>,_>(1,0) seq;
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
        //transition parse_netcache;
        transition select(hdr.udp.dstPort){
            NETCACHE_PORT0: parse_netcache;
            NETCACHE_PORT1: parse_netcache;
            NETCACHE_PORT2: parse_netcache;
            NETCACHE_PORT3: parse_netcache;
            NETCACHE_PORT4: parse_netcache;
            NETCACHE_PORT5: parse_netcache;
            NETCACHE_PORT6: parse_netcache;
            NETCACHE_PORT7: parse_netcache;
            NETCACHE_PORT8: parse_netcache;
            NETCACHE_PORT9: parse_netcache;
            NETCACHE_PORT10: parse_netcache;
            NETCACHE_PORT11: parse_netcache;
            NETCACHE_PORT12: parse_netcache;
            NETCACHE_PORT13: parse_netcache;
            NETCACHE_PORT14: parse_netcache;
            NETCACHE_PORT15: parse_netcache;
            default: parse_udp2;
        }
    }

    state parse_udp2 {
        //transition parse_netcache;
        transition select(hdr.udp.srcPort){
            NETCACHE_PORT0: parse_netcache;
            NETCACHE_PORT1: parse_netcache;
            NETCACHE_PORT2: parse_netcache;
            NETCACHE_PORT3: parse_netcache;
            NETCACHE_PORT4: parse_netcache;
            NETCACHE_PORT5: parse_netcache;
            NETCACHE_PORT6: parse_netcache;
            NETCACHE_PORT7: parse_netcache;
            NETCACHE_PORT8: parse_netcache;
            NETCACHE_PORT9: parse_netcache;
            NETCACHE_PORT10: parse_netcache;
            NETCACHE_PORT11: parse_netcache;
            NETCACHE_PORT12: parse_netcache;
            NETCACHE_PORT13: parse_netcache;
            NETCACHE_PORT14: parse_netcache;
            NETCACHE_PORT15: parse_netcache;
            default: accept;
        }
    }
        state parse_netcache {
        pkt.extract(hdr.netcache);
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


    //@pragma stage 6
    table ipv4_exact {
        key = {
            hdr.ipv4.dstAddr: exact;
        }
        actions = {
            ipv4_forward;
            drop;
            NoAction;
        }
        size = 16;
       default_action = drop();
    }


    action read_cache_hit_action(bit<32> idx){
        ig_md.isCached = YES;
        ig_md.cache_idx = idx;
    }

    action read_cache_miss_action(){
        ig_md.isCached = NO;
    }

    //@pragma stage 0
    table read_cache_table{
        key = {
            hdr.netcache.hkey_hi: exact;
            hdr.netcache.hkey_lo: exact;
        }
        actions = {
            read_cache_hit_action;
            read_cache_miss_action;
            NoAction;
            drop;
        }
        size = 65536;
        default_action = read_cache_miss_action;
    }

     RegisterAction<bit<8>, _, bit<8>>(cache_state) valid_cache_state = {
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
            reg_value = VALID;
        }
    };

     RegisterAction<bit<8>, _, bit<8>>(cache_state) invalid_cache_state = {
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
            reg_value = INVALID;
        }
    };

     RegisterAction<bit<8>, _, bit<8>>(cache_state) read_cache_state = {
        void apply(inout bit<8> reg_value, out bit<8> return_value) {
            return_value = reg_value;
        }
    };


     action read_cache_state_action(){
        ig_md.cache_state = read_cache_state.execute(ig_md.cache_idx);
    }

    //@pragma stage 2
    table read_cache_state_table{
        actions = {
            read_cache_state_action;
        }
        size = 1;
        default_action = read_cache_state_action;
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



    action invalid_cache_state_action(){
        invalid_cache_state.execute(ig_md.cache_idx);

    }

    table invalid_cache_state_table {
        actions = {
            invalid_cache_state_action;
        }
        size = 1;
        default_action = invalid_cache_state_action;
    }

    action valid_cache_state_action(){
        valid_cache_state.execute(ig_md.cache_idx);
    }

    table valid_cache_state_table {
        actions = {
            valid_cache_state_action;
        }
        size = 1;
        default_action = valid_cache_state_action;
    }




     RegisterAction<bit<32>, _, bit<32>>(cache_counter) inc_cache_counter = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                reg_value = reg_value + 1;

        }
    };

    action inc_cache_counter_action(){
        inc_cache_counter.execute(ig_md.cache_idx);
    }

    table inc_cache_counter_table{
        actions = {
            inc_cache_counter_action;
        }
        size = 1;
        default_action = inc_cache_counter_action;
    }

     RegisterAction<bit<32>, _, bit<32>>(cache_counter) dec_cache_counter = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
                if(reg_value > 0) reg_value = reg_value - 1; // -1로 만들면 안되므로..

        }
    };

    action dec_cache_counter_action(){
        dec_cache_counter.execute(ig_md.cache_idx);
    }

    table dec_cache_counter_table{
        actions = {
            dec_cache_counter_action;
        }
        size = 1;
        default_action = dec_cache_counter_action;
    }


    RegisterAction<bit<32>, _, bit<32>>(Value4) get_Value4 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    RegisterAction<bit<32>, _, bit<32>>(Value3) get_Value3 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };



    RegisterAction<bit<32>, _, bit<32>>(Value1) get_Value1 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value1_action(){
        hdr.value1.val1 = get_Value1.execute(ig_md.cache_idx);
    }


    table get_value1_table{
        actions = {
            get_Value1_action;
        }
        size = 1;
        default_action = get_Value1_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value2) get_Value2 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value2_action(){
        hdr.value1.val2 = get_Value2.execute(ig_md.cache_idx);
    }

    table get_value2_table{
        actions = {
            get_Value2_action;
        }
        size = 1;
        default_action = get_Value2_action;
    }

    action get_Value3_action(){
        hdr.value1.val3 = get_Value3.execute(ig_md.cache_idx);
    }

    table get_value3_table{
        actions = {
            get_Value3_action;
        }
        size = 1;
        default_action = get_Value3_action;
    }

    action get_Value4_action(){
        hdr.value1.val4 = get_Value4.execute(ig_md.cache_idx);
    }

    table get_value4_table{
        actions = {
            get_Value4_action;
        }
        size = 1;
        default_action = get_Value4_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value1) update_Value1 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.value1.val1;
        }
    };

    action update_Value1_action(){
        update_Value1.execute(ig_md.cache_idx);
    }

    table update_value1_table{
        actions = {
            update_Value1_action;
        }
        size = 1;
        default_action = update_Value1_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value2) update_Value2 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.value1.val2;
        }
    };

    action update_Value2_action(){
        update_Value2.execute(ig_md.cache_idx);
    }

    table update_value2_table{
        actions = {
            update_Value2_action;
        }
        size = 1;
        default_action = update_Value2_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value3) update_Value3 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.value1.val3;
        }
    };

    action update_Value3_action(){
        update_Value3.execute(ig_md.cache_idx);
    }

    table update_value3_table{
        actions = {
            update_Value3_action;
        }
        size = 1;
        default_action = update_Value3_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value4) update_Value4 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.value1.val4;
        }
    };

    action update_Value4_action(){
        update_Value4.execute(ig_md.cache_idx);
    }

    table update_value4_table{
        actions = {
            update_Value4_action;
        }
        size = 1;
        default_action = update_Value4_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value5) update_Value5 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.value2.val1;
        }
    };

    action update_Value5_action(){
        update_Value5.execute(ig_md.cache_idx);
    }

    table update_value5_table{
        actions = {
            update_Value5_action;
        }
        size = 1;
        default_action = update_Value5_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value6) update_Value6 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.value2.val2;
        }
    };

    action update_Value6_action(){
        update_Value6.execute(ig_md.cache_idx);
    }

    table update_value6_table{
        actions = {
            update_Value6_action;
        }
        size = 1;
        default_action = update_Value6_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value7) update_Value7 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.value2.val3;
        }
    };

    action update_Value7_action(){
        update_Value7.execute(ig_md.cache_idx);
    }

    table update_value7_table{
        actions = {
            update_Value7_action;
        }
        size = 1;
        default_action = update_Value7_action;
    }
    RegisterAction<bit<32>, _, bit<32>>(Value8) update_Value8 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            reg_value = hdr.value2.val4;
        }
    };

    action update_Value8_action(){
        update_Value8.execute(ig_md.cache_idx);
    }

    table update_value8_table{
        actions = {
            update_Value8_action;
        }
        size = 1;
        default_action = update_Value8_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value5) get_Value5 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value5_action(){
        hdr.value2.val1 = get_Value5.execute(ig_md.cache_idx);
    }

    table get_value5_table{
        actions = {
            get_Value5_action;
        }
        size = 1;
        default_action = get_Value5_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value6) get_Value6 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value6_action(){
        hdr.value2.val2 = get_Value6.execute(ig_md.cache_idx);
    }

    table get_value6_table{
        actions = {
            get_Value6_action;
        }
        size = 1;
        default_action = get_Value6_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value7) get_Value7 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value7_action(){
        hdr.value2.val3 = get_Value7.execute(ig_md.cache_idx);
    }

    table get_value7_table{
        actions = {
            get_Value7_action;
        }
        size = 1;
        default_action = get_Value7_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value8) get_Value8 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value8_action(){
        hdr.value2.val4 = get_Value8.execute(ig_md.cache_idx);
    }

    table get_value8_table{
        actions = {
            get_Value8_action;
        }
        size = 1;
        default_action = get_Value8_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value9) get_Value9 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value9_action(){
        hdr.value3.val1 = get_Value9.execute(ig_md.cache_idx);
    }

    table get_Value9_table{
        actions = {
            get_Value9_action;
        }
        size = 1;
        default_action = get_Value9_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value10) get_Value10 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value10_action(){
        hdr.value3.val2 = get_Value10.execute(ig_md.cache_idx);
    }

    table get_Value10_table{
        actions = {
            get_Value10_action;
        }
        size = 1;
        default_action = get_Value10_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value11) get_Value11 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value11_action(){
        hdr.value3.val3 = get_Value11.execute(ig_md.cache_idx);
    }

    table get_Value11_table{
        actions = {
            get_Value11_action;
        }
        size = 1;
        default_action = get_Value11_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value12) get_Value12 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value12_action(){
        hdr.value3.val4 = get_Value12.execute(ig_md.cache_idx);
    }

    table get_Value12_table{
        actions = {
            get_Value12_action;
        }
        size = 1;
        default_action = get_Value12_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value13) get_Value13 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value13_action(){
        hdr.value4.val1 = get_Value13.execute(ig_md.cache_idx);
    }

    table get_Value13_table{
        actions = {
            get_Value13_action;
        }
        size = 1;
        default_action = get_Value13_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value14) get_Value14 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value14_action(){
        hdr.value4.val2 = get_Value14.execute(ig_md.cache_idx);
    }

    table get_Value14_table{
        actions = {
            get_Value14_action;
        }
        size = 1;
        default_action = get_Value14_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value15) get_Value15 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value15_action(){
        hdr.value4.val3 = get_Value15.execute(ig_md.cache_idx);
    }

    table get_Value15_table{
        actions = {
            get_Value15_action;
        }
        size = 1;
        default_action = get_Value15_action;
    }

    RegisterAction<bit<32>, _, bit<32>>(Value16) get_Value16 = {
        void apply(inout bit<32> reg_value, out bit<32> return_value) {
            bit<32> temp = reg_value;
            reg_value = EMPTY;
            return_value = temp;

        }
    };

    action get_Value16_action(){
        hdr.value4.val4 = get_Value16.execute(ig_md.cache_idx);
    }

    table get_Value16_table{
        actions = {
            get_Value16_action;
        }
        size = 1;
        default_action = get_Value16_action;
    }

RegisterAction<bit<32>, _, bit<32>>(Value9) update_Value9 = {
    void apply(inout bit<32> reg_value, out bit<32> return_value) {
        reg_value = hdr.value3.val1;
    }
};

action update_Value9_action(){
    update_Value9.execute(ig_md.cache_idx);
}

table update_value9_table{
    actions = {
        update_Value9_action;
    }
    size = 1;
    default_action = update_Value9_action;
}

RegisterAction<bit<32>, _, bit<32>>(Value10) update_Value10 = {
    void apply(inout bit<32> reg_value, out bit<32> return_value) {
        reg_value = hdr.value3.val2;
    }
};

action update_Value10_action(){
    update_Value10.execute(ig_md.cache_idx);
}

table update_value10_table{
    actions = {
        update_Value10_action;
    }
    size = 1;
    default_action = update_Value10_action;
}

RegisterAction<bit<32>, _, bit<32>>(Value11) update_Value11 = {
    void apply(inout bit<32> reg_value, out bit<32> return_value) {
        reg_value = hdr.value3.val3;
    }
};

action update_Value11_action(){
    update_Value11.execute(ig_md.cache_idx);
}

table update_value11_table{
    actions = {
        update_Value11_action;
    }
    size = 1;
    default_action = update_Value11_action;
}

RegisterAction<bit<32>, _, bit<32>>(Value12) update_Value12 = {
    void apply(inout bit<32> reg_value, out bit<32> return_value) {
        reg_value = hdr.value3.val4;
    }
};

action update_Value12_action(){
    update_Value12.execute(ig_md.cache_idx);
}

table update_value12_table{
    actions = {
        update_Value12_action;
    }
    size = 1;
    default_action = update_Value12_action;
}

RegisterAction<bit<32>, _, bit<32>>(Value13) update_Value13 = {
    void apply(inout bit<32> reg_value, out bit<32> return_value) {
        reg_value = hdr.value4.val1;
    }
};

action update_Value13_action(){
    update_Value13.execute(ig_md.cache_idx);
}

table update_value13_table{
    actions = {
        update_Value13_action;
    }
    size = 1;
    default_action = update_Value13_action;
}

RegisterAction<bit<32>, _, bit<32>>(Value14) update_Value14 = {
    void apply(inout bit<32> reg_value, out bit<32> return_value) {
        reg_value = hdr.value4.val2;
    }
};

action update_Value14_action(){
    update_Value14.execute(ig_md.cache_idx);
}

table update_value14_table{
    actions = {
        update_Value14_action;
    }
    size = 1;
    default_action = update_Value14_action;
}

RegisterAction<bit<32>, _, bit<32>>(Value15) update_Value15 = {
    void apply(inout bit<32> reg_value, out bit<32> return_value) {
        reg_value = hdr.value4.val3;
    }
};

action update_Value15_action(){
    update_Value15.execute(ig_md.cache_idx);
}

table update_value15_table{
    actions = {
        update_Value15_action;
    }
    size = 1;
    default_action = update_Value15_action;
}

RegisterAction<bit<32>, _, bit<32>>(Value16) update_Value16 = {
    void apply(inout bit<32> reg_value, out bit<32> return_value) {
        reg_value = hdr.value4.val4;
    }
};

action update_Value16_action(){
    update_Value16.execute(ig_md.cache_idx);
}

table update_value16_table{
    actions = {
        update_Value16_action;
    }
    size = 1;
    default_action = update_Value16_action;
}


    apply {
        /*************** NetCache Block START *****************************/
            if (hdr.netcache.isValid()) { //
                ig_md.cache_state = INVALID;
                hdr.udp.checksum = 0; // Disable UDP checksum.
                read_cache_table.apply();
                if(hdr.netcache.op == OP_READ || hdr.netcache.op == OP_R_REPLY){
                    if(ig_md.isCached == YES){
                        read_cache_state_table.apply();
                        inc_cache_counter_table.apply();
                    }
                    if(hdr.netcache.op == OP_READ){
                            if(ig_md.cache_state == VALID){
                                hdr.value1.setValid();
                                get_value1_table.apply();
                                get_value2_table.apply();
                                get_value3_table.apply();
                                get_value4_table.apply();

                                hdr.value2.setValid();
                                get_value5_table.apply();
                                get_value6_table.apply();
                                get_value7_table.apply();
                                get_value8_table.apply();

                                hdr.value3.setValid();
                                get_Value9_table.apply();
                                get_Value10_table.apply();
                                get_Value11_table.apply();
                                get_Value12_table.apply();

                                hdr.value4.setValid();
                                get_Value13_table.apply();
                                get_Value14_table.apply();
                                get_Value15_table.apply();
                                get_Value16_table.apply();

                                bit<32> temp = hdr.ipv4.dstAddr;
                                hdr.ipv4.dstAddr = hdr.ipv4.srcAddr;
                                hdr.ipv4.srcAddr = temp;
                                bit<16> temp2 = hdr.udp.dstPort;
                                hdr.udp.dstPort = hdr.udp.srcPort;
                                hdr.udp.srcPort = temp2;
                                bit<48> temp3 = hdr.ethernet.dstAddr;
                                hdr.ethernet.dstAddr = hdr.ethernet.srcAddr;
                                hdr.ethernet.srcAddr = temp3;
                                hdr.netcache.cached = 1;
                                hdr.netcache.op = OP_R_REPLY;

                                ipv4_exact.apply();
                            }
                            else if(ig_md.cache_state == INVALID){
                                ig_intr_dprsr_md.drop_ctl=0;
                                ipv4_exact.apply();
                            }
                    }
                    else if (hdr.netcache.op == OP_R_REPLY) { // 다른 loopback recirculation 포트
                        //hdr.udp.checksum = 0; // Disable UDP checksum.
                        ipv4_exact.apply();
                    }
                }
                else if(hdr.netcache.op == OP_WRITE){
                    if(ig_md.isCached == YES){
                        invalid_cache_state_table.apply();
                        hdr.netcache.op = OP_FETCH;
                    }
                    ipv4_exact.apply();
                }
                else if(hdr.netcache.op == OP_W_REPLY){
                    ipv4_exact.apply();
                }
                else if(hdr.netcache.op == OP_F_REPLY){
                    //if(ig_md.isCached == YES)
                    valid_cache_state_table.apply();
                    /*
                    hdr.value1.setValid();
                    update_value1_table.apply();
                    update_value2_table.apply();
                    update_value3_table.apply();
                    update_value4_table.apply();

                    hdr.value2.setValid();
                    update_value5_table.apply();
                    update_value6_table.apply();
                    update_value7_table.apply();
                    update_value8_table.apply();


                    hdr.value3.setValid();
                    update_value9_table.apply();
                    update_value10_table.apply();
                    update_value11_table.apply();
                    update_value12_table.apply();

                    hdr.value4.setValid();
                    update_value13_table.apply();
                    update_value14_table.apply();
                    update_value15_table.apply();
                    update_value16_table.apply();
                    */
                    ig_tm_md.ucast_egress_port = CONTROLLER_PORT;
                    ipv4_exact.apply();

                }
            }
            else ipv4_exact.apply(); // Normal, not netcache packets
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
