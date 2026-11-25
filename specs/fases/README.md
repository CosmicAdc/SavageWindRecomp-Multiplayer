# Fases · mapa de dependencias y protocolo de relevo

Este fichero cubre F0, F1 y F2 (las fases F3 a F5 las describen sus propias carpetas). Cada carpeta de fase tiene
`spec.md` (qué y por qué), `plan.md` (cómo, con comandos), `tasks.md` (tareas ejecutables una a una) y
`verificacion.md` (lista de cierre y plantilla de bitácora).

Las tareas están escritas para que un agente **sin más contexto** pueda ejecutar una sola. El formato está
explicado en `F0-preparacion/tasks.md`, "Cómo leer una tarea".

## 1. Dependencias entre fases

```
 F0 Preparación ──────────► F1 Probar y arreglar ──────────► F2 Cimientos ──────────► F3, F4 (en paralelo) ─► F5
 (máquina, fork, juego,     (línea base con dos juegos;      (pruebas de protocolo;
  CI, Docker)                9 arreglos pequeños)             partir online_actors.c;
                                                              RT_ON_RESET; validación)
        T024 cierra F0 ──►  T101          T121 cierra F1 ──►  T201           T221 cierra F2
```

Una fase no empieza hasta que el dueño aprueba el cierre de la anterior (T024, T121, T221).

**F3 y F4 "en paralelo" tiene un límite:** F3 es dueña de `runtime/online.c` mientras dure su cadena núcleo
(T309-T319). Las investigaciones de F4 (T401-T403, T407, T408) y su lógica pura (`coop_*.c` con sus pruebas) pueden
empezar durante F3; todo lo que edite `runtime/online.c` (T413 y siguientes) espera al cierre del bloque 3 de F3, y
T409 (protocolo) espera a F3 completa. F3 no se cierra sin la prueba con dos PC por Internet (T341; ADR-011,
decisión 47).

## 2. Dependencias entre tareas

### F0 (25 tareas)

```
 DUEÑO          T001 fork        T002 extraer juego      T003 disco        (los tres en paralelo)
                  │                    │
                  ▼                    │
 REPO           T004 clon y remotos    │
                  │                    │
          ┌───────┼──────────────┐     │
          ▼       ▼              │     │
        T005    T006 coop/main   │     │
        MSYS2   + CI restaurado  │     │
          │       │   │   │      │     │
          │       │   │   └──► T008 CI en coop/main
          │       │   └──────► T009 specs al fork ──► T019 trampas de nombres
          ├──► T007 actualizador (necesita T005 y T006)
          │       │
          │       ├──► T020 [P] compila en Linux ─┐
          │       └──► T021 [P] imagen Docker ────┼──► T022 pruebas en Docker
          │                                       └──► T023 compose con bots
          ├──► T016 [P] pruebas Python sin juego
          ▼                            ▼
        T010 game_dir.txt + sw_probe (necesita T002 y T005)
          │              │
          ▼              ▼
        T011 make gen   T013 online_mod
          ▼              │
        T012 make -j8 ───┼──► T017 pruebas C ──► T018 pruebas con python3
          └──────┬───────┘
                 ▼
               T014 arranque desatendido ──► T015 arranque con ventana (dueño)
                 │
                 ▼
               T024 bitácora y cierre  ──► T025 push al fork (solo si el dueño lo pide)
```

### F1 (21 tareas)

```
 LÍNEA BASE (en serie)
   T101 carpetas ─► T102 dos_juegos.sh ─► T103 CU-01 ─► T104 CU-02 ─► T105 recorrido.py
        ─► T106 pasada 30 min ─► T107 ventana (dueño) ─► T108 lista priorizada
                                                              │
        ┌───────────────┬───────────────────┬────────────────┼──────────────────┐
        ▼               ▼                   ▼                ▼                  ▼
   T109 [P] bot     T110 [P] arnés      carril OA         (carril NET        T119 [P] UI
   hostil           de online.c         T111 versión       empieza en T110)   contraseña
        │               │               T112 has_body          │
        │               ▼               T113 runas             │
        │           T115 floats ◄───────────────────────── (usa T109)
        │           T116 stop_all       T114 nombres ◄──── (usa T109)
        │           T117 SIGPIPE            │
        │           T118 shield             │
        └───────────────┴─────────┬─────────┴──────────────────┘
                                  ▼
                        T120 integrar y pasada final ─► T121 cierre
```

Carril OA = `runtime/online_actors.c` (un dueño, en serie). Carril NET = `runtime/online.c` (un dueño, en serie).

