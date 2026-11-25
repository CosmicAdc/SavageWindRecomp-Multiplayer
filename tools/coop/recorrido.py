#!/usr/bin/env python3
"""Recorrido de prueba de 30 minutos con dos juegos (specs F1, plan.md 1.5; CU-01, CU-02, CU-03).

Se ejecuta con los dos juegos YA arrancados (bash tools/coop/dos_juegos.sh arrancar). Solo usa tools/swctl.py
(ordenes de docs/CONTROL.md) y los logs de los juegos; no lee memoria con `mem`. Imprime una linea
"PASO n: PASA|FALLA" por paso y termina con 0 si todos pasan, 1 si alguno falla, 2 si se usa mal.
Si un juego deja de responder, el paso FALLA y el guion se detiene SIN cerrar el otro juego.

Solo biblioteca estandar de Python 3.8 o posterior.
"""
import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

AQUI = Path(__file__).resolve().parent
RAIZ = AQUI.parent.parent
SW = Path(os.environ["SW"]) if os.environ.get("SW") else RAIZ.parent
SWCTL = RAIZ / "tools" / "swctl.py"
DOS_JUEGOS = AQUI / "dos_juegos.sh"

# Posiciones y recetas de docs/ONLINE.md y del plan (sin verificar en este equipo).
CAMPAMENTO = (-1203.0, 198.0, 1641.0)  # campamento del acantilado ("Recipes that found the bugs above")

LOG_UNION = r"joined \(slot \d+\)"
LOG_TRASPASO = r"slot \d+'s now \(was"
LOG_HORA = r"the host's time"
LOG_CARGA = r"a load: the players' actors are gone"
LOG_GOLPE = r"hit us: "


class Falla(Exception):
    """El paso no se cumple; el texto va en la linea de salida."""


class JuegoCaido(Exception):
    """Un juego no responde a `state`: el recorrido se detiene sin cerrar el otro."""


