# CU-07 · Cada jugador conserva su inventario, sus corazones y su resistencia

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Lo que lleva cada jugador (armas, materiales, comida, rupias) es suyo y no se copia del otro. El invitado empieza
con un perfil cooperativo vacío y conserva en él lo que consiga. Las recompensas de progreso (orbes, contenedores
de corazón, habilidades de campeón) son **para los dos, siempre**: también quien se une más tarde recibe lo que el
anfitrión ya tenía, y entra con el mismo número de contenedores de corazón y de mejoras de resistencia que el
anfitrión (ADR-007, decisiones 2 y 5; ADR-008, decisión 13). Hoy el reparto de flags ya regala objetos y
contenedores a los presentes, pero falta la entrega al unirse a un mundo avanzado.

## Actores

- **Jugador A** y **jugador B**, cada uno con su inventario y sus estadísticas.

## Precondiciones

- Sesión establecida (CU-01).
- Política de flags del anfitrión definida (CU-06).

## Flujo principal

1. El anfitrión entra con su partida. El invitado entra con su perfil cooperativo: la primera vez, sin objetos;
   después, con lo que guardó en ese perfil. Al entrar en un mundo avanzado el juego le **pone al día**: mismo
   número de contenedores de corazón y de mejoras de resistencia que el anfitrión, y las habilidades de campeón ya
   ganadas.
2. Durante la partida cada uno recoge lo que coge él: lo que recoge A va a la bolsa de A.
3. Cada uno gasta lo suyo: comida, flechas, durabilidad de armas.
4. El daño que recibe cada uno baja solo sus corazones; correr o trepar gasta solo su resistencia.
5. Cuando uno cambia orbes en una estatua de la Diosa, elige él si sube corazón o resistencia, y solo le sube a él.
6. Las rupias y el contenido de los cofres son de quien los consigue; puede soltar un objeto marcado para el
   compañero (CU-09).
7. Al terminar la sesión cada uno conserva su inventario y sus estadísticas tal como quedaron: el anfitrión en su
   partida, el invitado en su perfil cooperativo, en su PC.

## Flujos alternativos y de error

- **El compañero completa un santuario o una Bestia sin mí**: recibo igualmente el orbe, la habilidad y el
  contenedor de corazón (ADR-007, decisión 2). Es lo que ya hace hoy el código.
- **Me uno cuando el anfitrión ya va avanzado**: recibo al entrar lo que el anfitrión ya tenía (puesta al día,
  ADR-008, decisión 13). Si vuelvo a entrar más tarde, solo se me entrega la diferencia, nunca dos veces lo mismo.
- **Bolsa llena al recibir un objeto por flag**: hoy se registra "doesn't fit in the pouch" y el objeto se pierde,
  pero el flag queda activado (OA:3751-3757, OA:3986-3997).
- **Objetos clave necesarios para avanzar** (pizarra, runas, paravela): deben llegar a los dos aunque solo uno los
  consiga; si no, uno se queda sin poder seguir (caso real documentado con la paravela: DOC:480-485).
- **Compañero en otro mapa, en escena o muerto**: no afecta a mi inventario.
- **Desconexión**: cada uno conserva lo que tenía en ese momento (CU-05).
- **El invitado guarda durante la sesión**: se guarda en su perfil cooperativo; su partida individual no se toca
  (ADR-007, decisión 1).

## Estado actual en el PR: **PARCIAL**

A favor:

- "Each player plays their own game (their save, their world…)" (DOC:20-22). Ningún mensaje del protocolo lleva
  inventario, vida ni resistencia del jugador: en `runtime/online.h` los únicos campos de vida son de jefes y
  enemigos (NETH:91, NETH:218).
- El botín que alguien recoge es de quien lo recoge (mapa 07 §2.2, botín).
- El cambio en la estatua de la Diosa es de cada jugador: "the orbs are shared, the heart or stamina each one picks"
  (DOC:476-478).
- El daño PvP y el de los enemigos se aplica en el juego de la víctima, sobre su propia vida (OA:2907-2928).

En contra (regalos automáticos al llegar un flag que hizo otro, OA:3987-3997):

| Flag que llega | Lo que se entrega a quien no lo tenía | Línea |
|---|---|---|
| `IsGet_PortableUnit` | Piedra Sheikah (`Obj_DRStone_Get`) | OA:3987 |
| Runas y mejoras de la piedra | El objeto de la runa | OA:3988 |
| `FindDungeon_Finish` | Paravela (`PlayerStole2`) | OA:3992 |
| Kolog encontrado lejos | Semilla Kolog (`Obj_KorokNuts`) | OA:3993 |
| `Clear_Dungeon*` | Orbe espiritual (`Obj_DungeonClearSeal`) | OA:3994 |
| `IsGet_Obj_HeroSoul_*` | Habilidad del campeón | OA:3996 |
| `Get_HeartUtuwa_*` | **Contenedor de corazón** (`give_heart`, OA:3763-3769) | OA:3997 |

