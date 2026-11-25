# ADR-009 · Respuestas del dueño, ronda 3 (2026-11-24)

**Estado:** aceptado. **Decide:** el dueño del proyecto. Continúa a
[ADR-007](ADR-007-respuestas-del-dueno-ronda-1.md) y [ADR-008](ADR-008-respuestas-del-dueno-ronda-2.md).
Si un caso de uso o una tarea dice otra cosa, **vale este archivo**.

## Decisiones

| # | Pregunta | Decisión del dueño | ¿Coincide con el valor por defecto? |
|---|---|---|---|
| 21 | Q-14-3 ¿Los dos deben ver la misma escena? | **Basta con estar allí.** Uno ve la escena; el otro está presente y recibe el progreso. Verla a la vez se intenta más adelante | Sí |
| 22 | Q-13-3 Si el tinte 3D no es viable | **Nombre en color y marcador con forma.** No bloquea el resto de F5 | Sí |
| 23 | Q-04-3 Conexión directa imposible (anfitrión sin IP pública) | **No se ofrece nada:** ni guía de red virtual ni servicio de presentación | **No** (el defecto era recomendar una red virtual) |
| 24 | Q-09-2 Armas que pierden durabilidad y bonificaciones al cambiar de manos | **No se envían armas** hasta que esté resuelto. Solo materiales y comida | **No** (el defecto era aceptarlo como limitación) |
| 25 | Q-09-3 Objetos que no se pueden soltar para el compañero | **Los que el juego ya protege:** objetos clave, Espada Maestra y lo que el juego impide soltar. Las rupias no se envían | Sí |
| 26 | Q-10-2 Color y forma del marcador | **Se asignan por orden de entrada y se pueden cambiar en Opciones** | Sí |
| 27 | Q-10-4 Vida del compañero en pantalla | **Siempre visible, pequeña**; destacada por debajo del 25 % | Sí |
| 28 | Q-10-1 Frases del chat rápido | **Ocho:** "Ven aquí", "Espera", "Necesito ayuda", "Gracias", "Sí", "No", "Vamos", "Mira esto" | Sí |

## Consecuencias para las tareas

- **Decisión 23.** Se **eliminan** del alcance: la guía de Tailscale/ZeroTier dentro del juego y cualquier
  servicio de presentación o de perforación de NAT. Cuando la conexión falla, el juego muestra solo un
  mensaje de error claro (qué se intentó y que no se pudo conectar). Siguen en F3: descubrimiento en red
  local, apertura automática de puerto con miniupnpc, código de invitación e IPv6. Hay que revisar las
  tareas de F3 que mencionen la guía de red virtual y retirarlas o reducirlas al mensaje de error.
- **Decisión 24.** "Soltar marcado para el compañero" (CU-09) se limita a **materiales y comida** en F4.
  Las armas, arcos y escudos se excluyen explícitamente hasta que la durabilidad y las bonificaciones
  viajen por la red, lo que exige un cambio de protocolo con su subida de versión. Ojo: los objetos del
  suelo ya se comparten hoy entre jugadores (`specs/mapa/07`: "dropped items are already shared loot");
  la tarea debe comprobar si un arma soltada puede acabar igualmente en manos del otro y, si es así,
  impedirlo o documentarlo.
- **Decisión 21.** El bloque de historia de F5 se construye en su versión 1 (presencia y progreso). La
  sincronización de escenas queda como mejora posterior, sin tareas activas.
- **Decisiones 26-28.** CU-10 queda sin preguntas de diseño, salvo el botón.

## Lo que sigue abierto

**Q-10-5 · Botón del ping y de la rueda de chat rápido.** No es una decisión de diseño sino una
comprobación: hay que revisar en `docs/controls.md` y `runtime/input_bind.h` qué botones y teclas están
libres y proponer una combinación al dueño. Queda como tarea de investigación dentro de CU-10.

**Corrección (2026-11-24):** además de Q-10-5, quedan sin contestar 19 preguntas de detalle que las tres
rondas no cubrieron. Están listadas con su valor por defecto en
`../casos-de-uso/PREGUNTAS-ABIERTAS.md` y se construye con ese valor mientras el dueño no diga otra cosa.

**Aclaración a la decisión 12 (ADR-007):** la contraseña es **opcional**, también con el puerto abierto a
Internet. Eso cambia el valor por defecto de Q-01-2, que era "obligatoria".
