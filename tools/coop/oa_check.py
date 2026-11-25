#!/usr/bin/env python3
"""Red de seguridad para reordenar runtime/online_actors.c sin cambiar nada (T206).

Parte un fichero C en "elementos" de nivel superior: cada definicion (funcion,
variable, tipo, macro, #include) con el comentario pegado encima (sin linea en
blanco por medio) y con las lineas de comentario sangradas que la siguen (la
continuacion de un comentario de final de linea). Un comentario separado por una
linea en blanco es un elemento propio. Solo biblioteca estandar. No compila nada.

Usos:
  oa_check.py --antes COMMIT:RUTA|RUTA --despues RUTA
      Compara los dos multiconjuntos de elementos, texto exacto (espacios y
      comentarios incluidos). Se ignoran en los dos lados los prototipos de
      funcion "static ...;" sin comentario y las lineas separadoras
      "// ==== ... ====". Cada elemento se compara junto con la region de
      preprocesador (#if/#ifdef/#ifndef de nivel superior) en la que esta.
      Ademas, para cada nombre de macro que aparece en un elemento, comprueba
      que la macro este definida antes de el en los dos ficheros o en ninguno
      (moverlo por encima o por debajo de un #define cambiaria su expansion).
      Imprime lo que sobra y lo que falta; sale con 0 (iguales) o 1.

  oa_check.py --listar RUTA
      Una linea por elemento: linea inicial-final, clase, nombres definidos.

  oa_check.py --orden RUTA
      Comprobacion de orden de declaracion (lo que el compilador pediria):
      para cada elemento, los nombres de nivel superior del fichero que usa
      (las macros, con lo que usan al expandirse) y que se definen mas abajo.
      Una funcion asi necesita un prototipo antes del uso; una variable, una
      macro, un tipo o una constante de enum es un error. Heuristica (no es un
      compilador): puede dar avisos de mas (una variable local que se llama como
      algo global), no de menos salvo nombres construidos con ## (no hay).
      Sale con 0 si no falta nada.

  oa_check.py --reordenar TABLA --antes COMMIT:RUTA|RUTA [--salida RUTA]
      Escribe el fichero reordenado segun el bloque "oa-reparto" de TABLA (un
      .md: ver specs/decisiones/ADR-021-reparto-oa.md). Cada definicion se
      mueve como unidad junto con lo que la precede desde la anterior (lineas en
      blanco, comentarios sueltos, prototipos originales). Dentro de un modulo
      se conserva el orden original. Cada modulo va tras un separador
      "// ==== oa_<modulo> ====". Los prototipos static que hagan falta se
      generan a partir de la cabecera de cada definicion y se escriben, tras el
      separador "// ==== prototipos ====", al final del modulo que diga la
      linea "prototipos tras <modulo>" de la tabla. Lo que esta antes del primer
      #if de nivel superior y el #endif final no se mueven.

  oa_check.py --tabla TABLA --antes COMMIT:RUTA|RUTA
      Imprime en Markdown la tabla elemento -> modulo que resulta de TABLA.

  oa_check.py --usos RUTA
      Para cada definicion, que elementos la usan (para decidir el reparto).

  oa_check.py --autoprueba
      Casos sinteticos: igual -> 0, linea, espacio, linea en blanco o comentario
      cambiados -> 1, bloque movido con su comentario -> 0, elemento sacado de su
      #ifndef -> 1, macro movida debajo de su uso -> 1; y la comprobacion de
      orden con una variable y una funcion usadas antes de definirse.

Bloque "oa-reparto" (en un bloque de codigo del .md, entre "oa-reparto:" y la
linea "fin"): una orden por linea. Una orden por nombre manda sobre cualquier
rango; entre ordenes de la misma clase, la posterior manda:
  <modulo> <desde>-<hasta>   las definiciones cuya linea inicial (la de su codigo,
                             sin el comentario) esta en ese rango del "antes"
  <modulo> <nombre> ...      las definiciones que definen ese nombre
  orden <modulo> <modulo>... el orden de los modulos en el fichero
  prototipos tras <modulo>
Las lineas que empiezan por '#' son comentarios.
"""

import argparse
import os
import re
import subprocess
import sys
from collections import Counter

