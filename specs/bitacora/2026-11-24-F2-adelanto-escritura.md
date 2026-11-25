# Bitácora · F1 T120 (pruebas automáticas) y adelanto de la escritura de F2

- Fecha: 2026-11-24
- Quién: orquestador (Claude Opus) + agentes en Herdr: `firma-t201`, `cable-t202`, `trama-t203`, `sesion-t204`
  (Sonnet), `oa-t206` (Opus)
- Rama: `coop/main`; F1 aún sin cerrar (falta la pasada final de T120 con dos juegos y T121)

## Desviación del procedimiento

`fases/README.md` dice que una fase no empieza hasta que el dueño aprueba el cierre de la anterior. El dueño pidió
seguir con el desarrollo ("continúa con el desarrollo", "lanza los agentes siguientes para que puedan seguir
trabajando"). Como en F1, se adelanta solo la **escritura** de tareas de F2 sin cambio de comportamiento; se integran
tras la verificación del orquestador, y todo lo que exige juego o firma de referencia queda pendiente.

## T120 · pruebas automáticas en `coop/main` (`250a25a`)

| Prueba | Resultado |
|---|---|
| `make -j8` | 0 |
| `test-online-gate` / `test-online-net` / `test-online-names` | 0 / 162 checks, 0 fallos / 39 checks, 0 fallos |
| `test-launch-flags` / `test-gyro` | 0 (3073 checks; tabla al día, 577 nombres) / 0 |
| `test-console` / `test-control` / `test-mods` / `test-places` / `test-update-core` | 0 en todas |
| `i18n_check.py` | 0 |
| `test-actions` | FALLA bajo `make`; **causa: entorno** (ver abajo) |

### `test-actions` en Windows (fallo #5 de la lista de T108)

- Con el programa compilado lanzado desde Git Bash, pasa. Lanzado desde el `sh` o el `make` de MSYS2 heredados de Git
  Bash, falla en la aserción de `tools/test_actions.c` (`act_key_2 = E`).
- El búfer leído está vacío: en ese entorno `TMP` y `TEMP` llegan vacías, `tmpfile()` de UCRT no encuentra una carpeta
  temporal en la que pueda escribir y la prueba lee 0 bytes.
- `make test-actions TMP=<carpeta temporal> TEMP=<la misma>` → pasa. No es un fallo del código ni del modo online;
  en una consola MSYS2 UCRT64 normal (con su `/etc/profile`) `TMP` existe. Se cierra el #5 como **entorno**.

## F2 adelantada: integradas

| Tarea | Commit | Verificación del orquestador | Falta |
|---|---|---|---|
| T201 `tools/coop/firma_log.py` | `7b53d18` | `--autoprueba` 0 | etiqueta `f1-cerrada`, tres pasadas de referencia y cuarta con `--comparar` |
| T202 `runtime/online_wire.h` | `2f38cc9` | `make -j8` 0 (aserciones de tamaño compiladas); bot `cc -O2 -Wall -Iruntime` sin avisos; `grep` de `typedef struct` y `version = 38` en el bot vacíos; `test-online-net` 162/0 | bot contra el juego; Linux en Docker |
| T203 pruebas de trama | `baf62ff` | Windows: `test-online-wire` 16647/0 (error SKEL máx. 9,93e-5 rad; tolerancia 2e-4), `test-online-net` 309/0. El primer intento falló por la prueba: `cbuf` de 32 KB y un ENEMIES de 255 entradas pide 79 KB; se agrandó a 128 KB | Docker |

## F2 adelantada: en curso

- `sesion-t204` (T204): escrita (`fde83e8`), sin integrar hasta correrla en Docker (es solo POSIX).
- `oa-t206` (T206, solo puede ejecutar `oa_check.py`; base `7b53d18`).
- `valida-t213` (T213, Opus).
- Módulos puros sin dependencias, adelantados a petición del dueño ("¿hay como lanzar alguna otra tarea?"): `amenaza-t443` (T443, F4, Opus) y `modulos-f3` (T320, T322, T327 de F3, Sonnet). Solo ficheros nuevos; no tocan `online.c` ni el `Makefile`.

## Respuesta al dueño: cierre al recargar desde el título

No está corregido. Es el fallo #1 de T108, asignado a T212 (`RT_ON_RESET`), al final de la cadena OA (T206-T212). No se
parchea antes en el fichero monolítico para no chocar con la reorganización. Mientras tanto: cerrar el juego en vez de
salir al título y recargar.

## NO verificado

- Nada de F2 se ha visto en el juego; T201 no tiene referencia real todavía.

## 2026-11-25 (madrugada) · pasada completa interrumpida por memoria

Con `revisor` y `oa-t209` (Opus) trabajando, la pasada completa de dos juegos (`recorrido.py --enemigos`, build con T208,
`0fbcfa1`) la cortó Claude Code por memoria crítica antes del primer paso (el anfitrión ya ocupaba 4,2 GB). Se cerraron
los dos juegos (6,2 GB libres después). **No se relanzó automáticamente**: queda para cuando el dueño lo pida o con
menos agentes abiertos. Regla práctica: pasada con dos juegos = como mucho un agente abierto.

## 2026-11-25 · Pasada de dos juegos con T209 (build `69fa86c`, sin ventana)

- Pasos 1, 3, 4 (1 traspaso), 5, 6, 7, 8 y 9 **PASAN**. En la pasada de T208 fallaban el 4 y el 5.
- Paso 2 y paso de enemigos **FALLAN** por el teletransporte (`Position verification failed`): herramienta (HE-13).
- Paso 10 **FALLA** por tiempo: B se relanzó y volvió a unirse (tercera línea `joined` en A.log), pero con 0 GB libres
  tardó más de los 180 s que espera el guion. Igual en el paso 1 de los dos primeros intentos.
- Logs: ningún `dropped a STATE` ni otro descarte; ningún cierre. B.log: 166 líneas `made again` (HE-23, T453; ya
  ocurría antes de T208).
- `recorrido.py`: desde PowerShell `shutil.which("bash")` daba el de WSL; ahora prefiere Git Bash o MSYS2 (commit
  `557990f`). La pasada se lanza en un panel de Herdr para que el recortador de memoria de Claude Code no la corte.
- Firma (`firma_log.py --comparar`): sin referencia todavía (T201 pendiente). T209 queda `[~]` hasta tenerla.

## 2026-11-25 · Pasada con T210 + T222 (build con T222 integrada)

- PASAN 1, 3, 4 (3 traspasos, como la línea base de F1), 5, 6, 7 y 9. Ningún `dropped a …` ni `cleared an entry` en
  A.log ni B.log (lo que pedía T222). Sin cierres.
- FALLAN por el teletransporte (HE-13): 2 ("Position not found"), y la repetición del 8 ("Position verification
  failed"). El primer intento del 8 falló porque A no listó `OnlinePlayer` en 90 s aunque el log muestra la marioneta
  rehecha justo después del salto (igual que con T209): queda **sin explicar**, repetir con reintentos.
- FALLA 10 por la espera de 180 s (la pasada empezó antes de subirla a 420 s); B volvió a unirse.
- B.log: 309 `made again` (HE-23, T453).
- `recorrido.py`: el reintento de teletransporte (hasta 2, anotado como `aviso:`) cubre ahora todos los saltos
  (`ir`, `llegar`, paso 4 y paso 8). Prueba sintética: 2 fallos → llega; 3 fallos → Falla.

## 2026-11-25 · PE-1 con el dueño (T212, build `dc16647`, dos ventanas)

- B (invitado, el dueño) hace "Salir al inicio" → Continuar → Online → Join, **2 ciclos** (el dueño dio por buena la
  prueba con dos). Cada vez: B.log `[reboot] requested (start screen)`, `[online] reset`, `[online] reboot: leaving the
  session`, `[online] Not connected`, `game torn down in 795/993 ms (52 reset hooks)`; A.log `server: Client left`,
  `removed Client's Link`, y al volver `Client joined` y `Client: OnlinePlayer`; B ve `Host: OnlinePlayer`. Ningún
  cierre ni excepción. **El cierre `0xc0000409` al recargar (B1) no se reproduce**: criterio S7 cumplido para el
  invitado (2 de 3 ciclos pedidos, por decisión del dueño).
- Observado: B recibe "the host's time: 11.00" en bucle con A sin foco (la hora de A no avanza: ¿pausa por foco?).
  Relacionado con HE-07 / T456.
- El audio estaba a 0 por `dos_juegos.sh` (`SAVAGEWIND_AUDIO_GAIN=0`): el dueño lo notó.
- **Reinicio del anfitrión (A), 1 ciclo con el dueño:** A.log `[reboot] requested`, `[online] reset`, `reboot: leaving
  the session`, `server: Host left (slot 0)`, `Not connected`, `torn down in 1073 ms (52 reset hooks)`; B.log `removed
  Host's Link`, `Disconnected by the server` (sin cierre). A vuelve a hospedar desde el menú (`Host joined (slot 0)`), B
  se une (`Client joined (slot 1)`) y los dos se ven (`Host: OnlinePlayer` en B, `Client: OnlinePlayer` en A).
  Coincide con lo previsto en la bitácora de T212 (el invitado ve "Disconnected by the server"; no hay mensaje de
  "el servidor se va"). **T212: PE-1 cumplido** con el dueño.
