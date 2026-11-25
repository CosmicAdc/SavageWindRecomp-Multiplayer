# Glosario

Para alguien que programa en Go o Python y llega nuevo a la recompilación de juegos y a las redes de juegos. Cada
entrada: qué es, en una a tres frases, y dónde aparece en este código cuando ayuda. Las rutas son relativas a la raíz
del repositorio; las líneas son las del checkout del PR #294.

Dos avisos de vocabulario antes de empezar:

- **"Host" significa dos cosas distintas.** En recompilación, *host* es el PC y nuestro programa en C, frente al
  *guest* (el juego). En redes, *host* es el jugador que hospeda la partida. En estos documentos decimos **anfitrión
  de máquina** para el primero, **anfitrión** (a secas) para el segundo, e **invitado** para el juego recompilado.
  Al jugador que se une lo llamamos **cliente**.
- **"Frame" también**: ver *fotograma* más abajo.

---

## 1. Recompilación y el juego

**Actor.** Cualquier objeto vivo del mundo del juego: Link, un enemigo, una caja, un arma, un efecto. Cada uno tiene
nombre (`Enemy_Bokoblin_Junior`…), posición y, a veces, física e inteligencia artificial. El PR crea actores nuevos
para representar al otro jugador (`OnlinePlayer`, `OnlineArmor_*`, `OnlineGear_*`). Ver `mapa/07` §1.

**ActorInfo.** Tabla del juego con una ficha por tipo de actor. `tools/online_mod.py` le añade las fichas de los
actores `Online*`.

**AS (animation sequence).** Una animación con nombre que reproduce un actor. Un jugador sin esqueleto recibido cae
de vuelta a reproducir AS (`OA:1971-1985`).

**Código generado.** El C que produce el recompilador a partir del ejecutable del juego. Contiene código de
Nintendo: nunca se sube ni se edita (constitución, reglas 1 y 12).

**Cpu (estructura).** Los registros del procesador ARM64 emulado (`x0..x30`, `sp`, `pc`…) como una estructura de C.
Toda función generada y todo parche recibe un `Cpu *c`. `runtime/cpu.h:7-21`.

**DLC / AoC.** Contenido descargable del juego. Se extrae a carpetas aparte (`aoc1/`, `aoc2/`), sin mezclar con el
RomFS.

**ExeFS.** La parte de un juego de Switch que contiene los ejecutables (`main`, `sdk`, `subsdk*`). Es lo que se
recompila.

**Flag de GameData.** Una variable con nombre que guarda el progreso del juego: "santuario 38 superado", "cofre
abierto", "Kolog encontrado". La mayoría son booleanas. El modo en línea comparte hasta 32 768 de ellas como un mapa
de bits (`online_mod/quests.txt`, `OA:3851-3997`). El guardado (`game_data.sav`) es, en esencia, la lista de estas
variables.

**Fotograma (frame).** Aquí hay tres cosas distintas: una **actualización del juego** (un paso de simulación), un
**fotograma presentado** (una imagen enviada a pantalla) y un **fotograma generado** (una imagen interpolada). No
van siempre a la par. `mapa/02` §3.6.

**GAMELIB_ID.** Huella (hash) de las herramientas que generan el código del juego. Si cambia, la biblioteca del
juego ya compilada deja de valer y cada jugador recompila (unos 15 minutos). Cambia al tocar `tools/patches.py` o
`tools/game_sites.py`. `Makefile:160-164`.

**Gancho (hook).** Un punto donde nuestro código se cuela en la ejecución del juego. El del modo en línea es
`rt_online_frame`, llamado una vez por actualización del juego desde el parche `_vfr_ratio`
(`tools/patches.py:422-426`).

**Ghidra.** Herramienta gratuita de ingeniería inversa: muestra el código máquina como algo parecido a C. Se usa
sobre la copia propia del juego para averiguar qué hace una función. Su salida no se sube al repositorio.

**Guest / invitado.** El juego recompilado y su memoria. "Memoria del invitado" es la memoria que el juego cree
tener en la consola; aquí vive en una ventana fija de direcciones y un puntero del juego se usa tal cual
(`runtime/rt.h:35-36`).

