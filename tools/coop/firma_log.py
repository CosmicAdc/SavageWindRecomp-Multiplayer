#!/usr/bin/env python3
"""Firma de comportamiento de una sesion online a partir de sus logs (T201).

Toma las lineas con "[online]" de cada log, sustituye numeros, direcciones
hexadecimales y nombres de jugador por marcadores (<N>, <HEX>, <P>) y cuenta
cuantas hay de cada texto resultante (el "tipo de linea"). Solo biblioteca
estandar. No ejecuta el juego ni guarda logs.

Usos:
  firma_log.py A.log [B.log ...]
      Tabla "tipo de linea -> recuento" por juego (el 1.er fichero es el
      juego A, el 2.o el B, ...). Codigo 0.

  firma_log.py --generar-referencia A1.log B1.log A2.log B2.log [A3.log B3.log]
      Genera la referencia a partir de varias pasadas. Los ficheros van por
      pasadas completas, cada una con tantos logs como juegos (--juegos N,
      por defecto 2). Imprime en la salida estandar un texto con, por juego y
      por tipo, el MINIMO y el MAXIMO de lineas vistas en las pasadas (un
      tipo ausente en una pasada cuenta 0). Formato (estable, ordenado):
          # firma-referencia v1
          # pasadas: 3
          # juegos: 2
          [A]
          <min>TAB<max>TAB<tipo de linea>
          ...
          [B]
          ...
      Las lineas que empiezan por '#' son comentarios.

  firma_log.py --comparar REFERENCIA A.log B.log
      Compara una pasada con la referencia y sale con 0 (igual) o 1
      (diferente), explicando cada diferencia. Tolerancia (plan.md 4.4):
        (a) no aparece ningun tipo de linea nuevo (que no este en la
            referencia, ni siquiera con minimo 0);
        (b) no desaparece ningun tipo que salio en todas las pasadas de
            referencia (minimo > 0) y ahora sale 0 veces;
        (c) los tipos de evento unico (joined, hosting, actors on, a load)
            tienen un recuento dentro de [min, max] de la referencia, que es
            exacto cuando min == max.
      Los recuentos de los demas tipos (periodicos) pueden variar.
      Es un indicio fuerte, no una demostracion.

  firma_log.py --autoprueba
      Pruebas sinteticas de normalizacion, generacion y comparacion.

Nombres de jugador: siempre 'Host' y 'Client'; ademas los que aparezcan en
las lineas "server: <nombre> joined/left" del propio log, y los de --nombre.
Codigos de salida: 0 bien, 1 diferencia o fallo de autoprueba, 2 uso o
fichero ilegible.
"""
import argparse
import re
import sys
from collections import Counter

MARK = "[online]"
# Tipos de evento unico: se reconocen por un trozo del texto normalizado.
UNICOS = (" joined (slot <N>)", "hosting on port <N>", "actors on", "a load:")
NOMBRES_FIJOS = ("Host", "Client")

_RE_SERVER = re.compile(r"\[online\] server: (.+?) (?:joined|left) \(slot")
_RE_HEX = re.compile(r"\b(?:0[xX][0-9a-fA-F]+|(?=[0-9a-f]*\d)(?=[0-9a-f]*[a-f])[0-9a-f]{6,})\b")
_RE_NUM = re.compile(r"[-+]?\d+(?:\.\d+)?")


def leer_lineas(ruta):
    with open(ruta, "r", encoding="utf-8", errors="replace") as f:
        return [l.rstrip("\r\n") for l in f]


def nombres_de(lineas, extra=()):
    s = set(NOMBRES_FIJOS) | set(extra)
    for l in lineas:
        m = _RE_SERVER.search(l)
        if m:
            s.add(m.group(1))
    return s


def normalizar(linea, nombres):
    """Devuelve el tipo de una linea con '[online]', o None si no la tiene."""
    i = linea.find(MARK)
    if i < 0:
        return None
    t = linea[i:]
    # Primero los nombres (los mas largos antes), luego hex y numeros.
    for n in sorted(nombres, key=len, reverse=True):
        t = re.sub(r"(?<![A-Za-z0-9_])" + re.escape(n) + r"(?![A-Za-z0-9_])", "<P>", t)
    t = _RE_HEX.sub("<HEX>", t)
    t = _RE_NUM.sub("<N>", t)
    return t.strip()


