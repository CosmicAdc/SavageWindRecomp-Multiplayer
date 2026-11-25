# F4 — Funciones cooperativas: verificación de extremo a extremo

Se recorre entera al cerrar la fase (tarea T441). Cada casilla necesita evidencia en `specs/bitacora/`.

**Aviso:** F4 escribe flags y objetos en guardados y eso no se puede deshacer. **Todas** las pruebas se hacen con
copias de guardado. Guarda además una copia aparte de tu partida real antes de empezar.

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
| `B` | Invitado | Partida **más avanzada** que A (para probar que no vuelca su progreso) |
| `B0` | Invitado "nuevo" | Partida recién empezada |

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
`BOT_GIVE`, `BOT_GIVE_DROP`, `BOT_PVP_CHEAT`, `BOT_TP`, `BOT_FLOOD=mark,<hz>`.

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
| 6.7 | Reconexión conserva la línea base | Corta al invitado 10 s (bot con `BOT_DROP`) y repite 6.3 | Sigue sin volcarse nada | ☐ |
| 6.8 | Jugador nuevo en la ranura | Sale B, entra B0 en la misma ranura | La línea base es la de B0, no la de B | ☐ |
| 6.9 | Copia de seguridad | Tras "Create session" y tras "Join" | Carpeta `coop-<fecha>` nueva en las copias; también con `SAVAGEWIND_SAVE_DIR` | ☐ |
| 6.10 | Restaurar la copia | Sustituye la carpeta de guardado por la copia y arranca | La partida es la de antes de la sesión | ☐ |
| 6.11 | Luna Carmesí | `swctl bloodmoon` en A | Comportamiento igual que antes de F4 (los objetos vuelven en cada juego en su Luna) | ☐ |
| 6.12 | (Si se hizo R4-07) Mundo idéntico al cargar | B carga su perfil cooperativo tras unirse | Los 10 flags de muestra están a 0 en B dentro de ese perfil | ☐ |

## 2. CU-07 — Cada jugador conserva su inventario y corazones

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 7.1 | Política, unidad | `build/test_coop_rewards` | Pasa; coincide con el ADR | ☐ |
| 7.2 | Partida en solitario intacta | Calcula una suma de comprobación de la carpeta del perfil normal de B (`find <carpeta> -type f -exec sha256sum {} + | sha256sum`); juega 10 min en sesión; sal; recalcula | Igual | ☐ |
| 7.3 | Perfil cooperativo creado | Primera unión de B al mundo de A | Existe `save-profiles/coop-<8 hex>` en la raíz de datos de B; la pestaña lo muestra | ☐ |
| 7.4 | Persistencia | En sesión, B recoge 3 objetos y pierde 2 corazones; sale; vuelve a unirse | Los 3 objetos siguen; corazones según su último guardado | ☐ |
| 7.5 | Otro anfitrión, otro perfil | B se une a un segundo mundo (otra copia de A con otro identificador) | Perfil distinto; el primero no cambia | ☐ |
| 7.6 | Al unirse a un mundo avanzado | B0 (partida nueva) se une a un A avanzado | Recibe solo lo que la tabla marca "al unirse" (capacidades del mundo); **no** orbes ni corazones | ☐ |
| 7.7 | Santuario durante la sesión | Uno completa un santuario | El otro recibe lo que diga la tabla (orbe), una sola vez | ☐ |
| 7.8 | Sin duplicados | Repite 7.7 y corta la red del invitado justo después; reconecta | Sigue teniendo 1 orbe de ese santuario | ☐ |
| 7.9 | Kolog | Uno encuentra un Kolog con el otro lejos | Según la tabla | ☐ |
| 7.10 | Estatua de la Diosa | Cada uno canjea sus orbes | Cada uno elige lo suyo (corazón o resistencia), sin afectar al otro | ☐ |

### Cofres para los dos

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| C.1 | 5 cofres abiertos por el anfitrión | Anota el contenido de cada uno | El invitado recibe los mismos 5 objetos; 0 duplicados | ☐ |
| C.2 | 5 cofres abiertos por el invitado | Igual | El anfitrión recibe los 5 | ☐ |
| C.3 | Recogidas del suelo no se reparten | Recoge 5 objetos del suelo | El otro no recibe nada | ☐ |
| C.4 | Inventario lleno | El receptor tiene la categoría llena | No se pierde ni se cuelga; aviso en pantalla | ☐ |
| C.5 | Objeto inventado | Bot que envía REWARD con un nombre que no existe o sin ser anfitrión | Rechazado; nada en el inventario | ☐ |

