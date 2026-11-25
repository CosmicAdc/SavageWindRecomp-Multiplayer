# F4 — Funciones cooperativas: especificación

Estado: borrador para revisión del dueño. Depende de F2 (partición de `online_actors.c` en módulos) y de F3
(autoridad en el servidor, reconexión, versión estricta).

Decisiones del dueño ya incorporadas (2026-11-24): `../../decisiones/ADR-007-respuestas-del-dueno-ronda-1.md`,
`ADR-008-respuestas-del-dueno-ronda-2.md`, `ADR-009-respuestas-del-dueno-ronda-3.md` y `ADR-011-respuestas-del-dueno-ronda-4.md` (decisiones 29-47). Se citan como
"ADR-00x, decisión n" y van marcadas `[DUEÑO]`. Si este documento dice otra cosa que un ADR, **vale el ADR**.

Convenciones:

- Referencias `fichero:línea` del checkout del PR #294 (`pr294-online`, HEAD `350c5f8`). `OA:n` =
  `runtime/online_actors.c` línea n. `DOC:n` = `docs/ONLINE.md` línea n. Tras F2 el código de `online_actors.c`
  vive en módulos `oa_*` (propuesta del mapa 07 §8): se busca por nombre de función.
- "[sin verificar]" = no comprobado leyendo código ni mapas.
- Regla dura: lo que falte por saber del juego sale del Ghidra del dueño, con su propia copia. No se copia nada de
  proyectos de descompilación ni se pegan desensamblados en el repo. Este documento **no contiene direcciones
  nuevas**; las que cita ya están en el código del PR.

## 1. Objetivo

Convertir "cada uno juega su partida y se ven" en un cooperativo de verdad con las reglas que decidió el dueño:

1. El mundo y el progreso son los del **anfitrión** (hoy es la unión de los de todos).
2. Cada jugador conserva **su** inventario, corazones y resistencia; el personaje del invitado se guarda en su PC,
   en un perfil cooperativo ligado al mundo de ese anfitrión, que **nace vacío**: no trae nada de su partida
   individual ni se lleva nada a ella (`[DUEÑO]` ADR-007, decisiones 1 y 5).
3. Las recompensas (orbes, contenedores de corazón, habilidades de campeón) son **para los dos, siempre**, también
   para quien se une tarde: al entrar en un mundo avanzado el invitado se pone al día con los mismos contenedores de
   corazón y mejoras de resistencia que el anfitrión (`[DUEÑO]` ADR-007, decisión 2; ADR-008, decisión 13).
4. Ayudas para jugar juntos: teletransporte al compañero, soltar objetos marcados para él, marcadores, pings, chat
   rápido, revivir.
5. Cooperativo por defecto; PvP solo si los dos lo aceptan y el anfitrión no lo ha prohibido, con el daño validado
   por el anfitrión (`[DUEÑO]` ADR-008, decisión 17).
6. Accesibilidad: nada depende solo del color.

Límite de jugadores: **2 por ahora**. Las funciones nuevas se diseñan para N jugadores pero se prueban con 2; la
contraseña de sesión es opcional (`[DUEÑO]` ADR-007, decisión 12; el acceso se construye en F3).

## 2. Casos de uso que cubre

