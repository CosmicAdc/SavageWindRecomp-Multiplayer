# CU-09 · Enviar o soltar objetos para el compañero

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Un jugador puede dar cosas al otro soltándolas en el suelo **marcadas para el compañero**, que las recoge. En la
primera entrega solo se dan así **materiales y comida**; las armas, arcos y escudos quedan excluidos hasta que su
durabilidad y sus bonificaciones viajen por la red (ADR-007, decisión 10; ADR-009, decisiones 24 y 25). El envío
directo a la bolsa desde un menú queda para después. Soltar y recoger ya funciona en el PR; el marcado y el envío
directo no existen.

## Actores

- **Emisor**: quien da el objeto.
- **Receptor**: quien lo recibe.
- **Juego del anfitrión**: mantiene la lista de botín compartido.

## Precondiciones

- Sesión conectada y estable (CU-01, CU-03).
- Para soltar y recoger: los dos en el mismo mapa y a menos de 150 m del objeto (OA:3600-3632).
- Para el envío directo: el receptor está "en juego" (no en menú, escena ni carga), porque la entrega a la bolsa solo
  corre ahí (`rt_item_tick`, `runtime/items.c:420`; mapa 03 §1.3).

## Flujo principal

**A. Soltar marcado para el compañero (primera entrega; soltar y recoger existe, el marcado falta)**

1. El emisor elige un **material o una comida** de su bolsa y lo suelta "para <nombre>".
2. El objeto cae al suelo. Tras 0,4 s quieto entra en la lista de botín compartido, marcado para el compañero.
3. El receptor ve el mismo objeto en su mundo, en el mismo sitio, con un marcador en pantalla (CU-10).
4. El receptor lo recoge. Desaparece para los dos y queda en la bolsa del receptor.

**B. Enviar directamente (falta; después de la primera entrega)**

1. El emisor abre "Enviar objeto a <nombre>" en la pestaña Online y elige objeto y cantidad.
2. El objeto sale de la bolsa del emisor.
3. El receptor ve "<nombre> te ha enviado: <objeto> ×N" y el objeto aparece en su bolsa.
4. El emisor ve "Entregado".

## Flujos alternativos y de error

| Situación | Comportamiento esperado | Hoy |
|---|---|---|
| Bolsa del receptor llena | Objeto soltado marcado (primera entrega): **se queda en el suelo**, con aviso (**[DUEÑO]** ADR-011, decisión 38). Envío directo (después): `[PROPUESTA]` se **devuelve** al emisor con aviso; nada se pierde; **pendiente de confirmar con el dueño** (la decisión 38 no dice qué pasa en el envío directo) | `give_item` registra "doesn't fit in the pouch" y no entrega (OA:3751-3757) |
| Receptor en menú, escena o cargando | La entrega espera en cola hasta que vuelva al juego; si pasa el plazo, se devuelve | La cola de `items_request` espera sola (mapa 03 §1.3) |
| Receptor en otro mapa | Envío directo: permitido. Soltar: no ve el objeto | Botín solo a menos de 150 m en el mismo mundo |
| Receptor muerto o caído | El envío espera a que reviva | — |
| Desconexión entre "quitar" y "entregar" | El objeto vuelve al emisor: nunca se duplica ni se pierde | — |
| Lista de botín llena (64) | Lo nuevo se queda solo en el juego de quien lo soltó | OA:3359-3360 |
| Objeto protegido (Espada Maestra, objetos clave, lo que el juego impide soltar: etiquetas `Important` / `NotDropFromPlayer`) | No se puede soltar marcado ni enviar (ADR-009, decisión 25) | `runtime/quick_drop_action.c` ya protege `Weapon_Sword_070`, `_502` y esas etiquetas (mapa 03 §1.3) |
| Rupias | No se envían nunca (ADR-009, decisión 25) | No viajan por la red |
| Arma, arco o escudo | No se puede soltar marcado ni enviar hasta que durabilidad y bonificaciones viajen por la red (ADR-009, decisión 24) | Un arma soltada o lanzada **sí** entra hoy en el botín compartido (OA:1511-1569) y llega al otro solo con su nombre: hay que comprobarlo e impedirlo o documentarlo |
| Nombre de objeto inválido recibido por red | Se rechaza | Hoy **no** hay lista de permitidos al recibir botín (OA:3604; mapa 07 A4) |
| El anfitrión sale | El botín en el suelo se queda como estaba en cada juego | — |
| Versión distinta | No hay sesión (CU-01) | — |

