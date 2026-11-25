# Constitución: reglas no negociables

Estas reglas no se negocian en una revisión ni se saltan "solo esta vez". Si una regla estorba, se cambia aquí, con un
ADR en `decisiones/` que explique por qué, **antes** de escribir el código que la incumple.

Cada regla lleva su **por qué**, que cita un hallazgo concreto de los mapas (`mapa/0N-*.md`). Abreviaturas:
`OA:n` = `runtime/online_actors.c` línea n; `online.c:n` = `runtime/online.c` línea n. Las líneas se refieren al
checkout del PR #294 (commit `350c5f8`).

Vocabulario mínimo (más en `glosario.md`): **invitado** (*guest*) es el juego recompilado y su memoria; **anfitrión de
máquina** (*host*) es nuestro programa en C que lo ejecuta. No confundir con el **anfitrión de la partida**, que es el
jugador que hospeda la sesión.

---

## A. Legal y propiedad

**1. Nunca se sube al repositorio nada del juego ni derivado de él.** Ni archivos del juego (ExeFS, RomFS, NSP), ni
claves, ni partidas guardadas, ni código generado (`generated/`, `swgame.*`), ni la carpeta `online_mod/`, ni salida
de Ghidra o desensamblado, ni capturas de red o volcados de memoria usados como datos de prueba. Los datos de prueba
son sintéticos.
*Por qué:* es la "única regla dura" del proyecto original y su control automático rechaza esos nombres y extensiones
(`mapa/05` §4.2 y §8.2); la licencia MIT no cubre el juego ni el C generado a partir de él (`mapa/05` §8.1).

**2. No se copia nada de proyectos de descompilación.** Ni código, ni cabeceras, ni nombres de símbolos o estructuras.
Lo que sepamos del juego sale de nuestra propia copia y se describe con nuestras palabras.
*Por qué:* regla de `AGENTS.md` / `CONTRIBUTING.md` del proyecto (`mapa/01` §6, punto 12; `mapa/03` §5.2). El PR ya
usa nombres con espacios de nombres cuyo origen no consta (`mapa/07` §3.6): no añadimos más sin procedencia propia.

**3. Nada se sube al proyecto original, y al fork solo cuando el propietario lo pide.** Ningún *push*, PR, issue ni
comentario en `Revivedx/SavageWindRecomp` sin orden expresa del propietario del fork (cuenta CosmicAdc).
*Por qué:* decisión del plan ("Reglas de trabajo"); además el proyecto original responde "Multiplayer? No" y cierra
esas peticiones (`mapa/05` §8.3).

**4. Por la red no viajan recursos ni código del juego.** Solo estado (posiciones, nombres de actores, flags). El
modelo de Link, sus animaciones y texturas salen siempre de los archivos del propio jugador.
*Por qué:* `mapa/05` §8.2 ("sincronizar estado no debe transmitir assets") y `mapa/07` §1.1 (los actores remotos se
fabrican a partir de los archivos de cada usuario).

---

## B. Versión del juego

**5. El modo en línea solo se activa con el juego 1.9.0, y lo comprueba una puerta de versión.** Antes de llamar a
cualquier dirección fija del juego, el código compara `g_game_version` con `"1.9.0"`; si no coincide, el modo en línea
no arranca y lo dice en pantalla.
*Por qué:* `online_actors.c` usa 62 direcciones absolutas de 1.9.0 (47 funciones y 15 punteros) y hoy solo comprueba
que exista el overlay (`OA:225-229`); con otra versión saltaría a código arbitrario (`mapa/07` §3.1 y §6.1, A1). Las
demás funciones del runtime ya hacen esa comparación (`runtime/quick_inventory_sort.c:37`).

---

## C. Hilos

**6. La memoria y las funciones del invitado solo se tocan en el hilo del juego, dentro del gancho de fotograma.**
Leer o escribir memoria del juego, o llamar a una función suya, solo es legal dentro de `rt_online_frame` (o de otro
gancho que corra en el hilo del juego). Ningún otro hilo lo hace.
*Por qué:* no existe forma de llamar al invitado desde un hilo del anfitrión de máquina (`mapa/02` §3.4); los ganchos
"por fotograma presentado" corren en otro hilo y compiten con el juego (`mapa/02` §6.1, punto 1). Hoy el hilo de red
ya incumple esto: `sample_local` lee `PlayerInfo` 20 veces por segundo sin sincronizar (`online.c:828-831`,
`mapa/06` §1.3 y T-7), y los ganchos `rt_online_xlink_emit` / `rt_online_arrow` escriben anillos sin protección
desde cualquier hilo (`mapa/07` §6.2, M5).

**7. Los hilos de red solo hacen entrada/salida y entregan los datos por colas.** Reciben bytes, los validan, los
dejan en una cola; recogen de otra cola lo que hay que enviar. No interpretan el mundo ni guardan punteros del juego.
El hilo del juego nunca espera a la red: copia bajo cerrojo, suelta el cerrojo y entonces trabaja.
*Por qué:* código del anfitrión de máquina que se bloquea dentro de un hilo del invitado detiene a todos los hilos de
ese núcleo emulado, y un cerrojo tomado durante una llamada al juego queda tomado para siempre si hay reinicio
(`mapa/02` §6.1, puntos 2 y 3). El patrón correcto ya existe en `runtime/items.c:337-447`.

