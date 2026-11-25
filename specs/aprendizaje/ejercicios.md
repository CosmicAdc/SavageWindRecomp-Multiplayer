# Ejercicios

19 ejercicios graduados, atados al código real y a las fases. Los primeros (E01-E08) **no necesitan ficheros del
juego**. Cada uno trae objetivo, pasos, cómo comprobar tu respuesta y tiempo estimado.

Normas:

- `pr294-online/` es de **solo lectura**. Todo lo que escribas va en una carpeta tuya fuera del repo
  (`C:\tmp\ej\…`) o, a partir de F0, en una rama de tu fork.
- Ejecuta Python con `-B` dentro del checkout de lectura para no dejar `__pycache__`.
- Apunta el resultado. Los que tocan el juego, en `../bitacora/`.

| # | Título | Módulo | Fase | Juego | Tiempo |
|---|---|---|---|---|---|
| E01 | Leer `next_msg` | 01, 04 | F0 | no | 30 min |
| E02 | Decodificar una instrucción a mano | 01 | F0 | no | 45 min |
| E03 | Desensamblar tu propio C | 01 | F0 | no | 1 h |
| E04 | Ejecutar pruebas sin ficheros del juego | 02 | F0 | no | 30 min |
| E05 | Tamaños y offsets de estructuras de red | 01 | F0 | no | 45 min |
| E06 | Decodificar mensajes del protocolo a mano | 04 | F1 | no | 45 min |
| E07 | Eco UDP en Go con pérdida, e interpolación | 04 | F1 | no | 2-3 h |
| E08 | Mapa de hilos y cerrojos | 05 | F1 | no | 1 h |
| E09 | Servidor dedicado + bot | 06 | F1 | solo el build | 1 h |
| E10 | Dos juegos en un PC: seguir un STATE | 05, 06 | F1 | sí | 2 h |
| E11 | Un flag de misión de un juego a otro | 05, 06 | F1 | sí | 1 h |
| E12 | Prueba de ida y vuelta del esqueleto | 04, 06 | F2 | no | 2 h |
| E13 | Fijar los tamaños de cable con aserciones | 04, 07 | F2 | no | 1 h |
| E14 | Inventario de estado para `RT_ON_RESET` y una tarea bien escrita | 07 | F2 | no | 1,5 h |
| E15 | Reproducir "un par lento congela a todos" | 04 | F3 | solo el build | 2 h |
| E16 | Medir con latencia y pérdida, y escribir el ADR | 04 | F3 | solo el build | 3 h |
| E17 | Diseñar la autoridad de los flags | 04, 05 | F4 | sí (tus datos) | 2 h |
| E18 | De una cadena a un sitio por patrón | 03 | F5 | sí | 3-4 h |
| E19 | Diferencias de memoria de una habilidad | 03 | F5 | sí | 2-3 h |

---

## E01 · Leer `next_msg`

**Objetivo.** Entender el enmarcado de mensajes leyendo nueve líneas de C.

**Pasos.**
1. Abre `runtime/online.c:160-185` (`RecvBuf`, `recv_some`, `next_msg`, `consumed`).
2. Contesta por escrito: ¿qué devuelve `next_msg` si en el búfer hay 2 bytes? ¿Y si hay un mensaje y medio?
   ¿Qué pasa con una longitud 0? ¿Quién mueve los bytes sobrantes al principio?
3. Simula a mano el búfer `05 00 07 01 68 6F` (6 bytes): ¿hay mensaje completo?

**Comprobación.** (2) `false` (menos de 3 bytes); devuelve el primero y en la segunda llamada `false`; con
longitud 0, `l < 1` → `false` para siempre (el servidor acaba cortando al llenarse el búfer,
`online.c:394`); `consumed`. (3) No: la longitud dice 5 bytes tras la cabecera y solo hay 4.

## E02 · Decodificar una instrucción a mano

**Objetivo.** Usar `bits()` como lo usa el lifter.

**Pasos.**
1. Toma la palabra `0xd2800540`. Escríbela en binario.
2. Sigue `Lifter.lift` (`tools/lifter.py:180-194`) y luego `dp_imm` (`:198-243`): ¿qué `op` es? ¿Qué valen
   `sf`, `opc`, `hw`, el inmediato y `rd`?
