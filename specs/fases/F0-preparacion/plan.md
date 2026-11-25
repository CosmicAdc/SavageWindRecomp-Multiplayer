# F0 — Preparación · plan

Todo lo que sigue se comprobó leyendo el checkout del PR (`pr294-online`, HEAD `350c5f8`). Nada se compiló ni se
ejecutó en esta máquina, salvo `tools/i18n_check.py` (solo lectura). Las duraciones, el disco y la RAM son
**estimados** hasta que F0 los mida y los anote en `specs/bitacora/`.

Convención: `$SW` es la carpeta de trabajo (contiene `Game/`, `pr294-online/`, `specs/`). En una shell MSYS2:

    export SW="$(cygpath -u "$USERPROFILE")/Documents/SavageWind"

## 0. Orden general: qué va en serie y por qué

F0 es casi toda **en serie**: cada paso necesita el anterior (sin MSYS2 no hay `make`; sin fork no hay rama; sin
archivos del juego no hay `make gen`). Hay tres cosas que sí pueden ir en paralelo con el resto, porque no comparten
ficheros ni herramientas:

- Los requisitos del dueño (T001 a T003) entre sí.
- La línea Docker (T020 a T023): solo necesita el clon del fork, no MSYS2 ni el juego.
- Las pruebas Python sin juego (T016) mientras se compila.

## 1. Requisitos del dueño (antes de nada)

1. **Fork** de `Revivedx/SavageWindRecomp` en la cuenta CosmicAdc, desde la web de GitHub. Al crearlo, desmarcar
   "copiar solo la rama principal" para traer también `test`. En un fork recién creado GitHub deja Actions
   desactivado hasta que el dueño lo habilita [sin verificar aquí: comportamiento general de GitHub].
2. **Archivos del juego extraídos** de los NSP propios (base v0 + actualización 1.9.0 fusionadas; DLC 3.0 aparte),
   con las claves de la consola del dueño. Este documento no explica cómo. Resultado esperado
   (`docs/game-files.md`):

   ```
   <carpeta del juego>/
     exefs/   main, main.npdm, rtld, sdk, subsdk0
     romfs/   RomFS completo (base + 1.9.0 fusionados; unos 41 062 ficheros según docs/game-files.md)
     aoc1/    opcional: RomFS del primer DLC
     aoc2/    opcional: RomFS del segundo DLC (un fichero: System/AocVersion.txt)
   ```

   No mezclar los DLC dentro de `romfs/` (`docs/game-files.md`, sección "The DLC").
   La carpeta va **fuera del repositorio**, por ejemplo `$SW/juego/`, en una ruta sin espacios.
3. **Disco**: liberar hasta tener, idealmente, 100 GB libres. Presupuesto estimado del plan aprobado: juego
   extraído ~15 GB + imagen Docker ~15 GB + compilación + MSYS2 ≈ 40 GB.

## 2. Herramientas en Windows

En PowerShell, desde la raíz del clon (paso 3 primero si aún no hay clon):

    powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1

Qué hace (`tools/setup_windows.ps1`): instala MSYS2 con `winget` en la unidad del sistema; actualiza con
`pacman -Syuu` dos veces; instala los paquetes UCRT64 (`:15-21`: make, clang, lld, sdl3, ffmpeg, vulkan-headers,
vulkan-loader, shaderc, spirv-tools, pkgconf, gdb, vulkan-memory-allocator, vulkan-validation-layers, nasm);
instala Python 3.12 de python.org; crea `tools\venv` e instala `lz4 capstone unicorn oead` (`:28-29`); fija
`core.autocrlf false` y `core.eol lf` en el clon (`:32-33`). Se puede volver a ejecutar sin daño.

- Duración: 15-30 min **estimado** (depende de la red). Disco: 3-5 GB **estimado**.
- Go no hace falta todavía.
- Diferencia entre documentos: `docs/BUILD-windows.md` y el plan aprobado nombran `lz4 capstone unicorn`; el script
  del PR añade `oead`, que `tools/online_mod.py` necesita (`tools/online_mod.py:5`). Manda el script.

Shell de trabajo a partir de aquí: **MSYS2 UCRT64** (o Git Bash con MSYS2 delante en el PATH):

    export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
    clang --version && make --version && tools/venv/Scripts/python.exe -c "import capstone, unicorn, lz4, oead"

## 3. Git: clon, remotos y rama

