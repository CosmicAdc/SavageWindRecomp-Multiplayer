# 05 · Anatomía del modo online: dos recorridos guiados

Objetivo: seguir con el dedo, función a función, (A) el estado y el esqueleto de un jugador desde que se
muestrean hasta que mueven los huesos de su marioneta en el otro PC, y (B) dos eventos: un flag de misión y un
objeto soltado. En cada paso se dice **qué hilo** lo ejecuta.

Abre `runtime/online.c`, `runtime/online.h` y `runtime/online_actors.c` mientras lees. Es el ejercicio que el
plan pide para F1.

---

## 1. Los hilos, antes de empezar

| Abrev. | Hilo | Qué es | Evidencia |
|---|---|---|---|
| **J** | hilo del juego | el hilo principal *del invitado*: el bucle de fotogramas del juego. No es el hilo principal del proceso | `rt_online_frame`, `online_actors.c:5721`; comentario `online_actors.c:1-2` |
| **C** | hilo cliente de red | `cl_thread` | `online.c:1160` |
| **S** | hilo servidor | `srv_thread` → `srv_loop`; solo existe en el PC del anfitrión | `online.c:343`, `407-410` |
| **U** | hilo de interfaz | dibuja la pestaña Online y el overlay; arranca y para sesiones | `runtime/ui/menu.mm:2357`, `2594` |
| **R** | hilo de render | publica la cámara para el overlay | `runtime/nvn/vulkan/exec.c:11005` |

Dos cerrojos lo protegen todo: `g.lock` (estado del cliente, `online.c:443`, `485`) y `g_pose_lock` (pose,
equipo y enemigos locales, `online.c:811`).

Notación: PC-A es quien envía (su Link se mueve), PC-B quien recibe. Si A es el anfitrión, S vive en PC-A; si
no, en el del anfitrión.

## 2. Recorrido A: de un Link que camina a una marioneta que camina

```
 PC-A                                                             PC-B
 ┌───────────── J ─────────────┐                                  ┌───────────── J ──────────────┐
 │ rt_online_frame             │                                  │ rt_online_frame              │
 │  sample_pose ─► g_pose      │                                  │  online_players() ◄─ g.r[]   │
 │  sample_skeleton ─► g_skel  │                                  │  FN_SET_MTX (posición)       │
 └──────────────┬──────────────┘                                  │  pose_link:                  │
        buzones │ (mutex)                                         │   online_skeleton() ◄─ sk[]  │
 ┌──────────────▼──── C ───────┐      ┌───── S (anfitrión) ────┐  │   FN_BONE_LOCAL_SET × hueso  │
 │ cada 50 ms:                 │ TCP  │ srv_message:           │  └──────────────▲───────────────┘
 │  sample_local ─► WireState  ├─────►│  guarda el último      │         buzones │ (mutex)
 │  send M_STATE, M_SKEL       │      │  srv_broadcast a los   │  ┌──────────────┴──── C ────────┐
 └─────────────────────────────┘      │  demás, con la ranura  ├─►│ cl_message:                  │
                                      └────────────────────────┘  │  M_STATE ─► remote_snap      │
                                                                  │  M_SKEL  ─► skel_decode      │
                                                                  └──────────────────────────────┘
```

### Paso 1 · [J, PC-A] El gancho por fotograma

El juego, en su bucle, ejecuta el C del parche `_vfr_ratio` (`tools/patches.py:422-427`), que llama a
`rt_online_frame(c)`. Ocurre **antes** de que el juego actualice sus actores (`online_actors.c:1-2`), dato que
importa en el paso 9.

`rt_online_frame` (`online_actors.c:5721-5766`):

- lee el puntero al objeto de información del jugador desde un **sitio**: `g_site.player_info` (`:5722`);
- si el modo online no tiene sus actores disponibles, sale (`:5726`);
- copia la posición de Link (`:5729`) y decide si hay mundo (`:5730-5731`);
- detecta cargas: sin Link, o un salto de más de 100 m entre fotogramas (`:5748-5761`);
- y por fin muestrea: `sample_pose(c, pinfo), sample_skeleton(c, pinfo)` (`:5766`). No lo hace con el juego en
  pausa, salvo con la regla "menús sin pausa".

