# CU-07 · Cada jugador conserva su inventario, sus corazones y su resistencia

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Lo que lleva cada jugador (armas, materiales, comida, rupias) y su vida y resistencia máximas son suyos: no se
copian del otro ni se reparten sin que el jugador lo haya ganado. Hoy cada juego tiene su guardado, pero el reparto
de flags **regala** objetos y contenedores de corazón a todos.

## Actores

- **Jugador A** y **jugador B**, cada uno con su inventario y sus estadísticas.

## Precondiciones

- Sesión establecida (CU-01).
- Política de flags del anfitrión definida (CU-06).

## Flujo principal

1. Cada jugador entra en la sesión con el inventario, los corazones y la resistencia de su guardado.
2. Durante la partida cada uno recoge lo que coge él: lo que recoge A va a la bolsa de A.
3. Cada uno gasta lo suyo: comida, flechas, durabilidad de armas.
4. El daño que recibe cada uno baja solo sus corazones; correr o trepar gasta solo su resistencia.
5. Cuando uno cambia orbes en una estatua de la Diosa, elige él si sube corazón o resistencia, y solo le sube a él.
6. Al terminar la sesión cada uno conserva su inventario y sus estadísticas tal como quedaron.

## Flujos alternativos y de error

- **El compañero completa un santuario o una Bestia sin mí**: hoy recibo igualmente el orbe, la habilidad y el
  contenedor de corazón. Qué debe pasar es una decisión pendiente (ver reglas).
- **Bolsa llena al recibir un objeto por flag**: hoy se registra "doesn't fit in the pouch" y el objeto se pierde,
  pero el flag queda activado (OA:3751-3757, OA:3986-3997).
- **Objetos clave necesarios para avanzar** (pizarra, runas, paravela): deben llegar a los dos aunque solo uno los
  consiga; si no, uno se queda sin poder seguir (caso real documentado con la paravela: DOC:480-485).
- **Compañero en otro mapa, en escena o muerto**: no afecta a mi inventario.
- **Desconexión**: cada uno conserva lo que tenía en ese momento (CU-05).
- **El invitado guarda su partida durante la sesión**: ver pregunta abierta sobre el guardado del invitado.

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
   - *Recompensa personal* (sube estadísticas o inventario): orbes, contenedores de corazón, semillas Kolog,
     habilidades de campeón. Regla a decidir por el dueño.
   - *Estado del mundo* (sin objeto): cofres abiertos, enemigos muertos, puertas.
2. **Aplicar la regla** en el bloque de entrega de `quests_update` (OA:3986-3997; futuro `oa_flags`), usando las
   funciones que ya hay: `give_item` (OA:3751-3759) y `give_heart` (OA:3763-3769).
3. **Bolsa llena**: si un objeto de llave de progreso no cabe, reintentar más tarde en vez de perderlo, y avisar en
   pantalla.
4. **No regalar por diferencias de partida previa**: garantizado por la política de CU-06 (el anfitrión no acepta la
   línea base del invitado).
5. **Guardado del invitado**: perfil dedicado para la campaña cooperativa (CU-06, punto 4), de modo que su
   inventario "de la campaña" persista entre sesiones sin tocar su partida propia.
6. Aviso en pantalla cuando se recibe algo por el compañero ("Has recibido … porque <nombre> …"). Hoy solo va al
   registro.

## Reglas y decisiones de diseño

- **[DUEÑO] D4**: cada jugador conserva su inventario, sus corazones y su resistencia.
- **[DUEÑO] D3**: el progreso del mundo es el del anfitrión; no confundir con el inventario.
- **[PROPUESTA] P4**: las recompensas de santuario y de cofre son para los dos. Pendiente de concretar: ¿para los
  dos siempre, o solo si los dos están presentes?
- **[PROPUESTA]** Las llaves de progreso (piedra, runas, paravela) se entregan siempre a los dos.
- **[PROPUESTA]** El contenido de un cofre abierto lo recibe quien lo abre; el otro recibe una copia solo si P4 se
  confirma para cofres. Hoy el cofre queda abierto para todos y el contenido es de quien lo abrió
  [sin verificar en juego].
