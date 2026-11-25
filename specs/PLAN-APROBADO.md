# Plan: carpeta SPEC y fases del modo cooperativo de Savage Wind (fork en CosmicAdc)

## Contexto

Queremos un modo cooperativo de 2 jugadores (ampliable) para Savage Wind Recomp, construido sobre el
PR #294 "Online mode" (byluki, mstampfli) y desarrollado con spec-driven development y agentes en
paralelo. Todo vive en un fork de la cuenta **CosmicAdc**; no se sube nada al proyecto original por ahora.

Decisiones ya tomadas: mundo compartido del anfitrión; cada jugador con su inventario, corazones y
resistencia; conexión directa (un jugador hospeda); cooperativo por defecto con PvP activable; enfoque
"cimientos primero", empezando por probar el sistema y arreglar lo rápido y esencial.

Ya existe: 7 mapas del código en `specs/mapa/` (4.282 líneas), el clon
de `test` en `SavageWindRecomp\`, el PR en `pr294-online\` y dos guías de aprendizaje.

## Lo que encontré en tu equipo (solo lectura)

| Elemento | Estado | Consecuencia |
|---|---|---|
| Dell G15 5520, i7-12700H (14 núcleos, 20 hilos) | Bien | Recompilar el juego debería tardar ~10-15 min |
| RTX 3060 Laptop 6 GB, Vulkan presente | Bien | Es la GPU de referencia de los autores |
| RAM 15,7 GB | Justa | Dos juegos a la vez usan ~9,5 GB: hay que cerrar Docker Desktop y el navegador para esa prueba |
| Disco C: 70 GB libres | Justo | Juego extraído ~15 GB + imagen ~15 GB + compilación + MSYS2 ≈ 40 GB. Conviene liberar espacio o borrar los NSP después de extraer |
| Docker 28.3 (límite 6,2 GB), WSL | Disponible | Ver "Papel de Docker" |
| MSYS2, clang, make, Go | **Faltan** | Sin MSYS2 no se puede compilar |
| Python | Solo el lanzador `py` | Hay que crear el venv del proyecto |
| `Game\`: base v0, actualización v1114112, DLC 1 y 2 v196608 | Versiones correctas (1.9.0 y DLC 3.0) | **Son NSP, no sirven tal cual**: el proyecto necesita `exefs/` y `romfs/` ya extraídos |

**Papel de Docker.** No sirve para jugar: el juego necesita ventana y GPU con Vulkan, y eso en Docker
sobre Windows no es práctico. Sí lo usaremos para: (1) pruebas automáticas en Linux (Python con
capstone/unicorn y pruebas en C), (2) comprobar que el código compila en Linux, (3) pruebas de red con el
bot y con latencia y pérdida simuladas (`tc netem`). Compilar y jugar se hace nativo con MSYS2.

## Lo que tienes que hacer tú (no puedo hacerlo por ti)

1. **Fork** de `Revivedx/SavageWindRecomp` en la cuenta CosmicAdc, desde la web de GitHub.
2. **Extraer tus NSP** a `exefs/` y `romfs/` (base + 1.9.0 fusionados), y los DLC a `aoc1/` y `aoc2/` sin
   mezclarlos en `romfs/`. Requiere las claves de tu propia consola; el proyecto no documenta ese paso y
   yo tampoco lo guío. Cuando esté, verifico la versión con `tools/sw_probe.py`.
3. **Liberar disco** (ideal: 100 GB libres).
4. Decidir si escribes a los autores del PR #294 (te redacto el mensaje; no es bloqueante).

## Estructura de la carpeta SPEC (dentro del fork, rama `coop/main`)

```
specs/
  README.md                 índice, cómo se trabaja, estado de cada fase
  constitution.md           reglas no negociables (legal, hilos, validación de red, pruebas, evidencia)
  glosario.md               términos de recomp y de red, en lenguaje llano
  mapa/                     los 7 mapas ya escritos (01..07)
  casos-de-uso/             CU-01..CU-14, uno por archivo: actor, flujo, criterios de aceptación
  contratos/
    protocolo.md            mensajes, versiones, quién manda sobre qué
    hilos-y-frame.md        qué corre en qué hilo; orden dentro de rt_online_frame
    autoridad-mundo.md      qué es del anfitrión y qué es de cada jugador (flags, recompensas)
    validacion-red.md       reglas para todo dato que llega por red
  fases/
    F0-preparacion/         spec.md, plan.md, tasks.md, verificacion.md
    F1-prueba-y-arreglos/   ídem
    F2-cimientos/           ídem
    F3-red-robusta/         ídem
    F4-funciones-coop/      ídem
    F5-funciones-dificiles/ ídem
  decisiones/               ADR cortos (una decisión por archivo, con su porqué)
  bitacora/                 resultados de cada prueba real (fecha, versión, hardware, qué se vio)
  aprendizaje/              guía desde lo básico: recomp, Ghidra, netcode, con ejercicios sobre este código
