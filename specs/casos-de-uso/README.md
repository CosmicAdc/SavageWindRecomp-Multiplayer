# Casos de uso del modo cooperativo (CU-01 … CU-16)

Un archivo por caso de uso. Cada uno describe qué quiere hacer el jugador, qué hace hoy el PR #294, qué falta
construir y cómo se comprueba. La lista y las fases salen de `../PLAN-APROBADO.md`.

## Índice

| ID | Título | Estado actual en el PR | Fase | Dificultad | Depende de |
|---|---|---|---|---|---|
| [CU-01](CU-01-hospedar-y-unirse-por-ip.md) | Hospedar una partida y unirse por IP | EXISTE | F1 (probar) | baja | F0 |
| [CU-02](CU-02-ver-al-companero.md) | Ver al compañero moverse, con su equipo | EXISTE | F1 (probar) | baja | CU-01 |
| [CU-03](CU-03-sesion-estable-30-min.md) | Sesión estable de 30 minutos | PARCIAL | F1-F2 | media | CU-01, CU-02 |
| [CU-04](CU-04-encontrar-la-partida.md) | Encontrar la partida: red local, código de invitación, apertura de puerto | FALTA | F3 | media | CU-01, CU-03 |
| [CU-05](CU-05-reconexion.md) | Reconexión tras una caída | FALTA | F3 | media | CU-03 (limpieza de estado), CU-04, F3 (autoridad en el servidor) |
| [CU-06](CU-06-mundo-del-anfitrion.md) | El mundo y el progreso son los del anfitrión | PARCIAL (hoy es la unión de todos) | F4 | media | CU-03, F2 (`oa_flags`), F3 (autoridad en el servidor) |
| [CU-07](CU-07-inventario-y-corazones-propios.md) | Inventario, corazones y resistencia propios | PARCIAL | F4 | media | CU-06 |
| [CU-08](CU-08-teletransporte-al-companero.md) | Teletransporte al compañero (mismo mapa) | FALTA (piezas listas) | F4 | baja | CU-02, CU-03 |
| [CU-09](CU-09-enviar-o-soltar-objetos.md) | Enviar o soltar objetos para el compañero | PARCIAL | F4 | media | CU-03, CU-07, CU-10 |
| [CU-10](CU-10-marcadores-pings-accesibilidad.md) | Marcadores, pings, nombre en color, accesibilidad | PARCIAL (nombre, distancia y flecha existen; lo demás falta) | F4 | baja-media | CU-02 |
| [CU-11](CU-11-revivir-al-companero.md) | Revivir al compañero caído | PARCIAL (hoy: hada automática) | F4 | media-alta | CU-03, CU-08, CU-10 |
| [CU-12](CU-12-pvp-activable.md) | PvP activable por acuerdo, con daño validado | PARCIAL | F4 | media | CU-03, CU-10, CU-11 |
| [CU-13](CU-13-color-en-modelo-3d.md) | Color en el modelo 3D del segundo Link | FALTA | F5 | alta | CU-02, CU-10 |
| [CU-14](CU-14-historia-santuarios-teletransporte-entre-mapas.md) | Historia con los dos presentes, santuarios juntos, teletransporte entre mapas | FALTA (hay piezas) | F5 | alta | CU-03, CU-05, CU-06, CU-07, CU-08, CU-10 |
| [CU-15](CU-15-amenaza-de-los-enemigos.md) | Amenaza: el enemigo se vuelve hacia quien más daño le hace (ADR-012) | PARCIAL (hoy: hacia el último que golpea) | F4 | media-alta | CU-03, F2 (`oa_enemies`), F3 (autoridad) |
| [CU-16](CU-16-companero-en-el-minimapa.md) | El compañero en el minimapa: flecha dibujada encima (ADR-022, amplía ADR-011 decisión 41) | FALTA (minimapa sin medir) | F4 | media | CU-02, CU-10 (T425), T445 |

"Dificultad" sale de las tablas de huecos de `../mapa/06-pr294-red-y-sesion.md` §9 y
`../mapa/07-pr294-integracion-juego.md` §7.

Las preguntas que solo puede contestar el dueño están reunidas en [PREGUNTAS-ABIERTAS.md](PREGUNTAS-ABIERTAS.md).
El dueño ya ha contestado cuatro rondas (47 decisiones): están en `../decisiones/ADR-007-respuestas-del-dueno-ronda-1.md`,
`ADR-008-respuestas-del-dueno-ronda-2.md`, `ADR-009-respuestas-del-dueno-ronda-3.md` y
`ADR-011-respuestas-del-dueno-ronda-4.md`, y **mandan** sobre cualquier texto de un caso que diga otra cosa. La única
que sigue abierta es Q-10-5 (botón del ping), una comprobación técnica; lleva su valor por defecto.

Orden dentro de F5 (ADR-008, decisión 20): jugar los santuarios juntos (CU-14b) va antes que el tinte 3D (CU-13) y
que la historia con los dos presentes (CU-14a).

