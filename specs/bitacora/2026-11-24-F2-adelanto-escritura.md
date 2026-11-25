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
