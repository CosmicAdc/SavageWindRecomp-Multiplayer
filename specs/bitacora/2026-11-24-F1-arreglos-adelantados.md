# Bitácora · F1 · arreglos adelantados (antes de la línea base)

- Fecha: 2026-11-24
- Quién: orquestador (Claude Opus) + agentes en Herdr: `carril-oa` (Opus), `carril-net` (Opus), `t119-ui`, `t109-bot`,
  `t023-bots` (Sonnet)
- Juego: 1.9.0, build `cd57b23fa4bbad65`, DLC 1 y 2 (3.0) extraídos aparte; `GAMELIB_ID` `9bc886f8314dd42b`
- Hardware: i7-12700H, 15,7 GB, RTX 3060 Laptop 6 GB; Docker Desktop 28.3.3 (6,2 GB)

## Desviación del procedimiento

Las tareas de F1 dependen de T108 (línea base con dos juegos), que aún no se ha hecho. El dueño ordenó adelantar la
**escritura** de los arreglos. Se integraron en `coop/main` solo tras la verificación sintética o de compilación del
orquestador. Las comprobaciones **en el juego** de cada tarea quedan pendientes y las tareas siguen en `[~]` hasta la
línea base.

## Qué se integró y cómo se verificó

| Tarea | Nivel alcanzado | Evidencia |
|---|---|---|
| `<stddef.h>` en `runtime/control.h` | compilación | Linux en Docker (`distro_build.sh debian:13`) pasa de FALLA a 0 |
| T110 arnés de `online.c` | prueba sintética | `make test-online-net`: 58 checks, 0 fallos |
| T111 puerta de versión | sintética + juego parcial | `make test-online-gate` ok; arranque desatendido: el log muestra una vez `[online] game version 1.9.0: actors on` |
| T112 `has_body` | compilación | `has_body(a)` en las dos llamadas de `movers_update` |
| T113 runas | compilación | — |
| T116 limpiar sesión | sintética | `test_stop_clears_session` (`online_enemies` = -1, igual que un proceso nuevo; el `verify` decía 0) |
| T117 `SIGPIPE` | Docker | servidor vivo tras 10 `docker kill` de un bot; no se reprodujo el fallo "antes" |
| T118 `shield` | sintética | `test_estate_strings_terminated`; arnés en 96 checks, 0 fallos |
| T119 contraseña oculta | compilación | `i18n_check` sin textos nuevos faltantes (243, igual que la línea base) |
| T109 bot hostil | compilación | compila sin avisos (igual que antes) |

Después de integrar todo: `make -j8` = 0 (21 s, incremental) y arranque desatendido en la Meseta correcto.

## Fallos conocidos que pasan a la lista de F1 (T108)

| Qué | Dónde | Origen |
|---|---|---|
| `test-launch-flags`: 2 798 checks en 0, pero `runtime/launch_flags_table.c` desfasada respecto a su generador | `Makefile:877` | PR #294 |
| `test-actions`: `fread` falla en `tools/test_actions.c:279` | Windows | sin determinar (¿modo texto/CRLF?) |
| `test-gyro`: enlace, falta `framepace_dialogue_ui` | `Makefile:632`, `runtime/hle/bindings.c` | PR #294 |
| Aviso `for loop has empty body` | `runtime/online_actors.c` (~5939) | PR #294 |
| Sin `netem` en Docker Desktop: no hay latencia ni pérdida simuladas | kernel WSL2 | entorno |
| El servidor `--server` no imprime `joined` en su salida (el `verify` de T023 lo esperaba) | `runtime/online.c` | por revisar |

## Decisión nueva del dueño

ADR-012: los enemigos deben atacar a los dos jugadores. Se comprueba en T104 (paso 5); en F4 se añadirá el caso de
uso de **amenaza**.

## NO verificado

- Nada de F1 se ha visto todavía con **dos juegos** en partida (todo pendiente de T101-T108).
- T113 y T116 no tienen prueba en el juego de la ventana de fallo (un fotograma tras morir o cargar).
