# F2 — Cimientos · verificación

F2 se declara hecha cuando todas las casillas tienen evidencia o un motivo escrito, y el dueño lo aprueba.
Niveles de evidencia: **L** = lectura del código; **P** = prueba automática; **J** = visto en el juego. En un
refactor, "compila" y "la prueba de texto pasa" no bastan: hace falta además la firma y el recorrido.

## 1. Antes de empezar

- [ ] F1 cerrada; etiqueta local `f1-cerrada`.
- [ ] Firma de comportamiento de referencia guardada (tres pasadas) y `--comparar` en 0 contra una cuarta (T201).
- [ ] Commit de partida: ________.

## 2. Cadena NET

| Comprobación | Windows | Docker (Linux) | Nota |
|---|---|---|---|
| `make test-online-gate` | | | de F1 |
| `make test-online-wire` | | | T203 |
| `make test-online-net` (casos: ____) | | | T203, T218 |
| `make test-online-session` (casos: ____) | solo aviso | | T204; casos pendientes: ________ |
| `make test-online-validate` (aleatorios: 100 000) | | | T213 |
| Aserciones de tamaño de `online_wire.h` compilan | | | números corregidos respecto al mapa: ________ |
| El bot compila con `-Iruntime` y no tiene copia de `WireState` | | | T202 |
| Bot `BOT_NAN` / `BOT_LOOT` / `BOT_SHIELD`: el juego sigue y registra | | — | T213 |
| Flujo `coop.yml` en verde | — | — | T205, tras el push que pida el dueño |

- [ ] `grep -n "define PROTOCOL" runtime/online_wire.h` → 38.
- [ ] `git diff f1-cerrada -- runtime/online.h` → vacío, o solo lo que el coordinador aprobó: ________.

## 3. Cadena OA — partición sin cambio de comportamiento

| Paso | `oa_check.py` | Compila Win | Compila Linux | Firma `--comparar` | Recorrido | Nivel |
|---|---|---|---|---|---|---|
| T206 reordenar | | | — | | corto | |
| T207 cortar (commit 1) | | | | | completo | |
| T207 prefijos (commit 2) | | | | | corto | |
| T208 partir `g_act[]` | — | | | | completo | |
| T209 orden explícito | — | | | | completo | |

- [ ] `wc -l runtime/oa_*.c`: ________ (ninguno > 1 500, o excepción anotada).
- [ ] `grep -rn "g_act\[" runtime/` → nada.
- [ ] `rt_online_frame` con menos de 80 líneas y el orden en un comentario.
- [ ] Número de llamadas a `online_players` por fotograma sin cambios: antes ____ / después ____.
- [ ] Tamaños de función a `-O0 -fno-inline` antes y después: iguales / diferencias: ________ (indicio, no prueba).
- [ ] Bitácora: qué demuestra cada comprobación y qué no (`plan.md` §3.4).

## 4. Cambios de comportamiento intencionados

| Tarea | Qué cambia | Evidencia | Nivel |
|---|---|---|---|
| T210 Links validados al principio | el Link borrado se detecta antes de usarlo | morir ×5 con Magnesis a la vista: ____ | |
| T212 `RT_ON_RESET` | el estado se olvida en el reinicio en caliente | `oa_statics.py` → ____ ; "Quit to start screen" + Continuar ×3 en A y en B: ____ | |
| T213 Validación única | datos inválidos se descartan en un solo sitio | `test-online-validate`: ____ ; descartes en el recorrido normal: ____ (debe ser 0) | |

- [ ] En el recorrido normal (dos juegos honestos) la validación no descarta nada.
- [ ] Inventario de estáticas: total ____ ; limpiar ____ ; permitidas ____ (con motivo).

## 5. Puerta de la fase 0 (T214)

- [ ] Todas las pruebas automáticas en 0 en Windows y en Docker.
- [ ] Pruebas de la línea base de F0: mismo resultado o mejor.
- [ ] Recorrido completo desatendido: todos los pasos PASA.
- [ ] Recorrido con ventana (dueño): lista de `F1/verificacion.md` §3 marcada.
- [ ] Etiqueta local `f2-fase0`.
- [ ] El dueño aprueba abrir los carriles.

## 6. Carriles

| Carril | Tarea | Ficheros tocados (deben ser solo los suyos) | `verify` | Nivel | No verificado |
|---|---|---|---|---|---|
| Mundo | T215 | | | | |
| Enemigos y botín | T216 | | | | |
| Runas | T217 | | | | |
| Red | T218 | | | | |
| Bot y Docker | T219 | | | | |
| Traducciones | T220 | | | | |

Comprobación de que un carril no se salió de sus ficheros: `git diff --stat f2-fase0...coop/<carril>`.

Tabla de medidas de red (T219):

| Escenario | STATE/s recibidos bot A | STATE/s recibidos bot B | Observaciones |
|---|---|---|---|
| sin `netem` | | | |
| 80 ms | | | |
| 80 ms + 2 % pérdida | | | |
| 200 ms + 5 % pérdida | | | |
| un bot con `BOT_NOREAD` | | | fallo H-1 reproducido: sí / no |

## 7. Cierre

- [ ] `tools/i18n_check.py` → 0.
- [ ] Comprobación de trampas de nombres (`F0/plan.md` §8) limpia.
- [ ] `git diff f1-cerrada --stat -- tools/patches.py tools/game_sites.py` → vacío.
- [ ] Recorrido completo, desatendido y con ventana, sobre `coop/main` final.

| # | Criterio de `spec.md` | Cumplido | Evidencia |
|---|---|---|---|
| S1 | Pruebas de protocolo y sesión | | |
| S2 | Una sola definición de mensajes | | |
| S3 | `online_actors.c` partido | | |
| S4 | Sin cambio de comportamiento | | |
| S5 | `g_act[]` partida | | |
| S6 | Orden explícito | | |
| S7 | `RT_ON_RESET` completo | | |
| S8 | Validación única | | |
| S9 | CI | | |
| S10 | Sin regresión en juego | | |
| S11 | Carriles integrados | | |

## 8. Plantilla de bitácora

Copiar a `specs/bitacora/<aaaa-mm-dd>-F2-<tarea>-<tema>.md`. Sin rutas personales ni logs crudos.

```markdown
# Bitácora · F2 · T2xx · <tema>

- Fecha:
- Quién: dueño / agente (modelo)
- Rama y commits (`git log --oneline f1-cerrada..HEAD`):
- Tipo de tarea: refactor sin cambio de comportamiento / cambio de comportamiento intencionado / pruebas / carril
- Versión del juego (build id) y versión del overlay:
- Hardware: CPU, RAM total y libre, GPU y controlador:
- Ajustes: resolución, límite de FPS, con o sin ventana, variables `SAVAGEWIND_*` usadas:
- FPS (habitual / mínimo) de cada juego:

## Qué se hizo
(pasos, en orden)

## Comprobaciones automáticas
| Comando | Resultado |
|---|---|
| `oa_check.py ...` | |
| `oa_statics.py` | |
| `make test-online-...` | |
| compilación Windows / Linux | |
| `firma_log.py --comparar` | |

## Qué se observó en el juego
(recorrido: pasos PASA/FALLA; lo visto por el dueño)

## Diferencias de comportamiento
(ninguna / las intencionadas, una por línea, con su motivo)

## Qué demuestra esto y qué NO
(nivel L / P / J por afirmación; caminos no probados)

## Fallos vistos y no arreglados
(en un refactor no se arregla nada: aquí se apunta lo que se vio)

## Logs adjuntos
(nombre de los ficheros fuera del repositorio y líneas citadas)
```