Clon nuevo del fork; **no** se reutiliza `pr294-online` (queda como referencia de solo lectura).

    cd "$SW"
    git clone https://github.com/CosmicAdc/SavageWindRecomp-Multiplayer.git coop      # fork real: CosmicAdc/SavageWindRecomp-Multiplayer
    cd coop
    git config core.autocrlf false && git config core.eol lf
    git remote add upstream https://github.com/Revivedx/SavageWindRecomp.git
    git remote set-url --push upstream NO-SE-SUBE-A-UPSTREAM             # un push a upstream falla siempre
    git fetch upstream
    git fetch upstream pull/294/head:pr-294-online                       # la rama del PR #294, por su referencia
    git remote add pr294 <URL-del-fork-de-los-autores>                   # copiarla de la página del PR [sin verificar]
    git remote set-url --push pr294 NO-SE-SUBE
    git switch -c coop/main pr-294-online

Comprobar que la rama del PR es la que se revisó en los mapas: `git log -1 --format=%h pr-294-online` debe dar
`350c5f8`. Si los autores han seguido empujando, el hash será otro: anotarlo y avisar, porque las referencias
`fichero:línea` de los mapas y de F1 se habrán movido.

**Restaurar el CI** (el PR lo pierde en cada merge desde `test`; mapa 06 §8.4). Comparando `pr-294-online` con la
rama `test` local faltan ocho ficheros: `.github/dependabot.yml` y los flujos `activity-feed.yml`, `checks.yml`,
`linux-distros.yml`, `macos-dmg.yml`, `macos.yml`, `tests.yml`, `windows.yml`.

    git checkout upstream/test -- .github/workflows .github/dependabot.yml
    git diff --stat upstream/test -- .github          # no debe quedar ningún fichero "deleted"
    git commit -m "CI: restaurar los flujos de test que el PR #294 pierde en sus merges"

Ajustes del fork sobre esos flujos (un commit aparte, ficheros `.github/workflows/*.yml`):

- `checks.yml`, `tests.yml`, `windows.yml` y `macos.yml` solo se disparan con ramas `test` y `main`. Añadir
  `coop/main` a `branches:` de `push` y `pull_request` para que corran en el fork.
- `activity-feed.yml` no hace nada fuera del repositorio original (está limitado por nombre de repositorio).
- `triage.yml`, `stale.yml` y `crash-save.yml` actuarían sobre los issues del fork (y `stale` los cierra):
  desactivarlos desde la web de GitHub (Actions → flujo → Disable). Es decisión del dueño (T008).
- `macos-dmg.yml` intentaría adjuntar un `.dmg` a cada release del fork: desactivarlo igual.

Rollback de esta sección: borrar la carpeta `coop` y volver a clonar; nada se ha subido.

## 4. Actualizador automático

Hechos comprobados:

- El repositorio original está escrito en `runtime/platform/windows_update.c:46` (`REPO`), `:47-49` (URL
  derivadas), `runtime/update_appimage.c:26-28` y `recomplauncher.json:5`. La clave pública de firma está en
  `runtime/platform/sw_update_key.h`. Nota: el plan aprobado dice `runtime/update_*.c`; el fichero de Windows
  **no** casa con ese patrón porque vive en `runtime/platform/`.
- En una compilación desde fuentes el actualizador no hace nada: `update_check_start` vuelve si
  `post_update_shipped()` es falso (`runtime/post_update.c:434-441`), y eso solo es verdadero cuando hay un
  `sw-manifest.txt` junto al ejecutable (lo escribe `make dist`). Por tanto **hoy no hay riesgo**; el riesgo aparece
  el día que se empaquete un binario del fork.

Opciones (elegir una y dejarla escrita en `specs/decisiones/`):

| Opción | Cambio | A favor | En contra |
|---|---|---|---|
| A. Desactivar | Hacer que `update_check_start` vuelva siempre en el fork (un `#define` de compilación) | Mínimo, sin claves nuevas | Una release del fork no se actualiza sola |
| B. Redirigir | Cambiar `REPO` en los dos `.c` y en `recomplauncher.json`, y sustituir la clave pública por una del fork | Actualizaciones propias | Hay que generar y custodiar una clave privada; más superficie |

Recomendación para F0: **A**, porque no se van a publicar releases todavía. Pruebas que cubren esta zona y deben
seguir pasando: `make test-update-core` (`Makefile:847`); `make test-sw-update` llama a `python3` (`Makefile:857`).

## 5. Mover `specs/` al fork

    cp -r "$SW/specs" "$SW/coop/specs"
    cd "$SW/coop" && git add specs && git status --short | head

