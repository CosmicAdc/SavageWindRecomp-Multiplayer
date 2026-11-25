# F4 — Funciones cooperativas: plan

Lee antes `spec.md`. Referencias del checkout `pr294-online` (HEAD `350c5f8`); `OA:n` = `runtime/online_actors.c`.
Tras F2 las funciones de `online_actors.c` viven en módulos `oa_*`: búscalas por nombre.

Este plan ya incorpora las respuestas del dueño del 2026-11-24 (`../../decisiones/ADR-007-…`, `ADR-008-…` y
`ADR-009-…` y `ADR-011-…` (decisiones 29-47), citadas como "ADR-0xx, decisión n"). Lo marcado `[DUEÑO]` no se discute en la implementación; si algo
de aquí contradice un ADR, vale el ADR.

## 0. Idea de construcción

Cada función nueva se parte en dos:

1. **Lógica pura** en un fichero nuevo `runtime/coop_<tema>.c`: sin punteros del juego, sin sockets, con reloj
   inyectado. Se prueba sola con `tests/test_coop_<tema>.c`. Se puede hacer en paralelo.
2. **Integración** pequeña en los ficheros compartidos (módulo `oa_*`, `online.c`, `online_ui.mm`), en serie y con
   dueño único.

Así los modelos menos capaces hacen la parte pura con una prueba clara, y la parte delicada (memoria del juego, hilos)
queda concentrada en pocas tareas.

Hilos (regla que no se rompe):

- Hilo de red: solo sockets y colas.
- Hilo del juego (`rt_online_frame`, y `rt_item_tick` para la bolsa): único sitio donde se llama al juego.
- Hilo de presentación (overlay y pestaña): solo lee estados publicados con mutex; nunca llama al juego ni a la red.
- Patrón de paso entre hilos ya existente: cola con mutex + contador atómico de `runtime/items.c`
  (`items_request :367`, `rt_item_tick :420`, limpieza con `RT_ON_RESET` en `:337`).

## 1. Diseño por elemento

### A. Flags autoritativos del anfitrión (CU-06)

**Hoy.** `quests_update` (OA:3851) corre cada 60 llamadas. `online_mod/quests.txt` lista hasta 32 768 flags
compartidos (25 767 en el mod v37, DOC:991). Cada juego envía un bit por flag (QUEST, 4096 bytes); el cliente
anfitrión guarda el de cada ranura (`runtime/online.c:1065-1070`) y `online_take_quests` hace el OR de todos
(`online.c:664-670`); el anfitrión añade los suyos y reparte la unión (QUESTS, OA:3929-3931); cada juego activa los
que le faltan con el setter del juego (`set_bool_flag`, OA:3794-3804). Resultado: un invitado con una partida
avanzada vuelca todo su progreso en el guardado del anfitrión, sin vuelta atrás.

**Diseño (línea base por invitado).**

1. Al entrar un invitado, el primer QUEST que envía es su **línea base**. El anfitrión la guarda y **no la aplica**.
2. A partir de ahí, por cada QUEST del invitado, el anfitrión calcula `nuevo = actual & ~línea_base & ~ya_aceptado`.
   Solo esos bits son candidatos.
3. Cada candidato pasa un filtro:
   - categoría permitida (tabla por prefijo de nombre de flag, construida al cargar `quests.txt`);
   - tope de N candidatos por envío (propuesta: 16; por encima se descarta el envío entero y se registra: es la
     misma señal que usa el código para detectar una Luna Carmesí con 64 flags, OA:3913-3924);
   - el invitado está en sesión y no "cargando".
4. Los aceptados se activan en el juego del anfitrión y entran en QUESTS, que sigue siendo "los flags del anfitrión".
5. El invitado activa lo que QUESTS trae y él no tiene (como hoy). Sus flags de más se quedan en su perfil
   cooperativo y no viajan.
6. Reconexión (F3): la línea base se conserva junto a la ranura reservada, durante el plazo de reserva de
   **10 minutos** (`[DUEÑO]` ADR-008, decisión 18); un jugador nuevo en la ranura la reinicia (arregla de paso el
   hallazgo T-2 del mapa 06).
7. La lógica de Luna Carmesí (`held_off`, OA:3913-3955) no se toca.

La parte de bits (línea base, candidatos, tope, categorías) es pura: `runtime/coop_flags.c`.

Los flags de encargos y conversaciones con NPC siguen siendo de mundo: lo que uno avanza queda hecho para los dos y
cada uno puede repetir la conversación (`[DUEÑO]` ADR-007, decisión 9). No se separan por jugador.

**Sin ingeniería inversa.** Todo ocurre en `quests_update` y en `online.c`.

### B. Copia de seguridad y perfil cooperativo (CU-06, CU-07)

**Hoy.** La carpeta de guardado se decide una vez por arranque (`sp_session_begin`, `runtime/main.c:589`;
`runtime/services/fs.c:2096`). Existen perfiles (`sp_set_active` en `runtime/services/save_profiles.c:389`, `sp_copy`
en `:428`) y copia automática al arrancar (`save_backup_at_start`, `runtime/save_backup.c:55`), que **no** se hace si
está `SAVAGEWIND_SAVE_DIR` (`:57`). La pestaña Online solo existe dentro del juego (`runtime/ui/menu.mm:2245`,
`:2357`), así que hoy uno se une con la partida que tenga cargada.

