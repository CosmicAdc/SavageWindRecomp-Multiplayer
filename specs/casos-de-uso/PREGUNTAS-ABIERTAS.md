# Preguntas abiertas que necesitan la decisión del dueño

Reúne todas las preguntas de los casos CU-01 … CU-14 que solo el dueño del proyecto puede contestar. Cada una lleva
un **valor por defecto recomendado**: es lo que se construirá si no hay respuesta, y lo que debe usar quien ejecute
una tarea mientras la pregunta siga abierta.

Cómo se usa:

- Los grupos están ordenados por **cuánto trabajo bloquean** (primero lo que frena más casos y fases).
- Identificador `Q-<caso>-<n>`. Estado: `ABIERTA` hasta que el dueño conteste; entonces se anota la respuesta aquí,
  se convierte en regla `[DUEÑO]` en el caso y, si cambia el diseño, se escribe un ADR en `../decisiones/`.
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

**Sigue abierta solo Q-10-5** (botón del ping y de la rueda): requiere revisar qué botones están libres.

## Las diez que más bloquean

| # | Pregunta | Por defecto | Bloquea |
|---|---|---|---|
| 1 | Q-06-1 ¿Qué se lleva el invitado a su propia partida al terminar? | Nada: juega en un perfil de guardado cooperativo aparte | CU-06, CU-07, CU-05, CU-14 |
| 2 | Q-07-1 ¿Orbes, contenedores de corazón y habilidades de campeón son para los dos? | Sí, para los dos, si los dos están en la sesión cuando se consiguen | CU-07, CU-14b |
| 3 | Q-14-1 ¿Qué cuenta como "momento de historia"? | Lista corta: piedra, primera torre, monje de cada santuario, final de cada Bestia, recuerdos, jefes finales | CU-14a entero |
| 4 | Q-06-2 Si el invitado va más avanzado, ¿ve el mundo exactamente como el anfitrión? | Basta con no volcar su progreso; no se le "borra" nada en F4 | CU-06 |
| 5 | Q-07-4 ¿Con qué inventario entra el invitado y dónde se guarda entre sesiones? | Primera vez: copia de su partida; después, el del perfil cooperativo | CU-07, CU-09 |
| 6 | Q-14-2 ¿Qué significa "presente"? | Mismo mapa y a menos de 30 m | CU-14a |
| 7 | Q-11-1 ¿Vale como primera versión la reanimación automática mientras el compañero siga vivo? | Sí (entrega 1); el rescate manual se decide después de probarla | CU-11 |
| 8 | Q-14-4 Si el compañero no puede venir, ¿se puede seguir solo? | Sí, tras 5 minutos desconectado, solo el anfitrión | CU-14a, CU-05 |
| 9 | Q-06-3 ¿Las conversaciones y recados con NPC avanzan para los dos? | Los flags de mundo sí (como hoy); cada uno puede repetir la conversación | CU-06, CU-14a |
| 10 | Q-13-1 ¿"Solo cambia el color" es un tinte sobre todo el Link o el color de una prenda? | Tinte suave sobre todo el Link del compañero | CU-13 |

---

## 1. CU-06 · El mundo del anfitrión (bloquea CU-05, CU-07, CU-14)

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-06-1 | Al terminar la sesión, ¿qué se lleva el invitado a su propia partida: nada, o todo lo hecho juntos? | Nada. El invitado juega en un perfil de guardado "cooperativo con <anfitrión>" separado | Hoy el progreso se escribe en el guardado de todos de forma irreversible (OA:3851-3999). Separar perfiles es lo único que no puede estropear una partida |
| Q-06-2 | Si el invitado tiene una partida más avanzada, ¿debe ver el mundo exactamente como el anfitrión (olvidar su progreso), o basta con no volcarlo? | Basta con no volcarlo. Con el perfil cooperativo nuevo de Q-06-1 el problema desaparece solo | "Desactivar" flags en un juego en marcha no está probado y algunos efectos solo se ven tras una carga (DOC:441-442) |
| Q-06-3 | Las conversaciones y recados con NPC (D5), ¿avanzan para los dos cuando uno habla, o cada uno lleva las suyas? | Avanzan los flags de mundo, como hoy (`Npc*` son compartidos, DOC:990-993); ninguna conversación normal se bloquea | Separarlos exige clasificar miles de flags a mano |
| Q-06-4 | ¿El anfitrión es siempre la misma persona durante toda la campaña? | Sí | Cambiar de anfitrión significa cambiar de mundo; no hay migración |

