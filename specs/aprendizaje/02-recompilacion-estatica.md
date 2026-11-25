# 02 · Recompilación estática: el pipeline de este proyecto

Objetivo: seguir el camino `NSO → C → biblioteca → runtime` archivo a archivo, entender un sitio y un parche
línea a línea, y saber por qué tocarlos obliga a regenerar. Referencia completa: `../mapa/01-recompilador.md`.

---

## 1. Qué problema resuelve

**Recompilación estática**: traducir *todo* el ejecutable de una vez, antes de ejecutarlo. Un emulador traduce
mientras corre. La ventaja es rendimiento nativo y que el código traducido puede llamar a funciones de tu PC. El
coste es que hay que **descubrir todo el código de antemano** (tabla comparativa en
`../../GUIA-RECOMPILACION.md` §1).

Tres preguntas que cualquier recomp debe contestar, y cómo lo hace este:

| Pregunta | Respuesta aquí | Dónde |
|---|---|---|
| ¿Dónde empieza y acaba cada función? | Tablas de desenrollado de pila (`.eh_frame`), símbolos y relocaciones | `tools/gen.py:153` (`boundaries`), `tools/analyze.py` |
| ¿Adónde va un salto calculado (`br`)? | Se emula el trozo con Unicorn para leer la tabla de saltos; si no se resuelve, hay camino de reserva | `tools/gen.py:190` (`jump_table`), `:291` |
| ¿Cómo llamo a una función cuya dirección solo se conoce en ejecución (`blr`)? | Una tabla dirección → función C | `runtime/dispatch.c:166-172` (`rt_dispatch`) |

## 2. El pipeline, archivo a archivo

```
 exefs/main, subsdk0, sdk   (tu copia; formato NSO)
   │
   ▼ tools/nso.py        abre, descomprime, lee símbolos y relocaciones
   ▼ tools/layout.py     direcciones de cada módulo
   ▼ tools/game_sites.py localiza los "sitios" por patrón           ─┐
   ▼ tools/patches.py    C a mano para cada sitio                    ├─ solo en `main`
   ▼ tools/gen.py        descubre funciones, pide a lifter.py       ─┘
   │   tools/lifter.py   una instrucción → un trozo de C
   ▼
 generated/<módulo>/*.c  +  generated/layout.c
   ▼ clang (Makefile)
 build/swgame.dll  +  swgame.dll.id
   ▼ runtime/gamelib.c   comprueba el id y carga
   ▼ runtime/loader.c    pone los NSO en memoria donde dice el layout
   ▼ runtime/dispatch.c  tabla dirección → función; arranca el juego
```

### 2.1 `tools/nso.py`: abrir el ejecutable

Un **NSO** es el formato de ejecutable de la Switch: una cabecera y tres segmentos (`.text` código, `.rodata`
constantes, `.data` datos), cada uno opcionalmente comprimido con LZ4.

- `header()` (`nso.py:11-18`) lee solo la cabecera: comprueba la firma `NSO0`, saca posición y tamaño de los
  segmentos y el **build id** (una huella del binario; identifica la versión del juego).
- `class Nso` (`nso.py:21-45`) descomprime y construye `image`: un `bytearray` con el módulo tal como quedaría en
  memoria.
- `_parse_mod0` (`nso.py:50`) lee la tabla dinámica: símbolos y **relocaciones** (los huecos que el cargador
  rellena con direcciones reales al cargar).

Este fichero no descifra nada: parte de un `exefs/` ya extraído por ti.

### 2.2 `tools/layout.py`: una sola fuente de verdad para las direcciones

El cargador de la consola coloca los módulos uno detrás de otro. `compute()` (`layout.py:55-67`) hace lo mismo
desde `MODULES_LO = 0x7000000000` (`layout.py:21`), redondeando a páginas de 4 KiB (`image_size`, `:31-34`).

Por qué importa: el C generado lleva direcciones **horneadas** como constantes (acabas de verlo con `adrp`). Si
el generador y el cargador discreparan en un solo byte, todo apuntaría mal. La cabecera del fichero lo dice:
"the one place that decides them" (`layout.py:1`).

También filtra módulos ajenos: `GAME_SUBSDKS` (`layout.py:40-43`) lista qué `subsdk` pertenecen al juego; otro
que aparezca en la carpeta (un mod) se deja fuera.

### 2.3 `tools/gen.py` + `tools/lifter.py`: escribir el C

