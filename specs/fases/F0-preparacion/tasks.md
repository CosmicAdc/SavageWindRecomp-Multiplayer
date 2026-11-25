# F0 — Preparación · tasks

## Cómo leer una tarea

Cada tarea está escrita para que la ejecute **un agente sin más contexto que este fichero**. Formato:

```
- [ ] T0xx [P] [área] descripción            ([P] = paralelizable; sin [P] = en serie)
  files:           ficheros que puede tocar (y solo esos)
  verify:          comando u observación que demuestra que funciona
  depende de:      tareas que deben estar hechas antes
  quién:           dueño (la persona) / agente
  contexto:        qué hay hoy (fichero:línea) y por qué hace falta la tarea
  pasos:           pasos numerados
  hecho cuando:    resultado observable
  no hagas:        trampas
  modelo sugerido: alto / medio / bajo
```

`modelo sugerido`: **alto** = exige razonar sobre concurrencia, memoria del juego o ingeniería inversa;
**medio** = cambios de código o de configuración con criterio; **bajo** = pasos mecánicos.

## Reglas que valen para TODAS las tareas (no se repiten en cada una)

- `$SW` es la carpeta de trabajo que contiene `Game/`, `pr294-online/`, `specs/` y, tras T004, `coop/` (el clon del
  fork). En MSYS2: `export SW="$(cygpath -u "$USERPROFILE")/Documents/SavageWind"`.
- Shell: **MSYS2 UCRT64**, desde la raíz del clon, con
  `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`. Python del proyecto:
  `tools/venv/Scripts/python.exe`.
- No modificar la carpeta `pr294-online/` (referencia de solo lectura) ni la carpeta `Game/`.
- No subir nada (`git push`) salvo en T025 y solo si el dueño lo pide. Nunca a `upstream`.
- No añadir al repositorio archivos del juego, guardados, claves, `generated/`, `build/`, `online_mod/` ni logs
  crudos. No copiar nada de proyectos de decompilación.
- No escribir rutas absolutas del equipo en ningún fichero del repositorio ni en mensajes de commit.
- No crear carpetas llamadas `input`, `save`, `keys`, `dump`, `generated`, `extracted`, `build`, ni ficheros
  llamados `main` o `sdk`: el CI los rechaza o git los ignora en silencio (`plan.md` §8).
- F0 **no arregla** fallos del modo online: se anotan en la bitácora y pasan a F1.
- Al terminar: entrada en `specs/bitacora/` y lista de lo que **no** se pudo verificar.

---

## A. Requisitos del dueño

- [~] T001 [P] [dueño] Crear el fork de `Revivedx/SavageWindRecomp` en la cuenta CosmicAdc, con todas las ramas.
  reclamada: orquestador (Opus), 2026-11-24 — fork existe (`main`); falta subir `test` (credencial HTTPS de CosmicAdc caducada)
  files: ninguno (web de GitHub)
  verify: `git ls-remote --heads https://github.com/CosmicAdc/SavageWindRecomp-Multiplayer.git` lista `refs/heads/test` y `refs/heads/main`
  depende de: —
  quién: dueño
  contexto: Todo el trabajo vive en un fork de CosmicAdc; nada se sube al proyecto original. El desarrollo del original ocurre en la rama `test` (`CONTRIBUTING.md`, "Which branch to work on"), así que el fork debe traerla.
  pasos:
    1. En la página del repositorio original, botón Fork, cuenta CosmicAdc.
    2. Desmarcar "Copy the main branch only" para copiar también `test`.
    3. En el fork: pestaña Actions → habilitar los flujos (se terminará de configurar en T008).
  hecho cuando: el comando de `verify` lista las dos ramas.
  no hagas: no abras un pull request hacia el repositorio original.
  modelo sugerido: bajo (tarea humana)

