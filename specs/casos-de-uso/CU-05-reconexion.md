# CU-05 · Reconectar tras una caída sin perder la sesión

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Si la conexión de un jugador se corta un momento, el juego reintenta solo y el jugador vuelve a la misma sesión, en
su mismo sitio y con el mismo estado compartido, sin tener que volver a pulsar "Unirse". Hoy no existe: tras un corte
hay que unirse a mano y se entra como jugador nuevo.

## Actores

- **Jugador desconectado** (normalmente el invitado).
- **Anfitrión**: su juego mantiene la sesión y reserva el sitio.
- **Compañero que sigue conectado**: ve el aviso de "reconectando".

## Precondiciones

- Sesión establecida (CU-01).
- El anfitrión sigue con la sesión abierta. (Si quien cae es el anfitrión, ver flujos alternativos.)
- Limpieza de estado de sesión hecha (CU-03, punto 6): sin ella reconectar es inseguro (mapa 06 T-1).

## Flujo principal

1. La conexión del invitado se corta (Wi-Fi, suspensión breve, cable).
2. El invitado sigue jugando en su mundo. En pantalla aparece "Conexión perdida. Reconectando… (intento 1)".
3. El compañero ve "Client se ha desconectado. Esperando que vuelva…" y su Link desaparece.
4. El juego del invitado reintenta solo cada pocos segundos.
5. Al volver la red, el invitado entra en la misma sesión con el **mismo número de jugador y el mismo color**.
6. Recibe el estado actual del mundo del anfitrión (enemigos, botín, hora, progreso) y los dos vuelven a verse.
7. El aviso desaparece: "Reconectado".

## Flujos alternativos y de error

- **No vuelve dentro del plazo de reserva**: el anfitrión libera el sitio. El invitado ve "No se pudo reconectar" con
  un botón "Reintentar" y otro "Salir".
- **El invitado cancela**: pulsa "Salir" durante los reintentos y queda fuera, sin congelar la interfaz.
- **El anfitrión cierra la sesión mientras tanto**: los reintentos fallan; mensaje "El anfitrión terminó la sesión"
  [sin verificar: hoy el cliente no puede distinguir este caso de un corte; hace falta un mensaje de despedida].
- **Cae el anfitrión**: la sesión termina para todos; no hay migración de anfitrión (mapa 06 §3). El invitado puede
  reintentar por si el anfitrión vuelve a crear la sesión en la misma dirección.
- **Otro jugador intenta ocupar el sitio reservado**: se rechaza (identificador de sesión incorrecto).
- **El invitado estaba en otro mapa, en una escena o muerto al cortarse**: al volver se aplica el estado actual sin
  forzarle nada; un momento de historia que esperaba a los dos (CU-14) se considera en pausa mientras dura el corte.
- **El invitado cerró el juego entero**: al volver a arrancar es una unión normal (CU-01), no una reconexión
  [PROPUESTA: ofrecer "Volver a la última sesión"].
- **Versión distinta al volver**: rechazo normal de versión.

## Estado actual en el PR: **FALTA**

- Tras 10 s sin oír al servidor el cliente pasa a `ONLINE_FAILED` "Lost the connection" (NET:1196-1199); si el
  servidor cierra, "Disconnected by the server" (NET:1318). Se queda ahí hasta que el jugador pulsa Join.
- No hay identificador de sesión ni reserva de sitio: quien vuelve es un jugador nuevo en el primer sitio libre
  (NET:361-362; mapa 06 §3).
- Al quedarse fuera de la sesión, el juego borra los actores de los jugadores (OA:5733-5746). Si se corta el flujo
  del anfitrión, las marionetas de enemigos se quedan como estaban y los enemigos propios siguen suprimidos
  (DOC:584-585).
- `stop_all` no limpia flags, botín, enemigos ni colas pendientes (NET:1322-1335; T-1), y el bitmap de misiones de un
  sitio no se borra al cambiar de ocupante (T-2).
- El servidor da a quien entra el último STATE y EQUIP de cada jugador (NET:254-258), y el mundo se retransmite
  periódicamente (LOOTS cada 2 s, QUESTS al cambiar, objetos rotos uno cada 1,5 s; mapa 06 §2.3, mapa 07 §2.2). Esa
  es la base para "ponerse al día".

## Qué hay que construir

1. **Identificador de sesión y de jugador**: el servidor entrega en WELCOME un identificador aleatorio; el cliente
   lo presenta en el HELLO al volver. Cambia el protocolo (`online.h`, dueño único).
2. **Reserva de sitio** en el servidor durante un plazo (propuesta: 60 s), con el nombre y el color.
3. **Reintento automático** en `cl_thread` (NET:1160-1320): estado nuevo "reconectando" además de los cuatro de
   NETH:22, con espera creciente entre intentos y cancelación inmediata (hoy `stop_all` puede bloquear hasta 6 s por
   dirección: NET:1128-1158).
4. **Limpieza y resincronización**: al reconectar, vaciar colas de entrada y salida y pedir el estado completo
   (flags del anfitrión, botín, objetos rotos). En F3 usa la sincronización actual con autoridad del servidor; cuando
   entre CU-06 (F4), hereda automáticamente la política de flags del anfitrión.
