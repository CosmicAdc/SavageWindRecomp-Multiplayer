# Bitácora · F1 · lista priorizada de la línea base (`T108`)

- Fecha: 2026-11-24
- Quién: orquestador (Gemini 4 Hydrogen) — **borrador pendiente de aprobación del dueño**
- Rama y commit base: `coop/main` en `064d68a`
- Fuentes reunidas:
  - [2026-11-24-F1-arreglos-adelantados.md](2026-11-24-F1-arreglos-adelantados.md)
  - [2026-11-24-F1-linea-base-pasada.md](2026-11-24-F1-linea-base-pasada.md) (`T101`, `T103`–`T106`)
  - [2026-11-24-F1-prueba-ventana-dueno.md](2026-11-24-F1-prueba-ventana-dueno.md) (`T107`)

## Lista priorizada de fallos y pendientes

| # | Fallo | Gravedad | Repro | Frecuencia | Fase destino | Hallazgo / origen |
|---|---|---|---|---|---|---|
| 1 | Cierre inesperado de `Client` (`0xc0000409` en `ucrtbase.dll` + `0xa527e`) al acercarse al Host tras salir al título y recargar en el mismo proceso | cuelgue | Cargar partida → `goto tower 7` → salir a pantalla de título sin guardar → recargar → acercarse al Host | 1 de 1 en `T107` | **F2 (`T212` `RT_ON_RESET`)** | `runtime/online_actors.c` sin reset completo en reinicio en caliente |
| 2 | En `recorrido.py` sin contraseña, `A` devolvió `0` en `actors OnlinePlayer` (pasos 2, 3, 5), aunque `B.log` sí vio a `Host: OnlinePlayer` y en la corrida con contraseña `A.log` sí registró `Client: OnlinePlayer` | incorrecto | `recorrido.py --enemigos` (paso 2 tras `goto tower 7`) | 1 de 2 corridas | **F1 (`T120` re-verificar)** | Por aislar si es tiempo/paginación (`0,14 GB` libres) o intermitencia |
| 3 | `ENEMIGOS (a)(b)(c)`: `B no ve ningun 'Enemy_' en el campamento` (ADR-012 sin verificar en juego todavía) | incorrecto | `recorrido.py --enemigos` al final de pasos 1–10 | 1 de 1 | **F1 (`T120` re-verificar)** | Arrastrado tras pasos 4–8 o falta de enemigos activos a esa hora |
| 4 | Arranque desatendido y guiones (`SAVAGEWIND_INPUT` entrando en menú DLC, `PASO 8` `Teleport in progress`, `PASO 9–10` ruta MSYS en `recorrido.py`, `dos_juegos.sh ventana` bloqueando entrada) | incorrecto | Corrida inicial de `recorrido.py` y `T107` | siempre antes del arreglo | **F1 (`e805e02`, verificar en `T120`)** | Guiones de `tools/coop/` |
| 5 | `test-actions` falla en Windows (`tools/test_actions.c:279`) | incorrecto | `make test-actions` en Windows | por verificar tras `64cc708` | **F1 / CI** | Lectura de `tmpfile()` en modo texto Windows |
| 6 | Build de macOS falla en el CI | incorrecto | Workflow `macos.yml` en GitHub Actions | en CI | **F3 / CI** | Pendiente de F0/CI |
| 7 | Matrices internas de `WireState` (equipo, disparos, runas, objetos) sin validar en receptor | incorrecto | Lectura de `runtime/online.c` | permanente | **F2 (`T213`)** | Alcance explícito de F1 arreglo 5 |
| 8 | Contraseña en claro en `online.ini`, cable y `--server`; teclado en pantalla (`osk.mm`) sin verificar | incorrecto | Uso con mando / captura | permanente | **F3** | Alcance explícito de F1 arreglo 9 |
| 9 | Nombre por defecto del jugador (`"DELL"`) en `online.ini` | cosmético / privacidad | Arranque sin `SAVAGEWIND_ONLINE_NAME` | siempre | **F4** | Configuración inicial |
| 10 | Sin `netem` en Docker Desktop (kernel WSL2); `--server` no imprime `joined`; aviso `for loop has empty body` en `online_actors.c:6045` | cosmético | `compose.yml` / `make -j8` | siempre | **F2 (`T219` / `T206`)** | Entorno / PR #294 |

## Qué pasa a cada fase

- **Queda en F1 (`T120`–`T121`)**: re-ejecutar `recorrido.py --enemigos` con los arreglos de `e805e02` (cubriendo #2, #3, #4), correr pruebas de bot (`BOT_NAN=1`, `BOT_LOOT=...`), verificación Linux en Docker (`distro_build.sh debian:13`), completar `verificacion.md` §3 con el dueño y comprobar `test-actions`.
- **Pasa a F2**: #1 (`T212` `RT_ON_RESET`), #7 (`T213` capa única de validación), #10 (`T206`, `T219`).
- **Pasa a F3 / F4**: #6, #8 (F3), #9 (F4).
