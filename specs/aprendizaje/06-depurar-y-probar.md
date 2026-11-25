# 06 · Depurar y probar

Objetivo: las herramientas del día a día. Aquí no hay depurador paso a paso sobre el juego: se trabaja con
**logs, interruptores de entorno, un canal de control y experimentos repetibles**.

Regla que lo gobierna todo (`AGENTS.md:117-118`): anota versión del juego, FPS, GPU y driver, ajustes y
resultado observado, y separa **hipótesis, comprobaciones sintéticas, builds y juego verificado**.

---

## 1. Logs

- El runtime escribe un log (`savagewind.log`, el fichero que `CONTRIBUTING.md:66-67` pide adjuntar) y también
  por la terminal si lo lanzas desde una (`CONTRIBUTING.md:78`).
- Las líneas llevan prefijo por subsistema. Los tuyos: `[online]` (red y actores) y `[gamelib]` (qué biblioteca
  del juego se cargó, `runtime/gamelib.c:203`).
- **Primera línea que debes mirar siempre**: `[gamelib] … (game version 1.9.0, N sites)`. Si no está, o dice
  que la biblioteca es de otra versión, nada de lo demás importa (módulo 02 §2.4).
- Para confirmar que una ejecución desatendida llegó al mundo: los contadores de dibujo de las líneas
  `[vk] t=` (`AGENTS.md:114-115`).

Líneas `[online]` que conviene reconocer:

| Línea | Significa | Dónde |
|---|---|---|
| `server: X joined (slot N)` / `left` | el servidor aceptó o perdió a un jugador | `online.c:259`, `223` |
| `Connecting to …`, `Lost the connection`, `Disconnected by the server` | estado del cliente | `online.c:1170`, `1199`, `1318` |
| `wearing …|…|… carrying …` | cambió el equipo local | `online.c:1303-1304` |
| `N quest flags shared` | se cargó `quests.txt` | `online_actors.c:3877` |
| `quest flag X: set (another player did it)` | se aplicó un flag ajeno | `online_actors.c:3998` |
| `a load: the players' actors are gone` | se detectó una carga; se olvidan las marionetas | `online_actors.c:5757` |
| `the game removed X's Link …` | el juego borró una marioneta | `online_actors.c:6037` |
| `can't create a … for X (not loaded?)` | no se pudo crear el actor | `online_actors.c:6046` |

Como un mensaje de log es barato, **añade los tuyos mientras investigas** y quítalos (o ponlos tras un
interruptor) antes de entregar.

## 2. Interruptores `SAVAGEWIND_*`

Son variables de entorno leídas con `getenv`. No hay lista única: se documentan donde se definen. Búscalas así
(`CONTRIBUTING.md:103-104`):

```
grep -rn 'getenv("SAVAGEWIND_' runtime/ | less
```

Los que usarás en el cooperativo:

| Variable | Efecto | Fuente |
|---|---|---|
| `SAVAGEWIND_ONLINE=host` / `=<dirección>` | crear sesión o unirse al arrancar | `runtime/main.c:525-526` |
| `SAVAGEWIND_ONLINE_NAME`, `_PORT`, `_PVP`, `_COLLIDE` | ajustes para esta ejecución | `online.c:1531-1539` |
| `SAVAGEWIND_SAVE_DIR`, `SAVAGEWIND_SETTINGS` | guardado y ajustes en otra carpeta (**copias**) | `AGENTS.md:111` |
| `SAVAGEWIND_NO_LAUNCHER=1` | saltar la pantalla de inicio | `AGENTS.md:112` |
| `SAVAGEWIND_HIDDEN=1`, `SAVAGEWIND_AUDIO_GAIN=0` | sin ventana, sin sonido | `docs/ONLINE.md:740-741` |
| `SAVAGEWIND_CONTROL=<puerto>` | abre el canal de control | `docs/CONTROL.md:13` |
| `SAVAGEWIND_TEST_WARP=plateau`, `SAVAGEWIND_INPUT="900:A:8,…"` | arranque desatendido: aparecer en un sitio y pulsar botones en fotogramas dados | `AGENTS.md:113-114` |
| `SAVAGEWIND_SHOT_AT=<fotograma>` | captura de pantalla | `CONTRIBUTING.md:103` |
| `SAVAGEWIND_ROMFS_OVERLAY` | carpeta de datos superpuesta (el modo online usa `online_mod`) | `runtime/main.c:497-520` |
| `SAVAGEWIND_ONLINE_*_LOG` (`ENEMY_AS`, `SHOT`, `GEAR`, `POSE`, `LIFE`, `QUEST`, `FX`, `MOVER`…) | trazas por tema | `docs/ONLINE.md:755-758`; `online_actors.c:3895` |
| `SAVAGEWIND_ONLINE_*_TEST` | ganchos de prueba que fuerzan una situación | `online_actors.c:5881`, `5922`, `5934` |
| `SAVAGEWIND_FNPROF…` | perfilador de entradas a funciones del juego | `runtime/fnprof.c:1-60` |

