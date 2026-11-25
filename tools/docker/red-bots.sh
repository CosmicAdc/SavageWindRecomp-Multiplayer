#!/bin/bash
# Network test in Docker: a dedicated server and bots under latency and loss (T023). It runs INSIDE the swcoop-pruebas
# containers that tools/docker/compose.yml starts; it is not meant to be run on the host.
#
#   red-bots.sh servidor            build the app and the bot once, publish them in /compartido, run
#                                   `savagewind --server $PUERTO` (runtime/main.c, online_server_main)
#   red-bots.sh bot NOMBRE          add netem to eth0, then run `online_bot servidor $PUERTO NOMBRE`
#
# The app is built in /tmp/b (the swcoop-obj volume, so a second run only relinks) the way tools/linux/distro_build.sh
# does it: `make -j4 B=/tmp/b GEN=/nonexistent` builds only the app, no game code. The tree is mounted read-only in /src.
# Docker Desktop here has 6.2 GB: keep JOBS=4. The server builds once; the bots wait (compose healthcheck on
# /compartido/listo) and use the same binaries, so nothing is compiled three times.
#
# Variables (all optional):
#   PUERTO=52530      server port (the same one the server and the bots use)
#   JOBS=4            make -j
#   RED=...           the netem line; default "delay 80ms 20ms loss 2%". Empty: no netem
#   DURACION=45       a bot with no BOT_FREEZE leaves after that many seconds connected (exit 0); one that is
#                     disconnected before that exits 1. With --abort-on-container-exit this ends the run.
#   BOT_FREEZE=<s>    (tools/online_bot.c) the bot goes silent after s seconds, connection left open: the server
#                     drops it after TIMEOUT_NS (10 s). This role prints how long it took, then idles so the run
#                     doesn't stop at its expulsion.
#
# From Git Bash on Windows a path like /tmp/... in `docker run` needs MSYS_NO_PATHCONV=1 (otherwise MSYS rewrites it
# to C:/Program Files/Git/tmp/...). With compose that is only a problem if you add your own paths:
#   MSYS_NO_PATHCONV=1 docker compose -f tools/docker/compose.yml up --abort-on-container-exit
set -eu
PUERTO=${PUERTO:-52530}
JOBS=${JOBS:-4}
RED=${RED-delay 80ms 20ms loss 2%}
DURACION=${DURACION:-45}
COMPARTIDO=/compartido

servidor() {
    rm -f "$COMPARTIDO/listo"
    cd /src
    make -j"$JOBS" B=/tmp/b GEN=/nonexistent
    clang -O2 -o /tmp/online_bot tools/online_bot.c -lm
    cp /tmp/b/savagewind "$COMPARTIDO/savagewind"
    cp /tmp/online_bot "$COMPARTIDO/online_bot"
    touch "$COMPARTIDO/listo"
    echo "[red-bots] servidor: listo, puerto $PUERTO"
    exec "$COMPARTIDO/savagewind" --server "$PUERTO"
}

bot() {
    nombre=${1:?uso: red-bots.sh bot NOMBRE}
    [ -f "$COMPARTIDO/listo" ] || { echo "[red-bots] falta $COMPARTIDO/listo" >&2; exit 1; }
    if [ -n "$RED" ]; then
        # shellcheck disable=SC2086  (the words of $RED are tc arguments)
        # Docker Desktop's WSL2 kernel has no netem ("Specified qdisc kind is unknown"): say so and go on without it
        # (a Linux host, or a WSL kernel with sch_netem, applies it). RED_ESTRICTA=1 makes that an error instead.
        if tc qdisc add dev eth0 root netem $RED; then
            echo "[red-bots] $nombre: netem $RED"
        elif [ "${RED_ESTRICTA:-0}" = 1 ]; then
            echo "[red-bots] $nombre: netem no disponible y RED_ESTRICTA=1" >&2; exit 1
        else
            echo "[red-bots] $nombre: AVISO netem no disponible en este kernel: sin latencia ni pérdida simuladas" >&2
        fi
    fi
    # (the server listens a moment after the healthcheck sees the file: online_bot exits 1 when it can't connect)
    intento=0
    inicio=$(date +%s)
    while :; do
        rc=0
        if [ -n "${BOT_FREEZE:-}" ]; then
            "$COMPARTIDO/online_bot" servidor "$PUERTO" "$nombre" || rc=$?
        else
            timeout "$DURACION" "$COMPARTIDO/online_bot" servidor "$PUERTO" "$nombre" || rc=$?
        fi
        [ "$rc" = 1 ] && [ "$intento" -lt 15 ] && [ $(($(date +%s) - inicio)) -lt 15 ] || break
        intento=$((intento + 1))
        sleep 1
    done
    fin=$(date +%s)
    if [ -n "${BOT_FREEZE:-}" ]; then
        echo "[red-bots] $nombre: el servidor lo cerro tras $((fin - inicio)) s (BOT_FREEZE=$BOT_FREEZE, codigo $rc)"
        exec sleep infinity  # (staying up: --abort-on-container-exit would stop the others)
    fi
    if [ "$rc" = 124 ]; then
        echo "[red-bots] $nombre: sigue conectado tras $DURACION s"
        exit 0
    fi
    echo "[red-bots] $nombre: terminado antes de tiempo (codigo $rc, $((fin - inicio)) s)" >&2
    exit 1
}

case ${1:-} in
    servidor) servidor ;;
    bot) shift; bot "$@" ;;
    *) echo "uso: red-bots.sh servidor | bot NOMBRE" >&2; exit 2 ;;
esac