### Paso 2 · [J, PC-A] La pose

`sample_pose` (`online_actors.c:1493`) rellena un `OnlinePose` (`online.h:78-128`): qué animaciones reproduce
Link, en qué hueso va cada pieza de equipo, runas, mapa, clima, eventos de efectos… y termina con
`online_set_local_pose(&pose)` (`online_actors.c:1833`).

`online_set_local_pose` (`online.c:815-820`) es un buzón de "último valor": toma `g_pose_lock`, copia la
estructura entera a `g_pose`, anota la hora y suelta. El hilo J no espera a nadie.

### Paso 3 · [J, PC-A] El esqueleto

`sample_skeleton` (`online_actors.c:1871-1893`):

1. localiza la unidad de modelo que contiene los huesos de Link (`skel_unit`, `:1874`) y cuántos son;
2. añade los huesos del arco detrás, para que se vea la cuerda tensarse (`:1877-1883`);
3. por cada hueso **llama a una función del juego**: `call(c, FN_BONE_LOCAL_GET, …)` (`:1887`), que devuelve la
   matriz local del hueso;
4. convierte la matriz en cuaternión + traslación (`:1889-1890`);
5. entrega todo con `online_set_local_skeleton` (`:1892`).

Fíjate en `call` (`online_actors.c:232-259`): es la envoltura que guarda el `Cpu`, pone argumentos en
`x0…x6`, baja la pila, invoca `guest_call` y restaura (módulo 01 §8). **Solo J puede hacer esto.**

`online_set_local_skeleton` (`online.c:576-587`) ya **comprime** aquí, en el hilo J: `skel_encode` deja 12
bytes por hueso (`online.c:531-554`) en un búfer local y luego, bajo `g.lock`, lo copia a `g_skel`.

### Paso 4 · [C, PC-A] Empaquetar y enviar

`cl_thread` da una vuelta cada 10 ms como mucho (`online.c:1187-1188`). Cuando toca (`now >= next_send`,
20 veces por segundo, `:1279-1280`):

1. `sample_local(&w)` (`online.c:823-885`) construye el `WireState`:
   - lee **directamente de la memoria del juego** la posición de Link (`:829-831`), sin pasar por J. Es una
     lectura sin sincronizar (hallazgo T-7 del mapa 06);
   - descarta valores no finitos o absurdos (`:832-834`);
   - deduce rumbo y una animación genérica a partir del movimiento (`:836-840`, `motion_anim` `:513-528`);
   - bajo `g_pose_lock`, si la pose tiene menos de medio segundo, copia sus campos uno a uno (`:845-883`).
2. Si no hay Link, marca `ONLINE_F_LOADING` (`:1282`).
3. `send_msg(s, M_STATE, &w, sizeof w, …)` (`:1284`).
4. Bajo `g.lock` copia el esqueleto codificado si es reciente y lo envía como `M_SKEL` (`:1285-1290`).

`send_msg` (`online.c:150-158`) pone la cabecera (longitud en 2 bytes, tipo en 1) y llama a `send_all`
(`:140-148`), **bloqueante**.

### Paso 5 · [S, anfitrión] El relé

`srv_loop` despierta porque el socket de A tiene datos (`online.c:355`, `383-387`), acumula bytes
(`recv_some`, `:164-169`) y corta mensajes completos (`:393`).

`srv_message` (`online.c:232-312`):

- `M_STATE` (`:263-268`): exige longitud suficiente, guarda una copia como "último estado de este jugador"
  (para quien entre después, `:255`) y lo reenvía a **todos los demás** anteponiendo un byte con la ranura del
  emisor (`srv_broadcast(sv, i, M_STATE, &id, 1, …)`).
