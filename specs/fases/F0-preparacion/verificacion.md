# F0 — Preparación · verificación

La fase se declara hecha cuando todas las casillas están marcadas **con evidencia** (comando y salida resumida) o
tienen escrito por qué no aplican, y el dueño lo aprueba. "Compila" no cuenta como "funciona": S5 exige ver el
mundo.

## 1. Requisitos del dueño

- [ ] Fork en CosmicAdc con las ramas `test` y `main` (T001).
- [ ] Carpeta del juego con `exefs/` y `romfs/` (y `aoc1/`, `aoc2/` si hay DLC), fuera del repositorio (T002).
- [ ] Disco libre anotado antes de empezar: ______ GB (T003).

## 2. Entorno y repositorio

- [ ] `clang --version`, `make --version` responden en MSYS2 UCRT64.
- [ ] `tools/venv/Scripts/python.exe -c "import capstone, unicorn, lz4, oead"` termina con 0.
- [ ] `git remote -v`: `origin` = CosmicAdc; `upstream` y `pr294` con URL de subida anulada.
- [ ] `git log -1 --format=%h pr-294-online` = ________ (esperado `350c5f8`).
- [ ] `git diff --stat upstream/test -- .github` sin ficheros borrados.
- [ ] Decisión del actualizador escrita en `specs/decisiones/` y aplicada.
- [ ] `specs/` dentro del fork; comprobación de trampas de nombres (`plan.md` §8) limpia.

## 3. Línea base de pruebas sin juego

Rellenar: PASA / FALLA / NO EJECUTADA, y el motivo si no pasa.

| Prueba | Windows (MSYS2) | Docker (Linux) | Nota |
|---|---|---|---|
| `tools/test_game_sites.py` | | | |
| `tools/test_layout.py` | | | |
| `tools/test_simd.py --jobs 8` | | no previsto | |
| `tools/i18n_check.py` | | | fallo conocido en el PR: textos de Online sin traducir |
| `make test-control` | | no previsto | |
| `make test-update-core` | | no previsto | |
| `make test-console` | | no previsto | |
| `make test-places` | | no previsto | |
| `make test-mods` | | no previsto | |
| `make test-launch-flags` | | no previsto | usa `python3` |
| `bash tools/test_dsu.sh` | | no previsto | |

## 4. Juego

- [ ] `sw_probe.py` imprime el build id `cd57b23fa4bbad65`.
- [ ] `make gen` termina con 0. Duración: ______. `du -sh generated`: ______.
- [ ] `make -j8` (o `-j4`) termina. Duración: ______. Pico de RAM: ______. `du -sh build`: ______.
- [ ] El `.id` de `build/swgame.dll` coincide con el `GAMELIB_ID` del Makefile.
- [ ] `online_mod/v42` y `online_mod/online_mod.txt` existen. Duración de `online_mod.py`: ______.
- [ ] Arranque desatendido: `swctl.py state` y `swctl.py pos` responden; log con `game version 1.9.0`.
- [ ] Arranque con ventana visto por el dueño (FPS: ______).

## 5. Docker

- [ ] `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13` termina con 0.
- [ ] `docker build -f tools/docker/Dockerfile.pruebas -t swcoop-pruebas .` termina con 0. Tamaño: ______.
- [ ] Pruebas Python en la imagen con el mismo resultado que en Windows.
- [ ] `docker compose -f tools/docker/compose.yml up --abort-on-container-exit`: dos bots entran, se mantienen 30 s
      con `netem`, y el bot congelado es expulsado. Si el servidor dedicado no arrancó en contenedor, anotado.

## 6. Criterios de salida de `spec.md`

| # | Cumplido | Evidencia (comando → salida resumida) |
|---|---|---|
| S1 | | |
| S2 | | |
| S3 | | |
| S4 | | |
| S5 | | |
| S6 | | |
| S7 | | |
| S8 | | |
| S9 | | |
| S10 | | |

## 7. Plantilla de bitácora

Copiar a `specs/bitacora/<aaaa-mm-dd>-F0-<tema>.md`. Sin rutas personales ni logs crudos: los logs se guardan
fuera del repositorio y aquí se citan solo las líneas relevantes.

```markdown
# Bitácora · F0 · <tema>

- Fecha:
- Quién ejecutó: dueño / agente
- Commit del fork (`git rev-parse --short HEAD`) y rama:
- Commit de la rama del PR (`git log -1 --format=%h pr-294-online`):
- Versión del juego (build id de `sw_probe.py`) y DLC instalados:
- Hardware: CPU, RAM total, GPU y versión del controlador, disco libre antes / después:
- Sistema: versión de Windows, MSYS2 (`pacman -Q mingw-w64-ucrt-x86_64-clang`), Docker Desktop y su límite de memoria:
- Ajustes: resolución, límite de FPS, escalado, opciones cambiadas respecto al valor por defecto:
- FPS observados (mínimo / habitual) y dónde:

## Qué se hizo
(comando por comando, en orden)

## Medidas
| Paso | Duración | Disco | Pico de RAM | Nota |
|---|---|---|---|---|
| setup_windows.ps1 | | | | |
| make gen | | | | |
| make -j8 | | | | |
| online_mod.py | | | | |
| distro_build.sh debian:13 | | | | |
| docker build | | | | |

## Qué se observó
(hechos; separar hipótesis, comprobaciones sintéticas, compilaciones y juego verificado)

## Fallos y pendientes
(cada uno: qué, cómo se reproduce, a qué fase pasa)

## Logs adjuntos
(nombre del fichero fuera del repositorio y las líneas citadas)
```
