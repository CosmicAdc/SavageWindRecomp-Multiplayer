# ADR-012 · Los enemigos atacan a los dos jugadores; "amenaza" en F4

- Estado: aceptada por el dueño (2026-11-24)
- Afecta a: F1 (T104, paso 5), F4 (caso de uso nuevo CU-15, pendiente de escribir), `contratos/autoridad-mundo.md`

## Contexto

El dueño preguntó si los enemigos van también a por el invitado y no solo a por el anfitrión ("si no, sería
completamente aburrido").

Comportamiento del PR #294 (leído en `docs/ONLINE.md`, apartados "Owners" y "Their attacks on the others"; nivel
**hipótesis** hasta verlo en la línea base de F1):

- Los enemigos son del mundo del anfitrión, pero cada uno lo **posee** el juego del jugador más cercano, que ejecuta su
  IA: lo ve, lo persigue y le golpea. Cambia de dueño cuando el actual está a 40 m o más y otro está 10 m más cerca, o
  cuando otro lleva 5 s 15 m más cerca; después no cambia en 10 s. Nunca pasa a un jugador a más de 80 m.
- Sus ataques alcanzan a **cualquier** Link que esté a su alcance y delante, sea o no su dueño, salvo si se cubre.
- Los golpes de cualquier jugador a cualquier enemigo cuentan.
- Límite: un enemigo solo "piensa" en su dueño (el más cercano). No se vuelve hacia quien le ataca desde lejos.

## Decisión del dueño

1. **F1:** comprobar explícitamente en la línea base que los enemigos atacan a los dos (T104, paso 5: cambio de dueño,
   persecución del invitado y golpes al que no es dueño). Un fallo entra en la lista priorizada de T108.
2. **F4:** caso de uso nuevo de **amenaza**: el enemigo se vuelve hacia quien le hace más daño, no solo hacia el más
   cercano (uno distrae y el otro dispara). Afecta a la elección de dueño de cada enemigo y al protocolo (regla 15:
   dueño único del protocolo, sube `PROTOCOL`). Modelo sugerido: **alto**.

## Consecuencias

- La regla de dueño "el más cercano" pasa a ser "el de más amenaza, con el más cercano como desempate", sin perder las
  protecciones actuales (80 m, histéresis de 10 s) que evitan que un enemigo cambie de juego sin parar.
- El anfitrión sigue decidiendo el dueño (regla 11): la amenaza la calcula el juego del anfitrión con los golpes que
  ya recibe (HIT), no lo que diga cada cliente.
