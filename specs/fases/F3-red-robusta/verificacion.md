# F3 — Red robusta: verificación de extremo a extremo

Esta lista se recorre entera al cerrar la fase (tarea T340). Cada casilla necesita **evidencia** en
`specs/bitacora/`: la orden y su salida, o una captura. "Compila" no es evidencia.

## 0. Preparación

### 0.1 Pruebas sin juego (Docker)

Requisitos: Docker en marcha, el repo compilado en la imagen de pruebas de F0.

```
sh tools/nettest/run.sh smoke          # arnés vivo
sh tools/nettest/run.sh all            # todos los escenarios
make test-online                       # pruebas unitarias de los módulos
tools/venv/Scripts/python.exe tools/i18n_check.py
```

Perfiles de red disponibles en `tools/nettest/netem.sh`: `lan`, `fibra`, `normal`, `wifi`, `movil`, `rafagas`,
`subida`, `corte`, `quitar`. Por dentro son órdenes `tc qdisc ... netem` en la interfaz del contenedor (por ejemplo
`tc qdisc add dev eth0 root netem delay 80ms 20ms loss 2%`), que necesitan `cap_add: NET_ADMIN`.

### 0.2 Dos juegos en un PC

Procedimiento de `docs/ONLINE.md` §"Testing with two games on one PC" (líneas 738-769). Antes: **cierra Docker
Desktop y el navegador** (dos juegos usan unos 9,5 GB de RAM y el PC tiene 15,7 GB).

Reglas:

- Cada juego con su carpeta de guardado, que es una **copia** (nunca la carpeta en uso).
- Un puerto de sesión que no sea el real (52530) y un puerto de control distinto por juego.
- `SAVAGEWIND_ONLINE_NAME` evita además que se reescriba `online.ini` (los dos juegos comparten directorio de trabajo).
- El juego puede relanzarse una vez al arrancar: si vigilas por PID, busca al proceso hijo.

PowerShell, juego A (anfitrión):

```powershell
$env:SAVAGEWIND_SAVE_DIR="<copia-A>"; $env:SAVAGEWIND_NO_LAUNCHER="1"; $env:SAVAGEWIND_AUDIO_GAIN="0"
$env:SAVAGEWIND_ONLINE_PORT="52530"; $env:SAVAGEWIND_ONLINE="host"; $env:SAVAGEWIND_ONLINE_NAME="Host"
$env:SAVAGEWIND_CONTROL="7777"; .\build\savagewind.exe
```

Juego B (invitado), en otra consola:

```powershell
$env:SAVAGEWIND_SAVE_DIR="<copia-B>"; $env:SAVAGEWIND_NO_LAUNCHER="1"; $env:SAVAGEWIND_AUDIO_GAIN="0"
$env:SAVAGEWIND_ONLINE_PORT="52530"; $env:SAVAGEWIND_ONLINE="127.0.0.1"; $env:SAVAGEWIND_ONLINE_NAME="Client"
$env:SAVAGEWIND_CONTROL="7778"; .\build\savagewind.exe
```

Añade `$env:SAVAGEWIND_HIDDEN="1"` para ejecutar sin ventana (pruebas automáticas). Para mirar la pantalla, no lo
pongas. Conducirlos: `tools\venv\Scripts\python.exe tools\swctl.py --port 7777 goto <lugar>` (órdenes en
`docs/CONTROL.md`). Capturas con overlay incluido: `SAVAGEWIND_WINDOW_SHOT=<fotograma>` (las de `shot` no incluyen el
overlay).

### 0.3 Un juego y un bot

```
cc -O2 -o build/online_bot tools/online_bot.c            # Linux
cc -O2 -o build/online_bot.exe tools/online_bot.c -lws2_32   # Windows (MSYS2 UCRT64)
build/online_bot 127.0.0.1 52530 Bot 4
```

Variables del bot añadidas en T302: `BOT_NOREAD`, `BOT_FLOOD`, `BOT_SPOOF`, `BOT_BADPASS`, `BOT_PREAUTH`,
`BOT_DROP`, `BOT_STATS`. Las anteriores (`BOT_FREEZE`, `BOT_FAR`, `BOT_SWAP`…) siguen igual.

