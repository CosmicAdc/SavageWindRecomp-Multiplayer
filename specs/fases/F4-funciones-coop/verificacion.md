# F4 — Funciones cooperativas: verificación de extremo a extremo

Se recorre entera al cerrar la fase (tarea T441). Cada casilla necesita evidencia en `specs/bitacora/`.

**Aviso:** F4 escribe flags y objetos en guardados y eso no se puede deshacer. **Todas** las pruebas se hacen con
copias de guardado. Guarda además una copia aparte de tu partida real antes de empezar.

Los resultados esperados ya recogen las respuestas del dueño del 2026-11-24 (`specs/decisiones/ADR-007-…`,
`ADR-008-…`, `ADR-009-…` y `ADR-011-…`). Las filas de funciones que quedan para después de la primera entrega (envío directo,
copia de cofres, rescate manual) están marcadas "(después)" y **no** cuentan para cerrar F4.

## 0. Preparación

### 0.1 Sin juego

```
make test-online test-coop                 # pruebas unitarias (red y lógica cooperativa)
sh tools/nettest/run.sh all                # escenarios con bots en Docker (incluye los de F4)
tools/venv/Scripts/python.exe tools/i18n_check.py
```

Red degradada en Docker: `sh tools/nettest/netem.sh <perfil>` (`lan`, `fibra`, `normal`, `wifi`, `movil`, `rafagas`,
`corte`), que por dentro usa `tc qdisc ... netem` y necesita `cap_add: NET_ADMIN` en el contenedor.

### 0.2 Dos juegos en un PC

Procedimiento de `docs/ONLINE.md` líneas 738-769. Cierra Docker Desktop y el navegador antes (unos 9,5 GB de RAM).

Prepara **tres** copias de guardado:

| Copia | Para | Contenido recomendado |
|---|---|---|
| `A` | Anfitrión | Partida poco avanzada (Gran Meseta terminada) |
| `B` | Invitado | Partida **más avanzada** que A (para probar que no vuelca su progreso ni se le toca) |
| `B0` | Invitado "nuevo" | Partida recién empezada |

El perfil cooperativo del invitado **nace vacío** (ADR-007, decisiones 1 y 5): lo que haya en `B` o `B0` no entra
en la sesión. Estas copias sirven para comprobar justo eso, y que salen de la sesión sin un solo cambio. Para las
pruebas de "ponerse al día" hace falta además una copia `A+` del anfitrión **avanzada** (varios santuarios
canjeados por corazones y por resistencia, y al menos una Bestia Divina liberada).

Juego A (anfitrión), PowerShell:

```powershell
$env:SAVAGEWIND_SAVE_DIR="<copia-A>"; $env:SAVAGEWIND_NO_LAUNCHER="1"; $env:SAVAGEWIND_AUDIO_GAIN="0"
$env:SAVAGEWIND_ONLINE_PORT="52530"; $env:SAVAGEWIND_ONLINE="host"; $env:SAVAGEWIND_ONLINE_NAME="Host"
$env:SAVAGEWIND_CONTROL="7777"; .\build\savagewind.exe
```

Juego B (invitado): igual con `<copia-B>`, `SAVAGEWIND_ONLINE="127.0.0.1"`, `SAVAGEWIND_ONLINE_NAME="Client"`,
`SAVAGEWIND_CONTROL="7778"`.

Órdenes de control útiles (`tools\venv\Scripts\python.exe tools\swctl.py --port <p> ...`, ver `docs/CONTROL.md`):
`goto <lugar>`, `teleport x y z`, `flag <nombre>`, `setflag <nombre>`, `give <objeto> <n>`, `hearts`, `actors <texto>`,
`press`/`hold`/`stick`, `set god_mode true`.

Capturas con overlay: `SAVAGEWIND_WINDOW_SHOT=<fotograma>` (las de `shot` no incluyen el overlay).
Simular daltonismo: `SAVAGEWIND_CVD_SIMULATE=protan|deutan|tritan`.

Nota sobre el perfil cooperativo: con `SAVAGEWIND_SAVE_DIR` la carpeta la fija la variable. Para probar el cambio
automático de perfil (CU-07) usa en el juego B una **raíz de datos** propia (`SAVAGEWIND_DATA_ROOT=<carpeta>`) con
perfiles dentro, en vez de `SAVAGEWIND_SAVE_DIR`.

