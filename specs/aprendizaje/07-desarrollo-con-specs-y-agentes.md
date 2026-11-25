# 07 · Desarrollo guiado por especificaciones, con agentes en paralelo

Objetivo: entender cómo se organiza el trabajo en este fork para que varios agentes de IA (y tú) avancen a la
vez sin romperse unos a otros, y cuál es el listón de calidad que responde a la crítica contra los recomps
hechos con IA.

Fondo teórico y fuentes: `../../GUIA-EXPERTO-RECOMP-Y-SDD.md`, parte 3. Aquí va lo aplicado.

---

## 1. La idea en una frase

> Un agente solo puede trabajar en paralelo sobre algo que tiene **su propio verificador**.

Todo lo demás (carpetas, formatos, worktrees) existe para que esa frase se cumpla.

## 2. El ciclo: spec → plan → tasks → verificación → bitácora

Cada fase tiene su carpeta en `../fases/F*/` con cuatro ficheros (`../PLAN-APROBADO.md`, estructura):

| Fichero | Contesta a | Quién lo escribe | Cuándo cambia |
|---|---|---|---|
| `spec.md` | **Qué** y **por qué**: comportamiento visible, criterios de aceptación, qué queda fuera | tú, con ayuda | cuando cambia lo que se quiere |
| `plan.md` | **Cómo**: ficheros, contratos que toca, riesgos, orden | orquestador | antes de repartir |
| `tasks.md` | Unidades de trabajo, cada una con dueño de ficheros y prueba | orquestador | al repartir |
| `verificacion.md` | Qué comandos y qué recorrido en el juego demuestran que la fase está hecha | antes de implementar | casi nunca |

Y dos carpetas transversales:

- `../contratos/`: lo que no puede cambiar sin coordinación (protocolo, hilos y orden del fotograma, autoridad
  del mundo, validación de red).
- `../bitacora/`: **resultados de pruebas reales**. Fecha, versión, hardware, qué se hizo, qué se vio. No
  opiniones.
- `../decisiones/`: ADR, una decisión por fichero con su porqué (por ejemplo, TCP o UDP).

Por encima de todo, `../constitution.md`: las reglas no negociables. Las del proyecto original ya existen y se
heredan (`AGENTS.md:21-68`): nada de Nintendo en commits, no copiar de decomps, no editar `generated/`, sitios
por patrón con aserción, `RT_ON_RESET`, semántica de coma flotante intacta.

### El formato de una tarea

```markdown
- [ ] T1-04 [P] Rechazar NaN/infinito en los números que llegan por red
      files:  runtime/online.c
      verify: make test-online-proto && build/online_bot … (ver verificacion.md §3)
```

- `[P]`: se puede hacer en paralelo con otras.
- `files:`: los ficheros que toca. **Dos tareas que comparten un fichero no son paralelas.**
- `verify:`: el comando que demuestra que funciona. **Sin `verify:`, la tarea no se ejecuta en paralelo.**

(Los nombres de objetivo del ejemplo son ilustrativos: esas pruebas se crean en F2.)

## 3. Git worktrees

Un **worktree** es una segunda carpeta de trabajo del mismo repositorio, con otra rama extraída. Varios
worktrees comparten el mismo `.git`, así que no clonas varias veces.

```
git worktree add ../sw-t1-04 -b coop/t1-04 coop/main
git worktree list
git worktree remove ../sw-t1-04
```

Por qué uno por tarea: cada agente edita, compila y prueba en su carpeta sin pisar a los demás. Claude Code los
crea solo con `claude --worktree <nombre>`, y un subagente con `isolation: worktree` recibe siempre el suyo
(verificado en la guía experta, parte 3).

Tres trampas específicas de este proyecto:

1. **El código generado es enorme.** `generated/` y `build/` no se comparten entre worktrees. Regenerar en cada
   uno cuesta minutos y gigabytes (el plan estima ~15 min y avisa de que el disco va justo). Regla práctica:
   **un solo worktree "con juego"** para probar; los demás se validan con pruebas que no necesitan el juego, y
   su cambio se prueba en el juego al integrar.
2. **Los ficheros del juego no están en git.** Un worktree nuevo no tiene `extracted/`. Se apunta a la carpeta
   común con `game_dir.txt` o `SAVAGEWIND_GAME_DIR` (`Makefile:336-337`), nunca copiando.
3. **Los worktrees no evitan conflictos**: los aplazan hasta la fusión. Por eso existe `files:`.

## 4. Qué puede ir en paralelo y qué debe ir en serie

**En serie, un solo dueño (el orquestador):**