---

## D. Reinicio

**8. Todo el estado de sesión y todo puntero del invitado se limpian con `RT_ON_RESET`, y también al cortar la
sesión.** Cada variable global o `static` que guarde un puntero del juego, una cola o estado de la partida tiene su
limpieza registrada. Una sesión nueva empieza como en un proceso recién arrancado.
*Por qué:* "volver a la pantalla de inicio" borra la memoria del juego pero los hilos del anfitrión de máquina siguen
vivos (`mapa/02` §3.5 y §6.3). El PR no tiene ningún `RT_ON_RESET` y guarda ~85 variables con punteros del juego
(`mapa/07` §6.2, M1); además `stop_all` deja vivos los flags de misión, el botín y las colas de la sesión anterior
(`online.c:1322-1335`, `mapa/06` T-1), de modo que un jugador puede aplicar en una partida los flags de otra.

---

## E. Entrada de red no confiable

**9. Todo lo que llega por la red es hostil hasta que se valida, y se valida una sola vez, al recibir.** Como mínimo:
longitud exacta, contadores e índices dentro de rango, cadenas terminadas, números de coma flotante finitos **y**
acotados, enumerados dentro de su rango, y nombres de actor en una lista de permitidos. Detalle en
`contratos/validacion-red.md`.
*Por qué:* el receptor no comprueba que los números sean finitos (`mapa/06` H-6; `mapa/07` M4), un daño negativo sube
la vida de un enemigo (`OA:2308`), y un nombre de actor arbitrario llega a la función que crea actores
(`OA:3604`, `OA:2226`; `mapa/07` A4): un nombre concreto es un cuelgue documentado.

**10. Por la red nunca viajan direcciones de memoria, desplazamientos ni rutas de archivo.** Los objetos se nombran
con identificadores (ranura de jugador, id de enemigo, HashId del mapa, nombre de actor) que el receptor busca en sus
propias tablas.
*Por qué:* es lo que el PR ya hace bien y lo que hoy impide una escritura arbitraria en memoria (`mapa/06` §4.1).
Perderlo convertiría cualquier fallo de validación en ejecución de código.

**11. El anfitrión de la partida valida la autoridad.** Un mensaje que solo puede originar el anfitrión (enemigos,
lista de botín, flags del mundo, hora) se descarta si viene de otro; un mensaje dirigido al anfitrión solo se entrega
al anfitrión. Lo comprueba el servidor, no la buena fe de cada cliente.
*Por qué:* el servidor no sabe quién es el anfitrión y reenvía ENEMIES, LOOTS, QUESTS y PICK de cualquiera
(`online.c:278-298`); un invitado puede marcar los 32 768 flags y eso acaba en el guardado de todos, de forma
irreversible (`mapa/06` H-2).

---

## F. Código generado, parches y sitios

**12. El código generado no se edita nunca.** Lo que haya en `generated/` o salga de `make gen` se regenera; los
cambios de comportamiento del juego se hacen con un parche en `tools/patches.py` o con código en `runtime/`.
*Por qué:* el C del juego se genera en la máquina de cada jugador y la biblioteca lleva una huella (`GAMELIB_ID`)
calculada a partir de las herramientas; un cambio a mano se pierde en la siguiente generación y no se puede compartir
(`mapa/01` §3.3; regla 1).

**13. Cada parche comprueba la instrucción original, y cada sitio se encuentra por patrón.** Un parche afirma con
`assert` la palabra de instrucción que sustituye y reproduce su efecto. Una dirección nueva del juego entra como
sitio con patrón de bytes en `tools/game_sites.py` y se usa como `g_site.<nombre>`; no se añaden direcciones
literales nuevas. Las 62 que ya trae el PR se tratan como deuda: quedan detrás de la puerta de versión (regla 5),
agrupadas en un único módulo, y no crecen.
*Por qué:* regla del proyecto (`mapa/02`, cabecera; `mapa/01` §3.7, punto 1). El PR tiene 19 sitios por patrón frente
a 62 direcciones fijas (`mapa/07` §3.1), y uno de sus parches usa un diccionario en vez de `assert`, así que una
instrucción distinta falla con un error confuso (`tools/patches.py:636`, `mapa/07` §3.2).

**14. Los cambios que obligan a regenerar el juego se agrupan.** Tocar `tools/patches.py` o `tools/game_sites.py`
cambia `GAMELIB_ID` y obliga a **los dos jugadores** a recompilar el juego (del orden de 15 minutos). Esos cambios se
juntan en lotes planificados, con un único dueño, y se anuncian.
*Por qué:* `mapa/01` §3.3 y §6, punto 9; `mapa/02` §6.5, punto 22: dos compilaciones con parches distintos no son
intercambiables.

---

## G. Protocolo