### 0.3 Un juego y un bot

`build/online_bot 127.0.0.1 52530 Bot 4` con las variables de F4 (T438): `BOT_FLAGS_ALL`, `BOT_FLAGS_ONE`,
`BOT_PVP_CHEAT`, `BOT_TP`, `BOT_TP_FAST`, `BOT_WORLD_FAKE`, `BOT_FLOOD=mark,<hz>`; y, solo si se hizo el envío
directo, `BOT_GIVE` y `BOT_GIVE_DROP`.

---

## 1. CU-06 — El mundo y el progreso son los del anfitrión

Elige 10 flags de muestra que la copia B tenga y la copia A no (santuarios completados, cofres). Anótalos.

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 6.1 | Lógica de línea base | `build/test_coop_flags` | Pasa | ☐ |
| 6.2 | Bot con todos los bits a 1 | `sh tools/nettest/run.sh flags_all_ones` | QUESTS del anfitrión sin cambios | ☐ |
| 6.3 | Invitado avanzado no vuelca su partida | A hospeda, B se une; espera 60 s; en A: `swctl --port 7777 flag <cada uno de los 10>` | Los 10 siguen a 0 en A; el inventario de A no ha recibido nada | ☐ |
| 6.4 | Lo que el invitado hace en sesión sí cuenta | B activa y completa un santuario que A no tenía | En < 5 s el flag está a 1 en A; aviso en pantalla | ☐ |
| 6.5 | El invitado sigue al anfitrión | A abre un cofre | El flag llega a B en < 5 s | ☐ |
| 6.6 | Ráfaga sospechosa | Bot con `BOT_FLAGS_ONE` modificado para activar 40 bits a la vez | El anfitrión los rechaza todos, registra y avisa | ☐ |
| 6.7 | Reconexión conserva la línea base | Corta al invitado 10 s (bot con `BOT_DROP`) y repite 6.3. El sitio se reserva 10 minutos (ADR-008, decisión 18; lo prueba F3) | Sigue sin volcarse nada | ☐ |
| 6.8 | Jugador nuevo en la ranura | Sale B, entra B0 en la misma ranura | La línea base es la de B0, no la de B | ☐ |
| 6.9 | Copia de seguridad | Tras "Create session" y tras "Join" | Carpeta `coop-<fecha>` nueva en las copias; también con `SAVAGEWIND_SAVE_DIR` | ☐ |
| 6.10 | Restaurar la copia | Sustituye la carpeta de guardado por la copia y arranca | La partida es la de antes de la sesión | ☐ |
| 6.11 | Luna Carmesí | `swctl bloodmoon` en A | Comportamiento igual que antes de F4 (los objetos vuelven en cada juego en su Luna) | ☐ |
| 6.12 | (Si se hizo R4-07) Mundo idéntico al cargar | B carga su perfil cooperativo tras unirse | Los 10 flags de muestra están a 0 en B dentro de ese perfil | ☐ |
| 6.13 | Encargos y NPC, progreso común (ADR-007, decisión 9) | B habla con un NPC y avanza un encargo; después A habla con el mismo NPC | El encargo consta como avanzado en el mundo de A; A puede repetir la conversación | ☐ |