| Qué | Por qué |
|---|---|
| `runtime/online.h` y la versión de protocolo | es el contrato entre red, juego y bot; un campo nuevo toca cinco sitios (módulo 05 §5) |
| `tools/patches.py`, `tools/game_sites.py` | cambian el identificador de la biblioteca: los dos jugadores regeneran (módulo 02 §6). Se **agrupan** |
| `runtime/cpu.h`, `runtime/gamelib.h` | ídem, y son contratos del recompilador |
| El orden dentro de `rt_online_frame` | el orden es comportamiento; un fallo conocido nace de él (mapa 07 §6.1 A5) |
| Partir `online_actors.c` (F2) | hasta que no esté partido, todo el mundo toca el mismo fichero de 6.468 líneas |
| `ui/menu.mm`, `settings.*`, `ui/i18n/*.inc` | puntos calientes de conflicto, también con upstream (mapa 05 §7.1) |
| La integración | se fusiona de una en una: rebase, suite completa, siguiente |

**En paralelo** (cuando lo anterior está estable): módulos ya separados con su prueba, el overlay 2D,
`tools/online_mod.py` (su contrato son ficheros `.txt` y nombres `Online*`), las pruebas de protocolo, el bot,
la imagen de Docker, la documentación.

**Un problema monolítico no se paraleliza.** "El juego se cuelga a veces" no se reparte entre seis agentes:
primero alguien lo biseca hasta tener piezas independientes (módulo 06 §6).

**Cuántos a la vez.** De 3 a 6. El cuello de botella es tu revisión, no la capacidad de escribir código.

### Seguridad al lanzar agentes

- Cada agente recibe: la constitución, el contrato de su carril, **una** tarea, sus ficheros y su `verify:`.
- Debe entregar: el diff, la salida del `verify:` y **lo que no verificó**.
- Nunca recibe ni produce: código del juego, salida de Ghidra, rutas personales, claves.
- No puede: publicar, fusionar, cerrar issues, cambiar contratos, ni tocar ficheros fuera de `files:`
  (`AGENTS.md:154-155` lo dice para el proyecto original: abrir un PR no autoriza a fusionarlo).
- Un agente que no consigue pasar su `verify:` **para e informa**. No rebaja la prueba.

## 5. Cómo revisar C de bajo nivel escrito por IA

El modelo escribe C plausible con soltura. Los fallos típicos no son de sintaxis: son de **suposiciones**. Lista
de revisión, con ejemplos reales de este código:

**Memoria y límites**

- [ ] Todo `memcpy` con tamaño que viene de fuera tiene su comprobación antes. Modelo: `online.c:1011-1012`.
- [ ] Toda cadena recibida se termina en cero antes de usarse. Modelo: `online.c:1052`; contraejemplo, el
      `shield` olvidado en `online.c:1095`.
- [ ] Todo índice recibido se compara con el tamaño del array. Modelo: `online.c:1002`.
- [ ] Los números recibidos se validan (rango, `isfinite`, signo). Hoy falta en `online.c:907-964`.

**Punteros del juego**

- [ ] Ningún puntero a memoria del invitado se guarda entre fotogramas sin una forma de saber si sigue vivo.
      Modelo: `Watch` + `alive()` (`online_actors.c:84-94`).
- [ ] Estado por arranque limpiado con `RT_ON_RESET` (`runtime/hle.h:40`).
- [ ] Antes de desreferenciar, comprobación de rango (`VALID`, `online_actors.c:63`) **y** de tipo: que un
      campo exista en una clase no significa que exista en otra (`online_actors.c:67-72`).
- [ ] Toda dirección del juego llega por `g_site.<nombre>` y se comprueba que no es 0; nada de literales nuevos.

**Hilos**

- [ ] ¿En qué hilo corre cada función nueva? Debe estar escrito en un comentario.
- [ ] Ninguna llamada al juego fuera del hilo del juego.
- [ ] Nada bloqueante (socket, fichero, `pthread_join`) en el hilo del juego ni en el de interfaz. Contraejemplo:
      `stop_all` hace `pthread_join` desde la interfaz (`online.c:1322-1329`).
- [ ] Variables compartidas: con cerrojo o atómicas. `volatile` no basta.
- [ ] Variables `static` dentro de funciones: ¿sobreviven a la sesión cuando no deberían? (mapa 06, T-3).

**Recompilador**

- [ ] El parche afirma con `assert` la instrucción que sustituye y reproduce su efecto.
- [ ] El parche no hace nada cuando la función está apagada (patrón de `rt_online_has_fairy`,
      `online_actors.c:2826`).