- [ ] T002 [P] [dueño] Extraer los NSP propios a una carpeta fuera del repositorio.
  files: `$SW/juego/` (fuera del repositorio)
  verify: `ls "$SW/juego/exefs"` muestra `main main.npdm rtld sdk subsdk0`; existe `"$SW/juego/romfs/System/Version.txt"`; `find "$SW/juego/romfs" -type f | wc -l` da del orden de 41 062
  depende de: —
  quién: dueño
  contexto: El proyecto no usa NSP: necesita `exefs/` y `romfs/` ya extraídos (`docs/game-files.md`). En `$SW/Game/` hay cuatro NSP (base v0, actualización 1.9.0, DLC 1 y DLC 2 versión 3.0). Extraerlos requiere las claves de la consola del dueño; el proyecto prohíbe documentar ese paso (`CONTRIBUTING.md`, "The one hard rule") y aquí tampoco se explica.
  pasos:
    1. Extraer base + actualización 1.9.0 **fusionadas**: ExeFS a `$SW/juego/exefs/`, RomFS a `$SW/juego/romfs/`.
    2. Opcional: RomFS del DLC 1 a `$SW/juego/aoc1/` y del DLC 2 a `$SW/juego/aoc2/`, cada uno en su carpeta.
    3. Comprobar con los comandos de `verify`.
  hecho cuando: la carpeta tiene la forma de `docs/game-files.md` y la ruta no contiene espacios.
  no hagas: no mezcles los DLC dentro de `romfs/` (provoca una carga infinita tras un jefe de Bestia Divina, `docs/game-files.md`); no pongas la carpeta dentro del clon; no la compartas ni la subas.
  modelo sugerido: bajo (tarea humana)

- [x] T003 [P] [dueño] Liberar disco.
  reclamada: orquestador (Opus), 2026-11-24 — 128 GB libres; ver bitacora/2026-11-24-F0-arranque.md
  files: ninguno
  verify: `df -h "$SW"` muestra al menos 60 GB libres (ideal 100 GB); cifra anotada en la bitácora
  depende de: —
  quién: dueño
  contexto: Hay unos 70 GB libres. Uso estimado del plan: juego extraído ~15 GB + Docker ~15 GB + compilación + MSYS2 ≈ 40 GB, sin contar un worktree por agente.
  pasos:
    1. Liberar espacio (papelera, descargas, imágenes Docker sin usar con `docker system df`).
    2. Tras T002, decidir si se borran o se mueven los NSP a otro disco.
    3. Anotar los GB libres.
  hecho cuando: la cifra está anotada y supera 60 GB.
  no hagas: no borres los NSP antes de comprobar T002 y T010.
  modelo sugerido: bajo (tarea humana)

## B. Herramientas y repositorio (en serie)

- [x] T004 [entorno] Clonar el fork, fijar remotos y traer la rama del PR #294.
  reclamada: orquestador (Opus), 2026-11-24 — pr-294-online = 350c5f8; ver bitacora/2026-11-24-F0-arranque.md
  files: `$SW/coop/` (clon nuevo), `$SW/coop/.git/config`
  verify: `git remote -v` muestra `origin` → CosmicAdc y `upstream` con URL de push no válida; `git push upstream --dry-run` falla; `git log -1 --format=%h pr-294-online` imprime un hash (esperado `350c5f8`)
  depende de: T001
  quién: agente
  contexto: Hoy existe `pr294-online/`, un checkout de referencia cuyo `origin` es el repositorio original; no se trabaja ahí. Los mapas de `specs/mapa/` citan `fichero:línea` del commit `350c5f8`; si la rama del PR ha avanzado, esas líneas se habrán movido.
  pasos:
    1. `cd "$SW" && git clone https://github.com/CosmicAdc/SavageWindRecomp-Multiplayer.git coop && cd coop`
    2. `git config core.autocrlf false && git config core.eol lf`
    3. `git remote add upstream https://github.com/Revivedx/SavageWindRecomp.git`
    4. `git remote set-url --push upstream NO-SE-SUBE-A-UPSTREAM`
    5. `git fetch upstream && git fetch upstream pull/294/head:pr-294-online`
    6. Pedir al dueño la URL del fork de los autores del PR (está en la página del PR); `git remote add pr294 <URL>` y `git remote set-url --push pr294 NO-SE-SUBE`. Si no la da, omitir este paso y anotarlo.
    7. Anotar el hash de `pr-294-online`.
  hecho cuando: los tres comandos de `verify` dan lo esperado y el hash está anotado.
  no hagas: no inventes la URL del fork de los autores; no cambies el `origin` de `pr294-online/`; no hagas `git push`.
  modelo sugerido: bajo