Antes del commit, pasar la comprobación de trampas (§8): ninguna ruta personal, ningún nombre de carpeta prohibido.
`specs/PLAN-APROBADO.md` y los mapas no contienen hoy rutas personales salvo las cabeceras de los mapas 06 y 07,
que citan la ruta del checkout revisado: hay que sustituirla por `pr294-online` antes de añadirlos.

## 6. Juego: versión, generación, compilación, overlay, arranque

Dónde lee el juego esta rama: `extracted/` en la raíz, o la carpeta escrita en la primera línea de `game_dir.txt`
en la raíz (`Makefile:336`, `runtime/main.c:345-365`; ambas ignoradas por git). La opción `game_dir.txt` es del PR
y evita copiar 15 GB dentro de cada worktree [uso con `make gen` sin verificar en ejecución]. Alternativa
documentada y segura: copiar o mover la carpeta a `extracted/`.

    cd "$SW/coop"
    echo "<carpeta del juego, con barras normales>" > game_dir.txt       # o bien: usar extracted/

1. **Versión** (segundos; sin escribir nada junto al juego):

       tools/venv/Scripts/python.exe tools/sw_probe.py "<carpeta del juego>/exefs"

   Debe mostrar el build id `cd57b23fa4bbad65` (1.9.0). Si no, parar: el resto de la fase no aplica.
2. **Generar** (una vez; y otra cada vez que cambien `tools/patches.py`, `tools/game_sites.py` o el resto de
   `GAMELIB_SRCS`): `make gen` (`Makefile:339-340`). Duración 5-10 min **estimado**; disco: varios GB **estimado**.
3. **Compilar**: `make -j8` (si el sistema mata el proceso por memoria, `make -j4`). `docs/BUILD-windows.md`
   escribe `make -j` sin número; con 20 hilos y 15,7 GB eso es arriesgado, y `AGENTS.md` recomienda `-j8`.
   Duración 15-25 min **estimado** (el proyecto midió 16-19 min de clang en otra máquina,
   `docs/SAVAGE-WIND.md:2142`). Resultado: `build/savagewind.exe`, `build/savagewind.dll.a`, `build/swgame.dll`.
   Comprobación rápida previa, sin juego: `make runtime-objs` (`Makefile:353`).
4. **Overlay de actores** (`docs/ONLINE.md`, "The players' Links"):

       tools/venv/Scripts/python.exe tools/online_mod.py "<carpeta del juego>/romfs" online_mod

   El juego también lo hace solo en el primer arranque si falta `online_mod/v42` (`runtime/main.c:507-516`), con
   `system()`. Hacerlo a mano permite ver los errores. Duración y disco: **sin medir**.
5. **Arranque en solitario**, desatendido (`AGENTS.md`, "Build and verify"), con copias de guardado y de ajustes
   fuera del repositorio:

       export SAVAGEWIND_SAVE_DIR="$SW/pruebas/guardado-solo" SAVAGEWIND_SETTINGS="$SW/pruebas/ajustes-solo.ini"
       export SAVAGEWIND_NO_LAUNCHER=1 SAVAGEWIND_HIDDEN=1 SAVAGEWIND_TEST_WARP=plateau
       export SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8" SAVAGEWIND_CONTROL=7777
       ./build/savagewind.exe > "$SW/pruebas/solo.log" 2>&1 &
       tools/venv/Scripts/python.exe tools/swctl.py --wait 120 state
       tools/venv/Scripts/python.exe tools/swctl.py pos
       tools/venv/Scripts/python.exe tools/swctl.py exit

   Se confirma el mundo con los contadores de dibujo de las líneas `[vk] t=` del log y con `pos` devolviendo
   coordenadas. No lanzar la ventana bajo el `timeout` de MSYS (`docs/BUILD-windows.md`, "Tests"). Después conviene
   un arranque **con ventana** (sin `SAVAGEWIND_HIDDEN`) para que el dueño vea el juego.

Rollback: `build/`, `generated/` y `online_mod/` son salidas ignoradas por git; se borran y se rehacen.

## 7. Línea base de pruebas sin juego

Desde la raíz del clon, shell MSYS2 UCRT64. Se anota el resultado de cada una en `verificacion.md`; **no se
arregla nada en F0**.

