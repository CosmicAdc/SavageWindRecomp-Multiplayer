#!/usr/bin/env python3
"""Inventario de las variables que sobreviven a un reinicio en caliente del juego en el codigo en linea (T211).

"Salir al inicio" borra la memoria del juego y la vuelve a crear dentro del mismo proceso, pero las variables
estaticas del runtime siguen ahi. La regla (AGENTS.md, runtime/hle.h RT_ON_RESET, constitucion regla 8) es
limpiarlas en una funcion RT_ON_RESET. Este guion las encuentra y comprueba que cada una este limpiada o
permitida. Solo biblioteca estandar. No compila nada: es una heuristica con expresiones regulares sobre el
texto sin comentarios ni cadenas, no un compilador.

Que cuenta como variable:
  - "fichero": toda definicion de variable de nivel superior, con "static" o sin el. Las de sin "static" son las
    que T207 exporto de un modulo a otro (oa_g_act, oa_g_pool...): guardan el mismo estado que antes, asi que
    entran. No cuentan los "extern", los "typedef", los prototipos ni las funciones (tampoco las que devuelven un
    puntero a funcion).
  - "funcion": toda declaracion "static" dentro del cuerpo de una funcion (una local sin "static" muere con la
    llamada).
  Un "static const" cuenta (se lista; la clasificacion dira "permitir": no cambia).

Claves (las de la lista de permitidas y las de la tabla del documento):
  fichero:nombre            una de fichero (fichero = nombre base, p. ej. oa_core.c)
  fichero:funcion:nombre    una estatica de funcion

Usos:
  oa_statics.py [RUTA...]
      Modo comprobacion. Sale con 0 solo si cada variable esta en la lista de permitidas o, siendo de fichero,
      aparece asignada (=, op=, memset/memcpy/memmove de ella) dentro del cuerpo de una funcion RT_ON_RESET(...)
      de su propio fichero. Una estatica de funcion no se puede limpiar desde fuera: o se permite o se sube a
      nivel de fichero (T212 paso 2). Lista las pendientes y las entradas de la lista que ya no existen.
  oa_statics.py --listar [RUTA...]
      Una linea por variable: fichero:linea, ambito, funcion, nombre, tipo, clave (y "(sin static)").
  oa_statics.py --clasificacion DOC [RUTA...]
      Comprueba que la tabla "oa-estaticas:" ... "fin" de DOC (un .md; una linea "clave clase motivo", clase
      puntero | partida | permitir, con "?" al final si es dudosa) clasifica el 100 % de las variables, sin
      sobrar ninguna, y que las "permitir" son exactamente las de la lista de permitidas. Sale con 0 o 1.
  oa_statics.py --autoprueba
      Casos sinteticos del escaner y de la comprobacion. Sale con 0 si todos dan lo esperado.

Opciones: --permitidas RUTA (por defecto tools/coop/oa_statics_permitidas.txt). Sin RUTA, los ficheros son
runtime/oa_*.c, runtime/online.c y runtime/online_actors.c (sus listas de nombres de red).
Lista de permitidas: una linea "clave  motivo"; una linea que empieza por '#' es un comentario.
"""

import argparse
import glob
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEFAULT_GLOBS = ["runtime/oa_*.c", "runtime/online.c", "runtime/online_actors.c"]
DEFAULT_ALLOWED = "tools/coop/oa_statics_permitidas.txt"
CLASSES = ("puntero", "partida", "permitir")

IDENT_RE = re.compile(r"[A-Za-z_]\w*")
KEYWORDS = set(
    """auto break case char const continue default do double else enum extern float for goto if inline int long
    register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while
    _Bool _Static_assert static_assert _Alignof _Alignas _Atomic _Thread_local __thread __attribute__ __typeof__
    typeof __declspec __inline __inline__ __restrict __volatile__ __extension__""".split()
)
PAREN_KEYWORDS = {"__attribute__", "_Alignas", "__declspec", "__typeof__", "typeof", "_Atomic"}  # (su "(...)" no es declarador


# ---------------------------------------------------------------- texto sin comentarios