### F2 (21 tareas)

```
 T201 firma de referencia
   │
   ├── Cadena NET (en serie)                    ├── Cadena OA (en serie)
   │   T202 cabecera de cable + bot             │   T206 reordenar en un fichero
   │   T203 pruebas de trama ──► T205 [P] CI    │   T207 cortar en oa_*.c
   │   T204 pruebas de sesión                   │   T208 partir g_act[]
   │   T213 validación única                    │   T209 orden explícito del fotograma
   │                                            │   T210 Links validados al principio
   │                                            │   T211 inventario de estáticas
   │                                            │   T212 RT_ON_RESET
   └───────────────────────┬────────────────────┘
                           ▼
                     T214 puerta de la fase 0
                           │
   ┌──────────┬────────────┼───────────┬────────────┬─────────────┐
   ▼          ▼            ▼           ▼            ▼             ▼
 T215 [P]   T216 [P]     T217 [P]    T218 [P]     T219 [P]      T220 [P]
 mundo      enemigos     runas       online.c     bot+Docker    traducciones
            y botín
   └──────────┴────────────┴─────┬─────┴────────────┴─────────────┘
                                 ▼
                           T221 cierre de F2
```

## 3. Qué se puede hacer en paralelo ahora mismo

"Ahora mismo" = antes de que el dueño confirme F0 y sin nada instalado.

| Tarea | Quién | Por qué puede empezar ya |
|---|---|---|
| T001 fork en GitHub | dueño | no depende de nada |
| T002 extraer el juego | dueño | no depende de nada |
| T003 liberar disco | dueño | no depende de nada |

Nada más: todas las tareas de agente dependen de T001 (el clon) o de la confirmación de F0.

En cuanto exista el fork y se confirme F0:

| Momento | En paralelo | Recurso que los limita |
|---|---|---|
| Tras T004 | T005 (herramientas) y T006 (rama y CI) | ninguno en común |
| Tras T005 y T006 | T007, T008, T009, T016, T020, T021 | T020 y T021 usan Docker (6,2 GB): no a la vez que una compilación nativa grande |
| Mientras compila T012 | T013 (overlay), T016 | CPU y RAM |
| F1 tras T108 | T109, T110, T119, y el carril OA (T111) | la prueba en juego: solo dos juegos a la vez |
| F2 tras T201 | cadena NET y cadena OA | el `Makefile` (bloques al final) y los turnos de juego |
| F2 tras T214 | T215 a T220 | turnos de juego; `online.h` y `online_actors_internal.h` tienen un único coordinador |

Recursos únicos de esta máquina, que ninguna planificación puede duplicar:

- **Dos juegos a la vez como máximo** (15,7 GB de RAM; con Docker y navegador cerrados).
- **Docker y los dos juegos no caben juntos.**
- **Disco**: cada worktree necesita su `build/` y, si no comparte `GAMELIB_ID`, su `generated/`.

## 4. Ficheros con dueño único

| Fichero | Por qué | Quién lo toca |
|---|---|---|
| `runtime/online.h` | contrato entre la capa de red y los actores; cualquier campo nuevo toca el protocolo | el coordinador de la fase; nadie en F1 |
| `runtime/online_wire.h` (desde T202) | formato en el cable | cadena NET |
| `runtime/online_actors_internal.h` (desde T207) | cabecera de todos los módulos `oa_*`; recompila todo | cadena OA; en los carriles, el coordinador |
| `runtime/online_actors.c` (hasta T207) | 6 468 líneas, todo mezclado | carril OA de F1; cadena OA de F2 |
| `runtime/online.c` | servidor, cliente y colas en un fichero | carril NET de F1; cadena NET y T218 en F2 |
| `runtime/ui/i18n/*.inc` | cada texto nuevo añade una línea en los 13 | T220, y nadie más a la vez |
| `Makefile` | reglas de prueba | cualquiera, **solo añadiendo un bloque al final**; integra un dueño |
| `tools/patches.py`, `tools/game_sites.py` | cambian `GAMELIB_ID`: todos recompilan el juego | nadie en F0-F2 |

## 5. Protocolo de relevo: cómo coge una tarea un agente

**Este protocolo está enmendado por [ADR-006](../decisiones/ADR-006-orquestador-y-subagentes.md)** (un orquestador
integra y verifica; los subagentes solo escriben). Donde los pasos de abajo piden ejecutar algo (pasos 7, 8 y 12, y
cualquier "prueba", "mide", "compila" o "regenera" de una tarea), lo ejecuta **el orquestador** (o el dueño), no el
subagente: el subagente **no compila, no ejecuta pruebas, no lanza el juego ni Docker, no descarga ni instala nada y
no hace push**. Solo puede usar comprobaciones de solo lectura baratas (`grep`, `git diff`).

