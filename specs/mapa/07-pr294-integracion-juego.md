# 07 — PR #294 "Online mode": integración con el juego

Mapa del lado de **integración con el juego** del PR #294 (autores byluki, mstampfli) de Savage Wind Recomp.
Checkout analizado: `$SW/pr294-online` (carpeta hermana de solo lectura; HEAD `350c5f8`, base `22d36c9`). Solo lectura.

Abreviaturas de referencia usadas en todo el documento:

| Abrev. | Fichero |
|---|---|
| `OA:n` | `runtime/online_actors.c` línea n (6 468 líneas, leído completo) |
| `DOC:n` | `docs/ONLINE.md` línea n (1 017 líneas, leído completo) |
| `MOD:n` | `tools/online_mod.py` línea n (943 líneas, leído completo) |
| `GS:n` | `tools/game_sites.py` |
| `PA:n` | `tools/patches.py` |
| `EX` | `runtime/nvn/vulkan/exec.c` (diff del PR) |
| `NET` | `runtime/online.c` / `online.h` (capa de red: la cubre otro agente; aquí solo la interfaz) |

Alcance de lo verificado: todo lo que aquí se afirma sale de leer el código y la documentación. **No se ha compilado ni
ejecutado nada.** Donde algo es una inferencia y no un hecho leído, se dice.

Dato previo importante: el PR dice "mod v36" en un título de sección (DOC:770), pero la versión real del overlay es
**42** (`VERSION = 42`, MOD:66; `stat("online_mod/v42")` en `runtime/main.c:507`) y el protocolo de red es **38**
(`runtime/online.c:40`).

---

## 1. Cómo se pone un jugador remoto en el mundo

### 1.1 Idea general

Cada jugador juega **su propia partida** (su guardado, su mundo). Los demás aparecen como actores que el juego original
no tiene, fabricados a partir de los ficheros del propio usuario:

- `OnlinePlayer`: el Link remoto (un actor de perfil `DemoNPC`, sin IA, sin cuerpo físico).
- `OnlineArmor_<ModelUser>`: una pieza de armadura (solo modelo + física de tela/huesos de apoyo).
- `OnlineGear_<modelo>`: arma, escudo, arco, vaina, carcaj, paravela, flechas, materiales en la mano.
- `OnlineProj_<bomba>`, `OnlineBoom_<bomba>`, `OnlineKnock`, `OnlineShock`, `OnlineIce`, `OnlineBeam`,
  `OnlineSlate`, `OnlineRuneFx`, `OnlineMagnetFx`, `OnlineHorse_<actor>`, `OnlineEnemy_<enemigo>`.

Todo el trabajo por fotograma ocurre en `rt_online_frame(Cpu *c)` (OA:5721), llamada desde el parche `_frame_wait`
(PA:425-426: `rt_vfr_update(...); rt_online_frame(c);`), es decir, en el hilo principal del juego, dentro de
`ksys::VFR::updateInterval`, **antes** de la actualización de los actores (OA:1-2). Se llama siempre, haya o no sesión.

### 1.2 `tools/online_mod.py`: qué lee y qué escribe

Invocación: `python tools/online_mod.py <romfs> <out>` (MOD:4). Necesita `oead` (MOD:30). `runtime/main.c:490-520` lo
lanza con `system()` en el primer arranque si no existe `online_mod/v42` (y no hay `SAVAGEWIND_ROMFS_OVERLAY` ya
puesto): con el Python del venv en un checkout de fuentes, o con el ejecutable congelado `swonline/` (PyInstaller,
`make online-helper`) en una release. Si `online_mod/online_mod.txt` existe, pone
`SAVAGEWIND_ROMFS_OVERLAY=online_mod`. **Consecuencia:** el overlay de RomFS es un único hueco; si el usuario ya usa
su propio overlay (un mod), el modo online queda sin actores (solo figuras del overlay 2D).

**Lee** (todo de los ficheros del usuario):

| Fuente | Para qué | Ref. |
|---|---|---|
| `Pack/TitleBG.pack` | packs residentes; `Model/Link.sbfres` (nombres de huesos de Link); `Quest/QuestProduct.sbquestpack` | MOD:185-189, 297, 847 |
| `Actor/Pack/Dm_Npc_Rito_HeroSoul_Kago.sbactorpack` | ActorLink de un DemoNPC (base de todos los actores online) | MOD:33, 201 |
| `PauseMenuPlayer` | lista de modelos y física de Link (se le quitan `RigidContactInfo`, `RigidBodySet`) | MOD:34, 277-286 |
| `Dm_Animal_Heron_A` | programa de IA que "solo está de pie" (raíz `DummyAction`) | MOD:35, 210-214 |
| `GameROMPlayer` | lista de AS del jugador (`Player.baslist`, 399 AS) + todos sus `.bas` + `AnimationInfo` (estelas de espada) | MOD:36, 236-274 |
| `Actor/Pack/Armor_*_(Head\|Upper\|Lower)*` | una pieza online por lista de modelos; mapa de huesos | MOD:303-342 |
| `Actor/Pack/Weapon_*`, `Item_*`, `Animal_Insect_*`, flechas, `PlayerStole2*` | `OnlineGear_*` | MOD:349-366 |
| `Actor/Pack/Enemy_*` | `OnlineEnemy_*` (pack propio del enemigo, sin IA/horario/drops/químico; AS "aplanados") | MOD:485-567 |
| `RemoteBomb*`, `Obj_IceMakerBlock`, `EnemyShockWave`, `PlayerBeam`, `Item_Conductor`, caballos, sillas, riendas | proyectiles, explosiones, hielo, efectos de runas, monturas | MOD:367-384, 453-483, 569-713 |
| `Actor/ActorInfo.product.sbyml` | entradas de los actores nuevos; `attackPower`, `armorDefenceAddLevel` | MOD:720-794, 832-841 |
| `System/Resource/ResourceSizeTable.product.srsizetable` | tamaños RSTB | MOD:190-191 |
| `Pack/Bootup.pack`: `Actor/ResidentActors.byml`, `GameData/gamedata.ssarc` | lista de residentes; nombres de flags bool | MOD:806-820, 868-931 |

**Escribe** en `<out>` (carpeta `online_mod/`, ignorada por git):

| Salida | Contenido | Ref. |
|---|---|---|
| `Pack/TitleBG.pack` | el original + `Actor/Pack/OnlinePlayer.sbactorpack` | MOD:715-718, 822 |
| `Pack/Bootup.pack` | `ResidentActors.byml` con `OnlinePlayer` (`only_res: true`) | MOD:806-820 |
| `Actor/Pack/*.sbactorpack` | todos los actores no residentes (se cargan bajo demanda) | MOD:823-824 |
| `Actor/ActorInfo.product.sbyml` | entradas nuevas insertadas en orden de crc32 del nombre | MOD:759-766, 825 |
| `System/Resource/ResourceSizeTable.product.srsizetable` | RSTB con los tamaños nuevos | MOD:797-798, 826 |
| `v42`, `online_mod.txt` | sello de versión y marca de "overlay online" | MOD:827-828 |
| `gear.txt` | `actor del juego=actor online` (armas, flechas, bombas, `Boom_*`, caballos, materiales) | MOD:830 |
| `armor.txt`, `bones.txt` | pieza → `OnlineArmor_*`; por unidad de modelo, índice de hueso de Link (255 = Link no lo tiene) | MOD:936-938 |
| `enemies.txt`, `wscale.txt`, `power.txt`, `defense.txt` | enemigo → marioneta; escala de armas por enemigo; poder de ataque; defensa de armadura | MOD:831-844, 934 |
| `quests.txt` | hasta 32 768 nombres de flags compartidos | MOD:845-932 |

El runtime carga esos `.txt` de forma perezosa con `fopen` desde la carpeta del overlay (OA:445-469, 475-494,
3859-3877) y nunca los libera ni recarga.

### 1.3 RSTB (tabla de tamaños de recursos)

Es el punto más frágil del generador; varios cuelgues documentados vienen de aquí:

- Clase `Rstb` propia (MOD:113-146): crc32 → tamaño, y tabla de nombres para colisiones.
- Pack de actor: `len(pack) * 1.5 + 0x1000` (MOD:231, 481, 551, 709).
- ActorLink reescrito de un enemigo: tamaño del original + 2 × lo que creció + 0x400; si se queda corto, "the Tags
  parse runs out of the resource's memory and the game crashes" (MOD:544-546; DOC:578-580).
- `ResidentActors.byml`: `len * 4 + 0x1000`; demasiado pequeño → el juego no lee la lista y se cuelga al arrancar en
  `0x7000d49080` (MOD:818-819).
- `ActorInfo.product.byml`: `max(original, len * 2)` (MOD:795).
- Lista de AS de `OnlinePlayer`: `max(original, 512 × nº defines) + 0x4000` (MOD:264).
- Cualquier tamaño que falte se pone a `0x10000` (MOD:797-798).

### 1.4 Reglas que el generador debe respetar (aprendidas a golpes)

- **Las definiciones de AS deben quedar ordenadas por nombre**: el juego busca un AS por búsqueda binaria (MOD:234-262;
  DOC:89-90).
- El pack de un DemoNPC debe contener la lista de AS y los AS que nombra; sin ellos "its load never ends" (DOC:170;
  MOD:196-199).
- **Límite de actores residentes**: la lista admite solo diez más que los 48 del juego; "the 59th crashes it at
  start, 0x7000d49080's fixed arrays" (DOC:90-91); "only nine fit in the resident list" (DOC:169-170). Por eso solo
  `OnlinePlayer` es residente y todo lo demás se pide bajo demanda.
- `traverseDist = 1000` en todos los actores online (MOD:70, 756, 778): sin eso el juego borraba el equipo a ~50 m y
  al Link remoto a 80-140 m de la cámara y había que rehacerlos sin parar; los `OnlineEnemy_*` se rehacían unas dos
  veces por segundo (DOC:198-203).
- Las marionetas de enemigo no llevan `ChemicalUser` (MOD:518-522): si no, el fuego de un campamento las quemaba en
  dos juegos a la vez y el daño se enviaba como golpes de jugador (DOC:626-629).
- Física de la marioneta sin `use_character_controller` y con los cuerpos de ataque en la capa `SensorNoHit`
  (MOD:531-542; DOC:575-578).
- Los AS de enemigo/caballo se "aplanan" a un solo clip siguiendo el hijo `default` (o el primero) hasta el primer
  `TypeIndex 67` (MOD:386-421): la marioneta no tiene IA a la que preguntar. Efecto: las animaciones de las marionetas
  son aproximadas.
- Las piezas teñidas comparten lista de modelos: se ven sin teñir (DOC:153-154).

### 1.5 Creación y destrucción (spawn / despawn)

Dos vías de creación, ambas con direcciones fijas de 1.9.0:

- **Inmediata** `create()` (OA:543-550) → `FN_CREATE 0x7001535900`: solo funciona si los recursos ya están cargados
  (por eso `OnlinePlayer` es residente). Se usa para el Link remoto (OA:6044).
- **Bajo demanda** `request_p()` (OA:556-564) → `FN_REQUEST 0x7001535b10` con un `BaseProcHandle` de 16 bytes; el
  runtime mira cada fotograma el estado de la unidad (`int` en +0: 2 = hecho, 3 o 5 = falló) y entonces
  `FN_HANDLE_TAKE 0x7001516b74` o `FN_HANDLE_RELEASE 0x70015168a8` (OA:611-626).

Estructuras de ciclo de vida:

- `Watch {unit, id}` + `alive()` (OA:84-94): imita el `BaseProcLink` del juego (unidad en actor +0x68, id en +0x60; la
  unidad guarda id en +0x40 y el actor en +0x48; estado 3 en actor +0x64 = borrándose). `alive_rewatch()` (OA:98-105)
  cubre el caso de un actor "despertado" que cambia de unidad.
- `Handle g_handles[4096]` en anillo (OA:109-117): la unidad guarda la dirección del handle en +8, así que cada
  petición necesita un handle que no se mueva.