def firma(lineas, extra=()):
    nombres = nombres_de(lineas, extra)
    c = Counter()
    for l in lineas:
        t = normalizar(l, nombres)
        if t is not None:
            c[t] += 1
    return c


def es_unico(tipo):
    return any(u in tipo for u in UNICOS)


def etiqueta(i):
    return chr(ord("A") + i) if i < 26 else "J%d" % i


# ---------- tabla ----------
def tabla(c):
    filas = sorted(c.items(), key=lambda kv: (-kv[1], kv[0]))
    return "\n".join("%7d  %s" % (n, t) for t, n in filas)


# ---------- referencia ----------
def generar(pasadas):
    """pasadas: lista de pasadas; cada una, lista de Counter (uno por juego)."""
    juegos = len(pasadas[0])
    out = ["# firma-referencia v1", "# pasadas: %d" % len(pasadas), "# juegos: %d" % juegos]
    for g in range(juegos):
        out.append("[%s]" % etiqueta(g))
        tipos = set()
        for p in pasadas:
            tipos |= set(p[g])
        for t in sorted(tipos):
            v = [p[g].get(t, 0) for p in pasadas]
            out.append("%d\t%d\t%s" % (min(v), max(v), t))
    return "\n".join(out) + "\n"


def leer_referencia(texto):
    """Devuelve (pasadas, [ {tipo: (min, max)} por juego ])."""
    pasadas, juegos, actual = 0, [], None
    for l in texto.splitlines():
        if not l.strip():
            continue
        if l.startswith("#"):
            m = re.match(r"# pasadas: (\d+)", l)
            if m:
                pasadas = int(m.group(1))
            continue
        m = re.fullmatch(r"\[([A-Z]\w*)\]", l)
        if m:
            actual = {}
            juegos.append(actual)
            continue
        p = l.split("\t", 2)
        if actual is None or len(p) != 3:
            raise ValueError("linea de referencia no valida: %r" % l)
        actual[p[2]] = (int(p[0]), int(p[1]))
    return pasadas, juegos


def comparar(ref_juegos, nuevos):
    """Devuelve la lista de diferencias (vacia = igual)."""
    dif = []
    if len(ref_juegos) != len(nuevos):
        return ["la referencia tiene %d juegos y se dieron %d logs" % (len(ref_juegos), len(nuevos))]
    for g, (ref, c) in enumerate(zip(ref_juegos, nuevos)):
        j = etiqueta(g)
        for t in sorted(c):
            if t not in ref:
                dif.append("[%s] tipo NUEVO (%d veces): %s" % (j, c[t], t))
        for t in sorted(ref):
            lo, hi = ref[t]
            n = c.get(t, 0)
            if lo > 0 and n == 0:
                dif.append("[%s] tipo fijo DESAPARECIDO (salia %d-%d veces): %s" % (j, lo, hi, t))
            elif es_unico(t) and not lo <= n <= hi:
                dif.append("[%s] evento unico con recuento distinto: %d, referencia %d-%d: %s" % (j, n, lo, hi, t))
    return dif


