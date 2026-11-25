#!/usr/bin/env python3
"""Medir el minimapa del juego para dibujar al compañero encima (T445; ADR-022 / CU-16).

Se ejecuta con UN juego ya arrancado con SAVAGEWIND_CONTROL=<puerto>, en el mundo, Link quieto en un sitio llano y
abierto, sin menús ni eventos. Usa solo órdenes de docs/CONTROL.md (`sites`, `mem read`, `pos`, `camera`, `look`,
`teleport`, `time`, `get`, `set`, `shot`) a través de tools/swctl.py. NO escribe memoria del juego.

Subórdenes:
  paneles     Recorre el árbol de paneles del layout de AppMap (pantalla 0x2a, el minimapa en juego) con `mem read` y
              lista nombre, visibilidad, alfa, tamaño y matriz global (centro, giro y escala en unidades del layout
              1280x720, origen en el centro, y hacia arriba). Por defecto solo los visibles del cuarto inferior derecho.
  medir       El procedimiento completo de bitacora/2026-11-25-F4-T445-minimapa-investigacion.md §4: capturas y
              volcados de paneles en la posición inicial, con la cámara girada, tras mover a Link en +x y en +z, y
              con el HUD oculto; deja un JSON con centro, radio, escala, giro y dirección de los ejes en pantalla.
  prediccion  Pasa un círculo del layout a píxeles de ventana (o al revés con --inversa) con la misma cuenta que
              runtime/nvn/hud_scale.h y exec.c hud_patch: Llenar pantallas anchas, tamaño de HUD y HUD en los bordes.
  marcar      Dibuja un círculo y una cruz sobre un PNG, para comprobar a ojo un centro y un radio.

Nada de esto está verificado en el juego: los desplazamientos de memoria salen de leer runtime/ (ver la bitácora).
Solo biblioteca estándar de Python 3.8 o posterior.
"""
import argparse
import json
import math
import struct
import sys
import time
import zlib
from pathlib import Path

AQUI = Path(__file__).resolve().parent
RAIZ = AQUI.parent.parent
sys.path.insert(0, str(RAIZ / "tools"))
import swctl  # noqa: E402  (el mismo protocolo que tools/swctl.py)

# Desplazamientos leídos en runtime/ (juego 1.9.0; sin verificar con este guion)
SLOT_APPMAP = 0x2a          # ui_layout.c ui_layout_begin, framepace.c framepace_map_ui
MGR_CUENTA, MGR_PANTALLAS = 0x28, 0x30   # game_sites.h screen_mgr
PANT_LAYOUT, LAYOUT_RAIZ = 0x30, 0x18    # framepace.c (comentario de las tiendas): layout de la pantalla y su raíz
PANEL_HIJOS = 0x20          # pc_cursor.c pane_named: cabeza de la lista de hijos, enlazada 8 bytes dentro de cada hijo
PANEL_TAM = 0x50            # ancho, alto (pc_cursor.c options_box)
PANEL_BANDERAS, PANEL_ALFA = 0x58, 0x5a  # bit 0 visible; alfa global (pc_cursor.c, "visible, global alpha")
PANEL_MATRIZ = 0x70         # 3x4 global: fila 0 en +0x70..+0x7c (traslación x en +0x7c), fila 1 en +0x80..+0x8c
PANEL_NOMBRE = 0xb8         # nombre del panel (ui_layout.c, pc_cursor.c)
PANEL_LEER = 0xd0


class ErrorJuego(Exception):
    pass


