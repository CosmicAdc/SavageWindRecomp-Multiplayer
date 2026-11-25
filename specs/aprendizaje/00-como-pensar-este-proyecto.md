# 00 · Cómo pensar este proyecto

Objetivo: que tengas un modelo mental correcto antes de entrar en detalles. Una hora de lectura.

## 1. La idea en una frase

Savage Wind toma el ejecutable de *Breath of the Wild* de Switch (versión 1.9.0) de **tu copia**, lo traduce una
vez a C, lo compila para tu PC y lo ejecuta dentro de un programa que hace de consola. No es un emulador: el
código del juego ya es código nativo de tu máquina cuando arranca.

## 2. Analogía para alguien de backend

Piensa en esto:

- **Un transpilador** (el recompilador, `tools/`): entra código máquina ARM de 64 bits, sale C. Como un
  transpilador TypeScript → JavaScript, pero el "lenguaje fuente" son instrucciones de CPU.
- **Una capa de compatibilidad** (el runtime, `runtime/`): el juego cree que habla con el sistema operativo de la
  consola (Horizon) y con su GPU. El runtime responde a esas llamadas con código de PC. Es el mismo papel que
  Wine para programas de Windows, o que un *shim* que implementa la API de un servicio que ya no existe.
- **Un sistema de plugins muy fino** (sitios y parches): puntos concretos del juego donde se sustituye **una
  instrucción** por C escrito a mano que llama al runtime. Es lo más parecido a un *middleware* o a un *hook*.

El multijugador no cambia el transpilador. Vive en el runtime y se engancha al juego por unos pocos parches.

## 3. El dibujo

```
  TU CONSOLA / TU COPIA                         (nada de esto está en el repo)
  ┌──────────────────────┐
  │ juego 1.9.0 extraído │   exefs/  = el programa (main, subsdk0, sdk; formato NSO)
  │ por ti               │   romfs/  = los datos (modelos, mapas, textos)
  └──────────┬───────────┘
             │ exefs/main, subsdk0, sdk
             ▼
  ┌──────────────────────────────────────────────────────────────┐
  │ RECOMPILADOR (Python, tools/)   se ejecuta EN TU PC          │
  │  nso.py      abre el NSO y descomprime sus segmentos         │
  │  layout.py   decide en qué dirección va cada módulo          │
  │  gen.py      descubre funciones y escribe C                  │
  │  lifter.py   traduce cada instrucción ARM a una línea de C   │
  │  game_sites.py + patches.py   "aquí, en vez de esta          │
  │              instrucción, pon este C que llama al runtime"   │
  └──────────┬───────────────────────────────────────────────────┘
             │ generated/*.c   (cientos de MB; NUNCA se sube a git)
             ▼ clang
  ┌──────────────────────┐        carga al arrancar        ┌───────────────────────────────────────┐
  │ swgame.dll / .so     │ ◄────────────────────────────── │ RUNTIME (C, runtime/) = savagewind.exe │
  │ "el juego, nativo"   │ ──── llama a rt_* / hle_* ────► │                                       │
  └──────────────────────┘                                 │  kernel.c, ipc.c, services/           │
                                                           │     hacen de sistema operativo        │
                                                           │  hle/   sustituye funciones del SDK   │
                                                           │  nvn/   traduce la GPU de la consola  │
                                                           │         a Vulkan (y sus shaders)      │
                                                           │  ui/    menú, ajustes, overlay        │
                                                           │                                       │
                                                           │  ┌─────────────────────────────────┐  │
                                                           │  │ MODO ONLINE (PR #294)           │  │
                                                           │  │ online.c         sockets, hilos │  │
                                                           │  │ online_actors.c  lee y escribe  │  │
   otro PC  ◄════ TCP, puerto 52520 ══════════════════════►│  │                  el juego       │  │
                                                           │  │ ui/online_ui.mm  pestaña/overlay│  │
                                                           │  └─────────────────────────────────┘  │
                                                           └───────────────────┬───────────────────┘
                                                                               ▼
                                                                    ventana + Vulkan + audio
```

Dónde verlo: el resumen del propio repo está en `AGENTS.md:7-19` y `CONTRIBUTING.md:86-98`.

## 4. Las cinco ideas que sostienen todo

### 4.1 "Invitado" y "anfitrión de ejecución" (guest y host)

- **Guest (invitado)**: el juego y su mundo: sus direcciones de memoria, sus registros de CPU, sus hilos.
- **Host**: tu PC y el runtime.

Ojo con el vocabulario: en red, "host" es también el jugador que hospeda la partida. En esta guía digo
**anfitrión** para el jugador que hospeda y **host** solo para "el lado PC" del recomp.

Detalle que simplifica mucho: aquí una dirección del invitado **es** una dirección real del proceso. El runtime
reserva el rango de memoria que el juego espera y carga los módulos ahí (`tools/layout.py:21-22`:
`MODULES_LO = 0x7000000000`). Por eso `LD32(a)` en `runtime/cpu.h:66` es un simple acceso a puntero.

### 4.2 Cada función del juego es una función C que recibe la CPU

El C generado tiene esta forma: `void f_<dirección>(Cpu *c)`. `Cpu` es una estructura con los registros de la
consola (`runtime/cpu.h:7-21`). Una instrucción "suma x1 y 4 y guarda en x0" se convierte en
`c->x[0] = c->x[1] + 4;`. Módulo 01 y 02.

