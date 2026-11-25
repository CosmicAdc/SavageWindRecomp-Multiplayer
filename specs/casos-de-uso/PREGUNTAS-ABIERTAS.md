# Preguntas abiertas que necesitan la decisión del dueño

Reúne todas las preguntas de los casos CU-01 … CU-14 que solo el dueño del proyecto puede contestar. Cada una lleva
un **valor por defecto recomendado**: es lo que se construirá si no hay respuesta, y lo que debe usar quien ejecute
una tarea mientras la pregunta siga abierta.

Cómo se usa:

- Los grupos conservan el orden original, por **cuánto trabajo bloqueaban** antes de las respuestas del dueño.
- Identificador `Q-<caso>-<n>`. Estado: `ABIERTA` hasta que el dueño conteste; entonces se anota la respuesta aquí
  como `RESUELTA (ADR-00x, decisión n)`, se convierte en regla `[DUEÑO]` en el caso y, si cambia el diseño, se
  escribe un ADR en `../decisiones/`. En una fila resuelta, la columna "Por qué" conserva el razonamiento del valor
  por defecto original, solo como contexto: **manda la respuesta del dueño**.
- "Bloquea" indica qué no se puede cerrar sin la respuesta. Con el valor por defecto se puede **empezar**, no cerrar.


## Resueltas por el dueño

**Ronda 1 (2026-11-24):** Q-06-1, Q-06-3, Q-07-1, Q-07-4, Q-09 (forma de envío), Q-11-1, Q-13-1, Q-14-1, Q-14-2,
Q-14-4, librerías de terceros y reglas de acceso. Las respuestas están en
[`../decisiones/ADR-007-respuestas-del-dueno-ronda-1.md`](../decisiones/ADR-007-respuestas-del-dueno-ronda-1.md)
y **mandan sobre los valores por defecto de este archivo**. Tres cambian el valor por defecto: Q-07-1 (recompensas
para los dos siempre, también al unirse tarde), Q-07-4 (el invitado empieza de cero) y Q-14-4 (se puede continuar
solo de inmediato, con confirmación). Pregunta nueva: Q-07-7, en el ADR-007.

**Ronda 2 (2026-11-24):** Q-07-7, Q-07-6, Q-07-5, Q-11-2, Q-08-1, Q-08-3, Q-12-1, Q-12-3, Q-05-1 y Q-14-7. Respuestas en
[`../decisiones/ADR-008-respuestas-del-dueno-ronda-2.md`](../decisiones/ADR-008-respuestas-del-dueno-ronda-2.md).
Cambia un valor por defecto: Q-05-1 (el sitio se reserva 10 minutos, no 2). Requisito nuevo: poder pasarse los
santuarios juntos sube de prioridad en F5.

**Ronda 3 (2026-11-24):** Q-14-3, Q-13-3, Q-04-3, Q-09-2, Q-09-3, Q-10-1, Q-10-2 y Q-10-4. Respuestas en
[`../decisiones/ADR-009-respuestas-del-dueno-ronda-3.md`](../decisiones/ADR-009-respuestas-del-dueno-ronda-3.md).
Cambian dos valores por defecto: Q-04-3 (si la conexión directa es imposible no se ofrece nada: sin guía de red
virtual ni servicio de presentación) y Q-09-2 (no se envían armas hasta que viajen durabilidad y bonificaciones).

**Ronda 4 (2026-11-24):** las 19 preguntas de detalle que quedaban (Q-06-4, Q-07-3, Q-05-3, Q-05-4, Q-11-4, Q-11-3,
Q-11-5, Q-08-2, Q-08-4, Q-09-4, Q-12-2, Q-12-4, Q-10-3, Q-14-5, Q-13-4, Q-02-1, Q-02-2, Q-03-1 y Q-03-2; decisiones
29-47). Respuestas en
[`../decisiones/ADR-011-respuestas-del-dueno-ronda-4.md`](../decisiones/ADR-011-respuestas-del-dueno-ronda-4.md).
Dato de planificación: el dueño tendrá un segundo PC más adelante; F3 no se cierra sin la prueba con dos PC por
Internet.