| CU | Hoy | Qué entrega F4 |
|---|---|---|
| CU-06 Mundo y progreso del anfitrión | Unión de flags de todos, escrita en el guardado de todos (OA:3851-3936) | El anfitrión decide; el invitado no vuelca su partida; copia de seguridad antes de unirse |
| CU-07 Inventario y corazones propios | Cada juego tiene su guardado, pero orbes, contenedores de corazón, runas, semillas y paravela se **regalan** a todos al llegar el flag (OA:3987-3997) | Política de recompensas explícita (para los dos, siempre); perfil cooperativo del invitado por mundo, que nace vacío; "ponerse al día al entrar" en un mundo avanzado |
| CU-08 Teletransporte al compañero (mismo mapa) | No existe. Hay `control_tp_request` (`runtime/control_tp.c:298`), solo activo en modo desarrollador (`runtime/control_game.c:36-40`) | Ir con el compañero siempre disponible (también en combate), 30 s de espera y aterrizaje seguro; invocar al otro con su aceptación |
| CU-09 Enviar o soltar objetos | Lo que se suelta se comparte y el otro lo recoge (DOC:528-547). No hay envío directo ni función para quitar del inventario | Primera entrega: "soltar marcado para el compañero", **solo materiales y comida**. Envío directo: después, si la investigación lo permite |
| CU-10 Marcadores, pings, nombre en color, accesibilidad | Figura, nombre, distancia y flecha de borde en el overlay con 8 colores fijos por ranura (`runtime/ui/online_ui.mm:20-23`, `:181-304`) | Color y forma por orden de entrada y cambiables en Opciones, pings, chat rápido de ocho frases, avisos en pantalla, vida del compañero siempre visible |
| CU-11 Revivir al compañero | Reanimación automática instantánea por hada (OA:2825-2831) | Primera entrega: reanimación automática mientras el compañero siga en pie; si caen los dos, fin de partida normal. El rescate manual (estado "caído") queda planificado para después |
| CU-12 PvP por acuerdo, daño validado | Interruptor por jugador; el atacante declara el daño en su STATE y la víctima lo aplica (OA:2874-2942) | Acuerdo mutuo como regla de sesión, que el anfitrión puede prohibir; el anfitrión valida cada golpe; solo cuerpo a cuerpo y bombas |

Además (forma parte de CU-06/CU-07): las recompensas de santuario son **para los dos**. El contenido de un cofre es
**de quien lo abre**, que puede soltarlo marcado para el compañero; se revisa después de probarlo (`[DUEÑO]` ADR-008,
decisión 14). Las rupias son de cada jugador (`[DUEÑO]` ADR-008, decisión 19). Los encargos y conversaciones con NPC
son progreso común: lo que uno avanza queda hecho en el mundo y cada uno puede repetir la conversación (`[DUEÑO]`
ADR-007, decisión 9).

## 3. Alcance

### 3.1 Dentro

| # | Elemento | Requiere ingeniería inversa nueva |
|---|---|---|
| A | Flags autoritativos del anfitrión con línea base por invitado | No |
| B | Copia de seguridad automática y perfil de guardado cooperativo | No (un spike de runtime) |
| C | Flags idénticos a los del anfitrión al cargar (fijar y borrar en el guardado al leerlo) | No (spike de runtime); opcional |
| D | Política de recompensas por jugador y libro de entregas | No |
| D2 | Ponerse al día al entrar en un mundo avanzado (contenedores de corazón, mejoras de resistencia, habilidades de campeón) | Por comprobar: la vía para fijar la resistencia máxima no está en el código **[sin verificar]** |
| E | Recompensa de santuario para ambos (el orbe, por la política D). El contenido de cofre **no** se copia: es de quien lo abre | No |
| F | Teletransporte al compañero en el mismo mapa | No |
| G | "Soltar marcado para el compañero" (materiales y comida) | No |
| H | Envío directo: transacción, recepción y quitar del inventario del emisor (**segunda entrega**) | **Sí** (quitar) |
| I | Enviar armas con durabilidad y modificador (**fuera hasta que viajen por la red**) | **Sí** |
| J | Marcadores, color de nombre, formas, pings, chat rápido, avisos, vida del compañero | No |
| K | Reanimación automática mientras el compañero siga en pie (sin estado caído) | No |
| L | Rescate manual: estado "caído" esperando al compañero (**entrega posterior, prioridad baja**) | **Sí** |
| M | PvP por acuerdo y daño validado por el anfitrión | No |

Los elementos H, I y L tienen una tarea de investigación delante y **no forman parte de la primera entrega** de F4
(`[DUEÑO]` ADR-007, decisiones 7 y 10; ADR-009, decisión 24): la fase se cierra sin ellos. Sus tareas se conservan,
con prioridad baja.

