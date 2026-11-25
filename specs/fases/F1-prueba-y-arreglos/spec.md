# F1 — Probar el sistema y arreglos rápidos esenciales · spec

Requisito: F0 cerrada (juego compilado, overlay `online_mod/` generado, línea base de pruebas anotada).

Las referencias `fichero:línea` son del commit `350c5f8` de la rama del PR y se comprobaron leyendo el código. Se
mueven con cada cambio: cada tarea indica además el **nombre de la función** para localizar el sitio con `grep`.

## Objetivo

1. Saber, con evidencia, qué funciona y qué falla hoy en el modo online del PR #294 con dos juegos en este PC.
2. Corregir nueve problemas pequeños y esenciales ya identificados en los mapas 06 y 07, cada uno con su
   reproducción (o la explicación honesta de por qué no se puede reproducir) y su verificación.
3. Terminar con una sesión de 30 minutos sin cuelgues en un recorrido fijo, y una lista priorizada de lo que queda.

## Alcance

**Parte A — línea base** (antes de tocar código):

- Dos instancias en un PC según `docs/ONLINE.md`, "Testing with two games on one PC".
- Recorrido guionizado que cubre CU-01, CU-02 y CU-03.
- Registro de cuelgues y logs en `specs/bitacora/`.

**Parte B — arreglos** (uno por tarea):

| # | Arreglo | Sitio (commit `350c5f8`) | Hallazgo |
|---|---|---|---|
| 1 | Puerta de versión: no usar direcciones fijas de 1.9.0 si el juego no es 1.9.0 | `online_actors_on`, `runtime/online_actors.c:225-229`; `rt_online_frame`, `:5721-5726` | mapa 07 A1 |
| 2 | `has_body()` antes de `FN_BODY_TO` en mecanismos | `movers_update`, `runtime/online_actors.c:4928-4929` y gancho de prueba `:4901-4902` | mapa 07 A2 |
| 3 | Runas: usar el Link remoto solo si sigue vivo | `grab_update` / `stop_update` llamados en `:5976-5977`; la comprobación `alive` está después, en `:6036` | mapa 07 A5 |
| 4 | Lista de permitidos para nombres de actor que llegan por red | botín `:3604` (y `loot_host_add`, `:3360`); enemigos `:2226` (y la copia del registro, `:2398`) | mapa 07 A4 |
| 5 | Rechazar números no finitos o fuera de rango que llegan por red | `remote_snap`, `runtime/online.c:907`; `cl_message`, `:1042-1113` | mapa 06 H-6, mapa 07 M4 |
| 6 | Limpiar todo el estado de sesión al cortar | `stop_all`, `runtime/online.c:1322-1335` | mapa 06 T-1, T-2 |
| 7 | `SIGPIPE` en Linux y macOS | `send_all`, `runtime/online.c:140-148` (el `send` está en `:143`) | mapa 06 H-8 |
| 8 | Terminador de `shield` en ESTATE | `runtime/online.c:1095` (ENEMIES sí lo hace en `:1052`) | mapa 06 H-7 |
| 9 | Contraseña oculta en pantalla | `runtime/ui/online_ui.mm:64`; `opt_text`, `runtime/ui/option_rows.mm:302-318` | mapa 06 H-3 |

Dos tareas de apoyo hacen posibles las reproducciones: modos "hostiles" del bot (`tools/online_bot.c`) y un arnés
mínimo de prueba para `runtime/online.c` sin el juego.

## Fuera de alcance

- Partir `online_actors.c`, cambiar el orden de `rt_online_frame`, `RT_ON_RESET` general, capa única de validación:
  F2. En F1 los arreglos son **locales** y no reordenan nada.
- Envíos no bloqueantes, autoridad en el servidor, límite de intentos de contraseña, contraseña cifrada o fuera de
  `online.ini`, reconexión: F3.
- Cambios de protocolo: `PROTOCOL` sigue en 38 y no se toca ninguna estructura de `runtime/online.h` ni
  `WireState`.
