# F5 — Funciones difíciles: verificación de extremo a extremo

F5 se verifica **bloque a bloque** (tarea T539). Un bloque se cierra con OK, con FALLO (y tarea abierta) o con
"no viable" (y un ADR que lo demuestre y dé la alternativa). Cada casilla necesita evidencia en `specs/bitacora/`.

Las secciones siguen la letra de bloque. El **orden de trabajo** es el de `plan.md` §10: primero el bloque C
(santuarios juntos, sección 3), que es el objetivo prioritario `[DUEÑO]` (ADR-008, decisión 20), y después B, H, E,
A, G, F y D. Las reglas marcadas `[DUEÑO]` salen de `specs/decisiones/ADR-007-respuestas-del-dueno-ronda-1.md`,
`ADR-008-respuestas-del-dueno-ronda-2.md` y `ADR-009-respuestas-del-dueno-ronda-3.md`. Todo se prueba con 2
jugadores salvo el bloque G (ADR-007, decisión 12).

## 0. Preparación

### 0.1 Sin juego

```
make test-online test-coop
sh tools/nettest/run.sh all                 # escenarios con bots en Docker
sh tools/nettest/measure_n.sh               # ancho de banda con 2, 4 y 8 bots
tools/venv/Scripts/python.exe tools/i18n_check.py
tools/venv/Scripts/python.exe tools/test_game_sites.py
tools/venv/Scripts/python.exe tools/coop_addr_inventory.py
```

Red degradada en Docker: `sh tools/nettest/netem.sh <perfil>` (`lan`, `fibra`, `normal`, `wifi`, `movil`, `rafagas`,
`corte`); por dentro, `tc qdisc ... netem` en la interfaz del contenedor (`cap_add: NET_ADMIN`).

### 0.2 Dos juegos en un PC

Procedimiento de `docs/ONLINE.md` líneas 738-769. Cierra Docker Desktop y el navegador (unos 9,5 GB de RAM).
Siempre con **copias** de guardado.

```powershell
# Juego A (anfitrión)
$env:SAVAGEWIND_SAVE_DIR="<copia-A>"; $env:SAVAGEWIND_NO_LAUNCHER="1"; $env:SAVAGEWIND_AUDIO_GAIN="0"
$env:SAVAGEWIND_ONLINE_PORT="52530"; $env:SAVAGEWIND_ONLINE="host"; $env:SAVAGEWIND_ONLINE_NAME="Host"
$env:SAVAGEWIND_CONTROL="7777"; .\build\savagewind.exe
# Juego B (invitado): <copia-B>, SAVAGEWIND_ONLINE="127.0.0.1", SAVAGEWIND_ONLINE_NAME="Client", SAVAGEWIND_CONTROL="7778"
```

Conducir: `tools\venv\Scripts\python.exe tools\swctl.py --port <p> goto|teleport|pos|flag|actors|press ...`
(`docs/CONTROL.md`). Recuerda: `teleport` y `goto` solo funcionan dentro de un mapa; a un santuario hay que entrar
andando (o con lo que entregue el bloque B).

Guardados que conviene preparar (copias): uno junto a un santuario sin completar de cada tipo de la matriz; uno a la
entrada de una Bestia Divina; uno justo antes de cada momento de historia de la lista corta (ADR-007, decisión 3:
piedra y primera torre, final de cada Bestia Divina, recuerdos, jefes finales). Para T540 basta el primero: una
copia junto a un santuario sencillo sin completar.

### 0.3 Herramientas del renderizador (bloque A)

| Variable | Para qué |
|---|---|
| `SAVAGEWIND_WINDOW_SHOT=<f>` | Captura de la ventana (con overlay) en ese fotograma → `build/window.png` |
| `SAVAGEWIND_SHOT_AT=<f>` | Captura de la imagen del juego (sin overlay) → `build/frames/frame_N.png` |
| `SAVAGEWIND_DEBUG_FRAME=<f>` | Vuelca cada pase a `build/pass_NNN.png` y registra cada dibujo (`[draw]`) |
| `SAVAGEWIND_DEBUG_PICK=<x>,<y>` y `SAVAGEWIND_DEBUG_PICK_RT=<i>` | Lista qué dibujos cambiaron ese píxel |
| `SAVAGEWIND_EXIT_AT=<f>` | Termina en ese fotograma (pruebas desatendidas) |