## Estado actual en el PR: **PARCIAL**

Existe (soltar y recoger):

- Lo que cae al suelo cerca (objeto nuevo sin HashId, quieto 0,4 s) entra en la lista de botín (OA:3430-3488), igual
  que lo que suelta un enemigo propio (OA:3403-3429) y las armas lanzadas o soltadas (OA:1511-1569).
- La lista (64 entradas) vive en el anfitrión: `loot_host_add` (OA:3356-3363). Mensajes LOOT_ADD, LOOT_TAKE, LOOTS y
  PICK (mapa 06 §2.3; estructura `OnlineLoot`, NETH:239).
- Los demás juegos crean el objeto real por nombre (OA:3600-3632). Recogido = desaparecido con un Link a menos de 4 m,
  o pasa a su mano (OA:3523-3541, OA:3577-3599). Armas del suelo: `ground_update` (OA:5458-5560).
- Los materiales que un jugador sostiene sobre la cabeza se ven (`sample_held`, OA:1311; OA:6344-6376).

Falta o es débil:

- **Envío directo**: no hay mensaje ni interfaz (mapa 06 §9).
- **Solo viaja el nombre** del objeto y su posición (`OnlineLoot`, NETH:239): un arma soltada pierde su durabilidad y
  sus modificadores al llegar al otro juego (mapa 07 §7).
- **Quitar de la bolsa sin interfaz**: en el PR no hay función de retirada. En la base, `pouch_discard` +
  `pouch_cleanup` solo se usan para arma, arco y escudo y con el selector rápido abierto
  (`runtime/quick_drop_action.c:286-327`), y ese código exige 1.9.0 (mapa 03 §1.3).
- **Dar a la bolsa**: hay dos caminos ya hechos: `give_item` del PR (OA:3751-3759, direcciones fijas) y el de la base
  `items_request` → `rt_item_tick` → `pouch_item_get` (`runtime/items.c:367-384`, `420-460`), que pasa modificador 0
  y durabilidad nueva (mapa 03 §1.3).
- No probado según el propio PR: lanzar materiales, insectos, menas o cajas rotas por otro (DOC:525-526, 546).
- El anfitrión reenvía cualquier nombre de botín sin comprobar `is_loot()` (OA:3246 existe, pero no se usa al recibir).

## Qué hay que construir

1. **F1 (ya en el plan)**: lista de permitidos al recibir botín: llamar a `is_loot()` (OA:3246) antes de `request` en
   OA:3604 y en `loot_host_add` (OA:3356).
2. **Probar el camino A** con materiales y comida, que es lo que se entrega, y anotar qué funciona.
2b. **Comprobar las armas en el botín compartido** (ADR-009, decisión 24): con dos instancias, soltar y lanzar un
   arma, un arco y un escudo y ver si el otro jugador puede recogerlos por el botín del suelo que ya se comparte
   (OA:1511-1569; `ground_update`, OA:5458-5560). Si puede, **impedirlo** (que esas entradas no se ofrezcan al otro
   jugador) o, si impedirlo rompe algo, **documentarlo** como limitación conocida con el motivo. El resultado se
   anota en `../bitacora/`.
3. **Envío directo, receptor**: mensaje nuevo `GIVE {id de envío, nombre, cantidad}` dirigido a un jugador. En el
   receptor, validar el nombre contra la lista de objetos del juego (`runtime/items.c`, solo objetos con
   `CanGetPouch`) y entregar con el camino de la base: `items_request` + `rt_item_tick` (cola con cerrojo, ya
   segura entre hilos). Responder `GIVE_ACK {id, entregado | no cabe}` usando `items_last_result`
   (`runtime/items.c:352`).