# ------------------------------------------------------------------ el juego
class Juego:
    def __init__(self, puerto, host="127.0.0.1", timeout=10.0):
        self.puerto, self.host, self.timeout = puerto, host, timeout

    def orden(self, *args):
        args = [str(a) for a in args]
        conn = swctl.connect(self.host, self.puerto, self.timeout, 0)
        with conn:
            linea = swctl.exchange(conn, swctl.request_line(args), self.timeout)
        r = swctl.parse_reply(linea, args[0])
        if not r["ok"]:
            raise ErrorJuego(" ".join(args) + ": " + r["error"])
        return r["data"]

    def leer(self, dir_, n):
        out = b""
        while n > 0:
            k = min(n, 4096)
            out += bytes.fromhex(self.orden("mem", "read", hex(dir_), k)["hex"])
            dir_, n = dir_ + k, n - k
        return out

    def u64(self, dir_):
        return struct.unpack("<Q", self.leer(dir_, 8))[0]

    def esperar(self, orden, maximo=60.0):
        """`look status` / `teleport status` hasta `done`."""
        fin = time.monotonic() + maximo
        while time.monotonic() < fin:
            estado = self.orden(orden, "status").get("state", "")
            if estado == "done":
                return
            if estado in ("failed", "timeout", "idle"):
                raise ErrorJuego(f"{orden}: {estado}")
            time.sleep(0.2)
        raise ErrorJuego(f"{orden}: no terminó en {maximo:.0f} s")

    def mirar(self, yaw, pitch="keep"):
        self.orden("look", f"{yaw:.2f}", pitch)
        self.esperar("look", 20)
        time.sleep(0.5)  # (el minimapa anima su giro: unos fotogramas)

    def mover(self, x, y, z):
        self.orden("teleport", f"{x:.2f}", f"{y:.2f}", f"{z:.2f}")
        self.esperar("teleport", 90)
        time.sleep(2.0)  # (cae unos metros y se queda quieto)

    def ajuste(self, clave):
        d = self.orden("get", clave)
        return d.get(clave, next(iter(d.values()), None))


def raiz_appmap(j):
    """El panel raíz del layout de AppMap, o error con el eslabón que falla."""
    sitios = j.orden("sites")
    mgr = int(sitios.get("screen_mgr.value", "0"), 16)
    if not mgr:
        raise ErrorJuego("sites: screen_mgr sin valor (¿hay mundo?)")
    cuenta = struct.unpack("<I", j.leer(mgr + MGR_CUENTA, 4))[0]
    if cuenta <= SLOT_APPMAP or cuenta > 256:
        raise ErrorJuego(f"ScreenMgr: {cuenta} pantallas")
    pantalla = j.u64(j.u64(mgr + MGR_PANTALLAS) + 8 * SLOT_APPMAP)
    if not pantalla:
        raise ErrorJuego("AppMap (pantalla 0x2a) no existe ahora")
    estado = j.leer(pantalla + 0xfe, 5)  # (ui_layout.c: +0xfe, +0xff, +0x100, +0x102 deciden si dibuja)
    layout = j.u64(pantalla + PANT_LAYOUT)
    raiz = j.u64(layout + LAYOUT_RAIZ) if layout else 0
    if not raiz:
        raise ErrorJuego("AppMap: sin layout o sin panel raíz en +0x30 / +0x18")
    return pantalla, raiz, {"fe": estado[0], "ff": estado[1], "100": estado[2], "102": estado[4]}


def panel(j, dir_):
    b = j.leer(dir_, PANEL_LEER)
    f = lambda o: struct.unpack_from("<f", b, o)[0]  # noqa: E731
    nombre = b[PANEL_NOMBRE:PANEL_NOMBRE + 24].split(b"\0")[0].decode("latin-1")
    m = [f(PANEL_MATRIZ + 4 * k) for k in range(12)]  # filas 0 y 1 (y la 2, la profundidad)
    sx, sy = math.hypot(m[0], m[4]), math.hypot(m[1], m[5])
    return {
        "dir": hex(dir_), "nombre": nombre, "visible": bool(b[PANEL_BANDERAS] & 1), "alfa": b[PANEL_ALFA],
        "tam": [round(f(PANEL_TAM), 3), round(f(PANEL_TAM + 4), 3)],
        "centro": [round(m[3], 3), round(m[7], 3)],
        "giro": round(math.degrees(math.atan2(m[4], m[0])), 3),  # (en el layout, y hacia arriba: antihorario)
        "escala": [round(sx, 5), round(sy, 5)],
        "semiancho": [round(abs(m[0]) * f(PANEL_TAM) / 2 + abs(m[1]) * f(PANEL_TAM + 4) / 2, 3),
                      round(abs(m[4]) * f(PANEL_TAM) / 2 + abs(m[5]) * f(PANEL_TAM + 4) / 2, 3)],
        "hijos_cabeza": dir_ + PANEL_HIJOS,
    }


