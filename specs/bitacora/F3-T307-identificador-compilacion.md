# F3 · T307 · Qué identificador de compilación se puede comparar entre dos PC

- Fecha: 2026-11-25 · rama `coop/T307` · agente Sonnet (ADR-006: solo lectura; no se compiló ni se ejecutó nada)
- Estado: **investigación por lectura de código y de logs reales. Las órdenes de las preguntas 1 y 3 las ejecuta el
  orquestador o el dueño** (hace falta Docker/Linux o una segunda máquina). Nada de esto está verificado en ejecución.

## Tabla de las tres preguntas de la tarea

| Pregunta | Respuesta | Evidencia |
|---|---|---|
| 1 | NO (lectura) / SIN EJECUTAR | `Makefile:136-142`: el hash lleva `GENFLAGS`, y `GENFLAGS` difiere por SO (`-D_FILE_OFFSET_BITS=64` en Windows, `-fPIC` en Linux; `Makefile:84-120`). Por tanto Windows y Linux dan `GAMELIB_ID` distinto con el mismo commit. Falta ejecutar la orden en ambos. |
| 2 | SÍ | `gamelib_id()` (`runtime/gamelib.c:~52`) devuelve el `GAMELIB_ID` (o `GAMELIB_ID_COMPAT`) compilado en el ejecutable (`Makefile:264`, `-DGAMELIB_ID`). No necesita biblioteca cargada. |
| 3 | SIN EJECUTAR | `--server` vuelve antes de `boot()` (`runtime/main.c:440-445`, `gamelib_load` está en `:182`), así que no pasa por el juego. Que no pida GPU ni pantalla no se ha probado. |

Órdenes que debe ejecutar el orquestador (pegar la salida debajo, en "Salidas"):

```
make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id            # Windows (MSYS2) y Linux (Docker), mismo commit
make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID_COMPAT)' print-gamelib-id     # idem, compatible
git log -1 --format='commit %h, tree %t'                                           # en ambos
savagewind --server 7777                                                           # en contenedor sin GPU
```

### Salidas

(pendiente: las pega el orquestador)

## Inventario de identificadores

| Identificador | Dónde se define / se lee | Qué contiene | ¿Igual en dos PC con mismo juego y mismo código? | Qué lo cambia sin importar |
|---|---|---|---|---|
| Versión del juego (`g_game_version`, "1.9.0") | `tools/layout.py:115` lo escribe en la biblioteca; `runtime/gamelib.c:78,~100` lo copia | Versión que reconoció el generador por el build id del `main` | **Sí** | Nada. Vale `""` en `--server` y antes de cargar |
| Build id del `main` del juego | `runtime/gamelib.h:34` (`GameLibModule.build_id`, 64 hex), comprobado en `runtime/loader.c:55` | Build id del NSO con el que se generó | **Sí** (el cargador aborta si no coincide con el NSO local). 1.9.0 = `cd57b23fa4bbad65` (`runtime/game_versions.h:14`) | Nada. Solo existe con el juego cargado |
| `GAMELIB_ID` | `Makefile:142`, `runtime/gamelib.c` `gamelib_id()` | sha256[:16] de: fuentes del generador y cabeceras (`GAMELIB_SRCS`), `GENFLAGS`, `GENFLAGS_ZIG`, `--locals` | **Solo en el mismo SO y la misma CPU-clase.** Windows ≠ Linux; CPU sin AVX2/FMA usa `GAMELIB_ID_COMPAT` | Ver "Ruido" abajo |
| `GAMELIB_ID_COMPAT` | `Makefile:150` | Igual con `-march=x86-64-v2` | Igual que el anterior; solo se usa si la CPU lo exige (`gamelib.c` `gamelib_compat`, o `SAVAGEWIND_COMPAT_CPU`) | Idem |
| `SW_SOURCE` (commit y árbol del runtime) | `Makefile:178`, impreso en `runtime/main.c:405` | `git log -1 --format='commit %h, tree %t'` + `, modified` si hay cambios en `runtime tools Makefile` | **No fiable tal cual**: `%h` y `%t` abrevian con longitud que depende del repo; `modified` aparece por cambios locales; `unknown` sin git (compilaciones empaquetadas) | Longitud de abreviatura, trabajo sin confirmar |
| `SAVAGEWIND_VERSION` (`rt_version()`) | `runtime/ui/launcher.mm:113`, `Makefile:379` | Cadena de versión ("0.6.0-alpha.6") | **Sí** si es la misma versión; no distingue dos commits de la misma versión | Nada |
| `online_mod` versión | `tools/online_mod.py:66` (`VERSION = 43`); `runtime/main.c:507` busca `online_mod/v43` (literal, sin constante compartida) | Versión del overlay RomFS generado | **Sí** entre PC con el mismo código; sale del código, no del juego | Que el overlay no se haya regenerado en una de las PC |
| `PROTOCOL` | `runtime/online_wire.h:8` (38), comparado en `runtime/online.c:196` | Versión del cable | **Sí** (constante) | Nada. No dice nada sobre el código del juego |
| `MAGIC` | `runtime/online_wire.h:7` | "SWOL" | Sí | — |
| Estado de DLC | `rt_dlc_check(root)`, `runtime/dlc_check.h:38` | Depende de los ficheros del jugador | Puede diferir legítimamente | Que una PC tenga el DLC y la otra no |