class Ctx:
    def __init__(self, args):
        self.puerto = {"A": args.puerto_a, "B": args.puerto_b}
        self.logs = Path(args.logs) if args.logs else SW / "pruebas" / "logs"
        self.python = sys.executable
        self.vivo = {"A": True, "B": True}

    # -- swctl -----------------------------------------------------------------------------------------------
    def sw(self, x, *orden, espera=0, timeout=10):
        """Una orden de swctl.py --json. Devuelve (codigo, data|None, error). Codigo 0 bien, 1 error del juego,
        2 conexion/respuesta invalida (docs/CONTROL.md, "Using swctl")."""
        cmd = [self.python, str(SWCTL), "--json", "--port", str(self.puerto[x]), "--timeout", str(timeout)]
        if espera:
            cmd += ["--wait", str(espera)]
        cmd += [str(o) for o in orden]
        try:
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + espera + 15)
        except subprocess.TimeoutExpired:
            return 2, None, "swctl no termino"
        data, err = None, p.stderr.strip()
        if p.stdout.strip():
            try:
                rep = json.loads(p.stdout)
                data = rep.get("data") if rep.get("ok") else None
                err = err or ("" if rep.get("ok") else str(rep.get("error")))
            except ValueError:
                err = err or "respuesta no JSON"
        return p.returncode, data, err

    def ok(self, x, *orden, **kw):
        cod, data, err = self.sw(x, *orden, **kw)
        if cod != 0:
            raise Falla(f"{x}: '{' '.join(map(str, orden))}' fallo ({err or 'codigo ' + str(cod)})")
        return data or {}

    def responde(self, x):
        return self.sw(x, "state", timeout=8)[0] == 0

    def comprobar_ambos(self, solo=None):
        for x in solo or ("A", "B"):
            if not self.responde(x):
                raise JuegoCaido(f"el juego {x} no responde a state (puerto {self.puerto[x]})")

    def pos(self, x):
        d = self.ok(x, "pos")
        return float(d["x"]), float(d["y"]), float(d["z"])

    def actores(self, x, texto):
        """Lista de (nombre, x, y, z, distancia) de `actors <texto>` (campo `actors`: 'nombre x y z distm; ...')."""
        d = self.ok(x, "actors", texto)
        lista = []
        for fila in str(d.get("actors", "")).split(";"):
            p = fila.split()
            if len(p) >= 5:
                try:
                    lista.append((p[-5], *(float(v) for v in p[-4:])))
                except ValueError:
                    pass
        return lista

    # -- logs ------------------------------------------------------------------------------------------------
    def log(self, x):
        try:
            return (self.logs / f"{x}.log").read_text(errors="replace")
        except OSError:
            return ""

    def cuenta(self, x, patron):
        return len(re.findall(patron, self.log(x)))

    # -- movimiento ------------------------------------------------------------------------------------------
    def esperar_quieto(self, x, minimo=10, maximo=120):
        """Espera a que `pos` responda, termine el teletransporte y Link deje de moverse."""
        ini = time.monotonic()
        time.sleep(minimo)
        previa = None
        ocupado = {"scanning", "probing", "testing", "verifying", "settling"}
        while time.monotonic() - ini < maximo:
            cod_tp, d_tp, _ = self.sw(x, "teleport", "status", timeout=8)
            if cod_tp == 0 and d_tp and str(d_tp.get("state", "")).lower() in ocupado:
                previa = None
                time.sleep(3)
                continue
            cod, d, _ = self.sw(x, "pos", timeout=8)
            if cod == 0 and d:
                p = (float(d["x"]), float(d["y"]), float(d["z"]))
                if previa and math.dist(p, previa) < 2.0:
                    return p
                previa = p
            time.sleep(3)
        raise Falla(f"{x}: Link no quedo quieto en {maximo}s tras moverlo")

    def ir(self, x, *destino):
        self.ok(x, *destino)
        return self.esperar_quieto(x)

    def esperar_actor(self, x, texto, cantidad_min=1, maximo=60):
        ini = time.monotonic()
        while time.monotonic() - ini < maximo:
            if self.sw(x, "actors", texto, timeout=8)[0] == 0:
                lista = self.actores(x, texto)
                if len(lista) >= cantidad_min:
                    return lista
            time.sleep(3)
        return []

    def esperar_ausencia(self, x, texto, maximo=60):
        ini = time.monotonic()
        while time.monotonic() - ini < maximo:
            if self.sw(x, "actors", texto, timeout=8)[0] == 0 and not self.actores(x, texto):
                return True
            time.sleep(3)
        return False

    def god(self, x, valor):
        self.ok(x, "set", "god_mode", "true" if valor else "false")

    def dos_juegos(self, *orden):
        # Ruta completa de bash: en Windows, CreateProcess busca en System32 antes que en PATH y "bash" sería el de
        # WSL, que no ve las rutas C:/... (pasos 9 y 10 fallaban con "No such file or directory").
        bash = shutil.which("bash") or "bash"
        p = subprocess.run([bash, DOS_JUEGOS.as_posix(), *orden], capture_output=True, text=True)
        if p.returncode != 0:
            raise Falla(f"dos_juegos.sh {' '.join(orden)} fallo: {(p.stderr or p.stdout).strip()}")


# ---- pasos (plan.md 1.5) ---------------------------------------------------------------------------------------

def paso1(c):
    """Arrancar A y B (CU-01): los dos responden y A registra la sesion y las dos entradas."""
    for x in ("A", "B"):
        if c.sw(x, "state", espera=180, timeout=8)[0] != 0:
            raise JuegoCaido(f"el juego {x} no responde a state en 180 s")
    t = time.monotonic()
    while time.monotonic() - t < 120 and c.cuenta("A", LOG_UNION) < 2:
        time.sleep(3)
    hosting = c.cuenta("A", r"hosting on port")
    unidos = c.cuenta("A", LOG_UNION)
    if hosting < 1:
        raise Falla("A.log no tiene '[online] hosting on port'")
    if unidos < 2:
        raise Falla(f"A.log tiene {unidos} lineas 'joined (slot N)' (se esperan 2)")
    return f"hosting {hosting}, entradas {unidos}"