3. Escribe el C que crees que emite.
4. Compruébalo con el comando del módulo 01 §9 cambiando la palabra.

**Comprobación.** Es un "mover constante" de 64 bits a `x0` con valor 42 (`op` 5, `opc` 2). El lifter debe
imprimir una asignación de `0x2aull` a `c->x[0]`. Si tu C y el suyo difieren, encuentra en qué campo te
equivocaste.

## E03 · Desensamblar tu propio C

**Objetivo.** Aprender a leer un desensamblado teniendo el fuente al lado.

**Pasos.**
1. Escribe en tu carpeta un `suma.c` con una función que sume los elementos de un array de `int` y otra que la
   llame.
2. Compila a objeto para AArch64 y desensambla (con el clang y el `llvm-objdump` de MSYS2):
   `clang --target=aarch64-linux-gnu -O1 -c suma.c -o suma.o` y `llvm-objdump -d suma.o`
   **[comando sin verificar en este equipo: MSYS2 aún no está instalado]**.
3. Marca: el marco de entrada y salida, el bucle (salto hacia atrás), dónde llegan los argumentos, dónde sale
   el resultado, la llamada (`bl`).
4. Repite con `-O0` y compara.

**Comprobación.** Sabes señalar qué registro es el puntero al array y cuál el contador, y explicar por qué
`-O0` usa mucho más la pila.

## E04 · Ejecutar pruebas sin ficheros del juego

**Objetivo.** Ver cómo se prueba un parche con instrucciones inventadas.

**Pasos.**
1. Lee `tools/test_act_site.py` entero (es corto). Fíjate en el `gen` falso: un objeto con `base`, una imagen
   de **una** instrucción y un diccionario de sitios.
2. Ejecútalo: `py -3 -B tools/test_act_site.py` desde la raíz del checkout.
3. Cuando tengas el venv (F0): `tools/venv/Scripts/python.exe tools/test_game_sites.py`.
4. Contesta: ¿qué comprueba la prueba que no comprobaría compilar el juego?

**Comprobación.** Sale con código 0 y sin traza. Respuesta (4): que el texto C contiene la llamada esperada y
que el parche **rechaza** una instrucción distinta, sin necesitar el juego.

## E05 · Tamaños y offsets de estructuras de red

**Objetivo.** Calcular a mano el tamaño de una estructura empaquetada y verificarlo con el compilador.

**Pasos.**
1. Calcula el tamaño de `WireHello` (`runtime/online.c:105`) y de `OnlineLoot` (`runtime/online.h:239`), que
   están bajo `#pragma pack(1)`.
2. Calcula el offset de `name` en `OnlineLoot`.
3. En tu carpeta, escribe un `.c` que incluya `online.h` (con `-I<ruta>/runtime`) y contenga
   `_Static_assert(sizeof(OnlineLoot) == N, "loot");` y un `offsetof`.
4. Quita mentalmente el `pack(1)`: ¿cuánto mediría `OnlineLoot`?

**Comprobación.** `WireHello` = 4+2+24+24 = **54**; `OnlineLoot` = 4+4+1+40+12+36 = **97**; `name` en el offset
**9**. Compila sin error con esos números. Sin empaquetar habría 3 bytes de relleno tras `name` para alinear
los `float`: 100.

## E06 · Decodificar mensajes del protocolo a mano

**Objetivo.** Leer bytes de cable usando solo `online.h` y el comienzo de `online.c`.

**Pasos.** Con el `enum` de tipos (`online.c:50`) y las estructuras (`online.c:59-108`), decodifica:

```
A)  37 00 01 53 57 4F 4C 26 00 41 6E 61 00 ...(21 ceros)... 00 ...(24 ceros)
B)  02 00 02 01
C)  07 00 07 01 68 6F 6C 61 00
```

Para cada uno: longitud, tipo, sentido (cliente→servidor o al revés) y contenido.

**Comprobación.**
- A) longitud 0x37 = 55 (1 de tipo + 54); tipo 1 `HELLO`, cliente→servidor; magia `SWOL`
  (`0x4c4f5753` en little-endian), versión 0x26 = 38, nombre "Ana", sin contraseña.