- [ ] No se ha tocado ningún flag de optimización ni de coma flotante "de paso" (`AGENTS.md:46-47`).

**Honestidad del cambio**

- [ ] ¿Arregla la causa o tapa el síntoma? Señales de tapar: un `sleep`, un reintento sin explicación, un
      `if (ptr == NULL) return;` sin decir por qué puede ser nulo, un tiempo de espera ampliado.
- [ ] ¿El comentario explica el **porqué** (qué hace el juego, qué bug lo demostró)? Es el estilo del repo
      (`CONTRIBUTING.md:110-111`).
- [ ] ¿Sabes explicar cada línea? Si no, no se fusiona. Pídele al agente que lo explique y contrasta con el
      código.
- [ ] ¿Se ha visto funcionar en el juego? Compilar no demuestra nada.

Dos técnicas baratas: una **segunda revisión por otro agente sin contexto** (dale el diff, no tu conclusión), y
**leer el diff tú** en los cambios de protocolo, hilos y parches, siempre.

## 6. La polémica de los recomps hechos con IA

**Qué pasó.** En 2026 salieron muchos recomps escritos mayoritariamente con IA. Los autores de los proyectos de
referencia de N64 los criticaron con dureza y acuñaron "slopcomp"; su postura es no usar IA en sus proyectos.
Lo recoge Kotaku (enlace en `recursos.md`) y lo resume la guía experta, parte 1.

**La crítica técnica, que es la que importa:**

1. Ignoran el sistema de parches del ecosistema.
2. Parchean el código recompilado y la memoria "desde el lado equivocado".
3. El resultado es inmantenible para quien quiera hacer mods y no aporta al conocimiento común.
4. No hay pruebas, y los autores no saben explicar su propio código.

**Dónde está este proyecto.** Savage Wind declara abiertamente que está hecho casi por entero con IA, dirigida,
probada y revisada por una persona (`README.md:33-35`; `CONTRIBUTING.md:5-8`). Tiene sistema de parches con
aserción, sitios por patrón y pruebas diferenciales del lifter. El PR #294, en cambio, añade 62 direcciones
fijas, escribe en memoria del juego en varios puntos y no trae ninguna prueba automática (mapas 06 y 07). O
sea: la crítica **aplica en parte a lo que heredamos**, y el plan de F1-F2 es precisamente pagar esa deuda.

**El listón concreto que responde a la crítica** (es la constitución, dicha en positivo):

| Crítica | Nuestra respuesta comprobable |
|---|---|
| "No usan el sistema de parches" | Todo enganche nuevo es un sitio por patrón con parche afirmado; cero direcciones literales nuevas; puerta de versión para las heredadas |
| "Parchean la memoria desde el lado equivocado" | Se usa la función del propio juego cuando existe (como `set_bool_flag`, `online_actors.c:3794-3804`); escribir en memoria es el último recurso, comentado y con su comprobación de tipo |
| "No hay pruebas" | Pruebas de protocolo sin juego en cada cambio; repro antes/después en la bitácora para cada arreglo |
| "No saben explicarlo" | Esta carpeta. Nadie fusiona lo que no sabe explicar |
| "Inmantenible" | Módulos pequeños con dueño, orden del fotograma explícito, contratos escritos, función apagada por defecto e inocua fuera de sesión |
| "Uso de IA oculto" | Declarado, como hace el proyecto original |

No se trata de convencer a quien rechaza la IA por principio. Se trata de que alguien competente abra el fork,
lea un módulo y encuentre código que su autor entiende y puede defender.

## 7. Autoevaluación

1. ¿Por qué una tarea sin `verify:` no puede ir en paralelo?
2. Dos tareas tocan `runtime/online.h`. ¿Qué haces?
3. Un agente entrega un arreglo que añade `if (!link) return;`. ¿Qué le preguntas?
4. ¿Por qué no conviene un worktree con juego compilado por agente?
5. Nombra dos puntos donde la crítica de los "slopcomps" acierta con el PR #294.

Respuestas: (1) nadie puede saber si terminó ni si rompió algo sin revisarlo a mano. (2) En serie, con un solo
dueño. (3) Por qué puede ser nulo ahí, qué lo demuestra y si eso tapa un uso de un actor ya borrado.
(4) Minutos y gigabytes por cada uno; el disco va justo. (5) Direcciones fijas sin patrón ni puerta de versión;
ninguna prueba automática.

Siguiente: [08 · Contribuir a la comunidad](08-contribuir-a-la-comunidad.md).
