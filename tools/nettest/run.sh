#!/bin/sh
# Network test bench runner (T301). Run it on the HOST (Git Bash on Windows, or Linux), from anywhere:
#
#   sh tools/nettest/run.sh <escenario>       runs tools/nettest/scenarios/<escenario>.sh; exit 0 and
#                                             "<escenario>: OK" when it passes, exit 1 and "<escenario>: FALLO" if not
#   sh tools/nettest/run.sh                   lists the scenarios
#
# What it does, in order (README.md has the details):
#   1. builds the images if missing (swcoop-pruebas from tools/docker/Dockerfile.pruebas, then swcoop-nettest) and
#      the binaries (compose service `build`: the app without game code, and tools/online_bot.c), incrementally
#   2. starts the three idle nodes of tools/nettest/compose.yml (server, bot_a, bot_b)
#   3. applies the scenario's netem profile (PERFIL) to the nodes in NETEM_EN (tools/nettest/netem.sh)
#   4. starts `savagewind --server` in the server node and waits until it listens
#   5. calls the scenario's function `escenario` (it starts the bots, waits, and checks); its status is the result
#   6. collects the logs in build/nettest/<escenario>/, removes netem and takes the nodes down
#
# A scenario is a file tools/nettest/scenarios/<nombre>.sh that this script sources: it may set PERFIL (default lan)
# and NETEM_EN with a plain assignment (the same variables in the environment win), and must define the function
# `escenario`, which returns 0 when it passes. It can use these helpers:
#   nt_log TEXTO...                         print and append to run.log
#   nt_netem PERFIL [NODO...]               apply a profile now (default: the nodes in NETEM_EN); 1 if tc failed
#   nt_bot NODO NOMBRE SEGUNDOS [VAR=v...]  start tools/online_bot.c in that node for that many seconds (in the
#                                           background, with those environment variables); its output goes to
#                                           $NT_DIR/<nodo>.log
#   nt_esperar_bots                         wait for every bot started; each exit status in $NT_DIR/<nodo>.rc
#                                           (124: still connected when its time ran out; 0: the server closed it;
#                                           1: could not connect or was refused)
#   nt_total_state LOG                      STATE messages from the others the bot counted (its last "bot: stats" line)
# and these variables: NT_DIR (the output folder), NT_PUERTO.
#
# Variables (all optional):
#   PERFIL=<perfil>        override the scenario's profile (tools/nettest/netem.sh lista)
#   NETEM_EN="server bot_a bot_b"   where the profile goes (netem acts on what leaves each node)
#   NETEM_OPCIONAL=1       if this kernel has no sch_netem, go on without it (the result says "SIN netem"); by
#                          default that is a failure, because the scenario would not measure what it says
#   NT_PUERTO=52530        server port inside the compose network (nothing is published to the host)
#   JOBS=4                 make -j for the build (Docker Desktop here has 6.2 GB)
#   NT_SIN_CONSTRUIR=1     skip step 1 (images and binaries must exist)
#   NT_MANTENER=1          leave the nodes up at the end (netem removed); `docker compose -f
#                          tools/nettest/compose.yml down` afterwards
#   NT_LIMPIAR=1           at the end also drop the volume with the build (the next run compiles from scratch)
set -u
cd "$(dirname "$0")/../.." || exit 1
# (Git Bash would rewrite /opt/... and /tmp/... arguments of docker into Windows paths)
MSYS_NO_PATHCONV=1
MSYS2_ARG_CONV_EXCL='*'
export MSYS_NO_PATHCONV MSYS2_ARG_CONV_EXCL

NT_COMPOSE=tools/nettest/compose.yml
NT_PUERTO=${NT_PUERTO:-52530}
NT_NODOS="server bot_a bot_b"
NT_BOTS=""        # "nodo:pid ..." of the bots started by nt_bot
NT_ARRIBA=0       # the nodes are up (the cleanup takes them down)
NT_RECOGIDO=0
NT_SIN_NETEM=0