- `Piece` (OA:119-126): nombre, actor, watch, handle, `pending`, `fails`, `wait`.
- `piece_update()` (OA:599-642): máquina de estados por pieza (cambia el nombre → la vieja al pool; busca en el pool;
  pide; recoge; reintenta hasta 5 veces).
- **Pool por jugador** `g_pool[8][24]` (OA:280-281, 379-420): regla clave documentada — "a second actor of a piece
  made for him ... is never drawn, whatever was tried" (OA:338-342; DOC:136-145). Por eso una pieza que se quita no se
  borra: se esconde 5 000 m bajo el mundo (`hide_piece`, OA:371-375) y se reutiliza.
- **Huérfanos** `g_orphan[128]` (OA:288-310): peticiones en curso cuando se olvida todo; al completarse se borran.
- `forget_all()` (OA:316-336): olvida (no borra) todo tras una carga. `drop(i, left)` (OA:423-439): un jugador sale de
  alcance o se va.

Decisión de crear/quitar por jugador (OA:6013-6052): se quiere un Link remoto si el jugador está `used`, no es el
local, no está cargando, está en **el mismo mapa** (`same_map`, OA:1484-1486) y a menos de **400 m** en XZ (OA:6018).
Detección de carga: Link local sin posición o salto de más de 100 m entre fotogramas (OA:5748-5761) → `forget_all()` y
60 fotogramas de espera (`g_settle`); si el juego borra un Link remoto → `forget_all()` y 180 fotogramas (OA:6036-6041).
Mientras el Link local está muerto (AS `Dead*`) no se crea nada y se esperan 180 fotogramas al volver (OA:5873-5880):
"making them again during it crashed the game" (DOC:732-734). Fuera de sesión se borran todos (OA:5733-5746).

### 1.6 Copia del esqueleto (121 huesos, escritura por fotograma)

- **Muestreo local** `sample_skeleton()` (OA:1871-1893): encuentra la unidad de modelo que contiene `Skl_Root` con
  `FN_FIND_BONE 0x7000f77260` (`skel_unit`, OA:671-685); cuenta de huesos en `unidad+0x118 → +0x50` (121 en Link);
  por cada hueso llama a `FN_BONE_LOCAL_GET 0x7000f960f8` y convierte la matriz local a cuaternión + traslación. Añade
  los huesos del arco (cuerda) detrás (máx. 32; total máx. `ONLINE_BONES` = 160). Entrega a NET con
  `online_set_local_skeleton` (OA:1892). Lee la pose del fotograma **anterior** (el bucle corre antes de la
  actualización de actores, DOC:74-76).
- **Red**: mensaje SKEL 20 veces/s, 6 bytes de rotación + 3×s16 de traslación por hueso, ~1,6 KB, ~30 KB/s por
  jugador; interpolado 100 ms por detrás con nlerp (DOC:55-60).
- **Escritura remota** `pose_link()` (OA:1901-1942): cada fotograma escribe cada hueso con
  `FN_BONE_LOCAL_SET 0x7000f95fa8` en el Link remoto, que **no reproduce ningún AS**, así que nada lo sobrescribe
  antes de que el juego calcule las matrices de mundo. Si no llega esqueleto nuevo (pausa, menú) repite el último
  (OA:1907-1911). Sin esqueleto durante 60 fotogramas (cliente antiguo, bot) cae a `animate()` (OA:1971-1985): AS del
  jugador por canal, o 13 AS de un clip `Online*` (OA:183-185).
- Motivo de copiar huesos en vez de reproducir AS: los AS del jugador eligen clip según el estado del jugador (tipo de
  arma, escudo, arco) "and went wrong" (DOC:76-78).

### 1.7 Armadura y equipo

- **Qué lleva el jugador local** `equipment()` (OA:728-736): gestor de armadura en `player+0x2500` (enlaces en +0x10,
  0x10 cada uno; su +0x128 es el jugador); arma/escudo/arco en `player+0xb98 / +0xbb0 / +0xbc8`; vaina y carcaj en
  `+0x958` del arma/arco; paravela encontrada **escaneando** `player+0x100..0x4000` en busca de un enlace a
  `PlayerStole2*` (OA:706-726; 0x2c28 en 1.9.0, DOC:194-196). Se publica con `online_set_local_equip` (OA:1577).
- **Armadura remota** (OA:6063-6094): un `OnlineArmor_*` por pieza sobre la matriz del Link remoto; `pose_armor()`
  (OA:1945-1967) escribe en cada unidad los huesos de Link según `bones.txt`. Sin casco: `Armor_Default_Head` (pelo);
  sin pantalón: `Armor_Default_Lower`. `cover()` (OA:646-660) llama a `FN_HIDE_GROUPS 0x700126ba4c` para ocultar la
  piel bajo la ropa (grupos `HideMaterialGroup` del bfres).
- Historia: primero se usaron los `Armor_*` reales del juego, vestidos con `0x700117363c`; "crashed some games (mixed
  sets, fast changes)" y una segunda instancia nunca se dibujaba (DOC:147-154). Queda **código muerto** de esa época:
  rama `Armor_Default` (OA:628-635), llamadas a `FN_ARMOR_BIND` solo para nombres `Armor_*` (OA:345, 372),
  `FN_ARMOR_INIT` definido y sin uso (OA:55).
- **Tinte de armadura**: se lee de `player+0x1114/+0x1118/+0x111c` (OA:1821-1825), viaja en `pose.dye` y la pieza se
  pide con el parámetro `ArmorDyeColor` en un `InstParamPack` fabricado a mano (OA:569-578, 6069-6077). **No está
  comprobado que se vea** (DOC:1003-1008).
- **Equipo remoto** (OA:6275-6317): cada pieza se coloca cada fotograma en `matriz del hueso × matriz relativa`
  enviada. El emisor elige el hueso más cercano de 8 (`Weapon_R`, `Weapon_L`, `Pod_A/B/C`, `Spine_2`, `Waist`,
  `Skl_Root`; OA:189) con histéresis para que no parpadee (OA:1582-1604). Sin hueso pero a menos de 60 m:
  `ONLINE_GEAR_WORLD` con matriz de mundo (arma lanzada).
- Otros: flecha en la cuerda (`nock`, OA:6413-6440), materiales sobre la cabeza (`held`, OA:6344-6376), caballo/moto
  y arreos (OA:6318-6340), pizarra y efectos de runas (OA:4304-4331).

---

## 2. Sistemas sincronizados

Convención de la tabla: **Autoridad** = quién decide el estado. "Cada uno" = cada juego simula lo suyo y solo informa.

### 2.1 Tabla resumen

| # | Sistema | Autoridad | Funciones / sitios del juego usados | Código | Doc |
|---|---|---|---|---|---|
| 1 | Posición, orientación, AS, esqueleto del jugador | Cada jugador sobre sí mismo | sitio `player_info`; `FN_AS_IN_SLOT`, `FN_FIND_BONE`, `FN_BONE_LOCAL_GET/SET`, `FN_SET_MTX`, `FN_PLAY_AS` | OA:1493-1841, 1871-1985, 6054-6058 | DOC:62-78 |
| 2 | Armadura y equipo | Cada jugador | `FN_REQUEST`, `FN_HIDE_GROUPS`, vtable de unidad +0xb8/+0x1b0 | OA:728-736, 6063-6094, 6275-6317 | DOC:126-203 |
| 3 | Efectos y sonidos (estela de espada, giros, voz, Daruk, Revali, pisadas) | Cada jugador | sitio `xlink_emit`; sitios `blur_anim`, `blur_frame_a/b`, `blur_anim_draw`; `FN_EFFECT_EMIT`, `FN_EFFECT_KILL`, `FN_XLINK_EMIT` | OA:827-898, 919-945, 1069-1156, 6096-6274 | DOC:207-279, 679-688 |
| 4 | Bombas remotas | Lanzador (solo efectos y sonido para los demás) | sitio `remote_bomb_made`; `FN_SEND_MSG` (mensaje 0x800000a) | OA:814-818, 1183-1296, 1385-1447, 6441-6466 | DOC:288-313 |
| 5 | Flechas y haz de la Espada Maestra | Lanzador (los demás ven modelos; el daño se queda en su juego) | sitios `arrow_move`, `beam_move` | OA:820-825, 1208-1262, 6378-6412 | DOC:281-304 |
| 6 | Magnesis | Quien sostiene | `P_MAGNE`, `FN_BODY_TO`, `FN_ACTOR_LOOK_ON/OFF`, `FN_SPECIAL_FX_B`, `FN_XLINK_EFFECT`, `FN_MAGNE_BEAM_SET`, `P_MAGNE_FX`, `P_MAGNE_AIM` | OA:1464-1469, 1654-1674, 4333-4498 | DOC:315-383 |
| 7 | Stasis | Quien paró | actor +0x710 bit 3; `FN_BODY_TO`, `FN_SPECIAL_FX_A_UNIT`, `FN_SPECIAL_FX_B`, vtable +0x208 | OA:1675-1711, 4012-4122 | DOC:385-395 |
| 8 | Cryonis | Quien creó el pilar | `OnlineIce`, AS `Appear_Linear` | OA:1712-1735, 4126-4162 | DOC:397-400 |
| 9 | Jugadores en otros mapas | — (se ocultan) | `FN_MAP_TYPE`, `FN_MAP_NAME` | OA:1470-1486, 6014 | DOC:402-407 |
| 10 | Jefes (Ganon, Bestia, Iras) | Cada juego el suyo; el **daño** se suma | actor +0x848 (vida), +0x640 (HashId) | OA:2025-2027, 4164-4288 | DOC:409-427 |
| 11 | Misiones y flags | **Unión** de todos (el anfitrión hace el OR) | `test_warp_gdt_mgr`, `FN_GDT_HANDLE`, `FN_GDT_SET_BOOL`, `FN_POUCH_CAN_GET/GET`, `FN_GET_SPECIAL`, `FN_EVT_*` | OA:3651-4000 | DOC:429-487, 704-720, 988-997 |
| 12 | Botín de enemigos y cosas caídas | Lista en el anfitrión; el dueño del objeto lo añade | `FN_REQUEST` del objeto real, `FN_DELETE` | OA:3237-3649 | DOC:495-555 |
| 13 | Armas en el suelo (PICK) | Quien la recoge | `FN_DELETE` | OA:5458-5560 | DOC:557-563 |
| 14 | Enemigos | Lista y propietario en el anfitrión; la IA corre en el juego del **propietario** | lista de unidades de actor; `FN_REQUEST` con `InstParamPack` (GOT 0x7001d664b0); +0x848 | OA:1987-2636, 2950-3235 | DOC:565-658, 795-817 |
| 15 | Ataques de enemigos a jugadores | El juego de la víctima (heurística) | player +0x848; `OnlineShock` / `OnlineKnock` | OA:2692-2778 | DOC:644-667 |
| 16 | Luna de Sangre | Anfitrión (temporizador) + regla local para flags | `world_mgr` +0x5c8 → +0xc8, +0x120; `FN_GDT_SET_F32` | OA:959-1001, 3913-3924 | DOC:714-720, 783-790 |
| 17 | Hora del día | Anfitrión | `test_warp_hour`, `test_warp_set_hour` | OA:5851-5864 | DOC:722-724 |
| 18 | Clima | El primer jugador de la sesión a menos de 500 m | `world_mgr` +0x7d1, +0x794, +0x7d6 | OA:947-957, 1004-1024 | DOC:660-663, 792-793 |
| 19 | Reanimación (hada) | Regla del anfitrión | sitio `has_fairy` | OA:2796, 2825-2831 | DOC:775-781 |
| 20 | Daño entre jugadores (PvP) | El atacante declara, la víctima aplica | player +0x848, `power.txt`, `defense.txt` | OA:2874-2942 | DOC:696-702, 806-808 |
| 21 | Colisión entre jugadores | Cada juego empuja a su Link | `control_cell_player_pos` | OA:2833-2872 | DOC:690-694 |
| 22 | Objetos del mundo (cajas, rocas, troncos) | Quien lo puso en movimiento | `FN_BODY_TO`, `FN_ACTOR_WAKE`, `FN_SET_MTX` | OA:4510-4785 | DOC:819-862 |
| 23 | Mecanismos de nivel | Quien lo mueve (empate: número de jugador menor) | `FN_BODY_TO`, `FN_SET_MTX`, `FN_PLAY_AS`, `FN_ACTOR_WAKE` | OA:4787-4980 | DOC:971-986 |
| 24 | Objetos rotos / árboles talados / objetos de mapa recogidos | Quien rompe; lista de sesión de 2 048 | `FN_TAKE_OUT`, `FN_MAPOBJ_GONE`, `FN_ACTOR_WAKE`, +0x848 = 0, flags | OA:4982-5456 | DOC:931-969 |
| 25 | Caballos y Moto Maestra | Jinete | `OnlineHorse_*`, `FN_PLAY_AS` | OA:1758-1820, 6318-6340 | DOC:669-677 |
| 26 | Menús sin pausa | Regla del anfitrión | sitios `scene_state_*`, `menu_draw`, `pause_state`; 5 funciones y 4 punteros fijos | OA:2798-2824, 5767-5845 | DOC:864-929 |
| 27 | Pedestales Sheikah (bloqueo del botón A) | Quien está en el evento | `framepace_event_name`, `g_online_block_a` en `hle/input.c` | OA:1625-1633, 5981-6001 | DOC:458-461 |

