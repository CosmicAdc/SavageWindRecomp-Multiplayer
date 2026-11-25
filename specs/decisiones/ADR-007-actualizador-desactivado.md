# ADR-007 · El actualizador queda desactivado en el fork (opción A)

- Estado: aceptada por el dueño (2026-11-24)
- Tarea: T007 de `fases/F0-preparacion/tasks.md`; opciones en `fases/F0-preparacion/plan.md` §4

## Contexto

El actualizador apunta al repositorio original (`Revivedx/SavageWindRecomp`). En una compilación desde fuentes no hace
nada, porque `update_check_start` vuelve si no hay `sw-manifest.txt` junto al ejecutable (lo escribe `make dist`). El
riesgo aparece el día que se empaquete un binario del fork: ofrecería "actualizarse" a la versión oficial, sin
cooperativo.

## Decisión

Opción A: desactivar. En `runtime/post_update.c` se define `SW_FORK_NO_UPDATE` (`#define SW_FORK_NO_UPDATE 1`, junto a
las declaraciones del principio del fichero) y la versión real de `update_check_start` vuelve al principio cuando está
definida. Es un cambio mínimo.

No se tocan: la versión vacía de `update_check_start` (la rama "neither" del fichero), `runtime/update_core.c` (lo
cubre `make test-update-core`) ni el resto del código del actualizador, que no se borra. Para volver a activarlo basta
quitar el `#define`.

## Por qué

- El fork no publica releases todavía, así que no hay nada a lo que actualizarse.
- Opción B (redirigir `REPO` y sustituir la clave pública de firma) obliga a generar y custodiar una clave privada y
  amplía la superficie sin necesidad hoy.
- Una release del fork no se actualizará sola. Se acepta mientras no haya releases.

## Lo que queda apuntando al original a propósito

Salida de `grep -rn "Revivedx/SavageWindRecomp" runtime recomplauncher.json`:

1. `runtime/platform/windows_update.c:46`: `#define REPO "Revivedx/SavageWindRecomp"`
2. `runtime/update_appimage.c:26`: `#define REPO "Revivedx/SavageWindRecomp"`
3. `recomplauncher.json:5`: `"repo": "Revivedx/SavageWindRecomp",`

Los tres se dejan tal cual: con el actualizador desactivado no se consultan desde el juego. Si algún día se empaqueta
una release del fork habrá que revisar la entrada 3 por separado (`recomplauncher.json`; no se comprobó qué programa lo lee, y
`update_check_start` no lo menciona) y decidir entonces si se pasa a la opción B.

## Verificación

- Hipótesis (leído): `update_check_start` vuelve antes de crear el hilo de comprobación.
- [sin verificar] `make runtime-objs` y `make test-update-core`: los ejecuta el orquestador (ADR-006).