## 2. CU-07 — Cada jugador conserva su inventario y corazones

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 7.1 | Política, unidad | `build/test_coop_rewards` | Pasa; coincide casilla a casilla con la tabla de `plan.md` §D | ☐ |
| 7.2 | Partida en solitario intacta (ADR-007, decisión 1) | Calcula una suma de comprobación de la carpeta del perfil normal de B (`find <carpeta> -type f -exec sha256sum {} + | sha256sum`); juega 10 min en sesión; sal; recalcula | Igual | ☐ |
| 7.3 | Perfil cooperativo creado **vacío** (ADR-007, decisión 5) | Primera unión de B (partida avanzada) al mundo de A | Existe `save-profiles/coop-<8 hex>` en la raíz de datos de B; la pestaña lo muestra; dentro de la sesión B no tiene **ningún** objeto, arma ni rupia de su partida individual | ☐ |
| 7.4 | Persistencia | En sesión, B recoge 3 objetos y pierde 2 corazones; sale; vuelve a unirse | Los 3 objetos siguen; corazones según su último guardado | ☐ |
| 7.5 | Otro anfitrión, otro perfil | B se une a un segundo mundo (otra copia de A con otro identificador) | Perfil distinto, también vacío; el primero no cambia | ☐ |
| 7.6 | Al unirse a un mundo avanzado | B0 se une a `A+` | Recibe las capacidades del mundo y las habilidades de campeón ya ganadas; **no** recibe orbes ni contenedores uno a uno (los sustituye la puesta al día de abajo) | ☐ |
| 7.7 | Santuario durante la sesión (ADR-007, decisión 2) | Uno completa un santuario, con el otro lejos | El otro recibe el orbe, una sola vez | ☐ |
| 7.8 | Sin duplicados | Repite 7.7 y corta la red del invitado justo después; reconecta | Sigue teniendo 1 orbe de ese santuario | ☐ |
| 7.9 | Kolog | Uno encuentra un Kolog con el otro lejos | Una semilla para cada uno, como hoy (`[DUEÑO]` ADR-011, decisión 30) | ☐ |
| 7.10 | Estatua de la Diosa | Cada uno canjea sus orbes | Cada uno elige lo suyo (corazón o resistencia), sin afectar al otro | ☐ |
| 7.11 | Bestia durante la sesión (ADR-007, decisión 2) | Uno libera una Bestia Divina (o `setflag` del flag correspondiente en una copia) | El otro recibe el contenedor de corazón y la habilidad de campeón, una sola vez | ☐ |
| 7.12 | Rupias de cada uno (ADR-008, decisión 19) | A recoge 50 rupias | El contador de B no cambia | ☐ |

### Ponerse al día al entrar (T442; ADR-007, decisión 2; ADR-008, decisión 13)

Anota antes los contenedores de corazón y las mejoras de resistencia de `A+`.

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| P.1 | Lógica | `build/test_coop_rewards` (casos de puesta al día) | Pasa | ☐ |
| P.2 | Primera entrada en un mundo avanzado | `A+` hospeda; un invitado sin perfil cooperativo para ese mundo se une | El invitado tiene el **mismo número** de contenedores de corazón y de mejoras de resistencia que `A+`; aviso en pantalla | ☐ |
| P.3 | Habilidades de campeón | En la misma sesión, mira el inventario del invitado | Tiene las habilidades de campeón que `A+` ya había ganado | ☐ |
| P.4 | Entra sin objetos | Igual | Inventario vacío: ni armas, ni materiales, ni orbes sin canjear | ☐ |
| P.5 | No se repite | El invitado sale y vuelve a entrar; después, corta su red 10 s y reconecta | Mismos corazones y resistencia que tras P.2; nada nuevo | ☐ |
| P.6 | Corte a mitad | Cierra el juego del invitado durante la puesta al día y vuelve a entrar | Termina igualado; no se pasa del número de `A+` | ☐ |
| P.7 | Nunca baja | El invitado gana un contenedor más que el anfitrión (con `give` en su copia) y vuelve a entrar | Conserva el que tiene de más | ☐ |
| P.8 | Números falsos | `sh tools/nettest/run.sh world_fake` y bot con `BOT_WORLD_FAKE=1` contra un juego invitado | El WORLD del bot no se acepta; corazones y resistencia del invitado no cambian | ☐ |
| P.9 | Entrada posterior tras avanzar el mundo | Con el invitado fuera, A completa un santuario; el invitado vuelve | Recibe ese orbe una vez (valor por defecto de P4-11; anota si el dueño lo cambia) | ☐ |
| P.10 | Partida individual intacta | Repite 7.2 alrededor de P.2 | Suma de comprobación igual | ☐ |

Si el primer paso de T442 dejó la resistencia como "no resuelto", P.2 se comprueba solo para corazones y se anota
como FALLO parcial con su tarea abierta.

### Cofres: para quien los abre (ADR-008, decisión 14)

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| C.1 | El contenido es de quien abre | A abre 5 cofres; anota el contenido | A recibe los 5 objetos; B **no** recibe ninguno; el cofre aparece abierto también en el mundo de B | ☐ |
| C.2 | Igual con el invitado | B abre 5 cofres | B recibe los 5; A ninguno | ☐ |
| C.3 | Se puede soltar para el compañero | A suelta un material sacado de un cofre | B ve marca y aviso y lo recoge (fila 9.3) | ☐ |
| C.4 | REWARD no hace nada | Bot que envía REWARD con cualquier nombre, sea o no anfitrión | Rechazado o ignorado; nada en el inventario | ☐ |

