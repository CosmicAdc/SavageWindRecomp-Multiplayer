# Specs del modo cooperativo de Savage Wind (fork CosmicAdc)

> **Si retomas este trabajo, empieza por [`COMO-CONTINUAR.md`](COMO-CONTINUAR.md):** estado actual, decisiones del dueño, bloqueos y cómo coger una tarea.

Esta carpeta es la **fuente de verdad** del trabajo: qué queremos construir, con qué reglas, en qué orden y cómo se
demuestra que cada cosa funciona. El código se escribe a partir de estos documentos, no al revés. Este método se llama
*spec-driven development* (SDD, desarrollo guiado por especificaciones).

**El proyecto en un párrafo.** Savage Wind Recomp es una recompilación estática de Zelda BotW (Switch, versión 1.9.0):
traduce el ejecutable del juego a C y lo ejecuta en PC sobre un sustituto del sistema operativo de la consola. El
PR #294 "Online mode" (autores byluki y mstampfli) le añade un modo en línea. Nuestro fork construye sobre ese PR un
cooperativo de 2 jugadores (ampliable después). Decisiones tomadas: el mundo y el progreso son los del anfitrión; cada
jugador conserva su inventario, corazones y resistencia; conexión directa (uno hospeda, el otro se conecta a su
máquina); cooperativo por defecto con PvP (jugador contra jugador) opcional; y "cimientos primero". Nada se sube al
proyecto original.

Plan aprobado completo: [`PLAN-APROBADO.md`](PLAN-APROBADO.md).

> **Rutas.** Todos los documentos usan rutas relativas a la raíz del repositorio (por ejemplo `runtime/online.c:140`).
> No escribas rutas personales absolutas (del tipo unidad + carpeta de usuario): el control automático del proyecto
> rechaza cualquier línea que las contenga (`mapa/05-ui-input-build-ci.md` §4.2). El código del PR que describen los
> mapas está, en esta máquina, en la carpeta hermana `pr294-online/` (solo lectura).

---

## 1. Índice

| Ruta | Qué es | Cuándo leerlo |
|---|---|---|
| [`PLAN-APROBADO.md`](PLAN-APROBADO.md) | Decisiones, fases F0-F5 y casos de uso CU-01..CU-14 | Primero |
| [`constitution.md`](constitution.md) | Reglas no negociables, numeradas, cada una con su porqué | Antes de tocar nada |
| [`glosario.md`](glosario.md) | Términos de recompilación y de red en lenguaje llano | Cuando una palabra no se entienda |
| [`mapa/`](mapa/) | 7 mapas del código (01 recompilador, 02 runtime, 03 estado del juego, 04 gráficos, 05 UI/build/CI, 06 red y sesión del PR, 07 integración con el juego del PR) | Antes de trabajar en un área |
| [`casos-de-uso/`](casos-de-uso/) | CU-01..CU-14, uno por archivo: actor, flujo, criterios de aceptación | Para saber qué debe poder hacer el jugador |
| [`contratos/protocolo.md`](contratos/protocolo.md) | El protocolo de red tal como es hoy (versión 38) y los cambios previstos | Antes de tocar `runtime/online.h` u `online.c` |
| [`contratos/hilos-y-frame.md`](contratos/hilos-y-frame.md) | Qué hilo puede tocar qué; orden dentro de `rt_online_frame` | Antes de escribir código que cruce hilos |
| [`contratos/autoridad-mundo.md`](contratos/autoridad-mundo.md) | Quién manda sobre cada tipo de estado, hoy y como debe quedar | Antes de sincronizar algo nuevo |
| [`contratos/validacion-red.md`](contratos/validacion-red.md) | Reglas para todo dato que llega por red y lista de fallos actuales | Antes de leer un campo recibido |
| [`fases/`](fases/) | Una carpeta por fase, con `spec.md`, `plan.md`, `tasks.md`, `verificacion.md` | Para trabajar |
| [`decisiones/`](decisiones/) | ADR (registro de decisión de arquitectura): una decisión por archivo | Para saber por qué algo es así |
| [`bitacora/`](bitacora/) | Resultado de cada prueba real: fecha, versión, hardware, qué se vio | Al terminar cualquier prueba |
| [`aprendizaje/`](aprendizaje/) | Guía desde lo básico (recomp, Ghidra, netcode) con ejercicios sobre este código | En paralelo a todo |
| `governance/` | Registro de auditoría generado por herramientas; no se edita a mano | — |

Decisiones registradas hasta ahora:

| ADR | Decisión | Estado |
|---|---|---|
| [ADR-001](decisiones/ADR-001-construir-sobre-pr294.md) | Construir sobre el PR #294 | Aceptada |
| [ADR-002](decisiones/ADR-002-mundo-del-anfitrion.md) | El mundo es el del anfitrión; inventario y vida, de cada jugador | Aceptada |
| [ADR-003](decisiones/ADR-003-conexion-directa.md) | Conexión directa, sin servidor intermedio | Aceptada |
| [ADR-004](decisiones/ADR-004-cimientos-primero.md) | Cimientos primero | Aceptada |
| [ADR-005](decisiones/ADR-005-tcp-vs-udp.md) | TCP frente a UDP | Pendiente (F3) |

---

## 2. Cómo se trabaja aquí: spec → plan → tasks → verificación → bitácora

Cada fase vive en `fases/F<n>-<nombre>/` y pasa por cinco pasos, siempre en este orden:

1. **`spec.md` — el qué y el porqué.** Qué problema resuelve la fase, qué casos de uso cubre (CU-xx), qué queda
   fuera y cuáles son sus criterios de salida. No habla de archivos ni de funciones.
