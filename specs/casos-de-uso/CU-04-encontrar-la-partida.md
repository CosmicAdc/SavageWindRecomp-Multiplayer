# CU-04 · Encontrar la partida: red local, código de invitación, apertura de puerto

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

El invitado encuentra la partida del anfitrión sin tener que averiguar ni teclear una dirección IP: la ve en una
lista si están en la misma red, o pega un código corto si están en redes distintas, y el juego intenta abrir el
puerto del router por sí solo. Hoy no existe nada de esto.

## Actores

- **Anfitrión**: crea la sesión y comparte el código.
- **Invitado**: busca o pega el código.
- **Router del anfitrión**: puede aceptar o no la apertura automática de puerto.

## Precondiciones

- CU-01 funciona (unirse por IP sigue existiendo como camino de reserva).
- Envío no bloqueante y validación del servidor de F3 en marcha, porque este caso expone el puerto a Internet.

## Flujo principal

**A. Misma red local**

1. El anfitrión crea la sesión.
2. El invitado abre la pestaña Online. En "Partidas en tu red" aparece la del anfitrión con su nombre y si pide
   contraseña.
3. El invitado la elige y, si hace falta, escribe la contraseña. Entra.

**B. Redes distintas, con código de invitación**

1. El anfitrión crea la sesión. El juego intenta abrir el puerto en el router y le muestra un **código de
   invitación** y un botón para copiarlo.
2. El anfitrión envía el código al invitado por cualquier chat.
3. El invitado pulsa "Unirse con código", lo pega y entra.

**C. Apertura de puerto**

1. Al crear la sesión, el juego pide al router que redirija el puerto TCP.
2. La pestaña muestra el resultado: "Puerto abierto", o "No se pudo abrir: tendrás que redirigirlo a mano en el
   router". El juego no recomienda redes virtuales ni ningún otro servicio (ADR-009, decisión 23).
3. Al terminar la sesión, el juego retira la redirección.

## Flujos alternativos y de error

- **No aparece ninguna partida en la red local** (cortafuegos, redes Wi-Fi de invitados que aíslan equipos): la lista
  queda vacía con un texto de ayuda y sigue disponible "Unirse por dirección".
- **El router no admite apertura automática** o está desactivada: se avisa y se ofrece el camino manual. La sesión
  local sigue funcionando.
- **El anfitrión está detrás de un operador que comparte IP pública (CG-NAT)**: ni la apertura ni el código sirven.
  El juego **no ofrece nada más**: muestra solo un mensaje de error claro que dice qué se intentó y que no se pudo
  conectar (ADR-009, decisión 23). El texto actual de la interfaz que recomienda una red virtual (UI:57-59) se
  sustituye por ese mensaje.
- **La conexión con código o por dirección falla por cualquier otro motivo**: el mismo mensaje de error claro, sin
  guía de red virtual ni reintento por otro camino.
- **Código mal copiado o caducado**: mensaje claro, sin intentar conectar a direcciones inválidas.
- **Versión distinta**: la lista de red local muestra la partida como "otra versión" y no deja entrar; con código, el
  rechazo llega en el saludo (CU-01).
- **Dos anfitriones en la misma red**: aparecen los dos, distinguidos por el nombre del jugador.
- **El anfitrión cierra la sesión**: desaparece de la lista en pocos segundos.

## Estado actual en el PR: **FALTA**

- No hay UDP ni difusión en el código de red: sin descubrimiento en red local (mapa 06 §3).
- No hay UPnP, NAT-PMP, STUN ni relé. La interfaz pide redirigir el puerto a mano o usar una red virtual (UI:57-59).
- La única ayuda es la lista de direcciones IPv4 de este PC en la descripción del botón "Create session"
  (UI:67-72; `online_local_addresses`, NET:1464). No muestra la IP pública ni direcciones IPv6.
