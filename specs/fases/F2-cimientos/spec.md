# F2 — Cimientos · spec

Requisito: F1 cerrada (nueve arreglos integrados, recorrido de 30 min sin cuelgue, arnés `test-online-net`).

Referencias `fichero:línea`: commit `350c5f8` de la rama del PR, comprobadas leyendo el código. Tras F1 las líneas
se habrán movido: cada tarea localiza por nombre de función.

## Objetivo

Dejar el código del modo online en un estado en el que varios agentes puedan trabajar a la vez sin pisarse y sin
romper lo que funciona:

1. Pruebas automáticas del protocolo y de la sesión, sin el juego, ejecutadas en cada cambio.
2. Una sola definición de los mensajes, compartida con `tools/online_bot.c`.
3. `runtime/online_actors.c` (6 468 líneas, un solo fichero) partido en los módulos de `specs/mapa/07` §8, **sin
   cambiar el comportamiento**, con una forma de demostrarlo.
4. `g_act[]` partida por dueño; orden de `rt_online_frame` escrito de forma explícita.
5. `RT_ON_RESET` para todo estado que guarde punteros del juego.
6. Una única capa de validación para todo lo que llega por red.
7. A partir de ahí, carriles en paralelo con ficheros disjuntos.

## Alcance

**Fase 0 (en serie, dos cadenas con un dueño cada una):**

| Cadena | Contenido | Ficheros |
|---|---|---|
| NET | Cabecera de cable compartida con el bot; pruebas de trama, codificación y máquina de estados de sesión; capa única de validación | `runtime/online.c`, `runtime/online_wire.h` (nuevo), `runtime/online_validate.c/.h` (nuevos), `tools/online_bot.c`, `tools/test_online_*.c` |
| OA | Firma de comportamiento; reordenar; partir en módulos; partir `g_act[]`; orden explícito del fotograma; Links remotos validados al principio; inventario de estáticas; `RT_ON_RESET` | `runtime/online_actors.c` → `runtime/oa_*.c`, `runtime/online_actors_internal.h` (nuevo) |

**Carriles (en paralelo, tras la fase 0):** mundo (`oa_world.c`), enemigos y botín (`oa_enemies.c`, `oa_loot.c`),
runas (`oa_runes.c`), red (`online.c`: estáticas de función y atómicos), bot y Docker, traducciones.

## Fuera de alcance

- Cambiar el protocolo: `PROTOCOL` sigue en 38 y el formato en el cable no cambia ni un byte. La cabecera
  compartida **mueve** definiciones; unas aserciones de tamaño lo vigilan.
- Envíos no bloqueantes, autoridad en el servidor, reconexión, descubrimiento, UDP/ENet: F3.
- Política de flags del anfitrión, recompensas personales, teletransporte, reanimar, pings: F4.
- Convertir las 62 direcciones fijas de 1.9.0 en sitios por patrón; cualquier cosa que exija Ghidra
  (`specs/mapa/07` M3, M8): F5 o posterior.
- Cambios en `tools/patches.py` o `tools/game_sites.py`.
- Reducir `ONLINE_MAX` de 8 a 2.

## Casos de uso que cubre

| Caso | Aportación de F2 |
|---|---|
| CU-03 Sesión de 30 min sin cuelgues | Cierra las causas estructurales: Link remoto usado sin validar, estado que sobrevive a un reinicio en caliente, datos de red sin validar |
| CU-01, CU-02 | No deben cambiar: son la prueba de "sin cambio de comportamiento" |
| CU-04 a CU-12 | Sin funcionalidad nueva; F2 crea los módulos y las pruebas sobre los que F3 y F4 trabajan en paralelo |

## Criterios de salida (medibles)