Para comparar dos capturas hace falta la misma escena exacta: mismo guardado, cámara quieta, mismo fotograma,
`SAVAGEWIND_DEBUG_FIXED_STEP=<fps>` si el tiempo variable estorba, y sin efectos animados delante.

### 0.4 Tras cada lote de sitios o parches

```
make gen && make -j8
make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id     # anótalo: cambia con cada lote
```

Los **dos** jugadores deben regenerar y recompilar; con identificadores distintos la conexión se rechaza (F3).

---

## 1. Bloque A — CU-13: color en el modelo 3D del segundo Link

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| A.1 | Investigaciones cerradas | Bitácoras de T503, T504, T505, T507 (y T506 si se hizo) | Cada una responde su pregunta con evidencia | ☐ |
| A.2 | ADR | `specs/decisiones/ADR-tinte-3d.md` | Camino elegido y firma | ☐ |
| A.3 | Lógica | `build/test_coop_tint` | Pasa | ☐ |
| A.4 | Opción apagada = igual que antes | Captura de la misma escena antes del cambio y después con la opción apagada; `cmp` | Ficheros idénticos | ☐ |
| A.5 | Opción encendida | Misma escena con la opción encendida | Solo cambian los píxeles del compañero (cuerpo y ropa), con un tinte suave que deja reconocer el modelo; el Link local, igual `[DUEÑO]` (ADR-007, decisión 8) | ☐ |
| A.6 | Varios contextos | De día, de noche, bajo la lluvia, en un santuario, en el menú con el mundo detrás | El tinte se ve en todos; anota donde no | ☐ |
| A.7 | Sombras y reflejos | Mira la sombra del compañero y su reflejo en agua | Sin artefactos de color en la sombra | ☐ |
| A.8 | Cambio de equipo | El compañero cambia de armadura y arma 10 veces | El tinte sigue a las piezas nuevas; ninguna pieza de otro actor se tiñe | ☐ |
| A.9 | Con escaladores y generación de fotogramas | TAA, FSR/DLSS/XeSS si están disponibles, generación de fotogramas | Sin parpadeos del tinte | ☐ |
| A.10 | Daltonismo | `SAVAGEWIND_CVD_SIMULATE=protan|deutan|tritan` | El compañero sigue distinguiéndose (el overlay de F4 sigue dando forma y nombre) | ☐ |
| A.11 | Coste | fps y tiempo de fotograma, 60 s, misma escena, opción apagada y encendida | Tabla en bitácora con GPU, controlador y resolución | ☐ |
| A.12 | Cachés | Si se tocó el traductor: aviso claro y aprobación del dueño en el ADR | Anotado | ☐ |
| A.13 | Reinicio en caliente | "Quit to start screen" con el tinte activo y volver | Sin crash; el tinte vuelve | ☐ |

Si el bloque se cierra como "no viable": ADR con las evidencias. La solución final es entonces el **nombre en
color y el marcador con forma** del overlay (F4) `[DUEÑO]` (ADR-009, decisión 22); comprueba que siguen
funcionando (fila A.10) y sigue con los demás bloques: este cierre no bloquea nada.

