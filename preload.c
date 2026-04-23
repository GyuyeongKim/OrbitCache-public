/*
 * preload.c — standalone cache install tool for OrbitCache.
 *
 * Sends NUM_HOTKEY install packets through the programmable switch so
 * that the switch's cacheLookup_table and recirculation loop are seeded
 * with fresh cache packets before a measurement run.  The install path
 * is triggered by binding the source UDP port to ORBITCACHE_BASE_PORT,
 * which matches the P4 parser's srcPort range for the OrbitCache
 * pipeline.  Each packet carries op=OP_W_REPLY with flag=FETCHING and
 * cached=1, the same layout the previous server-embedded preload used.
 *
 * Usage:
 *   ./preload.out                       # NUM_HOTKEY items, target = dst_ip[0]
 *   ./preload.out <N>                   # N items, default target
 *   ./preload.out <N> <target_ip>       # N items, custom target IP
 */
#include "header.h"

#define PRELOAD_PER_PKT_US 10  // pacing between sends to mirror the
                                // original server-embedded preload rate

int main(int argc, char *argv[]) {
    int N = NUM_HOTKEY;
    const char *target_ip = dst_ip[0];

    if (argc >= 2) {
        N = atoi(argv[1]);
        if (N <= 0) {
            fprintf(stderr, "preload: invalid N=%s\n", argv[1]);
            return 1;
        }
    }
    if (argc >= 3) {
        target_ip = argv[2];
    }
    if (argc > 3) {
        fprintf(stderr, "Usage: %s [N] [target_ip]\n", argv[0]);
        return 1;
    }

    printf("[preload] installing %d items (small=%dB, large=%dB, "
           "LRATIO=%d%%) via %s:%d\n",
           N, SMALL_VALUE, LARGE_VALUE, LRATIO,
           target_ip, ORBITCACHE_BASE_PORT + 1);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("preload: socket");
        return 1;
    }

    // Bind src UDP port to ORBITCACHE_BASE_PORT so the switch parser
    // routes these packets down the OrbitCache pipeline and onto the
    // recirculation loop.
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    struct sockaddr_in src;
    memset(&src, 0, sizeof(src));
    src.sin_family = AF_INET;
    src.sin_addr.s_addr = htonl(INADDR_ANY);
    src.sin_port = htons((uint16_t)ORBITCACHE_BASE_PORT);
    if (bind(sock, (struct sockaddr *)&src, sizeof(src)) < 0) {
        perror("preload: bind src port");
        close(sock);
        return 1;
    }

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    // Destination port is irrelevant for the install path — the switch
    // parser matches on srcPort.  Pick BASE_PORT+1 so it does not also
    // match the dstPort branch.
    dst.sin_port = htons((uint16_t)(ORBITCACHE_BASE_PORT + 1));
    if (inet_pton(AF_INET, target_ip, &dst.sin_addr) != 1) {
        fprintf(stderr, "preload: bad target_ip %s\n", target_ip);
        close(sock);
        return 1;
    }

    struct orbitcache_hdr pkt;
    int n_small = 0, n_large = 0;

    for (uint32_t i = 0; i < (uint32_t)N; i++) {
        memset(&pkt, 0, sizeof(pkt));
        pkt.op     = OP_W_REPLY;
        pkt.flag   = FETCHING;
        pkt.cached = 1;

        tostring_key(i, pkt.org_key);
        pkt.hkey_hi = htonll(hash64_str(pkt.org_key));
        pkt.hkey_lo = htonll(hash64_str2(pkt.org_key));

        int value_size = (((double)i / (double)N) * 100 >= LRATIO)
                             ? SMALL_VALUE
                             : LARGE_VALUE;
        if (value_size == SMALL_VALUE) n_small++;
        else                           n_large++;

        memset(pkt.value, 0, value_size);
        size_t pkt_size = sizeof(pkt) - MAX_VALUE_SIZE + value_size;

        if (sendto(sock, &pkt, pkt_size, 0,
                   (struct sockaddr *)&dst, sizeof(dst)) < 0) {
            perror("preload: sendto");
            close(sock);
            return 1;
        }
        if (PRELOAD_PER_PKT_US > 0) usleep(PRELOAD_PER_PKT_US);
    }

    printf("[preload] done: %d items (small=%d, large=%d)\n",
           N, n_small, n_large);

    close(sock);
    return 0;
}