## 2. CU-07 · Inventario y corazones propios (bloquea CU-09, CU-14b)

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-07-1 | Los orbes espirituales: ¿para los dos, solo para los presentes en el santuario, o solo para quien lo completa? | Para los dos, siempre que los dos estén en la sesión cuando se completa | Es lo que hace hoy el código (OA:3994) y evita que el invitado se quede débil. Deja de regalar por diferencias de partida previa gracias a CU-06 |
| Q-07-2 | Contenedores de corazón de las Bestias y habilidades de campeón: ¿para los dos? | Sí, misma regla que Q-07-1 | Coherencia; ya existe (`give_heart`, OA:3997) |
| Q-07-3 | Semillas Kolog: ¿una por jugador o solo para quien lo encuentra? | Una para cada uno (como hoy, OA:3993) | Ya funciona y está probado por los autores |
| Q-07-4 | ¿Con qué inventario entra el invitado la primera vez y dónde se guarda entre sesiones? | Primera sesión: el perfil cooperativo se crea como copia de su partida; después persiste en ese perfil | Respeta D4 sin tocar su partida propia |
| Q-07-5 | ¿Las rupias son de cada uno o hay monedero común? | De cada uno | Nada las comparte hoy; D4 |
| Q-07-6 | El contenido de un cofre: ¿solo para quien lo abre o copia para los dos (P4)? | Solo para quien lo abre en F4; revisar tras probar | Dar copia exige interceptar la apertura del cofre: trabajo nuevo |

## 3. CU-14 · Historia, santuarios, teletransporte entre mapas (bloquea todo F5)

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-14-1 | ¿Qué cuenta exactamente como "momento de historia"? | Lista cerrada: pedestal de la piedra, primera torre, monje al final de cada santuario, final de cada Bestia Divina, recuerdos, jefes finales. Misiones secundarias y diálogos normales, no | Cuanto más corta, menos riesgo; los dos primeros ya tienen soporte (OA:1629) |
| Q-14-2 | ¿Qué significa "presente": a cuántos metros, o basta el mismo mapa? | Mismo mapa y a menos de 30 m | Es el radio que ya usa el bloqueo de A (OA:5988) |
| Q-14-3 | ¿Los dos deben **ver** la escena, o basta con que el segundo esté allí y reciba el progreso? | Basta con estar allí (versión 1). Ver la misma escena, evento a evento, cuando se pruebe seguro | Lanzar eventos en el otro juego puede colgarlo (DOC:751-752) |
| Q-14-4 | Si el compañero se desconecta o no puede venir, ¿se puede continuar solo tras un plazo? | Sí: tras 5 minutos desconectado, el anfitrión puede continuar | Sin válvula de escape la campaña puede quedar bloqueada |
| Q-14-5 | Si uno se salta la escena, ¿se salta para los dos? | No aplica en versión 1 (solo uno la ve). En versión 2: cada uno la suya | Sincronizar el salto es trabajo adicional sin valor claro |
| Q-14-6 | En un santuario, ¿tienen que entrar los dos para poder completarlo? | Sí para hablar con el monje (es momento de historia); el rompecabezas se puede empezar solo | Coherente con D6 y Q-14-1 |
| Q-14-7 | ¿Aceptas que el teletransporte entre mapas pase, en su primera versión, por una pantalla de carga con guardado en el perfil cooperativo? | Sí | Es el único camino conocido que cambia de mapa (`runtime/services/test_warp.c:86-143`) |