**Diseño.**

1. **Identidad de mundo.** El anfitrión genera una vez un identificador aleatorio de 16 bytes por perfil de guardado
   y lo guarda en un fichero propio del fork junto al perfil (`coop-world.txt`, fuera de `save_0`). Lo anuncia en un
   mensaje WORLD al entrar cada invitado. Parte pura: `runtime/coop_world.c` (generar, leer, escribir, nombre de
   perfil `coop-<8 hex>`).
2. **Copia de seguridad.** Función nueva que copia el perfil activo a la carpeta de copias con sello de fecha antes
   de la primera sesión del día, también con `SAVAGEWIND_SAVE_DIR`. Reutiliza `sp_copy` o el código de
   `save_backup.c`.
3. **Perfil cooperativo del invitado.** `[DUEÑO]` (ADR-007, decisiones 1 y 5) Al unirse a un mundo por primera vez
   se crea `coop-<id>` **vacío**: no se copia nada del perfil individual del invitado, y nada de la sesión se
   escribe en él. El perfil vive solo en el PC del invitado. Las siguientes veces se usa el que ya existe. No hace
   falta `sp_copy` ni ningún traspaso de inventario entre partidas.
   Consecuencia por comprobar: un perfil vacío es una partida nueva, y el juego puede arrancar por su introducción,
   lejos del anfitrión **[sin verificar]**. El spike T402 debe decir cómo llega el invitado al mundo en marcha
   (pregunta 5). La vida y la resistencia con las que entra las pone el elemento D2.
4. **Cambio de perfil.** Como la carpeta se fija por arranque, hay dos caminos; el spike T402 decide:
   - (a) reinicio del juego dentro del proceso al aceptar "Unirse" (existe el reinicio en caliente: `teardown`,
     `runtime/main.c:150-170`, y los hilos de red sobreviven, mapa 06 §1.4);
   - (b) unirse desde la pantalla de inicio (la pestaña de ajustes se comparte entre overlay y pantalla de inicio,
     `menu_tabs_draw`, mapa 05 §1.1), eligiendo el perfil antes de arrancar el juego.
5. El anfitrión no cambia de perfil: su mundo es su partida.

### C. Flags idénticos al cargar (opcional)

**Hoy.** El runtime ya sabe reescribir flags del `game_data.sav` en memoria al leerlo, por CRC32 del nombre
(`runtime/services/test_warp.c:57-79`, enganchado en `runtime/services/fs.c:2058-2068`; solo se usa con variables de
prueba).

**Diseño.** Si el spike T403 confirma que sirve para fijar **y borrar** flags booleanos de la lista compartida, al
cargar el perfil cooperativo se aplican los QUESTS recibidos del anfitrión antes de que el juego lea el fichero. El
invitado queda con el mismo mundo que el anfitrión sin efectos a medias. Es una mejora sobre A, no un requisito.

### D. Política de recompensas (CU-07)

**Hoy.** Al activar un flag recibido, `quests_update` entrega objetos (OA:3987-3997):

| Flag recibido | Entrega actual | Función |
|---|---|---|
| `IsGet_PortableUnit` | Piedra Sheikah | `give_item` (OA:3751) |
| runa de la piedra (`slate_rune`) | La runa | `give_item` |
| `FindDungeon_Finish` | Paravela | `give_item` |
| Kolog encontrado lejos | Semilla Kolog | `give_item` |
| `Clear_Dungeon*` | Orbe espiritual | `give_item` |
| `IsGet_Obj_HeroSoul_*` | Habilidad de campeón | `give_item` |
| `Get_HeartUtuwa_*` | Contenedor de corazón | `give_heart` (OA:3763) |

Además, a un invitado recién llegado a un mundo avanzado le caen de golpe todas las entregas pendientes, una a una.

**Diseño.** Tabla de política en `runtime/coop_rewards.c` (pura). La tabla **ya está decidida por el dueño**
(ADR-007, decisiones 2 y 5; ADR-008, decisiones 13, 14 y 19); T415 la escribe en código tal cual:

| Clase | Ejemplos | Durante la sesión | Primera entrada en un mundo avanzado | Marca |
|---|---|---|---|---|
| Capacidad del mundo | Piedra, runas, paravela, mejoras de la piedra | A los dos | Sí (sin ellas no se puede seguir al anfitrión) | `[PROPUESTA]` (los ADR no la tocan; es lo que hace hoy el código) |
| Habilidad de campeón | `IsGet_Obj_HeroSoul_*` | A los dos | **Sí**: las que el anfitrión ya ganó | `[DUEÑO]` ADR-007, decisión 2; ADR-008, decisión 13 |
| Hito de vida y resistencia | Orbe de santuario, contenedor de corazón de Bestia | A los dos, una vez por hito | No se entregan uno a uno: los sustituye "ponerse al día" (elemento D2), que deja al invitado con el **mismo número de contenedores de corazón y de mejoras de resistencia** que el anfitrión | `[DUEÑO]` ADR-007, decisión 2; ADR-008, decisión 13 |
| Coleccionable | Semilla Kolog | Una para cada uno (como hoy) | No | `[DUEÑO]` ADR-011, decisión 30 |
| Contenido de cofre | Lo que hubiera dentro | **No se copia**: es de quien lo abre, que puede soltarlo marcado para el compañero | No | `[DUEÑO]` ADR-008, decisión 14 (se revisa después de probarlo) |
| Rupias | — | No se reparten: son de cada jugador | No | `[DUEÑO]` ADR-008, decisión 19 |

