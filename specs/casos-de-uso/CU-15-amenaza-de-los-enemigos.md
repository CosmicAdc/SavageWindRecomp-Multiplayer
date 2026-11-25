# CU-15 · Amenaza de los enemigos: se vuelven hacia quien más daño les hace

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md). Las líneas `OA:n`, `NET:n`,
`NETH:n` y `DOC:n` son del checkout del PR (`pr294-online`, HEAD `350c5f8`); en `coop/main` (`e1a232f`) el mismo
código está unas 30 líneas más abajo (por ejemplo `OWN_SAFE`, OA:2064, está en la 2095): búscalo por el nombre de la
función. Tras F2 vive en `runtime/oa_enemies.c`.

## Resumen

Hoy cada enemigo del mundo del anfitrión lo **simula** el juego de un solo jugador, su **dueño**: el más cercano, con
histéresis, o el último que le golpeó desde menos de 80 m. Su IA solo "piensa" en su dueño: le mira, le persigue y le
ataca a él (aunque sus golpes alcanzan a cualquier Link que tenga delante). Este caso cambia la regla de dueño por la
de **amenaza**: el enemigo pasa al jugador que **más daño le está haciendo**, con el más cercano como desempate. Así,
si uno distrae cuerpo a cuerpo y el otro dispara flechas, el enemigo se vuelve hacia el que más le hiere.

La amenaza la calcula **el juego del anfitrión** con los golpes que ya ve (HIT y la vida que baja), no con lo que
declare cada cliente, y se conservan todas las protecciones de hoy: no pasa a nadie a más de 80 m (`OWN_SAFE`) y no
cambia de dueño más de una vez cada 10 s (`[DUEÑO]` ADR-012, decisión 2).

## Actores

- **Anfitrión**: su juego guarda la lista de enemigos, calcula la amenaza y decide el dueño de cada uno.
- **Dueño**: el jugador cuyo juego simula ese enemigo (puede ser el anfitrión o el invitado).
- **Atacante a distancia** y **atacante cuerpo a cuerpo**: los dos jugadores, cada uno haciendo daño a su manera.

## Precondiciones