- `M_SKEL` (`:275-277`): lo reenvía igual si el tamaño es razonable. No lo guarda.

El servidor no interpreta el contenido. Y recuerda: `srv_broadcast` envía con el mismo `send` bloqueante
(`:213-216`).

### Paso 6 · [C, PC-B] Recibir y guardar

`cl_thread` de B recibe (`online.c:1188-1195`) y llama a `cl_message` (`:967-1125`), que toma `g.lock` para
todo el `switch`.

- `M_STATE` (`:1001-1007`): comprueba longitud, que la ranura sea `< ONLINE_MAX` y que ese jugador exista;
  copia a un `WireState` local y llama a `remote_snap`.
- `remote_snap` (`:907-964`) escribe en el anillo de 8 instantáneas del jugador:
  - sella la hora **local** de llegada (`:910`);
  - copia campo a campo **saneando**: termina en cero cada cadena, recorta contadores, limita `anim`
    (`:912-959`);
  - no comprueba que los `float` sean finitos (hallazgo H-6: arreglo de F1);
  - avanza la cabeza del anillo (`:961-962`).
- `M_SKEL` (`:1008-1020`): valida número de huesos y longitud (`:1012`), decodifica con `skel_decode` al anillo
  de 4 esqueletos y sella la hora.

### Paso 7 · [J, PC-B] Pedir "cómo estaban hace 100 ms"

En su siguiente fotograma, `rt_online_frame` de B llama a `online_players(pl)` (`online_actors.c:6011-6012`).

`online_players` (`online.c:1387-1432`), bajo `g.lock`, para cada jugador:

- si su última instantánea tiene más de 3 s, lo marca cargando (`:1402-1405`);
- busca las dos instantáneas que rodean `ahora − 100 ms` (`:1406-1411`);
- interpola posición y rumbo (`:1412-1418`); la pose se toma de la más nueva (`:1421`).

Devuelve una **copia** de las 8 ranuras. J trabaja sobre la copia, sin cerrojos.

### Paso 8 · [J, PC-B] Decidir si hay marioneta y colocarla

Bucle de jugadores (`online_actors.c:6013-6058`):

- ¿se quiere un Link para él? Solo si existe, no es el local, no está cargando, está en el mismo mapa (`:6014`)
  y a menos de 400 m en horizontal (`:6017-6018`);
- si el juego borró la marioneta, se olvida todo y se espera (`:6036-6041`);
- si no hay actor, se crea (`create(c, actor_name())`, `:6044`) y se vigila (`watch`, `:6049`);
- se construye una matriz de posición con el rumbo y se llama a `FN_SET_MTX` (`:6054-6056`);
- `pose_link(c, i, &pl[i])` (`:6058`).

### Paso 9 · [J, PC-B] Escribir los huesos

`pose_link` (`online_actors.c:1901-1942`):

1. `online_skeleton(i, &n, &nbow, q, t)` (`:1908`) → en `online.c:775-808`, bajo `g.lock`: si el esqueleto más
   nuevo tiene menos de medio segundo, interpola entre los dos que rodean `ahora − 100 ms` con nlerp.
2. Si no llegó nada nuevo (pausa, menú), reutiliza el último (`:1907-1911`).
3. Por cada hueso: cuaternión → matriz (`quat_to_mtx`), traslación, y
   `call(c, FN_BONE_LOCAL_SET, …)` (`:1919-1927`). **Escribe el hueso en el modelo de la marioneta.**
4. Si nunca llegó esqueleto en 60 fotogramas (el bot, un cliente antiguo), cae a reproducir animaciones por
   nombre (`animate`, `:1930`).

¿Por qué funciona? La marioneta no reproduce ninguna animación, así que nada pisa esos huesos antes de que el
juego calcule las matrices finales (`online_actors.c:1895-1897`). Y como el gancho corre antes de la
actualización de actores (paso 1), lo escrito se usa en este mismo fotograma.

Después vienen la armadura, que copia los mismos huesos (`pose_armor`, `:1945`), y el equipo.