De las preguntas de las cuatro rondas (47 decisiones), **sigue abierta solo Q-10-5** (botón del ping y de la rueda):
requiere revisar qué botones están libres. Las 19 preguntas de detalle que ninguna de las tres primeras rondas llegó a
tratar quedaron resueltas en la ronda 4 y siguen listadas abajo (sección "Confirmadas por el dueño en ADR-011").

## Lo que sigue abierto (antes: "Las diez que más bloquean")

Las diez preguntas que más bloqueaban (Q-06-1, Q-07-1, Q-14-1, Q-06-2, Q-07-4, Q-14-2, Q-11-1, Q-14-4, Q-06-3 y
Q-13-1) están resueltas o han dejado de plantearse. **Ninguna pregunta abierta bloquea ya el arranque de una
fase.** Lo que queda:

**Pendiente de una comprobación, no de una decisión de diseño**

| Pregunta | Qué falta | Afecta a |
|---|---|---|
| Q-10-5 ¿Con qué botón se lanza el ping y se abre la rueda de chat rápido? | Revisar en `docs/controls.md` y `runtime/input_bind.h` qué botones y teclas están libres y proponer una combinación al dueño (ADR-009, "Lo único que sigue abierto") | CU-10 |

**Confirmadas por el dueño en ADR-011** (el valor de la columna "Por defecto" es ya la regla `[DUEÑO]`)

| Pregunta | Por defecto | Afecta a |
|---|---|---|
| Q-06-4 ¿El anfitrión es siempre la misma persona durante toda la campaña? | Sí | CU-06 |
| Q-07-3 Semillas Kolog: ¿una por jugador o solo para quien la encuentra? | Una para cada uno, como hoy | CU-07 |
| Q-14-5 Si uno se salta la escena, ¿se salta para los dos? | No aplica en la versión 1 (solo uno la ve) | CU-14a |
| Q-11-3 ¿Qué pasa si el contador se agota? (solo rescate manual) | Revive solo en el sitio con 3 corazones | CU-11, entrega 2 |
| Q-11-4 ¿Con cuántos corazones se vuelve y hay límite? | Entrega 1: lo que da el hada del juego, sin límite | CU-11 |
| Q-11-5 ¿Revivir cuesta algo al rescatador? (solo rescate manual) | Nada | CU-11, entrega 2 |
| Q-05-3 Lo que el invitado hizo en el mundo durante el corte, ¿se pierde? | Sí; conserva solo su inventario | CU-05 |
| Q-05-4 Si cae el anfitrión, ¿termina la sesión? | Sí; el invitado puede reintentar | CU-05 |
| Q-08-2 ¿El teletransporte debe costar algo? | Nada (solo la espera de 30 s ya decidida) | CU-08 |
| Q-08-4 ¿Se permite viajar hacia un compañero en plena escena o diálogo? | Sí; no se le interrumpe | CU-08 |
| Q-09-4 ¿El receptor confirma antes de recibir? | No; entra con un aviso y, si no cabe, se queda en el suelo (ADR-011, decisión 38). Pendiente del dueño: si eso vale también para el envío directo | CU-09 |
| Q-12-2 Al morir en un duelo, ¿qué pasa? | El duelo se apaga y se aplica la reanimación normal | CU-12 |
| Q-12-4 ¿Hay zonas sin duelo? | No se puede abrir en santuarios ni durante escenas | CU-12 |
| Q-10-3 ¿El marcador en el mapa del juego es imprescindible? | No en F4 | CU-10 |
| Q-13-4 ¿Aceptas un cambio que obligue a todos a recompilar sombreadores una vez? | Solo con aprobación del dueño, presentando antes el coste | CU-13 |
| Q-03-1, Q-03-2, Q-02-1, Q-02-2 (base: recorrido de 30 minutos, cierre de F1 con un PC, compañero visible en menús, colisión) | Los de la sección 12 | CU-01 a CU-03 |

Q-06-2 no aparece: ha dejado de plantearse (el perfil cooperativo nace vacío; ADR-007, decisiones 1 y 5).

---