## Estructura de cada caso (para quien ejecuta una tarea)

Cada archivo es autosuficiente: se puede entregar a un agente sin más contexto que este README. Secciones, siempre
en este orden:

1. **Resumen** · 2. **Actores** · 3. **Precondiciones** · 4. **Flujo principal** (lo que ve el jugador) ·
5. **Flujos alternativos y de error** · 6. **Estado actual en el PR** (EXISTE / PARCIAL / FALTA, con `fichero:línea`)
· 7. **Qué hay que construir** (pasos numerados, con las funciones existentes que se reutilizan) ·
8. **Reglas y decisiones de diseño** (`[DUEÑO]` / `[PROPUESTA]`) · 9. **Criterios de aceptación** (`CA-n`, forma
Dado / Cuando / Entonces, comprobables con un comando o un registro) · 10. **Qué NO hacer** ·
11. **Riesgos y preguntas abiertas** · 12. **Fase y dependencias**.

Reglas para quien ejecuta:

- Haz solo lo que pide el paso asignado de "Qué hay que construir". Si un paso depende de una regla marcada
  `[PROPUESTA]` o de una pregunta abierta, usa el valor por defecto de `PREGUNTAS-ABIERTAS.md` y déjalo anotado; no
  decidas otra cosa. Una regla `[DUEÑO]` que cita un ADR no se discute: si el texto del caso la contradice en
  algún punto, vale el ADR y se avisa.
- Una tarea está terminada cuando se cumplen sus `CA-n` y el resultado está anotado en `../bitacora/`.
- Antes de editar, abre el `fichero:línea` citado y confirma que el código es el descrito. Los números de línea son
  del checkout `pr294-online` (HEAD `350c5f8`); en la rama `coop/main` se moverán. Si no coincide, busca la función
  por su nombre.
- Los números de línea que aparecen **dentro** de `../mapa/04-graficos.md` no coinciden con el checkout del PR en
  `runtime/ui/menu.mm`, `runtime/nvn/vulkan/exec.c` ni `runtime/nvn/maxwell/translate.c`; los de CU-10 y CU-13 están
  recomprobados contra el PR.
- Varios criterios piden comandos de control nuevos de solo lectura (`coop overlay`, `coop rules`, `coop story`,
  `coop tp`): **no existen hoy**; construirlos forma parte del caso que los nombra (patrón: los comandos `actors` y
  `flag` del PR, OA:5565-5705).
- Lo marcado **[sin verificar]** no se da por cierto: se comprueba antes de usarlo o se pregunta.

## Cómo leer las referencias

Todo lo que se afirma sobre el comportamiento actual lleva `fichero:línea` del checkout del PR
(`pr294-online`, HEAD `350c5f8`). Abreviaturas:

| Abrev. | Fichero |
|---|---|
| `OA:n` | `runtime/online_actors.c` |
| `NET:n` | `runtime/online.c` |
| `NETH:n` | `runtime/online.h` |
| `UI:n` | `runtime/ui/online_ui.mm` |
| `DOC:n` | `docs/ONLINE.md` |
| mapa 03, 04, 06, 07 | `../mapa/03-estado-del-juego.md`, etc. |

Lo que no se pudo comprobar en el código, en `docs/ONLINE.md` ni en los mapas va marcado **[sin verificar]**.
Nada de lo escrito aquí se ha ejecutado todavía: son lecturas de código. La primera ejecución real es F1.

## Decisiones del dueño y propuestas

En cada caso, las reglas van marcadas así:

- **[DUEÑO]**: decisión ya tomada por el dueño del proyecto. No se discute en la implementación. Las que salen de
  las rondas de respuestas se citan como `(ADR-00x, decisión n)`, con la pregunta que cierran.
- **[PROPUESTA]**: idea aceptada como propuesta; hay que confirmarla antes de construirla.

Decisiones del dueño (se citan por su código):

| Código | Decisión |
|---|---|
| D1 | Dos jugadores recorren la historia juntos (más jugadores, más adelante). |
| D2 | Cada jugador es un Link completo; solo cambia el color. |
| D3 | El mundo y el progreso son los del anfitrión, compartidos, y el anfitrión manda. |
| D4 | Cada jugador conserva su inventario, sus corazones y su resistencia. |
| D5 | Cada jugador puede hablar con los NPC por su cuenta. |
| D6 | Los momentos de historia necesitan a los dos presentes para continuar. |
| D7 | Conexión directa: un jugador hospeda. |
| D8 | Cooperativo por defecto; el PvP se activa solo si los dos lo aceptan. |
| D9 | Ayudas de juego: teletransporte al compañero, recoger lo que el compañero soltó y enviar objetos. |

Cómo quedan D1–D9 tras las respuestas del dueño (el detalle está en cada caso):