- Sesión conectada con dos jugadores en el mismo mapa (CU-01, CU-03), con enemigos del mundo del anfitrión cerca.
- F2 hecha (el reparto de enemigos vive en `runtime/oa_enemies.c`; `RT_ON_RESET` por módulo).
- F3 hecha (el servidor sabe quién es el anfitrión y aplica la tabla de autoridad).
- T409 hecha: HIT llega con la **ranura de origen** que pone el servidor (cambio de protocolo, ver "Qué hay que
  construir", paso 1).
- La línea base de F1 confirma que los enemigos atacan a los dos (T104, paso 5; ADR-012, decisión 1).

## Flujo principal

1. A y B llegan a un campamento. Cada enemigo es del más cercano, como hoy: el de A lo simula el juego de A.
2. A se pone a pelear cuerpo a cuerpo con un enemigo (su dueño). B se queda a 25 m y le dispara flechas.
3. Cada flecha de B que acierta a la copia del enemigo en su juego envía un HIT con el daño (como hoy, OA:3097-3125).
   El juego del anfitrión suma ese daño a la amenaza de B sobre ese enemigo. El daño que hace A, que es el dueño, lo
   ve el anfitrión como la vida que baja en el estado del enemigo (ESTATE o su propio actor) y que ningún HIT explica:
   se suma a la amenaza de A.
4. La amenaza se va olvidando con el tiempo (se reduce a la mitad cada 8 s sin golpes `[PREGUNTA]` Q-15-3).
5. Cuando la amenaza de B llega a 1,5 veces la de A (`[PREGUNTA]` Q-15-2), B está a menos de 80 m del enemigo y han
   pasado 10 s desde el último cambio de dueño de ese enemigo, el anfitrión se lo da a B. El enemigo desaparece del
   juego de A y se crea en el de B, en el mismo sitio y con su vida (como hoy en cada cambio de dueño), y su IA se
   vuelve hacia B: le persigue y le ataca.
6. A sigue viendo la copia del enemigo, que ahora va a por B, y puede seguir golpeándola (sus golpes llegan a B por
   HIT y suman a su amenaza). Si A vuelve a hacer claramente más daño, el enemigo vuelve a A, con la misma regla.
7. Cuando nadie le golpea, la amenaza se apaga y manda otra vez la cercanía, con las reglas de hoy.

## Flujos alternativos y de error

| Situación | Comportamiento esperado |
|---|---|
| **Empate**: ninguno llega a 1,5 veces la amenaza del otro | Decide la cercanía con las reglas y la histéresis de hoy (40 m / 10 m, 15 m durante 5 s, 10 s sin cambiar): el más cercano desempata, sin ir y venir |
| Nadie golpea al enemigo (amenaza 0 para todos) | Exactamente el comportamiento de hoy por cercanía |
| El dueño es quien más amenaza tiene, claramente | Se queda con él mientras esté a menos de 80 m, aunque el otro esté más cerca (`[PREGUNTA]` Q-15-4: hoy el "golpeador" lo pierde a 45 m) |
| **Jugador a más de 80 m** del enemigo (dispara desde lejos) | Su daño cuenta y su amenaza crece, pero el enemigo **no** pasa a él: un juego borra el enemigo que crea tan lejos de su cámara y desaparecería para todos (DOC:587-597). Si se acerca a menos de 80 m con la amenaza aún alta, puede pasar a él (respetando los 10 s) |
| **Jugador muerto** (AS `Dead*`, o el anfitrión con `g_local_dead`) | Su amenaza se pone a 0 en todos los enemigos y no puede ser dueño. Los enemigos que tenía pasan al otro por la regla de hoy (`can_own`, OA:2367-2370; OA:2571-2575). Al revivir empieza de 0 |
| **Enemigo sin dueño** (`OWNER_NONE`: nadie a menos de 80 m) | Los golpes a su copia siguen sumando amenaza (aunque hoy ningún juego aplique ese daño). Cuando alguien entra en los 80 m, pasa al líder claro de amenaza si lo hay y está dentro; si no, al más cercano, como hoy (OA:2576-2587) |
| **Cambio de mapa** de un jugador (santuario, carga, otro mapa) | Deja de poder ser dueño (`same_map`, `ONLINE_F_LOADING`) y su amenaza se pone a 0 en todos los enemigos. Si es el anfitrión el que carga, la lista entera se olvida (`owns_forget`, OA:2324) y la tabla de amenaza con ella |
| El dueño se va de repente (desconexión, carga) | Regla de hoy de "dueño ido": pasa al líder de amenaza que pueda tenerlo, o al más cercano, sin esperar los 10 s; si nadie está a menos de 80 m, queda sin dueño (OA:2589, OA:2603-2613) |
| Un jugador se desconecta y vuelve (CU-05) | Su ranura empieza con amenaza 0 |
| El enemigo muere | Su fila de amenaza se borra con su entrada de la lista |
| Golpe que llega cuando el enemigo ya cambió de dueño | El HIT lo descarta el juego que ya no lo tiene (como hoy, `own_find`, OA:2302); la amenaza se suma igual, porque el anfitrión ve el HIT |
| Daño que no viene de un jugador (caída, fuego del campamento, explosión ajena) | Si baja la vida en el juego del dueño y ningún HIT lo explica, cuenta como del dueño. Se acepta: es poco y no causa cambios por sí solo |
| Jefes (Ganon, Iras) | Fuera de este caso: no están en la lista de enemigos; cada juego simula el suyo (OA:4251-4288) |
| Servidor dedicado (`--server`) | No hay anfitrión ni lista de enemigos: no aplica |
| Reinicio del juego o "salir de la sesión" | La tabla de amenaza se borra (`RT_ON_RESET` y al cortar la sesión, regla 8) |

## Estado actual en el PR: **PARCIAL** (hoy el enemigo se vuelve hacia el **último que golpea**, no hacia el que más daño hace)

Hallazgo al escribir este caso: ADR-012 (Contexto) dice que un enemigo "no se vuelve hacia quien le ataca desde
lejos". Eso vale para el apartado "Owners" de `docs/ONLINE.md`, pero el apartado "Blows" (protocolo 35,
DOC:812-817) describe un traspaso por golpe que sí existe en el código. La decisión del ADR sigue siendo válida: lo que
cambia es que la regla de hoy es "el último que golpea" y la pedida es "el que más daño hace".

- **Dueño por cercanía** (`enemies_host`, OA:2372; bucle de dueños OA:2536-2623): el más cercano en horizontal;
  cambia si el dueño está a más de 40 m y otro 10 m más cerca (OA:2599), o si otro lleva 50 pasadas (~5 s) 15 m más
  cerca (OA:2596); después, `hold = 100` pasadas (~10 s) sin cambiar (OA:2622). Nunca a un jugador a más de
  `OWN_SAFE` = 80 m (OA:2064, OA:2603). `OWNER_NONE` = 0xff (OA:2068).