`coop_reward_decide(clase, momento) → dar / no dar`, con tres momentos (durante la sesión, primera entrada, entrada
posterior), y un **libro de entregas** por perfil (`coop-ledger.txt`: una línea por hash de flag ya entregado)
consultado antes de cada `give_item`/`give_heart`. La integración sustituye las siete líneas de OA:3987-3997 por una
llamada a la política.

"Durante la sesión" se distingue de "al entrar" con la línea base del propio jugador: lo que QUESTS trae en el
primer mensaje tras entrar es "al entrar"; lo que aparece después es "durante la sesión". "Primera entrada" es la
del perfil cooperativo recién creado (su libro no tiene todavía la marca de puesta al día de D2).

Entrada posterior (el invitado vuelve a un mundo que avanzó sin él): `[PROPUESTA]`, pregunta P4-11 de `spec.md`.
Por defecto recibe los hitos que se perdió (orbe, contenedor, habilidad) como si hubieran ocurrido durante la
sesión; el libro evita duplicados. No se le vuelve a igualar al anfitrión: ya tiene su propio reparto entre
corazones y resistencia.

### D2. Ponerse al día al entrar (CU-07)

`[DUEÑO]` ADR-007, decisión 2 y ADR-008, decisión 13: quien se une más tarde recibe lo que el anfitrión ya tenía; al
unirse a un mundo avanzado iguala al anfitrión en contenedores de corazón y mejoras de resistencia, y recibe las
habilidades de campeón ya ganadas. Combinado con la decisión 5 (empieza de cero), el invitado entra sin objetos pero
con la misma vida y resistencia.

**Hoy.** No existe. Las entregas uno a uno de D llegan como orbes sin canjear, que no es lo decidido. Lo que hay en
el código y puede servir, todo **[sin verificar para este uso]**:

- `give_heart` (OA:3763) entrega un contenedor de corazón llamando a una función del propio juego.
- Una sonda de prueba que busca el valor `MaxHeartValue` de los datos de partida (OA:3890-3893, solo con
  `SAVAGEWIND_ONLINE_GDT_PROBE`).
- El control lee la resistencia actual del jugador (`runtime/control_game.c:182`). **No hay** nada que lea o fije
  la resistencia máxima ni el número de mejoras.

**Diseño.**

1. El anfitrión lee en el hilo del juego cuántos contenedores de corazón y cuántas mejoras de resistencia tiene, y
   lo anuncia en el mensaje WORLD que ya envía a cada invitado al entrar (dos campos de un byte, definidos en T409).
2. El invitado, **solo en la primera entrada** de su perfil cooperativo en ese mundo, sube los suyos hasta esos
   números. Nunca baja nada.
3. Las habilidades de campeón le llegan por la política D (clase "habilidad de campeón", primera entrada: sí).
4. Los flags de hito que trae el primer QUESTS (santuarios y Bestias ya hechos) se apuntan en el libro como vistos,
   sin entregar orbes ni contenedores: su valor ya está dentro de la igualación.
5. Al terminar se escribe en el libro una marca "puesto al día"; una reconexión o un reinicio no repiten nada.
6. Los dos números se validan al recibirlos (rango del juego; solo se aceptan del anfitrión).
7. Los orbes que el anfitrión tenga sin canjear no se entregan `[PROPUESTA]` (pregunta P4-10 de `spec.md`).

La forma exacta de leer y de fijar corazones y resistencia la decide el primer paso de T442, con una prueba en el
juego. Si para la resistencia no hay una vía que salga del código existente, la tarea se para y se abre una
investigación para el dueño (Ghidra); **no se inventa** un desplazamiento ni un nombre de objeto.

### E. Recompensas de santuario para ambos; cofres para quien los abre

**Hoy.** El flag del cofre se comparte (`<mapa>_TBox_<tipo>_<HashId>`, DOC:704-706): se abre en todos los mundos,
pero el contenido es solo de quien lo abrió. El orbe de santuario ya se entrega por flag (D).

**Decisión.** `[DUEÑO]` ADR-008, decisión 14: el contenido de un cofre es **para quien lo abre**; puede soltarlo
marcado para el compañero (elemento G). Se revisa después de probarlo. Por tanto en F4 **no se construye** la copia
del contenido: T408 y T417 quedan aplazadas hasta esa revisión. El orbe de santuario sí es para los dos, por la
política D. El mensaje REWARD se define igualmente en T409 (reservado, sin uso) para no necesitar otro salto de
versión si el dueño cambia de idea.

**Diseño aplazado (solo si el dueño lo pide tras probar).** El juego de quien abre observa qué entra en su bolsa y
lo anuncia; el del compañero lo recibe por la política D.