- Como hoy los flags son una unión (CU-06), un invitado avanzado hace que el anfitrión reciba de golpe orbes,
  habilidades y corazones que no ganó.
- **Resistencia**: nada del PR la trata (mapa 07 §7). En la base, el runtime sabe leerla (`PlayerInfo +0xa4`,
  `runtime/control_game.c:182`) e interceptar su escritura (`runtime/assists.c:48`), según mapa 03 §1.1.
- **Rupias**: no viajan por la red; cada uno tiene las suyas [verificado solo por ausencia en `online.h`].

## Qué hay que construir

1. **Clasificar las recompensas** en tres grupos, en `contratos/autoridad-mundo.md`:
   - *Llave de progreso* (los dos la necesitan para seguir): piedra, runas, paravela. Se entregan a los dos.
   - *Recompensa de progreso* (sube estadísticas): orbes, contenedores de corazón y habilidades de campeón. Regla
     del dueño: para los dos, siempre, también para quien se une tarde (ADR-007, decisión 2). Semillas Kolog: una
     para cada uno, como hoy (valor por defecto de Q-07-3, sin respuesta del dueño todavía).
   - *Estado del mundo* (sin objeto): cofres abiertos, enemigos muertos, puertas.
2. **Aplicar la regla** en el bloque de entrega de `quests_update` (OA:3986-3997; futuro `oa_flags`), usando las
   funciones que ya hay: `give_item` (OA:3751-3759) y `give_heart` (OA:3763-3769).
3. **Bolsa llena**: si un objeto de llave de progreso no cabe, reintentar más tarde en vez de perderlo, y avisar en
   pantalla.
4. **No regalar al anfitrión por diferencias de partida previa**: garantizado por la política de CU-06 (el anfitrión
   no acepta la línea base del invitado) y porque el perfil cooperativo nace vacío.
5. **Guardado del invitado**: perfil dedicado para la campaña cooperativa (CU-06, punto 4), que **nace vacío** y
   persiste entre sesiones en el PC del invitado. No se copia nada desde su partida individual ni se escribe en ella
   (ADR-007, decisiones 1 y 5).
6. **Ponerse al día al entrar** (ADR-007, decisión 2; ADR-008, decisión 13): cuando el invitado se une a un mundo
   avanzado, igualar su número de contenedores de corazón y de mejoras de resistencia al del anfitrión y entregarle
   las habilidades de campeón ya ganadas. Los corazones tienen función de entrega (`give_heart`, OA:3763-3769); para
   la resistencia el PR no tiene nada (ver estado actual): hay que encontrar un camino seguro antes de escribirla.
   La entrega debe ser idempotente (volver a entrar no duplica) y hacerse en el hilo del juego.
7. Aviso en pantalla cuando se recibe algo por el compañero ("Has recibido … porque <nombre> …"). Hoy solo va al
   registro.

## Reglas y decisiones de diseño

- **[DUEÑO] D4**: cada jugador conserva su inventario, sus corazones y su resistencia.
- **[DUEÑO] D3**: el progreso del mundo es el del anfitrión; no confundir con el inventario.
- **[DUEÑO] (ADR-007, decisión 2; Q-07-1 y Q-07-2)** Orbes, contenedores de corazón y habilidades de campeón son
  **para los dos, siempre**, estén o no presentes cuando se consiguen. También quien se une más tarde recibe lo que
  el anfitrión ya tenía. (Sustituye a la parte de santuarios de P4.)
- **[DUEÑO] (ADR-007, decisión 5; Q-07-4)** El invitado **empieza de cero**: su perfil cooperativo nace vacío. Lo
  que consiga se conserva en ese perfil, guardado en su PC. Nada se copia de su partida individual (decisión 1).
- **[DUEÑO] (ADR-008, decisión 13; Q-07-7)** Al unirse a un mundo avanzado el invitado **iguala al anfitrión**:
  mismo número de contenedores de corazón y de mejoras de resistencia, más las habilidades de campeón ya ganadas.
  Entra sin objetos pero con la misma vida y resistencia.
- **[DUEÑO] (ADR-008, decisión 14; Q-07-6)** El contenido de un cofre es **para quien lo abre**; puede soltarlo
  marcado para el compañero (CU-09). Se revisa después de probarlo. (Sustituye a la parte de cofres de P4.) Hoy el
  cofre queda abierto para todos y el contenido es de quien lo abrió [sin verificar en juego].
- **[DUEÑO] (ADR-008, decisión 19; Q-07-5)** Las rupias son **de cada jugador**; no hay monedero común.
- **[PROPUESTA]** Las llaves de progreso (piedra, runas, paravela) se entregan siempre a los dos.
- **[PROPUESTA]** Semillas Kolog: una para cada uno, como hoy (Q-07-3, sin respuesta del dueño).

## Criterios de aceptación