- B) longitud 2; tipo 2 `WELCOME`, servidor→cliente; ranura 1.
- C) longitud 7; tipo 7 `CHAT`, servidor→cliente (lleva la ranura delante: 1); texto "hola" con su cero final.
Extra: ¿cómo sería C) en sentido cliente→servidor? (Sin el byte de ranura: `06 00 07 68 6F 6C 61 00`.)

## E07 · Eco UDP en Go con pérdida, e interpolación

**Objetivo.** Sentir en tus manos por qué se interpola en el pasado.

**Pasos.**
1. Emisor: cada 50 ms envía por UDP `{seq uint32, t_envio int64, x float32}` donde `x` es una posición que se
   mueve (un seno vale).
2. Entre medias, un "mal cable" en el propio receptor: descarta un porcentaje configurable y retrasa cada
   paquete un tiempo aleatorio.
3. Receptor: guarda las últimas 8 muestras **con la hora de llegada**. A 60 Hz imprime tres series:
   (a) la última muestra recibida; (b) interpolación entre las dos muestras que rodean `ahora − 100 ms`;
   (c) extrapolación desde las dos últimas.
4. Prueba con 0 %, 5 % y 20 % de pérdida, y con retraso variable de 0-80 ms. Grafica o compara el error
   contra la posición real.
5. Cambia el sello de tiempo de llegada por el `t_envio` del emisor y repite.

**Comprobación.** Sabes explicar con tus datos: por qué (a) va a saltos; por qué (b) es suave pero va tarde;
cuándo (c) se dispara; y qué mejora al usar la hora del emisor cuando los paquetes llegan a ráfagas. Compara tu
receptor con `online_players` (`runtime/online.c:1406-1418`): ¿qué hace él que tú no (el corte por
teletransporte, el ángulo por el camino corto)?

## E08 · Mapa de hilos y cerrojos

**Objetivo.** Saber en qué hilo corre cada función de la interfaz `online.h`.

**Pasos.**
1. Haz una tabla con todas las funciones declaradas en `runtime/online.h:163-264`.
2. Para cada una, busca quién la llama (`grep -n "nombre(" runtime -r`) y anota: hilo que la llama, cerrojo
   que toma, y si es "último valor", "cola" o "copia".
3. Marca las variables que se leen sin cerrojo (pista: `online.c:1375-1377`).

**Comprobación.** Contrasta con el módulo 05 §1 y §5 y con `../mapa/06-pr294-red-y-sesion.md` §1.2 y §5.3.
Debes haber encontrado al menos: `g.state`, `g.hosting`, `g.id` y `g_srv_running` leídas sin cerrojo.

## E09 · Servidor dedicado + bot

**Objetivo.** Ver el protocolo vivo sin el juego. Necesita `savagewind.exe` compilado (sin código generado
basta, `AGENTS.md:80`) **[sin verificar que `--server` arranque sin ficheros del juego; por el código
(`runtime/main.c:440-446`) sale antes de cargarlos]**.

**Pasos.**
1. Compila el bot (módulo 06 §5).
2. Lanza `savagewind.exe --server 52530` y dos bots con nombres distintos contra ese puerto.
3. Lee la salida del servidor: entradas y salidas.
4. Mata un bot con Ctrl+C y cronometra el aviso de salida. Lanza otro con `BOT_FREEZE=5` y cronometra de
   nuevo.

**Comprobación.** Ves `joined (slot 0)` y `(slot 1)` (`online.c:259`). El bot cerrado se detecta al instante
(`recv` devuelve 0, `online.c:387`); el congelado, a los ~10 s (`TIMEOUT_NS`, `online.c:46`, `384`).

## E10 · Dos juegos en un PC: seguir un STATE

**Objetivo.** El recorrido A del módulo 05, visto en ejecución.

**Pasos.**
1. Lanza las dos instancias (módulo 06 §4), una con ventana.
2. Con `swctl.py` lleva a los dos Links cerca uno del otro (`pos` en uno, `goto` con esa línea en el otro).
3. En una rama de tu fork, añade **un** `LOG` temporal en cada paso del recorrido A (pasos 2, 4, 6, 7 y 9),
   limitado a una vez por segundo, con la posición X.
