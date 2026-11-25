# F0 — Preparación · spec

Estado: pendiente de confirmación del dueño. Nada de esta fase se ejecuta hasta que el dueño confirme F0
(`PLAN-APROBADO.md`, "Primer paso al aprobar").

Convención de rutas en todos los documentos de esta fase: `$SW` es la carpeta de trabajo que contiene `Game/`,
`pr294-online/` y `specs/`. No se escriben rutas absolutas de la carpeta de usuario de Windows: el CI del proyecto
las rechaza (ver "Trampas de nombres" en `plan.md`).

## Objetivo

Dejar la máquina del dueño y el fork de CosmicAdc listos para trabajar: herramientas instaladas, rama `coop/main`
creada sobre el PR #294 con el CI restaurado, el juego compilado y arrancando en solitario, las pruebas que no
necesitan el juego ejecutadas con su resultado anotado, y un entorno Docker para pruebas en Linux y de red.

## Alcance

1. Requisitos que solo puede cumplir el dueño: fork en GitHub, archivos del juego extraídos, disco libre.
2. MSYS2 UCRT64 y dependencias con `tools/setup_windows.ps1`; entorno Python `tools/venv`
   (capstone, unicorn, lz4 y también `oead`, que el script del PR ya instala: `tools/setup_windows.ps1:29`).
3. Clon del fork con remotos `origin` (CosmicAdc), `upstream` (Revivedx, sin permiso de subida) y `pr294`
   (fork de los autores del PR, solo lectura).
4. Rama `coop/main` desde `pr-294-online`, con `.github/workflows/` y `.github/dependabot.yml` restaurados
   desde `upstream/test`.
5. Actualizador automático redirigido o desactivado para que un binario del fork no ofrezca la versión oficial.
6. `specs/` movida dentro del fork.
7. Verificación de la versión del juego con `tools/sw_probe.py`; `make gen`; compilación; overlay de actores con
   `tools/online_mod.py`; primer arranque en solitario hasta el mundo.
8. Línea base de pruebas sin juego (Python y C), con lo que falla anotado como fallo conocido.
9. Imagen Docker de pruebas, comprobación de compilación en Linux y `docker compose` con servidor dedicado y bots
   bajo latencia y pérdida simuladas.
10. Lista de trampas de nombres del CI, aplicada a todo lo que se añada.

## Fuera de alcance

- Cualquier cambio de comportamiento del modo online (eso es F1 y F2).
- Instrucciones para volcar o descifrar el juego. Extraer los NSP es un paso del dueño con las claves de su
  consola; aquí solo figura como requisito previo.
- Go, servidor relay, ENet (F3).
- Publicar binarios o releases del fork.
- Subir nada a `Revivedx`. Subir al fork solo cuando el dueño lo pida.
- `README.md`, `constitution.md`, `contratos/`, `casos-de-uso/`, `aprendizaje/`: los escriben otros agentes.

## Casos de uso que cubre

Ninguno de forma directa. F0 es el requisito de CU-01, CU-02 y CU-03 (F1): sin juego compilado, overlay generado y
dos carpetas de guardado no se pueden probar.

## Criterios de salida (medibles)

| # | Criterio | Cómo se mide |
|---|---|---|
| S1 | El juego del dueño es 1.9.0 | `tools/sw_probe.py` imprime el build id `cd57b23fa4bbad65` (`tools/game_sites.py:46-48`) |
| S2 | `make gen` termina con código 0 | existe `generated/layout.c` y carpetas por módulo |
| S3 | La compilación termina | existen `build/savagewind.exe`, `build/savagewind.dll.a` y `build/swgame.dll` con su `.id` |
| S4 | El overlay de actores existe | existen `online_mod/v42` y `online_mod/online_mod.txt` (`runtime/main.c:507`, `:519`) |
| S5 | El juego llega al mundo en solitario | líneas `[vk] t=` del log con contador de dibujos propio del mundo, o respuesta de `swctl.py pos` con coordenadas |
| S6 | Línea base de pruebas anotada | tabla de `verificacion.md` rellena: cada prueba con PASA / FALLA / NO EJECUTADA y motivo |
| S7 | CI restaurado en la rama | `git diff --stat upstream/test -- .github` no lista ficheros borrados |
| S8 | Nada apunta al repositorio original para actualizarse | `grep -rn "Revivedx/SavageWindRecomp" runtime recomplauncher.json` solo devuelve lo que la decisión T007 dejó a propósito |
| S9 | Docker: pruebas, compilación Linux y red con bots | los tres comandos de `verificacion.md` §4 terminan con código 0 |
| S10 | Tiempos, disco y RAM anotados | entrada en `specs/bitacora/` con la plantilla de `verificacion.md` |

## Riesgos

| Riesgo | Efecto | Mitigación |
|---|---|---|
| Disco justo (unos 70 GB libres; el plan estima unos 40 GB de uso) | La compilación o la imagen Docker fallan a medias | T003 antes de empezar; medir con `df -h` antes y después de cada paso grande |
| RAM 15,7 GB con 20 hilos | `make -j` sin número (como dice `docs/BUILD-windows.md`) puede agotar la memoria | Usar `make -j8` y, si el sistema mata el proceso, `make -j4` (`AGENTS.md`, "Build and verify") |
| `python3` no existe en MSYS2 UCRT64 | Fallan las recetas `make test-*` que lo llaman literalmente (`Makefile:536`, `609`, `672`, `692`, `811`, `857`, `873`, `877`) | Ejecutar primero las que no lo usan; el resto, con el Python del venv a mano o en Docker |
| `tools/i18n_check.py` ya falla en el PR | La línea base nace en rojo en esa prueba | Anotarlo como fallo conocido (comprobado: sale con 1; 51 textos sin traducir en 12 idiomas y 3 en `es`) |
| El PR no se ha compilado nunca en esta máquina ni en Linux | Errores de compilación desconocidos | Tareas separadas para Windows y Linux; cualquier fallo se anota, no se arregla en F0 salvo que bloquee |
| Finales de línea CRLF | Los scripts bash se rompen | `tools/setup_windows.ps1:32-33` fija `core.autocrlf false`; se comprueba en cada worktree nuevo |
| Workflows de issues (`triage`, `stale`, `crash-save`) activos en el fork | Cierran o etiquetan issues del fork | Decisión en T008: dejarlos desactivados desde la web de GitHub |
| Nombres de carpeta o fichero prohibidos | El check "no game files" falla, o git ignora el fichero sin avisar | Lista de trampas en `plan.md` §8 y comprobación T019 |
| El servidor dedicado `--server` no se ha probado en contenedor | La prueba de red con bots no arranca | Marcado "sin verificar"; alternativa: servidor en el juego nativo y bots en Docker |
