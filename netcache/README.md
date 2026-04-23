# NetCache reference implementation

This directory contains a minimal NetCache-style switch cache built on
the same testbed and packet layout as OrbitCache.  It is not a faithful
reimplementation of NetCache (SOSP '17).  The cache lookup table and
the 64-byte value memory both live in the Tofino ingress pipeline, so
hits are served from the switch without any recirculation and without
CPU involvement.  We provide it so OrbitCache's comparison numbers can
be reproduced end-to-end with a single switch ASIC and a single set of
client/server binaries.

## Files

| File | Purpose |
|------|---------|
| `netcache.p4`   | P4 data plane: parser, match-action tables, registers for cached values, routing. |
| `controller.py` | Switch control plane: installs routing, clears and populates `read_cache_table` + `cache_state` from a `gen_keys_<N>_<LR>.txt` file. |

The value distribution (`gen_keys_<N>_<LR>.txt`) is expected to be
generated outside this repository and placed alongside `controller.py`
before running it.  `N` is the number of cached items and `LR` is the
percentage of large-value items in the sample; the file lists one
zero-based item rank per line for every key that should be installed
in the switch cache.

## Experiment workflow (switch side)

1. Open three terminals on the switch control plane.

2. Terminal 1 — start the NetCache P4 program:
   ```bash
   run_switchd.sh -p netcache
   ```

3. Terminal 2 — enable 100G ports with `run_bfshell.sh`, then `ucli` and
   `pm`.  Create each port with `port-add #/- 100G NONE` and enable it
   with `port-enb #/-`, then disable autoneg with `an-set -/- 2`.

4. Terminal 3 — run the controller to install routing and cache
   entries:
   ```bash
   python3 controller.py                     # default NUM_HOTKEY=10000, LRATIO=88
   python3 controller.py <NUM_HOTKEY> <LRATIO>
   ```
   The controller reads `gen_keys_<NUM_HOTKEY>_<LRATIO>.txt`, clears
   `read_cache_table` + `cache_state`, installs one entry per listed
   rank (hashed key to register index), and writes the `ipv4_exact`
   routing table from the `ip_list` / `port_list` / `mac_list` arrays
   near the top of `controller.py`.  Expected tail output:
   ```
   <number of cached keys>
   Done!
   ```

Re-run `controller.py` whenever you change the cached set
(`NUM_HOTKEY`, `LRATIO`, or the underlying `gen_keys_*.txt`).  No
server-side preload is needed.  NetCache hits are serviced directly
by the switch pipeline using the installed register contents.

## Experiment workflow (client / server side)

Clients and servers reuse the top-level `client.out` and `server.out`
from the parent directory.  Pass `PROTOCOL_ID=2` (NetCache) to both.

```bash
LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./server.out 2
LD_PRELOAD=libvma.so VMA_THREAD_MODE=2 ./client.out 2 3 10 1000000 0
```

UDP ports for this pipeline land in `[NETCACHE_BASE_PORT,
NETCACHE_BASE_PORT+63]` (default `[1000, 1063]`).  The P4 parser
dispatches packets into the NetCache pipeline when either `dstPort` or
`srcPort` falls in that range, so multi-worker clients and servers can
bind each worker to a unique local port without `SO_REUSEPORT`.

## Adapting to your cluster

Before running the controller:

- Adjust `NUM_SRV_CTRL` and the `ip_list` / `port_list` / `pipe_list` /
  `mac_list` arrays in `controller.py` to match the client/server
  nodes and switch ports actually in use.
- Adjust `#define MAX_SRV` in `netcache.p4` if more than 64 endpoints
  participate.

## Scope

This directory covers only the NetCache pipeline.  Host-side code,
packet layout, and general deployment notes live in the top-level
[`README.md`](../README.md).