## 4. CU-11 · Revivir al compañero

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-11-1 | ¿Vale como primera versión la reanimación automática "mientras el compañero siga vivo", o quieres desde el principio que el compañero tenga que ir a levantarte? | Vale la automática (entrega 1) | El rescate manual necesita ingeniería inversa alrededor de la muerte, la zona con más cuelgues |
| Q-11-2 | ¿Qué pasa si caen los dos? | Fin de partida normal para los dos | Mantiene algo de dificultad |
| Q-11-3 | ¿Qué pasa si el contador se agota (entrega 2)? | Revive solo en el sitio con 3 corazones | No separa a los jugadores |
| Q-11-4 | ¿Con cuántos corazones se vuelve y hay límite? | Entrega 1: lo que da el hada del juego, sin límite. Entrega 2: 3 corazones, espera de 60 s | No requiere tocar más código en la entrega 1 |
| Q-11-5 | ¿Revivir cuesta algo al rescatador? | Nada | Simplicidad; D4 |

## 5. CU-05 · Reconexión

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-05-1 | ¿Cuánto tiempo se guarda el sitio del compañero? | 2 minutos | Cubre cortes de Wi-Fi y suspensiones cortas |
| Q-05-2 | Mientras el compañero reconecta, ¿el anfitrión puede avanzar en la historia? | No en momentos de historia hasta agotar el plazo de Q-14-4; lo demás, sí | Coherente con D6 |
| Q-05-3 | Lo que el invitado hizo en el mundo durante el corte, ¿se pierde? | Sí; conserva solo su inventario | D3: manda el mundo del anfitrión |
| Q-05-4 | Si cae el anfitrión, ¿termina la sesión? | Sí; el invitado sigue solo en su copia y puede reintentar | No hay migración de anfitrión |

## 6. CU-08 · Teletransporte al compañero

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-08-1 | ¿Se puede usar en combate? | Sí, con 30 s de espera entre usos | Es una ayuda de accesibilidad (D9), no debe ser una huida gratis |
| Q-08-2 | ¿Debe costar algo? | Nada | D9 |
| Q-08-3 | ¿Quién puede invocar al otro (P5)? | Cualquiera, siempre con aceptación del invocado | Simetría entre jugadores |
| Q-08-4 | ¿Se permite viajar hacia un compañero en plena escena o diálogo? | Sí; no se le interrumpe | El viajero solo aparece cerca |

## 7. CU-09 · Enviar o soltar objetos

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-09-1 | ¿Basta en F4 con "soltar marcado para el compañero" o el envío directo a la bolsa es imprescindible? | Basta con soltar marcado | Quitar materiales de la bolsa sin interfaz necesita ingeniería inversa |
| Q-09-2 | ¿Aceptas armas que pierden durabilidad y modificadores al cambiar de manos mientras no se resuelva? | Sí, documentado como limitación | Hoy solo viaja el nombre (NETH:239) |
| Q-09-3 | ¿Qué no se puede enviar? ¿Y rupias? | No: objetos clave, Espada Maestra, objetos con etiqueta `Important` o `NotDropFromPlayer`. Rupias: no | Es lo que ya protege el juego al soltar |
| Q-09-4 | ¿El receptor confirma antes de recibir? | No; llega con aviso, y si no cabe se devuelve | Menos interrupciones |

## 8. CU-12 · PvP

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-12-1 | ¿El anfitrión puede prohibir el PvP en su sesión? | Sí | Igual que las otras reglas de sesión |
| Q-12-2 | Al morir en un duelo, ¿qué pasa? | El duelo se apaga y se aplica la reanimación normal (CU-11) | Sin reglas especiales |
| Q-12-3 | ¿Flechas y bombas en el PvP? | Cuerpo a cuerpo y bombas (lo que hay); flechas no | Las flechas necesitan trabajo nuevo |
| Q-12-4 | ¿Hay zonas sin duelo? | No se puede abrir en santuarios ni durante escenas | Menos riesgo en las zonas frágiles |