**15. El protocolo de red tiene un único dueño, y todo cambio de formato sube la versión.** Solo la tarea dueña
edita `runtime/online.h`, las estructuras de cable de `runtime/online.c` y `contratos/protocolo.md`. Cualquier cambio
en el tamaño, el orden o el significado de un campo incrementa `PROTOCOL` y se refleja en el bot de pruebas en el
mismo cambio.
*Por qué:* el formato de cable es el layout en memoria de estructuras empaquetadas, sin campos opcionales
(`mapa/06` §2.1-2.2); el estado del jugador está escrito cuatro veces más la copia del bot, y un olvido en una de
ellas ya produjo un fallo real (`shield` sin terminar, `online.c:1095`; `mapa/06` §8.1 y H-7).

---

## H. Evidencia

**16. "Hecho" significa visto funcionando en el juego.** Que compile no cuenta. Todo informe separa cuatro niveles y
dice en cuál está cada afirmación: **hipótesis** (leído o razonado), **prueba sintética** (prueba automática sin el
juego), **compilación** (compila y enlaza) y **juego** (visto en una partida real, con entrada en `bitacora/`).
*Por qué:* los mapas se hicieron sin compilar ni ejecutar nada (`mapa/06`, cabecera; `mapa/07`, cabecera), y el
propio PR documenta 59 entradas entre intentos fallidos, cuelgues, fallos rodeados y pendientes, muchos de cosas que
parecían correctas al leerlas (`mapa/07` §5).

**17. Un cambio por PR; y si es de rendimiento, con números de antes y después.** Cada PR hace una sola cosa. Un
cambio que afirme mejorar (o no empeorar) el rendimiento trae la medida anterior y la posterior, en el mismo
hardware, anotadas en `bitacora/`.
*Por qué:* el PR #294 mezcla el modo en línea con otras tres funciones y eso lo hace difícil de revisar y de rebasar
(`mapa/06` §8.2). Y hay costes por fotograma sin medir: 63 llamadas a `getenv` y hasta ocho copias de la tabla de
jugadores por fotograma (`mapa/07` §6.3).

---

## I. Texto para el jugador

**18. Todo texto que vea el jugador pasa por el sistema de traducción.** Se escribe en inglés en el código, se
envuelve con `tr()` y se añade a las 13 tablas de `runtime/ui/i18n/*.inc`; `tools/i18n_check.py` debe pasar.
*Por qué:* el PR solo tradujo `es.inc`; faltan sus 63 textos en los otros 12 idiomas y el control de traducciones
falla (`mapa/06` §6.3; `mapa/05` §0, punto 7).

---

## J. Agentes de IA

**19. Una tarea sin `verify:` no se ejecuta en paralelo.**
*Por qué:* no existe ninguna prueba automática del modo en línea (`mapa/06` §7.1); sin una comprobación mecánica no
hay forma de saber si dos trabajos simultáneos se han roto entre sí.

**20. Un agente toca solo los archivos que declara su tarea en `files:`.** Si necesita otro, se detiene y lo pide.
*Por qué:* hay archivos que todo cambio quiere tocar (`menu.mm`, `settings.*`, las 13 tablas de traducción,
`patches.py`), señalados como puntos calientes de conflicto (`mapa/05` §7.1); y en `online_actors.c` hay estado
compartido que cruza cinco áreas (`g_act[]`, `mapa/07` §8.2).

**21. Un agente no declara terminado lo que no ha visto.** Informa con los cuatro niveles de la regla 16 y marca
`[sin verificar]` lo que no pudo comprobar. Nunca instala nada, ni modifica la carpeta del PR de referencia
(`pr294-online/`), ni ejecuta órdenes sobre el repositorio original.
*Por qué:* reglas 3 y 16; el checkout del PR es la referencia contra la que se comprueban las citas de línea de todos
estos documentos.

---

## Resumen de una línea por regla

| Nº | Regla |
|---|---|
| 1 | Nada del juego ni derivado en el repositorio |
| 2 | Nada copiado de proyectos de descompilación |
| 3 | Nada al proyecto original; al fork, solo cuando lo pida el propietario |
| 4 | Por la red no viajan recursos ni código del juego |
| 5 | Solo 1.9.0, con puerta de versión |
| 6 | El invitado solo se toca en el hilo del juego, en el gancho de fotograma |
| 7 | Los hilos de red solo hacen E/S y entregan por colas |
| 8 | Estado de sesión y punteros del invitado, limpiados con `RT_ON_RESET` y al cortar |
| 9 | Entrada de red validada una vez, al recibir |
| 10 | Nunca direcciones ni rutas por la red |
| 11 | El anfitrión valida la autoridad |
| 12 | El código generado no se edita |
| 13 | Parches con `assert`; sitios por patrón |
| 14 | Cambios que fuerzan regenerar, agrupados |
| 15 | Protocolo con dueño único y versión que sube |
| 16 | Hecho = visto en el juego; cuatro niveles de evidencia |
| 17 | Un cambio por PR; rendimiento con números |
| 18 | Todo texto visible, traducible |
| 19 | Sin `verify:`, no hay paralelo |
| 20 | Un agente solo toca sus `files:` |
| 21 | Un agente no declara lo que no ha visto |