### 3.2 Fuera

- Teletransporte entre mapas, jugar santuarios y Bestias Divinas juntos, momentos de historia, tinte 3D del segundo
  Link, colisión física con el compañero, más de 2 jugadores: F5.
- Marcadores en el mapa o minimapa del propio juego (es un layout nativo; hoy el runtime solo sabe reconocerlo,
  mapa 04 §4a): F5 o posterior.
- Flechas y runas en PvP (`[DUEÑO]` ADR-008, decisión 17), derribo real.
- Dar o marcar para el compañero armas, arcos y escudos, hasta que la durabilidad y las bonificaciones viajen por
  la red (cambio de protocolo con su subida de versión). Nunca se envían rupias ni los objetos que el juego ya
  protege: objetos clave, Espada Maestra y lo que el juego impide soltar (`[DUEÑO]` ADR-009, decisiones 24 y 25).
- Copia del contenido de un cofre para el compañero (`[DUEÑO]` ADR-008, decisión 14; se revisa tras probar).
- Monedero común de rupias (`[DUEÑO]` ADR-008, decisión 19).
- Intercambio con ventana de comercio a dos bandas (solo "dar").
- Voz, texto a voz.

## 4. Requisitos

### A–C. Mundo del anfitrión y guardados

- **R4-01** El anfitrión nunca aplica el conjunto completo de flags de un invitado. Solo acepta **cambios ocurridos
  durante la sesión** respecto a la línea base que ese invitado envió al entrar.
- **R4-02** Cada cambio aceptado pasa por una lista de categorías permitidas y un tope de cambios por segundo. Un
  invitado con todos los bits a 1 no altera el mundo del anfitrión.
- **R4-03** El invitado aplica los flags del anfitrión; lo que el invitado tenía de más no se propaga.
- **R4-04** Antes de crear o unirse a una sesión por primera vez con un guardado, se hace una copia de seguridad
  automática de ese guardado, incluso cuando se usa `SAVAGEWIND_SAVE_DIR` (hoy la copia automática se salta en ese
  caso, `runtime/save_backup.c:57`).
- **R4-05** `[DUEÑO]` (ADR-007, decisiones 1 y 5) El invitado juega en un perfil de guardado cooperativo propio de
  ese mundo (identificador de mundo generado por el anfitrión). El perfil **nace vacío**: no se copia nada de su
  partida individual, y nada de la sesión se escribe en ella. Su partida en solitario no cambia nunca por jugar en
  línea.
- **R4-06** El perfil cooperativo vive solo en el PC del invitado y conserva entre sesiones el inventario, los
  corazones y la resistencia que consiga en ese mundo.
- **R4-07** (opcional, tras el spike) Al cargar el perfil cooperativo, los flags compartidos se igualan a los del
  anfitrión.

### D–E. Recompensas

- **R4-08** Ninguna recompensa se entrega por el mero hecho de recibir un flag. Cada entrega pasa por una tabla de
  política; la tabla ya está decidida por el dueño (`plan.md` §D).
- **R4-09** Un libro de entregas por perfil evita entregar dos veces lo mismo (reconexión, reinicio).
- **R4-10** `[DUEÑO]` (ADR-007, decisión 2) Orbes, contenedores de corazón y habilidades de campeón son para los
  dos, siempre: cuando un jugador completa un santuario o libera una Bestia durante la sesión, el otro recibe la
  misma recompensa, esté donde esté.
- **R4-28** `[DUEÑO]` (ADR-007, decisión 2; ADR-008, decisión 13) Al entrar en un mundo avanzado, el invitado recibe
  el **mismo número de contenedores de corazón y de mejoras de resistencia** que el anfitrión tiene en ese momento,
  más las habilidades de campeón ya ganadas. Entra sin objetos (R4-05) pero con la misma vida y resistencia.