**HashId.** Número único que el mapa del juego asigna a cada objeto colocado (una roca concreta, un cofre concreto).
Sirve para decir "este mismo objeto" en los dos juegos. Se lee del actor en `+0x640`.

**HLE (high-level emulation).** En vez de ejecutar el código original de una función del sistema de la consola, se
sustituye por una función nuestra que hace lo mismo. Se registra con `HLE_FUNC` (`runtime/hle.h`).

**Hilo del juego.** El hilo que ejecuta el bucle principal del juego. Es el único que puede tocar memoria del
invitado o llamar a sus funciones con seguridad. No es el hilo principal del proceso (ese atiende la ventana).
`mapa/02` §1.2.

**Host / anfitrión de máquina.** Nuestro programa en C y el PC donde corre. "Función host" = función nuestra, no del
juego.

**Lifter.** La parte del recompilador que traduce cada instrucción ARM64 a una línea de C (`tools/lifter.py`).

**NSO.** Formato de los ejecutables de Switch. `runtime/loader.c` los carga en memoria.

**NSP.** Archivo contenedor de un juego o actualización de Switch tal como se vuelca de la consola. Hay que
extraerlo a `exefs/` y `romfs/`; el proyecto no lo usa directamente.

**Overlay de RomFS.** Carpeta con la misma forma que el RomFS cuyos archivos sustituyen a los originales al
cargarlos (`SAVAGEWIND_ROMFS_OVERLAY`). El modo en línea usa `online_mod/` como overlay, y solo hay sitio para uno:
quien ya use un mod como overlay se queda sin actores en línea (`runtime/main.c:492-524`, `mapa/07` §1.2).

**Parche.** Sustitución de **una** instrucción del juego por un trozo de C nuestro, definida en
`tools/patches.py`. Debe comprobar con `assert` la instrucción que reemplaza y reproducir su efecto.

**Recompilación estática.** Traducir de una vez todo el ejecutable del juego a C y compilarlo para el PC, en lugar
de emularlo instrucción a instrucción mientras se juega.

**Residente (actor).** Actor cuyos recursos están siempre cargados, así que puede crearse al instante. Solo caben
unos pocos más que los del juego; el número 59 lo cuelga al arrancar. Por eso solo `OnlinePlayer` es residente
(`mapa/07` §1.4).

**RomFS.** La parte de un juego de Switch con los datos: modelos, texturas, mapas, sonido, tablas.

**RSTB.** Tabla del juego que dice cuánta memoria reservar para cada recurso. Si un archivo modificado crece y la
tabla no se actualiza (o se queda corta), el juego se cuelga. Es el punto más frágil de `tools/online_mod.py`
(`mapa/07` §1.3).

**`RT_ON_RESET`.** Macro que registra una función de limpieza. Se ejecuta cuando el juego se reinicia sin cerrar el
programa ("volver a la pantalla de inicio"), con los hilos del juego ya parados y antes de borrar su memoria. Todo
estado que apunte al juego debe limpiarse aquí. `runtime/hle.h:28-44`; ejemplo en `runtime/items.c:337`.

**Runtime.** Nuestro sustituto del sistema operativo de la consola: carga el juego, emula hilos, archivos, entrada,
audio y gráficos. Carpeta `runtime/`.

**SARC / Yaz0 / BYML.** Formatos de archivo de Nintendo: un contenedor, una compresión y un formato de datos tipo
YAML binario. `tools/online_mod.py` los lee y escribe con la biblioteca `oead`.

**Sitio (site).** Una dirección del juego con nombre (`player_info`, `world_mgr`…) que se localiza buscando un
patrón de bytes, no escribiendo la dirección a mano. Se define en `tools/game_sites.py` y se usa como
`g_site.<nombre>` (`runtime/game_sites.h`). Si el patrón no aparece en otra versión del juego, el sitio falta y la
función que depende de él se desactiva sola.

**SVC.** Llamada del juego al núcleo del sistema (crear un hilo, esperar, pedir memoria). El runtime las atiende en
`runtime/kernel.c`.

**Unidad de proceso / handle de actor.** Estructuras internas con las que el juego lleva la cuenta de cada actor
vivo. El PR las vigila (`Watch` + `alive()`, `OA:84-94`) para saber si un actor que creó sigue existiendo antes de
usarlo.