## 3. CU-08 — Teletransportarse al compañero (mismo mapa)

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 8.1 | Lógica | `build/test_coop_tp` | Pasa | ☐ |
| 8.2 | Petición y consentimiento | B pulsa "Go to" sobre A; A acepta | B aparece a < 3 m de A, sin daño de caída | ☐ |
| 8.3 | Rechazo y caducidad | A rechaza; después no responde 15 s | "Declined" / "No answer"; B no se mueve | ☐ |
| 8.4 | 10 seguidos | Con el enfriamiento a 0 para la prueba, 10 viajes a puntos distintos | 10 llegadas a < 3 m; 0 con daño | ☐ |
| 8.5 | Compañero en el aire / agua / pared | A planeando, nadando y escalando | Se usa su último punto en el suelo; B no cae al vacío ni al agua | ☐ |
| 8.6 | Bloqueos | Probar con: evento en curso, uno muerto, uno en menú, uno a caballo, PvP activo, enfriamiento | Rechazo con su motivo en cada caso | ☐ |
| 8.7 | Mapa distinto | A dentro de un santuario, B fuera | Rechazo "en otro mapa" | ☐ |
| 8.8 | Salto largo | A y B a más de 1 km | Llega tras la carga del terreno; el mundo no se congela | ☐ |
| 8.9 | Punto falso | Bot con `BOT_TP=1` modificado para responder con coordenadas absurdas o no finitas | El juego rechaza el viaje | ☐ |
| 8.10 | No abre el modo desarrollador | Tras un viaje, la pestaña Teleport/Cheats sigue oculta y `swctl teleport` en el puerto de compañía falla | Sin cambios de permisos | ☐ |

## 4. CU-09 — Enviar o soltar objetos

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 9.1 | Transacción, unidad | `build/test_coop_give` | Pasa (suma constante en todos los cortes) | ☐ |
| 9.2 | Recibir | Bot con `BOT_GIVE=Item_Fruit_A,3` | 3 manzanas más en el inventario del juego | ☐ |
| 9.3 | Soltar para el compañero | A saca 3 setas de la bolsa y las deja caer | B ve marca y aviso; las recoge; desaparecen para A | ☐ |
| 9.4 | (Si T422) Dar directo | A da 5 flechas a B desde la pestaña | A tiene 5 menos, B 5 más | ☐ |
| 9.5 | (Si T422) 20 envíos con cortes | Bot con `BOT_GIVE_DROP` en cada paso | Suma de objetos de ambos constante en los 20 | ☐ |
| 9.6 | Nombre inválido u objeto clave | Bot que ofrece un objeto inexistente y uno clave | Rechazado con motivo | ☐ |
| 9.7 | Receptor en menú o evento | Enviar mientras B tiene el menú abierto | Espera y entrega al volver al juego, o aborta y devuelve tras el plazo | ☐ |
| 9.8 | Inventario lleno | B sin sitio | Rechazo antes de quitar nada a A | ☐ |

## 5. CU-10 — Marcadores, pings, nombre en color, accesibilidad

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 10.1 | Paleta | `build/test_coop_palette` | Pasa con las 3 simulaciones | ☐ |
| 10.2 | Color y forma | Cada jugador elige; captura de las dos pantallas | Nombre, figura y flecha con el color y la forma del otro | ☐ |
| 10.3 | Misma elección | Los dos eligen lo mismo | El de ranura mayor recibe otra forma | ☐ |
| 10.4 | Ping | A hace ping (4 tipos) | B lo ve en < 0,5 s, con forma por tipo y distancia; desaparece a los 8 s | ☐ |
| 10.5 | Ping fuera de pantalla | B mira hacia otro lado | Flecha en el borde | ☐ |
| 10.6 | Límite de pings | `sh tools/nettest/run.sh mark_rate` | ≤ 2 por segundo | ☐ |
| 10.7 | Chat rápido en dos idiomas | A en español, B en inglés | La misma frase, cada uno en su idioma | ☐ |
| 10.8 | Avisos | Bot con `BOT_DEMO=1` a < 30 m; pulsar A | Aviso en pantalla de que A está retenido | ☐ |
| 10.9 | Vida del compañero | Dañar a B | Sus corazones bajan en la pantalla de A | ☐ |
| 10.10 | Daltonismo | Repetir 10.2 y 10.4 con `SAVAGEWIND_CVD_SIMULATE=protan`, `deutan`, `tritan` | Todo distinguible por forma y texto | ☐ |
| 10.11 | Tamaño de texto | Ajuste al 80 % y al 160 % | Legible, sin solapes graves | ☐ |
| 10.12 | Solo mando | Ping, chat rápido, aceptar/rechazar con mando | Todo alcanzable | ☐ |
| 10.13 | Las marcas no tiemblan | Mover la cámara rápido con una marca en pantalla, a 30 y a 60 fps, con y sin generación de fotogramas | Sin temblor visible; anótalo si lo hay | ☐ |

