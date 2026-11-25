# CU-06 · El mundo y el progreso son los del anfitrión

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Los dos jugadores juegan en el mundo del anfitrión: sus misiones, santuarios, cofres, enemigos, hora y clima. Lo que
cualquiera de los dos consigue durante la sesión avanza ese mundo, pero la partida previa del invitado no se vuelca
en él: el invitado juega con un perfil cooperativo aparte que nace vacío y su partida individual no se toca
(ADR-007, decisiones 1 y 5). Hoy el progreso es la **unión** de lo que tiene cada jugador, escrita en el guardado de todos.

## Actores

- **Anfitrión**: dueño del mundo y del progreso.
- **Invitado**: juega en ese mundo.

## Precondiciones

- Sesión establecida (CU-01) y estable (CU-03).
- El servidor sabe qué conexión es la del anfitrión (trabajo de F3).

## Flujo principal

1. El invitado se une con su perfil cooperativo (vacío la primera vez). Su juego recibe el progreso del anfitrión.
2. El invitado ve el mundo como lo tiene el anfitrión: mismos enemigos, mismo botín en el suelo, misma hora y clima,
   mismos santuarios y torres activados, mismos cofres abiertos, mismas misiones.
3. Cualquiera de los dos mata un enemigo, abre un cofre, activa una torre o avanza una misión: el cambio queda en el
   mundo del anfitrión y el otro lo ve.
4. El anfitrión guarda su partida como siempre: es la partida de la campaña compartida.
5. Al terminar la sesión, el mundo del anfitrión conserva todo lo que se hizo entre los dos. El invitado no se
   lleva nada a su partida individual: lo suyo queda en su perfil cooperativo, en su PC.

## Flujos alternativos y de error

- **El invitado tiene una partida individual más avanzada que el anfitrión**: no interviene. La sesión usa el
  perfil cooperativo, que nace vacío, así que su progreso anterior **no** pasa al anfitrión. (Hoy sí pasa: ver
  estado actual.)
- **El invitado tiene una partida individual menos avanzada**: tampoco interviene. Recibe el progreso del anfitrión
  en el perfil cooperativo y su partida individual queda como estaba.
- **El invitado está en otro mapa** (santuario, Bestia Divina): no posee enemigos del mundo y no se le crean los del
  anfitrión (OA:2367-2370); lo que haga allí sigue contando para el progreso.
- **El anfitrión carga otra partida a mitad de sesión**: hoy se confunde con una Luna de Sangre (mapa 07 M7). Debe
  tratarse como "el mundo ha cambiado: reenviar todo".
- **Se corta la conexión del invitado**: su copia del mundo se queda como estaba (DOC:584-585); al volver manda el
  estado del anfitrión (CU-05).
- **Un cliente manipulado envía progreso falso**: el anfitrión lo rechaza (validación).
- **Luna de Sangre**: llega la misma noche para los dos; manda el temporizador del anfitrión (OA:984-1001).
- **Compañero en una escena o diálogo**: el progreso que se genere se aplica al terminar; un efecto visible en el
  mundo puede no verse hasta una carga (DOC:441-442).

## Estado actual en el PR: **PARCIAL** (hoy es la unión de todos)

Lo que ya es del anfitrión:

- Enemigos: registro y propietario en el anfitrión (OA:2318; mapa 07 §2.2). Botín: lista en el anfitrión
  (OA:3237-3649). Hora (OA:5851-5864). Temporizador de Luna de Sangre (OA:984-1001).
- Reglas de sesión (colisión, revivir, menús sin pausa): las del jugador del sitio 0 (OA:2838-2840).

Lo que **no** es del anfitrión:

- **Flags de progreso**: `quests.txt` lista hasta 32 768 flags compartidos (25 767 en el mod v37, DOC:991). Cada 2 s
  cada juego envía un bit por flag que tiene; el anfitrión hace OR con los suyos y devuelve la unión; cada juego
  activa los que le faltan (`quests_update`, OA:3851-3999; el OR está en el bloque `if (host)`). Consecuencia: un
  invitado con una partida avanzada vuelca todo su progreso en el guardado del anfitrión, **de forma irreversible**
  (mapa 07 §2.2, M7).
- **El servidor no valida quién envía qué**: reenvía ENEMIES, LOOTS, QUESTS y PICK de cualquiera, y QUEST sin
  comprobar longitud (NET:278-298; mapa 06 H-2).