- [x] T005 [entorno] Instalar MSYS2 UCRT64, dependencias y el venv de Python.
  reclamada: orquestador (Opus), 2026-11-24 — verify = 0 (clang 23.1.5, make 4.4.1, venv OK); 8m33s; MSYS2 3,5 GB
  files: `tools/venv/` (ignorado por git); instalación de MSYS2 y Python fuera del repositorio
  verify: en MSYS2 UCRT64, `clang --version && make --version` responden y `tools/venv/Scripts/python.exe -c "import capstone, unicorn, lz4, oead"` termina con 0
  depende de: T004
  quién: dueño (lanza el instalador) + agente (comprueba)
  contexto: En el equipo faltan MSYS2, clang y make, y Python solo está como lanzador `py`. `tools/setup_windows.ps1` lo instala todo: MSYS2 con winget, paquetes UCRT64 (`:15-21`), Python 3.12 y `tools\venv` con `lz4 capstone unicorn oead` (`:28-29`), y fija finales de línea LF (`:32-33`).
  pasos:
    1. PowerShell en la raíz del clon: `powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1`
    2. Abrir "MSYS2 UCRT64" y ejecutar `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"`.
    3. Ejecutar los comandos de `verify`.
    4. Anotar duración y disco usado.
  hecho cuando: `verify` termina con 0.
  no hagas: no instales paquetes fuera de la lista del script; no uses el Python de MSYS2 para el venv (el script explica que no compila unicorn/capstone); no instales Go.
  modelo sugerido: bajo

- [x] T006 [git] Crear `coop/main` desde la rama del PR y restaurar el CI.
  reclamada: orquestador (Opus), 2026-11-24 — commit 0cbc5af
  files: `.github/workflows/activity-feed.yml`, `checks.yml`, `linux-distros.yml`, `macos-dmg.yml`, `macos.yml`, `tests.yml`, `windows.yml`, `.github/dependabot.yml`
  verify: `git branch --show-current` da `coop/main`; `git diff --stat upstream/test -- .github` no lista ficheros borrados
  depende de: T004
  quién: agente
  contexto: La rama del PR no tiene esos ocho ficheros: se pierden en cada merge que sus autores hacen desde `test` (artefacto del merge, no un cambio intencionado; `specs/mapa/06` §8.4). Sin ellos la rama no ejecuta pruebas ni la comprobación de archivos de Nintendo.
  pasos:
    1. `git switch -c coop/main pr-294-online`
    2. `git checkout upstream/test -- .github/workflows .github/dependabot.yml`
    3. `git diff --stat upstream/test -- .github` (debe salir vacío o sin "deleted").
    4. `git commit -m "CI: restaurar los flujos de test que el PR #294 pierde en sus merges"`
  hecho cuando: `verify` se cumple y hay un commit con solo ficheros de `.github/`.
  no hagas: no edites el contenido de los flujos aquí (eso es T008); no toques nada fuera de `.github/`.
  modelo sugerido: bajo

- [x] T007 [runtime] Decidir y aplicar qué hace el actualizador en el fork.
  reclamada: agente Sonnet t007-update, rama coop/T007 (a4588cc), integrada en 87ef166; verify orquestador: runtime-objs 0, test-update-core all passed
  files: `specs/decisiones/<n>-actualizador.md`; opción A: `runtime/post_update.c`; opción B: `runtime/platform/windows_update.c`, `runtime/update_appimage.c`, `runtime/platform/sw_update_key.h`, `recomplauncher.json`
  verify: `make test-update-core` termina con 0; `grep -rn "Revivedx/SavageWindRecomp" runtime recomplauncher.json` devuelve solo lo que la decisión deja a propósito y la decisión lo enumera
  depende de: T005, T006
  quién: dueño (decide) + agente (aplica)
  contexto: El actualizador apunta al repositorio original: `REPO` en `runtime/platform/windows_update.c:46` y `runtime/update_appimage.c:26`, y `recomplauncher.json:5`. En una compilación desde fuentes no hace nada: `update_check_start` (`runtime/post_update.c:434-441`) vuelve si no hay `sw-manifest.txt` junto al ejecutable. El riesgo aparece si algún día se empaqueta un binario del fork: ofrecería "actualizarse" a la versión oficial, sin cooperativo.
  pasos:
    1. Presentar al dueño las opciones A (desactivar) y B (redirigir) de `plan.md` §4. Recomendada: A.
    2. Escribir la decisión en `specs/decisiones/` (qué, por qué, qué queda apuntando al original).
    3. Opción A: en `update_check_start` de `runtime/post_update.c` (la versión real, línea 434), volver al principio cuando esté definida una macro de compilación del fork (por ejemplo `SW_FORK_NO_UPDATE`), definida en el mismo fichero. No tocar la versión vacía de la línea 519.
    4. `make runtime-objs` y `make test-update-core`.
  hecho cuando: decisión escrita, `verify` cumplido.
  no hagas: no generes ni guardes claves privadas en el repositorio; no cambies `runtime/update_core.c` (lo cubre una prueba); no borres el código del actualizador.
  modelo sugerido: medio

