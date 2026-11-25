# ADR-008 · Respuestas del dueño, ronda 2 (2026-11-24)

**Estado:** aceptado. **Decide:** el dueño del proyecto. Continúa a
[ADR-007](ADR-007-respuestas-del-dueno-ronda-1.md). Si un caso de uso o una tarea dice otra cosa, **vale
este archivo**.

## Decisiones

| # | Pregunta | Decisión del dueño | ¿Coincide con el valor por defecto? |
|---|---|---|---|
| 13 | Q-07-7 Corazones y resistencia del invitado al unirse a un mundo avanzado | **Iguala al anfitrión:** recibe el mismo número de contenedores de corazón y de mejoras de resistencia, más las habilidades de campeón ya ganadas | Sí |
| 14 | Q-07-6 Contenido de un cofre | **Para quien lo abre.** Puede soltarlo marcado para el compañero. Se revisa después de probarlo | Sí |
| 15 | Q-11-2 Caen los dos a la vez | **Fin de partida normal**: los dos recargan el último guardado | Sí |
| 16 | Q-08-1 / Q-08-3 Teletransporte al compañero | **Siempre disponible, también en combate, con 30 s de espera** entre usos. Invocar al otro requiere que acepte | Sí |
| 17 | Q-12-3 / Q-12-1 Ataques en PvP | **Cuerpo a cuerpo y bombas.** Flechas y runas, fuera de la primera entrega. El anfitrión puede prohibir el PvP en su sesión | Sí |
| 18 | Q-05-1 Tiempo de reserva del sitio al desconectarse | **10 minutos** | **No** (el defecto era 2 minutos) |
| 19 | Q-07-5 Rupias | **De cada jugador** | Sí |
| 20 | Q-14-7 Teletransporte a un compañero en otro mapa | **Aceptado con pantalla de carga y guardado.** El dueño añade el objetivo de fondo: **que los dos puedan pasarse los santuarios juntos** | Sí, con un requisito nuevo |

## Consecuencias para las tareas

- **Decisión 20 sube de prioridad "santuarios juntos".** No basta con poder llegar al mapa del compañero:
  los dos deben verse y jugar dentro del santuario. Hoy quien está en un santuario o en una bestia divina
  **no aparece** en el mundo de los demás (`docs/ONLINE.md` §"Players on other maps";
  `specs/mapa/07`, tabla de huecos: "Shrines/Divine Beasts together: Partial, untested, high"). En F5, el
  bloque de "jugar santuarios juntos" pasa a ir **antes** que el tinte 3D y que el bloque de historia.
  Antes de planificarlo hace falta una tarea de investigación: probar con dos instancias qué ocurre hoy
  cuando los dos entran al mismo santuario, y anotar el resultado en `bitacora/`.
- **Decisión 20 no convierte los santuarios en "momento de historia".** Según la decisión 3 (ADR-007), el
  monje de un santuario no exige a los dos presentes. Se pueden hacer juntos, no es obligatorio.
- **Decisión 18.** El plazo de reconexión de CU-05 es de 10 minutos. Es independiente de la historia: por
  la decisión 6 (ADR-007) el anfitrión puede continuar solo de inmediato, con confirmación.
- **Decisión 13.** La tarea de "ponerse al día al entrar" (derivada de ADR-007) entrega contenedores,
  mejoras de resistencia y habilidades de campeón iguales a las del anfitrión. Combinada con la decisión 5
  (empieza de cero), el invitado entra sin objetos pero con la misma vida y resistencia.
- **Decisión 17.** El daño de PvP debe pasar a validarlo el anfitrión (hoy lo declara el atacante); el
  alcance de armas no cambia.

## Preguntas que siguen abiertas (se construye con su valor por defecto)

Son de detalle y no bloquean ninguna fase: frases del chat rápido (Q-10-1), asignación de color y forma
(Q-10-2), vida del compañero siempre visible (Q-10-4), botón del ping (Q-10-5), qué objetos no se pueden
soltar para el compañero (Q-09-3), durabilidad de armas al cambiar de manos (Q-09-2), si los dos deben ver
la misma escena (Q-14-3), qué pasa si el experimento del tinte 3D sale negativo (Q-13-3) y qué recomendar
cuando la conexión directa es imposible (Q-04-3). Todas están en
`../casos-de-uso/PREGUNTAS-ABIERTAS.md` con su valor por defecto.