SEP_RE = re.compile(r"^// ={4} .* ={4}\s*$")
IDENT_RE = re.compile(r"[A-Za-z_]\w*")
KEYWORDS = set(
    """auto break case char const continue default do double else enum extern float for goto if inline int long
    register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while
    _Bool _Static_assert _Alignof _Alignas bool true false __attribute__ __typeof__ typeof defined""".split()
)


# ---------------------------------------------------------------- lectura

def read_source(spec):
    """Texto de RUTA, o de COMMIT:RUTA con git show."""
    if os.path.exists(spec):
        with open(spec, "r", encoding="utf-8", newline="") as f:
            return f.read()
    if ":" in spec:
        out = subprocess.run(["git", "show", spec], capture_output=True)
        if out.returncode != 0:
            sys.exit("oa_check: git show %s: %s" % (spec, out.stderr.decode("utf-8", "replace").strip()))
        return out.stdout.decode("utf-8")
    sys.exit("oa_check: no existe %s" % spec)


def strip_line(line, in_block):
    """La linea sin comentarios y con el contenido de cadenas y caracteres vaciado.
    Devuelve (codigo, sigue_en_comentario_de_bloque)."""
    out = []
    i, n = 0, len(line)
    while i < n:
        if in_block:
            j = line.find("*/", i)
            if j < 0:
                return "".join(out), True
            i, in_block = j + 2, False
            out.append(" ")
            continue
        c = line[i]
        if c == "/" and line.startswith("//", i):
            break
        if c == "/" and line.startswith("/*", i):
            in_block, i = True, i + 2
            continue
        if c in "\"'":
            q, i = c, i + 1
            while i < n and line[i] != q:
                i += 2 if line[i] == "\\" else 1
            out.append(q + q)
            i += 1
            continue
        out.append(c)
        i += 1
    return "".join(out), in_block


# ---------------------------------------------------------------- particion

class Item:
    def __init__(self, start):
        self.start = start  # indice (0) de su primera linea
        self.code_start = None  # indice de su primera linea con codigo
        self.lines = []
        self.code = []  # codigo de cada linea (sin comentarios)
        self.kind = None  # pp, func, proto, decl, comment
        self.region = ()
        self.names = []
        self.is_func = False

    @property
    def end(self):
        return self.start + len(self.lines) - 1

    @property
    def text(self):
        return "\n".join(self.lines)

    @property
    def codetext(self):
        return "\n".join(self.code)


def split_items(text, drop_separators=False):
    """Lista de elementos de nivel superior. Con drop_separators, las lineas
    "// ==== ... ====" de nivel superior se quitan antes (y se cuentan)."""
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    lines = [l[:-1] if l.endswith("\r") else l for l in lines]
    items, cur, pending = [], None, None
    in_block = False
    depth = 0
    seps = 0
    region = []
    prev_ended = None  # elemento que termino en la linea anterior

    def finish(it):
        nonlocal prev_ended
        classify(it)
        it.region = tuple(region)
        items.append(it)
        prev_ended = it
        code = it.codetext.strip()
        if it.kind == "pp":
            m = re.match(r"#\s*(\w+)", code)
            d = m.group(1) if m else ""
            if d in ("if", "ifdef", "ifndef"):
                region.append(code.split("//")[0].strip())
                it.region = tuple(region[:-1])
            elif d in ("else", "elif"):
                if region:
                    region[-1] = region[-1] + " | " + code
                it.region = tuple(region)
            elif d == "endif":
                if region:
                    region.pop()
                it.region = tuple(region)

    i = 0
    while i < len(lines):
        line = lines[i]
        if cur is None:
            if drop_separators and not in_block and SEP_RE.match(line):
                seps += 1
                i += 1
                continue
            if not in_block and line.strip() == "":
                if pending is not None:
                    pending.kind = "comment"
                    finish(pending)
                    pending = None
                prev_ended = None
                i += 1
                continue
            code, nb = strip_line(line, in_block)
            if code.strip() == "":
                # solo comentario
                if (pending is None and prev_ended is not None and prev_ended.kind != "comment"
                        and (line[:1] in " \t" or in_block)):
                    prev_ended.lines.append(line)
                    prev_ended.code.append(code)
                    in_block = nb
                    i += 1
                    continue
                if pending is None:
                    pending = Item(i)
                pending.lines.append(line)
                pending.code.append(code)
                in_block = nb
                prev_ended = None
                i += 1
                continue
            it = pending if pending is not None else Item(i)
            pending = None
            it.code_start = i
            if code.lstrip().startswith("#"):
                while True:
                    it.lines.append(lines[i])
                    c, in_block = strip_line(lines[i], in_block)
                    it.code.append(c)
                    if not lines[i].endswith("\\") or i + 1 >= len(lines):
                        break
                    i += 1
                it.kind = "pp"
                finish(it)
                i += 1
                continue
            cur = it
            cur.brace_seen = False
            cur.sig = ""  # codigo significativo acumulado a profundidad 0 (para ver si es funcion)
        # dentro de un elemento
        code, in_block = strip_line(line, in_block)
        cur.lines.append(line)
        cur.code.append(code)
        if not code.lstrip().startswith("#"):
            for ch in code:
                if ch in "([{":
                    if ch == "{" and depth == 0 and not cur.brace_seen:
                        cur.brace_seen = True
                        cur.is_func = cur.sig.rstrip().endswith(")") and "=" not in cur.sig
                    if depth == 0:
                        cur.sig += ch
                    depth += 1
                elif ch in ")]}":
                    depth -= 1
                    if depth == 0:
                        cur.sig += ch
                elif depth == 0:
                    cur.sig += ch
        last = code.rstrip()[-1:] if code.strip() else ""
        if depth == 0 and not in_block and (last == ";" or (last == "}" and cur.is_func)):
            finish(cur)
            cur = None
        i += 1
    if cur is not None:
        cur.kind = "partial"
        finish(cur)
    if pending is not None:
        pending.kind = "comment"
        finish(pending)
    return items, seps