- [~] T008 [git] Hacer que el CI corra en `coop/main` y apagar los flujos de issues en el fork.
  reclamada: orquestador (Opus), 2026-11-24 — ficheros hechos (19eda12); falta la parte web del dueño (desactivar triage, stale, crash-save, macos-dmg)
  files: `.github/workflows/checks.yml`, `tests.yml`, `windows.yml`, `macos.yml`
  verify: `grep -n "coop/main" .github/workflows/checks.yml .github/workflows/tests.yml .github/workflows/windows.yml .github/workflows/macos.yml` da al menos una línea por fichero
  depende de: T006
  quién: agente (ficheros) + dueño (web)
  contexto: Esos cuatro flujos solo se disparan con `branches: [test, main]` (bloque `on:` de cada uno). `triage.yml`, `stale.yml` y `crash-save.yml` actúan sobre issues (y `stale` los cierra); `macos-dmg.yml` adjunta un `.dmg` a cada release; `activity-feed.yml` ya está limitado al repositorio original.
  pasos:
    1. En cada uno de los cuatro ficheros, en el bloque `on:`, cambiar `branches: [test, main]` por `branches: [test, main, coop/main]` (en `push` y en `pull_request`, donde existan).
    2. Commit aparte: "CI: ejecutar también en coop/main".
    3. Dueño: en la web del fork, Actions → desactivar `triage`, `stale`, `crash-save` y `macos-dmg`.
  hecho cuando: `verify` cumplido; tras el primer push que pida el dueño, Actions muestra `Checks`, `Tests` y `Windows` ejecutados.
  no hagas: no añadas secretos; no cambies los pasos de los jobs; no borres flujos.
  modelo sugerido: bajo

- [x] T009 [specs] Mover `specs/` al fork.
  reclamada: agente Sonnet t009-specs, 2026-11-24, rama coop/T009 (0e62884), integrada en db22ccb
  files: `specs/**`
  verify: las dos órdenes de `plan.md` §8 terminan con `1` (sin coincidencias) sobre lo preparado; tras el commit, `git status --short specs | wc -l` da 0
  depende de: T006
  quién: agente
  contexto: `specs/` vive hoy en `$SW/specs`, fuera de git. `checks.yml` rechaza líneas añadidas con rutas personales; las cabeceras de `specs/mapa/06-*.md` y `07-*.md` citan la ruta absoluta del checkout revisado.
  pasos:
    1. `cp -r "$SW/specs" "$SW/coop/specs"`
    2. Buscar rutas personales: `grep -rnE '[Cc]:[\\/]+Users[\\/]|/Users/[A-Za-z]' specs` y sustituir cada una por una ruta relativa (`pr294-online`, `$SW/...`).
    3. `git add specs` y pasar las dos órdenes de `plan.md` §8.
    4. Commit: "specs: carpeta de especificaciones del cooperativo".
  hecho cuando: `verify` cumplido.
  no hagas: no cambies el contenido técnico de los mapas; no añadas `specs/governance/audit/` sin preguntar al dueño si es suyo.
  modelo sugerido: bajo

## C. Juego (en serie; necesita T002)

- [ ] T010 [juego] Apuntar la rama a los archivos del juego y comprobar la versión.
  files: `game_dir.txt` (ignorado por git)
  verify: `tools/venv/Scripts/python.exe tools/sw_probe.py "<carpeta del juego>/exefs"` imprime el build id `cd57b23fa4bbad65` y termina con 0; `git status --short` no muestra `game_dir.txt`
  depende de: T002, T005
  quién: agente
  contexto: Esta rama lee el juego de `extracted/` o de la carpeta escrita en la primera línea de `game_dir.txt` (`Makefile:336`, `runtime/main.c:345-365`). La única versión soportada es 1.9.0, build id `cd57b23fa4bbad65` (`tools/game_sites.py:46-48`). `sw_probe.py` no escribe nada junto al juego.
  pasos:
    1. Escribir en `game_dir.txt` (raíz del clon) la carpeta de T002, con barras normales, en una sola línea.
    2. Ejecutar `sw_probe.py` como en `verify` y guardar la salida fuera del repositorio.
    3. Si el build id no coincide: parar y avisar al dueño.
  hecho cuando: build id correcto anotado.
  no hagas: no copies archivos del juego dentro del repositorio salvo a `extracted/` (ignorada) si `game_dir.txt` diera problemas; no hagas commit de `game_dir.txt`.
  modelo sugerido: bajo

