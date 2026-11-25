# smoke (T301): two bots see each other through the dedicated server with the "wifi" profile on the three nodes.
# Sourced by tools/nettest/run.sh (read its header): `sh tools/nettest/run.sh smoke`.
#
# Passes when each bot (tools/online_bot.c)
#   - got its slot ("bot: in, slot N") and heard the other one join ("bot: <other> is slot N"),
#   - stayed connected the whole time (its time ran out: exit 124, not "disconnected"),
#   - counted at least SMOKE_MIN_STATE STATE messages from the other ("total=" of its last "bot: stats" line, printed
#     every 5 s). A bot sends 20 per second, so 20 s is ~400 at best; 200 leaves room for wifi's 2% loss on each hop.
#
# Variables: SMOKE_SEGUNDOS=20, SMOKE_MIN_STATE=200 (and PERFIL, NETEM_EN as in run.sh).
PERFIL=wifi
SMOKE_SEGUNDOS=${SMOKE_SEGUNDOS:-20}
SMOKE_MIN_STATE=${SMOKE_MIN_STATE:-200}

# smoke_comprobar: one line per bot, 0 when both pass. Reads $NT_DIR/<nodo>.log and <nodo>.rc.
smoke_comprobar() {
    _ok=0
    for _par in bot_a:BotB bot_b:BotA; do
        _nodo=${_par%%:*} _otro=${_par#*:}
        _log=$NT_DIR/$_nodo.log
        _rc=$(cat "$NT_DIR/$_nodo.rc" 2>/dev/null || echo "?")
        _total=$(nt_total_state "$_log")
        _fallos=""
        grep -q "bot: in, slot" "$_log" 2>/dev/null || _fallos="$_fallos sin-ranura"
        grep -q "bot: $_otro is slot" "$_log" 2>/dev/null || _fallos="$_fallos no-vio-a-$_otro"
        grep -q "bot: disconnected" "$_log" 2>/dev/null && _fallos="$_fallos desconectado"
        [ "$_rc" = 124 ] || _fallos="$_fallos terminó-con-código-$_rc"
        [ "$_total" -ge "$SMOKE_MIN_STATE" ] || _fallos="$_fallos pocos-STATE"
        if [ -z "$_fallos" ]; then
            nt_log "smoke: $_nodo OK: $_total STATE de $_otro (mínimo $SMOKE_MIN_STATE)"
        else
            nt_log "smoke: $_nodo FALLA:$_fallos ($_total STATE de $_otro, mínimo $SMOKE_MIN_STATE; ver $_log)"
            _ok=1
        fi
    done
    return $_ok
}

escenario() {
    # (2 s more than the window: its "bot: stats" line at t=20 is printed before the time runs out)
    nt_bot bot_a BotA $((SMOKE_SEGUNDOS + 2))
    nt_bot bot_b BotB $((SMOKE_SEGUNDOS + 2))
    nt_esperar_bots
    smoke_comprobar
}
