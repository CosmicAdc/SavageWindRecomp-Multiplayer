# ADR-011 · Respuestas del dueño, ronda 4 (2026-11-24)

**Estado:** aceptado. **Decide:** el dueño del proyecto. Continúa a ADR-007, ADR-008 y ADR-009. Si un caso
de uso o una tarea dice otra cosa, **vale este archivo**.

En esta ronda el dueño **confirmó el valor por defecto** de las 19 preguntas de detalle que quedaban. No
cambia ningún plan ni ninguna tarea: lo que ya estaba escrito como valor por defecto pasa a ser regla
`[DUEÑO]`.

## Decisiones

| # | Pregunta | Decisión del dueño |
|---|---|---|
| 29 | Q-06-4 ¿El anfitrión es siempre la misma persona? | **Sí.** Otro anfitrión es otro mundo. No se construye traspaso de partida |
| 30 | Q-07-3 Semillas kolog | **Una para cada uno** cuando alguien encuentra un kolog (como hoy) |
| 31 | Q-05-3 Lo que el invitado hace en el mundo durante un corte | **Se pierde.** Al volver recibe el mundo del anfitrión; conserva solo su inventario |
| 32 | Q-05-4 Cae el anfitrión | **Termina la sesión.** El invitado ve un aviso y puede reintentar. Sin traspaso de anfitrión |
| 33 | Q-11-4 Vida al revivir (entrega automática) | **La que da el hada del juego, sin límite de usos** mientras el compañero siga vivo |
| 34 | Q-11-3 Contador agotado (rescate manual, entrega posterior) | **Revive en el sitio con 3 corazones** |
| 35 | Q-11-5 Coste para quien rescata | **Ninguno** |
| 36 | Q-08-2 Coste del teletransporte | **Ninguno**, aparte de la espera de 30 s (decisión 16) |
| 37 | Q-08-4 Viajar hacia un compañero en plena escena o conversación | **Permitido, sin interrumpirle.** El viajero aparece cerca |
| 38 | Q-09-4 Recibir un objeto marcado | **Sin confirmación:** entra con un aviso; si no cabe, se queda en el suelo |
| 39 | Q-12-2 Morir en un duelo | **El duelo se apaga y se aplica la reanimación normal.** Sin pérdida de objetos |
| 40 | Q-12-4 Zonas sin duelo | **No se puede abrir en santuarios ni durante escenas** |
| 41 | Q-10-3 Marcador en el mapa del propio juego | **No en F4.** Flecha en pantalla, distancia y nombre del mapa. Se estudia después |
| 42 | Q-14-5 Saltarse una escena (versión futura con escena compartida) | **Cada jugador la suya** |
| 43 | Q-13-4 Cambio que obligue a recompilar sombreadores | **Solo con aprobación del dueño**, presentando antes el coste |
| 44 | Q-02-1 Menús | **No pausan.** El mundo sigue y el Link del compañero se ve quieto (como hoy) |
| 45 | Q-02-2 Colisión entre jugadores | **Activada; el anfitrión puede apagarla** (como hoy) |
| 46 | Q-03-1 Recorrido de prueba de 30 minutos | **Meseta de los Albores:** campo, un campamento, un santuario, menús, una muerte, salir y volver a entrar |
| 47 | Q-03-2 Cierre de F1 | **Dos juegos en un mismo PC.** La prueba con dos PC por Internet se hace antes de cerrar F3 |

## Dato de planificación (no es una regla de diseño)

El dueño tendrá un **segundo PC y un segundo jugador "más adelante"**, no ahora. Consecuencias:

- F1 y F2 se verifican solo con dos instancias en el PC de desarrollo y con el bot.
- Antes de cerrar F3 hace falta ese segundo PC. Si para entonces no existe, la prueba por Internet queda
  como pendiente explícito en la bitácora; no se da F3 por cerrada con pruebas simuladas en Docker.

## Lo único que sigue abierto

**Q-10-5 · Botón del ping y de la rueda de chat rápido.** Es una comprobación técnica: revisar en
`docs/controls.md` y `runtime/input_bind.h` qué botones y teclas están libres y proponer una combinación
al dueño.
