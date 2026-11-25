# 01 · Fundamentos de bajo nivel (lo justo)

Objetivo: poder leer `runtime/cpu.h` y `tools/lifter.py` sin perderte. No es un curso de C ni de ensamblador: es
el subconjunto que este proyecto usa, enseñado sobre **el código del propio repo**. Aquí no aparece código del
juego.

Todas las salidas de `lifter.py` de este módulo las obtuve ejecutando el lifter del checkout con instrucciones
sueltas escritas por mí (comando en §9).

---

## 1. C para quien viene de Go y Python

| Concepto | Go | C en este repo |
|---|---|---|
| Entero de 64 bits sin signo | `uint64` | `uint64_t` (de `<stdint.h>`, `runtime/cpu.h:4`) |
| Estructura | `type Cpu struct{…}` | `typedef struct … { … } Cpu;` (`cpu.h:7-21`) |
| Puntero a estructura | `c.sp` (Go desreferencia solo) | `c->sp` (flecha si `c` es puntero; punto si es valor) |
| Tipo función | `type GuestFn func(*Cpu)` | `typedef void (*GuestFn)(Cpu *);` (`cpu.h:30`) |
| Constante / función en línea | `const`, función normal | `#define` (sustitución de texto) y `static inline` |
| Privado al paquete | minúscula | `static` delante de la función o variable (privado al `.c`) |
| Interfaz pública | lo exportado | lo declarado en el `.h` (`runtime/online.h`) |
| Memoria | recolector | manual; aquí casi todo son arrays globales de tamaño fijo |
| Errores | `error` | valor de retorno (`bool`, `0`/`-1`); nadie te obliga a mirarlo |

Tres cosas que en Go no existen y aquí muerden:

1. **Comportamiento indefinido.** Leer fuera de un array no da *panic*: lee lo que haya. Un puntero colgante no
   falla hasta que falla.
2. **El preprocesador.** `#define LD32(a) (*(const u32u *)(uintptr_t)(a))` no es una función: es texto que se
   pega donde lo uses (`cpu.h:66`).
3. **Conversiones explícitas de puntero.** Un entero se puede convertir en puntero y leerse. Es exactamente lo
   que hace el recomp con las direcciones del juego.

## 2. Punteros: una dirección es un número

Mira `runtime/cpu.h:64-71`:

```c
#define LD32(a) (*(const u32u *)(uintptr_t)(a))
#define ST64(a, v) (*(u64u *)(uintptr_t)(a) = (uint64_t)(v))
```

Léelo de dentro afuera:

- `(a)` es un número de 64 bits: una dirección de memoria del juego.
- `(uintptr_t)(a)` dice "trata este número como una dirección de este proceso".
- `(const u32u *)` dice "ahí hay un entero de 32 bits".
- el `*` de fuera lo lee.

`u32u` (`cpu.h:55`) es `uint32_t` con dos atributos: `aligned(1)` (puede estar en cualquier dirección, no solo en
múltiplos de 4) y `may_alias` (el compilador no puede suponer que no se solapa con otros tipos). Sin eso, clang
optimizaría de forma incorrecta.

El comentario de `cpu.h:53` resume la decisión de diseño: *guest addresses are host addresses*. No hay tabla de
traducción.

**Dónde lo verás en el modo online:** `runtime/online.c:829` lee un puntero del juego con
`*(uint64_t *)(uintptr_t)g_site.player_info`, y `online.c:831` copia tres `float` desde ese objeto con `memcpy`.

## 3. Estructuras y offsets

Una estructura es un bloque de bytes con campos en posiciones fijas. El **offset** de un campo es cuántos bytes
hay desde el inicio.

```c
typedef struct __attribute__((aligned(16))) Cpu {
    uint64_t x[32];          // x0-x30 (x[31] unused; sp is separate)
    uint64_t sp;             // 256
    uint64_t pc;             // 264: target for indirect dispatch
    uint64_t nzcv;           // 272: flags in bits 31..28
    ...
    __uint128_t v[32];       // 512
} Cpu;

_Static_assert(offsetof(Cpu, sp) == 256, "sp");
```

(`runtime/cpu.h:7-28`.) `x` son 32 enteros de 8 bytes = 256 bytes, así que `sp` empieza en el byte 256. El
`_Static_assert` hace fallar la **compilación** si alguien mueve un campo.

¿Por qué tanto cuidado? Porque el generador en Python repite esos números (`tools/lifter.py:10-15`, con el
comentario "must match runtime/cpu.h"). Dos programas en dos lenguajes comparten un contrato binario. Es el
equivalente a un esquema protobuf, pero sin herramienta que lo verifique: solo el assert.