- El servidor escucha en todas las interfaces, con IPv6 de doble pila cuando hay (NET:318-333). El cliente resuelve
  con `AF_UNSPEC` (NET:1128-1158).
- No hay historial de servidores ni diagnóstico de puerto (mapa 06 §6.2).

## Qué hay que construir

1. **Descubrimiento en red local**: un socket UDP en el anfitrión que responde a una pregunta de difusión con
   nombre, puerto, número de jugadores, si hay contraseña y versión. Reutilizar el patrón de socket UDP sondeado de
   `runtime/dsu.c` y la abstracción Winsock/BSD de `runtime/control.c:24-39` (mapa 05). El hilo de red es el único
   que toca sockets; la interfaz solo lee una lista protegida por cerrojo.
2. **Código de invitación**: una codificación local y sin servidor de dirección + puerto (y versión) en texto corto,
   con dígito de control para detectar errores de copia. La dirección pública se obtiene del router al abrir el
   puerto [sin verificar: depende de la biblioteca elegida] o la escribe el anfitrión.
3. **Apertura automática de puerto (UPnP y, si es posible, NAT-PMP/PCP)** con **miniupnpc**: en el hilo de red,
   nunca en el hilo de UI; con plazo corto; con retirada al cerrar. La biblioteca ya está aprobada por el dueño
   (ADR-007, decisión 11); hay que anotarla en `docs/third-party.md` con su licencia.
4. **Interfaz**: sección "Partidas en tu red", botón "Unirse con código", estado del puerto, botón "Copiar código"
   y el **mensaje de error claro** cuando no se puede conectar (qué se intentó y que no se pudo), en lugar del
   texto actual sobre redes virtuales. Dueño único de `ui/menu.mm` y `ui/i18n/*.inc` según el plan. Textos
   traducidos y `tools/i18n_check.py`.
5. **IPv6**: mostrar también direcciones IPv6 propias y admitirlas en el código.
6. Datos de red no confiables: todo paquete UDP recibido pasa por la validación única de `contratos/validacion-red.md`
   (longitudes, cadenas terminadas, versión).

## Reglas y decisiones de diseño

- **[DUEÑO] D7**: conexión directa, un jugador hospeda. Por eso el código de invitación **no** usa ningún servidor
  de emparejamiento ni de relé.
- **[DUEÑO] (ADR-007, decisión 12; Q-04-1)** El código de invitación puede contener la dirección del anfitrión:
  quien lo recibe puede conocer su IP pública. Se avisa en la interfaz.
- **[DUEÑO] (ADR-007, decisión 11; Q-04-2)** Las bibliotecas de terceros están aceptadas (miniupnpc para abrir el
  puerto, ENet y las que hagan falta) siempre que sean de código abierto, con licencia compatible con MIT, y queden
  anotadas en `docs/third-party.md`. La apertura automática de puerto entra en F3 sin más consulta.
- **[DUEÑO] (ADR-009, decisión 23; Q-04-3)** Si la conexión directa es imposible (anfitrión sin IP pública), **no
  se ofrece nada**: ni guía de red virtual (Tailscale, ZeroTier…) dentro del juego ni servicio de presentación o de
  perforación de NAT. El juego muestra solo un mensaje de error claro. Siguen en el alcance: descubrimiento en red
  local, apertura de puerto con miniupnpc, código de invitación e IPv6.
- **[DUEÑO] (ADR-007, decisión 12, con la aclaración de ADR-009)** Límite de 2 jugadores por ahora. La contraseña es **opcional**; al abrir el
  puerto a Internet la interfaz la **recomienda**, no la exige.
- **[PROPUESTA]** La apertura automática de puerto va activada por defecto pero con un interruptor para apagarla.
- **[PROPUESTA]** El anuncio en red local se puede apagar ("partida oculta").

## Criterios de aceptación