def clean(text):
    """El texto con los comentarios, el contenido de cadenas y caracteres y las lineas de preprocesador
    (con sus continuaciones) cambiados por espacios. Misma longitud y mismos saltos de linea."""
    out = list(text)
    i, n = 0, len(text)
    line_start = True
    while i < n:
        c = text[i]
        if line_start:
            j = i
            while j < n and text[j] in " \t":
                j += 1
            if j < n and text[j] == "#":  # directiva: hasta el final de linea sin "\" delante
                k = j
                while k < n:
                    if text[k] == "\n" and text[k - 1] != "\\":
                        break
                    if text.startswith("/*", k):  # (un comentario de bloque dentro de la directiva)
                        e = text.find("*/", k + 2)
                        k = n if e < 0 else e + 2
                        continue
                    k += 1
                for m in range(i, k):
                    if out[m] != "\n":
                        out[m] = " "
                i, line_start = k, False
                continue
            line_start = False
        if c == "\n":
            line_start = True
            i += 1
            continue
        if text.startswith("//", i):
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
            continue
        if text.startswith("/*", i):
            e = text.find("*/", i + 2)
            e = n if e < 0 else e + 2
            for m in range(i, e):
                if out[m] != "\n":
                    out[m] = " "
            i = e
            continue
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            for m in range(i + 1, min(j, n)):
                out[m] = " "
            i = j + 1
            continue
        i += 1
    return "".join(out)


def match_close(s, i):
    """Indice del cierre que empareja la apertura s[i] (de "([{"), o len(s)."""
    pairs = {"(": ")", "[": "]", "{": "}"}
    stack = [pairs[s[i]]]
    j = i + 1
    while j < len(s) and stack:
        ch = s[j]
        if ch in pairs:
            stack.append(pairs[ch])
        elif ch in ")]}":
            if ch == stack[-1]:
                stack.pop()
        j += 1
    return j - 1 if not stack else len(s)


# ---------------------------------------------------------------- declaraciones

class Var:
    def __init__(self, path, line, scope, func, name, type_, static):
        self.path, self.line, self.scope, self.func = path, line, scope, func
        self.name, self.type, self.static = name, type_, static
        self.nth = 1  # (la n-esima estatica con ese nombre en esa funcion: bloques distintos)

    @property
    def file(self):
        return os.path.basename(self.path)

    @property
    def key(self):
        if self.scope == "funcion":
            return "%s:%s:%s%s" % (self.file, self.func, self.name, "#%d" % self.nth if self.nth > 1 else "")
        return "%s:%s" % (self.file, self.name)


def mask_groups(s, opens="[{"):
    """s con el contenido de los grupos [..] y {..} cambiado por espacios (los delimitadores quedan)."""
    out = list(s)
    i = 0
    while i < len(s):
        if s[i] in opens:
            j = match_close(s, i)
            for m in range(i + 1, min(j, len(s))):
                if out[m] != "\n":
                    out[m] = " "
            i = j + 1
            continue
        i += 1
    return "".join(out)


def split_top(s, sep):
    """Posiciones de sep en s fuera de (), [] y {}."""
    pos, depth = [], 0
    for i, ch in enumerate(s):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == sep and depth == 0:
            pos.append(i)
    return pos


def declarator_name(m):
    """(inicio, fin, es_funcion) del nombre declarado en la parte enmascarada m, o None.
    El nombre es el primer identificador que no es palabra clave, no va tras struct/union/enum y al que sigue
    "[", ")", "(", ",", "=", ";" o el final; se saltan los "(...)" de __attribute__ y compania."""
    i, prev = 0, ""
    while i < len(m):
        mt = IDENT_RE.match(m, i)
        if not mt:
            if m[i] not in " \t\n":
                prev = ""
            i += 1
            continue
        w, e = mt.group(0), mt.end()
        if i > 0 and (m[i - 1].isalnum() or m[i - 1] == "_"):
            i = e
            continue
        if w in PAREN_KEYWORDS:
            k = e
            while k < len(m) and m[k] in " \t\n":
                k += 1
            i = match_close(m, k) + 1 if k < len(m) and m[k] == "(" else e
            prev = w
            continue
        if w in KEYWORDS or prev in ("struct", "union", "enum"):
            prev, i = w, e
            continue
        k = e
        while k < len(m) and m[k] in " \t\n":
            k += 1
        nxt = m[k] if k < len(m) else ""
        after = IDENT_RE.match(m, k)
        if nxt in ("[", ")", "(", ",", "=", ";", "") or (after and after.group(0) in PAREN_KEYWORDS):
            return i, e, nxt == "("
        prev, i = w, e
    return None