Cuidado: algunas se leen una sola vez y se guardan en una variable `static`. Cambiarlas con el juego en marcha
no hace nada.

## 3. El canal de control y `tools/swctl.py`

Es un socket local por el que se envían **órdenes con nombre**: una línea de texto entra, una línea JSON sale
(`docs/CONTROL.md:1-4`, `40-41`).

- Se activa con `SAVAGEWIND_CONTROL=<puerto>`; escucha solo en `127.0.0.1` (`docs/CONTROL.md:19`).
- Cliente: `python tools/swctl.py [--json] [--port N] orden [args…]` (`tools/swctl.py:4`). Solo necesita
  Python 3.8.
- Las mismas órdenes se pueden teclear en la consola del juego (F12, `docs/CONTROL.md:49-51`).

Órdenes útiles para probar el cooperativo (`docs/CONTROL.md:64-111`):

| Orden | Uso |
|---|---|
| `state`, `pos`, `version` | ¿está vivo?, ¿dónde está Link? |
| `goto <lugar>` / `teleport x y z` | llevar a Link a un sitio (un solo mapa) |
| `press`, `hold`, `release`, `stick` | mando guionizado |
| `time`, `weather`, `give`, `set god_mode true` | preparar la escena |
| `actors <texto>` | actores vivos por nombre |
| `flag <nombre>` / `setflag <nombre>` | leer o activar un flag (en online, el otro juego lo recibe) |
| `event <flujo>` | lanzar un evento del juego |
| `bloodmoon` | Luna Carmesí |
| `mem read/scan/rescan/path`, `sites`, `cell` | inspección de memoria |
| `shot <fichero.png>` | captura |
| `frames`, `threads`, `prof` | rendimiento |

**En qué hilo corren.** Las órdenes se ejecutan una vez por fotograma presentado, en el hilo que presenta, **no
en el hilo del juego** (`../mapa/03-estado-del-juego.md` §2.3). Leen y escriben memoria mientras el juego corre.
Para leer suele bastar; para cualquier cosa que llame al juego, el modelo correcto es encolar y ejecutar en el
hilo del juego, como `runtime/items.c:420-460`.

El PR registra sus propias órdenes (`online_actors.c:5715-5718`): un buen ejemplo de cómo exponer tu función
nueva para poder probarla con un guion.

## 4. Dos juegos en un PC

Receta de `docs/ONLINE.md:738-745`. Cada instancia necesita su carpeta de guardado (una **copia**, nunca la que
usas), su nombre y su puerto de control; comparten el puerto de sesión.

```powershell
# Consola 1: anfitrión
$env:SAVAGEWIND_SAVE_DIR="C:\tmp\sw-A"; $env:SAVAGEWIND_NO_LAUNCHER="1"; $env:SAVAGEWIND_HIDDEN="1"
$env:SAVAGEWIND_AUDIO_GAIN="0"; $env:SAVAGEWIND_ONLINE_PORT="52530"; $env:SAVAGEWIND_ONLINE="host"
$env:SAVAGEWIND_ONLINE_NAME="Host"; $env:SAVAGEWIND_CONTROL="7777"; .\savagewind.exe

# Consola 2: invitado
$env:SAVAGEWIND_SAVE_DIR="C:\tmp\sw-B"; $env:SAVAGEWIND_NO_LAUNCHER="1"; $env:SAVAGEWIND_HIDDEN="1"
$env:SAVAGEWIND_AUDIO_GAIN="0"; $env:SAVAGEWIND_ONLINE_PORT="52530"; $env:SAVAGEWIND_ONLINE="127.0.0.1"
$env:SAVAGEWIND_ONLINE_NAME="Client"; $env:SAVAGEWIND_CONTROL="7778"; .\savagewind.exe

# Consola 3: conducirlos
tools\venv\Scripts\python.exe tools\swctl.py --port 7777 goto <lugar>
tools\venv\Scripts\python.exe tools\swctl.py --port 7778 actors Online
```

Notas:

- Usa un puerto de sesión distinto del de una partida real (`docs/ONLINE.md:741-742`).
- Dos juegos ocupan unos **9,5 GB** de RAM (`docs/ONLINE.md:743`). Con 15,7 GB: cierra Docker Desktop y el
  navegador.
- El juego puede relanzarse una vez al arrancar; un guion que vigile por PID debe buscar al hijo
  (`docs/ONLINE.md:744-745`).