# ---------- autoprueba ----------
def autoprueba():
    fallos = []

    def ok(cond, msg):
        if not cond:
            fallos.append(msg)

    nm = {"Host", "Client", "Zelda"}
    n = lambda s: normalizar(s, nm)
    ok(n("ruido sin marca") is None, "linea sin [online] debe ignorarse")
    ok(n("[online] Client: OnlinePlayer (actor 72eccd1240)") == "[online] <P>: OnlinePlayer (actor <HEX>)",
       "nombre y hex: %r" % n("[online] Client: OnlinePlayer (actor 72eccd1240)"))
    ok(n("[online] 160 enemies for the other players") == "[online] <N> enemies for the other players", "numero")
    ok(n("[online] Host: x at -1.50 2 0x1f") == "[online] <P>: x at <N> <N> <HEX>", "decimal, negativo, 0x: %r" % n("[online] Host: x at -1.50 2 0x1f"))
    ok(n("[online] we broke Obj_Plant_A_02 (419056119)") == "[online] we broke Obj_Plant_A_<N> (<N>)", "identificador con digitos")
    ok(n("[online] the host's weather: Zelda 3") == "[online] the host's weather: <P> <N>", "nombre extra, 'host' minuscula intacto")
    ok(n("[online] Hostile deadbeef") == "[online] Hostile deadbeef", "no sustituye palabras (Hostile, deadbeef sin digitos)")
    ok(nombres_de(["[online] server: Ana joined (slot 2)"]) >= {"Host", "Client", "Ana"}, "nombre aprendido del log")

    A = ["[online] hosting on port 5", "[online] server: Host joined (slot 0)", "[online] game version 1.9.0: actors on",
         "[online] a load: the players' actors are gone", "[online] fx other abc123 4: s"] + ["[online] shot 1 (x) gone"] * 3
    B = A + ["[online] solo en esta pasada"]
    p1, p2, p3 = [firma(A)], [firma(B)], [firma(A + ["[online] shot 9 (x) gone"] * 2)]
    ref = generar([p1, p2, p3])
    ok(ref == generar([p1, p2, p3]), "generacion estable")
    ok("\t" in ref and ref.startswith("# firma-referencia v1\n"), "formato de referencia")
    np_, rj = leer_referencia(ref)
    ok(np_ == 3 and len(rj) == 1, "lectura de la referencia")
    ok(rj[0]["[online] solo en esta pasada"] == (0, 1), "min/max con ausencia = 0")
    ok(rj[0]["[online] shot <N> (x) gone"] == (3, 5), "min/max de periodico")
    ok(comparar(rj, [firma(A + ["[online] shot 1 (x) gone"])]) == [], "igual: periodico dentro de rango/variable")
    ok(comparar(rj, [firma(A + ["[online] shot 1 (x) gone"] * 40)]) == [], "periodico muy distinto sigue igual")
    ok(any("NUEVO" in d for d in comparar(rj, [firma(A + ["[online] tipo inventado"])])), "tipo nuevo detectado")
    sin_fx = [l for l in A if "fx other" not in l]
    ok(any("DESAPARECIDO" in d for d in comparar(rj, [firma(sin_fx)])), "tipo fijo desaparecido detectado")
    sin_opc = [l for l in A]  # 'solo en esta pasada' (min 0) puede faltar
    ok(comparar(rj, [firma(sin_opc)]) == [], "tipo opcional ausente tolerado")
    ok(any("unico" in d for d in comparar(rj, [firma(A + ["[online] hosting on port 6"])])), "evento unico duplicado detectado")
    ok(any("juegos" in d for d in comparar(rj, [firma(A), firma(A)])), "numero de juegos distinto")

    for f in fallos:
        print("FALLO autoprueba:", f)
    print("autoprueba: %s" % ("fallos %d" % len(fallos) if fallos else "todo bien"))
    return 1 if fallos else 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="*", help="logs (A, B, ...)")
    ap.add_argument("--comparar", metavar="REFERENCIA", help="comparar los logs dados con un fichero de referencia")
    ap.add_argument("--generar-referencia", action="store_true", help="generar la referencia a partir de varias pasadas")
    ap.add_argument("--juegos", type=int, default=2, help="logs por pasada al generar (por defecto 2)")
    ap.add_argument("--nombre", action="append", default=[], help="nombre de jugador adicional a normalizar")
    ap.add_argument("--autoprueba", action="store_true", help="pruebas sinteticas")
    a = ap.parse_args(argv)

    if a.autoprueba:
        return autoprueba()
    if not a.logs:
        ap.print_usage(sys.stderr)
        return 2
    try:
        if a.generar_referencia:
            if a.juegos < 1 or len(a.logs) % a.juegos or len(a.logs) // a.juegos < 2:
                print("hacen falta al menos 2 pasadas completas de %d logs" % a.juegos, file=sys.stderr)
                return 2
            cs = [firma(leer_lineas(r), a.nombre) for r in a.logs]
            pasadas = [cs[i:i + a.juegos] for i in range(0, len(cs), a.juegos)]
            sys.stdout.write(generar(pasadas))
            return 0
        if a.comparar:
            with open(a.comparar, "r", encoding="utf-8") as f:
                _, ref = leer_referencia(f.read())
            dif = comparar(ref, [firma(leer_lineas(r), a.nombre) for r in a.logs])
            for d in dif:
                print(d)
            print("firma: %s" % ("DIFERENTE (%d diferencias)" % len(dif) if dif else "igual"))
            return 1 if dif else 0
        for i, r in enumerate(a.logs):
            c = firma(leer_lineas(r), a.nombre)
            print("== juego %s (%s): %d lineas [online], %d tipos ==" % (etiqueta(i), r.replace("\\", "/").rsplit("/", 1)[-1], sum(c.values()), len(c)))
            print(tabla(c))
        return 0
    except (OSError, ValueError) as e:
        print("error: %s" % e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