- **R4-29** `[DUEÑO]` (ADR-008, decisiones 14 y 19) El contenido de un cofre es de quien lo abre; las rupias son de
  cada jugador. Ninguno de los dos se reparte ni se copia.

### F. Teletransporte

- **R4-11** `[DUEÑO]` (ADR-008, decisión 16) "Ir con mi compañero" está **siempre disponible, también en combate**:
  no necesita que el compañero lo acepte (su juego contesta solo con el punto de llegada; sin respuesta en 15 s, se
  cancela). "Traer a mi compañero" (invocar) envía una petición que el invocado **debe aceptar**; si no acepta o no
  responde, no se le mueve. Puede invocar cualquiera de los dos.
- **R4-12** Motivos de rechazo del **viajero**: evento en curso, muerto, cargando, en pausa, montado, en espera.
  Motivos del **destino**: mapa distinto, muerto o cargando. `[DUEÑO]` (ADR-011, decisión 37) **Un compañero en plena
  escena o conversación no impide viajar hacia él**: el viajero aparece cerca y no se interrumpe a nadie. `[DUEÑO]`
  (ADR-008, decisión 16) Espera de **30 s** entre usos, fija; sin más coste (ADR-011, decisión 36).
- **R4-13** El destino es un punto junto al compañero tomado cuando este está en el suelo; si no lo está (cayendo,
  planeando, nadando, escalando), se usa su última posición en el suelo de los últimos segundos.
- **R4-14** El anfitrión valida la petición (mismo mapa, enfriamiento) antes de reenviarla.

### G–I. Objetos

- **R4-30** `[DUEÑO]` (ADR-007, decisión 10; ADR-009, decisiones 24 y 25) La primera entrega es "soltar marcado
  para el compañero" y solo marca **materiales y comida**. Armas, arcos y escudos quedan excluidos hasta que la
  durabilidad y las bonificaciones viajen por la red. Nunca se envían rupias ni los objetos que el juego ya
  protege (objetos clave, Espada Maestra y lo que el juego impide soltar).
- **R4-31** Se comprueba si un arma, arco o escudo soltado puede acabar igualmente en manos del compañero por el
  botín de suelo compartido que ya existe (DOC:528-547); si es así, se impide o se documenta (ADR-009, consecuencia
  de la decisión 24).
- **R4-15** (envío directo, segunda entrega) Dar un objeto es una transacción: nunca se duplica ni se pierde si la
  conexión cae a mitad.
- **R4-16** (envío directo) El receptor comprueba que le cabe antes de aceptar.
- **R4-17** (envío directo) Los nombres de objeto se validan contra la lista del propio juego del receptor
  (`runtime/items.c` solo admite objetos con la etiqueta `CanGetPouch`, sin `Important`).

### J. Marcadores y accesibilidad

- **R4-18** `[DUEÑO]` (ADR-009, decisión 26) El color y la forma del marcador se asignan **por orden de entrada** y
  cada jugador puede cambiarlos en Opciones; salen de una paleta segura para daltonismo y viajan por la red.
- **R4-19** Ping: una marca en el mundo visible 8 s para todos, con 4 tipos (aquí, peligro, objeto, ayuda), cada uno
  con forma y sonido distintos **[el sonido depende de qué pueda reproducir el overlay: sin verificar]**.
- **R4-20** `[DUEÑO]` (ADR-009, decisión 28) Chat rápido: ocho frases fijas enviadas por número, que cada jugador ve
  en su idioma: "Ven aquí", "Espera", "Necesito ayuda", "Gracias", "Sí", "No", "Vamos", "Mira esto".
- **R4-32** `[DUEÑO]` (ADR-009, decisión 27) La vida del compañero se ve **siempre**, en pequeño, y se destaca por
  debajo del 25 %.
- **R4-21** Los avisos de red importantes aparecen en pantalla, no solo en el registro (hoy "A held back" solo se
  registra, OA:5999).
