# ADR-002: El mundo es el del anfitrión; inventario y vida, de cada jugador

- **Estado:** aceptada (plan aprobado)
- **Fecha:** 2026-11-24
- **Afecta a:** F3 (autoridad en el servidor), F4 (CU-06, CU-07, CU-11, CU-12), F5 (CU-14)
- **Contrato que la desarrolla:** `contratos/autoridad-mundo.md`

## Contexto

En el PR #294 cada jugador ejecuta su juego completo con **su propia partida guardada**. Para compartir el progreso,
cada juego envía cada 2 segundos un bit por cada flag de progreso (hasta 32 768); el anfitrión combina los de todos
con una operación **O** y devuelve la unión; cada juego activa lo que le falta
(`runtime/online_actors.c:3851-3997`). Además, ciertos flags entregan objetos a quien no los tenía: orbes,
contenedores de corazón, runas, semillas Kolog, la paravela (`runtime/online_actors.c:3987-3997`).

Consecuencias de ese diseño:

- No hay "mundo de alguien": el resultado es la **unión de lo que haya hecho cualquiera**, escrita en la partida de
  todos.
- Es **irreversible**: un cliente con la partida terminada que se une vuelca todo su progreso (y los objetos
  asociados) en la partida del anfitrión.
- El servidor reenvía los flags de cualquiera sin comprobar nada (`runtime/online.c:291-298`): un cliente defectuoso
  puede marcar todos.

## Decisión

1. **El mundo y el progreso son los del anfitrión.** Su partida es la verdad. El cliente entra en ese mundo; no lo
   actualiza con el progreso que traía.
2. **Cada jugador conserva su inventario, sus corazones y su resistencia.** Nada de eso se copia, se regala ni se
   pierde por jugar en línea.
3. **Lo que ocurre durante la sesión avanza el mundo del anfitrión**, y el anfitrión es quien lo acepta.
4. La autoridad la hace cumplir el **servidor** (que sabe quién es el anfitrión), no la buena fe de cada cliente.

## Alternativas descartadas

| Alternativa | Por qué no |
|---|---|
| Mantener la unión de todos (lo que hay) | Estropea partidas de forma irreversible y no da respuesta a "¿de quién es este mundo?" |
| Mundo propio de cada jugador, solo se ven | No es cooperativo: no se pueden hacer juntos misiones ni santuarios |
| Partida nueva compartida, creada para la sesión | Obliga a empezar de cero y a decidir dónde vive ese guardado; no era lo pedido |
| Inventario y vida también del anfitrión | Contradice el objetivo de que cada uno conserve su personaje; exigiría sincronizar la bolsa entera |
| Servidor dedicado con el mundo | No existe un juego sin ventana que simule el mundo; hoy el servidor dedicado no tiene autoridad alguna (`runtime/online.c:412-423`) |

## Consecuencias

- Hay que **sustituir la unión de flags** por un flujo en el que el anfitrión decide (F4, CU-06).
- Hay que **separar "progreso del mundo" de "recompensa personal"** y quitar los regalos automáticos (F4, CU-07).
- **Quedan preguntas que esta decisión no responde** y que se resolverán en F4 con su propio ADR:
  - ¿Cómo gana su orbe o su semilla el cliente que estaba presente? ¿Y el que no?
  - Runas, pizarra, paravela y habilidades de campeón son a la vez progreso e inventario: ¿de quién son?
  - ¿Qué se guarda en la partida del **cliente** al terminar la sesión, y cómo se evita que su propia partida quede
    en un estado incoherente?
- El servidor debe **saber quién es el anfitrión** (hoy lo supone en la ranura 0) y filtrar por emisor (F3).
- El servidor dedicado (`--server`) deja de ser un modo soportado para el cooperativo mientras no tenga anfitrión.
- Al no usar ya la unión, desaparece la heurística que confunde "cargar una partida antigua" con una Luna Carmesí
  (`runtime/online_actors.c:3913-3924`).