## 1. CU-06 · El mundo del anfitrión

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-06-1 | Al terminar la sesión, ¿qué se lleva el invitado a su propia partida: nada, o todo lo hecho juntos? | **RESUELTA (ADR-007, decisión 1).** Nada. Juega con un perfil cooperativo aparte, ligado al mundo del anfitrión; su partida individual no se toca | Hoy el progreso se escribe en el guardado de todos de forma irreversible (OA:3851-3999). Separar perfiles es lo único que no puede estropear una partida |
| Q-06-2 | Si el invitado tiene una partida más avanzada, ¿debe ver el mundo exactamente como el anfitrión (olvidar su progreso), o basta con no volcarlo? | **SIN OBJETO** tras ADR-007 (decisiones 1 y 5): el perfil cooperativo nace vacío, así que el invitado no trae progreso propio que olvidar | "Desactivar" flags en un juego en marcha no está probado y algunos efectos solo se ven tras una carga (DOC:441-442) |
| Q-06-3 | Las conversaciones y recados con NPC (D5), ¿avanzan para los dos cuando uno habla, o cada uno lleva las suyas? | **RESUELTA (ADR-007, decisión 9).** Progreso común: lo que uno avanza queda hecho en el mundo; cada uno puede repetir la conversación | Separarlos exige clasificar miles de flags a mano |
| Q-06-4 | ¿El anfitrión es siempre la misma persona durante toda la campaña? | RESUELTA (ADR-011, confirma el valor por defecto): Sí | Cambiar de anfitrión significa cambiar de mundo; no hay migración |

## 2. CU-07 · Inventario y corazones propios

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-07-1 | Los orbes espirituales: ¿para los dos, solo para los presentes en el santuario, o solo para quien lo completa? | **RESUELTA (ADR-007, decisión 2).** Para los dos, siempre. También quien se une más tarde recibe lo que el anfitrión ya tenía | Es lo que hace hoy el código (OA:3994) y evita que el invitado se quede débil. Deja de regalar por diferencias de partida previa gracias a CU-06 |
| Q-07-2 | Contenedores de corazón de las Bestias y habilidades de campeón: ¿para los dos? | **RESUELTA (ADR-007, decisión 2).** Sí, para los dos, siempre (misma decisión que Q-07-1) | Coherencia; ya existe (`give_heart`, OA:3997) |
| Q-07-3 | Semillas Kolog: ¿una por jugador o solo para quien lo encuentra? | RESUELTA (ADR-011, confirma el valor por defecto): Una para cada uno (como hoy, OA:3993) | Ya funciona y está probado por los autores |
| Q-07-4 | ¿Con qué inventario entra el invitado la primera vez y dónde se guarda entre sesiones? | **RESUELTA (ADR-007, decisión 5).** Empieza de cero. Conserva lo que consiga en su perfil cooperativo, guardado en su PC | Respeta D4 sin tocar su partida propia |
| Q-07-5 | ¿Las rupias son de cada uno o hay monedero común? | **RESUELTA (ADR-008, decisión 19).** De cada jugador | Nada las comparte hoy; D4 |
| Q-07-7 | (Nace de ADR-007.) Al unirse a un mundo avanzado, ¿cómo recibe el invitado los corazones y la resistencia? | **RESUELTA (ADR-008, decisión 13).** Iguala al anfitrión: mismo número de contenedores de corazón y de mejoras de resistencia, más las habilidades de campeón ya ganadas | El invitado empieza de cero (decisión 5) pero recibe siempre las recompensas (decisión 2) |
| Q-07-6 | El contenido de un cofre: ¿solo para quien lo abre o copia para los dos (P4)? | **RESUELTA (ADR-008, decisión 14).** Para quien lo abre. Puede soltarlo marcado para el compañero. Se revisa después de probarlo | Dar copia exige interceptar la apertura del cofre: trabajo nuevo |