nombre=${1:-}
if [ -z "$nombre" ]; then
    echo "uso: sh tools/nettest/run.sh <escenario>" >&2
    echo "escenarios:" >&2
    for f in tools/nettest/scenarios/*.sh; do [ -f "$f" ] && echo "  $(basename "$f" .sh)" >&2; done
    exit 2
fi
case $nombre in
    *[!a-z0-9_-]*) echo "[nettest] nombre de escenario no válido: $nombre (solo a-z 0-9 _ -)" >&2; exit 2 ;;
esac
NT_ESCENARIO=tools/nettest/scenarios/$nombre.sh
if [ ! -f "$NT_ESCENARIO" ]; then
    echo "[nettest] no existe $NT_ESCENARIO" >&2
    exit 2
fi
command -v docker >/dev/null 2>&1 || { echo "[nettest] falta docker" >&2; exit 1; }

NT_DIR=build/nettest/$nombre
rm -rf "$NT_DIR"
mkdir -p "$NT_DIR"

dc() { docker compose -f "$NT_COMPOSE" "$@"; }

nt_log() {
    echo "[nettest] $*"
    echo "[nettest] $*" >> "$NT_DIR/run.log"
}

nt_netem() {
    _perfil=$1
    shift
    _nodos=${*:-$NETEM_EN}
    _fallo=0
    for _n in $_nodos; do
        dc exec -T "$_n" sh /src/tools/nettest/netem.sh "$_perfil" >> "$NT_DIR/netem.log" 2>&1
        _rc=$?
        if [ "$_rc" = 0 ]; then
            nt_log "netem: $_n perfil $_perfil"
        elif [ "$_rc" = 3 ] && [ "${NETEM_OPCIONAL:-0}" = 1 ]; then
            NT_SIN_NETEM=1
            nt_log "AVISO netem: $_n no pudo aplicar $_perfil (sin sch_netem); NETEM_OPCIONAL=1: se sigue SIN netem"
        else
            nt_log "netem: $_n no pudo aplicar $_perfil (código $_rc; ver $NT_DIR/netem.log)"
            _fallo=1
        fi
    done
    return $_fallo
}

# The dedicated server (runtime/main.c --server, runtime/online.c online_server_main). Without a terminal the app
# writes its log to $SAVAGEWIND_DATA_ROOT/savagewind.log instead of stderr (runtime/platform/linux.c log_to_file), so
# the data root is a folder of its own in the node and the log is read from there.
nt_servidor() {
    # (arranque.log: what is said before the app opens its log, e.g. a shared library the loader can't find)
    dc exec -d server sh -c "mkdir -p /tmp/sw && export SAVAGEWIND_DATA_ROOT=/tmp/sw && \
        exec /opt/nt/bin/savagewind --server $NT_PUERTO > /tmp/sw/arranque.log 2>&1" >> "$NT_DIR/run.log" 2>&1 || return 1
    _i=0
    while [ "$_i" -lt 30 ]; do
        if dc exec -T server grep -q "dedicated server on port" /tmp/sw/savagewind.log 2>/dev/null; then
            nt_log "servidor escuchando en server:$NT_PUERTO"
            return 0
        fi
        _i=$((_i + 1))
        sleep 1
    done
    nt_log "el servidor no dijo 'dedicated server on port' en 30 s (ver $NT_DIR/server.log; README.md: plan B)"
    return 1
}

nt_bot() {
    _n=$1 _b=$2 _s=$3
    shift 3
    dc exec -T "$_n" env "$@" timeout "$_s" /opt/nt/bin/online_bot server "$NT_PUERTO" "$_b" \
        > "$NT_DIR/$_n.log" 2>&1 &
    NT_BOTS="$NT_BOTS $_n:$!"
    nt_log "bot $_b en $_n durante $_s s${*:+ con $*}"
}

nt_esperar_bots() {
    for _p in $NT_BOTS; do
        wait "${_p#*:}"
        _rc=$?
        echo "$_rc" > "$NT_DIR/${_p%%:*}.rc"
        nt_log "bot de ${_p%%:*} terminó con código $_rc"
    done
    NT_BOTS=""
}

nt_total_state() {
    _t=$(sed -n 's/.*bot: stats .* total=\([0-9][0-9]*\).*/\1/p' "$1" 2>/dev/null | tail -n 1)
    echo "${_t:-0}"
}

nt_recoger() {
    [ "$NT_RECOGIDO" = 1 ] && return 0
    NT_RECOGIDO=1
    dc exec -T server cat /tmp/sw/savagewind.log > "$NT_DIR/server.log" 2>&1
    dc exec -T server cat /tmp/sw/arranque.log > "$NT_DIR/server-arranque.log" 2>&1
    for _n in $NT_NODOS; do
        dc exec -T "$_n" sh /src/tools/nettest/netem.sh mostrar > "$NT_DIR/tc-$_n.txt" 2>&1
    done
    dc logs --no-color > "$NT_DIR/compose.log" 2>&1
}