**Versión 1.9.0.** La única versión del juego soportada. El PR depende de 62 direcciones fijas de esa versión
(`mapa/07` §3.1).

---

## 2. Redes de juegos (netcode)

**Ancho de banda.** Bytes por segundo que se envían. Hoy cada jugador sube unos 100 kB/s porque envía su estado
completo 20 veces por segundo, cambie o no (`mapa/06` §2.5).

**Anfitrión (host de la partida).** El jugador que crea la sesión. Su proceso ejecuta el servidor y además se
conecta a él como un cliente más, por `127.0.0.1` (`runtime/online.c:1347-1361`). En nuestro diseño, su mundo es el
que vale.

**Autoridad.** Quién tiene la última palabra sobre un dato. "El anfitrión es autoritativo sobre los enemigos"
significa que, si dos juegos discrepan, gana el del anfitrión. Tabla completa en `contratos/autoridad-mundo.md`.

**Bloqueo de cabeza de línea (head-of-line blocking).** En TCP, si se pierde un paquete, todo lo que llegó después
espera hasta que se reenvía, aunque ya no interese. Para posiciones a 20 Hz esto produce un parón y luego una
ráfaga. Hoy está presente y sin mitigar (`mapa/06` §3).

**Cliente.** El jugador que se une a la sesión de otro. También, el hilo que habla con el servidor (`cl_thread`,
`runtime/online.c:1160`); el anfitrión también tiene uno.

**Cola.** Lista donde un hilo deja datos para que otro los recoja más tarde, sin esperarse entre sí. Es la forma
segura de pasar datos entre el hilo de red y el del juego (`contratos/hilos-y-frame.md`).

**Delta (compresión por diferencias).** Enviar solo lo que cambió desde el último mensaje, en vez de todo. El PR no
lo hace.

**Descubrimiento en red local (LAN discovery).** Que el juego encuentre solo las partidas abiertas en la misma red,
normalmente preguntando por difusión UDP. No existe hoy (CU-04).

**Desincronización (desync).** Cuando los dos juegos dejan de estar de acuerdo sobre el mundo: un enemigo vivo en
uno y muerto en otro, un objeto que "no está aquí".

**DoS (denegación de servicio).** Ataque o fallo que deja el servicio inutilizable para todos. Ejemplo real: un
cliente que deja de leer congela la sesión entera (`mapa/06` H-1).

**ENet.** Biblioteca de red sobre UDP que ofrece canales fiables y no fiables. Es la candidata si se abandona TCP
(ADR-005).

**Fiable / no fiable.** Un envío fiable se reintenta hasta que llega y en orden (bueno para "se abrió este
cofre"). Uno no fiable se manda una vez y, si se pierde, se pierde (bueno para posiciones: la siguiente ya viene de
camino). TCP solo ofrece lo primero.

**Handshake (saludo).** El intercambio inicial al conectar. Aquí: el cliente envía HELLO (versión, nombre,
contraseña) y el servidor responde WELCOME (tu ranura) o REJECT (motivo). `runtime/online.c:235-261`.

**Histéresis.** Exigir un margen antes de cambiar de decisión, para no oscilar. Ejemplo: un enemigo solo cambia de
dueño si el otro jugador está claramente más cerca durante un rato; sin eso se lo pasaban cada pocos segundos
(`OA:2536-2623`).

**Hole punching.** Técnica para que dos PC detrás de routers se conecten sin abrir puertos, con ayuda de un tercer
servidor que los presenta. No está en el plan inicial (ADR-003).

**Instantánea (snapshot).** Una foto del estado de un jugador en un instante. El receptor guarda las últimas 8 por
jugador (`SNAPS`, `runtime/online.c:427`).

**Interpolación.** Dibujar al otro jugador un poco en el pasado (aquí 100 ms, `INTERP_NS`,
`runtime/online.c:45`), mezclando las dos instantáneas que rodean ese momento. Da movimiento suave aunque los
paquetes lleguen a saltos.

**Invitación por código.** Una cadena corta que codifica la dirección de la partida para no dictar una IP. No
existe hoy (CU-04).