## 2. Bloque B — CU-14a: teletransporte entre mapas

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| B.1 | Investigación | Bitácora de T511 o de T512 | Camino viable identificado | ☐ |
| B.2 | Lógica | `build/test_coop_tp` | Pasa (incluye "requiere cambio de mapa") | ☐ |
| B.3 | Campo → santuario | A dentro de un santuario, B en el campo; B pulsa "Go to" | B aparece en la entrada del santuario; los dos se ven | ☐ |
| B.4 | Santuario → campo | Al revés | B aparece junto a A en el campo | ☐ |
| B.5 | 5 + 5 viajes | Repite B.3 y B.4 cinco veces cada uno, con santuarios distintos | 10 llegadas, 0 cuelgues | ☐ |
| B.6 | Bestia | A dentro de una Bestia | Llega, o rechazo claro si no está soportado | ☐ |
| B.7 | Aviso de recarga | Si se usa la recarga de guardado (aceptada: ADR-008, decisión 20) | Aviso antes de viajar: se cargará el último guardado. El invitado guarda en su perfil cooperativo; su partida individual no cambia | ☐ |
| B.8 | La sesión sobrevive | Durante el viaje | El compañero ve "cargando" y luego al jugador; sin desconexión, o reconexión automática de F3 dentro del plazo de 10 minutos (ADR-008, decisión 18) | ☐ |
| B.9 | Nombre de mapa falso | Bot que anuncia un mapa con caracteres raros o inexistente | El viaje se rechaza; sin crash | ☐ |
| B.10 | Bloqueos | En evento, muerto, PvP activo | Rechazo con motivo | ☐ |
| B.11 | Progreso | Tras 5 viajes, comprobar inventario y flags de muestra | Nada perdido salvo lo avisado en B.7 | ☐ |

## 3. Bloque C — CU-14b: santuarios juntos (primer bloque de la fase)

`[DUEÑO]` (ADR-008, decisión 20): objetivo prioritario. Se verifica antes que A y que E.

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| C.0 | Investigación previa | Bitácora de T540 (dos instancias en un PC, los dos en el mismo santuario) | Tabla por fases con lo visto en cada juego y conclusión sobre qué funciona hoy | ☐ |
| C.1 | Matriz | Bitácora de T514 con los 8 santuarios | Tabla completa | ☐ |
| C.2 | Sin crashes | Suma de la matriz | 0 cuelgues en los 8 | ☐ |
| C.3 | Verse | En cada santuario | Los dos se ven desde que el segundo entra | ☐ |
| C.4 | Ascensor montado | Uno sube en el ascensor; el otro mira | El que mira ve subir ascensor y jugador juntos | ☐ |
| C.5 | Bolas y cajas | Uno empuja, el otro mira; luego al revés | Misma posición final en ambos (± 0,5 m) | ☐ |
| C.6 | Interruptor y puerta | Uno pisa el interruptor | La puerta se abre en los dos juegos | ☐ |
| C.7 | Aparato de giroscopio | Uno lo maneja | El otro ve moverse el mecanismo | ☐ |
| C.8 | Combate | Santuario de combate, con y sin el anfitrión dentro | Mismo enemigo, misma vida, un solo botín | ☐ |
| C.9 | Recompensa | Al completar | Orbe para los dos, siempre `[DUEÑO]` (ADR-007, decisión 2); flag en el anfitrión | ☐ |
| C.9b | Recompensa de quien no estaba | Uno completa el santuario solo; el otro se une después a la sesión | El que se une queda igualado por la tarea de ponerse al día al entrar de F4 (mismos contenedores de corazón y mejoras de resistencia que el anfitrión: ADR-008, decisión 13) | ☐ |
| C.9c | No es momento de historia | Uno habla con el monje con el compañero fuera del santuario, con "Story needs both players" activada | No hay espera ni aviso: el santuario se completa (ADR-007, decisión 3) | ☐ |
| C.10 | Uno entra tarde | El segundo entra cuando el primero va por la mitad | Se anota qué ve (el rompecabezas no está en flags: es un límite conocido) | ☐ |
| C.11 | Límites documentados | `docs/coop-jugar.md` | Lista lo que no se sincroniza | ☐ |