| Prueba | Comando | Nota |
|---|---|---|
| Sitios y versiones | `tools/venv/Scripts/python.exe tools/test_game_sites.py` | — |
| Generador y layout | `tools/venv/Scripts/python.exe tools/test_layout.py` | NSO sintéticos |
| FP/SIMD | `tools/venv/Scripts/python.exe tools/test_simd.py --jobs 8` | la más larga; necesita clang |
| Textos de UI | `tools/venv/Scripts/python.exe tools/i18n_check.py` | **falla hoy** (comprobado): 51 textos de Online sin traducir en 12 idiomas, 3 en `es` |
| Capa de control | `make test-control` (`Makefile:745`) | no usa `python3` |
| Actualizador | `make test-update-core` (`Makefile:847`) | no usa `python3` |
| Consola, lugares, mods | `make test-console test-places test-mods` (`Makefile:767`, `757`, `711`) | — |
| Opciones de lanzamiento | `make test-launch-flags` (`Makefile:875-877`) | llama a `python3`; además el PR añade variables `SAVAGEWIND_ONLINE_*` y la tabla generada puede estar desfasada [sin verificar] |
| Cliente DSU | `tools/test_dsu.sh` | el PR le quitó el bit de ejecución (mapa 06 §8.2): lanzar con `bash tools/test_dsu.sh` |

`python3` en MSYS2: las recetas que lo llaman fallan si no existe. No se inventa un arreglo aquí: se ejecutan a
mano con el Python del venv donde la receta sea una sola línea, o se dejan para Docker, y se anota cuál fue el caso.

## 8. Trampas de nombres del CI y de `.gitignore`

`checks.yml` (en `upstream/test`, líneas 30-31 y 41-43) rechaza, en ficheros **añadidos** por un PR:

- Carpetas en cualquier nivel llamadas `extracted`, `generated*`, `input`, `dump`, `keys`, `save`.
- Ficheros llamados exactamente `main`, `main.npdm`, `rtld`, `sdk`, `subsdkN`.
- Extensiones de volcados y de recursos del juego (`.nsp`, `.nca`, `.nso`, `.sav`, `.pack`, `.byml`, `.sbfres`...).
- Líneas añadidas (fuera de `third_party/`) con rutas personales: la carpeta personal de Linux, la de macOS o la
  carpeta de usuarios de Windows en la unidad del sistema.

Además `.gitignore` ignora en cualquier nivel `input/`, `extracted/`, `generated/`, `build/`, `save/`, `venv/`,
`dump/`, `keys/`, `__pycache__/`: git **no añade** nada dentro de una carpeta con ese nombre **y no avisa**.

Consecuencias prácticas para este proyecto:

| No usar | Usar en su lugar |
|---|---|
| `specs/.../input/`, `tools/docker/build/`, `tests/save/`, `server/keys/` | `entrada/`, `salida/`, `guardado/`, `claves-de-prueba/` o cualquier otro nombre |
| Un binario llamado `main` (por ejemplo un programa Go compilado) | `main.go` sí vale; el binario, con otro nombre o fuera del repositorio |
| Rutas absolutas del equipo en specs, bitácora, comentarios o commits | `$SW/...`, rutas relativas a la raíz del repositorio |
| Logs crudos del juego dentro del repositorio | Guardarlos fuera (`$SW/pruebas/`) y citar en la bitácora las líneas relevantes sin rutas personales |

Comprobación local antes de cada commit (misma expresión que el CI, sobre lo que está preparado):

    git diff --cached --name-only --diff-filter=ACR | grep -iE \
      '(^|/)(extracted|generated[^/]*|input|dump|keys|save)/|(^|/)(main|main\.npdm|rtld|sdk|subsdk[0-9]+)$' ; echo "nombres: $?"
    git diff --cached -U0 -- . ':!third_party' | grep -E '^\+' | grep -E \
      '/home/[a-z][a-z0-9_-]+/|/Users/[A-Za-z][^/ ]*/|[Cc]:[\/]+Users[\/]' ; echo "rutas: $?"

Las dos deben terminar con `1` (sin coincidencias).

## 9. Docker

Papel: pruebas automáticas en Linux, comprobación de compilación en Linux y pruebas de red con el bot. No sirve
para jugar. Docker Desktop tiene un límite de 6,2 GB de memoria: dentro del contenedor, `JOBS=4`.

**9.1 Compilación en Linux.** Ya existe un script del proyecto que hace exactamente esto, con el árbol montado en
solo lectura y sin código del juego (`tools/linux/distro_build.sh`, cabecera):

    ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13

Es el mismo script que usa el flujo `linux-distros.yml`. Duración 10-20 min **estimado** la primera vez.
El script es `sh`; lanzarlo desde Git Bash o desde WSL [sin verificar cuál de los dos funciona con Docker Desktop
en esta máquina].