- [ ] T011 [juego] Generar el código del juego (`make gen`).
  files: `generated/` (ignorado por git)
  verify: `make gen` termina con 0; existen `generated/layout.c` y `generated/main/`; `du -sh generated` anotado
  depende de: T010
  quién: agente
  contexto: `make gen` (`Makefile:339-340`) traduce los módulos del ExeFS del dueño a C con el Python del venv. Solo hay que repetirlo si cambian `tools/patches.py`, `tools/game_sites.py` u otros ficheros de `GAMELIB_SRCS`.
  pasos:
    1. `time make gen`
    2. Anotar duración y `du -sh generated`.
  hecho cuando: `verify` cumplido.
  no hagas: no edites nada dentro de `generated/`; no lo añadas a git; no uses `GENARGS=--any-version`.
  modelo sugerido: bajo

- [ ] T012 [juego] Compilar la aplicación y la biblioteca del juego.
  files: `build/` (ignorado por git)
  verify: `ls -l build/savagewind.exe build/savagewind.dll.a build/swgame.dll` muestra los tres; el fichero `.id` junto a `swgame.dll` coincide con `make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id`
  depende de: T011
  quién: agente
  contexto: `docs/BUILD-windows.md` dice `make -j` (sin número); `AGENTS.md` dice `make -j8` y `-j4` si el sistema mata el proceso por memoria. Con 20 hilos y 15,7 GB de RAM se usa `-j8`. `make runtime-objs` (`Makefile:353`) compila solo el runtime y sirve de comprobación rápida.
  pasos:
    1. Cerrar Docker Desktop y el navegador.
    2. `make runtime-objs` (si falla, copiar el primer error y parar).
    3. `time make -j8`; si se queda sin memoria, `make -j4`.
    4. Anotar duración, pico de RAM (Administrador de tareas) y `du -sh build`.
  hecho cuando: `verify` cumplido.
  no hagas: no cambies flags de optimización ni de coma flotante; no edites el `.id` para saltarte la comprobación; no arregles errores de compilación del PR sin anotarlos y avisar.
  modelo sugerido: bajo

- [ ] T013 [juego] Generar el overlay de actores del modo online.
  files: `online_mod/` (ignorado por git)
  verify: existen `online_mod/v42`, `online_mod/online_mod.txt`, `online_mod/gear.txt` y `online_mod/quests.txt`; `git status --short` no muestra `online_mod/`
  depende de: T010
  quién: agente
  contexto: Los Links remotos son actores `Online*` fabricados a partir de los archivos del propio usuario por `tools/online_mod.py` (necesita `oead`). El juego lo lanza solo en el primer arranque si falta `online_mod/v42` (`runtime/main.c:507-516`), pero a mano se ven los errores (`docs/ONLINE.md`, "The players' Links").
  pasos:
    1. `time tools/venv/Scripts/python.exe tools/online_mod.py "<carpeta del juego>/romfs" online_mod`
    2. Comprobar los ficheros de `verify`; anotar duración y `du -sh online_mod`.
  hecho cuando: `verify` cumplido.
  no hagas: no subas `online_mod/` (deriva del juego); no cambies `VERSION` en `tools/online_mod.py`.
  modelo sugerido: bajo

- [ ] T014 [juego] Primer arranque en solitario, desatendido.
  files: `$SW/pruebas/` (fuera del repositorio)
  verify: `tools/venv/Scripts/python.exe tools/swctl.py --wait 120 state` responde; `swctl.py pos` devuelve coordenadas; el log contiene `[gamelib]` con `game version 1.9.0` y `[online] online play's actors: online_mod`
  depende de: T012, T013
  quién: agente
  contexto: `AGENTS.md` ("Build and verify") describe la ejecución desatendida: copias de guardado y de ajustes, `SAVAGEWIND_NO_LAUNCHER=1`, `SAVAGEWIND_HIDDEN=1`, `SAVAGEWIND_TEST_WARP=plateau`, `SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"` (pulsa Continuar). `SAVAGEWIND_CONTROL=7777` abre el socket de control (`docs/CONTROL.md`).
  pasos:
    1. Crear `$SW/pruebas/guardado-solo/` como **copia** de una carpeta de guardado (o vacía, si el dueño no tiene) y un `ajustes-solo.ini` vacío.
    2. Exportar las variables de `plan.md` §6 paso 5 y lanzar `./build/savagewind.exe > "$SW/pruebas/solo.log" 2>&1 &`
    3. Ejecutar los comandos `swctl.py` de `verify` y después `swctl.py exit`.
    4. Buscar en el log las líneas `[vk] t=` y anotar el contador de dibujos.
  hecho cuando: `verify` cumplido y el proceso termina limpio con `exit`.
  no hagas: no uses la carpeta de guardado real del dueño; no lances la ventana bajo `timeout` de MSYS; no guardes el log dentro del repositorio.
  modelo sugerido: medio

