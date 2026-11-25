# F4 — Funciones cooperativas: especificación

Estado: borrador para revisión del dueño. Depende de F2 (partición de `online_actors.c` en módulos) y de F3
(autoridad en el servidor, reconexión, versión estricta).

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
   ligado al mundo de ese anfitrión.
3. Ayudas para jugar juntos: teletransporte al compañero, enviar objetos, marcadores, pings, chat rápido, revivir.
4. Cooperativo por defecto; PvP solo si los dos lo aceptan, con el daño validado por el anfitrión.
5. Accesibilidad: nada depende solo del color.

## 2. Casos de uso que cubre

| CU | Hoy | Qué entrega F4 |
|---|---|---|
| CU-06 Mundo y progreso del anfitrión | Unión de flags de todos, escrita en el guardado de todos (OA:3851-3936) | El anfitrión decide; el invitado no vuelca su partida; copia de seguridad antes de unirse |
| CU-07 Inventario y corazones propios | Cada juego tiene su guardado, pero orbes, contenedores de corazón, runas, semillas y paravela se **regalan** a todos al llegar el flag (OA:3987-3997) | Política de recompensas explícita; perfil de guardado cooperativo del invitado por mundo |
| CU-08 Teletransporte al compañero (mismo mapa) | No existe. Hay `control_tp_request` (`runtime/control_tp.c:298`), solo activo en modo desarrollador (`runtime/control_game.c:36-40`) | Petición con consentimiento, enfriamiento y aterrizaje seguro |
| CU-09 Enviar o soltar objetos | Lo que se suelta se comparte y el otro lo recoge (DOC:528-547). No hay envío directo ni función para quitar del inventario | Envío directo con transacción (si la investigación lo permite) y "soltar para el compañero" como mínimo |
| CU-10 Marcadores, pings, nombre en color, accesibilidad | Figura, nombre, distancia y flecha de borde en el overlay con 8 colores fijos por ranura (`runtime/ui/online_ui.mm:20-23`, `:181-304`) | Color elegible, formas, pings, chat rápido, avisos en pantalla, vida del compañero |
| CU-11 Revivir al compañero | Reanimación automática instantánea por hada (OA:2825-2831) | Revivir condicionado por el compañero; estado "caído" si la investigación lo permite |
| CU-12 PvP por acuerdo, daño validado | Interruptor por jugador; el atacante declara el daño en su STATE y la víctima lo aplica (OA:2874-2942) | Acuerdo mutuo como regla de sesión; el anfitrión valida cada golpe |

Además: recompensas de santuario y de cofre **para los dos** (forma parte de CU-06/CU-07).

## 3. Alcance

### 3.1 Dentro

| # | Elemento | Requiere ingeniería inversa nueva |
|---|---|---|
| A | Flags autoritativos del anfitrión con línea base por invitado | No |
| B | Copia de seguridad automática y perfil de guardado cooperativo | No (un spike de runtime) |
| C | Flags idénticos a los del anfitrión al cargar (fijar y borrar en el guardado al leerlo) | No (spike de runtime); opcional |
| D | Política de recompensas por jugador y libro de entregas | No |
| E | Recompensas de cofre y santuario para ambos | Spike de observación; no hace falta Ghidra si el punto existente basta |
| F | Teletransporte al compañero en el mismo mapa | No |
| G | Enviar objeto: recepción | No (reutiliza `runtime/items.c`) |
| H | Enviar objeto: quitar del inventario del emisor | **Sí** |
| I | Enviar armas con durabilidad y modificador | **Sí** |
| J | Marcadores, color de nombre, formas, pings, chat rápido, avisos, vida del compañero | No |
| K | Revivir condicionado (sin estado caído) | No |
| L | Estado "caído" esperando al compañero | **Sí** |
| M | PvP por acuerdo y daño validado por el anfitrión | No |

Los elementos H, I y L tienen una tarea de investigación delante y una alternativa sin ingeniería inversa para que
el caso de uso no quede bloqueado.

### 3.2 Fuera

- Teletransporte entre mapas, jugar santuarios y Bestias Divinas juntos, momentos de historia, tinte 3D del segundo
  Link, colisión física con el compañero, más de 2 jugadores: F5.