def parse_decl(stmt, base_off, lines_at, path, scope, func):
    """Variables que declara una sentencia de declaracion (texto limpio, sin el ';' final)."""
    s = stmt
    st = s.lstrip()
    if not st:
        return []
    first = IDENT_RE.match(st)
    if first and first.group(0) in ("typedef", "extern", "_Static_assert", "static_assert", "return", "goto"):
        return []
    static = bool(re.match(r"(?:__attribute__\s*\(\(.*?\)\)\s*)?static\b", st, re.S))
    masked = mask_groups(s)
    commas = split_top(masked, ",")
    parts, start = [], 0
    for c in commas + [len(s)]:
        parts.append((start, c))
        start = c + 1
    out, base = [], None
    for k, (a, b) in enumerate(parts):
        pm, po = masked[a:b], s[a:b]
        eq = split_top(pm, "=")
        if eq:  # (sin el inicializador)
            pm, po = pm[: eq[0]], po[: eq[0]]
        found = declarator_name(pm)
        if not found:
            if k == 0:
                return []  # (struct X {...}; enum {...};: ningun declarador)
            continue
        ni, ne, is_func = found
        if is_func:
            continue
        name = pm[ni:ne]
        if k == 0:  # especificadores: hasta el primer '*' o '(' del declarador, o hasta el nombre
            pre = po[:ni]
            cut = first_decl_char(mask_groups(pre, "{"))
            base = pre[:cut]
            rest = pre[cut:] + po[ne:]
        else:
            rest = po[:ni] + po[ne:]
        type_ = squash_type((base or "") + " " + rest)
        line = lines_at(base_off + a + ni)
        out.append(Var(path, line, scope, func, name, type_, static))
    return out


def first_decl_char(pre):
    """Donde empieza el declarador en el texto de antes del nombre: el primer '*' o '(' que no es de
    __attribute__ y compania; sin ninguno, el final."""
    i = 0
    while i < len(pre):
        mt = IDENT_RE.match(pre, i)
        if mt and mt.group(0) in PAREN_KEYWORDS:
            k = mt.end()
            while k < len(pre) and pre[k] in " \t\n":
                k += 1
            i = match_close(pre, k) + 1 if k < len(pre) and pre[k] == "(" else mt.end()
            continue
        if mt:
            i = mt.end()
            continue
        if pre[i] in "*(":
            return i
        i += 1
    return len(pre)


def squash_type(t):
    """El tipo en una linea, sin "static" ni __attribute__((...))."""
    t = re.sub(r"\bstatic\b", " ", t)
    while True:
        m = re.search(r"\b__attribute__\s*\(", t)
        if not m:
            break
        e = match_close(t, m.end() - 1)
        t = t[: m.start()] + " " + t[e + 1:]
    t = re.sub(r"\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\}", "{...}", t, flags=re.S)
    t = re.sub(r"\s+", " ", t).strip()
    t = re.sub(r"\s*([\[\]()*,])\s*", r"\1", t)
    t = re.sub(r"([A-Za-z_0-9}])([*(\[])", r"\1 \2", t)
    t = re.sub(r",", ", ", t)
    return t


def func_name(header):
    """Nombre de la funcion que define una cabecera (texto limpio antes de su '{'), y si es RT_ON_RESET."""
    m = re.match(r"\s*RT_ON_RESET\s*\(\s*(\w+)\s*\)\s*$", header)
    if m:
        return m.group(1), True
    m = re.match(r"\s*HLE_FUNC\s*\(\s*\w+\s*,\s*(\w+)\s*\)\s*$", header)
    if m:
        return m.group(1), False
    found = declarator_name(mask_groups(header))
    if found:
        return header[found[0]: found[1]], False
    return "?", False