- **R4-22** Tamaño de texto del overlay ajustable; ningún estado depende solo del color.
- **R4-35** `[DUEÑO]` (ADR-011, decisión 41) No hay marcador en el mapa del propio juego en F4: junto a la flecha en pantalla
  se muestran la distancia y el **nombre del mapa** del compañero. Se estudia después.

### K–L. Revivir

- **R4-23** `[DUEÑO]` (ADR-007, decisión 7; ADR-008, decisión 15) Primera entrega: la reanimación es **automática**,
  como hace hoy el código, mientras el compañero siga en pie. Si caen los dos a la vez, **fin de partida normal**:
  los dos recargan su último guardado. No se añade límite de distancia.
- **R4-24** (entrega posterior, prioridad baja; tras la investigación) Rescate manual: estado "caído" con cuenta
  atrás; el compañero lo levanta manteniendo un botón.

### M. PvP

- **R4-25** El PvP es una regla de sesión que solo se activa si los dos jugadores lo aceptan; cualquiera puede
  apagarlo en el acto. `[DUEÑO]` (ADR-008, decisión 17) El anfitrión puede **prohibir** el PvP en su sesión: con la
  prohibición puesta no se puede ni proponer.
- **R4-26** `[DUEÑO]` (ADR-008, decisión 17) Cada golpe lo valida el juego del anfitrión (regla activa, distancia,
  alcance, ritmo, daño máximo según el arma equipada) antes de que la víctima lo aplique; hoy lo declara el
  atacante. Daño negativo, cero o desorbitado se descarta. Solo cuerpo a cuerpo y bombas; el alcance de las armas
  no cambia.
- **R4-27** Se ve quién tiene el PvP activo.
- **R4-33** `[DUEÑO]` (ADR-011, decisión 39) Si alguien muere en un duelo, el duelo se apaga y se aplica la reanimación
  normal; no se pierde ningún objeto.
- **R4-34** `[DUEÑO]` (ADR-011, decisión 40) No se puede abrir un duelo en santuarios ni durante escenas.

## 5. Criterios de salida (medibles)

| # | Criterio | Cómo se mide |
|---|---|---|
| S1 | El invitado no vuelca su partida | Bot que envía QUEST con todos los bits a 1 al entrar: 0 flags nuevos en el anfitrión (`swctl flag` sobre 10 flags de muestra) |
| S2 | Los cambios reales sí pasan | El invitado activa un santuario con el juego real: el flag aparece en el anfitrión en < 5 s |
| S3 | El invitado sigue al anfitrión | El anfitrión abre un cofre: el flag llega al invitado en < 5 s |
| S4 | Copia de seguridad | Tras "Unirse" existe una copia nueva con fecha en la carpeta de copias, también con `SAVAGEWIND_SAVE_DIR` |
| S5 | Partida en solitario intacta | Suma de comprobación de la carpeta del perfil normal del invitado igual antes y después de una sesión |
| S6 | Persistencia del invitado | Recoge 3 objetos en sesión, sale, vuelve a entrar: los tiene |
| S7 | Recompensas según política | Tabla de `plan.md` §D recorrida caso a caso con dos juegos; 0 duplicados tras reconectar |
| S8 | Teletransporte | 10 teletransportes al compañero (uno de ellos en combate): 10 llegadas a menos de 3 m, 0 caídas con daño, 0 dentro de eventos; un segundo intento antes de 30 s se rechaza; una invocación no aceptada no mueve a nadie |
| S9 | Soltar para el compañero | 3 materiales y 3 comidas soltados: marca y aviso en la otra pantalla; un arma, un arco, un escudo y rupias soltados: sin marca, y el resultado de R4-31 anotado |
| S10 | Marcadores | Captura con dos jugadores: nombre con color y forma elegidos; ping visible en ambos en < 0,5 s |
| S11 | Accesibilidad | Revisión con la simulación de daltonismo (`SAVAGEWIND_CVD_SIMULATE`, `runtime/colour_vision.h`): todos los estados distinguibles |
| S12 | Revivir | Compañero en pie: el caído se levanta solo. Los dos caídos a la vez: fin de partida normal para ambos. 10 repeticiones sin crash |
| S13 | PvP por acuerdo | Con uno solo activado: 0 daño. Con los dos: daño aplicado. Con el PvP prohibido por el anfitrión: no se puede proponer y 0 daño. Bot tramposo (daño 9999, negativo, a 500 m): 0 daño aplicado |
| S14 | Idiomas | `tools/i18n_check.py` → `0 missing`; los avisos nacen en `.c` por identificador y su literal vive en `coop_hud.mm` dentro de `tr()` (si no, el comprobador no los ve) |
| S15 | Regresión | Recorrido de 30 minutos de F1 sin crash |
| S16 | Ponerse al día al entrar | Un invitado con perfil cooperativo recién creado entra en un mundo avanzado: mismos contenedores de corazón y mejoras de resistencia que el anfitrión, habilidades de campeón ya ganadas, inventario vacío; al volver a entrar no recibe nada por segunda vez |