- **Traspaso por golpe** ("striker", OA:2520-2563): cada golpe a la copia de un enemigo que simula otro juego sube
  `g_aggro_seq` y apunta `g_aggro_id` (OA:3122-3123), que viajan en el STATE (`OnlinePose.aggro_id`/`aggro_seq`,
  NETH:119-120; `WireState`, NET:96-97, NET:873, NET:947). El anfitrión da el enemigo al último que le golpeó, si está
  a menos de 80 m y han pasado 6 s desde el último traspaso por golpe (`aggro = 120` pasadas, OA:2560). **No mira
  `hold`**: un traspaso por golpe puede llegar justo después de uno por cercanía. Después lo mantiene con él mientras
  esté a menos de 45 m (OA:2594-2595).
- **Golpes con daño** (HIT, `u32 id, i32 daño, u8 elemento`): quien golpea una copia calcula el daño por la vida que
  pierde la copia en su juego (OA:3101), lo recorta si fue un ataque por sorpresa sobre un enemigo que pelea
  (OA:3106-3118) y lo envía (OA:3121). El servidor lo reenvía **en crudo, sin decir de quién viene** (NET:281-282;
  protocolo.md §1.5); el juego del dueño lo aplica (`own_hits`, OA:2296-2311). En `coop/main` el receptor ya
  descarta el daño negativo (F1).
- **Lo que el anfitrión ve hoy**: todos los HIT de los demás (el relé va a "todos menos el emisor") y los suyos, que
  envía él; la vida de sus propios enemigos (`enemy_state`, OA:2102, con la vida real durante la guarda de 8 s,
  OA:2185-2240); y la vida de los enemigos de los demás en ESTATE, cada 3 fotogramas (OA:2973-2990; recepción
  OA:2377-2398). Con eso puede calcular el daño de cada uno **sin mensajes nuevos**, salvo saber de quién es cada HIT
  cuando hay más de dos jugadores.
- **Ritmo**: `enemies_host` corre una vez cada 3 fotogramas del juego (OA:6006); `hold` y `aggro` cuentan pasadas, no
  segundos.
- **No existe**: ninguna medida de daño acumulado por jugador ni por enemigo.

## Qué hay que construir

1. **Protocolo (T409, dueño único; constitución, regla 15).** HIT pasa de "relé crudo" a "relé con ranura": el
   servidor antepone la ranura de quien lo envió (de servidor a cliente: 1 + 9 bytes; de cliente a servidor sigue
   siendo 9). `online_take_hits` devuelve también esa ranura, acotada a `< ONLINE_MAX`. Va en el salto único de
   `PROTOCOL` de F4, con su prueba de ida y vuelta, su fila en `contratos/validacion-red.md` y el bot actualizado.
   **No** hay mensajes nuevos ni campos nuevos en STATE; `aggro_id`/`aggro_seq` se quedan en el cable aunque dejen de
   decidir el dueño (quitarlos sería otro cambio de versión). La ranura la pone el servidor, no el cliente: es lo que
   pide la regla 11.
2. **Lógica pura `runtime/coop_threat.c`** (T443), sin punteros del juego ni reloj propio (el reloj se inyecta en
   nanosegundos):
   - Por enemigo: amenaza por ranura, la última vida vista y un "pendiente" de daño de HIT aún no reflejado en la vida.
   - `blow(ranura, daño, vida_actual, ahora)`: un HIT de alguien que **no** es el dueño (recibido con su ranura, o
     enviado por el propio anfitrión). Suma `min(daño, vida_actual)` (daño efectivo: un golpe no puede valer más
     que la vida que le queda) a esa ranura y al pendiente.
   - `life(dueño, vida, ahora)`: la vida observada del enemigo. Lo que baja se descuenta primero del pendiente; lo
     que sobra es daño del dueño. Una subida no cuenta. El pendiente caduca a los 2 s `[PROPUESTA]`.
   - `decay(ahora)`: semivida configurable, 8 s por defecto (Q-15-3), calculada con el tiempo, no con fotogramas.
   - `forget_slot(ranura)`: a 0 (muerto, otro mapa, cargando, desconectado).
   - `pick(...)`: con la amenaza y quién puede ser dueño (`can_own` y a menos de 80 m), devuelve **CERCANÍA** (no hay
     líder claro: decide la regla de hoy), **MANTENER** (el dueño es el líder claro) o **PASAR A x**. Líder claro =
     amenaza > 0 y al menos 1,5 veces (Q-15-2) la segunda mayor, contando al dueño aunque ya no pueda serlo.