Cortar la red de **un** proceso en Windows sin Docker: no hay forma sencilla. Para CU-05 con dos juegos reales usa
el bot como par que se cae (`BOT_DROP`), o dos PC y desconecta el cable/WiFi de uno.

---

## 1. CU-03 — Jugar sin crashes (parte de red)

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 3.1 | Un par que no lee no congela a nadie | `sh tools/nettest/run.sh h1_noread` | Intervalo máximo entre STATE del otro bot ≤ 150 ms; el que no lee, expulsado < 15 s | ☐ |
| 3.2 | Lo mismo con el juego real | Juego A anfitrión + bot normal + bot con `BOT_NOREAD=20`. Mira el bot normal en la pantalla de A | El bot normal sigue moviéndose sin tirones; aparece "left" del otro | ☐ |
| 3.3 | Terminar sesión con un par colgado | Mismo montaje; pulsa "End session" a los 25 s | La pestaña responde en < 1 s | ☐ |
| 3.4 | Salir mientras conecta a una dirección muerta | Juego B: unirse a `10.255.255.1`, pulsar "Leave" a los 2 s | La imagen no se congela | ☐ |
| 3.5 | Suplantación del anfitrión | `sh tools/nettest/run.sh spoof` | 0 ENEMIES/LOOTS/QUESTS reenviados | ☐ |
| 3.6 | Lo mismo con el juego real | Juego A anfitrión, juego B invitado, bot con `BOT_SPOOF=1` | En B no aparecen enemigos falsos ni cambian misiones; registro de A cuenta infracciones | ☐ |
| 3.7 | Inundación | `sh tools/nettest/run.sh flood` | ≤ 5 CHAT en 5 s; emisor expulsado | ☐ |
| 3.8 | Conexiones mudas | `sh tools/nettest/run.sh preauth` | El jugador legítimo entra en < 2 s | ☐ |
| 3.9 | Contraseña | `sh tools/nettest/run.sh badpass`; después `grep` de la contraseña en `online.ini` y en una captura `tcpdump` | Bloqueo al 6.º intento; 0 apariciones | ☐ |
| 3.10 | Versión | `sh tools/nettest/run.sh version_matrix` | 5 motivos distintos, cada uno con su texto | ☐ |
| 3.11 | Fuzzing | `sh tools/nettest/fuzz.sh 120` | Sin hallazgos | ☐ |
| 3.12 | Regresión de 30 minutos | Recorrido de F1 (CU-01 a CU-03) con dos juegos | Sin crash; mismos resultados que en la bitácora de F1 | ☐ |
| 3.13 | Reinicio en caliente con sesión | Con sesión abierta, "Quit to start screen" en B y volver a cargar | La sesión sigue; B reaparece en A; sin crash | ☐ |
| 3.14 | Sesión normal sin falsos positivos | 10 minutos de juego normal de dos jugadores | 0 infracciones y 0 expulsiones en el registro del anfitrión | ☐ |

