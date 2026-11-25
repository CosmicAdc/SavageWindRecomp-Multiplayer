#!/usr/bin/env bash
# Dos juegos en un PC para probar el modo online (docs/ONLINE.md, "Testing with two games on one PC").
#
# Uso: bash tools/coop/dos_juegos.sh <orden>
#   arrancar     lanza A (anfitrión, control 7777) y B (se une a 127.0.0.1, control 7778), sin ventana
#   ventana      igual que arrancar, pero con ventana (sin SAVAGEWIND_HIDDEN)
#   parar        cierra los dos con "swctl exit" (puertos 7777 y 7778) y comprueba que no queda savagewind.exe
#   matar B      mata el proceso que escucha el puerto de control de B, sin cerrarlo limpio (también vale A)
#   relanzar B   lanza de nuevo B (o A) con las mismas variables; su log se añade al anterior
#
# Carpetas (fuera del repositorio; $SW = la carpeta que contiene coop/, juego/ y pruebas/, deducida de la ubicación
# de este guion, o la variable de entorno SW si existe):
#   $SW/pruebas/guardado-A, guardado-B   copias de un guardado; nunca la carpeta en uso
#   $SW/pruebas/ajustes-A.ini, -B.ini    ajustes de cada juego (se crean vacíos si faltan)
#   $SW/pruebas/logs/A.log, B.log        salida de cada juego
#
# No usar bajo "timeout" de MSYS cuando hay ventana. Puerto de sesión de prueba 52530 (no el 52520 real).

set -u

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RAIZ="$(cd "$AQUI/../.." && pwd)"                  # raíz del repositorio (desde ahí se lanza el juego)
SW="${SW:-$(cd "$RAIZ/.." && pwd)}"
PRUEBAS="$SW/pruebas"
LOGS="$PRUEBAS/logs"

PUERTO_SESION=52530
PUERTO_A=7777
PUERTO_B=7778
ESPERA_ANFITRION=180     # s que se espera a que A diga "hosting on port" antes de lanzar B
ESPERA_CIERRE=60         # s que se espera a que "parar" deje 0 procesos

# El juego se lanza desde MSYS2 UCRT64: sus DLL están en ucrt64/bin.
if [ -d /c/msys64/ucrt64/bin ]; then
    PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
fi
export PATH

if [ -x "$RAIZ/tools/venv/Scripts/python.exe" ]; then
    PY="$RAIZ/tools/venv/Scripts/python.exe"
else
    PY="python"
fi
EXE="$RAIZ/build/savagewind.exe"

die() { echo "dos_juegos: $*" >&2; exit 1; }

# Ruta con barras normales, en formato de Windows (el juego la lee como variable de entorno).
ruta_win() { (cd "$1" && pwd -W 2>/dev/null || pwd); }

preparar() {
    [ -f "$EXE" ] || die "no existe $EXE (compilar antes: make -j8)"
    mkdir -p "$PRUEBAS/guardado-A" "$PRUEBAS/guardado-B" "$LOGS"
    for x in A B; do
        [ -e "$PRUEBAS/ajustes-$x.ini" ] || : > "$PRUEBAS/ajustes-$x.ini"
        if [ -z "$(ls -A "$PRUEBAS/guardado-$x" 2>/dev/null)" ]; then
            echo "aviso: guardado-$x está vacío (el juego arrancará sin partida; hay que copiar un guardado ahí)" >&2
        fi
    done
}

puerto_de() {
    case "$1" in
        A|a) echo "$PUERTO_A" ;;
        B|b) echo "$PUERTO_B" ;;
        *) die "juego desconocido: '$1' (A o B)" ;;
    esac
}