### Resumen de latencia

| Tramo | Retraso típico |
|---|---|
| J → buzón → C (PC-A) | hasta 50 ms (el tick de envío) |
| red A → S → B | el ping |
| colchón de interpolación | 100 ms fijos |
| C → buzón → J (PC-B) | hasta un fotograma |

Ves al otro unos 150-200 ms tarde en LAN. Es normal en este diseño.

## 3. Recorrido B1: un flag de misión compartido

Caso: el invitado (PC-B) completa algo y el anfitrión (PC-A) lo recibe. Todo cuelga de `quests_update`
(`online_actors.c:3851-4000`), llamada desde `rt_online_frame` (`:5954`). Trabaja de verdad una vez cada 60
llamadas (`:3880`).

1. **[J, ambos] Carga perezosa de la lista.** La primera vez lee `quests.txt` de la carpeta del overlay
   (`:3859-3877`): un nombre de flag por línea, hasta 32 768. Calcula un hash por nombre. La posición en la
   lista es el número de bit.
2. **[J, PC-B] Leer mis flags.** Para cada nombre, `bool_flag(hash)` devuelve un puntero al valor en la memoria
   del juego; si está a 1 se enciende el bit en `mine` (`:3909-3912`).
3. **[J, PC-B] Publicar.** Si cambió, o cada cierto tiempo, `online_send_quests(mine)` (`:3936`) → copia a
   `g.quest_out` y marca `quest_out_new` (`online.c:658-662`).
4. **[C, PC-B] Enviar.** En su vuelta, si no es anfitrión y hay novedad, envía 4096 bytes como `M_QUEST`
   (`online.c:1222-1230`).
5. **[S] Relé.** `M_QUEST` se reenvía con la ranura delante, **sin comprobar longitud** (`online.c:291-293`).
6. **[C, PC-A] Guardar.** `cl_message`, solo si `g.hosting`: copia al mapa de ese jugador,
   `g.quest_of[ranura]` (`online.c:1065-1070`).
7. **[J, PC-A] Unir.** `online_take_quests(all)` hace el OR de los mapas de todos los jugadores presentes
   (`online.c:664-674`); el anfitrión añade los suyos (`online_actors.c:3929-3930`) y publica la unión con
   `online_set_quests` (`:3931`).
8. **[C, PC-A → S → C, PC-B]** viaja como `M_QUESTS` (`online.c:1227-1229`, relé crudo `:294-298`); el invitado
   la guarda si no es anfitrión (`:1071-1073`).
9. **[J, cada juego] Aplicar lo que falta.** Para cada bit que la unión tiene y yo no (`:3953-3954`):
   - se activa con la función del propio juego: `set_bool_flag` (`:3794-3804`) busca el identificador del flag
     y llama a su *setter*; si falla, se escribe el bit en memoria (`:3986`);
   - **ciertos flags entregan objetos**: paravela, orbe de santuario, habilidad de campeón, contenedor de
     corazón (`:3987-3997`);
   - se registra en el log: `quest flag … set (another player did it)` (`:3998`).

**Lo que debes ver aquí:**

- No hay evento "misión completada". Hay un **mapa de bits completo** que se reenvía y converge. Robusto ante
  pérdidas y entradas tardías; caro (4 KiB cada vez).
- La autoridad es la **unión**, no el anfitrión: un invitado con la partida avanzada vuelca su progreso (y los
  objetos asociados) en el guardado de los demás. Es justo lo que CU-06 y CU-07 cambian en F4.
- El estado no se limpia al salir: `g.quests` sobrevive a `stop_all` (`online.c:1322-1335`; hallazgo T-1,
  arreglo de F1).

Puedes provocarlo sin jugar: `swctl.py setflag <nombre>` en un juego y mirar el log del otro
(`docs/ONLINE.md:749-751`).

## 4. Recorrido B2: un objeto que cae al suelo