### 2.2 Detalle por sistema

**Estado del jugador (1).** `sample_pose()` (OA:1493) rellena un `OnlinePose` enorme y lo entrega con
`online_set_local_pose` (OA:1833). No se muestrea si el juego está en pausa, salvo con "menús sin pausa" (OA:5766).
Un jugador sin `pose.valid` (AS del canal 0 vacío) se trata como "en nuestro mapa" (OA:5846-5850).

**Lanzamientos (4, 5).** El parche `remote_bomb_made` anota la bomba recién creada (`x22`); `arrow_move` y `beam_move`
anotan cada flecha/haz en un anillo de 64 desde **cualquier hilo** (OA:806-825). El bucle principal acepta una flecha
como propia solo si se ve por primera vez a menos de 4 m del Link local **y** él estaba usando el arco en el último
segundo; si no, la recuerda como ajena (OA:1208-1253). Sin ambas comprobaciones se enviaban flechas de enemigos como
propias (DOC:301-304). Explosión: cuando la bomba empieza el AS `Explode` (OA:1274). En los demás juegos se crea
`OnlineBoom_*` y se le envía el mensaje de la runa. Fallos conocidos: el haz **nunca se ha visto funcionar**
(DOC:1011-1012).

**Magnesis (6).** El objeto se identifica por **nombre + posición** (el más cercano a donde lo cogió o a donde está
ahora, 15 m; primero el botín de ese jugador, 30 m; OA:4361-4401). Se mueve con `FN_BODY_TO` (velocidades para llegar
en un paso). Para dibujar el haz remoto se **escriben seis floats en un global del juego** (`P_MAGNE_AIM + 0x192cc`,
OA:4485-4489), salvo si el Link local usa Magnesis. Fallos anotados: "sometimes it doesn't show" (resuelto no
metiendo esos actores en el pool, DOC:359-361); si los dos mundos divergen el objeto puede "no estar aquí".

**Stasis (7).** El emisor busca cada 6 fotogramas entre todos los actores uno con `+0x710 & 8`, modelo y cuerpo
(OA:1685-1702). `has_body()` existe porque un `LinkTagAnd` tenía ese bit, se tomó por objeto parado "and the other
game, applying Stasis to it, crashed" (OA:67-72).

**Otros mapas (9).** Un jugador en un santuario o Bestia Divina no tiene Link en el mundo de los demás ni posee
enemigos de ese mundo (`can_own`, OA:2367-2370). Los que están en el **mismo** santuario sí se ven (DOC:996-997).

**Jefes (10).** Solo los que tienen HashId; los drones de las Iras no lo tienen y no se comparten (OA:4191-4194;
DOC:422-424). Cada juego envía el daño acumulado que hizo **su** jugador; los demás restan el incremento
(OA:4251-4288). Las fases las lleva cada juego por su cuenta.

**Misiones y flags (11).** `quests.txt` lista hasta 32 768 flags (25 767 en mod v37, DOC:991). Cada 2 s (cada 60
llamadas, OA:3880) cada juego envía un bit por flag; el anfitrión hace OR con los suyos y devuelve la unión; cada
juego activa los que le faltan con el setter del juego (`set_bool_flag`, OA:3794-3804; si falla, escribe el bit en
memoria, OA:3986). Con ciertos flags se **entregan objetos** (OA:3987-3997): pizarra, runas, paravela
(`FindDungeon_Finish`), semilla Kolog, Orbe espiritual (`Clear_Dungeon*`), habilidad de campeón, **contenedor de
corazón** (`Get_HeartUtuwa_*`). Kolog cercano (60 m): se lanza su propia escena `Demo017_0` (OA:3811-3847).
Puntos a tener en cuenta para nuestro diseño:

- **No es autoritativo del anfitrión**: es una unión. Un invitado con una partida avanzada vuelca todos sus flags (y
  los objetos asociados) en el guardado del anfitrión y de los demás, de forma irreversible.
- "What a flag does in the world beyond the quest ... may only show after a load" (DOC:441-442).
- El rompecabezas de un santuario no está en flags (DOC:996).
- Cargar otra partida a mitad de sesión se detecta como Luna de Sangre (ver §6, M7).

**Botín (12, 13).** Tres orígenes van a la misma lista (64 entradas, en el anfitrión): lo que suelta un enemigo
propio al morir (se vigila 10 s y 6 m, OA:3287-3302, 3403-3429), lo que cae al suelo cerca (objeto nuevo sin HashId,
quieto 0,4 s; OA:3430-3488) y las armas lanzadas/soltadas (OA:1511-1569). Los demás juegos crean el **objeto real**
por nombre (OA:3600-3632) a menos de 150 m. Recogido = desaparecido con un Link a menos de 4 m, o pasa a estar en su
mano (OA:3523-3541, 3577-3599). Lista llena: lo nuevo se queda solo donde cayó (OA:3359-3360). No probado: lanzar
materiales, insectos, menas/cajas rotas por otro (DOC:525-526, 546).

**Enemigos (14, 15).** El anfitrión mantiene un registro de 64 (`g_reg`, OA:2318) con los `Enemy_*` (no jefes) a
menos de 150 m de cualquier jugador y envía hasta 32, 10 veces/s. Cada enemigo tiene un **propietario** (ranura): su
juego lo tiene "de verdad" (IA incluida) y envía su estado (ESTATE); los demás borran los suyos a menos de 170 m del
centro (OA:3000-3010) y muestran marionetas `OnlineEnemy_*` (`g_puppet[64]`, OA:2640-2650).

- **Histéresis de propiedad** (OA:2536-2623): cambia solo si el dueño está a más de 40 m y otro 10 m más cerca, o si
  otro lleva 15 m más cerca durante 50 ticks; no otra vez durante 100 ticks (~10 s, corre cada 3 fotogramas). Nunca a
  un jugador a más de 80 m (`OWN_SAFE`) salvo que el dueño se haya ido. `OWNER_NONE`: de nadie hasta que alguien se
  acerque. Sin esto, dos jugadores a ~40 m se lo pasaban cada pocos segundos (DOC:588-590).
- **Agresión** (OA:2520-2563): quien golpea un enemigo ajeno se lo queda (hasta 45 m, no otra vez en 6 s).
- **Golpes**: la marioneta tiene la vida del enemigo real; lo que pierde se envía como HIT al dueño (OA:3096-3128).
- **Golpe furtivo**: la marioneta no tiene sentidos, el juego la cree desprevenida y multiplica por 8; se recorta al
  poder del arma si el enemigo real está luchando (OA:3102-3120), y un enemigo recién traspasado lleva 8 s de "vida de
  guardia" +1000 (`own_guard`, OA:2185-2216).
- **Elementos**: una flecha elemental cerca de la marioneta golpeada envía el elemento; el dueño crea una fuente
  (`FireRodLv1Fire`, `ExpandIceField`, `ThunderRodLv1Thunder`) un metro sobre el enemigo (OA:2247-2294).
- **Ataques sobre los demás** (`strike`, OA:2696-2727): cuando el AS del enemigo contiene `Attack` o `Kick`, 10
  fotogramas después se resta vida al Link local si está dentro del alcance, delante y sin `Guard`. Es una
  **heurística**, no una colisión real. Golpe fuerte → `OnlineShock` (se estremece; no lo derriba).
- Muerte: flag `<mapa>_<actor>_<HashId>` en todos los juegos (`mark_dead`, OA:2343-2357).
- Si se corta el flujo del anfitrión, las marionetas se quedan como estaban y los enemigos propios siguen suprimidos
  (DOC:584-585).

**Luna de Sangre (16).** Dos mecanismos distintos: el temporizador del anfitrión se impone si difiere en más de 30
(OA:984-1001); y cada juego que ve 64 o más flags "de reaparición" borrados a la vez considera que tuvo una Luna de
Sangre y deja de reaplicar los que la unión aún tenía (`held_off`, OA:3913-3924, 3952-3955).

**Reanimación (19).** El parche `has_fairy` hace que "¿tiene un hada?" responda sí mientras hay sesión y la regla del
anfitrión está activa (OA:2825-2831). Es una **auto-reanimación instantánea** con la animación del hada; no interviene
el compañero. Nota: la regla se actualiza dentro de `players_collide` (OA:2838-2839), **después** de un `return`
temprano (OA:2837): con `SAVAGEWIND_ONLINE_NO_COLLIDE` o durante un evento no se refresca.

**PvP (20).** Desactivado por defecto; solo entre dos jugadores que lo tengan activo. Al empezar un golpe (la estela
del arma) se elige al jugador más cercano dentro del alcance y delante (OA:2881-2905) y se envía `pvp_seq / target /
damage`; la víctima aplica daño menos defensa, o bloquea si su AS contiene `Guard` (OA:2907-2928). Bomba ajena a
menos de 4 m: 8 cuartos menos defensa y derribo con `OnlineKnock` (OA:2929-2941). Sin flechas PvP. El daño declarado
se acepta tal cual.

**Objetos del mundo, mecanismos, roturas (22-24).** Identificación por **HashId del mapa** (actor +0x640); un tronco
talado no tiene HashId propio y usa el del árbol (OA:4530-4531). Reglas de "de quién es" con ventanas de tiempo
(`OBJ_TIE_NS` 0,3 s, `OBJ_QUIET_NS` 0,7 s, `TRUNK_QUIET_NS` 8 s). Roto por otro → `break_here()` (OA:5132-5196):
árbol = despertar, sacar 0,4 s después, crear tocón y tronco; objeto de mapa recogido = `take_out`; el resto = vida a
0 y, si no se rompe en 1,5 s, se retira. La lista de sesión se reaplica una vez por segundo (`world_enforce`) y se
retransmite una entrada cada 1,5 s para los que entran tarde (`world_rotate`). No probado: ascensor montado de verdad,
piezas de una Bestia Divina (DOC:984-986, 1013-1014).

**Menús sin pausa (26).** El menú de pausa, el mapa y los menús rápidos ponen bits de pausa en el gestor de procesos;
el runtime los limpia cada fotograma, reanuda la física y hace que cinco lecturas del "estado de escena" devuelvan 0
(`rt_scene_state`, OA:2798). Excepción: al pulsar A o X en la bolsa se vuelve a parar el mundo, porque si no lo que
se saca "se queda colgado en el aire" (OA:5787-5809; DOC:898-909). Con el menú abierto los golpes de los enemigos
propios no llegan a Link, así que el runtime los aplica con la misma heurística (`own_strikes_in_menu`, OA:2736-2778).