`lifter.py` traduce **una** instrucción (módulo 01 §7). `gen.py` hace todo lo demás (`Module`, `gen.py:85`):

1. **Límites** (`boundaries`, `:153`) y **descubrimiento** (`discover`, `:737`): parte de lo seguro y añade
   destinos de `bl` y direcciones formadas con `adr`/`adrp+add` hasta que no aparece nada nuevo.
2. **Emisión** (`_emit_body`, `:543`). Para cada instrucción, según su tipo:

| Tipo | C emitido | Línea |
|---|---|---|
| `op` | el texto del lifter | `gen.py:634-636` |
| `bcond` | `if (cond) { goto L_…; }` | `:641-642` |
| `bl` a código conocido | `c->x[30] = <retorno>; f_<destino>(c);` | `:649-651` |
| `blr` | `c->pc = …; c->x[30] = …; rt_dispatch(c);` | `:655-657` |
| `br` con tabla | `switch (…) { case …: goto …; }` | `:663-667` |
| `ret` | `return;` | `:669-670` |
| **instrucción con parche** | el texto del parche, **en lugar** de la instrucción | `:608-619` |

3. **Optimización `--locals`**: los registros más usados pasan a variables locales C (`tools/regs.py:44`,
   `LOCALS = 6`) y los flags a un local (`tools/flags.py`). Solo necesitas saber que existe y que un parche
   siempre ve el `Cpu` completo: antes del parche se vuelca todo y después se recarga (`gen.py:611-616`).

Una instrucción que el lifter no entiende no rompe la generación: se emite una trampa `rt_udf` que aborta si
alguna vez se ejecuta (`lifter.py:193-194`).

### 2.4 La biblioteca `swgame` y su identificador

El C se compila en una biblioteca aparte (`swgame.dll` / `.so`). El `Makefile` calcula `GAMELIB_ID`: un hash de
los ficheros del generador, las cabeceras contra las que se compila y los flags (`Makefile:136-142`).

Al arrancar, `gamelib_load` (`runtime/gamelib.c:185-219`) compara el id guardado junto a la biblioteca con el
que espera el ejecutable (`id_matches`, `:123-126`). Si no coinciden, **no la carga**
(`"… was built for another version"`, `:197`). El log lo confirma con la línea
`[gamelib] … (game version 1.9.0, N sites)` (`gamelib.c:203`).

### 2.5 El runtime al otro lado

- `runtime/loader.c` carga cada NSO en la dirección del layout y aplica relocaciones.
- `runtime/dispatch.c` monta la tabla dirección → función. Un destino que no existe acaba en
  `FATAL("no function at …")` (`dispatch.c:153-158`): el síntoma clásico de una función no descubierta.
- `runtime/kernel.c`, `ipc.c`, `services/` hacen de sistema operativo; `runtime/hle/` sustituye funciones
  concretas; `runtime/nvn/` es la GPU. Para el cooperativo casi no se tocan (mapas 02 y 04).

## 3. Sitios: encontrar un punto del juego sin escribir su dirección

Un **sitio** es un punto con nombre dentro del `main` del juego. Se declara así
(`tools/game_sites.py:50`): `Site(name, kind, pattern, at)`.

- **pattern**: una secuencia de palabras de instrucción en hexadecimal. Cada una es `valor` o `valor/máscara`.
- **at**: cuál de esas palabras es "el" sitio (índice desde 0).
- **kind**: qué valor se quiere. `code` = la dirección de esa instrucción; `call` = el destino del `bl`;
  `ref` = la dirección que forma un `adrp+add`; `got`, `imm`… (tabla en `../mapa/01-recompilador.md` §3.6).

### La máscara: qué bits sobreviven a un cambio de versión

`mask()` (`game_sites.py:862-877`) decide qué bits de cada instrucción se comparan:

| Instrucción | Se conserva | Por qué |
|---|---|---|
| `b`, `bl` | solo el código de operación (`0xFC000000`) | el destino cambia si el código se mueve |
| `b.cond`, `cbz`, `cbnz` | operación + registro/condición | ídem |
| `adr`, `adrp` | operación + registro | la página depende de la posición |
| `add`/`ldr`/`str` tras un `adrp` del patrón | todo menos los 12 bits bajos | completan la dirección del `adrp` |
| resto | todo | incluidos los offsets de estructura |

El patrón se compila a una expresión regular sobre bytes (`compile_pattern`, `:901-912`) y debe encontrar
**exactamente un** lugar.

### Resolución y red de seguridad

`resolve()` (`game_sites.py:1041-1078`):