4. Mueve a un Link con `stick` y sigue el número por los cinco logs en los dos ficheros de log.
5. Quita los logs.

**Comprobación.** Puedes señalar, con marcas de tiempo, cuánto tarda la posición en llegar del paso 2 al 9 y
qué parte es el colchón de 100 ms. Anótalo en la bitácora.

## E11 · Un flag de misión de un juego a otro

**Objetivo.** El recorrido B1 del módulo 05.

**Pasos.**
1. Con **copias** de guardado, dos instancias conectadas.
2. En el invitado: `swctl.py --port 7778 flag <nombre>` de un flag que esté a 0 en ambos (elige uno inocuo de
   tu `quests.txt`).
3. `setflag <nombre>` en el invitado. Espera unos segundos.
4. Busca en el log del anfitrión la línea `quest flag … set (another player did it)` y confirma con `flag`.
5. Sal de la sesión en el invitado, únete a otra sesión nueva con otro guardado limpio y observa si el flag
   reaparece.

**Comprobación.** (4) ocurre en ~2-4 s (el ciclo es cada 60 llamadas, `online_actors.c:3880`). (5) es el
hallazgo T-1: documenta lo que veas; es el repro de un arreglo de F1.

## E12 · Prueba de ida y vuelta del esqueleto

**Objetivo.** Tu primera prueba de protocolo sin juego.

**Pasos.**
1. Lee `skel_encode` y `skel_decode` (`runtime/online.c:531-569`).
2. En tu rama, escribe una prueba al estilo del repo que: genere 10.000 cuaterniones unitarios y traslaciones
   aleatorias en ±3 m, codifique, decodifique y compare.
3. Decide la tolerancia **razonándola**: 15 bits por componente, 1/8192 m por unidad.
4. Añade casos límite: cuaternión nulo, traslación de 10 m, componente mayor negativa.

**Comprobación.** Pasa con una tolerancia que sabes justificar. Descubres al menos dos comportamientos que
merecen comentario: la traslación **satura** a ±4 m, y el cuaternión decodificado puede salir con el signo
global cambiado (representa la misma rotación).

## E13 · Fijar los tamaños de cable con aserciones

**Objetivo.** Que el bot y el juego no puedan divergir en silencio.

**Pasos.**
1. Calcula el tamaño de `WireState` sumando campos (`runtime/online.c:60-104`). Usa los tamaños de las
   subestructuras de `online.h`.
2. Compara con la copia del bot (`tools/online_bot.c:31-78`).
3. Propón (sin implementarlo aún) dónde iría un `_Static_assert` y cómo compartir la definición.

**Comprobación.** Te sale **3375** (`../mapa/06-pr294-red-y-sesion.md` §2.3 da el mismo número, calculado a
mano). Tu propuesta responde: ¿cabecera común?, ¿quién es su dueño?, ¿qué pasa con la versión de protocolo?

## E14 · Inventario para `RT_ON_RESET` y una tarea bien escrita

**Objetivo.** Preparar trabajo paralelizable.

**Pasos.**
1. Lista las variables `static` y globales de `runtime/online.c` que guardan estado de sesión. Marca cuáles
   limpia `stop_all` (`:1322-1335`) y cuáles no.
2. En `runtime/online_actors.c`, localiza 10 variables que guarden direcciones de memoria del juego.
3. Lee cómo se usa `RT_ON_RESET` en otro fichero del runtime (`grep -rn RT_ON_RESET runtime | head`).
4. Escribe tres tareas en el formato del módulo 07 §2 con `files:` y `verify:` reales.

**Comprobación.** Tu lista (1) incluye al menos `g.quests`, `quests_have`, `g.loots`, `g.en`, `quest_of` y las
colas pendientes. Ninguna de tus tres tareas comparte fichero con otra, o están marcadas en serie.

## E15 · Reproducir "un par lento congela a todos"

**Objetivo.** Demostrar el hallazgo H-1 antes de arreglarlo.