### Cofres para los dos (después; solo si el dueño lo pide tras revisar la decisión 14 y se hacen T408 y T417)

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| CD.1 | 5 cofres abiertos por el anfitrión | Anota el contenido de cada uno | El invitado recibe los mismos 5 objetos; 0 duplicados | ☐ |
| CD.2 | 5 cofres abiertos por el invitado | Igual | El anfitrión recibe los 5 | ☐ |
| CD.3 | Recogidas del suelo no se reparten | Recoge 5 objetos del suelo | El otro no recibe nada | ☐ |
| CD.4 | Inventario lleno | El receptor tiene la categoría llena | No se pierde ni se cuelga; aviso en pantalla | ☐ |
| CD.5 | Objeto inventado | Bot que envía REWARD con un nombre que no existe o sin ser anfitrión | Rechazado; nada en el inventario | ☐ |

## 3. CU-08 — Teletransportarse al compañero (mismo mapa)

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 8.1 | Lógica | `build/test_coop_tp` | Pasa | ☐ |
| 8.2 | Ir, siempre disponible (ADR-008, decisión 16) | B pulsa "Go to" sobre A; A no toca nada | B aparece a < 3 m de A, sin daño de caída; A ve un aviso; a A no se le pide permiso | ☐ |
| 8.3 | Sin respuesta | El juego de A no contesta en 15 s (bot que no responde a TP_REQ) | "No answer"; B no se mueve | ☐ |
| 8.4 | 10 viajes | 10 viajes a puntos distintos, respetando la espera de 30 s entre uno y otro | 10 llegadas a < 3 m; 0 con daño | ☐ |
| 8.5 | Compañero en el aire / agua / pared | A planeando, nadando y escalando | Se usa su último punto en el suelo; B no cae al vacío ni al agua | ☐ |
| 8.6 | Bloqueos del viajero y del destino | Viajero: evento en curso, muerto, en menú, a caballo. Destino: muerto, en otro mapa | Rechazo con su motivo en cada caso | ☐ |
| 8.6e | Compañero en plena escena (ADR-011, decisión 37) | A está en una escena o conversación; B pulsa "Go to" | B viaja y aparece cerca; la escena de A no se interrumpe | ☐ |
| 8.6b | En combate (ADR-008, decisión 16) | B con enemigos atacándole pulsa "Go to" | El viaje se hace; no hay rechazo "en combate" | ☐ |
| 8.6c | Espera de 30 s (ADR-008, decisión 16) | Tras un viaje, B pulsa otra vez a los 10 s y a los 31 s; además `sh tools/nettest/run.sh tp_cooldown` | A los 10 s: rechazo con los segundos que faltan. A los 31 s: viaja. El anfitrión no reenvía peticiones dentro de la espera | ☐ |
| 8.6d | Invocar, aceptando (ADR-008, decisión 16) | A pulsa "Bring here" sobre B; B acepta | B aparece a < 3 m de A | ☐ |
| 8.6e | Invocar, sin aceptar | A invoca; B rechaza; después B no responde 15 s | "Declined" / "No answer"; **B no se mueve** en ninguno de los dos casos | ☐ |
| 8.6f | Duelo de PvP activo (`[PROPUESTA]`) | Con PvP activo, B pulsa "Go to" | Lo que haya confirmado el dueño al cerrar T418; anótalo | ☐ |
| 8.7 | Mapa distinto | A dentro de un santuario, B fuera | Rechazo "en otro mapa" | ☐ |
| 8.8 | Salto largo | A y B a más de 1 km | Llega tras la carga del terreno; el mundo no se congela | ☐ |
| 8.9 | Punto falso | Bot con `BOT_TP=1` modificado para responder con coordenadas absurdas o no finitas | El juego rechaza el viaje | ☐ |
| 8.10 | No abre el modo desarrollador | Tras un viaje, la pestaña Teleport/Cheats sigue oculta y `swctl teleport` en el puerto de compañía falla | Sin cambios de permisos | ☐ |

## 4. CU-09 — Soltar objetos para el compañero