## 9. CU-10 · Marcadores, pings, accesibilidad

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-10-1 | ¿Qué frases rápidas quieres? | "Ven aquí", "Espera", "Necesito ayuda", "Gracias", "Sí", "No", "Vamos", "Mira esto" | Ocho caben en una rueda |
| Q-10-2 | ¿Cada jugador elige su color y forma, o se asignan por orden? | Se asignan por orden y se pueden cambiar en Opciones | Funciona sin configurar nada |
| Q-10-3 | ¿El marcador en el mapa del juego es imprescindible? | No en F4: flecha, distancia y nombre del mapa en la lista | No hay mecanismo para dibujar en el mapa nativo (mapa 04 §4a) |
| Q-10-4 | ¿La vida del compañero se ve siempre o solo cuando está baja? | Siempre, pequeña; destacada por debajo del 25 % | Accesibilidad |
| Q-10-5 | ¿Con qué botón se lanza el ping y se abre la rueda? | Sin decidir: proponer una combinación libre tras revisar `docs/controls.md` | No verificado qué botones están libres |

## 10. CU-13 · Color en el modelo 3D

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-13-1 | ¿"Solo cambia el color" es un tinte sobre todo el Link o el color de una prenda? | Tinte suave sobre todo el Link del compañero | Es lo que permite el gancho existente (binding 19) |
| Q-13-2 | ¿Cada jugador se ve a sí mismo con su color? | No: solo ve teñido al compañero | Teñir al Link local es mucho más difícil |
| Q-13-3 | Si el experimento sale negativo, ¿aceptas quedarte con el color en nombre y marcador? | Sí, hasta nuevo aviso | Evita bloquear F5 |
| Q-13-4 | ¿Aceptas un cambio que obligue a todos a recompilar sombreadores una vez? | Solo con ADR aprobado | Afecta a todos los usuarios del build |

## 11. CU-04 · Encontrar la partida

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-04-1 | ¿Aceptas que el código de invitación revele la IP pública del anfitrión? | Sí, con aviso en pantalla | Sin servidor central no hay otra forma (D7) |
| Q-04-2 | ¿Aceptas una biblioteca de terceros para abrir el puerto (UPnP)? | Sí, si su licencia es compatible; con ADR | Escribirlo a mano es mucho trabajo y riesgo |
| Q-04-3 | Si la conexión directa es imposible (CG-NAT), ¿basta con recomendar una red virtual? | Sí | Un relé propio contradice D7 |

## 12. CU-01, CU-02, CU-03 · Base

| ID | Pregunta | Por defecto recomendado | Por qué |
|---|---|---|---|
| Q-01-1 | ¿La sesión se limita a 2 jugadores desde el principio? | Sí, como opción del anfitrión con valor 2 | Simplifica pruebas y propiedad de enemigos |
| Q-01-2 | ¿Contraseña obligatoria cuando el puerto está abierto a Internet? | Sí | El servidor no tiene cifrado ni límites (mapa 06 H-3, H-4) |
| Q-03-1 | ¿Qué recorrido de 30 minutos representa "jugar normal"? | Meseta: campo, un campamento, un santuario, menús, una muerte, salir y volver a entrar | Es la zona que probaron los autores |
| Q-03-2 | ¿F1 se cierra con dos instancias en un PC, o hace falta probar con dos PC por Internet? | Dos instancias en un PC cierran F1; dos PC, antes de cerrar F3 | Disponibilidad de hardware |
| Q-02-1 | ¿El compañero debe verse cuando está en un menú? | Sí (regla "los menús no pausan" activada, como hoy) | Ya es el valor por defecto (NET:110) |
| Q-02-2 | ¿Los jugadores se bloquean el paso? | Sí (colisión activada, como hoy); el anfitrión puede apagarla | Ya es el valor por defecto (NET:110) |