def _en_mundo(c, x, maximo=240):
    ini = time.monotonic()
    intentos = 0
    while time.monotonic() - ini < maximo:
        if c.sw(x, "pos", timeout=8)[0] == 0:
            return
        intentos += 1
        if time.monotonic() - ini > 35:
            if intentos == 8:
                c.sw(x, "press", "b", "8", timeout=8)
                time.sleep(1)
            c.sw(x, "press", "a", "8", timeout=8)
        time.sleep(5)
    raise Falla(f"{x}: el mundo no cargo en {maximo}s (pos da error)")


def paso2(c):
    """goto tower 7 en ambos (CU-02): cada juego ve un OnlinePlayer."""
    for x in ("A", "B"):
        _en_mundo(c, x)
        c.god(x, True)
    for x in ("A", "B"):
        c.ok(x, "goto", "tower", "7")
    for x in ("A", "B"):
        c.esperar_quieto(x)
    res = []
    for x in ("A", "B"):
        n = len(c.esperar_actor(x, "OnlinePlayer", 1, 60))
        if n != 1:
            raise Falla(f"{x}: 'actors OnlinePlayer' devuelve {n} (se espera 1)")
        res.append(f"{x}:{n}")
    if c.cuenta("A", r"OnlinePlayer \(actor") + c.cuenta("B", r"OnlinePlayer \(actor") < 2:
        raise Falla("faltan lineas '[online] <nombre>: OnlinePlayer (actor ...)' en los logs")
    return "OnlinePlayer " + ", ".join(res)


def paso3(c):
    """B camina (CU-02): el OnlinePlayer visto en A sigue la posicion de B."""
    antes = c.esperar_actor("A", "OnlinePlayer", 1, 30)
    if not antes:
        raise Falla("A no ve a OnlinePlayer")
    for _ in range(5):
        c.ok("B", "stick", "l", "0", "1", "150")
        time.sleep(5)
    time.sleep(3)
    pos_b = c.pos("B")
    despues = c.esperar_actor("A", "OnlinePlayer", 1, 30)
    if not despues:
        raise Falla("A dejo de ver a OnlinePlayer")
    mov = math.dist(antes[0][1:4], despues[0][1:4])
    err = math.dist(despues[0][1:4], pos_b)
    if mov < 2.0:
        raise Falla(f"el OnlinePlayer en A se movio solo {mov:.1f} m")
    if err > 15.0:
        raise Falla(f"el OnlinePlayer en A esta a {err:.1f} m del pos de B (margen 15 m)")
    return f"movio {mov:.1f} m, a {err:.1f} m del pos de B"


def paso4(c):
    """Ambos al campamento del acantilado (CU-03): traspasos de enemigos sin cuelgue."""
    antes = c.cuenta("A", LOG_TRASPASO) + c.cuenta("B", LOG_TRASPASO)
    for x in ("A", "B"):
        c.ok(x, "teleport", *CAMPAMENTO)
    for x in ("A", "B"):
        c.esperar_quieto(x)
    time.sleep(60)
    c.comprobar_ambos()
    n = c.cuenta("A", LOG_TRASPASO) + c.cuenta("B", LOG_TRASPASO) - antes
    if n < 1:
        raise Falla("sin lineas de traspaso de enemigos en los logs")
    return f"{n} traspasos"


def paso5(c):
    """B se aleja mas de 400 m y vuelve (CU-02, CU-03): el Link remoto desaparece y se recrea."""
    c.ir("B", "goto", "stable", "2")
    if not c.esperar_ausencia("A", "OnlinePlayer", 60):
        raise Falla("A sigue viendo a OnlinePlayer con B a mas de 400 m")
    c.comprobar_ambos()
    c.ir("A", "goto", "tower", "7")
    c.ir("B", "goto", "tower", "7")
    if not c.esperar_actor("A", "OnlinePlayer", 1, 60):
        raise Falla("A no recreo a OnlinePlayer al volver B")
    return "desaparece y vuelve (cargas en log: %d)" % (c.cuenta("A", LOG_CARGA) + c.cuenta("B", LOG_CARGA))