- **Clima**: el del primer jugador de la sesión a menos de 500 m, no siempre el anfitrión (OA:1004-1024, DOC:792-793).
- **Objetos del mundo, mecanismos y roturas**: manda quien los mueve o rompe (mapa 07 §2.1, filas 22-24).
- **Estado entre sesiones**: la unión de flags de una sesión anterior sobrevive a `stop_all` y se entrega como válida
  en la siguiente (NET:1322-1335; mapa 06 T-1).
- "El anfitrión" se asume en el sitio 0 sin que nada lo garantice (mapa 06 §1.1).

## Qué hay que construir

1. **Política de flags "manda el anfitrión"** en `quests_update` (futuro módulo `oa_flags`, mapa 07 §8.1):
   - Al unirse, el invitado **no** envía su línea base. El anfitrión solo acepta de un invitado los flags que
     cambian **durante** la sesión (diferencia respecto al estado que tenía al entrar).
   - El invitado aplica los flags del anfitrión con el mecanismo actual (`set_bool_flag`, OA:3794-3804).
   - Separar los flags de progreso del mundo de los que conllevan recompensa personal (CU-07).
2. **Autoridad en el servidor** (F3): marcar la conexión del anfitrión; aceptar ENEMIES, LOOTS, QUESTS y TIME solo de
   él; encaminar HIT, ESTATE, LOOT_* y QUEST solo hacia él; comprobar longitudes (mapa 06 §9.1, punto 2).
3. **Limpieza entre sesiones** de `g.quests`, `quests_have`, botín, enemigos y colas (CU-03).
4. **Guardado del invitado**: jugar la sesión con un perfil de guardado cooperativo dedicado, ligado al mundo del
   anfitrión, que **nace vacío** y vive solo en el PC del invitado. No se copia nada desde su partida individual ni
   se escribe nada en ella (ADR-007, decisiones 1 y 5) (`runtime/save_profiles.c`; mapa 03 fila 26). Atención: la copia automática de seguridad se salta cuando se usa
   `SAVAGEWIND_SAVE_DIR` (`runtime/save_backup.c:57`, citado en mapa 02).
5. **Carga de partida a mitad de sesión**: detectarla explícitamente en vez de por "faltan 64 flags" (OA:3913-3924).
6. **Contrato** `contratos/autoridad-mundo.md` con la tabla "qué es del anfitrión / qué es de cada jugador".
7. Registro claro por flag aplicado, para poder auditar una sesión.

## Reglas y decisiones de diseño

- **[DUEÑO] D3**: el mundo y el progreso son los del anfitrión, compartidos, y el anfitrión manda.
- **[DUEÑO] D1**: los dos recorren la historia juntos; el progreso hecho por cualquiera durante la sesión cuenta.
- **[DUEÑO] D5**: cada jugador habla con los NPC por su cuenta.
- **[DUEÑO] (ADR-007, decisión 9; Q-06-3)** Encargos y conversaciones con NPC: **progreso común**. Lo que uno
  avanza queda hecho en el mundo (los flags `Npc*` siguen compartidos, DOC:990-993); cada uno puede repetir la
  conversación.
- **[DUEÑO] (ADR-007, decisión 1; Q-06-1)** El invitado no se lleva **nada** a su partida individual. Juega con un
  perfil cooperativo aparte, ligado al mundo del anfitrión; su partida individual no se toca.
- **[DUEÑO] (ADR-007, decisión 5)** El perfil cooperativo nace vacío y vive solo en el PC del invitado: no hay
  copia de inventarios ni de progreso entre partidas. Por eso el progreso previo del invitado nunca puede volcarse
  al anfitrión.
- **[DUEÑO] (ADR-007, decisión 12)** Límite de 2 jugadores por ahora: lo nuevo se diseña para N y se prueba con 2.
- **[PROPUESTA]** El clima pasa a ser siempre el del anfitrión cuando está a menos de 500 m, como hoy, y el del
  jugador más cercano si no; se revisa tras probar.
- **[PROPUESTA]** Antes de la primera sesión real, copia de seguridad obligatoria del guardado de ambos.

## Criterios de aceptación

- **CA-1** Dado el montaje común, con la partida individual de B más avanzada que la de A (B tiene un flag F que A no
  tiene; por ejemplo un `Clear_Dungeon*`), cuando B se une con su perfil cooperativo y pasan 30 s, entonces `swctl.py --port 7777 flag F` sigue dando
  0 y A no ha recibido ningún objeto (registro de A sin `into the pouch`).