5. **Interfaz**: aviso de reconexión con contador, botones Reintentar y Salir, aviso al compañero. Copiar el patrón
   de la línea de estado del overlay (UI:204-210).
6. **Mensaje de despedida** del anfitrión al terminar la sesión, para distinguirlo de un corte.
7. Pruebas en Docker con el bot y `tc netem` (cortes de 5, 30 y 150 s), como dice el plan.

## Reglas y decisiones de diseño

- **[PROPUESTA] P6**: la reconexión es una propuesta aceptada, pendiente de confirmar sus detalles.
- **[DUEÑO] D3**: al volver manda el estado del anfitrión; lo que el invitado hizo en el mundo durante el corte no
  se vuelca al anfitrión (coherente con CU-06).
- **[DUEÑO] D4**: lo que el invitado ganó en su inventario durante el corte es suyo y lo conserva.
- **[PROPUESTA]** Plazo de reserva de 60 s y reintentos cada 1, 2, 4, 8… s hasta 15 s.
- **[PROPUESTA]** Sin migración de anfitrión: si cae el anfitrión, la sesión termina.
- **[PROPUESTA]** Durante el corte, el invitado sigue jugando en solitario en su mundo (no se le congela).

## Criterios de aceptación

- **CA-1** Dado el montaje común con A y B conectados, cuando se corta la conexión de B durante 15 s (regla de
  cortafuegos, o `tc netem loss 100%` en Docker con el bot), entonces B vuelve solo, con el mismo número de jugador,
  y `swctl.py --port 7777 actors OnlinePlayer` vuelve a devolver 1 actor en menos de 10 s tras restablecer la red.
- **CA-2** Dado el corte anterior, entonces A no tiene dos entradas de B en la lista de jugadores en ningún momento.
- **CA-3** Dado un corte de más que el plazo de reserva, entonces B muestra el fallo con el botón Reintentar y A
  muestra a B como ausente; al pulsar Reintentar, B entra como jugador nuevo sin cuelgue.
- **CA-4** Dado B reconectando, cuando B pulsa Salir, entonces la interfaz responde en menos de 1 s.
- **CA-5** Dado un cliente que presenta un identificador de sesión falso para el sitio reservado de B, entonces el
  servidor lo rechaza y el sitio sigue reservado (prueba con el bot).
- **CA-6** Dado que A activó un flag de mundo durante el corte (`swctl.py --port 7777 setflag <flag>`), cuando B
  reconecta, entonces `swctl.py --port 7778 flag <flag>` da 1 en menos de 10 s.
- **CA-7** Dado un corte, entonces durante los reintentos ningún fotograma de B tarda más de 100 ms por culpa de la
  red (los reintentos no corren en el hilo del juego ni en el de UI).

## Qué NO hacer

- No reintentar la conexión desde el hilo de interfaz ni desde el del juego: solo en `cl_thread`.
- No reconectar sobre estado sin limpiar: antes hay que vaciar colas, flags, botín y enemigos de la sesión anterior
  (mapa 06 T-1, T-2).
- No aceptar una reconexión solo por el nombre del jugador: hace falta el identificador de sesión.
- No volcar al anfitrión lo que el invitado hizo en el mundo durante el corte (D3).
- No congelar ni pausar el juego del jugador desconectado.
- No implementar migración de anfitrión en este caso.
- No cambiar `online.h` (HELLO, WELCOME, estados) sin el dueño del protocolo, y repetir cada cambio en
  `tools/online_bot.c`.

## Riesgos y preguntas abiertas

- Riesgo: reconectar sobre un estado sin limpiar puede aplicar flags viejos (T-1). Por eso depende de CU-03.
- Riesgo: lo que el invitado hizo en solitario durante el corte (enemigos muertos, cofres abiertos en su copia del
  mundo) diverge del mundo del anfitrión; hay que definir qué se descarta.
- Riesgo: TCP tarda en detectar un par caído; el plazo de silencio actual es de 10 s (NET:46-48).
- **Pregunta al dueño**: ¿cuánto tiempo se guarda el sitio del compañero (propuesta: 60 s)?
- **Pregunta al dueño**: mientras el compañero reconecta, ¿el anfitrión puede seguir avanzando en la historia o los
  momentos de historia quedan bloqueados hasta que vuelva (D6)?
- **Pregunta al dueño**: lo que el invitado hizo en el mundo durante el corte, ¿se pierde siempre?
- **Pregunta al dueño**: si cae el anfitrión, ¿basta con terminar la sesión o quieres que el invitado pueda seguir
  solo con lo que tenía?

## Fase y dependencias

- **Fase**: F3 (red robusta).
- **Depende de**: CU-03 (limpieza de estado en `stop_all`, envío no bloqueante), autoridad en el servidor (F3), CU-04
  (dirección recordada). *(Nota: cuando se implemente CU-06 en F4, la resincronización adoptará su política de flags
  del anfitrión.)*
- **Lo necesitan**: CU-14 (qué pasa con un momento de historia si uno se cae).