def scan_text(text, path):
    """(variables, {funcion_reset: cuerpo limpio}) de un fichero C."""
    c = clean(text)
    nl = [i for i, ch in enumerate(c) if ch == "\n"]

    def lines_at(off):
        lo, hi = 0, len(nl)
        while lo < hi:
            mid = (lo + hi) // 2
            if nl[mid] < off:
                lo = mid + 1
            else:
                hi = mid
        return lo + 1

    out, resets = [], {}
    i, s, n = 0, 0, len(c)
    while i < n:
        ch = c[i]
        if ch in "([":
            i = match_close(c, i) + 1
            continue
        if ch == ";":
            out += parse_decl(c[s:i], s, lines_at, path, "fichero", "")
            i += 1
            s = i
            continue
        if ch == "{":
            sofar = c[s:i]
            msofar = mask_groups(sofar)
            if split_top(msofar, "=") or re.search(r"\b(struct|union|enum)\b\s*\w*\s*$", msofar):
                i = match_close(c, i) + 1  # (cuerpo de un tipo o inicializador: sigue la sentencia)
                continue
            e = match_close(c, i)
            name, is_reset = func_name(sofar)
            body = c[i + 1: e]
            if is_reset:
                resets[name] = body
            out += scan_body(body, i + 1, lines_at, path, name)
            i = e + 1
            s = i
            continue
        i += 1
    seen, last = {}, {}
    for v in out:
        if v.scope == "funcion":
            seen[(v.func, v.name)] = v.nth = seen.get((v.func, v.name), 0) + 1
        else:
            last[v.name] = v  # (una de fichero declarada dos veces, "static T x;" y luego "static T x = ...;": una)
    out = [v for v in out if v.scope == "funcion" or last[v.name] is v]
    return out, resets


def scan_body(body, off, lines_at, path, func):
    out = []
    for m in re.finditer(r"(?<![\w.])static\b", body):
        a = m.start()
        if a > 0 and (body[a - 1].isalnum() or body[a - 1] == "_"):
            continue
        j = a
        while j < len(body) and body[j] != ";":
            j = match_close(body, j) + 1 if body[j] in "([{" else j + 1
        out += parse_decl(body[a:j], off + a, lines_at, path, "funcion", func)
    return out


def reset_assigns(var, bodies):
    """Si var aparece asignada en alguno de los cuerpos RT_ON_RESET."""
    n = re.escape(var.name)
    pats = [
        re.compile(r"(?<![\w.>])" + n + r"\b\s*(?:\[[^\]]*\]\s*|\.\s*\w+\s*)*(?:[-+*/%|&^]|<<|>>)?=(?!=)"),
        re.compile(r"\bmem(?:set|cpy|move)\s*\(\s*\(?\s*&?\s*" + n + r"\b"),
    ]
    return any(p.search(b) for b in bodies for p in pats)


# ---------------------------------------------------------------- ficheros, listas, documento

def default_paths():
    out = []
    for g in DEFAULT_GLOBS:
        out += sorted(glob.glob(os.path.join(ROOT, g)))
    return out


def rel(p):
    try:
        return os.path.relpath(p, ROOT).replace(os.sep, "/")
    except ValueError:
        return p.replace(os.sep, "/")


def scan_files(paths):
    allv, resets = [], {}
    for p in paths:
        with open(p, "r", encoding="utf-8", newline="") as f:
            text = f.read()
        vs, rs = scan_text(text, rel(p))
        allv += vs
        resets[os.path.basename(p)] = list(rs.values())
    return allv, resets


def read_allowed(path):
    """{clave: (motivo, linea)}"""
    out = {}
    if not os.path.exists(path):
        return out
    with open(path, "r", encoding="utf-8") as f:
        for ln, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):  # (solo al principio: "#2" va en las claves)
                continue
            parts = line.split(None, 1)
            out[parts[0]] = (parts[1] if len(parts) > 1 else "", ln)
    return out