## 6. CU-11 — Revivir al compañero

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 11.1 | Política | `build/test_coop_revive` | Pasa | ☐ |
| 11.2 | Compañero cerca | B a < 50 m de A; vida de B a 0 (enemigo, o `SAVAGEWIND_ONLINE_DIE_TEST` si sigue existiendo) | B revive; aviso en ambas pantallas | ☐ |
| 11.3 | Compañero lejos | B a > 50 m | Fin de partida normal para B | ☐ |
| 11.4 | Compañero en otro mapa o muerto | A en un santuario / A muerto | Fin de partida para B | ☐ |
| 11.5 | Regla apagada por el anfitrión | "Revive" desactivado | Fin de partida | ☐ |
| 11.6 | 10 muertes seguidas | Alternando 11.2 y 11.3 | Sin crash; los actores del compañero reaparecen bien tras cada una | ☐ |
| 11.7 | Hada propia | B con un hada en la bolsa | Se usa primero la suya, como en el juego normal | ☐ |
| 11.8 | (Si T432) Estado caído | B cae; A mantiene el botón a < 2 m | Cuenta atrás visible; B se levanta a los 3 s | ☐ |
| 11.9 | (Si T432) Nadie ayuda | 30 s sin ayuda | Fin de partida | ☐ |
| 11.10 | (Si T432) Los dos caídos | Ambos a 0 | Lo que fijó la decisión P4-4 | ☐ |

## 7. CU-12 — PvP por acuerdo, daño validado

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 12.1 | Lógica | `build/test_coop_pvp` | Pasa | ☐ |
| 12.2 | Apagado por defecto | Sesión nueva; A golpea a B | 0 daño | ☐ |
| 12.3 | Solo uno propone | A propone; B no acepta; A golpea | 0 daño; B ve la propuesta | ☐ |
| 12.4 | Los dos aceptan | B acepta; A golpea con un arma de poder conocido | Daño = poder − defensa de B (mínimo 1), como antes de F4 | ☐ |
| 12.5 | Bloqueo | B con el escudo en alto | Bloqueado | ☐ |
| 12.6 | Apagar | Cualquiera pulsa "Turn off" | 0 daño desde ese momento | ☐ |
| 12.7 | Tramposo | `sh tools/nettest/run.sh pvp_cheat` | 0 PVP_DMG reenviados | ☐ |
| 12.8 | Tramposo contra el juego | Juego A anfitrión, juego B, bot con `BOT_PVP_CHEAT=1` y PvP activo | La vida de B no baja por el bot | ☐ |
| 12.9 | Indicador | Con PvP activo | Icono junto al nombre de cada jugador | ☐ |
| 12.10 | Bomba | Bomba de A a < 4 m de B con PvP activo y apagado | Daño solo con PvP activo | ☐ |
| 12.11 | Golpe del invitado con red mala | Bot o segundo PC con perfil `wifi` | El golpe llega con el retraso esperado; no se pierden golpes válidos por el margen de distancia | ☐ |

## 8. Regresión y cierre

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| R.1 | Recorrido de F1 (30 min) | CU-01 a CU-03 con dos juegos | Sin crash | ☐ |
| R.2 | Suite de red de F3 | `sh tools/nettest/run.sh all` | Pasa | ☐ |
| R.3 | Idiomas | `tools/venv/Scripts/python.exe tools/i18n_check.py` | `0 missing` | ☐ |
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