---

## 3. Funciones, direcciones, offsets y sitios de los que depende el PR

### 3.1 Recuento

| Clase | Nº | Cómo se localiza |
|---|---|---|
| **Sitios por patrón nuevos** (`game_sites.py`) | **16** (en 9 grupos opcionales) | Patrón de bytes con máscaras; tabla `KNOWN` solo para `1.9.0` (GS:571-572) |
| Sitios por patrón ya existentes que se reutilizan | 3 (`player_info`, `pause_state`, `world_mgr`) + el del reloj usado por `test_warp_gdt_mgr` | Patrón (GS:254-263) |
| **Direcciones absolutas fijas de 1.9.0 en `online_actors.c`** | **62** = 47 funciones + 15 punteros de datos/GOT | Literal en el código, sin patrón |
| Ranuras de vtable fijas | 4 (unidad de modelo +0x70, +0xb8, +0x1b0, +0x208) | Literal |
| Offsets de estructura distintos | ~67 literales `+ 0x..` distintos en el código (más los que solo están en parches) | Literal |
| Offsets descubiertos en ejecución | 3 (paravela OA:706-726; arma del enemigo OA:2036-2056; HashId para Kolog OA:3827-3831) | Escaneo de memoria |

**Resumen: 19 localizados por patrón frente a 62 direcciones fijas.** Además **no hay ninguna comprobación de
versión**: `online_actors_on()` solo mira si existe el overlay (OA:225-229, 208-221); `g_game_version` existe
(`runtime/game_sites.h:113`) pero no se usa en este fichero. Con otra versión del juego los sitios por patrón pueden
resolverse, pero las 62 direcciones fijas saltarían a código equivocado.

### 3.2 Sitios por patrón añadidos por el PR

| Sitio | Dirección 1.9.0 | Parche (`patches.py`) | Llama a | Offsets embebidos en el parche |
|---|---|---|---|---|
| `arrow_move` (GS:165) | 0x70005ee5e8 | `_arrow_move` PA:535 | `rt_online_arrow(x0)` | acción +0x140, actor en acción +8 |
| `remote_bomb_made` (GS:168) | 0x7000b3bb5c | `_remote_bomb_made` PA:545 | `rt_online_bomb(x22)` | actor +0x558 |
| `beam_move` (GS:172) | 0x70007104a4 | `_beam_move` PA:554 | `rt_online_arrow(x0)` | — |
| `xlink_emit` (GS:206) | 0x7000f5106c | `_xlink_emit` PA:563 | `rt_online_xlink_emit(x0, x1)` | — |
| `fmt128` (GS:178) | 0x7000579620 | `_fmt128` PA:573 | `rt_game_fmt` (solo registro) | — |
| `blur_anim` (GS:182) | 0x7001510f0c | `_blur_anim` PA:583 | `rt_blur_anim(x19, ...)` | ranura de animación 0xa8 |
| `blur_frame_a` | 0x7001510fe8 | `_blur_frame_a` PA:597 | `rt_blur_frame` | — |
| `blur_frame_b` | 0x7001511098 | `_blur_frame_b` PA:605 | `rt_blur_frame` | x26 +0xc |
| `blur_anim_draw` | 0x700151164c | `_blur_anim_draw` PA:590 | `rt_blur_anim(x20, ...)` | 0xa8 |
| `has_fairy` (GS:205) | 0x7000cec570 | `_has_fairy` PA:616 | `rt_online_has_fairy` | — |
| `menu_draw` (GS:201) | 0x7000d45d84 | `_menu_draw` PA:625 | `rt_menu_draw_pause` | — |
| `scene_state_get` (GS:192) | 0x700127f3c4 | `_scene_state` PA:634 | `rt_scene_state` | global +0xef0 |
| `scene_state_phys` | 0x70012800bc | ídem | ídem | +0xef0 |
| `scene_state_b` | 0x70012810b8 | ídem | ídem | +0xef0 |
| `scene_state_c` | 0x700128322c | ídem | ídem | +0xef0 |
| `scene_state_d` | 0x7001280164 | ídem | ídem | +0xef0 |

Grupos opcionales (GS:556-564): si un patrón no se encuentra, esa función online simplemente no existe.
`_scene_state` no usa `assert` sino un diccionario indexado por la instrucción (PA:636): una instrucción distinta da
`KeyError` en la generación. Además se modificó `_frame_wait` (PA:421-426) para llamar a `rt_online_frame`.

### 3.3 Las 47 funciones con dirección fija

| Nombre en el código | Dirección | Línea | Qué hace (según el comentario del fichero) |
|---|---|---|---|
| `FN_CREATE` | 0x7001535900 | OA:30 | crear actor ya, por nombre |
| `FN_SET_MTX` | 0x700152261c | OA:32 | poner la matriz de un actor (+0x398) |
| `FN_ACTOR_WAKE` | 0x7001515988 | OA:33 | despertar un actor dormido |
| `FN_DELETE` | 0x7001514bac | OA:34 | borrar actor |
| `FN_AS_IN_SLOT` | 0x70014a0040 | OA:35 | AS que suena en ranura/banco |
| `FN_PLAY_AS` | 0x700151e42c | OA:37 | "reproduce este AS" de una acción de IA (se le pasa una acción falsa, OA:272) |
| `FN_HIDE_GROUPS` | 0x700126ba4c | OA:40 | mostrar/ocultar grupos de materiales |
| `FN_FIND_BONE` | 0x7000f77260 | OA:45 | buscar hueso por nombre |
| `FN_REQUEST` | 0x7001535b10 | OA:49 | pedir actor (carga recursos antes) |
| `FN_HANDLE_TAKE` | 0x7001516b74 | OA:52 | recoger el actor de un handle |
| `FN_HANDLE_RELEASE` | 0x70015168a8 | OA:53 | soltar un handle fallido |
| `FN_ARMOR_BIND` | 0x700117363c | OA:54 | vestir armadura (código heredado) |
| `FN_ARMOR_INIT` | 0x7001173550 | OA:55 | **sin uso** |
| `FN_EFFECT_EMIT` | 0x7001100bd0 | OA:666 | emitir efecto por nombre (resultado por x8) |
| `FN_EFFECT_KILL` | 0x700056d010 | OA:667 | terminar efecto |
| `FN_XLINK_EMIT` | 0x700159f00c | OA:668 | emitir palabra clave XLink (efecto o sonido) |
| `FN_BONE_LOCAL_GET` | 0x7000f960f8 | OA:669 | leer matriz local de hueso |
| `FN_BONE_LOCAL_SET` | 0x7000f95fa8 | OA:670 | escribir matriz local de hueso |
| `FN_GDT_SET_F32` | 0x7000b36c80 | OA:974 | setter f32 de datos de juego |
| `FN_SEND_MSG` | 0x700154c61c | OA:1381 | enviar mensaje entre actores |
| `FN_XLINK_EFFECT` | 0x7001395354 | OA:1454 | efecto XLink por palabra clave |
| `FN_ACTOR_LOOK_ON` / `_OFF` | 0x700152a08c / 0x700152a0b4 | OA:1456-1457 | bits de aspecto en actor +0x710 |
| `FN_SPECIAL_FX_B` | 0x700124d220 | OA:1458 | `uk_special_effect_B` de los materiales de un modelo |
| `FN_MAGNE_BEAM_SET` | 0x7000d40bb8 | OA:1459 | colocar el haz de Magnesis |
| `FN_MAP_TYPE` / `FN_MAP_NAME` | 0x7001208304 / 0x70012082f8 | OA:1472-1473 | tipo y nombre del mapa |
| `FN_BODY_TO` | 0x70012d4e18 | OA:1490 | velocidades de un cuerpo rígido para llegar a una matriz |
| `FN_EVT_META_INIT` / `_NAME` / `_FREE` | 0x7001112508 / 0x7001112794 / 0x70011128b4 | OA:3694-3696 | metadatos de evento |
| `FN_EVT_CALL` | 0x700110d590 | OA:3697 | pedir un evento al gestor |
| `FN_GDT_HANDLE` | 0x7001143950 | OA:3744 | handle de un flag por crc32 |
| `FN_GDT_SET_BOOL` | 0x7000730148 | OA:3745 | setter bool |
| `FN_POUCH_CAN_GET` / `FN_POUCH_GET` | 0x700120af80 / 0x700120af98 | OA:3749-3750 | ¿cabe en la bolsa? / meter en la bolsa |
| `FN_GET_SPECIAL` | 0x7001211520 | OA:3763 | contenedor de corazón |
| `FN_EVT_START` | 0x700110d500 | OA:3810 | iniciar evento sobre un actor (Kolog) |
| `FN_SPECIAL_FX_A` | 0x700124ca00 | OA:4010 | **sin uso** |
| `FN_SPECIAL_FX_A_UNIT` | 0x700124ca7c | OA:4011 | `uk_special_effect_A` por unidad |
| `FN_TAKE_OUT` | 0x7001524598 | OA:5041 | retirar un objeto del mundo |
| `FN_MAPOBJ_GONE` | 0x7000bdc1f0 | OA:5042 | marcar el objeto de mapa como desaparecido |
| (en línea) | 0x700156f61c | OA:5803, 5825 | pausar/reanudar el sistema de física |
| (en línea) | 0x70015195d0 / 0x70015195e0 | OA:5804, 5826 | poner / limpiar bits de pausa de actores |
| (en línea) | 0x7001519720 | OA:5804, 5827 | avisar a los procesos del gestor |
| (en línea) | 0x7000e89490 | OA:5842 | activar la cámara del mundo |

### 3.4 Los 15 punteros de datos / GOT con dirección fija

| Nombre | Dirección | Línea | Qué es |
|---|---|---|---|
| `P_HIDE_GROUPS_X0` | 0x7001d66c98 | OA:44 | x0 de `FN_HIDE_GROUPS` |
| `P_SAFE_STRING_VT` | 0x7001d66260 | OA:48 | vtable de `sead::SafeString` (+0x10) |
| `P_CREATOR` | 0x7001d66550 | OA:56 | creador de actores |
| `P_HEAP_OWNER` | 0x7001d66558 | OA:57 | dueño del heap (+0x28) |
| (en línea) | 0x7001d664b0 | OA:571, 581 | GOT de la vtable de `InstParamPack` |
| `P_MAGNE` | 0x7001e15f18 | OA:1453 | objeto de la runa Magnesis (+0x318) |
| `P_MAGNE_FX` | 0x7001d66ab8 | OA:1455 | actor donde suenan los efectos de runa (+0x30) |
| `P_MAGNE_AIM` | 0x7001e58720 (+0x192cc) | OA:1460-1461 | puntería de la runa (se **escribe**) |
| `P_EVT_MGR` | 0x7001d66758 | OA:3698 | gestor de eventos |
| `P_GDT_MGR` | 0x7001d66478 | OA:3746 | gestor de datos de juego (+0xbd8, +0xc28) |
| `P_KOROK_EVT_MGR` | 0x7001e3a638 | OA:3809 | gestor de eventos (como lo lee el código del Kolog) |
| (en línea) | 0x7001e937f8 | OA:5780 | gestor de procesos de actor (+0x168 bits de pausa) |
| (en línea) | 0x7001d66ab0 | OA:5801, 5823 | sistema de física |
| (en línea) | 0x7001d66b80 | OA:5801, 5823 | escena de juego (+0x8b8 byte de pausa) |
| (en línea) | 0x7001d6b0b8 | OA:5835 | lista de cámaras (+0x40 cuenta, +0x48 array; cámara +0x90) |

### 3.5 Offsets de estructura principales (todos literales, 1.9.0)