## 2. CU-04 — Encontrar la partida

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 4.1 | Lista de red local (Docker) | `sh tools/nettest/run.sh lan` | 1 resultado en < 2 s en la misma red; 0 en redes distintas | ☐ |
| 4.2 | Lista de red local (juego) | Juego A crea sesión; juego B abre Online > "Search local network" | A aparece con su nombre, 1/2 jugadores (límite actual: ADR-007, decisión 12) y candado si hay contraseña; elegirlo y unirse funciona con mando | ☐ |
| 4.3 | Versión distinta en la lista | Bot anfitrión con otra versión de compilación en su respuesta | La fila se ve con aviso y no deja unirse (o avisa antes) | ☐ |
| 4.4 | Código de invitación, unidad | `build/test_online_invite` | Ida y vuelta IPv4/IPv6; una letra cambiada se rechaza | ☐ |
| 4.5 | Código de invitación, juego | A crea sesión, copia el código; B lo pega en "Dirección" | B entra sin teclear IP ni puerto | ☐ |
| 4.6 | Apertura automática (Docker) | `sh tools/nettest/run.sh upnp` | Abre, obtiene IP externa, cierra; sin IGD: "no disponible" en < 5 s | ☐ |
| 4.7 | La apertura no detiene el juego | Juego A: crear sesión en una red sin UPnP, mirando el overlay de rendimiento (F10) | Sin congelación durante los 5 s de búsqueda | ☐ |
| 4.8 | Mensajes de estado del puerto | Tres casos: abierto / no disponible / dirección compartida por el proveedor | El texto correcto en cada caso; ninguno nombra una red virtual ni un servicio externo (ADR-009, decisión 23) | ☐ |
| 4.8b | Error claro al no poder conectar | Juego B: unirse a una dirección que no responde y a un código válido de un anfitrión apagado | Mensaje que dice qué se intentó y que no se pudo conectar; sin guía de red virtual ni oferta de servicio | ☐ |
| 4.9 | Cierre de la redirección | Terminar sesión y mirar el router o `miniupnpd` | La redirección ya no está | ☐ |
| 4.10 | IPv6 local | Juego A crea sesión; B se une a `[::1]:52530` | Conecta | ☐ |
| 4.11 | IPv6 en la lista de direcciones | Pestaña de A con sesión abierta | Aparecen las IPv6 globales si el PC tiene | ☐ |
| 4.12 | Router real y cortafuegos, con dos PC por Internet | Tarea T341 | Bitácora rellena. **Sin esta fila en OK, F3 no se cierra** (ADR-011, decisión 47); sin segundo PC queda como pendiente explícito y no se sustituye por Docker | ☐ |
| 4.13 | Guía | El dueño sigue `docs/coop-conexion.md` sin ayuda; además `grep -ci -e tailscale -e zerotier docs/coop-conexion.md` | Conecta o llega a un diagnóstico claro; el `grep` da 0 | ☐ |
| 4.15 | Dependencia anotada | `grep -ci miniupnpc docs/third-party.md` | Distinto de 0, con versión y licencia (ADR-007, decisión 11) | ☐ |
| 4.14 | Accesibilidad | Recorre la pestaña solo con mando; mira los indicadores con la opción de visión de color activada y desactivada | Todo alcanzable; ningún estado depende solo del color | ☐ |

## 3. CU-05 — Reconectar

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| 5.1 | Reanudación en el servidor | `sh tools/nettest/run.sh resume_server` | Misma ranura dentro del plazo; jugador nuevo fuera de plazo | ☐ |
| 5.1b | Plazo por defecto | Sin clave `grace` en `online.ini`, mirar el plazo que anuncia el WELCOME en el registro | 600 s (10 minutos) | ☐ |
| 5.2 | Reintento del cliente | `sh tools/nettest/run.sh resume_client` (corte de 10 s) | Vuelve a `CONNECTED` solo, misma ranura | ☐ |
| 5.3 | Juego + bot que se cae | Juego A anfitrión; bot con `BOT_DROP=20,8` cerca de Link | A muestra "reconnecting" junto al bot, el Link del bot desaparece y **reaparece con su equipo** en < 5 s tras volver; sin piezas duplicadas | ☐ |
| 5.4 | Dos PC, corte real | Dos PC en la misma red; desconecta el WiFi del invitado 10 s | Invitado: "Reconnecting…", vuelve solo. Anfitrión: compañero "reconnecting" y luego normal | ☐ |
| 5.5 | Corte más largo que la gracia | Igual, con `grace = 60` en el `online.ini` del anfitrión y un corte de 90 s (el valor por defecto es 10 minutos: ADR-008, decisión 18) | Invitado: mensaje de fallo claro y botón "Reconnect"; anfitrión: "left" | ☐ |
| 5.6 | Cancelar | Durante "Reconnecting…", pulsar "Cancel" | Estado "Not connected" inmediato | ☐ |
| 5.7 | No se aplica estado viejo | Sesión 1 con misiones compartidas; salir; unirse a otra sesión con un anfitrión con partida nueva | Ningún flag de la sesión 1 se activa en la sesión 2 (compara con `swctl flag <nombre>`) | ☐ |
| 5.8 | El anfitrión cae | Cerrar el juego A a la fuerza | B reintenta, falla al vencer el plazo con mensaje claro; sin crash | ☐ |
| 5.9 | Sin fugas | 50 ciclos entrar/salir con el bot | Nº de hilos y descriptores del anfitrión estable | ☐ |

