#!/bin/sh
# Degrade this container's own link with tc netem (T301). Runs INSIDE a tools/nettest/compose.yml node (it needs
# NET_ADMIN); tools/nettest/run.sh calls it with `docker compose exec`:
#
#   netem.sh <perfil>       replace the root qdisc of $NETEM_DEV (default eth0) with that profile
#   netem.sh quitar         remove it (back to no delay, no loss)
#   netem.sh mostrar        print the qdisc and its counters (tc -s), for the logs
#   netem.sh lista          print the profiles and what each one applies
#
# netem only acts on what leaves the interface: a profile on the server and on a bot delays both directions of the
# path between them (RTT = 2 x delay); a profile on a bot only, just what that bot sends. run.sh applies it to the
# three nodes unless a scenario says otherwise.
#
# Variables: NETEM_DEV=eth0; NETEM_SECO=1 prints the tc command instead of running it (a check without a kernel).
# Exit codes: 0 applied (or removed), 2 unknown profile or usage, 3 tc failed (usually: this kernel has no sch_netem).
set -eu
DEV=${NETEM_DEV:-eth0}

# profile -> the netem arguments ("" = no netem at all)
argumentos() {
    case $1 in
        lan)     echo "" ;;
        fibra)   echo "delay 20ms 4ms loss 0.2%" ;;
        normal)  echo "delay 40ms 10ms loss 1%" ;;
        wifi)    echo "delay 80ms 20ms loss 2%" ;;
        movil)   echo "delay 150ms 40ms loss 5%" ;;
        rafagas) echo "delay 60ms 15ms loss gemodel 1% 20% 70% 0.1%" ;;
        corte)   echo "loss 100%" ;;
        subida)  echo "delay 40ms 10ms loss 1% rate 1mbit" ;;  # (T303: "normal" plus a 1 Mbit/s uplink, on the server)
        *) return 1 ;;
    esac
}
PERFILES="lan fibra normal wifi movil rafagas corte subida"

tc_() {
    if [ "${NETEM_SECO:-0}" = 1 ]; then
        echo "tc $*"
    else
        tc "$@"
    fi
}

quitar() {
    # (no qdisc of ours: tc says "Cannot delete qdisc with handle of zero"; that is already the wanted state)
    tc_ qdisc del dev "$DEV" root 2>/dev/null || true
}

perfil=${1:-}
case $perfil in
    "")
        echo "uso: netem.sh <perfil>|quitar|mostrar|lista   (perfiles: $PERFILES)" >&2
        exit 2 ;;
    lista)
        for p in $PERFILES; do printf '%-8s %s\n' "$p" "$(argumentos "$p")"; done
        exit 0 ;;
    mostrar)
        tc_ -s qdisc show dev "$DEV"
        exit 0 ;;
    quitar)
        quitar
        echo "[netem] $(uname -n): $DEV sin perfil"
        exit 0 ;;
esac

if ! args=$(argumentos "$perfil"); then
    echo "[netem] perfil desconocido: $perfil (perfiles: $PERFILES)" >&2
    exit 2
fi
if [ -z "$args" ]; then
    quitar
    echo "[netem] $(uname -n): $DEV perfil $perfil (sin netem)"
    exit 0
fi
# shellcheck disable=SC2086  (the words of $args are tc arguments)
if ! tc_ qdisc replace dev "$DEV" root netem $args; then
    echo "[netem] $(uname -n): tc no pudo aplicar '$args' en $DEV (¿el kernel no tiene sch_netem? Ver README.md)" >&2
    exit 3
fi
echo "[netem] $(uname -n): $DEV perfil $perfil: netem $args"