Nota sobre las líneas citadas en `tasks.md`: `PROTOCOL` está hoy en `runtime/online_wire.h`, no en `runtime/online.c:240`; el
`strcmp` de contraseña y demás no son de esta tarea.

## Ruido: qué cambia `GAMELIB_ID` (y qué no)

Entra en el hash (`Makefile:142`):
- Contenido de `tools/gen.py lifter.py analyze.py nso.py passes.py patches.py simd_c.py regs.py flags.py layout.py game_sites.py`, `gamelib_entry.c`, `runtime/hle/hle_symbols.txt`, `cpu.h simd.h simd_fp.h simd_softfma.h gamelib.h`.
- `GENFLAGS` (incluye `-O1 ...`, `-march=`, y los flags por SO), `GENFLAGS_ZIG`, `RECOMP_LOCALS`.
- Sobreescribir `GENARCH` en la línea de `make` o en el entorno cambia el id: dos PC que compilen con `GENARCH` distinto no coinciden, aunque el código sea el mismo.

No entra: versión de clang/zig, rutas, nombre de usuario, `GAMELIB_ARCH`, fecha, git. Es decir, **dos compiladores distintos con las mismas opciones dan el mismo id** y el id **no cubre el runtime** (`online.c`, `main.c`, etc. no están en `GAMELIB_SRCS`): detecta el código del juego recompilado, no el del modo cooperativo.

Riesgo de fin de línea: el hash es de `cat` de los ficheros. `.gitattributes` fuerza `eol=lf` (`* text=auto eol=lf`) y `core.autocrlf=false` aquí, así que en un checkout normal no cambia; un checkout o un zip con CRLF sí lo cambiaría. Sin ejecutar.

## Lo que muestran los logs reales (solo lectura, sin copiarlos)

`pruebas/logs/A.log` y `B.log` (dos instancias en la misma PC): primera línea `Savage Wind 0.6.0-alpha.6 (commit 4588d18, tree d0ba78d, modified)`; `[gamelib] ...swgame.dll (game version 1.9.0, 243 sites)`; `target: -march=x86-64-v3`. Ambos idénticos entre sí. Confirman:
- el log **no imprime `GAMELIB_ID`**: hoy solo se ve en el fichero `swgame.dll.id` (`coop/build/swgame.dll.id` contiene `9bc886f8314dd42b`);
- `modified` aparece en cuanto hay un cambio local, aunque las dos PC compilen el mismo commit.
Los otros logs revisados (`ventana-*`, `host-vivo`, `recorrido`) no tenían esas líneas en `grep`.

## Recomendación para el saludo (T314 / T325)

Comparar con **rechazo** (en este orden, igual que T314):
1. `PROTOCOL` (ya existe).
2. **Versión del modo cooperativo / versión de `rt_version()`**: constante, estable, igual en dos PC con la misma versión.
3. **`gamelib_id()` pero solo si los dos SO y la clase de CPU coinciden**; el cliente debe enviar también `GAMELIB_ARCH`/compat y plataforma (Windows/Linux) para que el servidor sepa si la diferencia es "ruido" (otro SO). Alternativa más limpia, que **no está en el código hoy**: un id nuevo calculado solo de fuentes (el hash sin `GENFLAGS`), aplicable entre SO. Proponerlo al dueño; no lo decido yo.
4. **Versión del juego** (`g_game_version`) y **build id del `main`** (16 hex): ambos iguales por construcción; el segundo es más estricto y no cuesta nada.
5. **Versión de `online_mod`** (43).

Solo **avisar**, no rechazar:
- `SW_SOURCE` (commit/árbol): útil en el log y en el mensaje de rechazo, pero poco fiable para decidir (abreviaturas, `modified`, `unknown`).
- Estado de DLC distinto, `target` (v3 frente a compat) si el `GAMELIB_ID` ya coincide por construcción mixta, SO distinto.

Para el servidor dedicado (`--server`), la versión del juego y el build id valen `""`: T314 debe tratarlos como "no aplicable", no como diferencia.

## NO verificado

- Ninguna orden `make`, ni `print-gamelib-id`, ni `savagewind --server` se ejecutó (ADR-006 y encargo).
- Que Windows y Linux den ids distintos es deducción de `Makefile`; falta pegar las dos salidas.
- Que `--server` arranque sin GPU ni pantalla.
- El efecto del fin de línea CRLF sobre el hash.
- Los números de línea de `gamelib.c` marcados con `~` son aproximados.
- No se leyeron `runtime/online*.c` completos, solo `PROTOCOL` y la ruta del saludo.
