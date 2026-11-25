# CU-13 · Color en el modelo 3D del segundo Link

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

El Link del compañero se ve teñido de su color en el mundo 3D, no solo en el nombre y el marcador. No existe en el
PR. El renderizador ya tiene un gancho de tinte por dibujo sin usar; lo difícil es saber qué dibujos pertenecen al
Link remoto. Es un caso de investigación: primero un experimento corto, después la decisión.

## Actores

- **Jugador local**: ve al compañero teñido.
- **Compañero**: su Link remoto (`OnlinePlayer` + `OnlineArmor_*` + `OnlineGear_*`) es el que se tiñe.

## Precondiciones

- CU-02 funciona (el Link remoto existe como actor).
- CU-10 hecho (cada jugador tiene un color asignado y conocido por los dos juegos).
- Renderizador Vulkan (el camino analizado en mapa 04).

## Flujo principal

1. El jugador entra en una sesión. Cada jugador tiene un color (CU-10).
2. El Link del compañero aparece en el mundo con un tinte de ese color sobre el cuerpo y la ropa.
3. El tinte se mantiene con cualquier armadura, de día y de noche, cerca y lejos.
4. El Link propio se ve normal.
5. En Opciones se puede bajar la intensidad del tinte o apagarlo.

## Flujos alternativos y de error

- **El tinte no se puede aplicar** (otro backend gráfico, grupo de sitios ausente, experimento negativo): el
  compañero se ve sin tinte y queda la distinción 2D de CU-10. Nunca se cuelga por esto.
- **Compañero en otro mapa, cargando o lejos**: no hay Link remoto, no hay nada que teñir.
- **Compañero cambia de armadura**: las piezas nuevas también salen teñidas.
- **Modos de visión de color activados**: el tinte pasa por la misma corrección de imagen final; debe seguir
  distinguiéndose (o apoyarse en la forma del marcador de CU-10).
- **Efectos del juego sobre el modelo** (brillo de Magnesis o Stasis sobre un jugador, daño): tienen prioridad.
- **Desconexión**: el Link remoto se borra (OA:5733-5746) y con él el tinte.
- **Reinicio a la pantalla de inicio**: el estado del tinte se limpia (`RT_ON_RESET`).

## Estado actual en el PR: **FALTA**

- El PR no cambia nada del renderizador para colorear jugadores. Sus dos cambios en `exec.c` son la cámara para el
  overlay y la imagen viva tras los menús (mapa 07 §4).
- Los 8 colores por sitio solo existen en el overlay 2D (`k_colors`, UI:20-23).
- Lo más parecido en el lado del juego (mapa 07 §4):
  - `FN_SPECIAL_FX_B` (OA:1458) y `FN_SPECIAL_FX_A_UNIT` (OA:4011): parámetros de material `uk_special_effect_B / A`
    (brillo dorado de Magnesis y Stasis, pulso rojo de Stasis). Son escalares del propio juego, no un color elegible.
  - Tinte de armadura `ArmorDyeColor` (OA:6069-6077): es el índice de tinte del juego, y no está comprobado que se
    vea en las piezas remotas (DOC:1003-1008).
  - `model_visible()` (OA:356-369): solo muestra u oculta materiales.
- En el renderizador de la base (mapa 04 §4b), **cableado pero inactivo**:
  - El traductor de sombreadores emite en todo sombreador de fragmentos cuyo destino 0 es de coma flotante un bloque
    `uniform Tint { vec4 ltint[2]; }` en el **binding 19** (`runtime/nvn/maxwell/translate.c:2310`) y mezcla el
    color final hacia el tinte con peso `ltint[0].w` (`translate.c:2424-2425`).
  - El renderizador lo enlaza en cada dibujo (`stage_set`, `runtime/nvn/vulkan/exec.c:7487`), pero el valor es
    siempre cero: `float tint[8] = {0};` (`exec.c:9161`), pasado a `stage_set_reuse` (`exec.c:9309`). Con peso 0 el
    sombreador no cambia nada.
- **Lo que no existe**: ninguna forma de saber qué dibujos son del Link remoto. El renderizador no relaciona dibujos
  con actores (mapa 03 fila 9; mapa 04 §6 riesgo 3).

## Qué hay que construir

