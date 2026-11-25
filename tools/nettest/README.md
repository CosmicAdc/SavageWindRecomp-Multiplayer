# Banco de pruebas de red (T301)

Servidor dedicado y dos bots (`tools/online_bot.c`) en tres contenedores Docker sobre una red puente propia, con
latencia y pérdida simuladas con `tc netem`. Sin el juego: el servidor es `savagewind --server` compilado sin código
del juego (`make GEN=/nonexistent`), el mismo que usa `tools/docker/red-bots.sh`.

Nada se publica al anfitrión (sin `ports:` ni `network_mode: host`), el árbol se monta de solo lectura y no se usa
ningún fichero del juego.

## Uso

Desde la raíz del repositorio, en Git Bash (Windows) o en Linux:

```sh
sh tools/nettest/run.sh            # lista los escenarios
sh tools/nettest/run.sh smoke      # sale con 0 e imprime "smoke: OK"
```

Tres veces seguidas (criterio de T301):

```sh
for i in 1 2 3; do sh tools/nettest/run.sh smoke || break; done
```

La primera ejecución construye las imágenes `swcoop-pruebas` (F0, `tools/docker/Dockerfile.pruebas`) y
`swcoop-nettest` (`tools/nettest/Dockerfile`) y compila la aplicación en el volumen `swcoop-nettest_nettest`: tarda
del orden de lo que tarda `make` del runtime con 4 trabajos. Las siguientes solo recompilan lo que cambió.

## Qué hace `run.sh <escenario>`

1. Construye lo que falte: imágenes y binarios (servicio `build` de `compose.yml`, perfil `build`).
2. Levanta los nodos `server`, `bot_a` y `bot_b`, que solo esperan (`sleep infinity`).
3. Aplica el perfil `netem` del escenario en los nodos de `NETEM_EN` (por defecto los tres).
4. Arranca `savagewind --server 52530` en `server` y espera a que diga `dedicated server on port`.
5. Llama a la función `escenario` del fichero `scenarios/<escenario>.sh`, que lanza los bots, espera y comprueba.
6. Recoge los registros en `build/nettest/<escenario>/`, quita `netem` y baja los nodos (`compose down`).

Los procesos se lanzan con `docker compose exec`, no como orden de cada servicio: así el perfil está puesto antes de
que nadie conecte y un escenario puede cambiarlo a mitad (`nt_netem corte bot_b`).

Registros en `build/nettest/<escenario>/` (fuera de git):

| Fichero | Contenido |
|---|---|
| `resultado.txt` | `<escenario>: OK` o `<escenario>: FALLO` |
| `run.log` | lo que dijo `run.sh` y la comprobación del escenario |
| `contexto.txt` | perfil, nodos, fecha, kernel de los nodos, versión de Docker |
| `server.log` | registro del servidor (`savagewind.log` de su raíz de datos) |
| `server-arranque.log` | lo que el servidor dijo antes de abrir su registro (p. ej. una biblioteca que falta) |
| `bot_a.log`, `bot_b.log` | salida de cada bot (`bot: stats ...` cada 5 s) |
| `bot_a.rc`, `bot_b.rc` | código de salida del bot: 124 = seguía conectado al acabar su tiempo |
| `tc-<nodo>.txt` | `tc -s qdisc` de cada nodo al terminar (prueba de que `netem` estaba puesto, con contadores) |
| `netem.log` | salida de `netem.sh` |
| `build.log` | construcción de imágenes y binarios |
| `compose.log` | `docker compose logs` |

## Perfiles (`netem.sh`)

| Perfil | `netem` |
|---|---|
| `lan` | nada |
| `fibra` | `delay 20ms 4ms loss 0.2%` |
| `normal` | `delay 40ms 10ms loss 1%` |
| `wifi` | `delay 80ms 20ms loss 2%` |
| `movil` | `delay 150ms 40ms loss 5%` |
| `rafagas` | `delay 60ms 15ms loss gemodel 1% 20% 70% 0.1%` |
| `corte` | `loss 100%` |
| `subida` | `delay 40ms 10ms loss 1% rate 1mbit` (para T303: solo en `server`) |
| `quitar` | quita el perfil |

`netem` solo actúa sobre lo que **sale** de cada interfaz. Con el perfil en los tres nodos, el camino bot→servidor
lleva el retardo una vez y el de vuelta otra: la ida y vuelta bot↔servidor es unas 2 × `delay` (≈160 ms con `wifi`,
lo que espera S12) y un STATE de un bot al otro cruza dos tramos con pérdida. Con `NETEM_EN="bot_a bot_b"` solo se
estropea lo que envían los bots.