def read_table(path):
    """{clave: (clase, dudosa, motivo)} del bloque "oa-estaticas:" ... "fin" de un .md."""
    with open(path, "r", encoding="utf-8") as f:
        lines = f.read().split("\n")
    out, inside, errs = {}, False, []
    for ln, raw in enumerate(lines, 1):
        line = raw.strip()
        if line == "oa-estaticas:":
            inside = True
            continue
        if not inside:
            continue
        if line == "fin":
            break
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 2)
        if len(parts) < 2:
            errs.append("%s:%d: linea sin clase: %s" % (path, ln, line))
            continue
        cls = parts[1]
        doubt = cls.endswith("?")
        cls = cls.rstrip("?")
        if cls not in CLASSES:
            errs.append("%s:%d: clase desconocida %r" % (path, ln, parts[1]))
            continue
        if parts[0] in out:
            errs.append("%s:%d: clave repetida %s" % (path, ln, parts[0]))
        out[parts[0]] = (cls, doubt, parts[2] if len(parts) > 2 else "")
    return out, errs


def check(vars_, resets, allowed, print=print):
    """0 si todo esta limpiado o permitido; si no, 1 (y lo dice)."""
    pending, bad = [], 0
    keys = {}
    for v in vars_:
        keys.setdefault(v.key, []).append(v)
    for key, vs in keys.items():
        if key in allowed:
            continue
        v = vs[0]
        if v.scope == "fichero" and reset_assigns(v, resets.get(v.file, [])):
            continue
        pending.append(v)
    for key, (_, ln) in sorted(allowed.items(), key=lambda kv: kv[1][1]):
        if key not in keys:
            print("permitida que no existe (linea %d de la lista): %s" % (ln, key))
            bad += 1
    if pending:
        print("pendientes de limpiar con RT_ON_RESET (o de permitir): %d" % len(pending))
        for v in pending:
            where = "" if v.scope == "fichero" else "  (de funcion: subirla a nivel de fichero)"
            print("  %s:%d  %s  %s%s" % (v.path, v.line, v.key, v.type, where))
    total = len(keys)
    print("variables: %d; permitidas: %d; limpiadas en RT_ON_RESET: %d; pendientes: %d"
          % (total, sum(1 for k in keys if k in allowed), total - len(pending) - sum(1 for k in keys if k in allowed),
             len(pending)))
    return 1 if pending or bad else 0


def classification(vars_, table, allowed, print=print):
    keys = {v.key for v in vars_}
    errs = 0
    for k in sorted(keys - set(table)):
        print("sin clasificar: %s" % k)
        errs += 1
    for k in sorted(set(table) - keys):
        print("clasificada pero no existe: %s" % k)
        errs += 1
    for k, (cls, _, _) in sorted(table.items()):
        if cls == "permitir" and k not in allowed:
            print("'permitir' en el documento y no en la lista: %s" % k)
            errs += 1
        if cls != "permitir" and k in allowed:
            print("en la lista de permitidas pero el documento dice %s: %s" % (cls, k))
            errs += 1
    for k in sorted(set(allowed) - set(table)):
        if k in keys:
            print("en la lista de permitidas y sin clasificar: %s" % k)
            errs += 1
    cnt = {c: sum(1 for k in keys if table.get(k, ("",))[0] == c) for c in CLASSES}
    doubt = sum(1 for k in keys if k in table and table[k][1])
    print("clasificadas %d de %d: puntero %d, partida %d, permitir %d (dudosas %d)"
          % (sum(cnt.values()), len(keys), cnt["puntero"], cnt["partida"], cnt["permitir"], doubt))
    return 1 if errs else 0


def listing(vars_, print=print):
    for v in vars_:
        print("%s:%d\t%s\t%s\t%s\t%s\t%s%s" % (v.path, v.line, v.scope, v.func or "-", v.name, v.type, v.key,
                                               "" if v.static or v.scope == "funcion" else "\t(sin static)"))
    nf = sum(1 for v in vars_ if v.scope == "fichero")
    print("total %d: de fichero %d (sin static %d), de funcion %d"
          % (len(vars_), nf, sum(1 for v in vars_ if v.scope == "fichero" and not v.static), len(vars_) - nf))