- **[PROPUESTA]** Las rupias no se comparten.

## Criterios de aceptación

- **CA-1** Dado el montaje común, cuando A recibe un objeto (`swctl.py --port 7777 give <objeto> 1`), entonces el
  inventario de B no cambia (registro de B sin `into the pouch`; comprobación a mano en el menú de B).
- **CA-2** Dado A y B en sesión, cuando B recibe daño (prueba `SAVAGEWIND_ONLINE_LIFE_LOG` activa), entonces la vida
  de A no cambia.
- **CA-3** Dado el guardado de B con más contenedores de corazón que el de A, cuando B se une y pasan 60 s, entonces
  A no recibe ningún contenedor (registro de A sin `a heart container`).
- **CA-4** Dado A y B en sesión con la regla de recompensas decidida, cuando A activa `Clear_Dungeon<n>` de un
  santuario (`setflag`), entonces B recibe o no el orbe **según la regla escrita en el contrato**, y el registro lo
  explica.
- **CA-5** Dado A que consigue la paravela (`setflag FindDungeon_Finish`), entonces B recibe `PlayerStole2` (regla de
  llave de progreso), como hoy (DOC:480-485).
- **CA-6** Dado B con la bolsa llena de ese tipo, cuando llega un objeto de llave de progreso, entonces no se pierde:
  se entrega en cuanto hay hueco y se avisa en pantalla.
- **CA-7** Dado un jugador que gasta resistencia (`hold` del botón de correr), entonces `swctl.py pos` del otro
  muestra su resistencia sin cambios.

## Qué NO hacer

- No enviar por la red el inventario, las rupias ni las estadísticas de un jugador para "copiarlos" al otro.
- No llamar a `give_item` (OA:3751) ni a `give_heart` (OA:3763) fuera del hilo del juego, ni con nombres que vengan
  de la red sin validar.
- No decidir por tu cuenta qué recompensas se reparten: la tabla está en `contratos/autoridad-mundo.md` y la aprueba
  el dueño.
- No quitar la entrega de llaves de progreso (piedra, runas, paravela): sin ellas un jugador se queda atascado
  (DOC:480-485).
- No escribir la vida o la resistencia máximas directamente en memoria: la semántica de `PlayerInfo +0xa0` es dudosa
  (mapa 03 §1.1).
- No probar con el guardado real: un contenedor de corazón entregado no se puede deshacer.
- No tocar el guardado propio del invitado desde el perfil cooperativo.

## Riesgos y preguntas abiertas

- Riesgo: `give_heart` y `give_item` usan direcciones fijas de 1.9.0 y cambian el guardado de forma irreversible.
- Riesgo: dar a los dos todos los orbes y corazones hace que la pareja avance el doble de rápido que un jugador
  solo; no darlos deja al invitado débil frente al mundo del anfitrión.
- Riesgo: la semántica de `PlayerInfo +0xa0` (vida actual o máxima) es dudosa (mapa 03 §1.1).
- **Pregunta al dueño**: los orbes espirituales, ¿son para los dos (cada uno su orbe por santuario), solo para los
  presentes en el santuario, o solo para quien lo completa?
- **Pregunta al dueño**: los contenedores de corazón de las Bestias Divinas y las habilidades de los campeones,
  ¿para los dos?
- **Pregunta al dueño**: las semillas Kolog, ¿una por jugador o una para quien lo encuentra?
- **Pregunta al dueño**: ¿qué pasa con el inventario del invitado entre sesiones: se guarda en un perfil de la
  campaña cooperativa, o usa siempre el de su partida propia?
- **Pregunta al dueño**: ¿el invitado entra a la primera sesión con su inventario de siempre o empieza de cero?
- **Pregunta al dueño**: ¿las rupias son de cada uno (propuesta) o hay monedero común?

## Fase y dependencias

- **Fase**: F4.
- **Depende de**: CU-06 (política de flags), F2 (`oa_flags`, `oa_gamedata`).
- **Lo necesitan**: CU-09 (enviar objetos), CU-14 (recompensas de santuario).