- Traducir los textos de Online que faltan (fallo conocido de `tools/i18n_check.py`): se lista como pendiente.
- Cambios en `tools/patches.py` o `tools/game_sites.py` (obligarían a regenerar el juego).

## Casos de uso que cubre

| Caso | Qué se prueba en F1 | Estado esperado al salir |
|---|---|---|
| CU-01 Hospedar y unirse por IP | Anfitrión y cliente en `127.0.0.1`, puerto de prueba; con y sin contraseña | Funciona; evidencia en bitácora |
| CU-02 Ver al compañero moverse, con su equipo | Actor `OnlinePlayer` presente en el otro juego, sigue su posición; armadura y arma visibles (visto por el dueño) | Funciona; defectos visuales anotados |
| CU-03 Sesión de 30 min sin cuelgues | Recorrido fijo, desatendido y con ventana | 30 min sin cuelgue en el recorrido; lo que quede, priorizado para F2 |

## Criterios de salida (medibles)

| # | Criterio | Cómo se mide |
|---|---|---|
| S1 | Línea base ejecutada y anotada **antes** de cualquier arreglo | entrada de bitácora con commit `coop/main` de F0, tabla del recorrido rellena |
| S2 | Cada uno de los 9 arreglos tiene "antes" y "después" | entrada de bitácora por arreglo, con repro o con la nota "sin repro: verificado por lectura y por no regresión" |
| S3 | Pruebas automáticas nuevas en verde | `make test-online-gate` y `make test-online-net` terminan con 0 |
| S4 | Sin regresiones en la línea base de F0 | mismas pruebas de `F0/verificacion.md` §3, mismo resultado o mejor |
| S5 | Compila en Windows y en Linux | `make -j8` y `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13` terminan con 0 |
| S6 | Sesión de 30 min sin cuelgue en el recorrido, dos veces | una desatendida (logs) y una con ventana (dueño) |
| S7 | Lista priorizada de fallos pendientes | fichero en `specs/bitacora/` con gravedad, repro y fase destino |
| S8 | Protocolo intacto | `grep -n "define PROTOCOL" runtime/online.c` sigue dando 38; `git diff f0-cerrada -- runtime/online.h` vacío (la etiqueta local `f0-cerrada` la crea T101) |

## Riesgos

| Riesgo | Efecto | Mitigación |
|---|---|---|
| RAM: dos juegos usan unos 9,5 GB (`docs/ONLINE.md`) de 15,7 GB | Paginación, tirones o cierre de un juego que parecería un cuelgue del modo online | Cerrar Docker Desktop y navegador; no compilar durante la prueba; anotar RAM libre antes de empezar |
| VRAM: dos juegos en una GPU de 6 GB | Sin medir | Instancias sin ventana (`SAVAGEWIND_HIDDEN=1`) para la pasada desatendida; anotar uso |
| La mayoría de arreglos tocan dos ficheros (`online_actors.c`, `online.c`) | Conflictos si se hacen a la vez | Tres carriles con un dueño cada uno (ver `plan.md` §5); dentro de un carril, en serie |
| Varios fallos no se pueden reproducir a voluntad (2, 3) | "Arreglado" sin prueba directa | Decirlo en la bitácora; verificar por lectura, por log nuevo y por no regresión |
| Un arreglo cambia comportamiento sin querer | Regresión en CU-02 | Recorrido completo tras cada carril; cambios mínimos, sin reordenar |
| Las líneas citadas se mueven tras el primer arreglo | Un agente edita el sitio equivocado | Localizar por nombre de función; cada tarea lo indica |
| Los dos juegos comparten carpeta de trabajo (`online.ini`, `online_mod/`, `cache/`) | Interferencias entre instancias [sin verificar] | Nombre por variable de entorno (así `online.ini` no se escribe, `runtime/online.c:1545`); anotar cualquier aviso de caché |
| Guardados | Los flags del modo online se escriben en el guardado | Siempre copias; nunca la carpeta de guardado en uso |