# ---------------------------------------------------------------- autoprueba

SELFTEST_SRC = r'''
#include <stdint.h>
#define WIDE(x) static int x##_wide;  /* (una macro: no se mira) */
typedef struct { int a; } T;
struct Tag { int b; };
struct Fwd;
extern int g_ext;
static int g_file = 1;          // de fichero
static const char *const k_names[] = {"a;b", "c{"};  // static const: cuenta
uint64_t g_global[4], *g_ptr;   // sin static: cuenta (dos)
static struct {
    uint64_t actor;
    int n;
} g_multi[8],
  g_multi2;                     // varias lineas
static int (*g_fp)(int);        // puntero a funcion: SI es variable
static void (*pick(int k))(void);  // funcion que devuelve un puntero a funcion: no
static char *name_of(int k);    // prototipo: no
static int add(int a, int b) { return a + b; }
static char *name_of(int k) {   // funcion que devuelve un puntero: no
    static const char *tab[2] = {"x", "y"};
    int local = 0;
    return (char *)tab[k & 1] + local;
}
static void (*pick(int k))(void) { return k ? 0 : 0; }
void work(void) {
    static uint64_t tr[4],
        seen;                   // de funcion, en dos lineas
    for (int i = 0; i < 4; i++) {
        static int deep = -1;   // de funcion, sangrada mas
        deep += tr[i] != 0;
    }
    (void)seen;
}
__attribute__((unused)) static float g_attr[3];
void twice(void) {
    { static int tick; tick++; }
    { static int tick; tick--; }   // otra con el mismo nombre en la misma funcion: clave "#2"
    static struct { uint64_t actor; } req __attribute__((aligned(16)));  // atributo tras el nombre
    (void)req;
}
static int g_tent;              // definicion tentativa...
static int g_tent = 2;          // ...y la de verdad: una variable
RT_ON_RESET(sample_reset) {
    g_file = 1;
    memset(g_multi, 0, sizeof g_multi);
    g_global[0] = g_global[1] = 0;
    if (g_fp == 0) g_ptr = 0;
}
'''

SELFTEST_EXPECT = [
    ("fichero", "", "g_file", "int"),
    ("fichero", "", "k_names", "const char *const []"),
    ("fichero", "", "g_global", "uint64_t [4]"),
    ("fichero", "", "g_ptr", "uint64_t *"),
    ("fichero", "", "g_multi", "struct {...} [8]"),
    ("fichero", "", "g_multi2", "struct {...}"),
    ("fichero", "", "g_fp", "int (*)(int)"),
    ("funcion", "name_of", "tab", "const char *[2]"),
    ("funcion", "work", "tr", "uint64_t [4]"),
    ("funcion", "work", "seen", "uint64_t"),
    ("funcion", "work", "deep", "int"),
    ("fichero", "", "g_attr", "float [3]"),
    ("funcion", "twice", "tick", "int"),
    ("funcion", "twice", "req", "struct {...}"),
    ("fichero", "", "g_tent", "int"),
]