- Punto de observación disponible: el parche a la entrada de la función de recogida del juego, que ya existe para
  el multiplicador de recogida (`runtime/items.c:404-415`, `_pouch_item_get` en `tools/patches.py:1194-1198`): ve el
  nombre y el valor de **toda** recogida.
- Pregunta del spike T408: ¿cómo distinguir "esto salió de un cofre" de "lo recogí del suelo"? Candidatos sin
  ingeniería inversa: correlacionar en el tiempo la recogida con el paso a 1 de un flag `*_TBox_*` del juego local
  (ya se muestrean todos cada 2 s; habría que muestrear los de cofre más a menudo), o con el nombre del evento activo
  (`framepace_event_name`).
- Mensaje REWARD (anfitrión → todos, o invitado → anfitrión → todos): hash del flag, nombre del objeto, valor.
- El receptor valida el nombre contra su lista de objetos y entrega por la cola de `items.c`.

### F. Teletransporte al compañero (CU-08)

**Hoy.** `control_tp_request(x, y, z)` (`runtime/control_tp.c:298`, declarado en `runtime/control.h:92`) deja una
petición atómica que la máquina de estados `control_tp_frame` atiende una vez por fotograma presentado; la usa la
pestaña Teleport (`runtime/ui/menu.mm:1748`). Pero `control_tp_frame` **solo se llama en modo desarrollador**
(`runtime/control_game.c:36-40`). Los saltos largos necesitan el sitio opcional `tp_gate`
(`tools/game_sites.py:252`, grupo en `:565`). Tras llegar, el daño de caída se anula 4 s (mapa 03 §1.2). La posición
y el mapa del compañero ya llegan (`OnlinePlayer.pos`, `pose.map`; `same_map`, OA:1484-1486).

**Reglas.** `[DUEÑO]` ADR-008, decisión 16: ir con el compañero está **siempre disponible, también en combate**,
con **30 s de espera** entre usos; invocar al otro **requiere que acepte**. Cada juego solo teletransporta a su
propio Link.

**Diseño.**

1. Lógica pura `runtime/coop_tp.c`: máquina de estados por petición (inactiva → pedida → aceptada → viajando →
   hecha / rechazada / caducada), espera de 30 s, y `coop_tp_can(estado de ambos)` con los motivos de rechazo.
   Dos operaciones:
   - **ir**: el juego del compañero contesta solo, sin preguntarle, con su punto de suelo seguro;
   - **invocar**: al invocado le aparece la petición "<nombre> quiere traerte"; si acepta, es **su** juego el que
     hace el viaje hacia quien le llamó; si rechaza o no responde, no se mueve.
2. "Suelo seguro": cada juego guarda su última posición en la que el AS del cuerpo (canal 0, ya muestreado en
   `sample_pose`, OA:1493) no era de caída, planeo, nado ni escalada, con su hora. La respuesta lleva esa posición,
   no la instantánea.
3. Punto de llegada: 1,5 m detrás del compañero y 0,5 m por encima.
4. Mensajes TP_REQ (dirigido, con la operación ir o invocar; pasa por el anfitrión, que valida mapa y espera) y
   TP_REPLY (punto de llegada, o rechazo + motivo).
5. Integración: una función pública `control_tp_allow_coop()` hace que `control_game_frame` avance
   `control_tp_frame` mientras haya un viaje cooperativo en curso, sin activar el resto del modo desarrollador.
6. Si `g_site.tp_gate` es 0 (grupo opcional ausente), se limita a distancias cortas (menos de 300 unidades en
   horizontal, umbral del propio teletransporte según el mapa 03) y se dice por qué.
7. Bloqueos (solo los técnicos), **separando viajero y destino**. Viajero: evento en curso (`framepace_in_event`),
   muerto, cargando, en pausa, montado, en espera de 30 s. Destino: mapa distinto, muerto, cargando. `[DUEÑO]` (ADR-011,
   decisión 37) un compañero en plena escena o conversación **no** bloquea: se viaja hacia él sin interrumpirle. **Estar en combate contra enemigos no bloquea.** Con un duelo de PvP activo se bloquea `[PROPUESTA]`
   (la decisión 16 habla de combate, no de duelos; confírmalo con el dueño antes de cerrar T418).

### G–I. Enviar objetos (CU-09)

**Hoy.**

- **Recibir**: `items_request(i, count)` (`runtime/items.c:367`) encola; `rt_item_tick` (`:420`) lo entrega en el
  hilo del juego llamando a la función de recogida del propio juego (sitios `item_tick`, `pouch_item_get`,
  `pouch_mgr`, `tools/game_sites.py:437-439`, grupo opcional en `:566`). Solo objetos de la lista (`CanGetPouch`, sin
  `Important`, sin platos cocinados ni rupias), hasta 32 en cola, solo "en juego", y sin modificador de arma.
  El PR tiene además `give_item` con direcciones fijas (OA:3751-3759); se prefiere la vía de `items.c` (por patrón).
- **Soltar**: lo que cae de la bolsa se añade a la lista de botín tras 0,4 s quieto y el otro lo recoge
  (DOC:528-547). Funciona hoy.
- **Quitar del inventario**: no existe para materiales ni comida. Solo `pouch_discard` + `pouch_cleanup` para arma,
  arco y escudo desde el selector rápido (mapa 03 §1.3, solo 1.9.0).