| Estructura | Offsets usados |
|---|---|
| `PlayerInfo` (sitio `player_info`) | +0x60 actor jugador, +0x88 posición |
| Actor (cualquiera) | +0x10 nombre, +0x60 id, +0x64 estado (0 init, 1 calc, 2 dormido, 3 borrado), +0x68 unidad, +0x190 cuerpo rígido, +0x398 matriz 3×4, +0x418 escala (3 floats), +0x4d4 "enlazado a Link", +0x4e0 modelo, +0x558 (+0x148), +0x560 gestor de AS, +0x568 (+0x48 ELink, +0x50 SLink; instancia +0x70 propiedades), +0x570 (+0x148 → +0x2c0 datos de estela), +0x5f0 receptor de mensajes, **+0x640 HashId**, **+0x710 bits de aspecto**, **+0x848 vida (s32)** |
| Jugador | +0xb98 / +0xbb0 / +0xbc8 enlaces arma/escudo/arco (0x18 c/u), +0xea8 (solo doc), +0x1114/+0x1118/+0x111c tintes, +0x1cd4 cargas de Daruk, +0x2350/+0x2360 handles de Daruk, +0x23c0 handle de Revali, +0x2500 gestor de armadura (+0x10 enlaces, +0x128 jugador, +0xd8 transceptor), +0x2c28 paravela (escaneado) |
| Arma / arco | +0x958 enlace a vaina / carcaj |
| Enemigo | +0xb98 arma, +0xbb0 escudo (como el jugador) |
| Unidad de proceso (0x58 bytes) | +0 estado/vtable, +8 handle, +0x10/+0x18 lista doble, +0x40 id, +0x48 actor |
| Modelo gsys | +0x38 nº de unidades, +0x40 array, +0xd0 control de animación (+0x40, +0x48; ranura 0xa8; +0x20 fotograma) |
| Unidad de modelo | +0x118 esqueleto (+0x18 locales, +0x50 nº de huesos); vtable +0x70, +0xb8, +0x1b0, +0x208 |
| Gestor de AS | +0xb8 nº ranuras, +0xc0 ranuras (0x50 c/u, +0x20 bancos) |
| Gestor de mundo (sitio `world_mgr`) | +0x5c0, +0x5c8 (gestor de tiempo: +0xb8 hora, +0xc8 temporizador Luna de Sangre, +0x120 handle), +0x794, +0x7d1 clima, +0x7d6 |
| Gestor de datos de juego | +0xbd8 buffer (→ +8 cuenta, +0x10 array de proxies 0x18: +8 def (+0xc crc32), +0x10 valor), +0xc28 |
| Handle de efecto | +0x20 id, +0x98/+0x9a flags, +0xa8 punto, +0xb4 normal/escala, +0xbc..+0xc4 posición, +0xc0 matriz, +0x180 flags, +0x190 datos de usuario (+8 actor) |
| Objeto Magnesis | +0x318 parte magnética (+8 actor, +0x10 bit 0) |

### 3.6 Procedencia del conocimiento (lo que dicen los ficheros, sin valorar)

- DOC:3-6: el modo online se hace "after the idea of Milk Bar Launcher" (multijugador para Cemu/Wii U) y afirma que
  nada de ese código se traslada: "this is written anew for Savage Wind".
- DOC:99: "Read from the code (capstone, and Ghidra: `tools/ghidra/README.md`; no symbols, no RTTI in `main`)".
  OA:9: "1.9.0 (no symbols; read from the code)".
- Métodos de descubrimiento descritos: comparar la memoria del jugador antes y durante una habilidad (DOC:214-215,
  767-768; OA:1645-1646); contar llamadas con `SAVAGEWIND_FNPROF` con y sin menú (DOC:872-875, 911-913);
  `SAVAGEWIND_FNPROF_ARGS` para ver cómo el propio juego retira un árbol (DOC:948-949); escanear la memoria del
  jugador o de un enemigo buscando un enlace (OA:704-726, 2034-2056); leer `ELink2DB` de los ficheros del juego
  (DOC:350-353); probar parámetros (`SAVAGEWIND_ONLINE_ELEMENT_TEST`, DOC:635-638).
- Los comentarios usan nombres con espacios de nombres como `ksys::VFR::updateInterval`, `ksys::act::BaseProcMgr`,
  `ksys::world::Manager`, `ksys::gdt::Manager`, `uking::act::PlayerInfo`, `xlink2::UserInstance::emit`,
  `sead::SafeString`, `sead::FormatFixedSafeString<128>`, `ksys::PhysicsEntitryWorldPre`, y nombres de clase como
  `EventFlagONAction`, `EventIncreasePorchItemAction`, `ForkASPlay`, `PlayerBeamMove`, `TriggerParam`,
  `BaseProcLink`, `InstParamPack`. Los ficheros no dicen de dónde salen esos nombres. Recuento con `git grep`: la
  rama base ya contenía 24 apariciones de `ksys::` / `uking::` en 11 ficheros; con el PR son 35 en 14.