3. **Integración en el anfitrión** (T444, `runtime/oa_enemies.c`), en el hilo del juego, dentro de `enemies_host`:
   - Una tabla de amenaza paralela a `g_reg` (64 entradas × 8 ranuras), que se borra con cada entrada nueva, en
     `owns_forget`, al cortar la sesión y con `RT_ON_RESET`.
   - Alimentarla: los HIT que recibe (con su ranura) y los que envía el propio anfitrión al golpear una copia (donde
     hoy llama a `online_send_hit`); la vida de sus enemigos (`enemy_state`) y la de los ajenos al recibir ESTATE.
     `own_hits` sigue aplicando solo los HIT de los enemigos propios, como hoy.
   - Sustituir el traspaso "al último que golpea" (OA:2520-2563 y el `striker` de OA:2594-2595) por `pick`:
     PASAR A x usa el mismo código de traspaso de hoy (`own_drop`/`own_add`, `hold = 100`), **solo con `hold` a 0**,
     salvo que el dueño se haya ido (como hoy); MANTENER salta la regla de cercanía; CERCANÍA deja el código de hoy
     sin tocar.
   - Registro con `SAVAGEWIND_ONLINE_AGGRO_LOG=1` (la variable que ya existe): cada golpe sumado y cada traspaso, con
     el motivo y las dos amenazas (`... slot 1's now (was 0's; threat 21 vs 9)`).
   - Gancho de prueba `SAVAGEWIND_ONLINE_THREAT_TEST=<daño>,<hz>,<s>` en el juego del anfitrión: a partir del segundo
     s, resta ese daño a la vida del enemigo propio más cercano a su Link, a ese ritmo, como si el dueño le golpeara
     (si la vida baja de la cuarta parte de la inicial, la devuelve a la inicial: una subida no cuenta como daño).
   - Actualizar la fila "Enemigos: simulación" de `contratos/autoridad-mundo.md` (regla A-10) y el párrafo "Blows" de
     `docs/ONLINE.md`.
4. **Bot** (T438): modo `BOT_THREAT=<daño>,<hz>,<s>`: lee ENEMIES, elige el enemigo vivo más cercano al jugador al
   que sigue y le envía HIT con ese daño y ritmo; además sube `aggro_id`/`aggro_seq` en su STATE, como hace un juego
   real al golpear, para poder comparar con el comportamiento de hoy.

## Reglas y decisiones de diseño

- **[DUEÑO] (ADR-012, decisión 2)** El enemigo se vuelve hacia quien más daño le hace, no solo hacia el más cercano;
  el más cercano desempata.
- **[DUEÑO] (ADR-012, Consecuencias)** Se conservan las protecciones de hoy: nunca a más de 80 m, 10 s sin cambiar
  de dueño. Esto vale también para los traspasos por amenaza (hoy los traspasos por golpe no miran los 10 s).
- **[DUEÑO] (ADR-012, Consecuencias; constitución, regla 11)** Decide el juego del anfitrión, con los golpes que ya
  ve. Un cliente no envía "mi amenaza".
- **[DUEÑO] (ADR-007, decisión 12)** Se diseña para N jugadores y se prueba con 2: por eso el HIT lleva la ranura de
  origen aunque con 2 jugadores el anfitrión podría deducirla.
- **[PROPUESTA]** Amenaza = daño efectivo (recortado a la vida que le quedaba), sin pesos por tipo de golpe
  (Q-15-1). Sin amenaza por acercarse, cubrirse o distraer sin hacer daño.
- **[PROPUESTA]** Líder claro con margen de 1,5 veces (Q-15-2); semivida de 8 s (Q-15-3); el líder conserva el
  enemigo hasta 80 m (Q-15-4); el pendiente de HIT caduca a los 2 s.
- **[PROPUESTA]** Al cambiar de dueño no se reinicia la amenaza: el que se lo quitó lo conserva mientras siga
  haciendo más daño.
- La protección de hoy contra el ataque por sorpresa al recibir un enemigo (guarda de 8 s, `own_ready`/`own_guard`,
  OA:2185-2240) no se toca.