`sh tools/nettest/netem.sh lista` imprime la tabla; `NETEM_SECO=1` imprime la orden `tc` sin ejecutarla.

## Variables de `run.sh`

| Variable | Por defecto | Qué hace |
|---|---|---|
| `PERFIL` | el del escenario | otro perfil (gana al del escenario) |
| `NETEM_EN` | `server bot_a bot_b` | en qué nodos se aplica |
| `NETEM_OPCIONAL` | `0` | `1`: si el kernel no tiene `sch_netem`, sigue sin él y el resultado dice `SIN netem` |
| `NT_PUERTO` | `52530` | puerto del servidor dentro de la red de compose |
| `JOBS` | `4` | `make -j` de la compilación |
| `NT_SIN_CONSTRUIR` | `0` | `1`: no construye nada (las imágenes y binarios ya existen) |
| `NT_MANTENER` | `0` | `1`: deja los nodos arriba al terminar (sin `netem`) |
| `NT_LIMPIAR` | `0` | `1`: al terminar borra también el volumen de compilación |

Del escenario `smoke`: `SMOKE_SEGUNDOS=20`, `SMOKE_MIN_STATE=200`.

## Escenarios

Cada escenario es `scenarios/<nombre>.sh`; `run.sh <nombre>` lo encuentra por el nombre, así que una tarea que añade
uno no edita `run.sh`. El fichero se carga con `.` y:

- puede fijar `PERFIL=...` y `NETEM_EN=...` con una asignación simple (la variable de entorno gana);
- debe definir la función `escenario`, que devuelve 0 si pasa;
- puede usar `nt_log`, `nt_netem PERFIL [NODO...]`, `nt_bot NODO NOMBRE SEGUNDOS [VAR=valor...]`,
  `nt_esperar_bots`, `nt_total_state LOG` y las variables `NT_DIR` y `NT_PUERTO` (detalle en la cabecera de
  `run.sh`).

Ejemplo de un bot con un modo del bot: `nt_bot bot_b BotB 30 BOT_NOREAD=10`.

### `smoke`

Perfil `wifi` en los tres nodos, 20 s. Pasa si cada bot obtuvo ranura, vio entrar al otro (`bot: <otro> is slot`),
siguió conectado todo el tiempo (código 124) y contó al menos 200 STATE del otro (`total=` de su última línea
`bot: stats`). Un bot envía 20 STATE por segundo: 20 s dan unos 400 como máximo.

## Requisitos y avisos

- **`sch_netem` en el kernel.** Los contenedores usan el kernel de la máquina virtual de Docker. Si no tiene el
  módulo, `tc` dice `Specified qdisc kind is unknown`, `run.sh` falla en el paso 3 y lo dice (`netem.log`). En
  Linux nativo suele bastar con `sudo modprobe sch_netem`. En Docker Desktop con WSL2 depende de la versión del kernel
  de WSL; si no lo trae, `NETEM_OPCIONAL=1` permite comprobar que el banco funciona, pero esa ejecución **no** mide
  latencia ni pérdida.
- **Plan B del servidor.** Si `savagewind --server` no arranca en el contenedor, `run.sh` falla en el paso 4 con
  `server.log` y `server-arranque.log`. Las pruebas de F2 (`tools/test_online_session.c`) no son un servidor que
  escuche (usan `socketpair`), así que no sirven de sustituto: habría que añadir un programa de servidor solo
  (`online_server_main` de `runtime/online.c`), lo que sale de los ficheros de T301.
- **Git Bash.** `run.sh` exporta `MSYS_NO_PATHCONV=1` para que Git Bash no convierta las rutas `/opt/...` de las
  órdenes de Docker.
- **RAM.** Los tres nodos en reposo usan muy poco (un `sleep` y, en marcha, el servidor y dos bots). Lo que pesa es
  la compilación del primer arranque, con `JOBS=4`, dentro del límite de 6,2 GB de Docker Desktop de este equipo
  [sin medir: anotar la cifra real en la bitácora la primera vez].
- Al terminar (bien, mal o con Ctrl+C) `run.sh` quita `netem` y baja los nodos; el volumen con la compilación queda
  para la siguiente vez (`NT_LIMPIAR=1` lo borra). A mano:
  `docker compose -f tools/nettest/compose.yml down -v`.