## 6. Riesgos

| # | Riesgo | Mitigación |
|---|---|---|
| 1 | Los flags y recompensas se escriben en guardados reales y no se pueden deshacer | R4-04 y R4-05 antes de cualquier otra tarea de flags; todas las pruebas con copias |
| 2 | El invitado conserva flags propios que el anfitrión no tiene (un cofre abierto para uno y cerrado para otro) | Aceptado en la primera entrega (línea base); R4-07 lo corrige si el spike sale bien |
| 3 | "Lo que hace un flag en el mundo puede verse solo tras una carga" (DOC:441-442) | Se documenta; la prueba S3 mide el flag, no el efecto visual |
| 4 | Cambiar de perfil de guardado exige reiniciar el juego dentro del proceso: la carpeta se decide una vez por arranque (`runtime/main.c:589`, `runtime/services/fs.c:2096`) | Spike T402 antes de construir; alternativa: elegir el perfil en la pantalla de inicio |
| 5 | Falsa Luna Carmesí: cargar otra partida a mitad de sesión se interpreta como Luna Carmesí (mapa 07, M7) | El cambio de perfil ocurre antes de entrar en sesión; prueba específica |
| 6 | El teletransporte lejano necesita el sitio opcional `tp_gate` (`tools/game_sites.py:565`); sin él un salto largo congela el mundo | Comprobar `g_site.tp_gate` y limitar la distancia si falta |
| 7 | La cola de `runtime/items.c` solo se vacía "en juego" (no en menús ni eventos) y admite 32 peticiones (`items.c:367-384`, `:420-460`) | La transacción espera confirmación; plazo y aborto |
| 8 | Quitar objetos del inventario sin conocer la función del juego puede corromper la bolsa | Sin investigación terminada no se implementa; la primera entrega es "soltar para el compañero" |
| 9 | La validación de PvP en el anfitrión añade un salto de red | Con 2 jugadores uno es el anfitrión: solo afecta a los golpes del invitado |
| 10 | Varias funciones nuevas necesitarían cambios en `tools/patches.py` (cambia `GAMELIB_ID`: todos recompilan ~15 min) | Agrupar todos los parches de F4 en una sola entrega |
| 11 | Muchos carriles quieren tocar el protocolo y la UI | Un salto de versión al principio (T409) y dueños únicos de `online.h`, `online_ui.mm`, `menu.mm`, `settings.*`, `i18n/*.inc` |
| 12 | Proyectar marcas con una cámara de otro fotograma las hace temblar (mapa 04 §4c) | Reutilizar la cámara que ya captura el PR en `exec.c` y descartar si tiene más de 0,5 s (`online_ui.mm:256`) |
| 13 | Un perfil cooperativo vacío es una partida nueva: el juego puede arrancar por la introducción, lejos del anfitrión **[sin verificar]** | Pregunta 5 del spike T402, antes de construir T414 |
| 14 | No hay en el código una vía para fijar la resistencia máxima; para los corazones solo existe `give_heart` (OA:3763), que entrega un contenedor cada vez **[sin verificar que sirva para igualar]** | Primer paso de T442: comprobarlo; si falta, parar y abrir investigación |
| 15 | Un arma soltada llega hoy al otro jugador por el botín de suelo, solo con su nombre (DOC:528-547) | R4-31: impedirlo o documentarlo en T423 |