Primera entrega (ADR-007, decisión 10; ADR-009, decisiones 24 y 25): "soltar marcado", solo materiales y comida.

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 9.3 | Soltar: materiales | A saca 3 setas de la bolsa y las deja caer | B ve marca y aviso; las recoge; desaparecen para A | ☐ |
| 9.3b | Soltar: comida | A deja caer 3 manzanas (y cualquier otra comida que el juego permita soltar) | Igual que 9.3 | ☐ |
| 9.3c | Clases excluidas | A deja caer una espada, un arco y un escudo, uno a uno | B **no** ve marca ni aviso de ninguno | ☐ |
| 9.3d | Armas por el suelo (ADR-009, decisión 24) | Tras 9.3c, B intenta recoger cada uno | Lo que dejó escrito T423 en su bitácora: o bien no aparecen en el juego de B (impedido), o bien es una limitación documentada con el visto bueno del dueño. Anota durabilidad vista | ☐ |
| 9.3e | Rupias (ADR-009, decisión 25) | A suelta rupias, si el juego lo permite | Sin marca ni aviso; resultado anotado igual que en 9.3d | ☐ |
| 9.3f | Objetos protegidos (ADR-009, decisión 25) | A intenta soltar un objeto clave y la Espada Maestra desde el menú | El juego no lo permite (como hoy); nada llega a B | ☐ |

Envío directo (después; solo si se hacen T420-T422):

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 9.1 | Transacción, unidad | `build/test_coop_give` | Pasa (suma constante en todos los cortes) | ☐ |
| 9.2 | Recibir | Bot con `BOT_GIVE=Item_Fruit_A,3` | 3 manzanas más en el inventario del juego | ☐ |
| 9.4 | Dar directo | A da 5 manzanas a B desde la pestaña | A tiene 5 menos, B 5 más | ☐ |
| 9.5 | 20 envíos con cortes | Bot con `BOT_GIVE_DROP` en cada paso | Suma de objetos de ambos constante en los 20 | ☐ |
| 9.6 | Nombre inválido, objeto clave o clase excluida | Bot que ofrece un objeto inexistente, uno clave, un arma, un arco y un escudo | Rechazados con motivo | ☐ |
| 9.7 | Receptor en menú o evento | Enviar mientras B tiene el menú abierto | Espera y entrega al volver al juego, o aborta y devuelve tras el plazo | ☐ |
| 9.8 | Inventario lleno | B sin sitio | Rechazo antes de quitar nada a A | ☐ |

## 5. CU-10 — Marcadores, pings, nombre en color, accesibilidad

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 10.1 | Paleta | `build/test_coop_palette` | Pasa con las 3 simulaciones | ☐ |
| 10.1b | Por orden de entrada (ADR-009, decisión 26) | Sesión nueva, nadie configura nada; captura de las dos pantallas | Cada jugador sale con el color y la forma de su orden de entrada, distintos entre sí | ☐ |
| 10.2 | Cambiar en Opciones (ADR-009, decisión 26) | Cada jugador elige otro color y otra forma; captura de las dos pantallas | Nombre, figura y flecha con el color y la forma elegidos por el otro; se conserva al reiniciar | ☐ |
| 10.3 | Misma elección | Los dos eligen lo mismo | El de ranura mayor recibe otra forma | ☐ |
| 10.4 | Ping | A hace ping (4 tipos) | B lo ve en < 0,5 s, con forma por tipo y distancia; desaparece a los 8 s | ☐ |
| 10.5 | Ping fuera de pantalla | B mira hacia otro lado | Flecha en el borde | ☐ |
| 10.6 | Límite de pings | `sh tools/nettest/run.sh mark_rate` | ≤ 2 por segundo | ☐ |
| 10.7 | Chat rápido en dos idiomas | A en español, B en inglés | La misma frase, cada uno en su idioma | ☐ |
| 10.7b | Las ocho frases (ADR-009, decisión 28) | Juego en español; abre la rueda y envía las ocho | Son exactamente, y en este orden: "Ven aquí", "Espera", "Necesito ayuda", "Gracias", "Sí", "No", "Vamos", "Mira esto" | ☐ |
| 10.8 | Avisos | Bot con `BOT_DEMO=1` a < 30 m; pulsar A | Aviso en pantalla de que A está retenido | ☐ |
| 10.9 | Vida del compañero siempre visible (ADR-009, decisión 27) | Con B a vida completa, y después dañándole | La vida de B se ve siempre, en pequeño, en la pantalla de A, y baja al dañarle | ☐ |
| 10.9b | Destacada por debajo del 25 % (ADR-009, decisión 27) | Baja la vida de B por debajo de un cuarto de su máximo | Se destaca con forma e icono, no solo con color; vuelve a pequeño al recuperarse. Si `life_max` no se pudo leer, anota que se usó el umbral de un corazón | ☐ |
| 10.10 | Daltonismo | Repetir 10.2 y 10.4 con `SAVAGEWIND_CVD_SIMULATE=protan`, `deutan`, `tritan` | Todo distinguible por forma y texto | ☐ |
| 10.11 | Tamaño de texto | Ajuste al 80 % y al 160 % | Legible, sin solapes graves | ☐ |
| 10.12 | Solo mando | Ping, chat rápido, aceptar/rechazar con mando | Todo alcanzable, con la combinación de botones que el dueño eligió a partir de la propuesta de T436 (Q-10-5). Si aún no la ha elegido, queda como única casilla abierta de CU-10 | ☐ |
| 10.14 | Nombre del mapa y distancia (ADR-011, decisión 41) | B en otro rincón del mapa de A; mira la flecha | La flecha muestra la distancia y el nombre del mapa de A; no hay marcador en el mapa del propio juego | ☐ |
| 10.13 | Las marcas no tiemblan | Mover la cámara rápido con una marca en pantalla, a 30 y a 60 fps, con y sin generación de fotogramas | Sin temblor visible; anótalo si lo hay | ☐ |