## 3. CU-14 · Historia, santuarios, teletransporte entre mapas

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-14-1 | ¿Qué cuenta exactamente como "momento de historia"? | **RESUELTA (ADR-007, decisión 3).** Lista corta: inicio (piedra y primera torre), final de cada Bestia Divina, recuerdos y jefes finales. Los santuarios no | Cuanto más corta, menos riesgo; los dos primeros ya tienen soporte (OA:1629) |
| Q-14-2 | ¿Qué significa "presente": a cuántos metros, o basta el mismo mapa? | **RESUELTA (ADR-007, decisión 4).** Mismo mapa y a menos de 30 m. Si falta uno, el juego avisa y ofrece teletransportarlo | Es el radio que ya usa el bloqueo de A (OA:5988) |
| Q-14-3 | ¿Los dos deben **ver** la escena, o basta con que el segundo esté allí y reciba el progreso? | **RESUELTA (ADR-009, decisión 21).** Basta con estar allí: uno ve la escena; el otro está presente y recibe el progreso. Verla a la vez se intenta más adelante | Lanzar eventos en el otro juego puede colgarlo (DOC:751-752) |
| Q-14-4 | Si el compañero se desconecta o no puede venir, ¿se puede continuar solo tras un plazo? | **RESUELTA (ADR-007, decisión 6).** Sí, de inmediato, con una confirmación del anfitrión. Sin plazo | Sin válvula de escape la campaña puede quedar bloqueada |
| Q-14-5 | Si uno se salta la escena, ¿se salta para los dos? | RESUELTA (ADR-011, confirma el valor por defecto): No aplica en versión 1 (solo uno la ve). En versión 2: cada uno la suya | Sincronizar el salto es trabajo adicional sin valor claro |
| Q-14-6 | En un santuario, ¿tienen que entrar los dos para poder completarlo? | **RESUELTA (ADR-007, decisión 3).** No hace falta: el monje de un santuario no es momento de historia. Se pueden hacer juntos, no es obligatorio (ADR-008, consecuencias de la decisión 20) | Coherente con D6 y Q-14-1 |
| Q-14-7 | ¿Aceptas que el teletransporte entre mapas pase, en su primera versión, por una pantalla de carga con guardado en el perfil cooperativo? | **RESUELTA (ADR-008, decisión 20).** Sí, con pantalla de carga y guardado. Requisito nuevo: que los dos puedan pasarse los santuarios juntos | Es el único camino conocido que cambia de mapa (`runtime/services/test_warp.c:86-143`) |

## 4. CU-11 · Revivir al compañero

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-11-1 | ¿Vale como primera versión la reanimación automática "mientras el compañero siga vivo", o quieres desde el principio que el compañero tenga que ir a levantarte? | **RESUELTA (ADR-007, decisión 7).** Automático en la primera entrega, como hace hoy el código. El rescate manual queda planificado para después | El rescate manual necesita ingeniería inversa alrededor de la muerte, la zona con más cuelgues |
| Q-11-2 | ¿Qué pasa si caen los dos? | **RESUELTA (ADR-008, decisión 15).** Fin de partida normal: los dos recargan el último guardado | Mantiene algo de dificultad |
| Q-11-3 | ¿Qué pasa si el contador se agota (entrega 2)? | RESUELTA (ADR-011, confirma el valor por defecto): Revive solo en el sitio con 3 corazones | No separa a los jugadores |
| Q-11-4 | ¿Con cuántos corazones se vuelve y hay límite? | RESUELTA (ADR-011, decisión 33): La que da el hada del juego, sin límite de usos mientras el compañero siga vivo | No requiere tocar más código en la entrega 1 |
| Q-11-5 | ¿Revivir cuesta algo al rescatador? | RESUELTA (ADR-011, confirma el valor por defecto): Nada | Simplicidad; D4 |

## 5. CU-05 · Reconexión

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-05-1 | ¿Cuánto tiempo se guarda el sitio del compañero? | **RESUELTA (ADR-008, decisión 18).** 10 minutos | Cubre cortes de Wi-Fi y suspensiones cortas |
| Q-05-2 | Mientras el compañero reconecta, ¿el anfitrión puede avanzar en la historia? | **RESUELTA (ADR-007, decisión 6).** Sí: el anfitrión puede continuar un momento de historia de inmediato, con confirmación. El plazo de reconexión es independiente de la historia | Coherente con D6 |
| Q-05-3 | Lo que el invitado hizo en el mundo durante el corte, ¿se pierde? | RESUELTA (ADR-011, confirma el valor por defecto): Sí; conserva solo su inventario | D3: manda el mundo del anfitrión |
| Q-05-4 | Si cae el anfitrión, ¿termina la sesión? | RESUELTA (ADR-011, decisión 32): Sí: termina la sesión; el invitado ve un aviso y puede reintentar. Sin traspaso de anfitrión | No hay migración de anfitrión |