- Mientras aprendes, quita `SAVAGEWIND_HIDDEN` en una de las dos para **ver** lo que pasa.
- Estos comandos están tomados de la documentación y del mapa 06; **no se han ejecutado en este equipo
  todavía** (eso es F1).

La doc trae recetas concretas que encontraron bugs (`docs/ONLINE.md:760-768`). Imítalas: posición exacta, qué
contar en el log y qué número se espera.

## 5. El bot

`tools/online_bot.c` es un jugador sin juego: se une, camina en círculo alrededor del primer jugador del que
oye, saluda por chat y envía equipo (`online_bot.c:1-5`).

```
cc -O2 -o build/online_bot.exe tools/online_bot.c -lws2_32      # MSYS2 UCRT64 (en Linux, sin -lws2_32)
build/online_bot [host] [puerto] [nombre] [radio] …             # por defecto 127.0.0.1 52520 Bot 4
```

(`online_bot.c:7-9`.) No está en el `Makefile`. Variables `BOT_*` cambian su comportamiento; la lista está en
`../mapa/06-pr294-red-y-sesion.md` §7.2 (`BOT_FREEZE=<s>` deja de enviar con la conexión abierta, útil para la
expulsión por silencio).

Para qué sirve: probar **un** juego con "alguien" en el mundo, y probar el servidor sin juego
(`savagewind.exe --server <puerto>`, `runtime/main.c:440`). Es la pieza que el plan mete en Docker con latencia
y pérdida simuladas.

Límites que debes conocer: duplica `WireState` a mano (`online_bot.c:31-78`), así que cada cambio de protocolo
hay que repetirlo ahí; no envía esqueleto, de modo que la marioneta cae al camino de animaciones por nombre
(`online_actors.c:1930`); su búfer de recepción es menor que el del juego (mapa 06 §5.2).

## 6. Bisecar

**Bisecar** es partir el espacio de sospechosos por la mitad hasta aislar el culpable. Cuatro variantes:

1. **`git bisect`**: funcionaba en el commit X y no en el Y. Git te va dando commits intermedios. Coste real
   aquí: si entre ellos cambian parches o sitios, cada paso exige regenerar (módulo 02 §6). Agrupa.
2. **Por interruptor.** Desactiva subsistemas hasta que el fallo desaparezca. El PR tiene varios
   (`SAVAGEWIND_ONLINE_NO_COLLIDE`, citado en el mapa 07 §2.2). En código nuevo, **da a cada función un
   interruptor de apagado**: es lo que hace posible bisecar mañana.
3. **Por orden dentro de `rt_online_frame`.** Hace muchas cosas en secuencia (muestreo, reglas, botín, flags,
   mundo, runas, enemigos, marionetas). Comentar la mitad de las llamadas localiza en cuál está el cuelgue. F2
   lo vuelve trivial al dejar ese orden como una lista explícita.
4. **Por función generada.** Para fallos del recompilador: así se encontró en este proyecto un bucle de espera
   que el compilador optimizó mal (`docs/RECOMP-LOCALS.md:46`).

Lleva la cuenta por escrito: qué mitad probaste, qué viste. Sin eso repites pasos.

## 7. Leer un crash

Qué imprime el runtime cuando algo va mal (`runtime/dispatch.c:224-261`, `rt_fatal_trace`):

```
host backtrace (guest functions are f_<addr>):     ← pila del proceso; las funciones del juego salen como f_<dirección>
guest: sp=… lr=…                                   ← pila y dirección de retorno del invitado
  x0 …  x1 …  x2 …  x3 …                           ← los 31 registros
guest frames: … … …                                ← cadena de llamadas del juego (siguiendo x29)
  x8 -> …                                          ← registros que apuntan a un objeto cuya primera palabra
                                                      cae en el módulo del juego (su tabla de métodos)
last RomFS files read (newest last):               ← últimos ficheros de datos leídos
```

Cómo leerlo:

1. **¿Quién falló, host o invitado?** Si en lo alto de la pila hay funciones `rt_*`, `online_*` o de
   `online_actors.c`, el fallo está en el runtime (probablemente tuyo). Si hay una `f_<dir>`, es código del
   juego… casi siempre porque **tú le pasaste algo malo** unas líneas más abajo en la pila.
2. **Busca la frontera.** La primera función tuya por debajo de las `f_` es quien hizo la llamada al juego.
   Con `call(...)` de por medio, mira qué `FN_*` era y con qué argumentos.
3. **Mira los registros como argumentos** (módulo 01 §8): `x0` suele ser el objeto. ¿Es 0? ¿Es una dirección
   fuera del rango del juego? `VALID()` (`online_actors.c:63`) es la comprobación de rango que usa el PR.