- [ ] T015 [juego] Arranque con ventana, visto por el dueño.
  files: ninguno
  verify: observación del dueño anotada en la bitácora (FPS, GPU y controlador, ajustes, qué se vio)
  depende de: T014
  quién: dueño
  contexto: Regla del proyecto: algo cuenta como hecho cuando se ha visto funcionar en el juego, no cuando compila. La pestaña Online está en el menú F1, solo dentro del juego (`runtime/ui/menu.mm`, id 13).
  pasos:
    1. Lanzar `./build/savagewind.exe` desde MSYS2 UCRT64 (sin `SAVAGEWIND_HIDDEN`).
    2. Cargar partida, caminar un minuto, abrir F1 → Online.
    3. Anotar FPS y cualquier defecto.
  hecho cuando: el dueño confirma que jugó y vio la pestaña.
  no hagas: no crees todavía una sesión (es F1).
  modelo sugerido: bajo (tarea humana)

## D. Línea base de pruebas sin juego

- [x] T016 [P] [pruebas] Ejecutar las pruebas Python sin juego y anotar su resultado.
  reclamada: orquestador (Opus), 2026-11-24 — test_game_sites 0, test_layout 0, i18n_check 1 (esperado: 51 textos de Online sin traducir por idioma); test_simd 0 (159 968 casos, 2m40s)
  files: ninguno (solo lectura); resultado en `specs/bitacora/`
  verify: los cuatro comandos ejecutados y su código de salida anotado en `verificacion.md` §3. Esperado hoy: `i18n_check.py` sale con 1
  depende de: T005
  quién: agente
  contexto: Pruebas del proyecto que no necesitan el juego (`AGENTS.md`). En el PR, `tools/i18n_check.py` falla (comprobado: 51 textos de Online sin traducir en 12 idiomas y 3 en `es`).
  pasos:
    1. `tools/venv/Scripts/python.exe tools/test_game_sites.py; echo $?`
    2. `tools/venv/Scripts/python.exe tools/test_layout.py; echo $?`
    3. `tools/venv/Scripts/python.exe tools/test_simd.py --jobs 8; echo $?`
    4. `tools/venv/Scripts/python.exe tools/i18n_check.py | tail -20; echo ${PIPESTATUS[0]}`
    5. Rellenar la tabla.
  hecho cuando: la tabla tiene los cuatro resultados.
  no hagas: no traduzcas ni arregles nada; no marques como PASA una prueba que no terminó.
  modelo sugerido: bajo

- [ ] T017 [pruebas] Ejecutar las pruebas en C que no usan `python3`.
  files: `build/` (ignorado por git)
  verify: `make test-control test-update-core test-console test-places test-mods` — cada objetivo termina con 0, o queda anotado cuál falla y con qué mensaje
  depende de: T012
  quién: agente
  contexto: Objetivos del `Makefile` (`:745`, `:847`, `:767`, `:757`, `:711`). Cada prueba es un programa con `assert` que sale con 0. Va en serie con T012 porque comparten `build/` y el compilador.
  pasos:
    1. Lanzar cada objetivo por separado y anotar el resultado.
  hecho cuando: cinco resultados anotados.
  no hagas: no modifiques pruebas ni código para que pasen.
  modelo sugerido: bajo

- [ ] T018 [pruebas] Probar las recetas que llaman a `python3` y la prueba DSU.
  files: `build/`
  verify: resultado por objetivo (PASA / FALLA / NO EJECUTADA con motivo) en `verificacion.md` §3
  depende de: T017
  quién: agente
  contexto: Varias recetas llaman a `python3` literalmente (`Makefile:536`, `672`, `877`...). `setup_windows.ps1` no instala un `python3` en MSYS2, así que pueden fallar por eso y no por el código. El PR quitó el bit de ejecución a `tools/test_dsu.sh`.
  pasos:
    1. `command -v python3` y anotar si existe.
    2. `make test-launch-flags`, `make test-actions`, `make test-gyro`; anotar.
    3. `bash tools/test_dsu.sh`; anotar.
  hecho cuando: resultados anotados, distinguiendo "falla la prueba" de "falta `python3`".
  no hagas: no instales paquetes nuevos sin preguntar; no regeneres `runtime/launch_flags_table.c`.
  modelo sugerido: bajo