## 4. Bloque D — Bestias Divinas

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| D.1 | Dentro, a dúo | Bitácora de T516 | Tabla de partes móviles | ☐ |
| D.2 | Mover una parte | Uno la mueve desde el mapa de la Bestia | El otro ve el mismo ángulo final | ☐ |
| D.3 | Jugador sobre una parte que se mueve | Uno encima, el otro la mueve | No atraviesa el suelo ni sale despedido; anota lo visto | ☐ |
| D.4 | Jefe | Los dos contra el jefe | El daño de ambos cuenta (ya lo hace el PR para jefes con identificador de mapa) | ☐ |
| D.5 | Desde fuera | Bitácora de T517 | Una de las tres respuestas, con evidencia | ☐ |
| D.6 | 20 minutos dentro | Sesión continua | Sin crash | ☐ |

## 5. Bloque E — CU-14c: momentos de historia con los dos presentes

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| E.1 | Decisión | ADR-007 (decisiones 3, 4 y 6) y ADR-009 (decisión 21) en `specs/decisiones/` | Ya decidido: T501 resuelta. La tabla de T518 no contiene más momentos que los de la lista corta | ☐ |
| E.2 | Catálogo | Bitácora de T518 | Una fila por momento de la lista | ☐ |
| E.3 | Lógica | `build/test_coop_story` | Pasa | ☐ |
| E.4 | Opción apagada | "Story needs both players" desactivada | Comportamiento igual que antes de F5 | ☐ |
| E.5 | Botón, compañero lejos | A junto a un punto de la tabla; B a más de 30 m; A pulsa el botón | No empieza; A ve "Waiting for your partner" y la oferta de traer a B; B ve aviso y marca | ☐ |
| E.5b | Oferta de teletransporte | A elige traer a B; B acepta. Repite con B rechazando | Si acepta, B aparece junto a A y el evento puede empezar; si rechaza, B no se mueve (ADR-007, decisión 4; ADR-008, decisión 16) | ☐ |
| E.6 | Botón, compañero cerca | B llega a menos de 30 m en el mismo mapa | A puede empezar. Solo A ve la escena; B está presente y recibe el progreso (flags de muestra en B y en el anfitrión). No se lanza la escena en el juego de B (ADR-009, decisión 21) | ☐ |
| E.7 | 3 eventos de botón | Repite E.5 y E.6 con tres eventos distintos | Igual en los tres | ☐ |
| E.8 | No estorba | Lejos de los puntos de la tabla | A funciona con normalidad (hablar, recoger, abrir cofres) | ☐ |
| E.9 | Zona (si T522) | Evento que arranca al entrar en una zona, compañero lejos | No empieza; al llegar el compañero, empieza. 10 repeticiones sin cuelgue | ☐ |
| E.10 | Compañero desconectado | B sale de la sesión; A (anfitrión) junto a un punto de la tabla | A ve la espera y puede continuar solo tras confirmar, en ese momento; no hay que esperar al plazo de reconexión de 10 minutos (ADR-007, decisión 6; ADR-008, decisión 18) | ☐ |
| E.10b | Continuar solo, compañero conectado | B conectado pero lejos; A (anfitrión) elige continuar y confirma. Repite cancelando el diálogo | Al confirmar, el evento empieza de inmediato y B recibe un aviso y el progreso; al cancelar, sigue la espera. Sin cuenta atrás | ☐ |
| E.10c | El invitado no puede continuar solo | B (invitado) junto a un punto de la tabla, A lejos | B no tiene la acción de continuar solo; un STORY "continúa solo" enviado por un bot que no es anfitrión se rechaza | ☐ |
| E.11 | Compañero en otro mapa | B dentro de un santuario | Cuenta como ausente | ☐ |
| E.12 | Fin de sesión | Salir con una espera activa | El bloqueo de A desaparece | ☐ |
| E.13 | El pedestal y las torres siguen bien | Prueba original del PR con `BOT_DEMO=1` | El bloqueo de 30 m sigue funcionando | ☐ |
| E.14 | Bloqueo existente frente a la lista corta | Bitácora de T521, paso 1 | Anotado qué cubre hoy el bloqueo de A; la exigencia de presencia solo alcanza a la piedra y a la primera torre, no a las demás torres ni a ningún santuario (ADR-007, decisión 3) | ☐ |