- **CA-1** Dado el montaje común, cuando A recibe un objeto (`swctl.py --port 7777 give <objeto> 1`), entonces el
  inventario de B no cambia (registro de B sin `into the pouch`; comprobación a mano en el menú de B).
- **CA-2** Dado A y B en sesión, cuando B recibe daño (prueba `SAVAGEWIND_ONLINE_LIFE_LOG` activa), entonces la vida
  de A no cambia.
- **CA-3** Dada la partida individual de B con más contenedores de corazón que la de A, cuando B se une con su
  perfil cooperativo y pasan 60 s, entonces A no recibe ningún contenedor (registro de A sin `a heart container`).
- **CA-4** Dado A y B en sesión, cuando A activa `Clear_Dungeon<n>` de un santuario (`setflag`), entonces B recibe
  el orbe aunque no esté en el santuario, y el registro lo explica.
- **CA-5** Dado A que consigue la paravela (`setflag FindDungeon_Finish`), entonces B recibe `PlayerStole2` (regla de
  llave de progreso), como hoy (DOC:480-485).
- **CA-6** Dado B con la bolsa llena de ese tipo, cuando llega un objeto de llave de progreso, entonces no se pierde:
  se entrega en cuanto hay hueco y se avisa en pantalla.
- **CA-7** Dado un jugador que gasta resistencia (`hold` del botón de correr), entonces `swctl.py pos` del otro
  muestra su resistencia sin cambios.
- **CA-8** Dado A con una partida avanzada (más contenedores de corazón y mejoras de resistencia que una partida
  nueva, y al menos una habilidad de campeón), cuando B se une con un perfil cooperativo recién creado, entonces en
  menos de 60 s B tiene el mismo número de contenedores y de mejoras de resistencia que A y esas habilidades, su
  bolsa no contiene objetos de su partida individual, y el registro de B enumera lo entregado.
- **CA-9** Dado B ya puesto al día, cuando B sale y vuelve a entrar, entonces no recibe nada por segunda vez.
- **CA-10** Dado A que gana rupias o abre un cofre, entonces las rupias y la bolsa de B no cambian.

## Qué NO hacer

- No enviar por la red el inventario, las rupias ni las estadísticas de un jugador para "copiarlos" al otro.
- No llamar a `give_item` (OA:3751) ni a `give_heart` (OA:3763) fuera del hilo del juego, ni con nombres que vengan
  de la red sin validar.
- No decidir por tu cuenta qué recompensas se reparten: la tabla está en `contratos/autoridad-mundo.md` y sigue las
  reglas `[DUEÑO]` de este caso.
- No dar al invitado, en la puesta al día, objetos, armas ni rupias del anfitrión: solo contenedores de corazón,
  mejoras de resistencia y habilidades de campeón (ADR-008, decisión 13).
- No quitar la entrega de llaves de progreso (piedra, runas, paravela): sin ellas un jugador se queda atascado
  (DOC:480-485).
- No escribir la vida o la resistencia máximas directamente en memoria: la semántica de `PlayerInfo +0xa0` es dudosa
  (mapa 03 §1.1).
- No probar con el guardado real: un contenedor de corazón entregado no se puede deshacer.
- No tocar el guardado propio del invitado desde el perfil cooperativo, ni leerlo para rellenar el perfil.

## Riesgos y preguntas abiertas

- Riesgo: `give_heart` y `give_item` usan direcciones fijas de 1.9.0 y cambian el guardado de forma irreversible.
- Riesgo aceptado por el dueño: dar a los dos todos los orbes y corazones hace que la pareja avance más rápido que
  un jugador solo (ADR-007, decisión 2).
- Riesgo: no hay hoy ninguna función conocida que suba la resistencia máxima; la puesta al día (punto 6) necesita
  encontrarla, o un camino equivalente, antes de poder cumplir la decisión 13 completa.
- Punto a vigilar: en la puesta al día el invitado recibe el **mismo reparto** corazones/resistencia que eligió el
  anfitrión, mientras que los orbes que lleguen después los reparte él (flujo principal, paso 5). Hay que evitar
  que reciba dos veces lo mismo (contenedor ya igualado más el orbe equivalente).
- Riesgo: la semántica de `PlayerInfo +0xa0` (vida actual o máxima) es dudosa (mapa 03 §1.1).
- Preguntas ya contestadas por el dueño (Q-07-1, Q-07-2, Q-07-4, Q-07-5, Q-07-6 y Q-07-7): ver ADR-007
  (decisiones 1, 2 y 5) y ADR-008 (decisiones 13, 14 y 19) en `../decisiones/`.
- **Pregunta al dueño** (Q-07-3, abierta; por defecto: una para cada uno, como hoy): las semillas Kolog, ¿una por
  jugador o una para quien lo encuentra?

## Fase y dependencias

- **Fase**: F4.
- **Depende de**: CU-06 (política de flags), F2 (`oa_flags`, `oa_gamedata`).
- **Lo necesitan**: CU-09 (enviar objetos), CU-14 (recompensas de santuario).