def depth0(s):
    """s con el contenido de (), [] y {} quitado (deja los delimitadores)."""
    out, d = [], 0
    for ch in s:
        if ch in "([{":
            if d == 0:
                out.append(ch)
            d += 1
        elif ch in ")]}":
            d -= 1
            if d == 0:
                out.append(ch)
        elif d == 0:
            out.append(ch)
    return "".join(out)


def split_top_commas(s):
    parts, d, cur = [], 0, []
    for ch in s:
        if ch in "([{":
            d += 1
        elif ch in ")]}":
            d -= 1
        if ch == "," and d == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    parts.append("".join(cur))
    return parts


def cut_top_eq(s):
    d = 0
    for k, ch in enumerate(s):
        if ch in "([{":
            d += 1
        elif ch in ")]}":
            d -= 1
        elif ch == "=" and d == 0:
            return s[:k], s[k + 1:]
    return s, ""


def declarator_name(d):
    d = re.sub(r"\[[^\]]*\]", "", d)
    m = re.search(r"\(\s*\*\s*(?:const\s+)?([A-Za-z_]\w*)\s*\)", d)
    if m:
        return m.group(1)
    ids = [x for x in IDENT_RE.findall(depth0(d)) if x not in KEYWORDS]
    return ids[-1] if ids else None


def enum_consts(code):
    out = []
    for m in re.finditer(r"\benum\b[^{;]*\{([^}]*)\}", code):
        for part in m.group(1).split(","):
            mm = IDENT_RE.match(part.strip())
            if mm:
                out.append(mm.group(0))
    return out


PROTO_RE = re.compile(r"^static\s+[\w\s\*]*?[\w\*]\s*\b([A-Za-z_]\w*)\s*\((?!\s*\*)[^;{}=]*\)\s*;$", re.S)