def paso6(c):
    """A cambia la hora (CU-03): B la sigue."""
    antes = c.cuenta("B", LOG_HORA)
    c.ok("A", "time", "set", "18")
    t = time.monotonic()
    while time.monotonic() - t < 60 and c.cuenta("B", LOG_HORA) <= antes:
        time.sleep(3)
    n = c.cuenta("B", LOG_HORA) - antes
    if n < 1:
        raise Falla("B.log no tiene '[online] the host's time' nuevo")
    return f"{n} lineas de hora en B"


def paso7(c):
    """A abre y cierra la pausa varias veces (CU-03): sin cuelgue."""
    antes = c.cuenta("A", r"our pose")
    for _ in range(8):
        c.ok("A", "press", "plus", "6")
        time.sleep(2.5)
    time.sleep(3)
    return f"pose en A: {c.cuenta('A', r'our pose') - antes} lineas nuevas"


def paso8(c):
    """Ambos saltan lejos a la vez (CU-03): carga en los dos, actores rehechos."""
    antes = c.cuenta("A", LOG_CARGA) + c.cuenta("B", LOG_CARGA)
    c.ok("A", "goto", "hateno")
    c.ok("B", "goto", "hateno")
    for x in ("A", "B"):
        c.esperar_quieto(x, minimo=15, maximo=150)
    for x in ("A", "B"):
        if not c.esperar_actor(x, "OnlinePlayer", 1, 90):
            raise Falla(f"{x} no ve a OnlinePlayer tras la carga")
    return "cargas en log: %d" % (c.cuenta("A", LOG_CARGA) + c.cuenta("B", LOG_CARGA) - antes)


def paso9(c):
    """Matar B sin cerrar (CU-03): A sigue un minuto y el servidor expulsa a B."""
    c.dos_juegos("matar", "B")
    c.vivo["B"] = False
    ini = time.monotonic()
    while time.monotonic() - ini < 60:
        c.comprobar_ambos(solo=("A",))
        time.sleep(5)
    if not c.esperar_ausencia("A", "OnlinePlayer", 30):
        raise Falla("A sigue viendo a OnlinePlayer 60 s despues de morir B")
    return "A responde 60 s y ya no ve a B"


def paso10(c):
    """Relanzar B (CU-01): vuelve a unirse y su actor se recrea."""
    antes = c.cuenta("A", LOG_UNION)
    c.dos_juegos("relanzar", "B")
    c.vivo["B"] = True
    if c.sw("B", "state", espera=180, timeout=8)[0] != 0:
        raise JuegoCaido("B no responde a state tras relanzarlo")
    _en_mundo(c, "B", 240)
    c.god("B", True)
    t = time.monotonic()
    while time.monotonic() - t < 120 and c.cuenta("A", LOG_UNION) <= antes:
        time.sleep(3)
    if c.cuenta("A", LOG_UNION) <= antes:
        raise Falla("A.log no tiene una nueva linea 'joined (slot N)'")
    c.ok("A", "goto", "tower", "7")
    c.ok("B", "goto", "tower", "7")
    for x in ("A", "B"):
        c.esperar_quieto(x)
    if not c.esperar_actor("A", "OnlinePlayer", 1, 60):
        raise Falla("A no ve a OnlinePlayer tras volver B")
    return "B vuelve a unirse y A lo ve"