## Criterios de aceptación

Montaje con bot: un juego anfitrión (montaje común, juego A, con `SAVAGEWIND_ONLINE_AGGRO_LOG=1`) junto a un
enemigo de un campamento de la Meseta (posición anotada en la bitácora de T104) y el bot de T438 siguiéndole. Lo
lanza `tools/coop/threat_run.sh` (T444). "Traspasos al bot" = líneas de registro de cambio de dueño hacia la ranura
del bot con motivo de amenaza.

- **CA-1** (lógica) Dado el módulo puro, cuando se ejecuta `tests/test_coop_threat.c`, entonces sale con 0 y cubre:
  líder claro, empate, dueño que ya no puede serlo, jugador a más de 80 m, semivida a 8 s (la amenaza a la mitad con
  un margen de 1 %), pendiente que caduca, daño recortado a la vida y subida de vida ignorada.
- **CA-2** (más daño, no el último golpe) Dado A con `SAVAGEWIND_ONLINE_THREAT_TEST=3,1,20` y el bot con
  `BOT_THREAT=1,1,25` a 10 m, cuando pasan 30 s desde que el bot empieza, entonces hay **0** traspasos al bot. Control:
  el mismo montaje con `coop/main` antes del cambio da al menos 1 traspaso "struck by it".
- **CA-3** (se vuelve hacia quien más daño hace) Dado A con `THREAT_TEST=1,1,20` y el bot con `BOT_THREAT=3,1,25` a
  10 m, entonces hay **exactamente 1** traspaso al bot, en los 12 s siguientes al primer HIT del bot, y ninguno de
  vuelta en los 30 s siguientes.
- **CA-4** (empate) Dado A con `THREAT_TEST=2,1,20` y el bot con `BOT_THREAT=2,1,25` a 10 m, entonces 0 traspasos al
  bot en 30 s y el enemigo sigue siendo de A (el más cercano).
- **CA-5** (80 m) Dado el bot a 90 m del enemigo (radio del bot 90) con `BOT_THREAT=5,2,25` y A sin golpear,
  entonces 0 traspasos al bot en 30 s y el registro muestra su amenaza creciendo.
- **CA-6** (dos juegos, dueño) Dado A cuerpo a cuerpo con un Bokoblin azul del que es dueño y B disparando flechas
  desde 20-30 m, cuando el daño de B supera claramente al de A, entonces en menos de 12 s el registro de A muestra el
  traspaso a B por amenaza, el de B dice `we have <enemigo> ... for real now`, y en 5 s la distancia del enemigo a B
  baja (`swctl.py --port 7778 actors Enemy_` frente a `pos`). Se anota el vídeo o las capturas en `../bitacora/`.
- **CA-7** (muerte) Dado B dueño por amenaza, cuando B muere (`SAVAGEWIND_ONLINE_DIE_TEST`), entonces en menos de
  2 s sus enemigos pasan a A con motivo "owner gone", la amenaza de B queda a 0 en el registro y, tras revivir, B no
  recupera ninguno en los 10 s siguientes sin volver a golpear.
- **CA-8** (cambio de mapa) Dado B dueño por amenaza, cuando B entra en un santuario (`swctl.py --port 7778 goto`
  a un santuario cercano), entonces su amenaza queda a 0, sus enemigos pasan a A o quedan sin dueño, y ningún juego se
  cuelga en los 60 s siguientes.
- **CA-9** (sin ir y venir) En los recorridos CA-2 a CA-6, ningún enemigo cambia de dueño más de una vez cada 10 s
  (comprobado restando las horas de las líneas de traspaso del mismo enemigo).
- **CA-10** (tiempo de juego) CA-3 repetido con el juego a 30 y a 60 FPS: el traspaso llega en los mismos 12 s; si la
  espera de `hold` (que cuenta pasadas) dura distinto, se anota la diferencia medida.
- **CA-11** (protocolo) `grep -n "PROTOCOL\|version" runtime/online.c tools/online_bot.c` da el mismo número, y la
  prueba de cable de T409 comprueba la ida y vuelta de HIT con su ranura de origen.
- **CA-12** (limpieza) Tras "Leave" y una sesión nueva en el mismo proceso, la primera línea de amenaza de cualquier
  enemigo empieza en 0 para todas las ranuras.

## Qué NO hacer

