#!/bin/sh
# Scenario "lan" (T323): LAN discovery (runtime/online_discovery.c) in Docker.
#   same network      host + client on one Docker network -> the client lists exactly 1 game in < 2 s
#   different network host and client on two networks     -> the client lists 0 games
# Run it on the HOST (Git Bash / Linux) from the repository root, normally as `sh tools/nettest/run.sh lan` (T301);
# it also runs by itself: `MSYS_NO_PATHCONV=1 sh tools/nettest/scenarios/lan.sh`.
# Needs the image swcoop-pruebas (docker build -f tools/docker/Dockerfile.pruebas -t swcoop-pruebas .). It needs no
# NET_ADMIN, no netem and no game files; nothing is published to the host. It creates and removes its own networks and
# containers (names start with swlan-). The probe program is written at run time into a temporary folder.
# Prints "lan: OK" and exits 0, or says what failed and exits 1.
set -u
cd "$(dirname "$0")/../../.." || exit 1
IMAGE=${IMAGE:-swcoop-pruebas}
export MSYS_NO_PATHCONV=1
TMP=$(mktemp -d "${TMPDIR:-/tmp}/swlan.XXXXXX") || exit 1
NET_A=swlan-a
NET_B=swlan-b

cleanup() {
    docker rm -f swlan-host swlan-client >/dev/null 2>&1
    docker network rm "$NET_A" "$NET_B" >/dev/null 2>&1
    rm -rf "$TMP"
}
trap cleanup EXIT INT TERM
fail() { echo "lan: FAIL: $*"; exit 1; }

# The probe: "host" answers queries for 25 s; "scan" scans and prints "lan-result n=<results> first_ms=<ms>"
# (first_ms -1 when none) after 2 s, then lists them. It only uses the public functions of the module.
cat > "$TMP/probe.c" <<'C'
#include "online_discovery.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static long ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static void nap(void) { struct timespec t = {0, 10000000L}; nanosleep(&t, NULL); }
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "host")) {
        DiscoveryInfo i;
        memset(&i, 0, sizeof i);
        i.protocol = 38, i.tcp_port = 52520, i.players = 1, i.max_players = 2;
        snprintf(i.name, sizeof i.name, "LanHost");
        if (!discovery_host_open(DISCOVERY_PORT)) { puts("probe: host_open failed"); return 1; }
        puts("probe: host ready");
        fflush(stdout);
        long end = ms() + 25000;
        while (ms() < end) { discovery_host_poll(&i); nap(); }
        discovery_host_close();
        return 0;
    }
    DiscoveryResult r[DISCOVERY_MAX_RESULTS];
    long t0 = ms(), first = -1;
    if (!discovery_scan_start(38)) { puts("probe: scan_start failed"); return 1; }
    int n = 0;
    while (ms() - t0 < 2000) {
        n = discovery_scan_results(r, DISCOVERY_MAX_RESULTS);
        if (n > 0 && first < 0) first = ms() - t0;
        nap();
    }
    n = discovery_scan_results(r, DISCOVERY_MAX_RESULTS);
    printf("lan-result n=%d first_ms=%ld\n", n, first);
    for (int k = 0; k < n; k++) printf("lan-found %s:%u \"%s\" %u/%u\n", r[k].address, r[k].info.tcp_port, r[k].info.name, r[k].info.players, r[k].info.max_players);
    discovery_scan_stop();
    return 0;
}
C

docker image inspect "$IMAGE" >/dev/null 2>&1 || fail "image $IMAGE missing (docker build -f tools/docker/Dockerfile.pruebas -t $IMAGE .)"
docker network create "$NET_A" >/dev/null || fail "cannot create network $NET_A"
docker network create "$NET_B" >/dev/null || fail "cannot create network $NET_B"

# Same compile line everywhere: the tree is read-only, the probe goes to /tmp in the container.
BUILD='clang -Wall -Iruntime /probe/probe.c runtime/online_discovery.c -o /tmp/probe'
run_host() { docker run -d --name swlan-host --network "$1" -v "$PWD":/src:ro -v "$TMP":/probe:ro -w /src "$IMAGE" sh -c "$BUILD && exec /tmp/probe host" >/dev/null; }
run_client() { docker run --rm --name swlan-client --network "$1" -v "$PWD":/src:ro -v "$TMP":/probe:ro -w /src "$IMAGE" sh -c "$BUILD && exec /tmp/probe scan"; }
wait_host() {  # the host prints "host ready" once it listens (or the compile failed)
    for _ in $(seq 1 60); do
        docker logs swlan-host 2>&1 | grep -q "probe: host ready" && return 0
        [ "$(docker inspect -f '{{.State.Running}}' swlan-host 2>/dev/null)" = true ] || break
        sleep 1
    done
    docker logs swlan-host 2>&1 | tail -n 20
    return 1
}
count_of() { echo "$1" | sed -n 's/.*lan-result n=\([0-9]*\) first_ms=\(-*[0-9]*\).*/\1/p' | head -n 1; }
first_of() { echo "$1" | sed -n 's/.*lan-result n=\([0-9]*\) first_ms=\(-*[0-9]*\).*/\2/p' | head -n 1; }

# 1. same network: 1 result in < 2 s
run_host "$NET_A" || fail "cannot start the host container"
wait_host || fail "the host probe did not start"
OUT=$(run_client "$NET_A") || { echo "$OUT"; fail "the client probe failed"; }
echo "$OUT"
N=$(count_of "$OUT"); F=$(first_of "$OUT")
[ "${N:-x}" = 1 ] || fail "same network: expected 1 game, got '${N:-nothing}'"
[ "${F:-x}" -ge 0 ] 2>/dev/null && [ "$F" -lt 2000 ] || fail "same network: first result after '${F:-never}' ms (expected < 2000)"
echo "lan: same network -> 1 game in ${F} ms"
docker rm -f swlan-host >/dev/null 2>&1

# 2. different networks: 0 results
run_host "$NET_A" || fail "cannot restart the host container"
wait_host || fail "the host probe did not start (second run)"
OUT=$(run_client "$NET_B") || { echo "$OUT"; fail "the client probe failed (other network)"; }
echo "$OUT"
N=$(count_of "$OUT")
[ "${N:-x}" = 0 ] || fail "different networks: expected 0 games, got '${N:-nothing}'"
echo "lan: different networks -> 0 games"

echo "lan: OK"