2. **`plan.md` — el cómo.** Diseño técnico: qué archivos se tocan, qué contratos de `contratos/` aplican o cambian,
   qué decisiones hacen falta (y su ADR), riesgos y orden de trabajo. Si el plan contradice la constitución o un
   contrato, **gana el contrato**; para cambiarlo se abre un ADR antes.
3. **`tasks.md` — los pasos.** Lista de tareas pequeñas con el formato de la sección 3. Aquí se decide qué puede ir
   en paralelo.
4. **`verificacion.md` — la demostración.** Cómo se comprueba la fase entera: comandos, recorrido de prueba en el
   juego y qué resultado se espera.
5. **`bitacora/` — lo que pasó de verdad.** Una entrada por prueba real: fecha, commit, hardware, qué se hizo y qué
   se vio. Una tarea solo se da por hecha cuando hay una entrada que lo respalda (regla 11 de la constitución).

Si al implementar se descubre que la spec estaba mal, **se corrige la spec primero** y después el código. Una spec
desactualizada es peor que no tener spec.

---

## 3. Convención de `tasks.md`

Cada tarea ocupa una línea de casilla más dos líneas obligatorias:

```markdown
- [ ] T012 [P] [red] Rechazar números no finitos en STATE al recibir
  files: runtime/online.c, tools/test_online_proto.c
  verify: make test-online-proto
```

| Parte | Significado |
|---|---|
| `- [ ]` / `- [x]` | Pendiente / hecha (hecha = con evidencia en `bitacora/`) |
| `T###` | Identificador único dentro de la fase; no se reutiliza ni se renumera |
| `[P]` | Opcional. La tarea **puede** ejecutarse en paralelo con otras marcadas `[P]` |
| `[área]` | Zona del código: `red`, `juego`, `ui`, `build`, `pruebas`, `docs`, `i18n`… |
| descripción | Una frase con verbo: qué queda distinto al terminar |
| `files:` | **Todos** los archivos que la tarea puede tocar. Un agente no toca ningún otro |
| `verify:` | Comando (o recorrido de prueba concreto) que demuestra que funciona |

Reglas:

- **Sin `verify:` una tarea no se ejecuta en paralelo.** Aunque lleve `[P]`. Sin una comprobación automática no hay
  forma de saber si dos trabajos simultáneos se han roto el uno al otro.
- Dos tareas `[P]` no pueden compartir ningún archivo de `files:`. Si lo comparten, van en serie.
- Los archivos con dueño único (`runtime/online.h`, `runtime/ui/menu.mm`, `runtime/settings.*`,
  `runtime/ui/i18n/*.inc`, `tools/patches.py`, `tools/game_sites.py`) solo los toca la tarea del dueño; las demás
  le entregan lo que necesitan (constitución, reglas 8, 9 y 15).
- Una tarea que cambie el protocolo de red debe citar la sección de `contratos/protocolo.md` que modifica.

---

## 4. Estado de las fases

| Fase | Nombre | Casos de uso | Modo de trabajo | Estado |
|---|---|---|---|---|
| F0 | Preparación | — | En serie | Specs en redacción; sin empezar (espera confirmación del propietario) |
| F1 | Probar el sistema y arreglos rápidos | CU-01, CU-02, CU-03 | En serie | No iniciada |
| F2 | Cimientos | CU-03 | Primero en serie, luego en paralelo | No iniciada |
| F3 | Red robusta | CU-04, CU-05 | En paralelo | No iniciada |
| F4 | Funciones cooperativas | CU-06 a CU-12 | En paralelo, un worktree por tarea | No iniciada |
| F5 | Funciones difíciles | CU-13, CU-14 | Una a una | No iniciada |

*Última actualización: 2026-11-24.* Quien cierre una fase actualiza esta tabla y enlaza la entrada de `bitacora/`.

Hasta hoy **nada se ha compilado ni ejecutado**: todo lo que dicen los mapas y los contratos sale de leer el código.
Lo que no se pudo comprobar leyendo va marcado `[sin verificar]`.

---

## 5. Cómo empezar (para alguien nuevo)

1. Lee [`PLAN-APROBADO.md`](PLAN-APROBADO.md) (10 minutos): decisiones, fases y casos de uso.
2. Lee [`constitution.md`](constitution.md) entera. Son las reglas que no se discuten en una revisión de código.
3. Ten abierto [`glosario.md`](glosario.md); no hace falta leerlo de corrido.
4. Lee los dos mapas del PR: [`mapa/06-pr294-red-y-sesion.md`](mapa/06-pr294-red-y-sesion.md) (la red) y
   [`mapa/07-pr294-integracion-juego.md`](mapa/07-pr294-integracion-juego.md) (cómo se pone al otro jugador en el
   mundo). Después, [`mapa/02-runtime-so.md`](mapa/02-runtime-so.md) para entender hilos y reinicio.
5. Lee los cuatro contratos de [`contratos/`](contratos/). Son cortos y dicen qué puede y qué no puede hacer el
   código nuevo.
6. Abre la fase en curso (tabla de la sección 4), lee su `spec.md` y elige una tarea de `tasks.md` que no esté
   cogida.
7. Si eres nuevo en recompilación o en redes de juegos, sigue la ruta de [`aprendizaje/`](aprendizaje/); cada fase
   trae lecturas cortas y un ejercicio sobre este código.
8. Al terminar una prueba, escribe la entrada en [`bitacora/`](bitacora/) antes de marcar la casilla.

Si algo de estos documentos contradice al código, **el código manda sobre cómo son las cosas hoy** y los documentos
mandan sobre cómo deben quedar. Anota la contradicción y corrige el documento.