**Jitter.** Variación en el tiempo de llegada entre paquetes. Con mucho jitter la interpolación tartamudea.

**Latencia / ping.** Tiempo que tarda un mensaje en ir y volver. Se mide con PING/PONG una vez por segundo
(`runtime/online.c:1292-1295`).

**Lista de permitidos (whitelist).** Lista cerrada de valores aceptables. Todo nombre de actor que llegue por red
debe estar en una antes de usarse (constitución, regla 9).

**Marioneta (puppet).** Copia local de algo que en realidad simula otro juego: el Link del compañero, o un enemigo
cuyo dueño es el otro jugador. No decide nada; solo se coloca y anima según lo que llega (`g_puppet[64]`,
`OA:2640-2650`).

**NAT.** Lo que hace el router de casa: varios aparatos comparten una dirección pública. Por eso nadie de fuera
puede conectarse a tu PC sin más: hay que abrir (redirigir) un puerto, o usar UPnP o una VPN.

**Propiedad de entidad (ownership).** Qué juego simula "de verdad" un objeto concreto en este momento. Cada enemigo
tiene un dueño (una ranura): su juego ejecuta la IA y envía el estado; los demás muestran una marioneta
(`OnlineEnemy.owner`, `runtime/online.h`).

**Protocolo.** El acuerdo sobre qué bytes se envían y qué significan. Hoy es la versión 38 (`PROTOCOL`,
`runtime/online.c:40`). Ver `contratos/protocolo.md`.

**Ranura (slot).** Número de 0 a 7 que el servidor asigna a cada jugador al entrar. Hoy se **supone** que el
anfitrión es la ranura 0 (`OA:2838-2840`), sin que nada lo garantice.

**Reconexión.** Volver a la misma sesión tras una caída sin perder el sitio ni el estado. No existe hoy (CU-05).

**Relé (relay).** Servidor que solo reenvía lo que recibe, sin interpretarlo. El servidor del PR es casi un relé
puro (`mapa/06` §2.4).

**Sesión.** Una partida en línea desde que alguien la crea hasta que la termina.

**SIGPIPE.** Señal con la que Linux y macOS matan por defecto a un proceso que escribe en una conexión que el otro
lado cerró. Hoy no está protegido (`runtime/online.c:143`, `mapa/06` H-8).

**Tasa de envío / tick.** Cuántas veces por segundo se envía o se simula algo. Aquí: estado y esqueleto a 20 Hz,
enemigos a 10 Hz, latencia a 1 Hz, hora cada 2 s.

**TCP.** Protocolo de transporte fiable y ordenado. Sencillo de usar, pero sufre bloqueo de cabeza de línea. Es lo
que usa el PR.

**Trama (framing).** Cómo se separan los mensajes dentro del chorro de bytes de TCP. Aquí: 2 bytes de longitud, 1
de tipo y la carga (`runtime/online.c:150-180`).

**UDP.** Protocolo de transporte sin garantías: los paquetes pueden perderse, duplicarse o desordenarse, pero no se
esperan unos a otros.

**UPnP.** Mecanismo por el que un programa pide al router que abra un puerto automáticamente. No existe hoy
(CU-04).

**Validación.** Comprobar que lo recibido tiene sentido antes de usarlo: tamaños, rangos, números finitos, nombres
permitidos. Ver `contratos/validacion-red.md`.

---

## 3. Método de trabajo

**ADR.** Registro de decisión de arquitectura: un archivo corto con contexto, decisión, alternativas descartadas y
consecuencias. Carpeta `decisiones/`.

**Bitácora.** Diario de pruebas reales. Carpeta `bitacora/`.

**Caso de uso (CU).** Algo que un jugador debe poder hacer, con criterios para saber si funciona. CU-01..CU-14.

**Contrato.** Documento que fija una frontera entre partes del sistema (protocolo, hilos, autoridad, validación).
El código nuevo debe cumplirlo.

**Fase (F0..F5).** Bloque de trabajo con una salida definida.

**SDD.** Desarrollo guiado por especificaciones: primero se escribe qué y por qué, luego cómo, luego las tareas.

**Worktree.** Copia de trabajo adicional del mismo repositorio git en otra carpeta, para que varios agentes
trabajen en ramas distintas sin pisarse.