Caso: en PC-B (invitado) un enemigo propio suelta un objeto. Función: `loot_update`
(`online_actors.c:3366`), llamada en `:5949`.

1. **[J, PC-B]** El objeto se detecta y se apunta en una tabla local `g_mine`. La primera vez se construye un
   `OnlineLoot` (`online.h:239`: identificadores, origen, **nombre del actor**, posición, orientación) y, al
   no ser anfitrión, `online_send_loot_add(&l)` (`online_actors.c:3510-3522`).
2. **[J → C, PC-B]** Cola acotada de 16 (`online.c:611-615`). C la vacía y envía un `M_LOOT_ADD` por elemento
   (`online.c:1238-1247`).
3. **[S]** Relé con la ranura delante (`online.c:287-290`).
4. **[C, PC-A]** Solo si es anfitrión: lo encola en `g.la_in`, fija `origin` con la ranura que puso el
   servidor y termina la cadena del nombre (`online.c:1055-1061`).
5. **[J, PC-A]** `online_take_loot_adds` vacía la cola y `loot_host_add` lo mete en la lista global con un
   identificador nuevo, si no estaba y hay sitio (`online_actors.c:3495-3496`, `3356-3363`). Publica la lista:
   `online_set_loots` (`:3543`).
6. **[C, PC-A → S → todos]** `M_LOOTS` cuando cambia y cada 2 s (`online.c:1241-1249`).
7. **[J, los demás]** `online_loots(list)` (`:3542`) y, para cada entrada que no sea suya y esté cerca, **piden
   al juego crear el objeto real por su nombre**: `request(c, list[j].name, …)` (`:3604`).
8. Recogerlo en cualquier juego viaja como `M_LOOT_TAKE` por el mismo camino (`:3523-3540`).

**Lo que debes ver aquí:**

- Patrón "evento hacia el anfitrión, lista completa hacia todos". El evento puede perderse o duplicarse; la
  lista corrige.
- El paso 7 crea un actor con un nombre que llegó por la red y nadie comprobó contra una lista de permitidos
  (hallazgo A4 del mapa 07). Es uno de los arreglos de F1.

## 5. El contrato entre los dos ficheros

`online_actors.c` nunca abre un socket; `online.c` nunca llama al juego. Se hablan por `online.h`:

| Sentido | Familia | Semántica |
|---|---|---|
| juego → red | `online_set_local_*`, `online_set_loots`, `online_set_quests` | último valor |
| juego → red | `online_send_*` | cola acotada, se descarta si llena |
| red → juego | `online_players`, `online_skeleton`, `online_enemies`, `online_loots`, `online_quests` | copia instantánea |
| red → juego | `online_take_*` | vaciar cola |

Cualquier campo nuevo en `OnlinePose` toca tres sitios a la vez: `online.h`, `WireState` y las dos copias
campo a campo (`sample_local` y `remote_snap`), además del bot (`tools/online_bot.c:31-78` duplica la
estructura a mano). Por eso `online.h` tiene **dueño único** en el plan.

## 6. Preguntas para comprobar que lo has seguido

1. ¿En qué hilo se comprime el esqueleto? ¿Y en cuál se descomprime?
2. ¿Qué dos funciones del recorrido A llaman al juego, y por qué solo pueden estar en J?
3. ¿Dónde se pierde información si la red entrega dos `STATE` a la vez?
4. En B1, ¿qué mensaje va del invitado al anfitrión y cuál vuelve?
5. ¿Qué línea convierte un dato de red en una creación de actor?

Respuestas: (1) J de A (`online_set_local_skeleton`); C de B (`cl_message`). (2) `sample_skeleton` y
`pose_link`, vía `call`; necesitan el `Cpu` de un hilo invitado. (3) En el sello de tiempo local de
`remote_snap`: quedan casi simultáneos. (4) `M_QUEST` va; `M_QUESTS` vuelve. (5) `online_actors.c:3604`.

Siguiente: [06 · Depurar y probar](06-depurar-y-probar.md).