- No aceptar una "amenaza" ni un "quiero ser el dueño" enviados por un cliente: la calcula el anfitrión.
- No quitar ni saltarse `OWN_SAFE` (80 m), los 10 s de `hold`, ni la guarda de 8 s contra el ataque por sorpresa.
- No dar un enemigo a un jugador muerto, cargando, en otro mapa o a más de 80 m.
- No contar fotogramas para la semivida ni para la caducidad del pendiente: usar el reloj (nanosegundos).
- No cambiar el formato de HIT fuera de T409, ni subir `PROTOCOL` en otra tarea.
- No quitar `aggro_id`/`aggro_seq` del cable en esta fase.
- No escribir en la vida de un enemigo que no es del juego local (el gancho de prueba solo toca enemigos propios).
- No tocar los jefes (Ganon, Iras): siguen sumando daño cada juego por su cuenta.
- No copiar de proyectos de descompilación ni inventar campos o direcciones del actor del enemigo: solo se usa la
  vida (+0x848) que ya usa el PR.

## Riesgos y preguntas abiertas

- Riesgo: más cambios de dueño que hoy. Cada uno borra el enemigo en un juego y lo crea en otro (y hoy deja el arma
  en el suelo si algo falla, DOC:603-609). Mitigación: margen de 1,5, 10 s de `hold` y CA-9.
- Riesgo: el daño se atribuye por diferencias de vida y por un pendiente de 2 s; con latencia alta un golpe ajeno puede
  contarse como del dueño. Afecta poco al resultado; se mide en CA-6.
- Riesgo: fuego, hielo o electricidad que la flecha de un jugador provoca en el juego del dueño (`elem_start`,
  OA:2296-2311) bajan la vida allí y cuentan como daño del dueño **[sin verificar cuánto]**.
- Riesgo: un cliente hostil puede enviar HIT para quedarse enemigos. Ya podía (cercanía, golpe); el daño se recorta a
  la vida que queda y F3 limita la frecuencia de mensajes. Un dueño que no simula el enemigo lo deja quieto: ya pasa
  hoy.
- Riesgo: `hold` y la espera de 5 s de cercanía cuentan pasadas de `enemies_host` (cada 3 fotogramas, OA:6006);
  si el juego corre a más actualizaciones por segundo duran menos **[sin verificar]**. CA-10 lo mide; este caso no lo
  cambia.
- Riesgo: el bot tiene un búfer de 8192 bytes y no lee ENEMIES con más de ~26 enemigos (README, montaje común): las
  pruebas se hacen en un campamento pequeño.
- **[DUEÑO] (ADR-024) Q-15-1** ¿Cuánto pesa una flecha frente a un golpe cuerpo a cuerpo? **Por defecto recomendado: lo
  mismo, punto de daño por punto de daño.** Distinguirlos exigiría un campo nuevo en HIT y saber clasificar el daño
  que hace el dueño en su propio juego, cosa que hoy no se sabe sin ingeniería inversa.
- **[DUEÑO] (ADR-024) Q-15-2** ¿Cuánta más amenaza hace falta para quitarle el enemigo a otro? **Por defecto: 1,5 veces la
  del otro.** Menos hace que vaya y venga entre dos que pegan parecido; más hace que tarde en volverse hacia el que
  dispara.
- **[DUEÑO] (ADR-024) Q-15-3** ¿Cuánto tarda en olvidar? **Por defecto: la amenaza se reduce a la mitad cada 8 s sin
  golpes.**
- **[DUEÑO] (ADR-024) Q-15-4** El jugador que más daño hace, ¿conserva el enemigo hasta 80 m aunque el otro esté al lado?
  **Por defecto: sí, hasta 80 m** (hoy, el que se lo quitó a golpes lo pierde a 45 m). Si no, un arquero a más de
  45 m nunca lo retiene.

## Fase y dependencias

- **Fase**: F4 (primera entrega). Tareas: T443 (lógica pura), T444 (integración); con un paso en T409 (HIT con ranura
  de origen) y otro en T438 (modo de bot `BOT_THREAT`).
- **Depende de**: CU-03 (estabilidad de la sesión), F2 (T207 módulo `oa_enemies.c`, T212 `RT_ON_RESET`, T213
  validación única, T216 pool de marionetas), F3 (T311 anfitrión identificado, T312 tabla de autoridad), F1 (T104,
  paso 5: la línea base de "los enemigos atacan a los dos").
- **Lo necesitan**: ninguno.