**Pasos.**
1. Escribe en Go un cliente que conecte, envíe un `HELLO` válido (ya sabes construirlo, E06), lea el `WELCOME`
   y **deje de leer** para siempre sin cerrar.
2. Servidor dedicado + dos bots normales + tu cliente.
3. Observa cuánto tardan los bots en dejar de recibir (añade a tu copia del bot un contador de mensajes por
   segundo).
4. Intenta parar el servidor.

**Comprobación.** Tras llenarse los búferes del sistema, los bots dejan de recibir y tu cliente **no** es
expulsado a los 10 s. Explica por qué con `online.c:140-148`, `213-216` y `384`. Guarda el programa: es el
`verify:` del arreglo de F3.

## E16 · Medir con latencia y pérdida, y escribir el ADR

**Objetivo.** Decidir TCP o UDP con datos.

**Pasos.**
1. En Docker (Linux), servidor dedicado + dos bots, con `tc netem` añadiendo 50 ms ± 20 ms y 2 % de pérdida.
2. Mide en el receptor el intervalo entre `STATE` consecutivos: media, percentil 95 y máximo.
3. Repite con 0 %, 2 % y 5 %.
4. Escribe un ADR de una página en `../decisiones/`: contexto, opciones, datos, decisión, consecuencias.

**Comprobación.** Tus datos muestran las ráfagas del bloqueo de cabeza de línea (máximos muy por encima de
50 ms seguidos de varios mensajes juntos). El ADR separa lo medido de lo supuesto.

## E17 · Diseñar la autoridad de los flags

**Objetivo.** Convertir CU-06 y CU-07 en reglas.

**Pasos.**
1. Abre tu `online_mod/quests.txt` (no lo copies a ningún sitio).
2. Elige 30 nombres variados y clasifícalos: progreso de historia, mundo físico (cofres, objetos), personal
   (objetos obtenidos, habilidades).
3. Para cada clase decide: quién manda, si se propaga, en qué sentido, y qué pasa con los objetos que hoy se
   entregan (`runtime/online_actors.c:3987-3997`).
4. Escribe la regla como tabla, **por patrón de nombre**, sin listar nombres del juego uno a uno.

**Comprobación.** Tu tabla contesta: un invitado avanzado, ¿puede cambiar el guardado del anfitrión? ¿Qué ve
un invitado nuevo en un mundo avanzado? ¿Qué pasa al desconectar? Contrástala con
`../contratos/autoridad-mundo.md`.

## E18 · De una cadena a un sitio por patrón

**Objetivo.** El flujo completo del módulo 03, en local.

**Pasos.**
1. Con tu proyecto Ghidra montado, elige el nombre de una animación o de un actor que conozcas.
2. `gq.py strings`, `xrefs`, `callees` hasta tener una función candidata y saber qué recibe en `x0`.
3. Confírmalo en vivo con `SAVAGEWIND_FNPROF_ARGS`.
4. Genera el patrón con `game_sites.py pattern` y comprueba que es único con `find`.
5. Escribe el comentario del sitio **en tus palabras**: qué es, cómo lo sabes.

**Comprobación.** `find` lo lista como `new`/`ok`; tu comentario no contiene pseudocódigo; tus notas locales
siguen la plantilla del módulo 03 §6. Nada de la salida de Ghidra ha salido de tu máquina.

## E19 · Diferencias de memoria de una habilidad

**Objetivo.** La técnica de `docs/ONLINE.md:767-768`.

**Pasos.**
1. Una instancia, canal de control abierto. Localiza al jugador (`sites`, y el sitio `player_info`).
2. Vuelca con `mem read` el rango que cita esa línea de la documentación, en reposo. Guárdalo en un fichero
   local.
3. Activa una habilidad visible y vuelve a volcar. Repite dos veces más.
4. Con un script tuyo, lista los offsets que cambian **siempre** al activarla y vuelven al soltarla.
5. Interpreta dos de ellos (puntero, bit, contador).

**Comprobación.** Tienes una lista corta y estable (no decenas de offsets ruidosos) y una hipótesis verificada
en vivo para al menos uno. Los volcados se quedan en tu máquina.