- [x] T019 [git] Pasar la comprobación de trampas de nombres sobre todo lo añadido en F0.
  reclamada: orquestador (Opus), 2026-11-24 — sin nombres prohibidos ni rutas personales en lo añadido respecto a upstream/test
  files: `specs/fases/F0-preparacion/verificacion.md` (resultado)
  verify: las dos órdenes de `plan.md` §8 terminan con `1` sobre `git diff --cached`; `git diff --name-only --diff-filter=A upstream/test...HEAD | grep -iE '(^|/)(input|save|keys|dump|build|generated[^/]*|extracted)/'` no devuelve nada
  depende de: T009
  quién: agente
  contexto: `checks.yml` rechaza nombres de carpetas y ficheros que se parezcan a archivos del juego, y rutas personales en líneas añadidas; `.gitignore` ignora en silencio varias de esas carpetas.
  pasos:
    1. Ejecutar las órdenes de `verify`.
    2. Renombrar o corregir lo que aparezca.
  hecho cuando: ninguna coincidencia.
  no hagas: no desactives ni edites `checks.yml` para que pase.
  modelo sugerido: bajo

## E. Docker (en paralelo con C y D)

- [x] T020 [P] [docker] Comprobar que la rama compila en Linux.
  reclamada: orquestador (Opus), 2026-11-24 — **FALLA** (código 1, 1m53s): `runtime/control.h:93` usa `size_t` sin `#include <stddef.h>` (el PR lo perdió; upstream/test lo tiene). Pasa a F1
  files: ninguno (el script monta el árbol en solo lectura)
  verify: `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13` termina con 0; si falla, el primer error queda en la bitácora
  depende de: T006
  quién: agente
  contexto: `tools/linux/distro_build.sh` compila la aplicación (sin código del juego) dentro de un contenedor de la distribución indicada; es lo que usa `linux-distros.yml`. Nadie ha compilado el código del PR en Linux en este proyecto. Docker Desktop tiene 6,2 GB de memoria, por eso `JOBS=4`.
  pasos:
    1. Arrancar Docker Desktop.
    2. Desde Git Bash en la raíz del clon, lanzar el comando de `verify`.
    3. Anotar duración y resultado.
  hecho cuando: código de salida anotado; si no es 0, el error copiado sin rutas personales.
  no hagas: no arregles el error en esta tarea; no montes la carpeta del juego.
  modelo sugerido: bajo

- [x] T021 [P] [docker] Crear la imagen de pruebas.
  reclamada: agente Sonnet t021-docker, rama coop/T021 (cea84f2), integrada en c0e1046; verify orquestador: build 0 (3m33s, 1,23 GB), clang 0, python 0
  files: `tools/docker/Dockerfile.pruebas`
  verify: `docker build -f tools/docker/Dockerfile.pruebas -t swcoop-pruebas .` termina con 0; `docker run --rm swcoop-pruebas clang --version` responde; `docker run --rm swcoop-pruebas /opt/venv/bin/python -c "import lz4, capstone, unicorn"` termina con 0
  depende de: T006
  quién: agente
  contexto: No existe ninguna imagen de pruebas. Los paquetes de compilación para Debian 13 están en `docs/linux.md` ("Building from source") y en `deps_for` de `tools/linux/distro_build.sh`.
  pasos:
    1. Escribir el Dockerfile: `FROM debian:13`; `apt-get install` de `make clang lld pkg-config python3 python3-venv iproute2 git libsdl3-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libvulkan-dev libshaderc-dev zlib1g-dev`; `python3 -m venv /opt/venv && /opt/venv/bin/pip install lz4 capstone unicorn`; `WORKDIR /src`.
    2. Construir y ejecutar los comandos de `verify`.
    3. Anotar `docker image ls swcoop-pruebas`.
  hecho cuando: `verify` cumplido.
  no hagas: no copies el código ni el juego dentro de la imagen (se montan al ejecutar); no pongas el Dockerfile en una carpeta llamada `build`.
  modelo sugerido: medio

- [x] T022 [docker] Ejecutar en la imagen las pruebas Python sin juego.
  reclamada: orquestador (Opus), 2026-11-24 — Docker: test_layout 0, test_game_sites 0, i18n_check 1 (igual que T016)
  files: ninguno
  verify: `docker run --rm -v "$PWD":/src:ro -w /src swcoop-pruebas /opt/venv/bin/python tools/test_layout.py` y lo mismo con `tools/test_game_sites.py` terminan con 0; `python3 tools/i18n_check.py` da el mismo resultado que T016
  depende de: T021
  quién: agente
  contexto: Sirve para confirmar que las pruebas dan lo mismo en Linux que en Windows antes de usarlas como puerta en F2.
  pasos:
    1. Lanzar los tres comandos; anotar resultados en la columna Docker de `verificacion.md` §3.
  hecho cuando: columna rellena.
  no hagas: no montes el árbol en modo escritura; si una prueba necesita escribir, anótalo y para.
  modelo sugerido: bajo

