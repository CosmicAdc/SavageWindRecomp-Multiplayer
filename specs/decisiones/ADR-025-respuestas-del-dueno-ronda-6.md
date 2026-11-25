# ADR-025 · Respuestas del dueño, ronda 6 (estados del juego Q-EST-1..8 y hallazgo I-2 de la revisión de F2)

- Fecha: 2026-11-25
- Estado: aceptado
- Decide: el dueño, en diálogo con el orquestador
- Fuentes: `specs/mapa/08-estados-del-juego.md` (§3, preguntas Q-EST) y
  `specs/bitacora/2026-11-25-F2-revision-independiente.md` (I-2)

## Decisiones

| # | Pregunta | Respuesta del dueño | Propuesta del mapa 08 |
|---|---|---|---|
| 1 | **I-2** · Un subcampo malo de STATE tira el mensaje entero | **Descartar solo la entrada**: el STATE se descarta solo por los campos núcleo (`pos`, `yaw`, `flags`, `anim`, `as`, reglas de sesión); una entrada mala de una lista se anula y el resto se aplica, como ya hacen ENEMIES y LOOTS | (propuesta del revisor) |
| 2 | **Q-EST-1** · "Salir al inicio" o reinicio por idioma o mods con la sesión abierta | **Se cierra la sesión**: el reinicio manda LEAVE y el juego empieza como recién arrancado; con la reconexión de F3 el invitado vuelve a unirse solo | igual |
| 3 | **Q-EST-2** · Quién mueve la hora durmiendo o esperando junto a la hoguera | **Solo el anfitrión cambia la hora.** El invitado **sí** puede usar la cama o la hoguera: se duerme y **espera a que el anfitrión elija la hora** (en su cama u hoguera); entonces la hora salta para los dos | difiere: la propuesta era bloquear al invitado con un aviso |
| 4 | **Q-EST-3** · Reloj del invitado con el anfitrión en una escena, cargando o en el título | **El invitado sigue con su reloj** y se iguala cuando el anfitrión vuelve al mundo (respuesta de la ronda: "solo el anfitrión"; sin objeción a este punto) | igual |
| 5 | **Q-EST-4** · Luna de Sangre del anfitrión en el juego del invitado | **Los dos ven la escena** | difiere: la propuesta era aviso y reinicio sin escena |
| 6 | **Q-EST-5** · Master Mode en uno y normal en el otro | **No**: se rechaza al conectar con un motivo claro (como el DLC distinto) | igual |
| 7 | **Q-EST-6** · Eventide y Prueba de la Espada | **Entran los dos juntos**: ambos pierden el inventario y hacen el reto a la vez | difiere: la propuesta era "el reto es de quien entra" |
| 8 | **Q-EST-7** · Modo dios y "daño recibido" con daño en línea | **Sí, a todo el daño** que recibe tu Link (enemigos de otro dueño, PvP, bombas) | igual |
| 9 | **Q-EST-8** · amiibo durante una sesión | **No durante la sesión**: se bloquean mientras hay sesión abierta | difiere: la propuesta era permitirlos en local |

## Consecuencias

- **I-2** → tarea nueva en F2 (capa de validación, `runtime/online_validate.h/.c` y `contratos/validacion-red.md`):
  STATE por campos núcleo, listas por entrada. Toca la regla "nunca reparar" solo en el sentido de anular una entrada,
  no de corregir su valor. Va junto con I-1 (cota de `bloody_moon`) e I-3 (pose de carga a cero en el emisor).
- **Q-EST-1** → T212 (y HE-01): `RT_ON_RESET` en `oa_*` y un LEAVE limpio desde `online.c` al reiniciar.
- **Q-EST-2/3** → CU-06 y T410 (HE-07). El invitado necesita un estado "durmiendo, esperando la hora del
  anfitrión": la escena de dormir del invitado no termina (o vuelve a empezar) hasta que llega la hora nueva; si el
  anfitrión no duerme, el invitado puede cancelar. Requiere investigar cómo se corta la escena de la cama
  (`Demo` de posada y hoguera) antes de programar: tarea de investigación primero.
- **Q-EST-4** → T411 (HE-08): lanzar la escena de la Luna de Sangre en el invitado. Riesgo conocido: escenas lanzadas
  fuera de contexto pueden colgarse (mapa 07 §5 n.º 31). Requiere investigación (qué dispara la escena, si se puede
  lanzar con el invitado en cualquier estado, qué hacer si está en un menú, una escena o cargando).
- **Q-EST-5** → T314/T325: comparar Master Mode en el saludo.
- **Q-EST-6** → HE-16, tarea nueva de F4 (investigación primero): cuando uno entra en Eventide o en la Prueba de la
  Espada, el otro entra con él y pierde también el inventario. Hay que ver cómo se dispara la entrada, cómo se guarda y
  devuelve el inventario y qué pasa si uno sale antes.
- **Q-EST-7** → HE-10, T434: `oa_strike`, `oa_own_strikes_in_menu` y `oa_pvp_update` pasan por la misma regla que
  `rt_link_life` (`god_mode` y `damage_taken`). Arregla N1.
- **Q-EST-8** → tarea nueva pequeña: con sesión abierta, el lector de amiibo (`services/nfp.c`) no devuelve
  ninguna figura (y un aviso la primera vez).

## Adenda (2026-11-25, tras T451)

| # | Pregunta | Respuesta del dueño |
|---|---|---|
| 10 | Q-EST-6 bis · Si uno entra en Eventide o en la Prueba de la Espada y el compañero no quiere o no puede | **Pregunta y espera**: al compañero le sale un aviso aceptar/rechazar con tiempo límite (≈ 30 s). Si rechaza o no contesta, quien entró hace el reto solo y el compañero no puede ayudarle mientras dure (aislamiento de T451) |
| 11 | Invitación a santuario: al aceptar, ¿a dónde va el compañero? | **A la puerta, entra andando**: teletransporte a la entrada exterior del santuario; entra él mismo por el ascensor (usa la entrada normal del juego) |
| 12 | ¿Y si el compañero no ha descubierto ese santuario? | **Puede unirse igual**; al entrar queda descubierto en su partida, como si hubiera llegado andando |
| 13 | ¿Cuándo sale la invitación? | **Siempre**, cada vez que uno entra a un santuario: aviso aceptar/rechazar (≈ 30 s), con opción en ajustes para apagarlo |

Consecuencia: tarea nueva **T465** (F4, invitación a santuario), que depende de la investigación **T540** (F5, adelantada).
| 14 | Invitación: si la puerta está cerrada en la partida del compañero | **Se abre sola** al aceptar (queda activado el santuario en su partida). Resuelve T465d |
| 15 | Alcance de la invitación | **Solo santuarios normales** (`CDungeon/DungeonNNN`); Bestias Divinas y otros interiores, después |
| 16 | ¿A se entera de que B viene o rechazó? | **No**: sin mensaje nuevo (T465e cerrada; sin T409) |
| 17 | ¿Se invita si el compañero ya estaba dentro al unirnos? | No (valor por defecto de T540, no preguntado) |