def arbol(j, raiz, presupuesto=2000):
    """Todos los paneles bajo la raíz, con su profundidad (el mismo recorrido que pc_cursor.c pane_named)."""
    out, pila = [], [(raiz, 0)]
    while pila and presupuesto > 0:
        dir_, prof = pila.pop()
        presupuesto -= 1
        p = panel(j, dir_)
        p["prof"] = prof
        out.append(p)
        cabeza = p.pop("hijos_cabeza")
        enlace, hijos = j.u64(cabeza + 8), []
        while enlace and enlace != cabeza and len(hijos) < 256:
            hijos.append(enlace - 8)
            enlace = j.u64(enlace + 8)
        pila.extend((h, prof + 1) for h in reversed(hijos))
    return out


def abajo_derecha(p):
    return p["visible"] and p["alfa"] > 0 and p["centro"][0] > 160 and p["centro"][1] < -90


# ------------------------------------------------------------------ PNG sin dependencias
def leer_png(ruta):
    d = Path(ruta).read_bytes()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{ruta}: no es PNG")
    pos, idat, cab = 8, [], None
    while pos < len(d):
        n, tipo = struct.unpack(">I4s", d[pos:pos + 8])
        cuerpo = d[pos + 8:pos + 8 + n]
        pos += 12 + n
        if tipo == b"IHDR":
            cab = struct.unpack(">IIBBBBB", cuerpo)
        elif tipo == b"IDAT":
            idat.append(cuerpo)
        elif tipo == b"IEND":
            break
    w, h, prof, color, _, _, entrelazado = cab
    if prof != 8 or color not in (2, 6) or entrelazado:
        raise ValueError(f"{ruta}: solo RGB/RGBA de 8 bits sin entrelazar (es {prof} bits, tipo {color})")
    bpp = 3 if color == 2 else 4
    crudo, paso = zlib.decompress(b"".join(idat)), w * bpp
    filas, prev, i = [], bytearray(paso), 0
    for _ in range(h):
        filtro, fila = crudo[i], bytearray(crudo[i + 1:i + 1 + paso])
        i += 1 + paso
        if filtro == 1:
            for k in range(bpp, paso):
                fila[k] = (fila[k] + fila[k - bpp]) & 255
        elif filtro == 2:
            for k in range(paso):
                fila[k] = (fila[k] + prev[k]) & 255
        elif filtro == 3:
            for k in range(paso):
                fila[k] = (fila[k] + ((fila[k - bpp] if k >= bpp else 0) + prev[k]) // 2) & 255
        elif filtro == 4:
            for k in range(paso):
                a, b, c = (fila[k - bpp] if k >= bpp else 0), prev[k], (prev[k - bpp] if k >= bpp else 0)
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                fila[k] = (fila[k] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        filas.append(fila)
        prev = fila
    return {"w": w, "h": h, "bpp": bpp, "filas": filas}


def escribir_png(ruta, img):
    w, h, bpp = img["w"], img["h"], img["bpp"]
    crudo = b"".join(b"\0" + bytes(f) for f in img["filas"])
    trozo = lambda t, c: struct.pack(">I", len(c)) + t + c + struct.pack(">I", zlib.crc32(t + c))  # noqa: E731
    Path(ruta).write_bytes(b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2 if bpp == 3 else 6,
                                                                              0, 0, 0))
                           + trozo(b"IDAT", zlib.compress(crudo, 6)) + trozo(b"IEND", b""))


def luma(img, x, y):
    if x < 0 or y < 0 or x >= img["w"] or y >= img["h"]:
        return None
    f, k = img["filas"][y], x * img["bpp"]
    return 0.299 * f[k] + 0.587 * f[k + 1] + 0.114 * f[k + 2]


def pintar(img, x, y, rgb):
    if 0 <= x < img["w"] and 0 <= y < img["h"]:
        f, k = img["filas"][y], x * img["bpp"]
        f[k:k + 3] = bytes(rgb)


def marcar_circulo(img, cx, cy, r, rgb=(255, 0, 255)):
    for i in range(int(2 * math.pi * r) * 2 + 8):
        t = i / (int(2 * math.pi * r) * 2 + 8) * 2 * math.pi
        pintar(img, round(cx + r * math.cos(t)), round(cy + r * math.sin(t)), rgb)
    for d in range(-6, 7):
        pintar(img, round(cx) + d, round(cy), rgb)
        pintar(img, round(cx), round(cy) + d, rgb)