# (numero, titulo, funcion, comprueba a los dos juegos al final)
PASOS = [
    (1, "Arrancar A y B (CU-01)", paso1, True),
    (2, "goto tower 7 en ambos (CU-02)", paso2, True),
    (3, "B camina (CU-02)", paso3, True),
    (4, "Campamento del acantilado (CU-03)", paso4, True),
    (5, "B se aleja >400 m y vuelve (CU-02, CU-03)", paso5, True),
    (6, "A cambia la hora (CU-03)", paso6, True),
    (7, "A abre y cierra la pausa (CU-03)", paso7, True),
    (8, "Ambos saltan lejos a la vez (CU-03)", paso8, True),
    (9, "Matar el proceso de B (CU-03)", paso9, False),
    (10, "Relanzar B (CU-01)", paso10, True),
]


# ---- enemigos atacan a los dos (T104 paso 5, ADR-012) ----------------------------------------------------------

def enemigos(c, campamento=CAMPAMENTO):
    """Funcion separada, NO es uno de los 10 pasos. Devuelve [(letra, PASA|FALLA, detalle)] para (a), (b), (c).

    A se queda en la torre (>60 m de B); B va al campamento. (a) los enemigos pasan a B y se acercan a B;
    (b) con A junto a B y sin god_mode, A recibe un golpe; (c) con B a >40 m de un enemigo y A 10 m mas cerca,
    el enemigo pasa a A. Dueno = lineas de traspaso '[online] <enemigo> <id>: slot N's now' (docs/ONLINE.md,
    'Owners'); el golpe, la linea '[online] <enemigo> hit us:'."""
    res = []
    c.god("A", True)
    c.god("B", True)
    c.ir("A", "goto", "tower", "7")
    c.ir("B", "teleport", *campamento)
    pa, pb = c.pos("A"), c.pos("B")
    if math.dist(pa, pb) <= 60.0:
        raise Falla(f"A y B estan a {math.dist(pa, pb):.0f} m (se necesitan mas de 60 m)")

    # (a) B toma los enemigos y se acercan a B
    try:
        base = c.cuenta("A", LOG_TRASPASO) + c.cuenta("B", LOG_TRASPASO)
        ini = c.esperar_actor("B", "Enemy_", 1, 45)
        if not ini:
            raise Falla("B no ve ningun 'Enemy_' en el campamento")
        d0 = min(e[4] for e in ini)
        time.sleep(30)
        fin = c.actores("B", "Enemy_")
        d1 = min((e[4] for e in fin), default=d0)
        n = c.cuenta("A", LOG_TRASPASO) + c.cuenta("B", LOG_TRASPASO) - base
        if n < 1:
            raise Falla("sin lineas de traspaso de dueno")
        if not (d1 < d0 - 1.0 or d1 <= 8.0):
            raise Falla(f"el enemigo mas cercano a B no se acerca ({d0:.0f} m -> {d1:.0f} m)")
        res.append(("a", "PASA", f"{n} traspasos; distancia {d0:.0f} -> {d1:.0f} m"))
    except Falla as e:
        res.append(("a", "FALLA", str(e)))

    # (b) A junto a B, sin god_mode: recibe al menos un golpe
    try:
        pb = c.pos("B")
        golpes = c.cuenta("A", LOG_GOLPE)
        c.ir("A", "teleport", pb[0] + 2.0, pb[1] + 2.0, pb[2])
        c.god("A", False)
        try:
            t = time.monotonic()
            while time.monotonic() - t < 20 and c.cuenta("A", LOG_GOLPE) <= golpes:
                time.sleep(2)
        finally:
            c.god("A", True)
        n = c.cuenta("A", LOG_GOLPE) - golpes
        if n < 1:
            raise Falla("A no recibio ningun golpe en 20 s ('hit us' en A.log)")
        res.append(("b", "PASA", f"A recibio {n} golpes"))
    except Falla as e:
        res.append(("b", "FALLA", str(e)))

    # (c) B a >40 m del enemigo y A 10 m mas cerca: el enemigo pasa a A
    try:
        lista = c.actores("B", "Enemy_") or c.actores("A", "Enemy_")
        if not lista:
            raise Falla("no hay enemigos que mover")
        e = lista[0]
        patron = r"slot 0's now \(was"
        antes = c.cuenta("A", patron) + c.cuenta("B", patron)
        c.ir("B", "teleport", e[1] + 50.0, e[2] + 3.0, e[3])
        c.ir("A", "teleport", e[1] - 25.0, e[2] + 3.0, e[3])
        t = time.monotonic()
        n = 0
        while time.monotonic() - t < 40:
            n = c.cuenta("A", patron) + c.cuenta("B", patron) - antes
            if n >= 1:
                break
            time.sleep(3)
        if n < 1:
            raise Falla("ningun enemigo paso al slot 0 (A) en 40 s")
        res.append(("c", "PASA", f"{n} traspasos al slot 0"))
    except Falla as e:
        res.append(("c", "FALLA", str(e)))
    return res