| # | Criterio | Cómo se mide |
|---|---|---|
| S1 | Pruebas de protocolo y de sesión sin el juego | `make test-online-wire test-online-net test-online-validate` terminan con 0 en Windows y en Docker; `make test-online-session` termina con 0 en Docker (Linux) |
| S2 | Una sola definición de mensajes | `grep -c "typedef struct" tools/online_bot.c` no incluye `WireState` ni `WireHello`; el bot incluye `runtime/online_wire.h`; `_Static_assert` de tamaños compila |
| S3 | `online_actors.c` partido | existen los módulos `runtime/oa_*.c`; ninguno pasa de 1 500 líneas salvo excepción anotada; `runtime/online_actors.c` no existe o solo contiene el orquestador |
| S4 | Partición sin cambio de comportamiento | `tools/coop/oa_check.py` termina con 0 (mismo conjunto de definiciones antes y después); firma de comportamiento del recorrido igual que la de referencia, dentro de la tolerancia definida en `plan.md` §4.4 |
| S5 | `g_act[]` partida | `grep -rn "g_act\[" runtime/` no devuelve nada |
| S6 | Orden explícito | `rt_online_frame` es una lista de llamadas a `oa_*_frame` con el orden documentado en un comentario y en `specs/contratos/hilos-y-frame.md` |
| S7 | `RT_ON_RESET` completo | `tools/coop/oa_statics.py` termina con 0 (toda estática con puntero del juego se limpia o está en la lista de permitidas con motivo); "Quit to start screen" + Continuar con sesión activa, 3 veces sin cuelgue |
| S8 | Validación única | todo `case` de `cl_message` pasa por `online_validate_*` antes de guardar; prueba con 100 000 mensajes aleatorios: tras validar, ningún float no finito, ninguna cadena sin terminar, ningún contador fuera de rango |
| S9 | CI | flujo `coop.yml` ejecuta las pruebas sin juego en cada push a `coop/main` |
| S10 | Sin regresión en juego | recorrido de F1 completo, desatendido y con ventana |
| S11 | Carriles integrados | cada carril con su `verify` cumplido y su entrada de bitácora |

## Riesgos

| Riesgo | Efecto | Mitigación |
|---|---|---|
| Partir 6 468 líneas introduce un cambio sin querer | Regresión difícil de localizar | Dos pasos (reordenar en un fichero; luego cortar), cada uno con comprobación automática de que el conjunto de definiciones es el mismo; un commit por paso; firma de comportamiento antes y después |
| No existe prueba "bit a bit" tras cortar en varios ficheros | La demostración no es absoluta | Decirlo: la igualdad exacta se demuestra para el texto de las definiciones; el comportamiento, por pruebas y recorrido |
| Símbolos que dejan de ser `static` chocan con otros del ejecutable (exporta todo, `Makefile` Windows) | Error de enlace o, peor, símbolo equivocado | Prefijo `oa_` para todo lo que cruce de módulo; `-Wall` y enlace completo en Windows y Linux |
| Una macro o una estática depende del orden en el fichero | Cambio silencioso al reordenar | Comprobar `#define` duplicados y `#undef` antes de reordenar; el compilador detecta el resto |
| `RT_ON_RESET` que llama al juego | Cuelgue en el reinicio (los hilos del juego ya están parados) | Regla: en un reset solo se ponen variables a cero; nunca `call(c, ...)` |
| Subir a variables de fichero las ~200 estáticas de función cambia su vida | Comportamiento distinto tras un reinicio (que es lo que se busca) o en uso normal (que no) | Inventario primero (T211), aplicar por módulo, recorrido tras cada módulo |
| La validación rechaza datos legítimos | Jugadores que dejan de verse | Límites tomados del emisor honesto (`sample_local`, `runtime/online.c:832-834`); contador de descartes en el log; recorrido |
| Dos cadenas tocan el `Makefile` | Conflictos | Bloques de prueba solo al final, uno por tarea; integra un dueño |
| Solo caben dos juegos en el PC | La verificación en juego es un cuello de botella | Verificación automática primero; turnos de juego al final de cada cadena y de cada carril |
| `upstream/test` avanza durante F2 | Conflictos al sincronizar | No sincronizar con upstream durante la fase 0; hacerlo antes de empezar o al terminar |