## 6. CU-08 · Teletransporte al compañero

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-08-1 | ¿Se puede usar en combate? | **RESUELTA (ADR-008, decisión 16).** Sí: siempre disponible, también en combate, con 30 s de espera entre usos | Es una ayuda de accesibilidad (D9), no debe ser una huida gratis |
| Q-08-2 | ¿Debe costar algo? | RESUELTA (ADR-011, confirma el valor por defecto): Nada | D9 |
| Q-08-3 | ¿Quién puede invocar al otro (P5)? | **RESUELTA (ADR-008, decisión 16).** Invocar al otro requiere que acepte | Simetría entre jugadores |
| Q-08-4 | ¿Se permite viajar hacia un compañero en plena escena o diálogo? | RESUELTA (ADR-011, confirma el valor por defecto): Sí; no se le interrumpe | El viajero solo aparece cerca |

## 7. CU-09 · Enviar o soltar objetos

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-09-1 | ¿Basta en F4 con "soltar marcado para el compañero" o el envío directo a la bolsa es imprescindible? | **RESUELTA (ADR-007, decisión 10).** Soltar marcado para el compañero en la primera entrega. Envío directo, después | Quitar materiales de la bolsa sin interfaz necesita ingeniería inversa |
| Q-09-2 | ¿Aceptas armas que pierden durabilidad y modificadores al cambiar de manos mientras no se resuelva? | **RESUELTA (ADR-009, decisión 24).** No. No se envían armas hasta que esté resuelto: solo materiales y comida | Hoy solo viaja el nombre (NETH:239) |
| Q-09-3 | ¿Qué no se puede enviar? ¿Y rupias? | **RESUELTA (ADR-009, decisión 25).** No se envían los que el juego ya protege: objetos clave, Espada Maestra y lo que el juego impide soltar. Las rupias no se envían | Es lo que ya protege el juego al soltar |
| Q-09-4 | ¿El receptor confirma antes de recibir? | RESUELTA (ADR-011, decisión 38): No; entra con un aviso y, si no cabe, se queda en el suelo. Errata: ADR-011 dice que confirma el valor por defecto, pero el valor por defecto era "se devuelve"; queda pendiente que el dueño diga si vale también para el envío directo | Menos interrupciones |

## 8. CU-12 · PvP

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-12-1 | ¿El anfitrión puede prohibir el PvP en su sesión? | **RESUELTA (ADR-008, decisión 17).** Sí: el anfitrión puede prohibir el PvP en su sesión | Igual que las otras reglas de sesión |
| Q-12-2 | Al morir en un duelo, ¿qué pasa? | RESUELTA (ADR-011, confirma el valor por defecto): El duelo se apaga y se aplica la reanimación normal (CU-11) | Sin reglas especiales |
| Q-12-3 | ¿Flechas y bombas en el PvP? | **RESUELTA (ADR-008, decisión 17).** Cuerpo a cuerpo y bombas. Flechas y runas, fuera de la primera entrega. El daño lo valida el anfitrión | Las flechas necesitan trabajo nuevo |
| Q-12-4 | ¿Hay zonas sin duelo? | RESUELTA (ADR-011, confirma el valor por defecto): No se puede abrir en santuarios ni durante escenas | Menos riesgo en las zonas frágiles |

## 9. CU-10 · Marcadores, pings, accesibilidad

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-10-1 | ¿Qué frases rápidas quieres? | **RESUELTA (ADR-009, decisión 28).** Ocho: "Ven aquí", "Espera", "Necesito ayuda", "Gracias", "Sí", "No", "Vamos", "Mira esto" | Ocho caben en una rueda |
| Q-10-2 | ¿Cada jugador elige su color y forma, o se asignan por orden? | **RESUELTA (ADR-009, decisión 26).** Se asignan por orden de entrada y se pueden cambiar en Opciones | Funciona sin configurar nada |
| Q-10-3 | ¿El marcador en el mapa del juego es imprescindible? | RESUELTA (ADR-011, decisión 41): No en F4: flecha en pantalla, distancia y nombre del mapa. Se estudia después | No hay mecanismo para dibujar en el mapa nativo (mapa 04 §4a) |
| Q-10-4 | ¿La vida del compañero se ve siempre o solo cuando está baja? | **RESUELTA (ADR-009, decisión 27).** Siempre visible, pequeña; destacada por debajo del 25 % | Accesibilidad |
| Q-10-5 | ¿Con qué botón se lanza el ping y se abre la rueda? | ABIERTA. Por defecto: Sin decidir: proponer una combinación libre tras revisar `docs/controls.md` | No verificado qué botones están libres |