## 6. Bloque F — Colisión con el compañero

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| F.1 | Investigación | Bitácora de T524 | Veredicto viable / no viable | ☐ |
| F.2 | 20 carreras | Correr de frente contra el compañero | 0 traspasos | ☐ |
| F.3 | Subirse encima | Saltar sobre el compañero | Se puede (si el veredicto fue viable) | ☐ |
| F.4 | Enemigos y cámara | Combate con el compañero al lado | Los enemigos no se quedan atascados contra la marioneta; la cámara no salta | ☐ |
| F.5 | Coste | fps con y sin colisión por cuerpo | Anotado | ☐ |
| F.6 | Opción | Cambiar entre cuerpo / empuje / apagado | Los tres modos funcionan | ☐ |
| F.7 | 20 minutos | Sesión continua con colisión por cuerpo | Sin crash | ☐ |
| F.8 | Overlay regenerado | Tras cambiar el generador | Los dos PC regeneran `online_mod/` (número de versión nuevo) y la conexión no se rechaza | ☐ |

## 7. Bloque G — Más de 2 jugadores

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| G.1 | Línea base | `sh tools/nettest/measure_n.sh` antes de T528 | CSV con N = 2, 4, 8, juntos y repartidos | ☐ |
| G.2 | Filtro, unidad | `build/test_coop_interest` | Pasa | ☐ |
| G.3 | Filtro, integración | `sh tools/nettest/run.sh interest` | ~1 mensaje/s lejos, ~20/s cerca | ☐ |
| G.4 | Ahorro repartidos | `measure_n.sh` tras T528, 4 bots repartidos | Subida del servidor ≤ 50 % de la línea base | ☐ |
| G.5 | Codificador | `build/test_coop_statecodec` (con sanitizadores) | Pasa; estado sin cambios < 100 bytes | ☐ |
| G.6 | Ahorro juntos | `measure_n.sh` tras T530, 4 bots juntos | Subida del servidor ≤ 50 % de la de G.4 en esa disposición | ☐ |
| G.7 | Esqueleto | `measure_n.sh` con `BOT_SKEL=1` tras T531 | Reducción anotada | ☐ |
| G.8 | Criterio R5-14 | 4 jugadores repartidos frente a 2 | Subida del anfitrión ≤ 2 × la de 2 jugadores | ☐ |
| G.9 | Calidad visual | Dos juegos reales, 10 minutos | El compañero se mueve igual que antes (sin tirones nuevos) | ☐ |
| G.10 | Red mala | `netem wifi` con 4 bots | Sin desincronización permanente tras pérdidas (cada bloque se reenvía como mucho a los 2 s) | ☐ |
| G.11 | 4 jugadores | 2 juegos + 2 bots, 20 minutos | Sin crash; enemigos, botín y reglas correctos. El límite de la sesión real sigue en 2 (ADR-007, decisión 12): esto prueba el diseño para N, no lo abre | ☐ |
| G.12 | Entrar tarde | Un jugador entra a mitad | Recibe el estado completo de todos en < 2 s | ☐ |
| G.13 | Fuzzing | `sh tools/nettest/fuzz.sh 120` con el codificador nuevo | Sin hallazgos | ☐ |