4. **Envío directo, emisor**: quitar el objeto **solo después** de recibir `GIVE_ACK entregado` (regla anti-duplicado
   y anti-pérdida). Para arma, arco y escudo se puede reutilizar `pouch_discard` + `pouch_cleanup` evitando que el
   objeto aparezca en el suelo [sin verificar: `pouch_discard` es "la función de tirar, que crea el objeto en el
   suelo"; hay que encontrar una retirada sin soltar]. Para materiales y comida **no hay función conocida**: hace
   falta ingeniería inversa (mapa 07 §7: "Sí (quitar de la bolsa, durabilidad y modificadores)").
5. **Primera entrega, sin ingeniería inversa [DUEÑO] (ADR-007, decisión 10)**: el botón "Soltar para <nombre>"
   suelta el objeto con el camino A y marca esa entrada de botín como "para <nombre>", con un marcador en pantalla
   (CU-10) para que el compañero la encuentre. Solo admite **materiales y comida**; rechaza armas, arcos, escudos,
   objetos protegidos y rupias (ADR-009, decisiones 24 y 25). Así D9 queda cubierto en F4; los puntos 3 y 4 (envío
   directo real) pasan a ser una mejora posterior.
6. **Durabilidad y modificadores** (condición para admitir armas, arcos y escudos; no entra en la primera entrega):
   ampliar `OnlineLoot` con valor y modificador (cambia el protocolo y sube su versión) y aplicarlos al
   crear o entregar. Leerlos del `PouchItem` (`+0x20` valor, mapa 03 §1.3); el modificador necesita el sitio
   `replace_weapon_modifier` [sin verificar cómo se lee].
7. Interfaz, textos traducidos, avisos en pantalla.

## Reglas y decisiones de diseño

- **[DUEÑO] D9**: se puede recoger lo que el compañero soltó y enviarle objetos.
- **[DUEÑO] D4**: lo enviado deja de ser del emisor y pasa a ser del receptor.
- **[DUEÑO] (ADR-007, decisión 10; Q-09-1)** La primera entrega es **soltar marcado para el compañero**. El envío
  directo a la bolsa, después.
- **[DUEÑO] (ADR-009, decisión 24; Q-09-2)** **No se envían armas** (ni arcos ni escudos) hasta que su durabilidad
  y sus bonificaciones viajen por la red. En F4, soltar marcado admite solo **materiales y comida**.
- **[DUEÑO] (ADR-009, decisión 25; Q-09-3)** No se pueden soltar para el compañero los objetos que el juego ya
  protege: objetos clave, Espada Maestra y lo que el juego impide soltar. Las **rupias no se envían**.
- **[DUEÑO] (ADR-008, decisión 14)** El contenido de un cofre es de quien lo abre; puede soltarlo marcado para el
  compañero si es material o comida.
- **[PROPUESTA]** Nunca se duplica ni se pierde un objeto: quitar solo tras confirmación de entrega.
- **[PROPUESTA]** Sin límite de cantidad ni tiempo de espera entre envíos.

## Criterios de aceptación

- **CA-1** Dado el montaje común con A y B a menos de 10 m, cuando A suelta un material desde su bolsa (a mano o con
  `press`), entonces en menos de 3 s `swctl.py --port 7778 actors <nombre del objeto>` devuelve 1 actor a menos de
  2 m de la posición donde lo ve A.
- **CA-2** Dado lo anterior, cuando B lo recoge, entonces en menos de 3 s `actors <nombre>` queda vacío en A y en B, y
  el objeto está 1 vez en la bolsa de B y 0 en la de A.
- **CA-3** Dado un bot que envía LOOT_ADD con nombre `GameROMPlayer`, entonces ni A ni B piden ese actor (registro con
  el rechazo) y no hay cuelgue.
- **CA-4** (envío directo) Dado A que envía 3 unidades de un material a B, entonces el registro de B contiene la
  entrega, B tiene +3 y A tiene −3; la suma de los dos inventarios no cambia.
- **CA-5** Dado B con la bolsa llena de ese tipo, cuando A envía, entonces A conserva el objeto y ve el aviso "no
  cabe"; la suma no cambia.
- **CA-6** Dado un corte de red de B justo después de enviar A (bot o `tc netem`), entonces tras 30 s el objeto está
  exactamente en un inventario.
- **CA-7** Dado un mensaje `GIVE` con un nombre que no está en la lista de objetos, entonces se rechaza sin llamar al
  juego.
- **CA-8** (armas excluidas) Dado A con un arma, un arco y un escudo en la bolsa, cuando abre "Soltar para <nombre>",
  entonces ninguno de los tres se puede elegir; igual con la Espada Maestra, un objeto clave y las rupias.
- **CA-9** (botín compartido) Dado A que suelta o lanza un arma a mano, entonces la bitácora recoge qué ocurre en B
  y, según lo decidido en el punto 2b, o bien B no puede recogerla (`actors <nombre del arma>` vacío en B), o bien
  la limitación está escrita en la documentación del modo cooperativo.
- **CA-10** (durabilidad; solo cuando se haga el punto 6) Dado A que suelta un arma con durabilidad gastada, cuando
  B la recoge, entonces B tiene la misma durabilidad (±1) y las mismas bonificaciones. Hasta entonces las armas no
  se envían.

## Qué NO hacer

- No llamar a `pouch_item_get` ni a ninguna función del juego desde el hilo de red: solo desde el hilo del juego
  (usa la cola de `items_request`).
- No quitar el objeto al emisor antes de la confirmación del receptor.
- No aceptar nombres de actor de la red sin pasar por `is_loot()` o la lista de `runtime/items.c`.
- No inventar offsets del inventario: los únicos conocidos están en mapa 03 §1.3.
- No cambiar `OnlineLoot` ni añadir mensajes sin pasar por el dueño de `online.h` (cambia la versión de protocolo).
- No copiar código de proyectos de decompilación para encontrar la función de retirada.
- No probar con el guardado real: siempre copias (`SAVAGEWIND_SAVE_DIR`).
- No ofrecer armas, arcos, escudos, objetos protegidos ni rupias en "Soltar para <nombre>" (ADR-009, decisiones 24
  y 25), ni admitir armas antes de que el punto 6 esté hecho y probado.

## Riesgos y preguntas abiertas

- Riesgo: duplicación de objetos si los dos juegos no están de acuerdo en quién recogió (el criterio actual es por
  distancia, OA:3523-3541).
- Riesgo: la retirada de materiales de la bolsa necesita Ghidra; puede retrasar el envío directo real a F5.
- Riesgo: el botín del suelo ya se comparte y hoy incluye armas soltadas o lanzadas; la exclusión de armas puede
  quedar sin efecto por ese camino si no se hace el punto 2b.
- Preguntas ya contestadas por el dueño (Q-09-1, Q-09-2 y Q-09-3): ver ADR-007 (decisión 10) y ADR-009
  (decisiones 24 y 25) en `../decisiones/`.
- **[DUEÑO] (ADR-011, decisión 38; Q-09-4)** Recibir un objeto marcado **no pide confirmación**: entra con un aviso y, si
  no cabe, se queda en el suelo. Errata: ADR-011 la presenta como confirmación del valor por defecto, pero el valor por
  defecto era "se devuelve". **Pregunta pendiente para el dueño:** ¿vale "se queda en el suelo" también para el envío
  directo, donde el objeto no está en el suelo?

## Fase y dependencias

- **Fase**: F4 (lista de permitidos en F1): soltar marcado de materiales y comida. Envío directo, durabilidad,
  bonificaciones (y con ellas las armas) y retirada de materiales: después, posible F5.
- **Depende de**: CU-03, CU-07 (qué es de cada jugador), CU-10 (marcador del objeto soltado).
- **Lo necesitan**: ninguno.
