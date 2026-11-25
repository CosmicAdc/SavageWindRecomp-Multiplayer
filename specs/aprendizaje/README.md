# Ruta de aprendizaje: de backend Go/Python a cooperativo en un recomp

Esta carpeta es un curso corto pegado al trabajo real. Está pensada para alguien que programa bien (Go, Python,
herramientas de IA) pero no ha tocado ingeniería inversa de consolas, recompilación estática, C de bajo nivel ni
red para juegos.

**Cómo leer las referencias.** Toda cita `archivo:línea` apunta al checkout de solo lectura
`pr294-online/` (carpeta hermana de `specs/`, PR #294 "Online mode", protocolo 38, `runtime/online.c:40`).
Las líneas se comprobaron leyendo ese checkout; si el fork avanza, se moverán unas pocas líneas, así que busca
siempre por nombre de función. Lo que no pude comprobar va marcado **[sin verificar]**.

**Reglas que valen para todo el curso** (y para todo el proyecto):

1. Trabajas con **tu propia copia** del juego. Aquí no hay código del juego, ni claves, ni instrucciones de
   volcado o descifrado (`CONTRIBUTING.md:10-21`).
2. No se copia de proyectos de decompilación (`AGENTS.md:25-26`). El módulo 03 explica por qué.
3. Un arreglo cuenta cuando lo has **visto funcionar en el juego**, no cuando compila (`CONTRIBUTING.md:103`).

## El mapa

```
 00 Cómo pensar el proyecto ─┬─► 01 Bajo nivel ──► 02 Recompilación ──► 03 Ghidra ───────────┐
                             │                                                               │
                             └─► 04 Redes para juegos ──► 05 Anatomía del modo online ───────┼─► 06 Depurar
                                                                                             │      y probar
                               07 Specs y agentes  (léelo antes de F2)                       │
                               08 Contribuir       (léelo antes de publicar nada) ◄──────────┘
```

Hay dos carriles que puedes alternar: el de **máquina** (01 → 02 → 03) y el de **red** (04 → 05). El 00 va
primero y el 06 se usa desde el primer día que arranques el juego.

## Módulos, horas y qué sabrás hacer

Las horas son de estudio activo (leer + hacer los ejercicios marcados). Son una estimación mía, no una medida.

| # | Módulo | Horas | Al terminar puedes… | Fase del plan |
|---|---|---|---|---|
| 00 | [Cómo pensar este proyecto](00-como-pensar-este-proyecto.md) | 1-2 | Explicar en dos minutos qué hace cada pieza y dónde se engancha el multijugador | F0 |
| 01 | [Fundamentos de bajo nivel](01-fundamentos-bajo-nivel.md) | 6-8 | Leer `runtime/cpu.h`, decodificar a mano una instrucción AArch64 y predecir el C que produce el lifter | F0 |
| 02 | [Recompilación estática](02-recompilacion-estatica.md) | 5-7 | Seguir el pipeline archivo a archivo, leer un sitio y un parche, y saber cuándo un cambio obliga a regenerar | F0, F2 |
| 03 | [Ingeniería inversa con Ghidra](03-ingenieria-inversa-con-ghidra.md) | 8-12 | Montar tu proyecto Ghidra, encontrar una función por cadena o por comportamiento y convertir el hallazgo en un sitio por patrón | F2 (lectura), F5 (uso intensivo) |
| 04 | [Redes para juegos](04-redes-para-juegos.md) | 6-8 | Razonar sobre autoridad, interpolación, TCP/UDP y NAT, y señalar en `online.c` dónde está cada cosa | F1, F3 |
| 05 | [Anatomía del modo online](05-anatomia-del-modo-online.md) | 4-6 | Seguir un paquete STATE/SKEL y un evento de misión de punta a punta, con el hilo de cada paso | F1 |
| 06 | [Depurar y probar](06-depurar-y-probar.md) | 4-6 | Lanzar dos juegos en un PC, conducirlos con `swctl.py`, usar el bot, bisecar y leer un crash | F0, F1 (y siempre) |
| 07 | [Desarrollo con specs y agentes](07-desarrollo-con-specs-y-agentes.md) | 3-4 | Escribir una tarea con `files:` y `verify:`, repartir trabajo en worktrees y revisar C de bajo nivel escrito por IA | F2 en adelante |
| 08 | [Contribuir a la comunidad](08-contribuir-a-la-comunidad.md) | 2 | Preparar un PR que upstream pueda integrar, hablar con los autores del PR #294 y no meterte en un lío legal | antes de publicar |
| — | [Ejercicios](ejercicios.md) | 30-45 en total | Demostrar cada módulo sobre el código real | repartidos |
| — | [Recursos](recursos.md) | a demanda | Saber adónde ir cuando esto se quede corto | — |

Total: unas 40-55 horas de lectura más los ejercicios. Repartido en paralelo al trabajo, son 6-8 semanas a ritmo
tranquilo.

## Qué leer en cada fase

| Fase (ver `../PLAN-APROBADO.md`) | Qué hace la fase | Lectura mínima antes | Ejercicios |
|---|---|---|---|
| **F0 Preparación** | Instalar, compilar, arrancar en solitario | 00, 01 (§1-§4), 02 (§1-§3), 06 (§1-§2) | E01-E05 |
| **F1 Probar y arreglos rápidos** | Dos juegos en un PC, CU-01 a CU-03, arreglos pequeños | 04 (§1-§6), 05 entero, 06 entero | E06-E11 |
| **F2 Cimientos** | Pruebas de protocolo sin juego, partir `online_actors.c`, `RT_ON_RESET` | 02 entero, 07 entero, 03 (§1-§3) | E12-E14 |
| **F3 Red robusta** | Envío no bloqueante, validación, descubrimiento, reconexión, ADR TCP/UDP | 04 (§7-§12) | E15-E16 |
| **F4 Funciones cooperativas** | Autoridad del anfitrión, teletransporte, objetos, marcadores, PvP | 05 (§4 eventos), 04 (§3 autoridad) | E17 |
| **F5 Funciones difíciles** | Color en el modelo, historia a dos, más de 2 jugadores | 03 entero | E18-E19 |

## Cómo estudiar (consejos de alguien que ya tropezó)

- **Lee con el código abierto.** Cada sección dice "míralo en…". Si no abres el archivo, no se te queda.
- **Escribe a mano lo pequeño.** Decodificar un mensaje byte a byte (E06) enseña más que diez páginas sobre
  protocolos.
- **No estudies Ghidra antes de necesitarlo.** El módulo 03 es largo; para F1 basta saber que existe.
- **Apunta lo que ves, no lo que crees.** `../bitacora/` es para resultados de pruebas reales. Separa hipótesis,
  prueba sintética, build y juego verificado (`AGENTS.md:117-118`).
- **Cuando una explicación de aquí contradiga al código, gana el código.** Y abre una nota para corregir la guía.

## Relación con el resto de `specs/`

- `../mapa/01..07` son los mapas de referencia (exhaustivos, con todas las líneas). Esta carpeta es la versión
  para aprender: menos completa, más ordenada.
- Las dos guías de investigación (`../../GUIA-RECOMPILACION.md` y `../../GUIA-EXPERTO-RECOMP-Y-SDD.md`) dan el
  contexto de otros proyectos; aquí se reutilizan sus enlaces y no se las contradice.
- `../glosario.md` (lo escribe otro carril) es el sitio para buscar un término suelto.