def cmd_selftest():
    fails = 0

    def expect(what, got, want):
        nonlocal fails
        ok = got == want
        fails += not ok
        print("%s  %s%s" % ("ok  " if ok else "MAL ", what, "" if ok else "  (sale %r, se esperaba %r)" % (got, want)))

    vs, rs = scan_text(SELFTEST_SRC, "sint.c")
    got = [(v.scope, v.func, v.name, v.type) for v in vs]
    for e in SELFTEST_EXPECT:
        expect("encuentra %s %s%s (%s)" % (e[0], e[1] + ":" if e[1] else "", e[2], e[3]), e in got, True)
    expect("no encuentra nada mas (funciones, prototipos, extern, typedef, struct, macro, locales)",
           sorted(x[2] for x in got if x not in SELFTEST_EXPECT), [])
    lines = {v.name: v.line for v in vs}
    expect("linea de g_multi2 (segunda linea del declarador)", lines.get("g_multi2"), 15)
    expect("linea de seen (de funcion, segunda linea)", lines.get("seen"), 28)
    expect("RT_ON_RESET encontrado", list(rs), ["sample_reset"])
    expect("declarada dos veces a nivel de fichero: una, la ultima", [v.line for v in vs if v.name == "g_tent"], [43])
    expect("dos estaticas con el mismo nombre en una funcion: claves distintas",
           [v.key for v in vs if v.func == "twice" and v.name == "tick"], ["sint.c:twice:tick", "sint.c:twice:tick#2"])

    sink = []
    allowed = {"sint.c:k_names": ("tabla", 1), "sint.c:name_of:tab": ("tabla", 2), "sint.c:g_attr": ("x", 3)}
    r = check(vs, {"sint.c": list(rs.values())}, allowed, print=sink.append)
    pend = sorted(l.split()[1] for l in sink if l.startswith("  "))
    expect("comprobacion: pendientes las no limpiadas ni permitidas",
           pend, sorted(["sint.c:g_fp", "sint.c:g_multi2", "sint.c:work:tr", "sint.c:work:seen", "sint.c:work:deep",
                        "sint.c:twice:tick", "sint.c:twice:tick#2", "sint.c:twice:req", "sint.c:g_tent"]))
    expect("comprobacion: sale con 1 si hay pendientes", r, 1)
    full = dict(allowed, **{k: ("x", 9) for k in pend})
    sink = []
    expect("comprobacion: sale con 0 con todo limpiado o permitido", check(vs, {"sint.c": list(rs.values())}, full,
                                                                           print=sink.append), 0)
    sink = []
    stale = dict(full, **{"sint.c:no_existe": ("x", 10)})
    expect("comprobacion: una permitida que no existe da 1", check(vs, {"sint.c": list(rs.values())}, stale,
                                                                   print=sink.append), 1)
    sink = []
    expect("comprobacion: limpiar en el reset de OTRO fichero no cuenta",
           check(vs, {"otro.c": list(rs.values())}, allowed, print=sink.append) == 1
           and any("sint.c:g_file" in l for l in sink), True)
    v_eq = Var("sint.c", 1, "fichero", "", "g_fp", "", True)
    expect("una comparacion (==) no es una asignacion", reset_assigns(v_eq, ["if (g_fp == 0) x = 1;"]), False)

    table = {v.key: ("permitir" if v.key in full and v.key in allowed else "partida", False, "") for v in vs}
    sink = []
    expect("clasificacion completa y coherente: 0", classification(vs, table, allowed, print=sink.append), 0)
    t2 = dict(table)
    t2.pop("sint.c:g_ptr")
    sink = []
    expect("clasificacion: una sin clasificar da 1", classification(vs, t2, allowed, print=sink.append), 1)
    t3 = dict(table, **{"sint.c:g_file": ("permitir", False, "")})
    sink = []
    expect("clasificacion: 'permitir' que no esta en la lista da 1", classification(vs, t3, allowed,
                                                                                    print=sink.append), 1)
    print("autoprueba: %s" % ("bien" if not fails else "%d MAL" % fails))
    return 1 if fails else 0


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("rutas", nargs="*")
    ap.add_argument("--listar", action="store_true")
    ap.add_argument("--clasificacion", metavar="DOC")
    ap.add_argument("--permitidas", default=os.path.join(ROOT, DEFAULT_ALLOWED))
    ap.add_argument("--autoprueba", action="store_true")
    a = ap.parse_args()
    if a.autoprueba:
        return cmd_selftest()
    paths = a.rutas or default_paths()
    if not paths:
        sys.exit("oa_statics: ningun fichero")
    vars_, resets = scan_files(paths)
    if a.listar:
        listing(vars_)
        return 0
    allowed = read_allowed(a.permitidas)
    if a.clasificacion:
        table, errs = read_table(a.clasificacion)
        for e in errs:
            print(e)
        return 1 if classification(vars_, table, allowed) or errs else 0
    return check(vars_, resets, allowed)


if __name__ == "__main__":
    sys.exit(main())