def classify(it):
    code = " ".join(c.strip() for c in it.code if c.strip())
    if it.kind == "comment":
        return
    if it.kind == "pp":
        m = re.match(r"#\s*define\s+(\w+)", code)
        it.names = [m.group(1)] if m else []
        return
    if it.is_func:
        head = code[: code.index("{")]
        for m in re.finditer(r"([A-Za-z_]\w*)\s*\(", head):
            if m.group(1) not in ("__attribute__", "__declspec") and m.group(1) not in KEYWORDS:
                it.names = [m.group(1)]
                break
        it.kind = "func"
        it.header = "\n".join(it.lines[it.code_start - it.start:])
        it.header = it.header[: it.header.index("{")] if "{" in it.header else it.header
        return
    if not it.lines[it.code_start - it.start:] or not code.endswith(";"):
        it.kind = "partial"
        return
    if PROTO_RE.match(code) or re.match(r"^[\w\s\*]+\b[A-Za-z_]\w*\s*\((?!\s*\*)[^;{}=]*\)\s*;$", code):
        # declaracion de funcion. "Suelta" (se ignora al comparar): static, sin ningun comentario
        it.kind = "proto"
        m = re.search(r"\b([A-Za-z_]\w*)\s*\((?!\s*\*)", code)
        it.names = [m.group(1)]
        it.bare = bool(PROTO_RE.match(code)) and "//" not in it.text and "/*" not in it.text
        return
    it.kind = "decl"
    names = []
    body = code.rstrip(";")
    if body.startswith("typedef"):
        names.append(declarator_name(depth0(body) if "(" not in depth0(body) else body))
        m = re.match(r"typedef\s+(?:struct|union|enum)\s+([A-Za-z_]\w*)", body)
        if m:
            names.append(m.group(1))
        it.typedef = True
    elif re.match(r"^(struct|union|enum)\s+\w*\s*\{[^;]*\}$", body, re.S) or re.match(r"^enum\s*\{", body):
        m = re.match(r"^(?:struct|union|enum)\s+([A-Za-z_]\w*)", body)
        if m:
            names.append(m.group(1))
    else:
        for part in split_top_commas(body):
            decl, _ = cut_top_eq(part)
            n = declarator_name(decl)
            if n:
                names.append(n)
    names += enum_consts(body)
    it.names = [n for n in names if n]


def comparable(items):
    """Elementos que cuentan para la comparacion (sin prototipos sueltos)."""
    return [it for it in items if not is_bare(it)]


def is_bare(it):
    return it.kind == "proto" and getattr(it, "bare", False)


# ---------------------------------------------------------------- usos y orden

def item_uses(it, macro_names, type_names):
    """Identificadores que usa un elemento (sin los suyos)."""
    code = it.codetext
    if it.kind == "pp":
        m = re.match(r"\s*#\s*define\s+(\w+)(\([^)]*\))?(.*)", code, re.S)
        if not m:
            return set()
        params = set(IDENT_RE.findall(m.group(2) or ""))
        return {x for x in IDENT_RE.findall(m.group(3)) if x not in params and x not in KEYWORDS}
    ids = set()
    for m in IDENT_RE.finditer(code):
        pre = code[: m.start()].rstrip()
        if pre.endswith(".") or pre.endswith("->"):
            continue
        ids.add(m.group(0))
    ids -= KEYWORDS
    ids -= set(it.names)
    if it.kind == "decl" or it.kind == "proto":
        # dentro de un struct solo cuentan macros y tipos; en un inicializador, todo
        flat = " ".join(c for c in it.code)
        keep = {x for x in ids if x in macro_names or x in type_names}
        for part in split_top_commas(flat.rstrip().rstrip(";")):
            _, init = cut_top_eq(part)
            if init:
                for m in IDENT_RE.finditer(init):
                    pre = init[: m.start()].rstrip()
                    if not (pre.endswith(".") or pre.endswith("->")):
                        keep.add(m.group(0))
        ids = keep - KEYWORDS - set(it.names)
    return ids


def order_problems(items):
    """[(elemento, nombre, clase_del_nombre, elemento_que_lo_define)] usados antes de definirse
    (las funciones, solo si no hay antes un prototipo)."""
    defs, kinds = {}, {}
    protos = {}
    macro_names, type_names = set(), set()
    for k, it in enumerate(items):
        if it.kind == "proto":
            for n in it.names:
                protos.setdefault(n, []).append(k)
            continue
        for n in it.names:
            if n not in defs:
                defs[n], kinds[n] = k, it.kind
            if it.kind == "pp":
                macro_names.add(n)
            if getattr(it, "typedef", False):
                type_names.add(n)
    uses = [item_uses(it, macro_names, type_names) for it in items]
    macro_uses = {}
    for k, it in enumerate(items):
        if it.kind == "pp" and it.names:
            macro_uses[it.names[0]] = uses[k]
    out = []
    for k, it in enumerate(items):
        if it.kind == "comment":
            continue
        seen, todo = set(), list(uses[k])
        while todo:
            n = todo.pop()
            if n in seen or n not in defs:
                continue
            seen.add(n)
            d = defs[n]
            if kinds[n] == "pp" and d < k:
                todo.extend(macro_uses.get(n, ()))
            if d >= k and d != k:
                if kinds[n] == "func" and any(p < k for p in protos.get(n, ())):
                    continue
                out.append((k, n, kinds[n], d))
    return out, defs