1. **Leer** `specs/constitution.md` (reglas no negociables) y el contrato que toque la tarea en `specs/contratos/`
   (`protocolo.md`, `hilos-y-frame.md`, `autoridad-mundo.md`, `validacion-red.md`). Si alguno no existe todavía,
   decirlo en la bitácora y seguir con las reglas comunes del `tasks.md` de la fase.
2. **Leer la tarea entera** y las "Reglas que valen para todas las tareas" de su `tasks.md`. Si algo de `contexto`
   no coincide con el código (una función que ya no está, una línea que no dice eso), **parar y avisar**: no
   improvisar.
3. **Comprobar dependencias**: todas las tareas de `depende de:` están marcadas `[x]`. Si no, no empezar.
4. **Reclamar la tarea**: cambiar su casilla de `- [ ]` a `- [~]` y añadir debajo una línea
   `  reclamada: <agente o modelo>, <fecha>, rama coop/<tarea>`. Ese cambio va en un commit propio en `coop/main`
   (o se lo pide al coordinador). Una tarea con `[~]` de otro no se toca.
5. **Trabajar en un worktree propio**:
   `git worktree add "$SW/wt/<tarea>" -b coop/<tarea> coop/main`. Preparar lo que el worktree no trae
   (`game_dir.txt`, venv, `generated/`, `build/`, `online_mod/`; F0 `plan.md` §10). Comprobar finales de línea:
   `git config core.autocrlf` debe dar `false`.
6. **Tocar solo los ficheros de `files:`**. Si hace falta otro, parar y pedirlo.
7. **Dejar escrito el `verify`** tal como está en la tarea y marcarlo "NO verificado" (ADR-006): lo ejecuta el
   orquestador, de uno en uno. Si un comando de `verify` no existe o fallaría por una razón ajena (falta `python3`,
   falta Docker), no sustituirlo por otro en silencio: anotarlo para el orquestador.
8. **Pedir turno al orquestador** para cualquier prueba con los dos juegos: el subagente no los lanza.
9. **Antes del commit**: la comprobación de trampas de nombres (`F0-preparacion/plan.md` §8). Un commit por cambio
   verificable, con el número de tarea en el mensaje. Sin `git push`.
10. **Entrada de bitácora** en `specs/bitacora/` con la plantilla de `verificacion.md` de la fase: fecha, commit,
    versión del juego, hardware, ajustes, FPS, qué se observó, logs (fuera del repositorio).
11. **Decir lo que NO se verificó**, de forma explícita y separada: qué se comprobó solo por lectura, qué caminos no
    se probaron, qué quedó "sin reproducir". Una tarea entregada sin esta sección se devuelve.
12. **Entregar**: hacer commit en la rama de la tarea y entregar diff, resumen y la lista de "NO verificado" al
    orquestador. El orquestador integra en `coop/main` (merge `--no-ff`), ejecuta el `verify`, marca `- [x]`, enlaza la
    entrada de bitácora en la línea `reclamada:` y hace el push solo al cerrar la fase. Borrar el worktree solo después de integrar
    (`git worktree remove "$SW/wt/<tarea>"`).

Si el agente no puede terminar: dejar la casilla en `[~]`, escribir en la bitácora hasta dónde llegó, qué probó y
qué falló, y dejar la rama como está. No revertir trabajo ajeno ni dejar cambios sin commit en el worktree.

## 6. Qué modelo para qué tarea

`modelo sugerido` en cada tarea: **alto** cuando hay que razonar sobre concurrencia, memoria del juego, orden de
ejecución o ingeniería inversa; **medio** para código o configuración con criterio; **bajo** para pasos mecánicos.

| Fase | alto | medio | bajo |
|---|---|---|---|
| F0 | — | T007, T014, T021, T023 | el resto (incluidas las tareas del dueño) |
| F1 | T110, T111, T113, T114, T116 | T102, T105, T108, T109, T112, T115, T117, T119, T120 | T101, T103, T104, T106, T107, T118, T121 |
| F2 | T204, T206, T207, T208, T209, T210, T211, T212, T213, T215, T216, T218 | T201, T202, T203, T214, T217, T219, T220, T221 | T205 |

Las tareas **alto** de la cadena OA de F2 conviene que las haga el mismo agente de principio a fin: cada una
hereda el conocimiento de la anterior.