- Marcadores en el mapa o minimapa del propio juego (es un layout nativo; hoy el runtime solo sabe reconocerlo,
  mapa 04 §4a): F5 o posterior.
- Flechas y runas en PvP, derribo real.
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
- **R4-05** El invitado juega en un perfil de guardado cooperativo propio de ese mundo (identificador de mundo
  generado por el anfitrión). Su partida en solitario no cambia nunca por jugar en línea.
- **R4-06** El perfil cooperativo conserva inventario, corazones y resistencia del invitado entre sesiones.
- **R4-07** (opcional, tras el spike) Al cargar el perfil cooperativo, los flags compartidos se igualan a los del
  anfitrión.

### D–E. Recompensas

- **R4-08** Ninguna recompensa se entrega por el mero hecho de recibir un flag. Cada entrega pasa por una tabla de
  política decidida por el dueño (pregunta P4-1).
- **R4-09** Un libro de entregas por perfil evita entregar dos veces lo mismo (reconexión, reinicio).
- **R4-10** Cuando un jugador abre un cofre o completa un santuario durante la sesión, el otro recibe la misma
  recompensa según la política, esté donde esté en el mismo mapa (o según decida P4-1).

### F. Teletransporte

- **R4-11** "Ir con mi compañero" envía una petición; el compañero acepta o rechaza (o tiene activada la aceptación
  automática). Sin respuesta en 15 s, se cancela.
- **R4-12** Solo en el mismo mapa, fuera de eventos, ninguno muerto, cargando ni en pausa; enfriamiento
  configurable por el anfitrión.
- **R4-13** El destino es un punto junto al compañero tomado cuando este está en el suelo; si no lo está (cayendo,
  planeando, nadando, escalando), se usa su última posición en el suelo de los últimos segundos.
- **R4-14** El anfitrión valida la petición (mismo mapa, enfriamiento) antes de reenviarla.

### G–I. Objetos

- **R4-15** Dar un objeto es una transacción: nunca se duplica ni se pierde si la conexión cae a mitad.
- **R4-16** El receptor comprueba que le cabe antes de aceptar.
- **R4-17** Los nombres de objeto se validan contra la lista del propio juego del receptor (`runtime/items.c`
  solo admite objetos con la etiqueta `CanGetPouch`, sin `Important`).

### J. Marcadores y accesibilidad

- **R4-18** Cada jugador elige color y forma de una paleta segura para daltonismo; ambos viajan por la red.
- **R4-19** Ping: una marca en el mundo visible 8 s para todos, con 4 tipos (aquí, peligro, objeto, ayuda), cada uno
  con forma y sonido distintos **[el sonido depende de qué pueda reproducir el overlay: sin verificar]**.
- **R4-20** Chat rápido: 8 frases fijas enviadas por número; cada jugador las ve en su idioma.
- **R4-21** Los avisos de red importantes aparecen en pantalla, no solo en el registro (hoy "A held back" solo se
  registra, OA:5999).
- **R4-22** Tamaño de texto del overlay ajustable; ningún estado depende solo del color.

### K–L. Revivir

- **R4-23** Revivir depende del compañero: si está vivo y a menos de la distancia configurada, el caído se levanta;
  si no, fin de partida normal.
- **R4-24** (tras la investigación) Estado "caído" con cuenta atrás; el compañero lo levanta manteniendo un botón.

### M. PvP

- **R4-25** El PvP es una regla de sesión que solo se activa si los dos jugadores lo aceptan; cualquiera puede
  apagarlo en el acto.
- **R4-26** Cada golpe lo valida el juego del anfitrión (regla activa, distancia, alcance, ritmo, daño máximo según
  el arma equipada) antes de que la víctima lo aplique. Daño negativo, cero o desorbitado se descarta.
- **R4-27** Se ve quién tiene el PvP activo.

## 5. Criterios de salida (medibles)