- Para la versión conocida hay una tabla `KNOWN` con la dirección esperada de cada sitio. Si el patrón encuentra
  **otra** dirección, error y se para (`:1058-1059`). Es una autocomprobación.
- Si el patrón ya no es único, se usa la de la tabla con aviso (`:1065-1067`).
- Los sitios opcionales van en **grupos todo-o-nada** (`OPTIONAL_GROUPS`, `:500`): si falta uno, se quita el
  grupo entero y sus parches (`:1070-1077`).
- Una versión del juego que no está en `VERSIONS` se rechaza (`for_module`, `:1081-1092`). Hoy solo hay una
  entrada, 1.9.0 (`:46-48`).

## 4. Un ejemplo existente, línea a línea: `has_fairy`

Función: en online, quien muere es reanimado "por un hada" en vez de cargar partida. Tres piezas.

### 4.1 El sitio (`tools/game_sites.py:202-205`)

```python
# whether Link has a fairy to revive him as he dies (...), its `and w0, w0, #1` before it returns
# (tools/patches.py _has_fairy: in online play he always has one, runtime/online_actors.c rt_online_has_fairy)
Site('has_fairy', 'code', 'a90023e9 94000000/fc000000 a9417bfd 910083ff 12000000 d65f03c0', 4),
```

- El comentario dice **qué es** y **quién lo usa**. Es la documentación del hallazgo.
- Seis palabras. La segunda es `94000000/fc000000`: "un `bl` a donde sea" (destino enmascarado). Las otras se
  comparan enteras.
- `at = 4`: el sitio es la quinta palabra, `12000000`. La sexta, `d65f03c0`, es el `ret` que ya conoces del
  módulo 01.
- `kind = 'code'`: interesa la dirección de esa instrucción, para sustituirla.
- Está en un grupo opcional propio, `('has_fairy',)` (`game_sites.py:560`), y tiene su entrada en `KNOWN`
  (`:724`).

### 4.2 El parche (`tools/patches.py:616-619`)

```python
def _has_fairy(gen, a):
    assert _word(gen, a) == 0x12000000, hex(_word(gen, a))  # and w0, w0, #1
    return ('{ extern uint32_t rt_online_has_fairy(uint32_t); c->x[0] = rt_online_has_fairy((uint32_t)c->x[0] & 1); }'
            f'  /* {a:#x}: and w0, w0, #1 (has Link a fairy: online play revives him) */')
```

Línea a línea:

1. `gen` es el módulo que se está generando; `a` la dirección del sitio.
2. **El `assert`**: lee la palabra real del binario (`_word`, `patches.py:18-19`) y exige que sea la esperada.
   Si el patrón hubiera caído en otro sitio, la generación falla aquí en vez de producir un juego roto. Regla
   del proyecto: `AGENTS.md:42-44`.
3. La cadena devuelta es C que se pega **en lugar de** la instrucción:
   - `extern …;` declara la función del runtime (los `.c` generados solo incluyen `cpu.h`);
   - `(uint32_t)c->x[0] & 1` **reproduce la instrucción original** (`and w0, w0, #1`);
   - el resultado pasa por `rt_online_has_fairy` y vuelve a `x0`, que es el valor de retorno (AAPCS64).
4. El comentario final queda en el C generado para quien lo lea.

El parche se registra por nombre de sitio en `PATCHES` (`patches.py:1367`) y `table()` lo asocia a la dirección
(`:1446-1449`), solo si el sitio existe.

### 4.3 El lado runtime (`runtime/online_actors.c:2825-2831`)

```c
uint32_t rt_online_has_fairy(uint32_t had) {
    if (had || online_state() != ONLINE_CONNECTED) return had;
    int rule = g_revive_rule;
    if (rule < 0) rule = g_online.revive;
    if (rule) LOG("[online] Link died: revived by a fairy (online play)\n");
    return rule ? 1 : 0;
}
```

- Si ya tenía un hada de verdad, o no hay sesión, **devuelve lo que el juego iba a devolver**. El parche es
  invisible fuera del modo online. Ese es el patrón que debes copiar.
- Si hay sesión, aplica la regla del anfitrión.

### 4.4 El otro ejemplo que debes conocer: el gancho por fotograma

`_vfr_ratio` (`tools/patches.py:422-427`) sustituye una lectura de 32 bits y, de paso, llama a
`rt_vfr_update(c->x[19])` y a `rt_online_frame(c)`. Termina con `c->x[8] = LD32(c->x[19] + 0x78);`, que es la
instrucción original. Todo el trabajo por fotograma del modo online cuelga de esa línea.