| Código | Precisión | ADR y decisión |
|---|---|---|
| D1 | Límite de 2 jugadores por ahora; lo nuevo se diseña para N y se prueba con 2. Contraseña opcional | ADR-007, 12 |
| D2 | Tinte suave sobre todo el Link del compañero; el propio se ve normal. Si no es viable: nombre en color y marcador con forma | ADR-007, 8; ADR-009, 22 |
| D3 | El invitado juega con un perfil cooperativo aparte que nace vacío; su partida individual no se toca. Encargos y conversaciones con NPC: progreso común | ADR-007, 1, 5 y 9 |
| D4 | Orbes, contenedores de corazón y habilidades de campeón: para los dos, siempre, también al unirse tarde; el invitado iguala los contenedores y las mejoras de resistencia del anfitrión. Cofres: para quien los abre. Rupias: de cada jugador | ADR-007, 2; ADR-008, 13, 14 y 19 |
| D6 | Momentos de historia = lista corta (inicio: piedra y primera torre; final de cada Bestia Divina; recuerdos; jefes finales); los santuarios no. Presente = mismo mapa y a menos de 30 m, con oferta de teletransporte. El anfitrión puede continuar solo de inmediato tras confirmar. Versión 1: basta con estar allí | ADR-007, 3, 4 y 6; ADR-009, 21 |
| D7 | Bibliotecas de terceros aceptadas (miniupnpc, ENet…) con licencia compatible con MIT y anotadas en `docs/third-party.md`. Si la conexión directa es imposible no se ofrece nada: solo un error claro. Reserva del sitio al desconectarse: 10 minutos | ADR-007, 11; ADR-009, 23; ADR-008, 18 |
| D8 | PvP: cuerpo a cuerpo y bombas; el anfitrión puede prohibirlo y valida el daño | ADR-008, 17 |
| D9 | Teletransporte al compañero siempre disponible, también en combate, con 30 s de espera; invocar al otro requiere que acepte; entre mapas, con pantalla de carga y guardado. Dar objetos: soltar marcado, solo materiales y comida; nunca objetos protegidos ni rupias; armas, cuando viajen durabilidad y bonificaciones | ADR-008, 16 y 20; ADR-007, 10; ADR-009, 24 y 25 |

Propuestas P1–P8 tras las respuestas: ya son decisión del dueño la vida del compañero siempre visible (P1, ADR-009,
27), revivir automático con rescate manual planificado para después (P3, ADR-007, 7; ADR-008, 15), recompensas de
santuario para los dos y cofre para quien lo abre (P4), invocar con consentimiento (P5, ADR-008, 16), el plazo de
reconexión (P6, ADR-008, 18), las ocho frases rápidas (P7, ADR-009, 28) y color y forma por orden de entrada,
editables en Opciones (P8, ADR-009, 26). Siguen como propuesta: los
pings (P2) y los demás detalles de la reconexión (P6). El marcador en el mapa del juego quedó decidido: no en F4
(ADR-011, decisión 41).

## Montaje común de prueba (dos juegos en un PC)

Sale de `docs/ONLINE.md:738-769` y del mapa 06 §7.3. Cada criterio de aceptación que diga "montaje común" se refiere
a esto:

- Dos procesos, cada uno con su carpeta de guardado **copiada** (`SAVAGEWIND_SAVE_DIR`, nunca la que está en uso),
  `SAVAGEWIND_NO_LAUNCHER=1`, `SAVAGEWIND_HIDDEN=1`, `SAVAGEWIND_AUDIO_GAIN=0`.
- Puerto de sesión de prueba: `SAVAGEWIND_ONLINE_PORT=52530` (no el 52520 de una partida real).
- Juego A: `SAVAGEWIND_ONLINE=host`, `SAVAGEWIND_ONLINE_NAME=Host`, `SAVAGEWIND_CONTROL=7777`.
- Juego B: `SAVAGEWIND_ONLINE=127.0.0.1`, `SAVAGEWIND_ONLINE_NAME=Client`, `SAVAGEWIND_CONTROL=7778`.
- Se conducen con `tools/swctl.py --port 7777 …` y `--port 7778 …` (`pos`, `teleport`, `goto <lugar>`,
  `actors <texto>`, `flag`, `setflag`, `event`, `give`, `press`/`hold`/`stick`, `bloodmoon`; ver `docs/CONTROL.md`).
- Los dos juegos juntos usan unos 9,5 GB de RAM: cerrar Docker Desktop y el navegador.
- El juego puede relanzarse una vez al arrancar (proceso hijo): un guion que vigile por PID debe buscar al hijo.
- Los overlays de ImGui no salen en el `shot` del control; para verlos hacen falta capturas de ventana
  (`SAVAGEWIND_WINDOW_SHOT`, mapa 04 §4a). Si funcionan con `SAVAGEWIND_HIDDEN=1` está **[sin verificar]**.
- Sin segundo juego: el bot `tools/online_bot.c` (se compila a mano; mapa 06 §7.2). Limitaciones: no envía PING ni
  SKEL, y su búfer de 8192 bytes se queda corto con más de 26 enemigos cerca del anfitrión.

Regla del plan: un caso cuenta como hecho cuando se ha visto funcionar en el juego y queda anotado en
`../bitacora/`, no cuando compila.