**Reglas.** `[DUEÑO]`

- ADR-007, decisión 10: la primera entrega es **soltar marcado para el compañero**; el envío directo va después.
- ADR-009, decisión 24: **solo materiales y comida**. Armas, arcos y escudos se excluyen hasta que la durabilidad y
  las bonificaciones viajen por la red, lo que exige un cambio de protocolo con su subida de versión.
- ADR-009, decisión 25: no se sueltan para el compañero los objetos que el juego ya protege (objetos clave, Espada
  Maestra y lo que el juego impide soltar). Las rupias no se envían.

**Diseño.**

1. **Primera entrega (T423): "soltar para <nombre>".** El flujo actual de soltar, más una marca en el overlay sobre
   lo soltado y un aviso al compañero, **solo si lo soltado es un material o una comida**. No hay duplicación
   posible porque el objeto pasa por el suelo. No hace falta ingeniería inversa.
2. **Comprobación obligatoria dentro de T423.** Según `docs/ONLINE.md` (DOC:528-547), lo que cae al suelo cerca del
   Link local entra en la lista de botín compartida, y eso incluye `Weapon_*` y `PutRupee*`: hoy un arma soltada
   aparece en el juego del otro, solo con su nombre (mapa 07 §7). Hay que probarlo con dos juegos (espada, arco,
   escudo, rupias) y, si el otro puede recogerlos, **impedirlo** (no añadir a la lista lo que el propio jugador
   suelta de esas clases) o, si impedirlo rompe otra cosa, **documentarlo** como limitación conocida con el visto
   bueno del dueño.
3. **Segunda entrega (envío directo, T420-T422), después y con prioridad baja.** Transacción pura
   `runtime/coop_give.c`, con identificador único por envío:
   `OFERTA(emisor) → ACEPTA|RECHAZA(receptor) → RETIRADO(emisor, tras quitarlo) → ENTREGADO(receptor)`.
   Reglas: el emisor quita **antes** de que el receptor añada; si el emisor no puede quitar, aborta; si la conexión
   cae tras RETIRADO, el emisor reintenta ENTREGAR al reconectar y, pasado el plazo, se lo devuelve a sí mismo.
   Los identificadores ya vistos se ignoran (sin duplicados). Recepción (T421): validar el nombre contra la lista de
   `items.c`, admitir solo materiales y comida, encolar, y confirmar con el resultado de la cuenta de entradas de la
   bolsa (`items.c:352-365`). Retirada (T422): **depende de la investigación T404.**
4. **Armas con durabilidad y modificador: fuera.** Depende de T405 y de ampliar el cable (durabilidad y
   bonificaciones) en una ventana de protocolo posterior. Hasta entonces no se marcan ni se envían armas, arcos ni
   escudos.

### J. Marcadores, pings, color, formas, chat rápido (CU-10)

**Hoy.** El overlay ya dibuja para cada jugador figura, nombre, distancia y flecha en el borde
(`runtime/ui/online_ui.mm:181-304`), proyectando con `project` (`:135`) y la matriz que el hilo de render publica con
`online_camera_set` (`runtime/nvn/vulkan/exec.c:10999-11005`; se descarta si tiene más de 0,5 s, `online_ui.mm:256`).
Colores: 8 fijos por ranura (`:20-23`). El chat solo se escribe desde el menú. La opción de visión de color del
proyecto corrige la imagen del juego al presentar, y **no** altera el menú propio (`runtime/colour_vision.h`): el
overlay debe ser legible por sí mismo.

**Diseño.**

1. `runtime/coop_palette.c` (puro): 8 colores de una paleta pensada para daltonismo y 8 formas (círculo, cuadrado,
   triángulo, rombo, estrella, cruz, hexágono, anillo); función de contraste mínimo contra fondo claro y oscuro.
2. `[DUEÑO]` (ADR-009, decisión 26) Color y forma se asignan **por orden de entrada** (la ranura: el primero recibe
   el primer color y la primera forma, y así sucesivamente) y cada jugador puede cambiarlos en Opciones; viajan en
   STATE (2 bytes). Si dos coinciden tras un cambio, el de ranura mayor recibe la siguiente forma libre.
3. Ping: mensaje MARK (posición, tipo, emisor), 8 s de vida, máximo 4 por jugador, límite de 2 por segundo.
   Colocación sin ingeniería inversa: (a) en mi posición; (b) en el enemigo o jugador más cercano al centro de la
   pantalla (se proyectan los que ya se conocen y se elige el más próximo al centro). "Donde miro" sobre el terreno
   exige un trazado de rayo contra el mundo: fuera de F4.
4. Chat rápido: mensaje QCHAT con un número de frase (0-7); cada cliente lo pinta con `tr()` de su idioma.
   `[DUEÑO]` (ADR-009, decisión 28) Las ocho frases, en este orden: "Ven aquí", "Espera", "Necesito ayuda",
   "Gracias", "Sí", "No", "Vamos", "Mira esto".