def macro_context(items):
    """Para cada elemento (por texto), las macros que nombra y si estan definidas antes."""
    defined = set()
    allmacros = {n for it in items if it.kind == "pp" for n in it.names}
    res = Counter()
    for it in items:
        ids = set(IDENT_RE.findall(it.codetext)) & allmacros
        ids -= set(it.names) if it.kind == "pp" else set()
        if ids and it.kind != "comment":
            res[(it.text, tuple(sorted((n, n in defined) for n in ids)))] += 1
        if it.kind == "pp":
            defined.update(it.names)
    return res


# ---------------------------------------------------------------- modos

def cmd_compare(antes, despues):
    return compare_texts(read_source(antes), read_source(despues), print)


def compare_texts(a_text, d_text, print):
    a_items, a_seps = split_items(a_text, drop_separators=True)
    d_items, d_seps = split_items(d_text, drop_separators=True)
    rc = 0
    for name, its in (("antes", a_items), ("despues", d_items)):
        bad = [it for it in its if it.kind == "partial"]
        for it in bad:
            print("%s: elemento sin terminar en la linea %d" % (name, it.start + 1))
            rc = 1
    ca = Counter((it.region, it.text) for it in comparable(a_items))
    cd = Counter((it.region, it.text) for it in comparable(d_items))
    sobra, falta = cd - ca, ca - cd
    first_line = {}
    for it in d_items:
        first_line.setdefault((it.region, it.text), it.start + 1)
    for it in a_items:
        first_line.setdefault(("a",) + (it.region, it.text), it.start + 1)
    for (reg, t), n in sorted(sobra.items(), key=lambda kv: first_line.get(kv[0], 0)):
        rc = 1
        print("SOBRA en despues (x%d, linea %d, region %s):" % (n, first_line.get((reg, t), 0), " > ".join(reg) or "-"))
        print("    " + t.replace("\n", "\n    ")[:2000])
    for (reg, t), n in sorted(falta.items(), key=lambda kv: first_line.get(("a",) + kv[0], 0)):
        rc = 1
        print("FALTA en despues (x%d, linea antes %d, region %s):" % (n, first_line.get(("a",) + (reg, t), 0), " > ".join(reg) or "-"))
        print("    " + t.replace("\n", "\n    ")[:2000])
    # macros: definidas antes o no, igual en los dos
    ma, md = macro_context(a_items), macro_context(d_items)
    if not sobra and not falta and ma != md:
        rc = 1
        for (t, ctx), n in (md - ma).items():
            print("MACRO con otro contexto en despues: %s\n    %s" % (
                ", ".join("%s %s" % (m, "definida antes" if b else "NO definida antes") for m, b in ctx),
                t.split("\n")[0][:200]))
    # lo de antes del primer #if de nivel superior, en el mismo orden
    pa = [it.text for it in a_items if not it.region and not (it.kind == "pp" and re.match(r"\s*#\s*if", it.codetext))]
    pd = [it.text for it in d_items if not it.region and not (it.kind == "pp" and re.match(r"\s*#\s*if", it.codetext))]
    if pa != pd:
        rc = 1
        print("El orden de los elementos fuera de toda region #if ha cambiado")
    pa_n = sum(1 for it in a_items if is_bare(it))
    pd_n = sum(1 for it in d_items if is_bare(it))
    print("antes: %d elementos (%d prototipos sueltos ignorados, %d separadores)" % (len(a_items), pa_n, a_seps))
    print("despues: %d elementos (%d prototipos sueltos ignorados, %d separadores)" % (len(d_items), pd_n, d_seps))
    print("resultado: %s" % ("IGUALES" if rc == 0 else "DISTINTOS"))
    return rc


SELFTEST_C = """// cabecera
#include <stdio.h>
#define N 4  // cuatro
                // (continuacion)

#ifndef SOLO
static int g_a[N];
// suma
static int suma(int x) {
    int s = 0;

    for (int i = 0; i < N; i++) s += g_a[i] + x;
    return s;
}
static int uso(void);
static struct { int a; } g_b = {1};
static int uso(void) { return suma(g_b.a); }
#endif  // SOLO
"""