**Cuando leas código de ingeniería inversa** verás cosas como `player + 0x398`. Significa "el campo que está a
0x398 bytes del inicio del objeto jugador". Nadie tiene la definición de la estructura; se deduce de cómo el
código accede a ella (módulo 03 §5).

**Empaquetado.** Por defecto el compilador mete relleno entre campos para alinearlos. `#pragma pack(push, 1)`
(`runtime/online.h:44`) lo prohíbe: los bytes quedan pegados. El PR lo usa para que una estructura en memoria
sea, tal cual, el mensaje de red (`runtime/online.c:59-108`). Cómodo y frágil a la vez (módulo 04 §8).

## 4. Endianness

Un entero de varios bytes puede guardarse con el byte bajo primero (**little-endian**) o el alto primero
(**big-endian**). x86-64 y AArch64 en la Switch son little-endian, así que aquí casi nunca hay que invertir nada.

Dónde verlo:

- `tools/nso.py:16`: `struct.unpack_from('<II', raw, …)`. El `<` es "little-endian", `I` es `uint32`.
- `runtime/online.c:154`: la longitud de un mensaje se escribe a mano byte a byte,
  `buf[0] = (uint8_t)len, buf[1] = (uint8_t)(len >> 8)`. Byte bajo primero: little-endian explícito.
- `runtime/online.c:175`: la lectura inversa, `rb->data[o] | rb->data[o + 1] << 8`.

Contraste: los recomps de Wii U (PowerPC, big-endian) tienen que invertir bytes en cada acceso a memoria; lo
cuenta `../../GUIA-EXPERTO-RECOMP-Y-SDD.md` (fila B5).

## 5. Bits: extraer campos de un número

Una instrucción AArch64 son siempre 32 bits. Dentro van campos: qué operación, qué registros, qué constante.
`tools/lifter.py:25-30` tiene las dos herramientas:

```python
def bits(w, lo, n):
    return (w >> lo) & ((1 << n) - 1)

def sext(v, n):
    return v - (1 << n) if v & (1 << (n - 1)) else v
```

- `bits(w, lo, n)`: desplaza `lo` posiciones a la derecha y se queda con `n` bits.
- `sext` (*sign extend*): interpreta un campo de `n` bits como número con signo. Los saltos relativos lo usan
  para poder ir hacia atrás.

## 6. AArch64 en una página

AArch64 (también "ARM64") es la arquitectura de la CPU de la Switch.

| Cosa | Qué es | Dónde está en el `Cpu` |
|---|---|---|
| `x0`…`x30` | 31 registros generales de 64 bits | `c->x[0]`…`c->x[30]` |
| `w0`…`w30` | los 32 bits bajos de los mismos registros | `(uint32_t)c->x[n]` (`lifter.py:85-86`) |
| `sp` | puntero de pila | `c->sp` (aparte, `cpu.h:9`) |
| `xzr` / `wzr` | "registro cero": leerlo da 0, escribirlo no hace nada | `0ull` (`lifter.py:77-78`) |
| `pc` | contador de programa | no se guarda mientras corre; `c->pc` solo se usa como "destino" en saltos indirectos (`cpu.h:10`) |
| NZCV | cuatro bits de estado: Negativo, Cero (Z), aCarreo, desbordamiento (V) | `c->nzcv` bits 31..28 (`cpu.h:100-103`) |
| `v0`…`v31` | registros de 128 bits para coma flotante y vectores (`s0` = 32 bits bajos, `d0` = 64) | `c->v[n]` (`cpu.h:20`) |

Dos sutilezas que el lifter muestra muy bien:

- **El número de registro 31 es ambiguo**: según la instrucción significa `sp` o el registro cero. Por eso hay
  dos funciones, `X(r)` y `XS(r)` (`lifter.py:77-82`).
- **Escribir un `w` borra la mitad alta del `x`.** `set_reg` (`lifter.py:99-106`) emite
  `c->x[n] = (uint32_t)(…)` para resultados de 32 bits: la conversión a `uint32_t` y la asignación a un campo
  de 64 dejan los 32 bits altos a cero.

## 7. Una instrucción, una línea de C

Esto es lo que produce el lifter del repo para instrucciones que ensamblé yo mismo:

| Palabra | Ensamblador | C que emite `lifter.py` |
|---|---|---|
| `0x91001020` | `add x0, x1, #4` | `c->x[0] = (uint64_t)(c->x[1] + 0x4ull);` |
| `0xf9400100` | `ldr x0, [x8]` | `{ uint64_t ea = c->x[8] + 0x0ull; uint64_t v = LD64(ea); c->x[0] = (uint64_t)(v); }` |
| `0xeb01001f` | `cmp x0, x1` | `(void)(subs64(c, c->x[0], (uint64_t)(c->x[1])));` |
| `0x54000040` | `b.eq +8` | tipo `bcond`, condición `COND_EQ(c)`, destino `pc + 8` |
| `0x94000002` | `bl +8` | tipo `bl`, destino `pc + 8` |
| `0xd63f0100` | `blr x8` | tipo `blr`, destino `c->x[8]` |
| `0xd65f03c0` | `ret` | tipo `ret` |