- **CA-2** Dado A y B en sesión, cuando B consigue un flag de mundo durante la sesión
  (`swctl.py --port 7778 setflag <flag de prueba compartido>`), entonces `swctl.py --port 7777 flag` da 1 en menos de
  10 s.
- **CA-3** Dado A con un flag de mundo que B no tiene, cuando B se une, entonces `swctl.py --port 7778 flag` da 1 en
  menos de 10 s.
- **CA-4** Dado un bot que envía un mensaje QUESTS (el del anfitrión) sin ser el anfitrión, entonces el servidor no
  lo reenvía y B no cambia ningún flag. Igual con ENEMIES y LOOTS. (Requiere ampliar el bot.)
- **CA-5** Dado un bot que envía QUEST con todos los bits a 1, entonces A solo acepta los que son cambios válidos
  según la política (ninguno, si el bot acaba de entrar).
- **CA-6** Dado B que sale de la sesión de A y entra en la de otro anfitrión C, entonces B no entrega a C flags
  aprendidos de A (enlaza con CU-03 CA-5).
- **CA-7** Dado A que mata un enemigo (`set god_mode true` y combate, o prueba `SAVAGEWIND_ONLINE_KILL_TEST`,
  OA:2403-2406), entonces en B ese enemigo desaparece y su flag de muerto queda activado.
- **CA-8** Dado un guardado de B copiado antes de la sesión, cuando termina la sesión con perfil dedicado, entonces
  el guardado propio de B es idéntico byte a byte al de antes.
- **CA-9** Dado B que se une por primera vez a A, entonces su perfil cooperativo se crea vacío (sin objetos de su
  partida individual) y ningún fichero de su partida individual se lee para rellenarlo ni se modifica.

## Qué NO hacer

- No probar nunca con guardados reales: siempre copias (`SAVAGEWIND_SAVE_DIR`). Hoy una sesión cambia el guardado de
  forma irreversible.
- No escribir flags solo en memoria: nada del juego los ve (DOC:437-438). Usar `set_bool_flag` (OA:3794-3804).
- No aceptar en el anfitrión la línea base de flags del invitado: solo cambios hechos durante la sesión.
- No confiar en que "el anfitrión es el sitio 0": el servidor debe marcarlo explícitamente.
- No ampliar ni recortar `quests.txt` a mano: lo genera `tools/online_mod.py`.
- No lanzar eventos del juego para "aplicar" progreso (riesgo de cuelgue, DOC:751-752).
- No mezclar en este caso la decisión de recompensas personales: eso es CU-07.
- No copiar nada de la partida individual del invitado al perfil cooperativo ni al revés (ADR-007, decisiones 1 y 5).
- No cambiar el formato de QUEST / QUESTS sin el dueño de `online.h`.

## Riesgos y preguntas abiertas

- Riesgo alto: hoy una sola sesión puede cambiar para siempre el guardado del anfitrión (mapa 06 H-2). Hasta que
  esto esté hecho, **toda prueba se hace con copias de guardado**.
- Riesgo: la lista de flags compartidos se genera con `tools/online_mod.py` (MOD:845-932) a partir de los ficheros
  del usuario; dos jugadores con mods distintos pueden tener listas distintas, y la versión del `online_mod` no se
  compara al conectar (mapa 06 §2.2).
- Riesgo: algunos efectos de un flag solo se ven tras una carga (DOC:441-442).
- Preguntas ya contestadas por el dueño (Q-06-1 y Q-06-3): ver ADR-007 (decisiones 1, 5 y 9) en `../decisiones/`.
  Q-06-2 (¿debe el invitado "olvidar" su progreso propio?) deja de plantearse: el perfil cooperativo nace vacío.
- **[DUEÑO] (ADR-011, decisión 29; Q-06-4)** El anfitrión es **siempre la misma persona** durante toda la campaña: otro
  anfitrión es otro mundo. No se construye ningún traspaso de partida.

## Fase y dependencias

- **Fase**: F4. La autoridad en el servidor es de F3; la limpieza de estado, de F1-F2.
- **Depende de**: CU-03, F2 (`oa_flags`, `oa_gamedata`), F3 (servidor que conoce al anfitrión).
- **Lo necesitan**: CU-05, CU-07, CU-14.