## 8. Bloque H — Menos direcciones fijas

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| H.1 | Inventario | `tools/venv/Scripts/python.exe tools/coop_addr_inventory.py --check 62` (antes de empezar) | Sale con 0 | ☐ |
| H.2 | Sitios | `cd tools && ../tools/venv/Scripts/python.exe game_sites.py find ../extracted/exefs/main` | Todos los sitios nuevos `ok` | ☐ |
| H.3 | Pruebas de sitios | `tools/venv/Scripts/python.exe tools/test_game_sites.py` | Sale con 0 | ☐ |
| H.4 | Recuento final | `coop_addr_inventory.py` | 0 direcciones de función; los punteros de datos que queden, listados con motivo en la bitácora | ☐ |
| H.5 | Carga | Registro de arranque | Línea `[gamelib] ... (game version 1.9.0, N sites)` con N mayor que antes | ☐ |
| H.6 | Sin regresión | Recorrido de 30 minutos de F1 tras **cada** lote | Sin crash; mismo comportamiento | ☐ |
| H.7 | Grupo ausente | Prueba sintética: quitar un sitio de un grupo opcional de `KNOWN` en una copia local y generar con `--any-version` sobre un binario de prueba, o simular `g_site.<x> = 0` | Ese subsistema se desactiva solo y lo registra una vez; nada salta a una dirección 0 | ☐ |
| H.8 | Nº de recompilaciones | Lista de valores de `GAMELIB_ID` durante el bloque | Como mucho 3 cambios | ☐ |
| H.9 | Sin valores cambiados | Revisión del diff de T537 | Ningún desplazamiento cambió de valor | ☐ |

## 9. Regresión general (al cerrar cada bloque)

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| R.1 | Recorrido de F1 | CU-01 a CU-03, 30 minutos, dos juegos | Sin crash | ☐ |
| R.2 | Recorrido de F4 | `verificacion.md` de F4, secciones 1 a 7 (versión corta: una fila por caso de uso) | Igual que al cerrar F4 | ☐ |
| R.3 | Suite de red | `sh tools/nettest/run.sh all` | Pasa | ☐ |
| R.4 | Idiomas | `tools/venv/Scripts/python.exe tools/i18n_check.py` | `0 missing` | ☐ |
| R.5 | Compila en Linux | `make runtime-objs` en el contenedor | Sin avisos nuevos | ☐ |
| R.6 | Opciones apagadas | Con todas las opciones nuevas de F5 apagadas | Comportamiento idéntico al del cierre de F4 | ☐ |

---

## 10. Plantilla de bitácora

Un fichero por prueba o investigación en `specs/bitacora/`, con nombre
`F5-<tarea o bloque>-<fecha AAAA-MM-DD>.md`.

```markdown
# <Tarea, investigación o bloque> — <fecha>

- Quién: <dueño | agente (modelo)>
- Commit: <hash corto>   Rama: <rama>
- Protocolo: <n>   Versión del juego: 1.9.0   GAMELIB_ID: <16 hex>   online_mod: v<n>
- Equipo: <CPU, GPU y controlador, RAM>   Sistema: <Windows 11 / contenedor Linux>
- Montaje: <dos juegos en un PC | juego + bot | Docker (perfil netem, N bots) | dos PC>
- Ajustes relevantes: <resolución, fps, escalador, opciones nuevas, variables de entorno>

## Pregunta (solo investigaciones)
<la pregunta exacta de la tarea>

## Qué se hizo
<una frase por paso, con la orden exacta>

## Respuesta / resultado
| Paso o pregunta | Esperado | Visto | OK / FALLO / respuesta |
|---|---|---|---|

## Evidencia
<salida recortada de registros, o ruta relativa de capturas en build/ (no se suben)>

## Para investigaciones con Ghidra
- Sitio propuesto: <nombre>, clase <code|call|ref|got|imm>
- Línea `Site(...)` generada por `tools/game_sites.py pattern`: <pegar la línea>
- Qué hace, con tus palabras: <argumentos, valor devuelto, precondiciones>
- Prueba en el juego: <qué se hizo y qué se vio>
- (Nunca desensamblado, pseudocódigo ni nombres tomados de proyectos de descompilación.)

## Hipótesis frente a hechos
- Visto en el juego: <...>
- Comprobado con prueba sintética: <...>
- Solo supuesto: <...>

## Veredicto
<viable / no viable / pendiente> y siguiente tarea
```

Reglas: sin rutas absolutas, sin nombres de usuario, sin IP públicas, sin guardados, capturas con arte del juego ni
ficheros del juego adjuntos.