**Paso 0 — experimento (1-2 días, sin tocar el protocolo).** Responde a dos preguntas antes de diseñar nada:

- *¿El tinte del binding 19 da un cambio de color limpio en personajes?* Poner temporalmente un tinte no nulo para
  **todos** los dibujos tras una variable de entorno de prueba (en `exec.c:9161`) y mirar una captura. Duda conocida:
  el tinte solo afecta al destino de color 0, y la documentación del proyecto dice que el albedo del G-buffer es el
  destino 1 (`docs/SAVAGE-WIND.md:1603`; mapa 04 §4b); qué hay en el destino 0 está **[sin verificar]**.
  Herramientas: `SAVAGEWIND_DEBUG_PICK`, `SAVAGEWIND_DEBUG_FRAME` (mapa 04 §4b).
- *¿Se pueden distinguir los dibujos del Link remoto?* Probar las tres vías de mapa 04 §4b, en este orden:
  1. **Por los datos del dibujo**: `runtime/nvn/vulkan/mvec.c` ya construye una identidad por dibujo y sabe que la
     matriz de mundo 3×4 del objeto está en el slot 4 de constantes (`mvec.c:93-95`). Comparar su traslación con la
     posición conocida del Link remoto (la publica el juego; el PR ya pasa datos juego → renderizador en sentido
     contrario con `online_camera_set`). Duda: los modelos con huesos pueden no usar una única matriz de mundo
     **[sin verificar]**.
  2. **Recursos distintos**: el Link remoto ya es un actor aparte fabricado por `tools/online_mod.py`
     (`OnlinePlayer`, `OnlineArmor_*`). Si el generador le diera un modelo o texturas propios (copias generadas en el
     PC del usuario a partir de sus ficheros, como ya hace con los packs), sus recursos tendrían direcciones
     distintas y se podrían reconocer, o venir ya coloreados. Coste de memoria, tamaños RSTB y viabilidad
     **[sin verificar]**; el RSTB es el punto frágil del generador (mapa 07 §1.3).
  3. **Ámbito en la lista de comandos**: un registro solo-host alrededor del dibujo del actor, como `CMD_UI_SCOPE`
     (`runtime/nvn/cmdbuf.c:368`). Necesita que exista una función del juego "dibuja este actor" que envuelva todos
     sus dibujos: **[sin verificar]**, requiere Ghidra.
- Resultado del paso 0: una nota en `bitacora/` con capturas y un ADR en `decisiones/` que elige vía o descarta.

**Paso 1 — implementación (solo si el paso 0 es positivo).**

1. Interfaz mínima juego → renderizador: "posiciones (o identidades) de los Links remotos y su color", publicada por
   el hilo del juego y leída por el hilo de render con cerrojo o instantánea atómica, igual que la cámara en sentido
   contrario (`online_camera_set`, `exec.c:10999-11005`).
2. En `exec.c`, un gancho **mínimo y condicionado** que elige el tinte por dibujo en lugar del cero de `exec.c:9161`.
   El contenido idéntico se deduplica (`DEDUPE_TINT`, `exec.c:7487`), así que el coste es bajo.
3. Cubrir cuerpo, armadura y equipo (son actores distintos: `OnlinePlayer`, `OnlineArmor_*`, `OnlineGear_*`).
4. Ajuste de intensidad y apagado; `RT_ON_RESET`.
5. Si hay que tocar el traductor (teñir otro destino), es un cambio grande: cambia el identificador del traductor e
   **invalida las cachés de sombreadores de todos los usuarios** (mapa 04). Decisión aparte, con ADR.

**Alternativa por el lado del juego**: encontrar un parámetro de material de color en el modelo (más allá de
`uk_special_effect_A/B`) o hacer visible `ArmorDyeColor`. Requiere Ghidra; el propio PR dice que, si el tinte de
armadura no se ve, "the colour has to go on its materials" (DOC:1007-1008).

## Reglas y decisiones de diseño

- **[DUEÑO] D2**: cada jugador es un Link completo; solo cambia el color.
- **[PROPUESTA]** En cada pantalla se tiñe solo al compañero; el Link propio se ve normal. (Teñir al Link local es
  más difícil: sus dibujos son los del jugador real y no hay actor aparte.)