### 4.3 No se edita lo generado: se parchea por sitios

Si quieres que el juego haga algo distinto en un punto, no tocas `generated/`. Declaras un **sitio** (un patrón
de instrucciones que localiza ese punto en cualquier versión) y un **parche** (el C que sustituye una
instrucción). Regla del proyecto: `AGENTS.md:39-44`. Módulo 02.

### 4.4 El juego no sabe que hay red

El juego original es de un jugador. El modo online **no** le enseña a ser multijugador: lo observa y lo manipula
desde fuera. Cada fotograma, en el hilo del juego, el runtime:

1. **lee** cosas de la memoria del juego (dónde está Link, qué huesos tiene, qué lleva puesto);
2. **llama** a funciones del propio juego (crear un actor, mover un hueso, activar un flag);
3. deja lo leído en buzones para que otro hilo lo envíe.

El punto de enganche es una sola línea de un parche: `rt_online_frame(c)` en `tools/patches.py:425-426`.
Módulo 05.

### 4.5 El segundo Link es una marioneta

El otro jugador no "existe" en tu juego. Lo que ves es un actor fabricado con tus propios archivos
(`tools/online_mod.py`) al que el runtime le escribe posición y huesos cada fotograma
(`runtime/online_actors.c:6053-6058`). No tiene IA ni cuerpo físico. Casi todas las rarezas del modo online
salen de ahí.

## 5. La analogía goroutine + canal, y dónde se rompe

Tú pensarías la red así en Go:

```go
// hilo del juego                       // goroutine de red
estados <- muestrear(link)              for e := range estados { conn.Write(e) }
```

En el PR es parecido en espíritu, distinto en mecánica:

| En Go | En `runtime/online.c` | Dónde |
|---|---|---|
| goroutine de red | un hilo `pthread`: `cl_thread` | `online.c:1160-1320` |
| goroutine del servidor | otro hilo: `srv_thread` → `srv_loop` | `online.c:343-410` |
| canal "último valor gana" (capacidad 1, se pisa) | una variable global protegida por mutex: `g_pose` + `g_pose_lock` | `online.c:811-820` |
| canal con búfer y `select { case ch <- x: default: }` (si está lleno, se tira) | array fijo + contador + mutex: `g.hit_out[32]`, `g.nhit_out` | `online.c:603-609` |
| `for range ch` para vaciar | funciones `online_take_*` que copian y ponen el contador a 0 | `online.c:617-624` |
| `context.Cancel` | `volatile bool stop` + `pthread_join` | `online.c:1322-1335` |

**Las salvedades (importantes):**

1. **No hay canales.** Son buzones de tamaño fijo con cerrojo. Nadie bloquea esperando: quien produce pisa o
   descarta, quien consume sondea.
2. **El hilo del juego no puede esperar.** Si lo bloqueas 20 ms, el juego pierde un fotograma. Por eso el hilo
   del juego nunca toca un socket.
3. **Solo el hilo del juego puede llamar al juego.** Llamar a una función del juego necesita una `Cpu` de un hilo
   invitado (`runtime/dispatch.c:174-180`, `guest_call`). El hilo de red no la tiene. Si la red quiere que pase
   algo en el juego, deja un recado y el hilo del juego lo ejecuta en su siguiente fotograma.
4. **No hay recolector de basura ni detector de carreras.** Un puntero a memoria del juego guardado en una
   variable global puede apuntar a basura tras "volver al título". El proyecto obliga a limpiar ese estado con
   `RT_ON_RESET` (`runtime/hle.h:40`; regla en `AGENTS.md:49-50`). El PR hoy **no** lo hace en los ficheros
   online (lo dice `../mapa/06-pr294-red-y-sesion.md` §1.4) y es trabajo de F2.
5. **El envío bloquea.** `send()` es bloqueante (`online.c:140-148`). En Go el runtime aparca la goroutine; aquí
   se para el hilo entero, y en el servidor ese hilo atiende a todos. Es el fallo H-1 del mapa 06.

## 6. Dónde se enchufa el cooperativo que vamos a construir

Lo que ya existe (PR #294) y lo que el plan cambia:

| Pieza | Hoy | El plan |
|---|---|---|
| Quién es el servidor | El juego del anfitrión abre un hilo servidor y se conecta a sí mismo por `127.0.0.1` (`online.c:1347-1361`, `1165`) | Se mantiene: la máquina del anfitrión es el servidor, conexión directa |
| Quién manda en el mundo | Mezcla: enemigos y botín los lleva el anfitrión; las misiones son la **unión** de todos (`online_actors.c:3926-3938`) | El mundo del anfitrión es la autoridad (CU-06, F4) |
| Inventario | Cada juego el suyo, pero algunos flags entregan objetos (`online_actors.c:3987-3997`) | Cada jugador conserva el suyo de forma explícita (CU-07, F4) |
| Robustez de red | Envío bloqueante, sin validación semántica, sin reconexión | F3 |
| Pruebas | Ninguna automática | F2: pruebas de protocolo sin el juego |



Siguiente: [01 · Fundamentos de bajo nivel](01-fundamentos-bajo-nivel.md).