# ------------------------------------------------------------------ análisis de capturas (y hacia abajo)
def puntos_anillo(r_in, r_out, paso):
    return [(x, y) for y in range(-r_out, r_out + 1, paso) for x in range(-r_out, r_out + 1, paso)
            if r_in * r_in <= x * x + y * y <= r_out * r_out]


def diferencia(a, b, cx, cy, pts, f):
    """Media de |a(c + p) - b(c + f(p))| sobre los puntos del anillo."""
    s, n = 0.0, 0
    for x, y in pts:
        u, v = f(x, y)
        la, lb = luma(a, cx + x, cy + y), luma(b, round(cx + u), round(cy + v))
        if la is not None and lb is not None:
            s, n = s + abs(la - lb), n + 1
    return s / n if n else float("inf")


def mejor_giro(a, b, cx, cy, r, paso_grados=1.0):
    """Ángulo (grados, horario en pantalla) que lleva el contenido del minimapa de a a b."""
    pts = puntos_anillo(int(r * 0.25), int(r * 0.85), 2)
    mejor = (float("inf"), 0.0)
    for i in range(int(360 / paso_grados)):
        t = math.radians(i * paso_grados)
        c, s = math.cos(t), math.sin(t)
        mejor = min(mejor, (diferencia(a, b, cx, cy, pts, lambda x, y: (c * x - s * y, s * x + c * y)), i * paso_grados))
    sin_giro = diferencia(a, b, cx, cy, pts, lambda x, y: (x, y))
    ang = mejor[1] if mejor[1] <= 180 else mejor[1] - 360
    return {"giro_horario": ang, "error": round(mejor[0], 2), "error_sin_giro": round(sin_giro, 2)}


def mejor_desplazamiento(a, b, cx, cy, r, maximo):
    """(dx, dy) en píxeles que lleva el contenido de a a b, de grueso a fino."""
    def busca(pts, centro, radio, paso):
        mejor = (float("inf"), centro)
        for dy in range(centro[1] - radio, centro[1] + radio + 1, paso):
            for dx in range(centro[0] - radio, centro[0] + radio + 1, paso):
                e = diferencia(a, b, cx, cy, pts, lambda x, y: (x + dx, y + dy))
                mejor = min(mejor, (e, (dx, dy)))
        return mejor
    grueso = busca(puntos_anillo(int(r * 0.25), int(r * 0.85), 4), (0, 0), maximo, 3)
    fino = busca(puntos_anillo(int(r * 0.25), int(r * 0.85), 1), grueso[1], 3, 1)
    return {"dx": fino[1][0], "dy": fino[1][1], "error": round(fino[0], 2)}