def cmd_selftest():
    """Casos sinteticos del comparador y de la comprobacion de orden. Sale con 0 si todos dan lo esperado."""
    base = SELFTEST_C
    lines = base.split("\n")
    i_suma = lines.index("// suma")
    i_end = lines.index("    return s;") + 1  # la llave que cierra suma
    i_b = lines.index("static struct { int a; } g_b = {1};")
    moved = lines[:i_suma] + [lines[i_b]] + lines[i_suma: i_end + 1] + lines[i_end + 1: i_b] + lines[i_b + 1:]
    sep = lines[:6] + ["", "// ==== oa_x ===="] + lines[6:]
    cases = [
        ("identico", base, 0, None),
        ("bloque movido con su comentario, separador y prototipo de mas", "\n".join(
            moved[:6] + ["// ==== oa_x ====", "static int suma(int x);"] + moved[6:]), 0, 0),
        ("linea alterada", base.replace("s += g_a[i] + x;", "s += g_a[i] - x;"), 1, None),
        ("espacio al final de una linea", base.replace("return s;", "return s; "), 1, None),
        ("linea en blanco quitada dentro de una funcion", base.replace("    int s = 0;\n\n", "    int s = 0;\n"), 1, None),
        ("comentario cambiado", base.replace("// suma", "// suma de todo"), 1, None),
        ("elemento sacado del #ifndef (mismo texto)", "\n".join(lines[:5] + [lines[6]] + [lines[5]] + lines[7:]), 1, 0),
        ("macro movida debajo de quien la usa", "\n".join(lines[:2] + lines[4:i_end + 1] + lines[2:4] + lines[i_end + 1:]),
         1, 1),
        ("variable usada antes de definirse", "\n".join(
            lines[:6] + lines[7:i_end + 1] + [lines[6]] + lines[i_end + 1:]), 0, 1),
        ("funcion sin prototipo antes de su uso", base.replace("static int uso(void);\n", "").replace(
            "static int suma(int x) {", "static int uso2(void) { return uso(); }\nstatic int suma(int x) {"), 1, 1),
        ("separador anadido", "\n".join(sep), 0, 0),
    ]
    rc = 0
    sink = lambda *a, **k: None
    for name, text, want_cmp, want_order in cases:
        got = compare_texts(base, text, sink)
        ok = got == want_cmp
        detail = "comparar=%d (esperado %d)" % (got, want_cmp)
        if want_order is not None:
            items, _ = split_items(text, drop_separators=True)
            probs, _ = order_problems(items)
            go = 1 if probs else 0
            ok = ok and go == want_order
            detail += ", orden=%d (esperado %d)" % (go, want_order)
        print("%s  %s: %s" % ("ok   " if ok else "FALLO", name, detail))
        rc |= 0 if ok else 1
    print("autoprueba: %s" % ("bien" if rc == 0 else "MAL"))
    return rc


def cmd_list(path):
    items, _ = split_items(read_source(path), drop_separators=True)
    for it in items:
        print("%5d-%-5d %-8s %s" % (it.start + 1, it.end + 1, it.kind, ",".join(it.names)))
    return 0


def report_order(items):
    probs, defs = order_problems(items)
    funcs = {}
    rc = 0
    for k, n, kind, d in probs:
        if kind == "func":
            funcs.setdefault(n, []).append(k)
        else:
            rc = 1
            print("USO ANTES DE DEFINIR: %s (%s, definido en la linea %d) en %s (linea %d)" % (
                n, kind, items[d].code_start + 1, ",".join(items[k].names) or items[k].kind, items[k].code_start + 1))
    for n, ks in sorted(funcs.items(), key=lambda kv: defs[kv[0]]):
        rc = 1
        print("FALTA PROTOTIPO: %s (definida en la linea %d), usada antes en: %s" % (
            n, items[defs[n]].code_start + 1,
            ", ".join("%s:%d" % (",".join(items[k].names) or items[k].kind, items[k].code_start + 1) for k in ks)))
    return rc, funcs, defs