| # | Criterio | Cómo se mide |
|---|---|---|
| S1 | El invitado no vuelca su partida | Bot que envía QUEST con todos los bits a 1 al entrar: 0 flags nuevos en el anfitrión (`swctl flag` sobre 10 flags de muestra) |
| S2 | Los cambios reales sí pasan | El invitado activa un santuario con el juego real: el flag aparece en el anfitrión en < 5 s |
| S3 | El invitado sigue al anfitrión | El anfitrión abre un cofre: el flag llega al invitado en < 5 s |
| S4 | Copia de seguridad | Tras "Unirse" existe una copia nueva con fecha en la carpeta de copias, también con `SAVAGEWIND_SAVE_DIR` |
| S5 | Partida en solitario intacta | Suma de comprobación de la carpeta del perfil normal del invitado igual antes y después de una sesión |
| S6 | Persistencia del invitado | Recoge 3 objetos en sesión, sale, vuelve a entrar: los tiene |
| S7 | Recompensas según política | Tabla del ADR recorrida caso a caso con dos juegos; 0 duplicados tras reconectar |
| S8 | Teletransporte | 10 teletransportes seguidos al compañero: 10 llegadas a menos de 3 m, 0 caídas con daño, 0 dentro de eventos |
| S9 | Dar objetos | 20 envíos con cortes de red aleatorios (bot): suma de objetos en ambos inventarios constante |
| S10 | Marcadores | Captura con dos jugadores: nombre con color y forma elegidos; ping visible en ambos en < 0,5 s |
| S11 | Accesibilidad | Revisión con la simulación de daltonismo (`SAVAGEWIND_CVD_SIMULATE`, `runtime/colour_vision.h`): todos los estados distinguibles |
| S12 | Revivir | Compañero cerca: se levanta. Compañero lejos o muerto: fin de partida. 10 repeticiones sin crash |
| S13 | PvP por acuerdo | Con uno solo activado: 0 daño. Con los dos: daño aplicado. Bot tramposo (daño 9999, negativo, a 500 m): 0 daño aplicado |
| S14 | Idiomas | `tools/i18n_check.py` → `0 missing` |
| S15 | Regresión | Recorrido de 30 minutos de F1 sin crash |

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
| 8 | Quitar objetos del inventario sin conocer la función del juego puede corromper la bolsa | Sin investigación terminada no se implementa; alternativa "soltar para el compañero" |
| 9 | La validación de PvP en el anfitrión añade un salto de red | Con 2 jugadores uno es el anfitrión: solo afecta a los golpes del invitado |
| 10 | Varias funciones nuevas necesitarían cambios en `tools/patches.py` (cambia `GAMELIB_ID`: todos recompilan ~15 min) | Agrupar todos los parches de F4 en una sola entrega |
| 11 | Muchos carriles quieren tocar el protocolo y la UI | Un salto de versión al principio (T409) y dueños únicos de `online.h`, `online_ui.mm`, `menu.mm`, `settings.*`, `i18n/*.inc` |
| 12 | Proyectar marcas con una cámara de otro fotograma las hace temblar (mapa 04 §4c) | Reutilizar la cámara que ya captura el PR en `exec.c` y descartar si tiene más de 0,5 s (`online_ui.mm:256`) |

## 7. Preguntas abiertas para el dueño

| # | Pregunta | Propuesta por defecto | Bloquea |
|---|---|---|---|
| P4-1 | Política de recompensas: ¿qué recibe cada jugador cuando el otro completa un santuario, abre un cofre, encuentra un Kolog, libera una Bestia? ¿Y al unirse a un mundo ya avanzado? | Ver tabla de `plan.md` §D | T401 → T415-T417 |
| P4-2 | ¿El invitado puede hacer avanzar el mundo del anfitrión cuando están separados (lejos, mismo mapa)? | Sí | T410 |
| P4-3 | Teletransporte: ¿consentimiento siempre o aceptación automática por defecto? ¿Enfriamiento? ¿Permitido en combate? | Consentimiento; 30 s; no en combate | T418 |
| P4-4 | Si los dos caen a la vez, ¿qué pasa? | Fin de partida para ambos; cada uno recarga su último guardado | T430 |
| P4-5 | ¿Dar objetos sin límite o con restricciones (objetos clave nunca)? | Nunca objetos clave; el resto libre | T420 |
| P4-6 | ¿El PvP lo propone cualquiera o solo el anfitrión? | Cualquiera propone; hacen falta los dos | T433 |
| P4-7 | ¿Se muestra la vida del compañero? | Sí, corazones bajo su nombre | T429 |
| P4-8 | ¿El anfitrión también hace copia de seguridad cada vez que hospeda o solo la primera vez por día? | Una por día de juego | T413 |