Observa:

- `cmp` no existe como instrucción: es una resta (`subs`) cuyo resultado se tira a `xzr` y solo quedan los flags.
  `subs64` está en `cpu.h:127-131` y calcula N, Z, C y V con aritmética normal.
- Un salto condicional lee esos flags: `COND_EQ(c)` es `FLAG_Z(c)` (`cpu.h:104`).
- Los saltos no producen "C": producen un **tipo** (`Insn.kind`, `lifter.py:143`). Es `gen.py` quien decide si
  eso es un `goto`, una llamada a función C o una búsqueda en tabla (módulo 02 §4).

### Decodificar `0x91001020` a mano

En binario: `1001 0001 0000 0000 0001 0000 0010 0000`.

1. `lift()` mira `bits(w, 25, 4)` para elegir familia (`lifter.py:182-192`). Aquí sale "datos con inmediato" →
   `dp_imm`.
2. `op = bits(w, 23, 3)` = 2 → suma o resta con constante (`lifter.py:209`).
3. `sf = bit 31` = 1 → 64 bits. `opc = bit 30` = 0 → suma. `S = bit 29` = 0 → no toca flags.
4. `imm = bits(w, 10, 12)` = 4. `rn = bits(w, 5, 5)` = 1. `rd = bits(w, 0, 5)` = 0.
5. Resultado: `x0 = x1 + 4`.

Haz lo mismo con otra en el ejercicio E02.

## 8. Pila y convención de llamadas (AAPCS64)

Una **convención de llamadas** es el acuerdo sobre cómo se pasan argumentos y quién guarda qué. La de AArch64 se
llama AAPCS64. Lo que necesitas:

| Regla | Dónde se ve en el repo |
|---|---|
| Argumentos enteros en `x0`…`x7`; resultado en `x0` | `runtime/online_actors.c:236` pone `a0..a6` en `c->x[0..6]`; `:241` lee el resultado de `c->x[0]` |
| Primer argumento/resultado `float` en `s0` (= `v0`) | `online_actors.c:237-238` escribe `c->v[0]` |
| Resultado grande "por memoria": quien llama pasa en `x8` dónde escribirlo | `online_actors.c:246-255`, `call_sret` |
| `x30` (`lr`, *link register*) guarda la dirección de retorno | `tools/gen.py:651`: antes de cada llamada emite `c->x[30] = <dirección siguiente>` |
| `x29` (`fp`, *frame pointer*) encadena los marcos de pila | `runtime/dispatch.c:238-246` recorre `[fp]` y `[fp+8]` para imprimir quién llamó a quién en un crash |
| `x19`…`x28` (y `x29`, `sp`) los debe conservar la función llamada (*callee-saved*) | `tools/regs.py:41`: `CALLEE_SAVED = frozenset(range(19, 30)) | {SP}` |
| `sp` alineado a 16 bytes | `online_actors.c:239`: `c->sp = (c->sp - 0x400) & ~0xFull;` |