## 6. CU-11 — Revivir al compañero

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 11.1 | Política | `build/test_coop_revive` | Pasa | ☐ |
| 11.2 | Automático, compañero en pie (ADR-007, decisión 7) | A en pie; vida de B a 0 (enemigo, o `SAVAGEWIND_ONLINE_DIE_TEST` si sigue existiendo) | B revive solo, sin que A haga nada; aviso en pantalla | ☐ |
| 11.3 | Compañero en pie y lejos | Igual, con B a más de 500 m de A | B revive igual: no hay límite de distancia | ☐ |
| 11.4 | Los dos caen a la vez (ADR-008, decisión 15) | Vida de A y de B a 0 en el mismo momento (por ejemplo, la misma explosión) | Fin de partida normal para los dos; cada uno recarga su último guardado | ☐ |
| 11.5 | Regla apagada por el anfitrión | "Revive" desactivado | Fin de partida | ☐ |
| 11.6 | 10 muertes seguidas | Alternando 11.2 y 11.4 | Sin crash; los actores del compañero reaparecen bien tras cada una | ☐ |
| 11.7 | Hada propia | B con un hada en la bolsa | Se usa primero la suya, como en el juego normal | ☐ |
| 11.7b | Compañero en otro mapa | A en un santuario, en pie; B cae fuera | B revive (sin límite de mapa); anota cualquier efecto raro | ☐ |

Rescate manual (después, prioridad baja; solo si se hacen T406 y T432):

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 11.8 | Estado caído | B cae; A mantiene el botón a < 2 m | Cuenta atrás visible; B se levanta a los 3 s | ☐ |
| 11.9 | Nadie ayuda (después; ADR-011, decisión 34) | 30 s sin ayuda | B revive en el sitio con 3 corazones; no hay fin de partida por tiempo | ☐ |
| 11.10 | Los dos caídos | Ambos a 0 | Fin de partida normal para los dos (ADR-008, decisión 15) | ☐ |