```

Cada `tasks.md` usa el formato `[P]` (paralelizable), `files:` (archivos que toca) y `verify:` (comando
que demuestra que funciona). Sin `verify:` una tarea no se ejecuta en paralelo.

## Casos de uso principales

| ID | Caso | Hoy | Fase |
|---|---|---|---|
| CU-01 | Hospedar una partida y que el otro se una por IP | Existe | F1 (probar) |
| CU-02 | Ver al compañero moverse, con su equipo | Existe | F1 (probar) |
| CU-03 | Jugar sin crashes una sesión de 30 min | Parcial | F1-F2 |
| CU-04 | Encontrar la partida sola en red local / código de invitación / apertura de puerto | Falta | F3 |
| CU-05 | Reconectar tras una caída sin perder la sesión | Falta | F3 |
| CU-06 | El mundo y el progreso son los del anfitrión | Parcial (hoy es la unión de todos) | F4 |
| CU-07 | Cada jugador conserva su inventario y corazones | Parcial | F4 |
| CU-08 | Teletransportarse al compañero (mismo mapa) | Falta | F4 |
| CU-09 | Enviar o soltar objetos para el compañero | Parcial | F4 |
| CU-10 | Marcadores, pings y nombre en color; accesibilidad | Falta | F4 |
| CU-11 | Revivir al compañero caído | Parcial | F4 |
| CU-12 | PvP activable por acuerdo, con daño validado | Parcial | F4 |
| CU-13 | Color en el modelo 3D del segundo Link | Falta | F5 |
| CU-14 | Momentos de historia solo con los dos presentes; santuarios juntos; teletransporte entre mapas | Falta | F5 |

## Fases

### F0 — Preparación (en serie)
- Instalar MSYS2 UCRT64 y dependencias con `tools/setup_windows.ps1` / `docs/BUILD-windows.md`; crear
  `tools/venv` (capstone, unicorn, lz4); instalar Go no hace falta todavía.
- Remotos: `origin` = fork CosmicAdc, `upstream` = Revivedx. Rama `coop/main` desde `pr-294-online`,
  restaurando `.github/workflows/` desde `test` (el PR los borra por un efecto de sus fusiones).
- Desactivar o redirigir el actualizador automático del fork (`runtime/update_*.c`), que apunta al
  repositorio original.
- Mover `specs/` al fork y escribir `constitution.md`, `README.md` y los contratos iniciales a partir de
  los mapas.
- `sw_probe.py` sobre tu `exefs`, `make gen`, `make -j`, `tools/online_mod.py` para generar los actores.
- Imagen Docker de pruebas (Linux: Python + clang) y `docker compose` con el bot.
- **Salida:** el juego arranca en solitario; pruebas base en verde; tiempos y uso de disco anotados en
  `bitacora/`.

### F1 — Probar el sistema y arreglos rápidos esenciales
1. **Línea base**: dos juegos en este PC según `docs/ONLINE.md` §"Testing with two games on one PC"
   (`SAVAGEWIND_ONLINE=host` / `=127.0.0.1`, carpetas de guardado separadas, `tools/swctl.py`). Recorrer
   CU-01 a CU-03 y anotar cada fallo en `bitacora/`.
2. **Arreglos rápidos** (cada uno pequeño, con su repro), tomados de los mapas 06 y 07:
   - Puerta de versión: no llamar direcciones de 1.9.0 si el juego no es 1.9.0.
   - `movers_update` sin `has_body()` (`runtime/online_actors.c:4928`).
   - Runas: usar el Link remoto solo después de comprobar que sigue vivo (`:5976` frente a `:6036`).
   - Lista de permitidos para nombres de actores que llegan por red (`:3604`, `:2226`).
   - Rechazar NaN/infinito en los números que llegan por red.
   - Limpiar el estado de sesión al cortar (`stop_all`, `runtime/online.c:1322-1335`).
   - `SIGPIPE` en Linux; terminador de `shield` (`online.c:1095`); contraseña oculta en pantalla.
- **Salida:** sesión de 30 min sin crash en el recorrido de prueba; lista priorizada de lo que quede.

### F2 — Cimientos (primero en serie, luego en paralelo)
- Pruebas automáticas del protocolo sin el juego (codificar/decodificar, máquina de estados de sesión),
  compartiendo la definición de mensajes con `tools/online_bot.c`.
- Partir `online_actors.c` en los 11 módulos propuestos en `specs/mapa/07` §8 (`oa_core`, `oa_gamedata`,
  `oa_puppet`, `oa_enemies`, …), sin cambiar comportamiento; partir `g_act[]` por dueño; orden explícito
  en `rt_online_frame`.
- `RT_ON_RESET` para las ~85 variables con punteros del juego; validación única de entradas de red.
- Reutilizar patrones existentes: cola hilo-externo → hilo del juego de `runtime/items.c:337-447`;
  sockets de `runtime/control.c` y `runtime/dsu.c`.

### F3 — Red robusta (paralelo)
- Envío no bloqueante por cliente (hoy un jugador lento congela a todos, `online.c:140-148`).
- El anfitrión valida quién puede enviar enemigos, botín y misiones; límite de mensajes; límite de
  intentos de contraseña y contraseña no guardada en claro.
- Descubrimiento en red local, código de invitación, apertura automática de puerto (UPnP), IPv6.
- Reconexión. Comprobación estricta de versión (protocolo + compilación + versión del juego).
- Decisión documentada (ADR) sobre migrar de TCP a UDP con ENet; se evalúa con medidas en Docker.

### F4 — Funciones cooperativas (paralelo, un worktree por tarea)
CU-06 a CU-12. Reutiliza `control_tp_request` (teletransporte), `rt_item_tick` / `pouch_item_get` (dar
objetos), los overlays `perf_overlay_draw` / `photo_overlay_draw` como modelo para marcadores, y los
filtros de visión de color existentes. Archivos con dueño único para evitar choques: `online.h`
(protocolo), `ui/menu.mm`, `settings.*`, `ui/i18n/*.inc`.

### F5 — Funciones difíciles (una a una, requieren ingeniería inversa con tu copia en Ghidra)
CU-13 y CU-14, y más de 2 jugadores (hoy el ancho de banda crece con el cuadrado de jugadores).

### Aprendizaje (en paralelo a todo)
`specs/aprendizaje/` con una ruta desde cero ligada al trabajo real: cada fase incluye lecturas cortas y
un ejercicio sobre este código (por ejemplo, en F1: seguir un paquete `STATE` desde el socket hasta el
hueso del Link remoto).

## Reglas de trabajo
- Nada se sube a `Revivedx`; solo a tu fork, y solo cuando lo pidas.
- Nunca se suben archivos del juego, claves, guardados ni código generado (`Game\` queda fuera del repo).
- No se copia código de proyectos de decompilación.
- Cada cambio en `tools/patches.py` o `tools/game_sites.py` obliga a recompilar el juego (~15 min) a los
  dos jugadores: se agrupan.
- Un arreglo cuenta como hecho cuando se ha visto funcionar en el juego, no cuando compila.

## Verificación
- F0: `tools/sw_probe.py` reconoce 1.9.0; `make -j` termina; el juego llega al mundo en solitario.
- F1: recorrido de dos instancias de `docs/ONLINE.md` sin crash 30 min; cada arreglo con su repro antes
  y después en `bitacora/`.
- F2+: pruebas de protocolo en Docker en cada cambio; `tools/i18n_check.py`; compilación Linux en Docker.
- Red: bot (`tools/online_bot.c`) con latencia y pérdida simuladas en Docker.

## Primer paso al aprobar
Solo lo que no depende de ti: crear la estructura `specs/` con `README.md`, `constitution.md`,
`glosario.md`, los 14 casos de uso y las 6 carpetas de fase con su `spec.md`/`tasks.md` detallados, en
`specs/` (se moverá al fork cuando exista). No instalo nada ni toco
código hasta que confirmes F0.
