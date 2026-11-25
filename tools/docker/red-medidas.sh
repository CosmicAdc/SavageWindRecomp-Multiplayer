#!/bin/bash
# Network measurements with the dedicated server and two bots under netem profiles (T219). Run it on the HOST
# (Git Bash / Linux), from the repository root; it starts tools/docker/compose.yml once per scenario:
#
#   docker build -f tools/docker/Dockerfile.pruebas -t swcoop-pruebas .      (once)
#   MSYS_NO_PATHCONV=1 bash tools/docker/red-medidas.sh
#
# Scenarios (servidor + bot-a + bot-b; bot-congelado is not started):
#   1 sin-netem        no latency, no loss
#   2 80ms             delay 80ms 20ms
#   3 80ms+2%          delay 80ms 20ms loss 2%
#   4 200ms+5%         delay 200ms 40ms loss 5%
#   5 noread           no netem; bot-b stops calling recv at s=10 (BOT_NOREAD=10), connection left open (H-1:
#                      the server's blocking sends to it stall its only thread; F3 fixes it). Expected: bot-a stops
#                      receiving (its last 5 s window drops to ~0 STATE/s) and/or the server drops someone.
#
# Each bot prints "bot: stats t=.. state=N in 5.0s (R/s) ..." every 5 s (STATE messages from the OTHER bot; a bot
# sends 20 per second, so ~20/s is healthy). The table, one row per bot and scenario, is written to $SALIDA
# (default red-medidas.md in the current directory; a Markdown table) and printed. Raw compose logs go to $LOGS
# (default a temp folder, not the repository).
#
# Variables (all optional):
#   DURACION=45     seconds each bot stays (the run ends when the first one leaves)
#   SALIDA=red-medidas.md   result table
#   LOGS=<tmp>      folder for the per-scenario compose logs
#   ESCENARIOS="1 2 3 4 5"  which ones to run
#   LIMPIAR=1       at the end `compose down -v` (drops the compiled objects: the next run rebuilds the app)
#
# netem needs a kernel with sch_netem. Docker Desktop's WSL2 kernel usually has none: then red-bots.sh warns and runs
# without it, and the table says "NO" in the netem column: those rows are NOT latency/loss measurements.
# Nothing is published to the host; no ports are opened outside the compose network.
set -u
cd "$(dirname "$0")/../.."
COMPOSE=tools/docker/compose.yml
DURACION=${DURACION:-45}
SALIDA=${SALIDA:-red-medidas.md}
LOGS=${LOGS:-${TMPDIR:-/tmp}/red-medidas-logs}
ESCENARIOS=${ESCENARIOS:-1 2 3 4 5}
export DURACION MSYS_NO_PATHCONV=1
mkdir -p "$LOGS"

# scenario number -> "name|RED|BOT_NOREAD"
escenario() {
    case $1 in
        1) echo "sin-netem||" ;;
        2) echo "80ms|delay 80ms 20ms|" ;;
        3) echo "80ms+2%|delay 80ms 20ms loss 2%|" ;;
        4) echo "200ms+5%|delay 200ms 40ms loss 5%|" ;;
        5) echo "noread||10" ;;
        *) return 1 ;;
    esac
}

# bot rows from one log: STATE/s mean over the 5 s windows, STATE/s of the last window, windows, disconnected?
fila() {  # fila LOG BOT NOMBRE-ESCENARIO NETEM
    awk -v bot="$2" -v esc="$3" -v netem="$4" '
        index($0, bot "-1") == 1 {
            if (match($0, /state=[0-9]+ in [0-9.]+s/)) {
                split(substr($0, RSTART, RLENGTH), a, /[= ]/)   # a[2]=count a[4]="5.0s"
                n++; sum += a[2]; secs += a[4] + 0; last = a[2] / (a[4] + 0)
            }
            if ($0 ~ /bot: disconnected/) desc = "SI"
            if ($0 ~ /bot: not reading/) noread = "SI"
        }
        END {
            printf "| %s | %s | %s | %s | %s | %s | %s |\n", esc, netem, bot, n ? sprintf("%.1f", sum / secs) : "-", \
                   n ? sprintf("%.1f", last) : "-", n + 0, (desc ? "desconectado" : "conectado") (noread ? ", dejo de leer" : "")
        }' "$1"
}

{
    echo "| escenario | netem aplicado | bot | STATE/s media | STATE/s ultima ventana | ventanas de 5 s | estado final |"
    echo "|---|---|---|---|---|---|---|"
} > "$SALIDA"

for n in $ESCENARIOS; do
    def=$(escenario "$n") || { echo "[red-medidas] escenario $n desconocido" >&2; continue; }
    IFS='|' read -r nombre red noread <<< "$def"
    log="$LOGS/escenario-$n.log"
    echo "[red-medidas] === escenario $n: $nombre (RED='$red' BOT_NOREAD='$noread') ==="
    docker compose -f "$COMPOSE" down >/dev/null 2>&1
    RED="$red" BOT_NOREAD="$noread" docker compose -f "$COMPOSE" up --abort-on-container-exit servidor bot-a bot-b \
        > "$log" 2>&1
    rc=$?
    docker compose -f "$COMPOSE" down >/dev/null 2>&1
    if grep -q "AVISO netem no disponible" "$log"; then
        netem=NO
    elif [ -z "$red" ]; then
        netem="- (sin perfil)"
    elif grep -q "netem $red" "$log"; then
        netem=SI
    else
        netem="?"
    fi
    echo "[red-medidas] compose termino con $rc; log: $log"
    fila "$log" bot-a "$nombre" "$netem" >> "$SALIDA"
    fila "$log" bot-b "$nombre" "$netem" >> "$SALIDA"
    # what the server said about each one (expulsions, timeouts), for the notes
    grep -i "servidor-1.*\(drop\|timeout\|kick\|left\|closed\|stale\)" "$log" | tail -5 | sed 's/^/    /'
done

if [ "${LIMPIAR:-0}" = 1 ]; then
    docker compose -f "$COMPOSE" down -v >/dev/null 2>&1
fi

echo
cat "$SALIDA"
echo "[red-medidas] tabla en $SALIDA ; logs en $LOGS"