5. Avisos: una cola de avisos en pantalla alimentada desde el hilo del juego (por ejemplo "tu compañero está en un
   pedestal: A en espera", que hoy solo va al registro, OA:5999).
6. Vida del compañero: `[DUEÑO]` (ADR-009, decisión 27) **siempre visible, pequeña**, y destacada (otra forma y
   texto, no solo otro color) por debajo del 25 %. STATE añade la vida en cuartos de corazón (el juego local ya lee
   la suya en `player + 0x848`, DOC:644) y la vida máxima, si se puede leer. El máximo de corazones no se conoce
   todavía por una vía fiable **[sin verificar]**: lo establece el primer paso de T442 (que lo necesita para ponerse
   al día). Mientras no se conozca, el campo viaja a 0 y se destaca por debajo de un corazón.
7. Dibujo: todo dentro de `online_overlay_draw` o en un fichero nuevo `runtime/ui/coop_hud.mm` llamado desde ahí.
   Modelos a copiar: `perf_overlay_draw` (`runtime/ui/menu.mm:1997`) y `photo_overlay_draw` (`:1935`). Como el
   overlay de Online ya está en el camino de dibujo (`menu.mm:2594`), no hace falta tocar `menu_draws()`.
8. Entrada: atajos nuevos (ping, chat rápido, aceptar/rechazar) como acciones del sistema de asignaciones
   (`runtime/hle/bindings.c`, `docs/ACTIONS.md`) **[sin verificar el detalle de cómo se añade una acción: la tarea
   T436 empieza leyéndolo]**. Qué botón lanza el ping y abre la rueda es lo único de CU-10 que sigue abierto
   (Q-10-5; ADR-009, "Lo único que sigue abierto"): T436 revisa `docs/controls.md` y `runtime/input_bind.h`, anota
   qué botones y teclas están libres y propone una combinación al dueño.

### K–L. Revivir (CU-11)

**Hoy.** El parche `has_fairy` hace que "¿tiene un hada?" responda sí mientras hay sesión y la regla `revive` del
anfitrión está activa (`rt_online_has_fairy`, OA:2825-2831). Es una reanimación instantánea; el compañero no
interviene. La regla se refresca dentro de `players_collide`, después de un `return` temprano (OA:2837-2839): con
colisiones apagadas o durante un evento no se actualiza.

**Reglas.** `[DUEÑO]` ADR-007, decisión 7: en la primera entrega revivir es **automático**, como hace hoy el
código; el rescate manual queda planificado para después. ADR-008, decisión 15: si caen los dos a la vez, **fin de
partida normal** y los dos recargan su último guardado.

**Diseño en dos escalones.**

1. **Primera entrega, sin ingeniería inversa (T431).** `rt_online_has_fairy` consulta la política pura
   `runtime/coop_revive.c`: responde sí mientras la regla esté activa y haya al menos un compañero en pie (vivo y no
   caído en ese mismo momento). Si no lo hay, deja que el juego siga su curso (fin de partida normal). **No se añade
   límite de distancia ni de mapa**: sería una restricción que el dueño no ha pedido. De paso, mover el refresco de
   reglas fuera de `players_collide`.
2. **Entrega posterior, prioridad baja: rescate manual con estado caído (T432), tras la investigación T406.** El
   caído espera en el suelo con cuenta atrás; el compañero lo levanta manteniendo un botón cerca. Mensajes DOWN y
   REVIVE; el anfitrión valida distancia y tiempo. No bloquea el cierre de F4.

### M. PvP por acuerdo, daño validado (CU-12)

**Hoy.** `g_online.pvp` por jugador, apagado por defecto. Al empezar un golpe (la estela del arma), el atacante
elige al jugador más cercano en alcance y delante, y publica en su STATE `pvp_seq`, `pvp_target`, `pvp_damage` = poder
de ataque de su arma según `power.txt` (OA:2874-2905). La víctima aplica el daño menos su defensa, o lo bloquea con
el escudo (OA:2907-2928). Solo se descarta `pvp_damage <= 0`; cualquier valor positivo se acepta. Bombas ajenas a
menos de 4 m: daño fijo y derribo (OA:2929-2941).

**Diseño.**

1. **Acuerdo.** `runtime/coop_pvp.c` (puro): estado de sesión `apagado → propuesto por X → activo`. Mensaje PVP_CTL
   (proponer, aceptar, apagar). Solo el anfitrión publica el estado resultante (como las reglas `collide`/`revive`).
   Apagar es inmediato y unilateral. `[DUEÑO]` (ADR-008, decisión 17) El anfitrión tiene además una regla de sesión
   "PvP permitido": si la apaga, toda propuesta se rechaza y un duelo en curso termina.
2. **Golpe validado.** El atacante envía PVP_HIT (objetivo, secuencia) **solo al anfitrión**. El juego del anfitrión
   comprueba: PvP activo; ambos vivos, mismo mapa, no cargando; distancia entre las posiciones que el anfitrión
   conoce de los dos ≤ alcance + margen por latencia; ritmo ≤ 4 golpes por segundo; y calcula él mismo el daño con
   `power.txt` a partir del arma que el atacante tiene equipada según su último EQUIP. Emite PVP_DMG (objetivo,
   daño), que el servidor solo acepta del anfitrión.
3. La víctima aplica PVP_DMG con su propia defensa y bloqueo, como hoy.
4. Los campos `pvp_*` de STATE dejan de usarse para daño.
5. Indicador: icono con forma junto al nombre de quien tiene el PvP activo.
6. Alcance `[DUEÑO]` (ADR-008, decisión 17): **cuerpo a cuerpo y bombas**. Flechas y runas quedan fuera de la primera
   entrega. El alcance de las armas no cambia; lo que cambia es quién valida el daño (el anfitrión, no el atacante).

### N. Amenaza de los enemigos (CU-15)

**Hoy.** El dueño de cada enemigo es el más cercano (con histéresis de 40 m / 10 m, 15 m durante 5 s y 10 s sin
cambiar) o el último que le golpeó desde menos de 80 m, con 6 s de espera (OA:2520-2623). El HIT viaja sin origen
(`online.c:281-282`).

**Reglas.** `[DUEÑO]` ADR-012, decisión 2: el enemigo pasa a quien más daño le hace, con el más cercano como
desempate, sin perder los 80 m ni los 10 s; lo decide el anfitrión con los golpes que ya ve.

**Diseño.** Lógica pura `runtime/coop_threat.c` (T443): amenaza por ranura y enemigo = daño efectivo, con semivida de
8 s; el daño del dueño es la vida que baja y ningún HIT explica. `pick` devuelve cercanía (sin líder claro: regla de
hoy), mantener o pasar a otro (líder claro: 1,5 veces la segunda amenaza). Integración en `oa_enemies.c` (T444), que
sustituye el traspaso "al último que golpea"; los traspasos por amenaza respetan `hold`. Protocolo: HIT con ranura de
origen (T409). Detalle y preguntas Q-15-1 a Q-15-4 en `../../casos-de-uso/CU-15-amenaza-de-los-enemigos.md`.

## 2. Ingeniería inversa necesaria (tareas de investigación para el dueño)

Cada una se hace con Ghidra sobre la copia del dueño (`tools/ghidra/README.md`, `tools/ghidra/gq.py`). La respuesta
se entrega como **sitio por patrón** en `tools/game_sites.py` (receta del mapa 01 §4), nunca como dirección suelta en
un `.c`, y con una prueba en el juego anotada en bitácora.

Las tres son de **prioridad baja** y ninguna bloquea el cierre de F4: T404 sirve al envío directo (segunda entrega,
ADR-007, decisión 10), T405 a las armas (fuera hasta nuevo aviso, ADR-009, decisión 24) y T406 al rescate manual
(planificado para después, ADR-007, decisión 7). No se eliminan.

| Tarea | Pregunta | Forma de la respuesta |
|---|---|---|
| T404 | ¿Qué función del juego resta N unidades de un material o comida de la bolsa (la que usan vender, cocinar o soltar desde el menú) y con qué argumentos? ¿Qué tipos de entrada de bolsa corresponden a material, comida, armadura y objeto clave? | Nombre de sitio nuevo, clase de sitio, firma en palabras (qué es cada argumento), precondiciones (hilo, cerrojo de la bolsa), prueba: quitar 1 manzana y verla desaparecer del menú |
| T405 | ¿Qué significa el cuarto argumento de la función de recogida (hoy se pasa 0, mapa 03 §1.3) y cómo se lee la durabilidad y el modificador de un arma en la bolsa? | Descripción de los dos valores y de dónde se leen; prueba: dar un arma con durabilidad a la mitad |
| T406 | Entre "la vida llega a 0" y "pantalla de fin de partida", ¿dónde decide el juego morir o revivir, y se puede retrasar esa decisión N segundos manteniendo a Link en el suelo? | Sitio(s), qué instrucción se envuelve, qué ocurre con la cámara y la música; prueba: Link queda caído 10 s y luego (a) revive o (b) sigue a fin de partida, sin crash |

Todas cambian `tools/patches.py` o `tools/game_sites.py` → cambian `GAMELIB_ID` → los dos jugadores recompilan el
juego (~15 min). **Se agrupan en una sola entrega.**

## 3. Carriles y propiedad de ficheros

| Carril | Ficheros (dueño único) | Tareas |
|---|---|---|
| **Protocolo** | `runtime/online.h`, `PROTOCOL`, cabecera compartida con el bot, tabla de autoridad de F3 | T409 |
| **Lógica pura** (paralelo) | `runtime/coop_flags.*`, `coop_world.*`, `coop_rewards.*`, `coop_tp.*`, `coop_give.*`, `coop_palette.*`, `coop_revive.*`, `coop_pvp.*`, `coop_threat.*` + `tests/test_coop_*.c` | T410, T412, T415, T418, T424, T430, T433, T443 (T420: segunda entrega) |
| **Mundo** (serie, en la cadena única de §5) | módulo de flags de `online_actors` (`oa_flags` / `oa_gamedata`), guardados (`runtime/save_backup.c`, `runtime/services/save_profiles.*`), parte de `online.c` que guarda los QUEST | T411, T413, T414, T416, T442 (T417: aplazada) |
| **Reglas** (serie) | módulo de reglas (`oa_rules`), `runtime/control_game.c`, `runtime/control_tp.c`, `runtime/items.c` | T419, T423, T431, T434 (segunda entrega y posteriores: T421, T422, T432) |
| **UI** (serie, en la cadena única de §5) | `runtime/ui/online_ui.mm`, `runtime/ui/coop_hud.mm` (nuevo) | T425-T429, T435 |
| **Enemigos** (fuera de la cadena: ninguna otra tarea de F4 toca su fichero) | `runtime/oa_enemies.c`, `tools/coop/threat_run.sh` | T444 |
| **Entrada** | `runtime/hle/input.c`, `runtime/hle/bindings.c` | T436 |
| **i18n** | `runtime/ui/i18n/*.inc` | T437 |
| **Parches** (una entrega) | `tools/patches.py`, `tools/game_sites.py`, `runtime/game_sites.h` | lo que salga de T404-T406 |
| **Integrador** | orquestador de fotograma (`oa_frame`), `Makefile`, `runtime/launch_flags_table.c` (generado), `runtime/ui/menu.mm` y `runtime/settings.*` solo si hiciera falta | T439 |

**Dueño único de los ficheros compartidos.** Los tres carriles (Mundo, Reglas y UI) editan los mismos ficheros:
`runtime/online.c`, `runtime/ui/coop_hud.mm`, `runtime/ui/online_ui.mm` y `textos-nuevos.txt`. Por eso **no corren en
paralelo entre sí**: las tareas que los tocan van en la cadena única de §5 (una a la vez, aunque su `depende de:` solo
exija una parte). Solo la lógica pura (`coop_*.c` y sus pruebas) y las investigaciones corren en paralelo.
`runtime/online.h` lo edita solo T409.

Los ajustes nuevos van a `online.ini` (como en F3), no a `savagewind.ini`: así `runtime/settings.*` no se toca.

## 4. Coordinación de la versión de protocolo

- **Un salto de versión para toda F4**, al principio (T409): todos los mensajes y campos de STATE de esta fase se
  definen de una vez, con manejadores vacíos.
- Mensajes nuevos: WORLD, REWARD, TP_REQ, TP_REPLY, GIVE, MARK, QCHAT, PVP_CTL, PVP_HIT, PVP_DMG, DOWN, REVIVE.
  Campos nuevos de STATE: vida, vida máxima, color, forma, indicadores (caído, PvP activo).
- HIT pasa a llevar la ranura de origen, que pone el servidor (CU-15, ADR-012): sin mensajes nuevos.
- WORLD lleva, además del identificador de mundo, los contenedores de corazón y las mejoras de resistencia del
  anfitrión (elemento D2). TP_REQ lleva la operación (ir o invocar). Las reglas de sesión del anfitrión ganan
  "PvP permitido".
- REWARD, GIVE, DOWN y REVIVE se definen ya, con manejadores vacíos, aunque sus funciones (cofre para los dos, envío
  directo, rescate manual) no entran en la primera entrega: así no hace falta otro salto de versión para ellas.
- **Lo que no cabe en este salto:** la durabilidad y las bonificaciones de un arma. Enviar armas exige su propio
  cambio de protocolo con subida de versión, después de T405 (ADR-009, decisión 24).
- Mensajes **dirigidos** (a una ranura concreta): el servidor reenvía solo a esa ranura. T409 lo añade a la tabla de
  autoridad de F3: PVP_DMG, REWARD y WORLD solo del anfitrión; PVP_HIT solo hacia el anfitrión.
- Si una investigación (T404-T406) obliga a cambiar el cable más tarde, lo agrupa el dueño del protocolo en una
  segunda ventana única.
- Cada cambio actualiza a la vez la cabecera compartida, el bot, las aserciones de tamaño y
  `specs/contratos/protocolo.md`.

## 5. Orden recomendado

1. Decisiones e investigación en marcha desde el primer día: T401 (dueño; reducida a P4-2 y P4-8), T402, T403, T407
   (agentes). Pueden empezar durante F3 porque solo escriben bitácora; todo lo que edite `runtime/online.c` espera al
   bloque 3 de F3 (T413) y T409 a F3 completa.
2. T409 (protocolo), y **justo después** T438 (modos de bot y escenarios de F4).
3. En paralelo, toda la lógica pura: T410, T412, T415, T418, T424, T430, T433. T415, T418 y T430 ya no esperan
   ninguna decisión del dueño.
4. **Cadena única** (serie; los tres carriles comparten `online.c`, `coop_hud.mm` y `online_ui.mm`):
   T413 → T411 → T414 → T416 → T436 → T425 → T426 → T427 → T428 → T429 → T442 → T419 → T423 → T431 → T434 → T435.
   Dependencias que la justifican: T436 antes de T426 y T427 (sus acciones); T423 después de T426 (cola de MARK) y
   T428; T419, T431 y T442 después de T428 (avisos por identificador).
5. Los carriles Mundo, Reglas y UI son agrupaciones de esa cadena, no hilos paralelos. T444 (amenaza) va aparte,
   tras T409, T438 y T443, porque solo toca `oa_enemies.c`; T439 la espera.
6. Cierre: T437 → T439 → T440 → T441 (T438 ya se hizo tras T409).
7. Después del cierre, por este orden de prioridad y solo si el dueño lo pide: envío directo (T404 → T420 → T421 →
   T422), revisión de cofres (T408 → T417), rescate manual (T406 → T432), armas (T405 y su ventana de protocolo).