## 4. Calidad de red y transporte

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| Q.1 | Estadísticas, unidad | `build/test_online_netstat` | Pasa | ☐ |
| Q.2 | Indicador con red mala | Juego A + bot dentro de Docker con perfil `wifi` conectándose al PC | A muestra 150-190 ms y pérdida > 0 junto al bot, con 2 barras o menos | ☐ |
| Q.3 | Indicador con red buena | Perfil `lan` | 3 barras; pérdida 0 | ☐ |
| Q.4 | Sin saltos tras ráfaga | Perfil `rafagas`, mirar al bot caminar en círculo durante 2 minutos | Movimiento continuo; anota los saltos vistos (objetivo: 0) | ☐ |
| Q.5 | Línea base TCP | `sh tools/nettest/measure.sh tcp` | `resumen.csv` con 7 perfiles | ☐ |
| Q.6 | Banco UDP | `sh tools/nettest/measure.sh udp` | `resumen.csv` comparable | ☐ |
| Q.7 | ADR de transporte | Leer `specs/decisiones/ADR-transporte-tcp-o-udp.md` | Tabla, decisión y firma del dueño | ☐ |

## 5. Idiomas y compilación

| # | Comprobación | Cómo | Esperado | ☐ |
|---|---|---|---|---|
| I.1 | Traducciones completas | `tools/venv/Scripts/python.exe tools/i18n_check.py` | `0 missing` en los 13 idiomas | ☐ |
| I.2 | Vista en tres idiomas | Capturas de la pestaña Online en español, japonés y hebreo | Texto completo, sin cortes; código e IP legibles en hebreo | ☐ |
| B.1 | Windows | `make runtime-objs` y `make -j8` en MSYS2 UCRT64 | Sin avisos nuevos | ☐ |
| B.2 | Linux | `make runtime-objs` en el contenedor | Sin avisos nuevos | ☐ |
| B.3 | Identificador del juego intacto | `make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id` antes y después de F3 | El mismo valor (F3 no toca parches ni sitios) | ☐ |
| B.4 | Variables de entorno | `make test-launch-flags` | Pasa | ☐ |

---

## 6. Plantilla de bitácora

Un fichero por prueba en `specs/bitacora/`, con nombre `F3-<tarea o caso>-<fecha AAAA-MM-DD>.md`.

```markdown
# <Tarea o caso de uso> — <fecha>

- Quién: <dueño | agente (modelo)>
- Commit: <hash corto>   Rama: <rama>
- Protocolo: <n>   Versión del juego: 1.9.0   GAMELIB_ID: <16 hex>
- Equipo: <CPU, GPU y controlador, RAM>   Sistema: <Windows 11 / contenedor Linux>
- Montaje: <dos juegos en un PC | juego + bot | Docker (perfil netem) | dos PC>
- Ajustes relevantes: <fps, opciones de Online, variables de entorno>

## Qué se probó
<una frase por paso, con la orden exacta>

## Resultado
| Paso | Esperado | Visto | OK/FALLO |
|---|---|---|---|

## Evidencia
<salida recortada de la orden, o ruta relativa de la captura en build/>

## Hipótesis frente a hechos
- Visto en el juego: <...>
- Comprobado con prueba sintética: <...>
- Solo supuesto: <...>

## Fallos y tareas abiertas
<id de tarea nueva o "ninguno">
```

Reglas de la bitácora: sin rutas absolutas, sin nombres de usuario, sin IP públicas ni códigos de invitación reales,
sin ficheros del juego ni guardados adjuntos.