**9.2 Imagen de pruebas** (ficheros nuevos; nombres libres de trampas): `tools/docker/Dockerfile.pruebas`.
Contenido previsto: base `debian:13` (tiene SDL3 empaquetado, `tools/linux/distro_build.sh` `deps_for`), los
paquetes de compilación de `docs/linux.md` ("Building from source"), `python3-venv`, `iproute2` (para `tc`), y un
venv con `lz4 capstone unicorn`. El código se monta, no se copia en la imagen. Nunca se monta la carpeta del juego.

    docker build -f tools/docker/Dockerfile.pruebas -t swcoop-pruebas .
    docker run --rm -v "$PWD":/src:ro -w /src swcoop-pruebas python3 tools/i18n_check.py
    docker run --rm -v "$PWD":/src:ro -w /src swcoop-pruebas /opt/venv/bin/python tools/test_layout.py
    docker run --rm -v "$PWD":/src:ro -w /src swcoop-pruebas /opt/venv/bin/python tools/test_game_sites.py

Tamaño de imagen: 1-2 GB **estimado** (muy por debajo de los 15 GB reservados en el plan).

**9.3 Red con bots** (`tools/docker/compose.yml`). Servicios previstos:

- `servidor`: el ejecutable Linux compilado dentro del contenedor, lanzado como
  `build/savagewind --server 52530` (servidor dedicado sin juego, `runtime/main.c:440-446`). **Sin verificar** que
  arranque sin pantalla en un contenedor; si no arranca, el servidor será el juego nativo del dueño y los bots se
  conectarán a `host.docker.internal`.
- `bot-a`, `bot-b`: `tools/online_bot.c` compilado con `cc -O2 -o /tmp/online_bot tools/online_bot.c`
  (`tools/online_bot.c:7`; en Linux puede hacer falta añadir `-lm` porque usa `math.h` [sin verificar]).
- Latencia y pérdida: `tc qdisc add dev eth0 root netem delay 80ms 20ms loss 2%` en cada bot, con
  `cap_add: [NET_ADMIN]`.

Limitaciones conocidas del bot que condicionan lo que esta prueba demuestra (mapa 06 §7.2): lleva el protocolo 38
escrito a mano (`tools/online_bot.c:102`), su buffer es de 8192 bytes (`:107`), no envía PING ni SKEL, y
`BOT_FREEZE` deja de enviar pero sigue leyendo. Con el servidor dedicado nadie es anfitrión, así que no circulan
ENEMIES, LOOTS ni QUESTS (mapa 06 §1.1). En F0 la prueba solo demuestra: conexión, relé de STATE entre dos bots y
expulsión por silencio a los 10 s. Lo demás es F2 y F3.

Rollback: `docker compose -f tools/docker/compose.yml down`, `docker image rm swcoop-pruebas`,
`docker volume ls` para los volúmenes `swr-pkgcache-*` que crea `distro_build.sh`.

## 10. Worktrees para los agentes (preparación para F1 y F2)

    git worktree add "$SW/wt/<nombre>" -b coop/<nombre> coop/main

Cada worktree nace **sin** `build/`, `generated/`, `tools/venv/`, `online_mod/` ni `game_dir.txt` (todos ignorados
por git). Por worktree: copiar `game_dir.txt`, crear o enlazar el venv, y compilar con `make -j8`. Compartir un
`generated/` entre worktrees solo vale si su `GAMELIB_ID` coincide
(`make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id`); si un agente toca
`tools/patches.py` o `tools/game_sites.py`, el suyo deja de coincidir. Disco por worktree: **sin medir**; con 70 GB
libres hay que contar cuántos caben antes de abrir el tercero.

## 11. Ficheros implicados

| Fichero | Para qué |
|---|---|
| `tools/setup_windows.ps1` | instalación única |
| `docs/BUILD-windows.md`, `docs/linux.md`, `docs/game-files.md`, `AGENTS.md` | fuentes de los comandos |
| `Makefile:336-340`, `:353`, `:483-486` | `GAME_DIR`, `gen`, `runtime-objs`, `online-helper` |
| `runtime/main.c:345-365`, `:492-524` | carpeta del juego propia; generación automática del overlay |
| `runtime/platform/windows_update.c:46-49`, `runtime/update_appimage.c:26-28`, `runtime/post_update.c:434-448`, `recomplauncher.json:5` | actualizador |
| `.github/workflows/*.yml`, `.github/dependabot.yml` | CI restaurado y ajustado |
| `tools/linux/distro_build.sh` | compilación Linux en contenedor (se reutiliza tal cual) |
| `tools/docker/Dockerfile.pruebas`, `tools/docker/compose.yml` | nuevos |
| `tools/online_bot.c`, `tools/online_mod.py`, `tools/sw_probe.py`, `tools/swctl.py` | se usan, no se modifican en F0 |
