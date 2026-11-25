# ADR-004: Cimientos primero

- **Estado:** aceptada (plan aprobado)
- **Fecha:** 2026-11-24
- **Afecta a:** el orden de F0 a F5

## Contexto

El PR #294 hace muchas cosas y, según su documentación, funciona entre dos juegos. Pero leído el código:

- **No hay ninguna prueba automática** del modo en línea, y la rama ni siquiera ejecuta las del proyecto.
- **Cinco defectos de gravedad alta** que pueden colgar el juego en uso normal: sin puerta de versión; llamadas a la
  física sobre actores sin cuerpo (`runtime/online_actors.c:4928`); escritura de vida en actores elegidos por un dato
  recibido (`:5178`); creación de actores con nombres recibidos sin lista de permitidos (`:3604`, `:2226`); uso del
  Link remoto antes de comprobar que sigue vivo (`:5976` frente a `:6036`).
- **Un cliente que deja de leer congela la sesión de todos** (`runtime/online.c:140-148`).
- **El estado sobrevive entre sesiones y entre reinicios** (`runtime/online.c:1322-1335`; ningún `RT_ON_RESET`).
- **Un archivo de 6468 líneas** con una estructura por jugador (`g_act[]`) que comparten cinco áreas. Dos agentes
  trabajando ahí a la vez se pisan.
- Nosotros aún **no hemos compilado ni ejecutado nada**.

Queremos trabajar con varios agentes en paralelo. Eso solo es seguro si hay pruebas que digan cuándo algo se rompe y
si el código está partido de forma que dos tareas no toquen lo mismo.

## Decisión

**Primero los cimientos, después las funciones**, en este orden:

1. **F0** — que compile y arranque en solitario.
2. **F1** — probar lo que hay con dos juegos y aplicar solo los arreglos rápidos y esenciales, cada uno con su
   reproducción antes y después.
3. **F2** — pruebas automáticas del protocolo sin el juego; partir `online_actors.c` en módulos **sin cambiar
   comportamiento**; `RT_ON_RESET`; validación única de entradas.
4. **F3** — red robusta: envío no bloqueante, autoridad en el servidor, reconexión, versión estricta.
5. **F4** y **F5** — las funciones nuevas (CU-06 a CU-14).

Regla que la acompaña: un arreglo cuenta como hecho cuando se ha **visto funcionar en el juego**, no cuando compila.

## Alternativas descartadas

| Alternativa | Por qué no |
|---|---|
| Empezar por las funciones visibles (teletransporte, marcadores, revivir) | Se construirían sobre defectos que cuelgan el juego y sobre un archivo que no admite trabajo en paralelo; cada función nueva añadiría campos a un protocolo sin pruebas |
| Reescribir la capa de red antes de probar nada | No sabemos aún qué falla en la práctica; F1 existe para medirlo |
| Arreglar y añadir a la vez | Cuando algo falle no se sabrá si es el arreglo o la función; incumple "un cambio por PR" |
| Refactorizar todo antes de probar | Sin una línea base (F1) no hay con qué comparar que el comportamiento no ha cambiado |

## Consecuencias

- **Las funciones que se ven tardan más en llegar.** F1 a F3 casi no cambian lo que el jugador nota, salvo que deja
  de colgarse.
- **F2 arranca en serie** (un solo agente parte el archivo y fija las interfaces) y solo después se paraleliza.
- **F4 puede ir en paralelo**, un worktree por tarea, porque para entonces hay pruebas y archivos con dueño.
- La partición de F2 debe ser **verificable como "sin cambio de comportamiento"**: mismo recorrido de F1, mismo
  resultado.
- Los cambios que obligan a regenerar el juego (`tools/patches.py`, `tools/game_sites.py`) se agrupan: cada uno
  cuesta unos 15 minutos a cada jugador.
- Riesgo asumido: que el proyecto original cambie mucho mientras tanto. Se mitiga rebasando a menudo (ADR-001).