def mancha_hud(con, sin, umbral=24.0):
    """Píxeles del cuarto inferior derecho que cambian al ocultar el HUD; la mayor mancha conexa (paso 2)."""
    w, h = con["w"], con["h"]
    mascara = set()
    for y in range(h // 2, h, 2):
        for x in range(w // 2, w, 2):
            if abs(luma(con, x, y) - luma(sin, x, y)) > umbral:
                mascara.add((x, y))
    mejor, vistos = [], set()
    for semilla in mascara:
        if semilla in vistos:
            continue
        cola, comp = [semilla], []
        vistos.add(semilla)
        while cola:
            x, y = cola.pop()
            comp.append((x, y))
            for v in ((x + 2, y), (x - 2, y), (x, y + 2), (x, y - 2)):
                if v in mascara and v not in vistos:
                    vistos.add(v)
                    cola.append(v)
        if len(comp) > len(mejor):
            mejor = comp
    if not mejor:
        return None
    xs, ys = [p[0] for p in mejor], [p[1] for p in mejor]
    return {"caja": [min(xs), min(ys), max(xs), max(ys)], "centroide": [sum(xs) / len(xs), sum(ys) / len(ys)],
            "radio_area": math.sqrt(4 * len(mejor) / math.pi), "pixeles": 4 * len(mejor)}


# ------------------------------------------------------------------ layout <-> ventana (hud_scale.h, exec.c)
def caja_imagen(W, H, llenar=True, wide_test=0.0):
    """output_scale.c output_scale en juego (sin título, película ni carga): forma de la vista y caja de la imagen."""
    forma = wide_test if wide_test > 0 else (W / H) / (16 / 9)
    if not (llenar and abs(forma - 1) > 0.005):
        forma = 1.0
    ajuste = min(W / 1920, H / 1080)
    vw, vh = (W, H) if forma != 1 else (1920 * ajuste, 1080 * ajuste)
    if wide_test > 0:
        f2 = min(W / (1920 * wide_test), H / 1080)
        vw, vh = 1920 * wide_test * f2, 1080 * f2
    return forma, ((W - vw) / 2, (H - vh) / 2, vw, vh)


def ancla(c, borde):
    return -borde if c < -borde / 3 else borde if c > borde / 3 else 0.0


def layout_a_ventana(lx, ly, rl, W, H, llenar=True, hud=100, bordes=False, wide_test=0.0):
    forma, (vx, vy, vw, vh) = caja_imagen(W, H, llenar, wide_test)
    ex, ey = 1 / max(forma, 1), min(forma, 1)
    nx, ny = lx / 640 * ex, ly / 360 * ey
    ax, ay, s = ancla(nx, ex), ancla(ny, ey), hud / 100
    nx, ny = ax + (nx - ax) * s, ay + (ny - ay) * s
    if bordes and abs(forma - 1) >= 0.005:  # (hud_edge_mode 1: el minimapa es de MainScreen/AppMap, ámbito HUD)
        if forma > 1:
            nx += (ax / ex) * (1 - ex)
        if forma < 1:
            ny += (ay / ey) * (1 - forma)
    X, Y = vx + (nx + 1) / 2 * vw, vy + (1 - ny) / 2 * vh
    r = rl / 640 * ex * s * vw / 2
    return {"x": X, "y": Y, "r": r, "px_por_unidad": r / rl if rl else ex * s * vw / 1280, "forma": forma,
            "caja": [vx, vy, vw, vh]}


def ventana_a_layout(X, Y, r, W, H, llenar=True, hud=100, bordes=False, wide_test=0.0):
    forma, (vx, vy, vw, vh) = caja_imagen(W, H, llenar, wide_test)
    ex, ey = 1 / max(forma, 1), min(forma, 1)
    nx, ny, s = (X - vx) / vw * 2 - 1, 1 - (Y - vy) / vh * 2, hud / 100
    if bordes and abs(forma - 1) >= 0.005:  # (deshace el paso a los bordes: ±(1 - borde) en la zona exterior)
        if forma > 1 and abs(nx) > ex / 3:
            nx -= math.copysign(1 - ex, nx)
        if forma < 1 and abs(ny) > ey / 3:
            ny -= math.copysign(1 - forma, ny)
    ax, ay = ancla(nx, ex), ancla(ny, ey)
    nx, ny = ax + (nx - ax) / s, ay + (ny - ay) / s
    return {"lx": nx / ex * 640, "ly": ny / ey * 360, "rl": r / (ex * s * vw / 2) * 640}


# ------------------------------------------------------------------ órdenes
def ord_paneles(a):
    j = Juego(a.puerto)
    pantalla, raiz, estado = raiz_appmap(j)
    ps = arbol(j, raiz)
    if not a.todos:
        ps = [p for p in ps if abajo_derecha(p)]
    out = {"appmap": hex(pantalla), "estado": estado, "raiz": hex(raiz), "paneles": ps}
    texto = json.dumps(out, ensure_ascii=False, indent=1)
    if a.salida:
        Path(a.salida).write_text(texto, encoding="utf-8")
    print(texto)


def cambios(antes, despues, campo):
    por_dir = {p["dir"]: p for p in antes}
    out = []
    for p in despues:
        q = por_dir.get(p["dir"])
        if q and p["nombre"] == q["nombre"]:
            if campo == "giro":
                d = (p["giro"] - q["giro"] + 180) % 360 - 180
                if abs(d) > 0.5:
                    out.append({"nombre": p["nombre"], "dir": p["dir"], "giro": round(d, 3)})
            else:
                dx, dy = p["centro"][0] - q["centro"][0], p["centro"][1] - q["centro"][1]
                if math.hypot(dx, dy) > 0.25:
                    out.append({"nombre": p["nombre"], "dir": p["dir"], "dx": round(dx, 3), "dy": round(dy, 3)})
    return out


def ord_medir(a):
    j, carpeta = Juego(a.puerto), Path(a.juego)
    res = {"config": {}, "capturas": {}, "paneles": {}}
    for clave in ("window_size", "wide_view", "hud_size", "hud_edges", "field_of_view"):
        try:
            res["config"][clave] = j.ajuste(clave)
        except ErrorJuego as e:
            res["config"][clave] = f"? ({e})"
    pos, cam = j.orden("pos"), j.orden("camera")
    x0, y0, z0, yaw0 = pos["x"], pos["y"], pos["z"], round(cam["yaw"], 1)
    res["inicio"] = {"pos": [x0, y0, z0], "yaw_camara": yaw0, "place": pos.get("place")}
    hud_original = res["config"].get("hud_size")

    def captura(etiqueta):
        nombre = f"minimapa_{a.etiqueta}_{etiqueta}.png"
        j.orden("shot", nombre)
        ruta = carpeta / nombre
        for _ in range(50):
            if ruta.exists() and ruta.stat().st_size > 0:
                break
            time.sleep(0.1)
        res["capturas"][etiqueta] = str(ruta)
        try:
            res["paneles"][etiqueta] = [p for p in arbol(j, raiz_appmap(j)[1]) if abajo_derecha(p)]
        except ErrorJuego as e:
            res["paneles"][etiqueta] = f"sin paneles: {e}"
        return ruta

    j.orden("time", "freeze")
    try:
        j.mirar(yaw0)
        captura("base")
        j.mirar(yaw0 + a.giro)
        captura("girada")
        j.mirar(yaw0)
        j.mover(x0 + a.paso, y0 + 3, z0)
        j.mirar(yaw0)
        captura("mas_x")
        j.mover(x0, y0 + 3, z0 + a.paso)
        j.mirar(yaw0)
        captura("mas_z")
        j.mover(x0, y0 + 3, z0)
        j.mirar(yaw0)
        captura("vuelta")
        j.orden("set", "hud_size", "0")
        time.sleep(0.5)
        captura("sin_hud")
    finally:
        if hud_original is not None and not str(hud_original).startswith("?"):
            j.orden("set", "hud_size", hud_original)
        j.orden("time", "unfreeze")

    img = {k: leer_png(v) for k, v in res["capturas"].items()}
    base = img["base"]
    res["imagen"] = [base["w"], base["h"]]
    if a.centro:
        cx, cy, r = a.centro[0], a.centro[1], a.radio
        res["circulo"] = {"origen": "argumentos", "cx": cx, "cy": cy, "r": r}
    else:
        m = mancha_hud(img["vuelta"], img["sin_hud"])
        if not m:
            raise ErrorJuego("no se ve el HUD en el cuarto inferior derecho: pasa --centro y --radio")
        res["mancha_hud"] = m
        x0c, y0c, x1c, y1c = m["caja"]
        r = min(x1c - x0c, y1c - y0c) / 2
        cx, cy = (x0c + x1c) / 2, y1c - r  # (el minimapa es lo más bajo de la mancha; encima van los medidores)
        res["circulo"] = {"origen": "mancha del HUD, aproximado: compruébalo con marcar", "cx": cx, "cy": cy, "r": r}
    cx, cy, r = round(cx), round(cy), round(r)
    res["giro"] = mejor_giro(base, img["girada"], cx, cy, r)
    res["giro"]["yaw_pedido"] = a.giro
    for k in ("mas_x", "mas_z", "vuelta"):
        res["desplazamiento_" + k] = mejor_desplazamiento(base, img[k], cx, cy, r, a.maximo)
    ex, ez = res["desplazamiento_mas_x"], res["desplazamiento_mas_z"]
    # El terreno se mueve al revés que Link: +x en el mundo es -desplazamiento en pantalla
    res["ejes_px_por_metro"] = {"x": [-ex["dx"] / a.paso, -ex["dy"] / a.paso], "z": [-ez["dx"] / a.paso, -ez["dy"] / a.paso]}
    res["px_por_metro"] = {"x": math.hypot(*res["ejes_px_por_metro"]["x"]), "z": math.hypot(*res["ejes_px_por_metro"]["z"])}
    if isinstance(res["paneles"].get("base"), list):
        for k in ("girada", "mas_x", "mas_z"):
            if isinstance(res["paneles"].get(k), list):
                res[f"paneles_cambian_{k}"] = cambios(res["paneles"]["base"], res["paneles"][k],
                                                      "giro" if k == "girada" else "centro")
    marcado = leer_png(res["capturas"]["base"])
    marcar_circulo(marcado, cx, cy, r)
    for eje, rgb in (("x", (255, 64, 64)), ("z", (64, 160, 255))):
        ux, uy = res["ejes_px_por_metro"][eje]
        for t in range(0, int(r)):
            pintar(marcado, round(cx + ux / max(math.hypot(ux, uy), 1e-6) * t),
                   round(cy + uy / max(math.hypot(ux, uy), 1e-6) * t), rgb)
    ruta_m = carpeta / f"minimapa_{a.etiqueta}_marcado.png"
    escribir_png(ruta_m, marcado)
    res["marcado"] = str(ruta_m)
    salida = Path(a.salida or carpeta / f"minimapa_{a.etiqueta}.json")
    salida.write_text(json.dumps(res, ensure_ascii=False, indent=1), encoding="utf-8")
    print(json.dumps({k: res[k] for k in ("config", "imagen", "circulo", "giro", "px_por_metro", "ejes_px_por_metro")
                      if k in res}, ensure_ascii=False, indent=1))
    print("resultado completo:", salida)


def ord_prediccion(a):
    kw = dict(llenar=not a.sin_llenar, hud=a.hud, bordes=a.bordes, wide_test=a.wide_test)
    if a.inversa:
        print(json.dumps(ventana_a_layout(a.x, a.y, a.r, a.ventana[0], a.ventana[1], **kw), indent=1))
    else:
        print(json.dumps(layout_a_ventana(a.x, a.y, a.r, a.ventana[0], a.ventana[1], **kw), indent=1))


def ord_marcar(a):
    img = leer_png(a.png)
    marcar_circulo(img, a.cx, a.cy, a.r)
    escribir_png(a.salida, img)
    print(a.salida)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--puerto", type=swctl.port_number, default=7777)
    sub = ap.add_subparsers(dest="orden", required=True)
    p = sub.add_parser("paneles")
    p.add_argument("--todos", action="store_true", help="todos los paneles, no solo los visibles abajo a la derecha")
    p.add_argument("--salida")
    p = sub.add_parser("medir")
    p.add_argument("--juego", required=True, help="carpeta del juego (donde `shot` guarda los PNG)")
    p.add_argument("--etiqueta", default="16x9", help="nombre de esta configuración (16x9, 21x9, hud80...)")
    p.add_argument("--paso", type=float, default=40.0, help="metros que se mueve a Link en x y en z")
    p.add_argument("--giro", type=float, default=90.0, help="grados que se gira la cámara")
    p.add_argument("--maximo", type=int, default=60, help="desplazamiento máximo buscado en la imagen (px)")
    p.add_argument("--centro", type=float, nargs=2, metavar=("CX", "CY"), help="centro ya medido (px de la imagen)")
    p.add_argument("--radio", type=float, default=0, help="radio ya medido (px de la imagen), con --centro")
    p.add_argument("--salida")
    p = sub.add_parser("prediccion")
    p.add_argument("x", type=float)
    p.add_argument("y", type=float)
    p.add_argument("r", type=float)
    p.add_argument("--ventana", type=float, nargs=2, required=True, metavar=("W", "H"))
    p.add_argument("--inversa", action="store_true", help="de píxeles de ventana (y hacia abajo) al layout")
    p.add_argument("--sin-llenar", action="store_true", help="Llenar pantallas anchas apagado")
    p.add_argument("--hud", type=int, default=100, help="Tamaño de HUD en %%")
    p.add_argument("--bordes", action="store_true", help="HUD en los bordes encendido")
    p.add_argument("--wide-test", type=float, default=0.0, help="SAVAGEWIND_WIDE_TEST (21:9 = 1.3333)")
    p = sub.add_parser("marcar")
    p.add_argument("png")
    p.add_argument("cx", type=float)
    p.add_argument("cy", type=float)
    p.add_argument("r", type=float)
    p.add_argument("salida")
    a = ap.parse_args(argv)
    if a.orden == "medir" and a.centro and a.radio <= 0:
        ap.error("--centro necesita --radio")
    try:
        {"paneles": ord_paneles, "medir": ord_medir, "prediccion": ord_prediccion, "marcar": ord_marcar}[a.orden](a)
    except (ErrorJuego, OSError, ValueError) as e:
        print("error:", e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