## 5. Qué puede y qué no puede hacer un parche

Resumen de `../mapa/01-recompilador.md` §3.7:

- Sustituye **una** instrucción y debe reproducir su efecto en el camino normal.
- Puede llamar al runtime con `c` o con valores sueltos.
- Puede salir con `return;` solo donde es seguro (primera instrucción o un `ret`).
- Puede saltar con `goto L_<hex>;` solo a etiquetas que ya existen en esa función.
- Usa `c->sp`, nunca el local `sp_`.
- La función Python debe ser determinista: `gen.py` la invoca varias veces.

## 6. Por qué tocar sitios o parches obliga a regenerar

1. El parche no vive en `savagewind.exe`: es texto dentro del C generado, compilado en `swgame`.
2. `tools/patches.py` y `tools/game_sites.py` forman parte de `GAMELIB_SRCS` (`Makefile:138-141`). Cambiar un
   carácter cambia `GAMELIB_ID`.
3. Con otro id, el runtime **rechaza** la biblioteca vieja (§2.4). Nunca se mezcla un ejecutable nuevo con
   código del juego viejo.

Consecuencias prácticas:

- Tras cambiar generador, lifter, parches o sitios: `make gen` y `make -j8` antes de probar
  (`AGENTS.md:75-80`).
- En un checkout, `gen.py` solo reescribe los trozos que cambian (`write_if_changed`, `gen.py:725-734`), así que
  un parche nuevo recompila pocos ficheros. Aun así, el plan presupuesta ~15 min por si toca reconstruir entero
  y pide **agrupar** estos cambios: los dos jugadores tienen que regenerar (`../PLAN-APROBADO.md`, reglas de
  trabajo).
- Lo que **no** obliga a regenerar: `runtime/online.c`, `online_actors.c`, la UI. Eso es solo `make`. Por eso
  conviene que la lógica viva en el runtime y el parche sea una línea que la llama.
- Atención: `runtime/cpu.h` y `runtime/gamelib.h` **sí** están en `GAMELIB_SRCS`.

Biblioteca desfasada: compara el `.id` con
`make -s --eval='print-gamelib-id:;@echo $(GAMELIB_ID)' print-gamelib-id` (`AGENTS.md:82-89`). No edites el
`.id` para saltarte la comprobación.

## 7. La deuda del PR #294 en este terreno

El PR añade 16 sitios por patrón, pero `runtime/online_actors.c` usa además **62 direcciones fijas** de 1.9.0
escritas como constantes (`#define FN_CREATE …`, `online_actors.c:30` y siguientes) y no comprueba la versión
del juego antes de llamarlas (`../mapa/07-pr294-integracion-juego.md` §3.1 y §6.1 A1). Funciona porque solo
existe 1.9.0. El primer arreglo de F1 es una puerta de versión; convertir direcciones en sitios es trabajo
gradual (módulo 03 §7).

## 8. Pruebas del generador que puedes ejecutar

| Prueba | Necesita | Qué demuestra |
|---|---|---|
| `tools/test_act_site.py` | solo Python | el texto C de un parche con instrucciones inventadas |
| `tools/test_game_sites.py` | `capstone` | coherencia de sitios, versiones, máscaras |
| `tools/test_layout.py` | `lz4`, `capstone`, `unicorn` | NSO **sintéticos** por todo el pipeline |
| `tools/test_lift.py` | tu `exefs/main` + clang | cada instrucción contra Unicorn (módulo 06 §8) |

Comandos y entorno en `AGENTS.md:94-104`.

## 9. Autoevaluación

1. ¿Qué pasa si el patrón de un sitio opcional deja de encontrarse en una versión nueva?
2. ¿Por qué el parche `_has_fairy` incluye `& 1`?
3. Añades un `LOG` en `rt_online_has_fairy`. ¿Hay que regenerar?
4. ¿Qué garantiza el `assert` del parche que el patrón no garantiza ya?

Respuestas: (1) se retira su grupo entero y sus parches; el juego arranca sin esa función. (2) Reproduce la
instrucción sustituida. (3) No: es runtime, basta `make`. (4) Protege contra el camino de reserva de `KNOWN` y
contra errores al editar el patrón: comprueba la instrucción exacta en la dirección final.

Siguiente: [03 · Ingeniería inversa con Ghidra](03-ingenieria-inversa-con-ghidra.md).