- [ ] T023 [docker] `docker compose` con servidor dedicado y dos bots bajo latencia y pérdida.
  files: `tools/docker/compose.yml`, `tools/docker/red-bots.sh`
  verify: `docker compose -f tools/docker/compose.yml up --abort-on-container-exit` muestra en el log del servidor dos líneas `[online] server: <nombre> joined (slot N)` y los bots siguen conectados 30 s con `netem delay 80ms loss 2%`; un bot con `BOT_FREEZE=5` es expulsado tras unos 10 s
  depende de: T020, T021
  quién: agente
  contexto: `savagewind --server [puerto]` es un servidor sin juego (`runtime/main.c:440-446`, `runtime/online.c:412`). `tools/online_bot.c` es un jugador sin juego (compilación en su línea 7; protocolo 38 escrito a mano en la línea 102). El servidor expulsa a quien calla 10 s (`TIMEOUT_NS`, `runtime/online.c:46`). **Sin verificar** que `--server` arranque sin pantalla en un contenedor.
  pasos:
    1. `red-bots.sh`: compila la aplicación en `/tmp` con `make -j4 B=/tmp/b GEN=/nonexistent` (la misma forma que usa `tools/linux/distro_build.sh`) y el bot con `cc -O2 -o /tmp/online_bot tools/online_bot.c -lm`.
    2. `compose.yml`: servicio `servidor` (`/tmp/b/savagewind --server 52530`), servicios `bot-a` y `bot-b` (`/tmp/online_bot servidor 52530 BotA`), con `cap_add: [NET_ADMIN]` y `tc qdisc add dev eth0 root netem delay 80ms 20ms loss 2%` antes de lanzar el bot.
    3. Añadir un tercer bot con `BOT_FREEZE=5` y comprobar la expulsión.
    4. Si el servidor no arranca en contenedor: anotarlo, y usar como servidor el juego nativo (`SAVAGEWIND_ONLINE=host`) con los bots apuntando a `host.docker.internal`.
  hecho cuando: `verify` cumplido, o la alternativa del paso 4 funcionando y anotada.
  no hagas: no modifiques `tools/online_bot.c` ni `runtime/online.c`; no publiques el puerto fuera de la red de compose.
  modelo sugerido: medio

## F. Cierre

- [ ] T024 [bitácora] Entrada de bitácora de F0 y criterios de salida.
  files: `specs/bitacora/<fecha>-F0.md`, `specs/fases/F0-preparacion/verificacion.md`
  verify: los diez criterios S1-S10 de `spec.md` tienen evidencia (comando y salida resumida) o un "no cumplido" con motivo; el dueño da el visto bueno
  depende de: T014, T015, T016, T017, T018, T019, T022, T023
  quién: agente (redacta) + dueño (aprueba)
  contexto: La salida de F0 exige tiempos y uso de disco anotados, y la línea base de pruebas. La plantilla está en `verificacion.md` §7.
  pasos:
    1. Copiar la plantilla y rellenarla con lo anotado en cada tarea.
    2. Marcar S1-S10 en `verificacion.md` §6.
    3. Listar fallos conocidos y a qué fase pasa cada uno.
  hecho cuando: el dueño aprueba.
  no hagas: no pegues logs crudos ni rutas personales; no marques un criterio sin evidencia.
  modelo sugerido: bajo

- [ ] T025 [git] Subir `coop/main` al fork, solo si el dueño lo pide.
  files: ninguno
  verify: `git ls-remote --heads origin coop/main` devuelve el commit local; `git ls-remote --heads upstream coop/main` no devuelve nada
  depende de: T024
  quién: dueño (pide) + agente (ejecuta)
  contexto: Regla del proyecto: nada se sube a `Revivedx`; al fork, solo cuando el dueño lo pida.
  pasos:
    1. Esperar la petición explícita del dueño.
    2. `git push -u origin coop/main`
  hecho cuando: `verify` cumplido.
  no hagas: no uses `--force`; no subas otras ramas; no abras pull requests.
  modelo sugerido: bajo

## Resumen

25 tareas (T001-T025). Paralelizables: T001-T003 entre sí; T016; T020 y T021. El resto va en serie por dependencia
real (herramientas → repositorio → juego → pruebas → cierre).
