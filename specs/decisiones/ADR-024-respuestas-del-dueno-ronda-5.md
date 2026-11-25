# ADR-024 · Respuestas del dueño, ronda 5 (CU-15 amenaza y CU-16 minimapa; cierre de F1; autonomía)

- Fecha: 2026-11-25
- Estado: aceptado
- Decide: el dueño, en diálogo con el orquestador

## Decisiones

| # | Pregunta | Respuesta del dueño |
|---|---|---|
| 1 | Cerrar F1 con lo probado y subir `coop/main` al fork | **Sí**: cerrar F1 (T121) con lo pendiente pasado a la lista priorizada y hacer push de `coop/main` (HTTPS, cuenta CosmicAdc, sin `--force`) |
| 2 | Qué puede integrar el orquestador sin el dueño delante | **Todo lo verificado**: refactors y cambios de comportamiento (T208-T212, incluido el arreglo del cierre al recargar) si pasan compilación, pruebas automáticas y pasada de dos juegos sin ventana; lo que solo se prueba jugando queda marcado para la sesión del dueño |
| 3 | Docker Desktop para las pruebas de Linux | **Sí**: el orquestador lo abre, lo usa y lo cierra; nunca a la vez que los dos juegos |
| 4 | Q-15-1 peso de una flecha frente a cuerpo a cuerpo | **Igual: cuenta el daño** |
| 5 | Q-15-2 margen para quitar el enemigo | **1,5 veces** la segunda amenaza |
| 6 | Q-15-3 olvido de la amenaza | **Semivida de 8 s** |
| 7 | Q-15-4 el líder conserva el enemigo | **Sí, hasta 80 m**, aunque el otro esté al lado |
| 8 | Q-16-1 altura del compañero en el minimapa | **No**: solo posición horizontal |
| 9 | Q-16-2 tamaño de la flecha | **Como la flecha de Link** (≈ 3 % del alto) y sigue la escala del HUD |
| 10 | Q-16-6 compañero en pausa o en un menú | **Igual que siempre** (sin atenuar). Cambia el valor por defecto propuesto en CU-16 |
| 11 | Q-16-7 tres o más jugadores | **Una flecha por compañero** |

Q-16-3 (orientación según la medición de T445), Q-16-4 (opción para apagarla, activada por defecto), Q-16-5 (distancia en
metros en la marca de borde) y Q-16-8 (sin lectura de zoom: solo en el zoom por defecto) no se preguntaron: quedan con su
valor por defecto, revisables.

## Consecuencias

- CU-15 y T443: los valores ya implementados en `runtime/coop_threat.c` (T443) coinciden con 4-7; no cambia código.
- CU-16 y T446: la flecha no se atenúa en pausa (difiere de CU-10); el resto, como el valor por defecto.
- F1 se cierra con T121 y se sube al fork al terminar la redacción.
