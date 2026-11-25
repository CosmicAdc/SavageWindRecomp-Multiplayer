# ADR-007 · Respuestas del dueño, ronda 1 (2026-11-24)

**Estado:** aceptado. **Decide:** el dueño del proyecto. **Sustituye:** los valores por defecto de las
preguntas listadas en `../casos-de-uso/PREGUNTAS-ABIERTAS.md`.

Si un caso de uso o una tarea dice otra cosa que este archivo, **vale este archivo**. Quien toque el caso
afectado debe actualizar su sección "Reglas y decisiones de diseño" y marcar la regla como `[DUEÑO]`.

## Decisiones

| # | Pregunta | Decisión del dueño | ¿Coincide con el valor por defecto? |
|---|---|---|---|
| 1 | Q-06-1 ¿Qué se lleva el invitado a su partida individual? | **Nada.** Juega con un perfil cooperativo aparte, ligado al mundo del anfitrión. Su partida individual no se toca | Sí |
| 2 | Q-07-1 ¿Quién recibe orbes, contenedores de corazón y habilidades de campeón? | **Los dos, siempre.** También quien se une más tarde recibe lo que el anfitrión ya tenía | **No** (el defecto era "solo si los dos están en la sesión") |
| 3 | Q-14-1 ¿Qué es un "momento de historia"? | **Lista corta:** inicio (piedra y primera torre), final de cada bestia divina, recuerdos y jefes finales. Los santuarios no | Casi: el defecto incluía el monje de cada santuario; **se quita** |
| 4 | Q-14-2 ¿Qué significa "presente"? | **Mismo mapa y a menos de 30 m.** Si falta uno, el juego avisa y ofrece teletransportarlo | Sí |
| 5 | Q-07-4 ¿Con qué inventario entra el invitado? | **Empieza de cero.** Conserva lo que consiga en su perfil cooperativo, guardado en su PC | **No** (el defecto era "copia de su partida") |
| 6 | Q-14-4 ¿Se puede continuar la historia sin el compañero? | **Sí, de inmediato**, con una confirmación del anfitrión | **No** (el defecto era "tras 5 minutos desconectado") |
| 7 | Q-11-1 Revivir | **Automático en la primera entrega**, como hace hoy el código. El **rescate manual queda planificado** para después | Sí |
| 8 | Q-13-1 Color | **Tinte suave sobre todo el Link del compañero.** El Link propio se ve normal. Acompañado de nombre y forma para accesibilidad | Sí |
| 9 | Q-06-3 Encargos y conversaciones con NPC | **Progreso común.** Lo que uno avanza queda hecho en el mundo; cada uno puede repetir la conversación | Sí |
| 10 | Q-09 Envío de objetos | **Soltar marcado** para el compañero en la primera entrega. Envío directo, después | Sí |
| 11 | Librerías de terceros | **Aceptadas**: miniupnpc, ENet y las que hagan falta, mientras ahorren trabajo. Deben ser de código abierto con licencia compatible con MIT y quedar anotadas en `docs/third-party.md` | Era pregunta abierta |
| 12 | Acceso a la partida | **Límite de 2 jugadores por ahora. Contraseña opcional** (recomendada por internet). El código de invitación puede contener la IP del anfitrión | Sí |

## Consecuencias para las tareas

- **Decisión 2 (recompensas para los dos, siempre).** El código actual ya regala orbes, contenedores,
  runas y semillas a todos los presentes (`specs/mapa/07`, tabla de huecos). Falta la entrega **al unirse
  a un mundo avanzado**. Las tareas T415–T417 de F4 dejan de estar bloqueadas, y hay que añadir una tarea
  de "ponerse al día al entrar".
- **Decisiones 1 y 5 juntas.** El invitado no trae ni se lleva nada: el perfil cooperativo nace vacío y
  vive solo en su PC. Esto elimina el trabajo de copiar inventarios entre partidas y reduce el riesgo de
  corromper la partida individual. La tarea T402 (perfil cooperativo) se simplifica.
- **Decisión 3.** El bloque de historia de F5 deja de estar bloqueado. No hace falta ningún bloqueo en los
  120 santuarios; el bloqueo de botón A en pedestales que existe hoy hay que revisarlo contra esta lista.
- **Decisión 6.** No hace falta temporizador de desconexión para la historia: basta un diálogo de
  confirmación para el anfitrión. El plazo de gracia de reconexión (CU-05) sigue siendo una cuestión aparte.
- **Decisión 7.** La investigación en Ghidra del "estado caído" (T406) pasa a prioridad baja, pero no se
  elimina.
- **Decisión 11.** ADR-005 (TCP frente a UDP) sigue pendiente de medición, pero ya no depende de permiso
  para usar ENet. La apertura automática de puerto con miniupnpc entra en F3 sin más consulta.
- **Decisión 12.** `MAX` de jugadores: las funciones nuevas se diseñan para N pero se prueban con 2.

## Pregunta nueva que nace de estas respuestas

**Q-07-7 (ABIERTA).** El invitado empieza de cero (decisión 5) pero recibe siempre las recompensas del
anfitrión (decisión 2). Al unirse a un mundo avanzado, ¿cómo se le entregan los corazones y la resistencia?

- Opción A (defecto recomendado): recibe **el mismo número de contenedores de corazón y de mejoras de
  resistencia** que el anfitrión tiene en ese momento, y las habilidades de campeón ya ganadas.
- Opción B: recibe los orbes equivalentes sin gastar y los reparte él entre corazones y resistencia.

Mientras siga abierta, se construye la opción A.