def cmd_xref(path):
    """Para cada definicion, quien la usa (nombre del elemento que la usa)."""
    items, _ = split_items(read_source(path), drop_separators=True)
    macro_names = {n for it in items if it.kind == "pp" for n in it.names}
    type_names = {n for it in items if getattr(it, "typedef", False) for n in it.names}
    owner = {}
    for it in items:
        if it.kind not in ("proto", "comment"):
            for n in it.names:
                owner.setdefault(n, it)
    users = {}
    for it in items:
        if it.kind in ("comment", "proto"):
            continue
        for n in item_uses(it, macro_names, type_names):
            if n in owner and owner[n] is not it:
                users.setdefault(n, []).append(",".join(it.names) or "(linea %d)" % (it.code_start + 1))
    for it in items:
        if it.kind in ("comment", "proto"):
            continue
        for n in it.names:
            print("%5d %-6s %s <- %s" % (it.code_start + 1, it.kind, n, " ".join(users.get(n, ["-"]))))
    return 0


def cmd_order(path):
    items, _ = split_items(read_source(path), drop_separators=True)
    rc, _, _ = report_order(items)
    print("orden: %s" % ("sin problemas" if rc == 0 else "HAY PROBLEMAS"))
    return rc


def parse_table(path):
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"^oa-reparto:\s*$(.*?)^fin\s*$", text, re.S | re.M)
    if not m:
        sys.exit("oa_check: %s no tiene el bloque oa-reparto: ... fin" % path)
    rules, order, protos_after = [], [], None
    for raw in m.group(1).split("\n"):
        line = raw.split("#", 1)[0].strip() if not raw.strip().startswith("#") else ""
        if not line:
            continue
        w = line.split()
        if w[0] == "orden":
            order = w[1:]
        elif w[0] == "prototipos" and len(w) == 3 and w[1] == "tras":
            protos_after = w[2]
        elif len(w) == 2 and re.match(r"^\d+-\d+$", w[1]):
            a, b = map(int, w[1].split("-"))
            rules.append((w[0], ("rango", a, b)))
        else:
            for n in w[1:]:
                rules.append((w[0], ("nombre", n)))
    if not order:
        sys.exit("oa_check: falta la linea 'orden ...' en el bloque oa-reparto")
    return rules, order, protos_after


def units_of(items):
    """Unidades movibles: (lista de indices de elementos, indice del ancla). El ancla es la definicion; antes van
    los comentarios sueltos y prototipos que la preceden."""
    units, acc = [], []
    for k, it in enumerate(items):
        acc.append(k)
        if it.kind in ("comment",) or (is_bare(it)):
            continue
        units.append((acc, k))
        acc = []
    if acc:
        units.append((acc, acc[-1]))
    return units


def assign(items, rules, order):
    units = units_of(items)
    # zona movible: tras el primer #if de nivel superior y antes de su #endif
    first_if = next(k for k, it in enumerate(items) if it.kind == "pp" and re.match(r"\s*#\s*if", it.codetext))
    last_endif = max(k for k, it in enumerate(items) if it.kind == "pp" and re.match(r"\s*#\s*endif", it.codetext))
    res = []
    used_names = set()
    for idxs, a in units:
        if a <= first_if or a >= last_endif:
            res.append((idxs, a, None))
            continue
        it = items[a]
        mod = by_name = None
        for m, r in rules:
            if r[0] == "rango" and r[1] <= it.code_start + 1 <= r[2]:
                mod = m
            elif r[0] == "nombre" and r[1] in it.names:
                by_name = m
                used_names.add(r[1])
        mod = by_name or mod
        if mod is None:
            sys.exit("oa_check: sin modulo: linea %d %s" % (it.code_start + 1, ",".join(it.names)))
        if mod not in order:
            sys.exit("oa_check: modulo %s no esta en 'orden'" % mod)
        res.append((idxs, a, mod))
    for m, r in rules:
        if r[0] == "nombre" and r[1] not in used_names:
            sys.exit("oa_check: el nombre %s de la tabla no se define en el fichero" % r[1])
    return res, first_if, last_endif


def build(items, lines_of, res, order, protos_after, first_if, protos=()):
    out = []
    for idxs, a, mod in res:
        if mod is None and a <= first_if:
            out += lines_of(idxs)
    for mod in order:
        units = [u for u in res if u[2] == mod]
        if not units:
            continue
        out += ["", "// ==== oa_%s ====" % mod]
        for idxs, a, m in units:
            out += lines_of(idxs)
        if mod == protos_after and protos:
            out += ["", "// ==== prototipos ===="]
            out += list(protos)
    for idxs, a, mod in res:
        if mod is None and a > first_if:
            out += lines_of(idxs)
    return out