- **CA-1** Dado el montaje común pero con B **sin** `SAVAGEWIND_ONLINE`, cuando B abre la pestaña Online, entonces
  en menos de 5 s la lista de red local muestra la partida de A con nombre "Host", y al elegirla B entra.
  (Con dos instancias en un PC la difusión llega por la interfaz local; en dos PC reales se repite a mano.)
- **CA-2** Dado A con sesión creada, cuando A la termina, entonces desaparece de la lista de B en menos de 10 s.
- **CA-3** Dado un código generado por A, cuando se decodifica en B, entonces da exactamente la dirección y el puerto
  de A; cuando se altera un carácter, entonces B lo rechaza sin intentar conectar. Prueba unitaria sin juego (Docker).
- **CA-4** Dado un router con UPnP (o un emulador en Docker [sin verificar: hay que elegir uno]), cuando A crea la
  sesión, entonces existe la redirección TCP del puerto; cuando A la termina, desaparece.
- **CA-5** Dado un router sin UPnP, cuando A crea la sesión, entonces crear no tarda más de 5 s adicionales, la
  interfaz no se congela y se muestra el aviso con el camino manual (redirigir el puerto en el router), sin
  mencionar redes virtuales.
- **CA-6** Dado un paquete UDP de descubrimiento mal formado o gigante enviado al anfitrión, entonces se ignora y no
  hay cuelgue (prueba con un guion en Docker).
- **CA-7** Dado un anfitrión con otro número de versión, entonces la lista de B lo marca como incompatible.
- **CA-8** Dado un código o una dirección a los que no se puede conectar (puerto de prueba cerrado), cuando B
  intenta unirse, entonces ve un mensaje de error que dice qué se intentó y que no se pudo conectar, y ningún texto
  de la pestaña Online nombra una red virtual ni un servicio externo (búsqueda en `ui/i18n/*.inc`).

## Qué NO hacer

- No usar ningún servidor externo de emparejamiento, de relé ni de "cuál es mi IP" (contradice D7 y la privacidad).
- No abrir sockets ni hacer peticiones UPnP en el hilo de interfaz ni en el del juego.
- No dejar el puerto abierto en el router al cerrar la sesión o el juego.
- No confiar en ningún paquete UDP recibido: longitudes, cadenas terminadas y versión se validan siempre.
- No responder a la difusión con datos sensibles (contraseña, ruta de ficheros, nombre del PC).
- No construir una guía de red virtual dentro del juego ni un servicio de presentación o de perforación de NAT
  (ADR-009, decisión 23).
- No añadir una biblioteca de terceros que no sea de código abierto con licencia compatible con MIT, ni sin
  registrarla en `docs/third-party.md` (ADR-007, decisión 11).
- No entregar este caso antes del endurecimiento del servidor de F3 (envío no bloqueante, límites, validación).
- No quitar "Unirse por dirección": sigue siendo el camino de reserva.

## Riesgos y preguntas abiertas

- Riesgo aceptado por el dueño: muchos routers domésticos tienen UPnP apagado y muchos operadores usan CG-NAT; en
  esos casos la conexión directa por Internet no es posible y el juego solo lo dice (ADR-009, decisión 23).
- Riesgo de seguridad: abrir el puerto expone un servidor sin cifrado ni autenticación fuerte (mapa 06 H-3, H-4,
  H-5). Este caso no debería entregarse antes que el endurecimiento de F3.
- Riesgo: una biblioteca nueva de terceros afecta a la compilación en Windows, Linux y macOS.
- Preguntas ya contestadas por el dueño (Q-04-1, Q-04-2, Q-04-3 y la contraseña): ver ADR-007 (decisiones 11 y 12)
  y ADR-009 (decisión 23) en `../decisiones/`. Este caso no tiene preguntas abiertas.

## Fase y dependencias

- **Fase**: F3 (red robusta).
- **Depende de**: CU-01, CU-03 y el endurecimiento del servidor de F3 (envío no bloqueante, validación, límites).
- **Lo necesitan**: CU-05 (reconectar usa la misma dirección recordada).