## 10. CU-13 · Color en el modelo 3D

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-13-1 | ¿"Solo cambia el color" es un tinte sobre todo el Link o el color de una prenda? | **RESUELTA (ADR-007, decisión 8).** Tinte suave sobre todo el Link del compañero, acompañado de nombre y forma para accesibilidad | Es lo que permite el gancho existente (binding 19) |
| Q-13-2 | ¿Cada jugador se ve a sí mismo con su color? | **RESUELTA (ADR-007, decisión 8).** No: el Link propio se ve normal | Teñir al Link local es mucho más difícil |
| Q-13-3 | Si el experimento sale negativo, ¿aceptas quedarte con el color en nombre y marcador? | **RESUELTA (ADR-009, decisión 22).** Sí: nombre en color y marcador con forma. No bloquea el resto de F5 | Evita bloquear F5 |
| Q-13-4 | ¿Aceptas un cambio que obligue a todos a recompilar sombreadores una vez? | RESUELTA (ADR-011, decisión 43): Solo con aprobación del dueño, presentando antes el coste | Afecta a todos los usuarios del build |

## 11. CU-04 · Encontrar la partida

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-04-1 | ¿Aceptas que el código de invitación revele la IP pública del anfitrión? | **RESUELTA (ADR-007, decisión 12).** Sí: el código de invitación puede contener la IP del anfitrión | Sin servidor central no hay otra forma (D7) |
| Q-04-2 | ¿Aceptas una biblioteca de terceros para abrir el puerto (UPnP)? | **RESUELTA (ADR-007, decisión 11).** Sí: miniupnpc, ENet y las que hagan falta, si son de código abierto con licencia compatible con MIT y quedan anotadas en `docs/third-party.md` | Escribirlo a mano es mucho trabajo y riesgo |
| Q-04-3 | Si la conexión directa es imposible (CG-NAT), ¿basta con recomendar una red virtual? | **RESUELTA (ADR-009, decisión 23).** No se ofrece nada: ni guía de red virtual ni servicio de presentación. Solo un mensaje de error claro | Un relé propio contradice D7 |

## 12. CU-01, CU-02, CU-03 · Base

| ID | Pregunta | Estado y respuesta (o valor por defecto) | Por qué |
|---|---|---|---|
| Q-01-1 | ¿La sesión se limita a 2 jugadores desde el principio? | **RESUELTA (ADR-007, decisión 12).** Sí: límite de 2 jugadores por ahora (se diseña para N, se prueba con 2) | Simplifica pruebas y propiedad de enemigos |
| Q-01-2 | ¿Contraseña obligatoria cuando el puerto está abierto a Internet? | **RESUELTA (ADR-007, decisión 12, con la aclaración de ADR-009).** No obligatoria: contraseña opcional, también con el puerto abierto a Internet; recomendada | El servidor no tiene cifrado ni límites (mapa 06 H-3, H-4) |
| Q-03-1 | ¿Qué recorrido de 30 minutos representa "jugar normal"? | RESUELTA (ADR-011, confirma el valor por defecto): Meseta: campo, un campamento, un santuario, menús, una muerte, salir y volver a entrar | Es la zona que probaron los autores |
| Q-03-2 | ¿F1 se cierra con dos instancias en un PC, o hace falta probar con dos PC por Internet? | RESUELTA (ADR-011, confirma el valor por defecto): Dos instancias en un PC cierran F1; dos PC, antes de cerrar F3 | Disponibilidad de hardware |
| Q-02-1 | ¿El compañero debe verse cuando está en un menú? | RESUELTA (ADR-011, confirma el valor por defecto): Sí (regla "los menús no pausan" activada, como hoy) | Ya es el valor por defecto (NET:110) |
| Q-02-2 | ¿Los jugadores se bloquean el paso? | RESUELTA (ADR-011, confirma el valor por defecto): Sí (colisión activada, como hoy); el anfitrión puede apagarla | Ya es el valor por defecto (NET:110) |