## 7. CU-12 — PvP por acuerdo, daño validado

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 12.1 | Lógica | `build/test_coop_pvp` | Pasa | ☐ |
| 12.2 | Apagado por defecto | Sesión nueva; A golpea a B | 0 daño | ☐ |
| 12.3 | Solo uno propone | A propone; B no acepta; A golpea | 0 daño; B ve la propuesta | ☐ |
| 12.4 | Los dos aceptan | B acepta; A golpea con un arma de poder conocido | Daño = poder − defensa de B (mínimo 1), como antes de F4 | ☐ |
| 12.5 | Bloqueo | B con el escudo en alto | Bloqueado | ☐ |
| 12.6 | Apagar | Cualquiera pulsa "Turn off" | 0 daño desde ese momento | ☐ |
| 12.6b | El anfitrión prohíbe el PvP (ADR-008, decisión 17) | El anfitrión apaga "Allow PvP in this session"; B intenta proponer; repetir con un duelo ya activo | No se puede proponer; el duelo activo se apaga; 0 daño | ☐ |
| 12.6c | Solo cuerpo a cuerpo y bombas (ADR-008, decisión 17) | Con PvP activo, A dispara flechas a B y usa runas sobre él | 0 daño de PvP por flechas y runas (bombas: fila 12.10) | ☐ |
| 12.7 | Tramposo (daño validado por el anfitrión; ADR-008, decisión 17) | `sh tools/nettest/run.sh pvp_cheat` | 0 PVP_DMG reenviados | ☐ |
| 12.8 | Tramposo contra el juego | Juego A anfitrión, juego B, bot con `BOT_PVP_CHEAT=1` y PvP activo | La vida de B no baja por el bot | ☐ |
| 12.9 | Indicador | Con PvP activo | Icono junto al nombre de cada jugador | ☐ |
| 12.10 | Bomba | Bomba de A a < 4 m de B con PvP activo y apagado | Daño solo con PvP activo | ☐ |
| 12.12 | Morir en un duelo (ADR-011, decisión 39) | Con PvP activo, A mata a B | El duelo se apaga; B revive con la reanimación normal; B no pierde ningún objeto | ☐ |
| 12.13 | Sin duelo en santuarios ni escenas (ADR-011, decisión 40) | Intentar proponer con uno en un santuario y con uno en una escena | Botones deshabilitados con "zona sin duelo"; no se abre | ☐ |
| 12.11 | Golpe del invitado con red mala | Bot o segundo PC con perfil `wifi` | El golpe llega con el retraso esperado; no se pierden golpes válidos por el margen de distancia | ☐ |

## 8. Regresión y cierre

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| R.1 | Recorrido de F1 (30 min) | CU-01 a CU-03 con dos juegos | Sin crash | ☐ |
| R.2 | Suite de red de F3 | `sh tools/nettest/run.sh all` | Pasa | ☐ |
| R.3 | Idiomas | `tools/venv/Scripts/python.exe tools/i18n_check.py` | `0 missing`, y los avisos de `online_notice_push` (literales en `coop_hud.mm` dentro de `tr()`) aparecen entre los textos traducidos | ☐ |
| R.4 | Segunda sesión limpia | Salir y entrar en otra sesión con otro anfitrión | Sin marcas, avisos, transacciones ni PvP de la anterior | ☐ |
| R.5 | Reinicio en caliente | "Quit to start screen" con sesión abierta y volver | Sin crash; estado cooperativo coherente | ☐ |
| R.6 | Identificador del juego | Si F4 tocó `tools/game_sites.py` o `tools/patches.py`: un solo cambio de `GAMELIB_ID` en toda la fase | Anotado en bitácora el antes y el después | ☐ |
| R.7 | Rendimiento | fps con y sin overlay cooperativo, misma escena, 60 s | Diferencia anotada (fps y tiempos de fotograma) | ☐ |

---

## 9. Plantilla de bitácora

Un fichero por prueba en `specs/bitacora/`, con nombre `F4-<tarea o caso>-<fecha AAAA-MM-DD>.md`.

```markdown
# <Tarea o caso de uso> — <fecha>

- Quién: <dueño | agente (modelo)>
- Commit: <hash corto>   Rama: <rama>
- Protocolo: <n>   Versión del juego: 1.9.0   GAMELIB_ID: <16 hex>
- Equipo: <CPU, GPU y controlador, RAM>   Sistema: <Windows 11 / contenedor Linux>
- Montaje: <dos juegos en un PC | juego + bot | Docker (perfil netem) | dos PC>
- Guardados usados: <copia A / B / B0: qué tiene cada una, sin adjuntarlas>
- Ajustes relevantes: <fps, reglas de sesión, variables de entorno>

## Qué se probó
<una frase por paso, con la orden exacta>

## Resultado
| Paso | Esperado | Visto | OK/FALLO |
|---|---|---|---|

## Evidencia
<salida recortada, o ruta relativa de la captura en build/>

## Hipótesis frente a hechos
- Visto en el juego: <...>
- Comprobado con prueba sintética: <...>
- Solo supuesto: <...>

## Fallos y tareas abiertas
<id de tarea nueva o "ninguno">
```

Reglas: sin rutas absolutas, sin nombres de usuario, sin IP públicas, sin guardados ni ficheros del juego adjuntos.