4. **`no function at <dir> (lr=…)`** (`dispatch.c:157`): se saltó a una dirección que no es inicio de función.
   En el modo online casi siempre es un puntero a función o una tabla de métodos leídos de un objeto ya
   liberado, o una dirección fija de otra versión del juego.
5. **Los últimos ficheros leídos** delatan fallos de datos: si el último es un pack de tu `online_mod`, sospecha
   del generador de actores (`tools/online_mod.py`) y de los tamaños de recursos (mapa 07 §1.3).
6. Lleva las direcciones `0x70…` a Ghidra con `gq.py` (módulo 03 §2).

Causas típicas en este PR, por frecuencia esperada (mapa 07 §6): usar una marioneta que el juego ya borró;
llamar a una función de física sobre un actor sin cuerpo; crear un actor con un nombre que el juego no puede
crear; tocar actores durante una carga o un *game over*.

Para reproducir: guarda **copia del guardado**, posición (`swctl.py pos` da una línea reutilizable con `goto`),
hora y los pasos. Un crash sin repro no se puede dar por arreglado.

## 8. Pruebas diferenciales contra Unicorn, explicado sencillo

**Problema.** ¿Cómo sabes que el C que emite el lifter para una instrucción hace exactamente lo que hace la
CPU real?

**Idea.** Pregunta a un **oráculo**: un emulador de CPU independiente y fiable. **Unicorn** es eso, en forma de
biblioteca. Ejecutas la misma instrucción con el mismo estado inicial por los dos caminos y comparas.

Lo que hace `tools/test_lift.py` (uso en `:3`):

1. Abre un NSO y elige instrucciones distintas repartidas por el módulo (`:200-215`), saltando las que no se
   pueden comparar aisladas (`classify`, `:60-74`).
2. Para cada una genera estados iniciales aleatorios, con valores "malvados" mezclados: 0, −1, el mayor entero,
   potencias de 2 (`SPECIAL`, `:30-31`).
3. **Camino A, Unicorn**: carga registros, ejecuta **una** instrucción y lee registros, flags, `pc` y un hash de
   la memoria de trabajo (`run_unicorn`, `:98-118`).
4. **Camino B, el lifter**: traduce la instrucción a C, compila un pequeño programa con todos los casos y lo
   ejecuta.
5. Compara registro a registro, `sp`, flags, `pc`, memoria y registros vectoriales (`:222-244`) e imprime las
   codificaciones que difieren, desensambladas con Capstone (`:247-253`).

Es **pruebas basadas en propiedades** aplicadas a un traductor: no escribes el resultado esperado, lo calcula el
oráculo. Con `--locals` prueba además la versión optimizada.

Necesita un NSO (tu `exefs/main`) y clang. Sus hermanos funcionan **sin** ficheros del juego:
`tools/test_simd.py` (coma flotante y vectores), `tools/test_locals.py` (funciones aleatorias) y
`tools/test_layout.py` (NSO sintéticos). Comandos en `AGENTS.md:97-99`.

**Qué te llevas para el cooperativo.** El mismo patrón sirve para el protocolo: codificar → decodificar debe
devolver lo mismo (con tolerancia en el esqueleto); el bot y el juego deben coincidir en el tamaño de cada
mensaje. Son las "pruebas de protocolo sin el juego" de F2.

## 9. Qué pruebas existen y cuáles faltan

- Convención del repo: cada prueba es un programa con `assert` que sale con 0; muchas incluyen el `.c` de
  producción directamente; los datos de prueba son siempre sintéticos (`../mapa/05-ui-input-build-ci.md` §5).
- Objetivos `make test-*` hay muchos (`make test-control` prueba la capa de órdenes, `Makefile:745`).
- **Del modo online no hay ninguna prueba automática** (mapa 06 §7.1). Es el hueco que F2 cubre.
- `tools/i18n_check.py` comprueba que los textos estén traducidos; el PR solo tradujo al español (mapa 06
  §6.3).

## 10. Lista de comprobación antes de decir "arreglado"

1. ¿Tengo un repro escrito que fallaba **antes**?
2. ¿Lo he ejecutado **después** y he mirado el resultado en el juego, no solo el log?
3. ¿He probado también el caso contrario (sin sesión, como anfitrión y como invitado)?
4. ¿Sigue funcionando tras una carga, una muerte y un "volver al título"?
5. ¿Está anotado en `../bitacora/` con fecha, versión, hardware y qué se vio?
6. ¿He separado lo verificado de lo supuesto?

Siguiente: [07 · Desarrollo con specs y agentes](07-desarrollo-con-specs-y-agentes.md).