# ---- linea de ordenes ------------------------------------------------------------------------------------------

def rango(texto):
    m = re.fullmatch(r"(\d+)(?:-(\d+))?", texto.strip())
    if not m:
        raise argparse.ArgumentTypeError("use N, a-b o 'enemigos'")
    a = int(m.group(1))
    b = int(m.group(2) or a)
    if not (1 <= a <= b <= len(PASOS)):
        raise argparse.ArgumentTypeError(f"pasos entre 1 y {len(PASOS)}")
    return a, b


def ayuda():
    return "Pasos:\n" + "\n".join(f"  {n:>2}  {t}" for n, t, _f, _a in PASOS) + (
        "\n  enemigos  Los enemigos atacan a los dos (T104 paso 5, ADR-012); funcion aparte, se pide con "
        "--pasos enemigos o --enemigos")


def main(argv=None):
    ap = argparse.ArgumentParser(add_help=False, description=__doc__.split("\n")[0], epilog=ayuda(),
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ayuda", "-h", action="help", help="muestra esta ayuda y la lista de pasos")
    ap.add_argument("--pasos", default="1-%d" % len(PASOS), metavar="A-B",
                    help="pasos a ejecutar: N, a-b o 'enemigos' (por defecto todos, 1-%d)" % len(PASOS))
    ap.add_argument("--enemigos", action="store_true", help="ejecuta ademas la comprobacion de enemigos")
    ap.add_argument("--puerto-a", type=int, default=7777, help="puerto de control de A (7777)")
    ap.add_argument("--puerto-b", type=int, default=7778, help="puerto de control de B (7778)")
    ap.add_argument("--logs", help="carpeta de A.log y B.log (por defecto $SW/pruebas/logs)")
    args = ap.parse_args(argv)
    solo_enemigos = args.pasos.strip().lower() == "enemigos"
    try:
        a, b = (0, -1) if solo_enemigos else rango(args.pasos)
    except argparse.ArgumentTypeError as e:
        ap.error(str(e))
    c = Ctx(args)
    fallos = 0

    for n, titulo, fn, ambos in PASOS:
        if not a <= n <= b:
            continue
        try:
            try:
                detalle = fn(c)
            except Falla as e:
                print(f"PASO {n}: FALLA - {titulo}: {e}", flush=True)
                fallos += 1
                c.comprobar_ambos(None if c.vivo["B"] else ("A",))
                continue
            c.comprobar_ambos(None if ambos and c.vivo["B"] else ("A",))
            print(f"PASO {n}: PASA - {titulo}: {detalle}", flush=True)
        except JuegoCaido as e:
            print(f"PASO {n}: FALLA - {titulo}: {e}; se detiene sin cerrar el otro juego", flush=True)
            return 1

    if solo_enemigos or args.enemigos:
        try:
            c.comprobar_ambos()
            for letra, veredicto, detalle in enemigos(c):
                print(f"ENEMIGOS ({letra}): {veredicto} - {detalle}", flush=True)
                fallos += veredicto == "FALLA"
        except (Falla, JuegoCaido) as e:
            print(f"ENEMIGOS: FALLA - {e}", flush=True)
            fallos += 1
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