nt_limpiar() {
    _rc=$?
    trap - EXIT INT TERM
    for _p in $NT_BOTS; do kill "${_p#*:}" 2>/dev/null; done
    if [ "$NT_ARRIBA" = 1 ]; then
        nt_recoger
        for _n in $NT_NODOS; do dc exec -T "$_n" sh /src/tools/nettest/netem.sh quitar >> "$NT_DIR/netem.log" 2>&1; done
        if [ "${NT_MANTENER:-0}" = 1 ]; then
            nt_log "NT_MANTENER=1: los nodos siguen arriba (sin netem)"
        elif [ "${NT_LIMPIAR:-0}" = 1 ]; then
            dc down -v >> "$NT_DIR/run.log" 2>&1
        else
            dc down >> "$NT_DIR/run.log" 2>&1
        fi
    fi
    exit "$_rc"
}
trap nt_limpiar EXIT
trap 'exit 130' INT TERM

# 1. images and binaries
if [ "${NT_SIN_CONSTRUIR:-0}" != 1 ]; then
    if ! docker image inspect swcoop-pruebas >/dev/null 2>&1; then
        nt_log "construyendo la imagen swcoop-pruebas (F0, tools/docker/Dockerfile.pruebas)"
        docker build -f tools/docker/Dockerfile.pruebas -t swcoop-pruebas . >> "$NT_DIR/build.log" 2>&1 \
            || { nt_log "falló la imagen swcoop-pruebas"; tail -n 30 "$NT_DIR/build.log"; exit 1; }
    fi
    nt_log "imagen swcoop-nettest"
    dc build server >> "$NT_DIR/build.log" 2>&1 \
        || { nt_log "falló la imagen swcoop-nettest"; tail -n 30 "$NT_DIR/build.log"; exit 1; }
    nt_log "compilando savagewind (sin juego) y online_bot; la primera vez tarda (ver $NT_DIR/build.log)"
    JOBS=${JOBS:-4} dc --profile build run --rm build >> "$NT_DIR/build.log" 2>&1 \
        || { nt_log "falló la compilación"; tail -n 30 "$NT_DIR/build.log"; exit 1; }
fi

# 2. the scenario's settings (the environment wins over the scenario, the scenario over the defaults), then the nodes
PERFIL_ENTORNO=${PERFIL:-}
NETEM_EN_ENTORNO=${NETEM_EN:-}
PERFIL=""
NETEM_EN=""
# shellcheck disable=SC1090  (the scenario is chosen at run time)
. "$NT_ESCENARIO"
PERFIL=${PERFIL_ENTORNO:-${PERFIL:-lan}}
NETEM_EN=${NETEM_EN_ENTORNO:-${NETEM_EN:-$NT_NODOS}}
if ! command -v escenario >/dev/null 2>&1; then
    echo "[nettest] $NT_ESCENARIO no define la función escenario" >&2
    exit 2
fi

dc down >> "$NT_DIR/run.log" 2>&1  # (whatever a cut-short run left)
NT_ARRIBA=1
dc up -d $NT_NODOS >> "$NT_DIR/run.log" 2>&1 || { nt_log "no arrancaron los nodos (ver $NT_DIR/run.log)"; exit 1; }
{
    echo "escenario: $nombre"
    echo "perfil: $PERFIL en: $NETEM_EN"
    echo "fecha: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    echo "kernel de los nodos: $(dc exec -T server uname -r 2>&1)"
    echo "docker: $(docker version --format '{{.Server.Version}}' 2>&1)"
} > "$NT_DIR/contexto.txt"

# 3-5
nt_netem "$PERFIL" || exit 1
nt_servidor || exit 1
escenario
rc=$?

# 6
nt_recoger
sufijo=""
[ "$NT_SIN_NETEM" = 1 ] && sufijo=" (SIN netem: este kernel no tiene sch_netem)"
if [ "$rc" = 0 ]; then
    echo "$nombre: OK$sufijo" | tee "$NT_DIR/resultado.txt"
    exit 0
fi
echo "$nombre: FALLO$sufijo" | tee "$NT_DIR/resultado.txt"
exit 1