**La pila** crece hacia direcciones bajas. Una función típica empieza reservando sitio y guardando `x29` y
`x30`, y termina restaurándolos. La instrucción clásica de entrada es `stp x29, x30, [sp, #-0x30]!` ("guarda el
par en `sp - 0x30` y deja `sp` ahí"). El lifter la traduce así (salida real):

```c
{ uint64_t ea = c->sp - 48; ST64(ea, c->x[29]); ST64(ea + 8, c->x[30]); c->sp = ea; }
```

Esa misma forma aparece escrita a mano en un parche del repo, `tools/patches.py:416-418`, porque el parche
sustituye justo esa instrucción y debe reproducir su efecto.

**Por qué te importa para el cooperativo:** cuando el runtime llama a una función del juego (crear un actor,
mover un hueso) tiene que respetar estas reglas a mano. `call_f` (`online_actors.c:232-244`) guarda una copia
entera del `Cpu`, pone los argumentos, baja `sp` para no pisar la pila de quien le llamó, llama y restaura. Si
alguna vez ves un crash "raro" tras una llamada al juego, sospecha de aquí.

## 9. Saltos y direcciones: `bl`, `blr`, `br`, `ret`, `adrp`

| Instrucción | Qué hace | Cómo la trata el recomp |
|---|---|---|
| `b destino` | salto incondicional | `goto` dentro de la función, o llamada de cola si sale de ella |
| `b.cond`, `cbz`, `tbz` | salto condicional | `if (cond) goto …` (`lifter.py:280-294`) |
| `bl destino` | llamada: guarda retorno en `x30` y salta | llamada directa a la función C `f_<destino>(c)` (`gen.py:649-651`) |
| `blr xN` | llamada a la dirección que hay en un registro (métodos virtuales, punteros a función) | `c->pc = …; rt_dispatch(c);` (`gen.py:655-657`): se busca la función en una tabla en ejecución |
| `br xN` | salto a registro (tablas de `switch`, llamadas de cola indirectas) | `switch` con los destinos conocidos, o `rt_dispatch` (`gen.py:658-668`) |
| `ret` | vuelve a `x30` | `return;` (`gen.py:669-670`) |
| `adrp xN, página` | carga la dirección de una página de 4 KiB cercana | una constante: el lifter la calcula ya (`lifter.py:202-208`) |

`adrp` merece un minuto, porque los sitios por patrón giran a su alrededor. El código AArch64 no lleva
direcciones absolutas: forma una dirección en dos pasos, `adrp x8, <página>` y luego `add x8, x8, #<12 bits
bajos>` (o un `ldr` con ese desplazamiento). Con `pc = 0x7000001234`, el lifter convierte `adrp x8, .` en:

```c
c->x[8] = (uint64_t)(0x7000001000ull);
```

La misma cuenta está en `tools/game_sites.py:835-836` (`adrp_page`). Como la página depende de **dónde** está
la instrucción, si el código se mueve entre versiones esos bits cambian. Por eso los patrones los enmascaran
(módulo 02 §5).

Para repetir cualquiera de estas traducciones (no escribe nada en el repo; `-B` evita los `__pycache__`):

```
cd pr294-online
py -3 -B -c "import sys; sys.path.insert(0,'tools'); from lifter import Lifter; i=Lifter(0x7000000000).lift(0x91001020, 0x7000001000); print(i.kind, i.code, i.target, i.cond)"
```

## 10. Leer un desensamblado

Un desensamblado es una lista `dirección: palabra  mnemónico operandos`. Truco para leerlo sin ahogarte:

1. **Busca el marco.** Entrada (`stp x29, x30, …` / `sub sp, sp, …`) y salida (`ldp x29, x30, …` + `ret`)
   delimitan la función.
2. **Sigue `x0`.** Al entrar es el primer argumento (en C++ casi siempre `this`); al salir, el resultado.
3. **Marca las llamadas** (`bl`, `blr`). Entre dos llamadas, mira qué se carga en `x0`…`x7`: son los argumentos.
4. **Los accesos `[xN, #0x…]` son campos.** Mismo `xN` con distintos desplazamientos = misma estructura.
5. **`adrp` + `add`/`ldr` = una dirección global** (una cadena, una tabla, un puntero a un gestor).
6. **`cmp` + `b.cond` = un `if`.** Un salto hacia atrás = un bucle.

Practica con código **tuyo**: escribe una función de 10 líneas en C, compílala para AArch64 y desensámblala
(ejercicio E03). Tener el fuente al lado es la forma más rápida de aprender a leer.

## 11. Dos avisos sobre concurrencia que el `Cpu` ya te enseña

- **Atómicos.** El juego usa instrucciones "exclusivas" (`ldxr`/`stxr`) para sus cerrojos. El recomp las emula
  con *compare-and-swap* del compilador (`cpu.h:73-97`: `ldx##N` recuerda dirección y valor, `stx##N` solo
  escribe si nadie lo cambió). Es el mismo concepto que `atomic.CompareAndSwapUint64` en Go.
- **`volatile`.** Dice al compilador "esta variable puede cambiar sin que tú lo veas; léela siempre de memoria".
  El PR lo usa para banderas entre hilos (`online.c:210`, `446`). **No** es un cerrojo ni un atómico: en C11
  sigue siendo una carrera de datos si dos hilos escriben. Tenlo presente cuando revises código (módulo 07 §5).

## 12. Autoevaluación

1. ¿Qué diferencia hay entre `c->x[3]` y `(uint32_t)c->x[3]` en términos de registros de la consola?
2. ¿Por qué `LD32` no necesita invertir bytes?
3. ¿Qué registro tienes que mirar para saber quién llamó a una función?
4. ¿Qué pasaría si alguien añadiera un campo de 8 bytes al principio de `Cpu` y no tocara `lifter.py`?
5. ¿Por qué `adrp` es un problema para "buscar el mismo código en otra versión"?

Respuestas: (1) `x3` frente a `w3`. (2) Invitado y host son ambos little-endian y la dirección es directa.
(3) `x30`. (4) No compilaría: saltan los `_Static_assert` de `cpu.h:23-28`; sin ellos, el C generado leería
`sp` donde ahora hay otra cosa. (5) Porque codifica una distancia en páginas desde la propia instrucción.

Siguiente: [02 · Recompilación estática](02-recompilacion-estatica.md).