def cmd_reorder(table, antes, salida, only_table=False):
    src = read_source(antes)
    items, seps = split_items(src)
    if seps:
        sys.exit("oa_check: el 'antes' ya tiene separadores")
    rules, order, protos_after = parse_table(table)
    res, first_if, last_endif = assign(items, rules, order)
    if only_table:
        print("| Modulo | Elemento | Clase | Linea en el antes |")
        print("|---|---|---|---|")
        for idxs, a, mod in res:
            it = items[a]
            if mod is None:
                continue
            print("| %s | %s | %s | %d |" % (mod, ", ".join("`%s`" % n for n in it.names) or "(%s)" % it.kind,
                                               {"func": "funcion", "decl": "variable/tipo", "pp": "macro",
                                                "proto": "declaracion"}.get(it.kind, it.kind), it.code_start + 1))
        counts = Counter(mod for _, _, mod in res if mod)
        print()
        for mod in order:
            print("- `%s`: %d definiciones" % (mod, counts.get(mod, 0)))
        return 0
    span = {}
    prev_end = -1
    for idxs, a, mod in res:
        # cada unidad lleva las lineas (en blanco incluidas) desde el final de la anterior
        start = prev_end + 1
        end = items[idxs[-1]].end
        span[a] = (start, end)
        prev_end = end
    all_lines = src.split("\n")
    trailing = []
    if all_lines and all_lines[-1] == "":
        all_lines.pop()
        trailing = [""]

    def lines_of(idxs):
        a = idxs[-1]
        s, e = span[a]
        return all_lines[s: e + 1]

    tail_from = span[res[-1][1]][1] + 1
    # primera pasada sin prototipos, para ver cuales hacen falta
    out = build(items, lines_of, res, order, protos_after, first_if)
    new_items, _ = split_items("\n".join(out + all_lines[tail_from:]) + "\n", drop_separators=True)
    probs, defs = order_problems(new_items)
    need = []
    for k, n, kind, d in probs:
        if kind == "func" and n not in need:
            need.append(n)
    protos = []
    for n in sorted(need, key=lambda n: defs[n]):
        h = new_items[defs[n]].header
        h = " ".join(x.strip() for x in h.split("\n"))
        protos.append(h.rstrip() + ";")
    if protos and not protos_after:
        sys.exit("oa_check: hacen falta prototipos y la tabla no dice 'prototipos tras <modulo>'")
    out = build(items, lines_of, res, order, protos_after, first_if, protos)
    out += all_lines[tail_from:]
    data = "\n".join(out) + ("\n" if trailing else "")
    with open(salida, "w", encoding="utf-8", newline="") as f:
        f.write(data)
    print("escrito %s: %d lineas, %d prototipos anadidos" % (salida, len(out), len(protos)))
    for p in protos:
        print("  " + p)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--antes")
    ap.add_argument("--despues")
    ap.add_argument("--listar")
    ap.add_argument("--orden")
    ap.add_argument("--usos")
    ap.add_argument("--reordenar")
    ap.add_argument("--tabla")
    ap.add_argument("--salida")
    ap.add_argument("--autoprueba", action="store_true")
    a = ap.parse_args()
    if a.autoprueba:
        return cmd_selftest()
    if a.listar:
        return cmd_list(a.listar)
    if a.orden:
        return cmd_order(a.orden)
    if a.usos:
        return cmd_xref(a.usos)
    if a.tabla:
        if not a.antes:
            ap.error("--tabla necesita --antes")
        return cmd_reorder(a.tabla, a.antes, None, only_table=True)
    if a.reordenar:
        if not a.antes or not a.salida:
            ap.error("--reordenar necesita --antes y --salida")
        return cmd_reorder(a.reordenar, a.antes, a.salida)
    if a.antes and a.despues:
        return cmd_compare(a.antes, a.despues)
    ap.error("nada que hacer: --antes y --despues, --listar, --orden, --reordenar o --tabla")


if __name__ == "__main__":
    sys.exit(main())
