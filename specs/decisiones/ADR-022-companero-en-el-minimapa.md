# ADR-022 · El compañero en el minimapa del juego, dibujado encima

- Estado: aceptada por el dueño (2026-11-25)
- Decide: el dueño del proyecto
- Afecta a: CU-16 (nuevo), CU-10, `fases/F4-funciones-coop/` (elemento O, R4-37, S18, T445-T447)
- **Amplía** ADR-011, decisión 41 (no la contradice ni la reabre): ver "Relación con la decisión 41"

## Contexto

ADR-011, decisión 41 (Q-10-3): "No hay marcador en el mapa del propio juego en F4: flecha en pantalla, distancia y
nombre del mapa. Se estudia después." El motivo de fondo está en CU-10: el mapa es un layout nativo del juego y el
runtime no sabe qué coordenadas del mundo cubre (mapa 04 §4a). Mientras tanto el jugador solo ve a su compañero por
la flecha de borde del overlay y por su lista.

El dueño quiere ver al compañero **en el minimapa** (el círculo del HUD, abajo a la derecha por defecto), que es lo
que un jugador mira para orientarse. Lo que sí existe hoy en el runtime, y es solo lectura:

- `ui_layout_map_mode` (`runtime/ui_layout.c`) distingue, para la versión 1.9.0, si la pantalla del mapa (AppMap)
  está en modo minimapa (0), en modo mapa grande (1) o animando entre los dos (-1).
- El escalado y los bordes del HUD (`runtime/nvn/hud_scale.h`, ajustes `hud_edges` y `hud_size`) mueven el minimapa
  respecto a la pantalla; hay que tenerlos en cuenta al medir.
- El overlay de ImGui ya dibuja marcadores sobre el mundo (CU-10) con la cámara que publica el renderizador.

## Opciones

**A. Capa encima del minimapa (elegida).** El overlay del runtime dibuja una flecha del compañero en la posición del
minimapa que corresponde a su posición en el mundo. No toca el layout del juego ni el guardado. Necesita saber dónde
está el minimapa en pantalla y cómo convierte metros en píxeles: se **mide** (T445) y se programa después (T446).
Riesgo: si la medida falla en algún formato de pantalla, el compañero aparece desplazado; se acota con CA y con
"no dibujar si no se puede fiar".

**B. Mapa grande del menú de pausa: leer desplazamiento y zoom del propio mapa.** Permitiría marcar al compañero en
el mapa grande, que se mueve y se amplía. Exige leer estado interno del layout (desplazamiento, zoom) que hoy no se
conoce: es investigación. **Se aplaza, sin fecha** (T447, prioridad baja).

**C. Pines o marcas reales del juego.** Usar el sistema de marcas del propio juego (pines, sellos) para señalar al
compañero. **Descartada**: esas marcas se guardan en la partida (`game_data.sav`), con lo que el compañero acabaría
escrito en el guardado, que las reglas del proyecto y ADR-007 (decisiones 1 y 5) prohíben tocar. Tampoco hay
mecanismo comprobado para crearlas desde el runtime.

## Decisión

1. Se muestra al compañero en el **minimapa** con la **opción A**: dibujado encima por la capa de interfaz del
   runtime (la misma de los marcadores de CU-10), sin tocar el layout del juego ni el guardado.
2. Aspecto: **flecha pequeña con el color y la forma del compañero** (los 8 colores de CU-10, ADR-009, decisión 26)
   que apunta hacia donde mira.
3. Fuera del alcance del minimapa: **marca pegada al borde** del círculo, en su dirección, **con la distancia al
   lado**.
4. **Medir ya** el minimapa (posición, tamaño, escala metros→píxeles, si gira con la cámara o va con el norte
   arriba, en 16:9 y en ultrawide, y cuándo está oculto o animando) y **programar en F4**, junto a CU-10 (mismos
   ficheros de UI).
5. Mapa grande del menú de pausa (opción B): investigación para después, sin fecha. Opción C: descartada.

## Relación con la decisión 41

La decisión 41 sigue en pie en lo esencial: **no se dibuja nada dentro del mapa nativo** y **el mapa grande queda
fuera de F4**. Este ADR la **amplía** en un punto: el **minimapa sí** muestra al compañero, dibujado **encima** por
el overlay. Lo decidió el dueño el 2026-11-25. Ninguna otra decisión anterior cambia; la flecha de borde de pantalla,
la distancia y el nombre del mapa de R4-35 se mantienen. Si un texto anterior dice "no hay marcador en el minimapa",
vale este ADR para el minimapa y la decisión 41 para el mapa grande.

## Consecuencias

- Nuevo caso de uso CU-16, elemento **O** de F4, regla **R4-37**, criterio de salida **S18**.
- Nuevo módulo puro `runtime/coop_minimap.c/.h` con su prueba (T446); el dibujo va en la capa de UI de CU-10.
- T445 (medición) es la primera tarea y bloquea a T446: si el minimapa no se puede medir de forma fiable, T446 no se
  hace y se anota "no viable" con el motivo.
- Los valores medidos se guardan como **constantes por proporción de pantalla** (o se leen del layout si T445 lo
  permite), nunca como píxeles absolutos.
- El juego solo se lee, como ya hace `ui_layout_map_mode`; no se añaden parches ni sitios (`GAMELIB_ID` no cambia)
  salvo que T445 concluya que son imprescindibles, y entonces se pregunta al dueño.
- Como todo overlay, ImGui se dibuja encima del HUD: la flecha puede tapar un icono del minimapa; se acepta.