- **[PROPUESTA]** Tinte suave (se sigue viendo la ropa), con intensidad ajustable.
- **[PROPUESTA]** Si el paso 0 es negativo, CU-13 se aparca y D2 se cumple con el color 2D de CU-10 hasta nuevo
  aviso.
- **[PROPUESTA]** No se toca el traductor de sombreadores sin un ADR aprobado por el dueño.

## Criterios de aceptación

- **CA-0** (paso 0) Existe una entrada en `bitacora/` con: capturas con el tinte global activado, respuesta escrita
  a "¿cambia el color de los personajes de forma limpia?" y a "¿qué vía identifica al Link remoto?", y un ADR con la
  decisión.
- **CA-1** Dado el montaje común con ventanas visibles y A y B a 5 m, cuando se toma una captura de ventana en B
  (`SAVAGEWIND_WINDOW_SHOT`), entonces el color medio de la región del Link remoto se desplaza hacia el color de A
  respecto a la misma captura con el tinte apagado, y el del Link local de B no cambia (diferencia < 2 %).
  Comparación por guion sobre las dos imágenes.
- **CA-2** Dado lo anterior, entonces el resto de la escena (una región de terreno fija) no cambia (< 2 %).
- **CA-3** Dado A que cambia de armadura, entonces la pieza nueva sale teñida en B en menos de 10 s.
- **CA-4** Dado el tinte apagado en Opciones, entonces la imagen es idéntica a la de un build sin esta función
  (misma captura, diferencia 0 salvo ruido temporal).
- **CA-5** Dado el tinte activado, entonces el tiempo de fotograma medio no sube más de un 2 % (medido con el
  overlay de rendimiento F10 o `swbench` [sin verificar la herramienta exacta]).
- **CA-6** Dado un reinicio a la pantalla de inicio con sesión abierta, entonces no queda ningún objeto teñido por
  error al volver al mundo.
- **CA-7** Dado otro backend o el grupo ausente, entonces el juego funciona sin tinte y lo dice en el registro.

## Qué NO hacer

- No empezar la implementación sin el resultado del paso 0.
- No cambiar `runtime/nvn/maxwell/translate.c` sin ADR: invalida las cachés de sombreadores de todos los usuarios.
- No meter lógica grande en `exec.c` (más de 12 000 líneas, en plena división por upstream): gancho mínimo y
  condicionado.
- No identificar dibujos por el hash del sombreador: Link y el Link remoto comparten sombreador (mapa 04 §4b).
- No sustituir texturas por dirección de memoria compartida: cambiaría a los dos Links a la vez (mapa 04 §4b).
- No incluir en el repositorio modelos ni texturas del juego, ni derivados: lo generado vive en `online_mod/`, que
  está ignorado por git.
- No inventar el layout del bloque de material de Link: no está investigado.
- No leer memoria del juego desde el hilo de render: los datos llegan publicados por el hilo del juego.
- No copiar de proyectos de decompilación.

## Riesgos y preguntas abiertas

- Riesgo principal: identificar los dibujos del segundo jugador, no el tinte (mapa 04 §6).
- Riesgo: el destino 0 no es el albedo; el tinte puede dar un resultado sucio en materiales diferidos.
- Riesgo: sombras, reflejos y otros pases dibujan el mismo modelo; teñirlos puede dar artefactos.
- Riesgo: constantes ligadas a 1.9.0 y al backend Vulkan.
- **Pregunta al dueño**: ¿"solo cambia el color" significa un tinte sobre todo el Link, o cambiar el color de una
  prenda concreta (por ejemplo la túnica)?
- **Pregunta al dueño**: ¿cada jugador debe verse a sí mismo también con su color, o basta con que vea teñido al
  compañero?
- **Pregunta al dueño**: si el experimento sale negativo, ¿aceptas quedarte con el color en nombre y marcador?
- **Pregunta al dueño**: ¿aceptas un cambio que obligue a todos los usuarios a recompilar sombreadores una vez?

## Fase y dependencias

- **Fase**: F5 (una a una, con ingeniería inversa). El paso 0 se puede hacer en F4 si hay tiempo.
- **Depende de**: CU-02, CU-10 (color asignado por jugador).
- **Lo necesitan**: ninguno.