## 7. Preguntas para el dueño

Resueltas en las cuatro rondas del 2026-11-24 (la respuesta manda sobre la propuesta que había aquí):

| # | Pregunta | Respuesta del dueño | Estado |
|---|---|---|---|
| P4-1 | Política de recompensas: ¿qué recibe cada jugador cuando el otro completa un santuario, abre un cofre, encuentra un Kolog, libera una Bestia? ¿Y al unirse a un mundo ya avanzado? | Orbes, contenedores y habilidades de campeón: para los dos, siempre, también al unirse tarde; al entrar iguala corazones y resistencia del anfitrión. Cofre: para quien lo abre. Rupias: de cada uno. Tabla en `plan.md` §D | RESUELTA (ADR-007, decisión 2; ADR-008, decisiones 13, 14 y 19) |
| P4-3 | Teletransporte: ¿consentimiento siempre o aceptación automática por defecto? ¿Enfriamiento? ¿Permitido en combate? | Ir con el compañero: siempre disponible, también en combate, 30 s de espera. Invocar al otro: requiere que acepte | RESUELTA (ADR-008, decisión 16) |
| P4-4 | Si los dos caen a la vez, ¿qué pasa? | Fin de partida normal: los dos recargan el último guardado | RESUELTA (ADR-008, decisión 15) |
| P4-5 | ¿Dar objetos sin límite o con restricciones (objetos clave nunca)? | Solo materiales y comida. Armas, arcos y escudos, no hasta que viajen durabilidad y bonificaciones. Nunca rupias ni lo que el juego ya protege | RESUELTA (ADR-009, decisiones 24 y 25) |
| P4-7 | ¿Se muestra la vida del compañero? | Siempre visible, pequeña; destacada por debajo del 25 % | RESUELTA (ADR-009, decisión 27) |

Siguen abiertas (se construye con su valor por defecto y se anota):

| # | Pregunta | Propuesta por defecto | Bloquea |
|---|---|---|---|
| P4-2 | ¿El invitado puede hacer avanzar el mundo del anfitrión cuando están separados (lejos, mismo mapa)? | Sí | T410 |
| P4-6 | ¿El PvP lo propone cualquiera o solo el anfitrión? (Ya decidido: el anfitrión puede prohibirlo, ADR-008, decisión 17) | Cualquiera propone; hacen falta los dos | T433 |
| P4-8 | ¿El anfitrión también hace copia de seguridad cada vez que hospeda o solo la primera vez por día? | Una por día de juego | T413 |
| P4-9 | Botón del ping y de la rueda de chat rápido (Q-10-5 de `../../casos-de-uso/PREGUNTAS-ABIERTAS.md`). No es una decisión de diseño sino una comprobación de qué botones están libres | Sin decidir: T436 revisa `docs/controls.md` y `runtime/input_bind.h` y propone una combinación al dueño | T436 (asignación en mando) |
| P4-10 | Al ponerse al día (R4-28): ¿los orbes que el anfitrión tiene **sin canjear** también se entregan al invitado? Los ADR solo hablan de contenedores y mejoras ya canjeados | No se entregan | T442 |
| P4-11 | Un invitado que **vuelve** a un mundo que avanzó sin él: ¿se le iguala otra vez al anfitrión o recibe los hitos que se perdió? | Recibe los hitos que se perdió (orbes, contenedores, habilidades), por el libro de entregas; así conserva su propio reparto entre corazones y resistencia | T442 |