# lanzar <A|B> <ventana:0|1> <modo_log: ">" | ">>">
lanzar() {
    local x="$1" ventana="$2" modo="$3" ctl online nombre
    x="$(echo "$x" | tr a-z A-Z)"
    ctl="$(puerto_de "$x")"
    if [ "$x" = A ]; then online=host; nombre=Host; else online=127.0.0.1; nombre=Client; fi
    local guardado ajustes log
    guardado="$(ruta_win "$PRUEBAS/guardado-$x")"
    ajustes="$(ruta_win "$PRUEBAS")/ajustes-$x.ini"
    log="$LOGS/$x.log"
    (
        cd "$RAIZ" || exit 1
        export SAVAGEWIND_SAVE_DIR="$guardado" SAVAGEWIND_SETTINGS="$ajustes"
        export SAVAGEWIND_NO_LAUNCHER=1 SAVAGEWIND_AUDIO_GAIN=0
        export SAVAGEWIND_TEST_WARP=plateau SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"
        export SAVAGEWIND_ONLINE_PORT="$PUERTO_SESION" SAVAGEWIND_ONLINE="$online"
        export SAVAGEWIND_ONLINE_NAME="$nombre" SAVAGEWIND_CONTROL="$ctl"
        if [ "$ventana" = 1 ]; then unset SAVAGEWIND_HIDDEN; else export SAVAGEWIND_HIDDEN=1; fi
        if [ "$modo" = ">>" ]; then
            nohup "$EXE" >> "$log" 2>&1 < /dev/null &
        else
            nohup "$EXE" > "$log" 2>&1 < /dev/null &
        fi
    )
    echo "lanzado $x ($online, control $ctl, log $log)"
}

contar() { tasklist 2>/dev/null | grep -ci savagewind; }

cmd_arrancar() {
    local ventana="$1" n
    preparar
    n="$(contar)"
    [ "${n:-0}" -eq 0 ] || die "ya hay $n proceso(s) savagewind.exe; usar 'parar' antes"
    lanzar A "$ventana" ">"
    # B se une a A: esperar a que A anuncie la sesión (si no aparece, se lanza B igualmente).
    local t=0
    until grep -q "hosting on port" "$LOGS/A.log" 2>/dev/null; do
        [ "$t" -ge "$ESPERA_ANFITRION" ] && { echo "aviso: A no dijo 'hosting on port' en ${ESPERA_ANFITRION}s; se lanza B igualmente" >&2; break; }
        sleep 2; t=$((t + 2))
    done
    lanzar B "$ventana" ">"
}

cmd_parar() {
    local x ctl t n
    for x in A B; do
        ctl="$(puerto_de "$x")"
        "$PY" "$RAIZ/tools/swctl.py" --port "$ctl" --timeout 5 exit > /dev/null 2>&1 \
            && echo "exit enviado a $x (puerto $ctl)" || echo "$x no respondió en el puerto $ctl"
    done
    t=0
    while [ "$t" -lt "$ESPERA_CIERRE" ]; do
        n="$(contar)"
        [ "${n:-0}" -eq 0 ] && { echo "savagewind.exe: 0 procesos"; return 0; }
        sleep 2; t=$((t + 2))
    done
    echo "quedan $(contar) proceso(s) savagewind.exe tras ${ESPERA_CIERRE}s" >&2
    return 1
}

# PID que escucha en un puerto TCP local (el juego abre ahí su socket de control; sirve aunque el juego se
# haya relanzado como proceso hijo).
pid_escucha() {
    netstat.exe -ano -p tcp 2>/dev/null | tr -d '\r' \
        | awk -v p=":$1" '$1=="TCP" && $2 ~ (p "$") && $4=="LISTENING" {print $5; exit}'
}

cmd_matar() {
    local ctl pid
    ctl="$(puerto_de "${1:-}")"
    pid="$(pid_escucha "$ctl")"
    [ -n "$pid" ] || die "nadie escucha en el puerto $ctl"
    taskkill.exe //PID "$pid" //F //T > /dev/null || die "taskkill falló con el PID $pid"
    echo "muerto el proceso $pid (puerto $ctl) sin cerrarlo limpio"
}

cmd_relanzar() {
    local x ctl
    x="${1:-}"
    ctl="$(puerto_de "$x")"
    preparar
    [ -z "$(pid_escucha "$ctl")" ] || die "el juego $x sigue escuchando en el puerto $ctl; usar 'matar $x' antes"
    lanzar "$x" 0 ">>"
}

orden="${1:-}"
case "$orden" in
    arrancar)  cmd_arrancar 0 ;;
    ventana)   cmd_arrancar 1 ;;
    parar)     cmd_parar ;;
    matar)     cmd_matar "${2:-}" ;;
    relanzar)  cmd_relanzar "${2:-}" ;;
    *)
        sed -n '2,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//' >&2
        [ -z "$orden" ] && exit 2
        die "orden desconocida: '$orden'"
        ;;
esac