- No hay en estos ficheros ninguna mención a proyectos de descompilación. La regla del proyecto ("Do not copy from
  decompilation projects", `AGENTS.md`) no se cita en el PR.

### 3.7 Interfaz con la capa de red (`runtime/online.c` / `online.h`)

`online_actors.c` no abre sockets. Todo pasa por estas funciones (nombres tal como se llaman en OA):

| Dirección | Funciones |
|---|---|
| Juego → red (publicar lo local) | `online_set_local_pose`, `online_set_local_skeleton`, `online_set_local_equip`, `online_set_local_hour`, `online_set_local_enemies`, `online_set_loots`, `online_set_quests` |
| Juego → red (mensajes sueltos) | `online_send_hit`, `online_send_estates`, `online_send_loot_add`, `online_send_loot_take`, `online_send_pick`, `online_send_quests`, `online_leave` |
| Red → juego (instantánea) | `online_players` (copia 8 `OnlinePlayer`; se llama **8 veces por fotograma**: OA:5777, 5957, 5968, 5975, 5983, 6003, 6012 y en `rt_menu_draw_pause` OA:2820), `online_skeleton`, `online_enemies`, `online_loots`, `online_quests`, `online_host_hour` |
| Red → juego (colas que se vacían) | `online_take_hits`, `online_take_estates`, `online_take_loot_adds`, `online_take_loot_takes`, `online_take_picks`, `online_take_quests` |
| Estado de sesión | `online_state`, `online_local_id`, `online_is_host`; ajustes `g_online.collide / revive / nopause / pvp` |
| Juego → interfaz 2D | `online_has_actor(slot)` / `g_online_actor[]` (OA:202-204): el overlay no dibuja figura si hay actor |
| Render → red | `online_wants_camera`, `online_camera_set` (EX): cámara de la escena para las marcas del overlay |
| Juego → entrada | `g_online_block_a`, `g_game_buttons` (OA:5562-5563; `runtime/hle/input.c:1199-1206`) |

Saneado que hace NET al recibir (comprobado en `online.c:906-962`): termina en 0 todas las cadenas, limita
`shots.n` ≤ 8, `ice_n` ≤ 3, `anim` < `ONLINE_A_COUNT`, `n + nbow` ≤ 160 huesos, nº de enemigos ≤ 32, botín ≤ 64.
**No** comprueba que los floats sean finitos ni que los nombres de actor sean de un tipo permitido.

---

## 4. Cambios en el renderizador (`exec.c`, `shaders.c`)

Son dos cambios, ninguno relacionado con colorear jugadores:

1. **Cámara de la escena para el overlay** (EX, `fg_draw` y `vk_fg_generate`). La captura de cámara que usaba la
   generación de fotogramas (y DLSS/XeSS) ahora también corre cuando `online_wants_camera()` es cierto (sesión
   conectada y `show_markers`, `online.c:1483`). Cada fotograma se toma el candidato de cámara con más dibujos y se
   pasa su matriz vista-proyección a `online_camera_set`. Sirve para proyectar en pantalla los nombres y la flecha del
   borde para un jugador fuera de vista (DOC:26-27, 47).
2. **Imagen viva detrás de los menús** (`menu_capture_refresh`, EX; sombreador `menu_blur.frag`,
   `runtime/nvn/vulkan/shaders.c:1330-1352`). El juego muestra tras el menú una foto pequeña y borrosa tomada al
   abrirlo. Con "menús sin pausa" el mundo sigue, así que antes del primer dibujo que muestrea esa textura se rehace
   desde el fotograma en curso: reducción a mitades sucesivas con `blit` lineal, desenfoque gaussiano 13×13 con alfa 1
   y copia final. Heurística para reconocer la textura: destino 16:9, de dos tercios de pantalla o menos, al menos 64
   de ancho, que ninguna pasada ha dibujado desde el fotograma anterior, muestreada hacia un destino de 1280 o más de
   ancho; no en los 4 primeros fotogramas del menú (DOC:920-926). Se añaden 4 imágenes de posproceso (36-39).
   Notas de riesgo (solo lectura): se llama en **cada** `draw_`; cuando `rt_online_menu_live()` es cierto recorre las
   texturas del programa y llama a `vkGetPhysicalDeviceFormatProperties` dos veces por candidata; usa `getenv` dentro
   del bucle; corta la pasada en curso (`vk_end_pass`) y pone barreras completas.

**¿Hay ya un mecanismo de tinte/color?** No en el renderizador. Lo único parecido en todo el PR:

- `FN_SPECIAL_FX_B` (OA:1458) y `FN_SPECIAL_FX_A_UNIT` (OA:4011): escriben los parámetros de material
  `uk_special_effect_B` (brillo dorado de Magnesis / de Stasis) y `uk_special_effect_A` (pulso rojo de Stasis) de un
  modelo. Son efectos del propio juego, de valor escalar, no un color elegible.
- `model_visible()` (OA:356-369): muestra u oculta materiales por unidad.
- El tinte de armadura `ArmorDyeColor` (OA:6069-6077), que es el color de tinte del juego (índice), no un tinte por
  jugador, y además no está comprobado que se vea (DOC:1003-1008).
- El overlay 2D (otro agente) ya dibuja nombres y marcas; un color por jugador ahí no toca el renderizador del juego.

---

## 5. Todo lo intentado y fallido, cuelgues conocidos, pendientes y "Not done yet"

No hay marcadores `TODO` / `FIXME` en `online_actors.c`, `online_mod.py` ni `ONLINE.md` (comprobado con grep): lo
pendiente está escrito en prosa. Tipo: **F** = intentado y fallido / descartado; **C** = cuelgue del juego conocido;
**B** = fallo encontrado y ya rodeado (la causa sigue ahí si se toca el código); **P** = pendiente o no probado.

| # | Tipo | Qué | Referencia |
|---|---|---|---|
| 1 | F | `RemoteBombCube` como jugador: se dibuja, pero su IA y física lo dejan en las manos de Link | DOC:120 |
| 2 | F | `PauseMenuPlayer`: nunca se dibuja en el mundo | DOC:121 |
| 3 | C | `GameROMPlayer` como segundo jugador: el juego se cuelga justo después (acceso en 0x4e0) | DOC:122 |
| 4 | F | `Dm_Npc_Rito_HeroSoul_Kago` tal cual: queda dormido y su IA oculta el modelo | DOC:123 |
| 5 | F | `Item_Material_01`: creado pero no dibujado (modelo no cargado) | DOC:124 |
| 6 | F | Reproducir los AS del jugador por nombre en otro actor: eligen clip según el estado del jugador y salía mal; un AS sobre la marioneta posa los huesos por encima del esqueleto copiado | DOC:76-78, 247-249 |
| 7 | C | Vestir los `Armor_*` reales sobre el esqueleto remoto: "crashed some games (mixed sets, fast changes)" | DOC:147-148; OA:6060-6062 |
| 8 | F | Un segundo actor de la misma pieza para un jugador nunca se dibuja (probado con creación inmediata, con petición, con y sin parámetros "@P"/"@DC", quitada antes de borrar) | DOC:136-140; OA:338-342 |
| 9 | F | Una petición en curso no se puede cancelar (`0x70015168a8` solo suelta una fallida) | DOC:143-145 |
| 10 | F | Un actor por pieza de armadura reproduciendo los AS de Link: se desincroniza y solo caben nueve residentes | DOC:168-170 |
| 11 | C | El actor residente nº 59 cuelga el juego al arrancar (arrays fijos en 0x7000d49080) | DOC:90-91 |
| 12 | C | RSTB demasiado pequeño: `ResidentActors.byml` → cuelgue al arrancar; ActorLink de enemigo reescrito → cuelgue al analizar Tags | MOD:818-819, 544-546; DOC:578-580 |
| 13 | B | Sin `traverseDist` 1000 el juego borra equipo (~50 m), Link remoto (80-140 m) y marionetas, que se rehacían sin parar | DOC:198-203 |
| 14 | F | Leer `*(player+0xb98)` como array de ranuras de 0x58: era la unidad del enlace, no el equipo | DOC:132-133 |
| 15 | C | Emisión genérica `0x7000f3fc10` sobre una instancia SLink: cuelgue (escritura en 1) | DOC:256-257 |
| 16 | B | Sonidos continuos reproducidos enteros a volumen máximo: ráfagas de ruido blanco de 2,5-4 s | DOC:260-270; OA:1039-1067 |
| 17 | B | Efectos repetidos que se apilan (`Kago_Ready`) | DOC:271-272 |
| 18 | F | `Footprint` no se crea por su nombre en los otros juegos: no se envía | DOC:686-687, 1015 |
| 19 | B | Flechas de enemigos enviadas como propias (16 en una prueba sin filtros; 7 y 4 solo con distancia) | DOC:301-304; OA:1208-1214 |
| 20 | B | El escudo de Daruk dibujaba el viento de Revali (comparten handle) | DOC:216-218 |
| 21 | F | Magnesis poniendo solo la matriz del actor: el cuerpo físico lo deshace | DOC:325 |
| 22 | B | Actores de efectos de runa guardados bajo el mundo: el juego los borraba y ya no se dibujaban ("sometimes it doesn't show") | DOC:358-361; OA:4300-4303 |
| 23 | F | Haz de Magnesis: no eran las propiedades de ELink, ni los valores/handle del objeto de la runa, ni `MagneMarker*` / `MagneBall` / `MagneHit`, ni un `Item_Magnetglove` real, ni la acción de IA de la cámara, ni las palabras del sistema xlink, ni los registros de partículas | DOC:373-375 |
| 24 | B | Arma soltada y cogida con Magnesis enviada como "lanzada": la copia se iba al cielo | DOC:379-383 |
| 25 | C | Bit de Stasis en un `LinkTagAnd` (no es objeto físico): el otro juego le aplicó Stasis y se colgó | OA:67-72 |
| 26 | F | Cadenas de Stasis creadas pero no colocadas: nunca se veían | DOC:394 |
| 27 | F | Pilar de Cryonis real creado por alguien que no es la runa: se borra solo | DOC:399 |
| 28 | B | Drones de una Ira ocupaban los dos huecos de jefe; tras cuatro jefes el quinto compartía pista | DOC:422-424; OA:4206-4207, 4252-4253 |
| 29 | F | Escribir el valor de un flag solo en memoria: nada del juego lo ve (el HUD de la pizarra no aparecía) | DOC:437-438 |
| 30 | F | Flag de runa sin dar el objeto: las runas de la pizarra no cambian; la bolsa sola no sube los corazones; `IsGet_PlayerStole2` no indica nada | DOC:463-464, 469-470, 484-485 |
| 31 | C | Un evento lanzado fuera de contexto puede colgarse (`Demo022_0` en pantalla negra) | DOC:751-752; `docs/CONTROL.md` |
| 32 | B | Recogidas a mano libre no detectadas; lo que Link sostenía aparecía en el suelo; arco añadido en el aire; objeto creado donde estaba Link perdido al instante; un PICK borraba otra arma del botín | DOC:506-509, 532-541, 559-560 |
| 33 | B | Materiales en la mano a escala 1 (una seta como columna) o amontonados en un bulto | DOC:518-519 |
| 34 | F | Marioneta con controlador de personaje: se queda donde está y su arma se va flotando | DOC:576-577 |
| 35 | B | Propiedad de enemigos: ping-pong a ~40 m; enemigo dado a un jugador a más de 80 m desaparecía para todos; campamento que nunca se despejaba en el anfitrión | DOC:588-600 |
| 36 | B | Marionetas con químico: el enemigo real moría quemado sin nadie cerca | DOC:626-629 |
| 37 | F | Fuentes de elemento: `ExpandFireField` hace daño propio, `ExpandElectricField` mató a uno de 13 de vida, el hielo de las varas no congela, las flechas creadas en el sitio no hacen nada | DOC:636-638; OA:2247-2252 |
| 38 | F | `OnlineKnock` (derribo): se ve la explosión y quita un cuarto de corazón pase lo que pase; las ondas de los jefes solo hacen estremecerse | DOC:651-656 |
| 39 | F | Escala metida en la matriz del arma: no se dibuja (hay que escalar el hueso raíz) | DOC:617-618 |
| 40 | B | Golpe furtivo ×8 sobre marionetas y sobre enemigos recién traspasados (mató a dos Bokoblin) | DOC:795-804 |
| 41 | F | Objetos del mundo: moverlos cada 3 fotogramas los pasaba de largo y los rompía; "del jugador más cercano" y "del primero que lo envía" como regla de propiedad fallaron; límite de 20 m dejaba troncos sin moverse | DOC:826, 853-857 |
| 42 | F | Sin pausa: limpiar solo los bits dejaba a los actores quietos; con los bits limpios lo que se saca de la bolsa se queda colgado en el aire; parar solo con X no bastaba | DOC:873-875, 898-909 |
| 43 | B | Destello oscuro si la foto del menú se rehace en sus 4 primeros fotogramas | DOC:925-926; OA:2801-2802 |
| 44 | F | Árbol dormido: borrado, oculto o hundido seguía dibujándose de pie junto a su tocón; hay que despertarlo y esperar 0,4 s | DOC:940-945; OA:5055-5058 |
| 45 | B | Cada juego devolvía al otro su propia tala: pila de tocones y troncos | DOC:959-960; OA:5324-5327 |
| 46 | F | Un mecanismo dormido movido a mano: el juego lo crea de nuevo | DOC:985 |
| 47 | C | Rehacer los actores durante el game over colgaba el juego | DOC:732-734; OA:5871-5872 |
| 48 | B | Con el menú propio abierto Link no recibe golpes: se aplican por el runtime | DOC:884-887 |
| 49 | B | Búfer del bot de 1 100 bytes con un STATE de 2 969: el anfitrión lo veía cargando para siempre | DOC:333-334 |
| 50 | P | Enviar menos: solo los huesos que cambian o menos veces por segundo | DOC:1001 |
| 51 | P | Color del tinte en pantalla ajena: no visto; si no, "the colour has to go on its materials" | DOC:1003-1008 |
| 52 | P | Cuerpos físicos para las marionetas de jugador (subirse a uno, que les afecten los golpes del mundo) | DOC:1009-1010 |
| 53 | P | Haz de la Espada Maestra: `beam_move` y `OnlineBeam` sin probar | DOC:1011-1012 |
| 54 | P | Bestia Divina vista en movimiento desde fuera; partes `Remains*` no probadas dentro | DOC:1013-1014, 985-986 |
| 55 | P | Huellas, hierba cortada; reacciones del agua no vistas | DOC:1015-1016, 278-279 |
| 56 | P | "The world shared: enemies, items, time of day, quests ...; damage between players" (entrada antigua que el propio documento ya describe como hecha más arriba) | DOC:1017 |
| 57 | P | No probado: montar a caballo de verdad; lanzar materiales; insectos; mena o caja rota por otro; ascensor montado | DOC:676, 525-526, 546, 984-986 |
| 58 | P | Frase "No defence from his armour yet", que la sección siguiente contradice (ya se resta la defensa) | DOC:656 frente a DOC:665-667 |
| 59 | P | Cruzar de mapa por control remoto: `teleport` / `goto` solo dentro de un mapa; un santuario o Bestia "needs walking in" | DOC:747-748 |

---

## 6. Revisión de riesgo de cuelgue de `online_actors.c`

Solo lo comprobado leyendo el código. Gravedad: **ALTA** = puede colgar el juego en uso normal de dos jugadores o con
un compañero con datos incorrectos; **MEDIA** = necesita una condición poco frecuente; **BAJA** = improbable o solo
degrada.

### 6.1 Gravedad ALTA

**A1. Sin puerta de versión para 62 direcciones fijas.** `online_actors_on()` (OA:225-229) solo comprueba que existe
`online_mod.txt` en el overlay (OA:208-221). A partir de ahí `rt_online_frame` llama a funciones en direcciones de
1.9.0 (OA:30-57 y resto de §3.3) sin mirar `g_game_version`. En otra versión del juego, con el overlay presente, se
salta a código arbitrario en cuanto hay mundo y sesión (por ejemplo `local_map` → `FN_MAP_TYPE`, OA:1476, se ejecuta
en cada muestreo).

**A2. `FN_BODY_TO` sobre actores sin comprobar que tienen cuerpo físico.** El propio fichero explica (OA:67-72) que
en clases que no son objetos físicos el campo +0x190 es otra cosa ("a heap pointer to a heap pointer") y que eso ya
causó un cuelgue. `has_body()` se usa en Magnesis, Stasis y objetos, pero **no** en:

- `movers_update` (OA:4928-4929; también OA:4901-4902 en el gancho de prueba): los mecanismos se eligen por HashId y
  porque el nombre contiene `Lift`, `Door`, `Move`, `Floor`, `Turn`, `Fan`... (OA:4807-4819), y basta que +0x190 pase
  `VALID()` para llamar a `FN_BODY_TO` con una matriz que llega de la red.
- troncos en `objs_update` (OA:4611-4627, 4727-4738): entran sin `has_body`.
- `grab_update` tras encontrar el objeto en el botín (OA:4369-4381 → 4405-4406).

Es el camino más probable de cuelgue en santuarios y Bestias Divinas, justo las zonas "no probadas".

**A3. Escrituras en campos de clase sobre actores elegidos por nombre o HashId que llegan de la red.**

- `break_here` escribe `vida = 0` en `actor+0x848` de cualquier actor "rompible" cuyo HashId esté en la lista `broken`
  de otro jugador (OA:5178-5180); "rompible" es una lista de prefijos de nombre (OA:5025-5033). Si la clase de ese
  actor no guarda la vida ahí, se pisa otro campo.
- `FN_ACTOR_LOOK_ON(actor, 2)` en Magnesis (OA:4398) y `stasis_look` (OA:4014) fijan bits en +0x710.
- La escala en `actor+0x418` (OA:6354) es sobre actores propios: sin riesgo.

**A4. Creación de actores con nombres que vienen de la red, sin lista blanca.**

- Botín: `request(c, list[j].name, ...)` (OA:3604). El nombre lo pone cualquier cliente con LOOT_ADD y el anfitrión
  lo reenvía (`loot_host_add`, OA:3356-3363; `online.c` M_LOOT_ADD solo termina la cadena). No se comprueba
  `is_loot()` al recibir.
- Enemigos: `request_p(c, e->name, ...)` (OA:2226) con el nombre del registro, que en el anfitrión se sobrescribe con
  el ESTATE de un cliente (OA:2398). Tampoco se comprueba `is_enemy()`.

Un nombre como `GameROMPlayer` es un cuelgue documentado (DOC:122). Entre amigos con el mismo build es improbable; con
versiones distintas del overlay o un cliente defectuoso, no.

**A5. Uso del Link remoto antes de comprobar que sigue vivo en ese fotograma.** El orden en `rt_online_frame` es:
`players_collide` (OA:5961) → `grab_update` / `stop_update` (OA:5976-5977) → ... → bucle de jugadores, donde por fin
se hace `alive(g_act[i].link, ...)` (OA:6036). Antes de esa comprobación se llama al juego con el puntero guardado:
`bone_mtx(c, g_act[i].link, "Weapon_R", ...)` (OA:4323, 4437, 4439), lectura de `link+0x398` (OA:4307, 4326) y
`FN_SET_MTX` de las piezas de runa. Si el juego borró la marioneta desde el fotograma anterior (una carga que el
criterio de "salto de 100 m" no detectó, o un game over), `FN_FIND_BONE` recibe el modelo de un actor liberado.
El propio documento dice que tocar actores durante el game over colgaba (DOC:732-734).

### 6.2 Gravedad MEDIA

**M1. Ningún `RT_ON_RESET`.** `grep` no encuentra ningún uso en `online_actors.c` ni en `online.c`, pese a la regla
de `AGENTS.md` ("Reset per-boot state with RT_ON_RESET; do not retain guest pointers across reboots") y a que
`runtime/hle.h:28-31` dice que en un reinicio en caliente la memoria del huésped se borra. El fichero tiene ~85
variables globales estáticas más decenas de estáticas dentro de funciones con punteros del huésped: `tr[]` de
`sample_shots` y `sample_held` (OA:1184, 1312), `held/thrown` (OA:1517), `stopped` (OA:1677), `horse/tack` (OA:1760),
`g_obj_seen`, `g_brk`, `g_ground`, `g_boss`, `g_btrack`, `g_cutting`, `g_breaking`, `g_spawn`, `g_their_trunk`,
`g_our_cut`, `g_esrc`, `g_rf`, `g_world[].a`, y los offsets cacheados. `forget_all()` (OA:316-336) solo limpia
`g_act`, pool, marionetas, propios, botín, explosiones y mecanismos. Casi todo pasa por `alive()`, que lee
`unidad+0x40` de un puntero antiguo (lectura de memoria borrada o reutilizada, no necesariamente fallo), pero hay
desreferencias directas del estado de la unidad de peticiones pendientes — `*unit == 2` en huérfanos (OA:300-301),
pool (OA:408-410), `g_spawn` (OA:5244-5252), `g_boom` (OA:1405-1413) — que tras un reinicio apuntan a memoria de otro
arranque y pueden acabar en `FN_HANDLE_TAKE` sobre basura. También sobrevive estado lógico: `slate_started`,
`korok_played`, `had` / `held_off`, `tested`, `told`.

**M2. Anillo de handles y handles pendientes.** `new_handle()` recicla 4 096 entradas (OA:110-117) sin saber si la
anterior sigue pendiente; la unidad del juego guarda la dirección del handle en +8 (OA:107-108). `delete_piece()`
llama a `FN_HANDLE_RELEASE` sobre una petición **en curso** (OA:348), cuando la documentación dice que eso solo vale
para una fallida (DOC:143-145), y luego olvida la pieza: el actor que llegue queda suelto en el mundo. Ocurre, por
ejemplo, al desaparecer un pilar de hielo (OA:4150) o al soltar un enemigo propio que aún se estaba creando
(OA:2176). `g_orphan` tiene 128 entradas y descarta en silencio la 129.ª (OA:290-294).

**M3. Vida de los `InstParamPack` fabricados a mano.** Para la armadura se usa **una sola** `static InstParams dp`
que se pone a cero y se reescribe para cada pieza y cada jugador en cada fotograma (OA:6069-6076). Para los enemigos,
`Own.params` vive dentro de `g_own[]`, que `own_drop` y `owns_forget` ponen a cero aunque la petición siga pendiente
(OA:2176-2177, 2324-2329). Si el juego lee esos parámetros cuando el actor se crea de verdad (tras cargar recursos) y
no en el momento de pedirlo, leería un paquete con vtable 0. **No verificado** cuál de las dos cosas hace el juego;
hay que mirarlo en Ghidra.

**M4. Floats de la red sin comprobar.** Se comprueba `isfinite` en objetos (OA:4683-4685), mecanismos (OA:4913-4915),
caballo (OA:6323-6324), arreos, equipo en modo mundo (OA:6289-6291) y materiales (OA:6356-6357). **No** se comprueba
en: posición y orientación del jugador → `FN_SET_MTX` (OA:6054-6056); matrices relativas de equipo sobre hueso
(OA:6287); matrices de disparos (OA:6397); matrices de enemigos (OA:2235, 3087) y de sus armas (OA:3067-3070);
pilares de hielo (OA:4157); Magnesis y Stasis → `FN_BODY_TO` (OA:4406, 4106); posición de botín (OA:3616-3623, solo
la rotación); posición de explosiones (OA:6457). Tampoco se limita la magnitud: un `FN_BODY_TO` a 1e9 m es válido
para `isfinite`.

**M5. Hilos.**

- `rt_online_xlink_emit` (OA:868-898) y `rt_online_arrow` (OA:820-825) corren en cualquier hilo del juego. El índice
  es atómico, pero la entrada del anillo (`name` + `who`, o `name` + `pos` + `props`) se escribe sin protección y el
  hilo principal puede leerla a medias; además lee `g_rf + 0x398` y copia 64 bytes de propiedades de un actor que el
  hilo principal puede estar dando por muerto (OA:874-878). `rt_online_arrow` desreferencia `action + 8` sin validar
  `action` (OA:821).
- `rt_menu_draw_pause` (OA:2812-2824) y `rt_online_has_fairy` (OA:2825-2831) se ejecutan desde código del juego (el
  fotograma de los menús, la muerte de Link) y hacen `online_players()` (copia de 8 `OnlinePlayer` en la pila) y `LOG`.
- `rt_blur_anim` / `rt_blur_frame` (OA:935-945) leen `g_blur` desde los callbacks del efecto mientras el hilo
  principal lo reescribe (OA:6257-6261): `volatile`, no atómico.
- `call_f` / `call_sret` guardan el contexto en una `static Cpu saved` (OA:234, 248): no reentrante. Hoy solo se usan
  desde el hilo principal; cualquier llamada futura desde otro hilo lo rompe.
- Los comandos de control (`actors`, `flag`, `event`) están bien hechos: el hilo de control deja una petición y el
  hilo principal responde (OA:5565-5705).

**M6. Supuesto de tamaño de actor.** Se leen `+0x4d4`, `+0x640`, `+0x710` y `+0x848` de **todos** los actores de la
lista (hasta 8 192) sin saber su clase (OA:1320, 1690, 3442, 4194-4195, 4592, 4841, 5206, 5351, 5383). Son lecturas
dentro del heap, así que el riesgo directo es bajo; el peligro es la identificación errónea que lleva a las
escrituras de A2/A3. Los dos escaneos de 0x100 a 0x4000 bytes dentro de un actor (OA:715-724, 2045-2054) leen hasta
16 KB más allá del inicio de un enemigo o del jugador.

**M7. Flags: efectos irreversibles y falsa Luna de Sangre.** No es un cuelgue, pero puede estropear guardados:
(a) unión de flags con entrega de objetos (ver §2.2); (b) `quests_update` compara los flags "de reaparición" que
tenía con los que tiene y, si faltan 64 o más, declara Luna de Sangre (OA:3913-3924). Cargar una partida más antigua
en mitad de la sesión produce exactamente eso, y `had` es estática (no se reinicia; ver M1).

**M8. `start_event` libera los metadatos justo después de pedir el evento** (OA:3703-3713) y usa un búfer y una
petición **estáticos** compartidos; si el gestor de eventos conserva el puntero más allá de la llamada, queda
colgando. Se imita lo que hace un `EventTag` (OA:3690-3693); no verificado más allá de eso. Un evento fuera de
contexto ya se sabe que puede colgar (tabla §5, nº 31).

### 6.3 Gravedad BAJA

- `g_actors[8192]` es un único búfer compartido por todas las funciones que llaman a `all_actors` (OA:853-854); en
  `loot_update` se reutiliza entre bloques con la bandera `scanned` (OA:3373-3438). Reordenar llamadas puede hacer
  que un bloque lea la lista de otro.
- Marionetas de enemigo, botín, fuentes de elemento y pruebas usan `piece_update(c, 0, ...)`: comparten el **pool del
  jugador 0** (OA:2231, 3044, 3058, 2281). `drop(c, 0, true)` les borra piezas; pool lleno (24) borra la entrada 0
  (OA:385).
- `sample_pose` desreferencia `*(mgr + 0xc0)` sin `VALID` (OA:1503).
- 63 llamadas a `getenv` en el fichero, varias dentro de bucles por fotograma (OA:1132, 3061, 3082, 5830, 5881, 5897,
  6140, 6213, 6250, 6313, 6398): coste, no fallo.
- Hasta ocho copias de `OnlinePlayer[8]` por fotograma, tomadas en instantes distintos: los subsistemas de un mismo
  fotograma pueden ver instantáneas diferentes.
- `korok_event` hace `strrchr(flag, '_') + 1` (OA:3816): seguro solo porque el prefijo ya contiene `_`.
- La regla de reanimación y la de menús sin pausa se refrescan dentro de `players_collide`, después de un `return`
  temprano (OA:2837-2839): con `SAVAGEWIND_ONLINE_NO_COLLIDE` o durante un evento quedan con el valor anterior.
- 45 variables de entorno `SAVAGEWIND_ONLINE_*` de prueba compiladas en el binario de release; algunas matan a Link o
  a los enemigos (`_DIE_TEST`, `_KILL_TEST`, `_BOSS_KILL`).

---

## 7. Lista de huecos frente a nuestros objetivos

Estado: **EXISTE** / **PARCIAL** / **FALTA**. Dificultad: baja / media / alta. "Ghidra" = necesita ingeniería inversa
nueva.

| Objetivo | Estado | Qué hay | Qué falta | Dificultad | ¿Ghidra? |
|---|---|---|---|---|---|
| Co-op de 2 jugadores, mundo del anfitrión compartido | PARCIAL | Enemigos, botín, objetos, roturas, hora, clima y Luna de Sangre tienen autoridad o lista en el anfitrión. Sesión de hasta 8 | Los **flags son una unión**, no los del anfitrión: hay que decidir y aplicar una política (anfitrión manda; el invitado no vuelca su partida). Reducir de 8 a 2 simplifica la propiedad de enemigos | media | No (lógica en `quests_update`) |
| Inventario / corazones / resistencia propios | PARCIAL | Cada juego tiene su guardado; el botín recogido es de quien lo recoge | Hoy se **regalan** a todos: orbes, contenedores de corazón de Bestias, runas, semillas Kolog, paravela (OA:3987-3997). Hay que separar "progreso del mundo" de "recompensa personal". Nada trata la resistencia | media | No para quitar regalos; sí si se quiere dar recompensas de otra forma |
| Estabilidad | PARCIAL | Mucho endurecimiento empírico (watch/alive, pool, huérfanos, esperas tras carga y muerte) | A1-A5 y M1-M5 de §6: puerta de versión, `has_body` en mecanismos, lista blanca de nombres, comprobar `alive` antes de usar, `RT_ON_RESET`, floats finitos y acotados | media (mucho trabajo mecánico) | Solo M3 y M8 |
| Tinte de color por jugador | FALTA | Nada en el render. Hay setters de parámetros de material del juego (`uk_special_effect_A/B`) y visibilidad por material | (a) Color en nombre/marca del overlay 2D. (b) Tinte del modelo 3D: encontrar un parámetro de material de color o intervenir en el sombreador; el render no sabe a qué actor pertenece un dibujo | (a) baja; (b) alta | (b) sí |
| Teletransporte al compañero | FALTA | Piezas: posición y mapa del compañero (`OnlinePlayer.pos`, `pose.map`); `control_cell_player_pos` (lo usa `players_collide`, OA:2868); comandos `teleport` / `goto` y sitio `tp_gate` en la base (GS:565) | Acción de usuario + llamada. **Entre mapas** (santuario, Bestia) no hay nada: "needs walking in" (DOC:747-748) | baja (mismo mapa) / alta (otro mapa) | Solo para otro mapa |
| Enviar / soltar objetos al compañero | PARCIAL | Soltar de la bolsa ya lo comparte: cae, 0,4 s quieto → lista de botín → el otro lo recoge (OA:3430-3488). `give_item()` mete objetos en la bolsa (OA:3751-3759) | Envío directo: mensaje nuevo + `give_item` en destino + **quitar** del origen (no hay función de retirada en este PR; la base tiene sitios `pouch_item_get`, `pouch_mgr`, GS:566). Lista de botín limitada a 64; de un arma solo viaja el nombre (no durabilidad ni modificadores) | media | Sí (quitar de la bolsa, durabilidad y modificadores) |
| Reanimar | PARCIAL | Auto-reanimación con hada (`has_fairy`) según regla del anfitrión | Estado "caído" esperando al compañero, temporizador, interacción de reanimar, qué pasa si caen los dos | media-alta | Sí (retener la muerte sin game over) |
| Pings / marcadores | FALTA | El overlay ya proyecta nombres y flecha de fuera de pantalla con la cámara capturada en `exec.c` | Mensaje de ping + dibujo en el overlay con esa misma proyección; convertir "donde apunto" en un punto del mundo; opcional: pin en el mapa del juego | baja-media (overlay) / alta (mapa del juego) | Solo para el mapa del juego o trazado de rayo |
| Accesibilidad | FALTA | Nada específico en estos ficheros. Avisos importantes van solo al registro (por ejemplo "A held back", OA:5999) | Avisos en pantalla y sonoros de eventos de red, tamaño y contraste de marcas, no depender solo del color, textos traducidos (existe `runtime/ui/i18n/es.inc`) | baja-media | No |
| PvP opcional | EXISTE (básico) | Interruptor por jugador, solo si ambos lo activan; cuerpo a cuerpo por cono y alcance; bombas; bloqueo con escudo; defensa de armadura | Flechas, daño de runas, derribo real, validación del daño declarado, indicador de quién tiene PvP activo | media | No para lo básico; sí para golpes con colisión real |
| Momentos de historia con los dos presentes | FALTA (hay piezas) | Bit `ONLINE_E_DEMO` + bloqueo del botón A a 30 m (solo pizarra y torres, OA:1625-1633, 5981-6001); `start_event()` para lanzar un evento en el otro juego; `framepace_in_event` / `framepace_event_name`; flags compartidos | Tabla de eventos "que exigen a los dos"; puerta "compañero a menos de X m y en el mismo mapa" antes de dejar empezar; arranque sincronizado en ambos; qué hacer con eventos que arrancan por zona y no por botón | alta | Sí (interceptar el arranque de eventos en general) |
| Santuarios y Bestias Divinas juntos | PARCIAL | Se ven si están en el mismo mapa; objetos y mecanismos se envían | No probado (DOC:984-986, 1013-1014); riesgo A2; el rompecabezas no está en flags | alta | Probablemente |
| Otras versiones del juego | FALTA | 16 sitios por patrón | 62 direcciones y ~67 offsets fijos de 1.9.0 | alta | Sí, todo |

---

## 8. Descomposición propuesta de `online_actors.c`

### 8.1 Módulos e interfaces

Propuesta de 11 ficheros con una cabecera interna común (`online_actors_internal.h`). Entre paréntesis, las líneas
actuales de las que sale cada uno.

| Módulo | Contenido | Interfaz que expone | Depende de |
|---|---|---|---|
| `oa_core` | ABI de llamadas al huésped (`call`, `call_f`, `call_sret`, OA:231-259); `VALID`, `IN_IMAGE`, `has_body`; `Watch` / `alive` / `alive_rewatch`; anillo de handles; `Piece`, `piece_update`, pool, huérfanos, `delete_piece`, `hide_piece` (OA:84-126, 280-439, 599-642); `create` / `request_p`; `InstParams`; `all_actors`, `actor_name_of`, `linked`, `running`; `bone_mtx`, `skel_unit`, `first_unit`, `model_visible`; `playing` / `play_as`; utilidades de matrices; carga de `*.txt` (`map_load`, `gear_actor`, `armor_actor`, `enemy_actor`, `attack_power`, `weapon_scale`); **todas** las direcciones y offsets | Las funciones citadas + un registro `oa_register_forget(fn)` para que cada módulo aporte su limpieza | sitios, `rt.h` |
| `oa_gamedata` | `crc32_str`, `gdt_value`, `bool_flag`, `set_bool_flag`, `give_item`, `give_heart`, `start_event` (OA:3656-3769, 3794-3804) | Esas funciones | `oa_core` |
| `oa_sample` | Lado local: `sample_pose`, `sample_skeleton`, `sample_shots`, `sample_held`, `fx_update`, `boss_sample`, `magne_held`, `local_map` / `same_map`, y los ganchos de parche `rt_online_arrow`, `rt_online_bomb`, `rt_online_xlink_emit`, `rt_game_fmt` (OA:798-945 en parte, 1026-1841, 1871-1893, 4181-4249) | `oa_sample_frame(c, pinfo)`; lecturas: `oa_local_dead()`, `oa_map()`, `oa_swing_time()` | `oa_core`; hoy recoge datos de casi todos los demás (ver 8.2) |
| `oa_puppet` | Lado remoto del jugador: Link, `pose_link`, `pose_armor`, `cover`, `animate`, equipo, caballo, materiales, disparos, flecha en la cuerda, explosiones (`boom_*`), efectos por evento, Daruk/Revali/estela y `rt_blur_*` (OA:1377-1447, 1895-1985, 6011-6468) | `oa_puppets_frame(c, pl, me, link)`; `oa_puppet_link(i)` (actor o 0, **ya validado**) | `oa_core`, `oa_loot` (arma lanzada) |
| `oa_runes` | `grab_update`, `stop_update`, `ice_update`, `rune_keep`, `rune_pieces`, `stasis_look`, `chains_place`, `fx_on` / `fx_end` (OA:4008-4162, 4290-4498) | `oa_runes_frame(...)`; `oa_rune_holds(actor)` (¿lo tiene otro con Magnesis o Stasis?), `oa_rune_released(i)` | `oa_core`, `oa_puppet`, `oa_loot` |
| `oa_enemies` | Registro del anfitrión, propietarios, marionetas, `own_*`, golpes, elementos, `strike`, `own_strikes_in_menu`, `mark_dead`, jefes `boss_update` (OA:1987-2778, 2944-3235, 4251-4288) | `oa_enemies_frame(...)`; `oa_own_iter()` (enemigos propios vivos, para botín y suelo) | `oa_core`, `oa_gamedata` |
| `oa_loot` | `loot_update` (muertes, caídas, lista), `ground_update` (PICK), `drop_hush` (OA:3237-3649, 5458-5560) | `oa_loot_frame(...)`; `oa_loot_is(actor)`, `oa_loot_thrown(actor, name)`, `oa_loot_of(origin, name)`, `oa_drop_hush(pos)` | `oa_core`, `oa_enemies` |
| `oa_world` | `objs_update`, `movers_update`, `breaks_update`, lista de sesión, troncos, `take_out`, `cut_later`, `spawn_at` (OA:4510-5456) | `oa_world_frame(...)`; salidas para la pose (`objs`, `movers`, `broken`, `trunks_gone`) | `oa_core`, `oa_gamedata`, `oa_runes`, `oa_loot` |
| `oa_flags` | `quests_update`, Kolog, pizarra, temporizador de Luna de Sangre, clima, hora (OA:947-1024, 3718-3738, 3771-4000, 5851-5864) | `oa_flags_frame(...)` | `oa_core`, `oa_gamedata` |
| `oa_rules` | Colisión, PvP, hada, menús sin pausa (`rt_scene_state`, `rt_menu_draw_pause`, `rt_online_menu_live` y el bloque OA:5767-5845), bloqueo de A (OA:2780-2942, 5981-6001) | `oa_rules_frame(...)` + los `rt_*` | `oa_core`, `oa_puppet`, `oa_enemies` |
| `oa_frame` + `oa_control` | `rt_online_frame` como orquestador con el **orden** explícito, `forget_all`, detección de carga y muerte; comandos de control y ganchos de prueba (OA:5565-5764, 5865-5945) | `rt_online_frame`, `online_actors_control_init` | todos |

### 8.2 Qué está demasiado enredado para separarlo sin riesgo

1. **`g_act[]` (OA:131-180)** es una sola estructura por jugador que usan cinco áreas: marioneta, runas (`grab`,
   `stop`, `released`, piezas de runa, `holder`), hielo, objetos del mundo (lee `grab`, `stop`, `released`;
   OA:4698, 4756), colisión (OA:2852) y `g_blur`. Hay que partirla por dueño antes de paralelizar.
2. **Ciclo de vida de piezas**: `Piece` + pool + huérfanos + `forget_all` + `drop`. `forget_all` conoce a mano las
   piezas de todos (OA:316-336) y aun así se deja fuera `g_obj_seen`, `g_brk`, `g_ground`, `g_boss`, etc. (§6, M1).
   Debe ser un solo dueño (`oa_core`) con registro de limpieza.
3. **El orden dentro de `rt_online_frame`** es parte del comportamiento: muestreo → reglas → botín → flags → mundo →
   runas → enemigos → marionetas, con retornos tempranos (`g_settle`, muerto) en medio (OA:5870-5880) y el fallo A5
   nacido precisamente de ese orden.
4. **Enemigos + botín + suelo** comparten `g_own[]` (OA:3375-3402, 5500-5506), el poder de ataque y la lógica de
   "quién lleva qué". Mejor tratarlos como un bloque en una primera fase.
5. **Objetos + roturas + troncos**: `trunks_near` / `trunk_made_t` (OA:4998-5016), `g_our_cut`, `g_their_trunk` y
   `drop_hush` cruzan `objs_update`, `breaks_update` y `loot_update`.
6. **`sample_pose` es un embudo**: copia a la pose salidas de casi todos (`g_objs_out`, `g_movers_out`, `g_broken`,
   `g_trunks_gone`, `g_pvp`, `g_aggro_*`, `g_fxev`, `g_fx_trail`, `g_rf_props`; OA:1741-1757). La estructura
   `OnlinePose` es además el contrato con la capa de red: cualquier campo nuevo toca protocolo (versión 38).
7. **Estado compartido suelto**: `g_actors` (búfer de trabajo único), `g_map`, `g_local_dead`, `g_dist`,
   `g_piece_params` (parámetro implícito de `piece_update`, OA:596, 6074-6076), `g_blast` (marioneta → PvP),
   `g_fx_trail_t` (efectos → PvP), `g_carried_now` (botín → marioneta), `g_theirs` (botín → runas y marioneta), pool
   del jugador 0 (enemigos, botín y pruebas).

### 8.3 Orden de trabajo recomendado

1. **Fase 0, en serie, un solo worktree, sin cambio de comportamiento**: extraer `oa_core` y `oa_gamedata`, la
   cabecera interna, partir `g_act`, y convertir `rt_online_frame` en una lista de llamadas con el orden escrito.
   Incluir aquí los arreglos transversales: puerta de versión (A1), `RT_ON_RESET` (M1), validación única de entradas
   de red (A4, M4), y "marioneta validada" antes de cualquier uso (A5).
2. **Fase 1, en paralelo** (un worktree cada uno), solo cuando la fase 0 esté integrada:
   `oa_flags` (política de flags del anfitrión, recompensas personales) · `oa_rules` (reanimar, PvP, teletransporte)
   · `oa_world` (con `has_body` en mecanismos, A2/A3) · `oa_enemies` + `oa_loot` juntos · `oa_puppet` + `oa_runes`
   juntos.
3. **Fuera de este fichero**, en paralelo desde el principio: overlay 2D (color de nombre, pings, accesibilidad) y
   `tools/online_mod.py` (no comparte código C; su contrato son los `.txt` y los nombres `Online*`).

Regla práctica para los worktrees: cualquier cambio en `OnlinePose` / `OnlinePlayer` (`online.h`) o en la versión de
protocolo lo coordina una sola persona, porque lo tocan a la vez este lado y la capa de red.
