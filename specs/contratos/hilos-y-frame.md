# Contrato: hilos y orden dentro del fotograma

Este documento dice **qué hilo puede tocar qué** y **en qué orden ocurren las cosas** dentro de un fotograma del modo
en línea. Es autosuficiente: no hace falta haber leído ningún otro documento para aplicarlo.

- Las secciones 1 a 4 describen el código **tal como es hoy** (PR #294, commit `350c5f8`), con `archivo:línea`.
- Las secciones 5 a 7 son las **reglas que debe cumplir el código nuevo**, escritas como líneas comprobables
  (DEBE / NO DEBE).

Nada de esto se ha ejecutado: todo sale de leer el código. Lo que no se pudo comprobar leyendo va marcado
`[sin verificar]`.

## 0. Cuatro ideas antes de empezar

1. **Invitado** (*guest*) = el juego recompilado y su memoria. **Anfitrión de máquina** (*host*) = nuestro programa
   en C. Un puntero del juego es una dirección normal del proceso (ventana `0x7000000000`..`0x7320000000`,
   `runtime/rt.h:35-36`): se puede desreferenciar desde cualquier hilo **sin que nada lo impida**, y por eso hay que
   prohibirlo por contrato.
2. **El hilo del juego no es el hilo principal del proceso.** El hilo principal atiende la ventana; el bucle del
   juego corre en otro hilo creado al arrancar (`runtime/kernel.c:498-501`).
3. **Solo el hilo del juego puede llamar a funciones del juego.** Hace falta su `Cpu *c` (los registros del
   procesador emulado). Un hilo de red no lo tiene (`kernel_current_cpu()` devuelve `NULL` fuera de un hilo del
   invitado, `runtime/kernel.c:504`).
4. **Al "volver a la pantalla de inicio" la memoria del juego se borra, pero nuestros hilos siguen vivos.** Todo lo
   que apunte al juego debe limpiarse (`RT_ON_RESET`, `runtime/hle.h:28-44`).

---

## 1. Hilos que existen hoy

| Hilo | Función de entrada | Qué hace | Referencia |
|---|---|---|---|
| **Juego** (invitado principal) | bucle del juego → parche `_vfr_ratio` → `rt_online_frame(Cpu *c)` | Muestrea a Link, publica su estado, consume el de los demás, crea y mueve actores | `tools/patches.py:422-426`, `runtime/online_actors.c:5721` |
| **Red, servidor** (solo en el anfitrión de la partida) | `srv_thread` → `srv_loop` | `select()` de 100 ms sobre el socket de escucha y hasta 8 clientes; acepta, lee, trocea y reenvía con `send()` bloqueante | `runtime/online.c:343-401` |
| **Red, cliente** | `cl_thread` | `select()` de 10 ms; recibe y guarda (`cl_message`); vacía las colas de salida; envía STATE y SKEL a 20 Hz y PING a 1 Hz | `runtime/online.c:1160-1320` |
| **Interfaz** (presentación / menú) | `online_tab_draw`, `online_overlay_draw` | Dibuja la pestaña Online y las marcas en pantalla; llama a `online_host` / `online_join` / `online_leave` | `runtime/ui/menu.mm:2357`, `2594`; `runtime/ui/online_ui.mm` |
| **Render** (Vulkan) | `online_camera_set` | Publica la matriz de cámara para proyectar nombres en pantalla | `runtime/nvn/vulkan/exec.c:10999-11005`, `runtime/online.c:1485` |
| **Principal del proceso** | `main` | Carga `online.ini`; arranque automático por `SAVAGEWIND_ONLINE` | `runtime/main.c:463`, `525-529` |
| **Otros hilos del invitado** | parches `arrow_move`, `beam_move`, `xlink_emit`, `blur_*`, `has_fairy`, `menu_draw`, `scene_state_*` | Código del juego que llama a `rt_online_arrow`, `rt_online_xlink_emit`, `rt_blur_anim`, `rt_online_has_fairy`, `rt_menu_draw_pause`, `rt_scene_state` desde el hilo en que se esté ejecutando | `runtime/online_actors.c:820`, `868`, `935`, `2825`, `2812`, `2798` |

Sobre el gancho del hilo del juego: `rt_online_frame` se llama **una vez por actualización del juego**, siempre (haya
o no sesión), justo después de `rt_vfr_update` y antes de la actualización de los actores. En pausa hay dos
actualizaciones por fotograma (`runtime/hle/framepace.c:678-679`). Qué hilo del invitado ejecuta exactamente ese
código no se ha comprobado en ejecución `[sin verificar]`; el comentario del parche dice "on this thread before the
actors' update" (`tools/patches.py:424`).

## 2. Cerrojos y datos compartidos hoy

No hay colas sin bloqueo: son buffers de tamaño fijo protegidos por dos cerrojos (`pthread_mutex_t`).

| Cerrojo | Protege | Referencia |
|---|---|---|
| `g.lock` | Todo el estado del cliente: jugadores remotos `g.r[]`, chat, avisos, colas de entrada y de salida, enemigos, botín, flags, cámara, y el esqueleto local `g_skel` | `runtime/online.c:441-486`, `582-586` |
| `g_pose_lock` | Lo que el hilo del juego publica de sí mismo: `g_pose`, `g_equip`, `g_local_en` | `runtime/online.c:811` |
| (ninguno) | La estructura `Server`: solo la toca su hilo. Comparte `sv->stop` y `g_local_hour` como `volatile` | `runtime/online.c:43`, `194-211` |

Variables que hoy se leen **sin** cerrojo desde otro hilo (carreras de datos, benignas en x86-64 pero incorrectas
según el estándar de C): `g.state`, `g.hosting`, `g.id`, `g_srv_running`, `g.thread_on`
(`runtime/online.c:1202`, `1262`, `1265`, `1375-1377`, `1389`). Los ajustes `g_online` los escribe la interfaz y los
leen el hilo de red y el del juego sin cerrojo (`runtime/online.c:1165-1166`, `1177-1178`).

## 3. Cómo se pasan los datos hoy entre el hilo del juego y la red

Toda la comunicación pasa por las funciones de `runtime/online.h`. `online_actors.c` no abre sockets.

| Sentido | Mecanismo | Funciones |
|---|---|---|
| Juego → red, "último valor" | El juego sobrescribe un buffer bajo cerrojo; el hilo de red lo copia a su ritmo | `online_set_local_pose` (`online.c:815`), `online_set_local_skeleton` (`:576`), `online_set_local_equip`, `online_set_local_enemies` (`:594`), `online_set_local_hour` (`:763`), `online_set_loots`, `online_set_quests` |
| Juego → red, cola con tope | El juego añade; si la cola está llena, **se pierde en silencio** | `online_send_hit` (32), `online_send_pick` (8), `online_send_loot_add` (16), `online_send_loot_take` (16), `online_send_estates`, `online_send_quests` |
| Red → juego, instantánea | El juego pide una copia interpolada 100 ms por detrás | `online_players` (`online.c:1387`), `online_skeleton`, `online_enemies`, `online_loots`, `online_quests`, `online_host_hour` |
| Red → juego, cola que se vacía | El hilo de red encola en `cl_message`; el juego recoge y vacía | `online_take_hits` (64), `online_take_estates` (64), `online_take_loot_adds` (32), `online_take_loot_takes` (32), `online_take_picks` (16), `online_take_quests` |

Dos incumplimientos actuales del modelo "la red solo hace entrada/salida":

- **El hilo de red lee memoria del juego.** `sample_local` lee `PlayerInfo` (posición de Link) y el estado de pausa
  20 veces por segundo, sin sincronizar (`runtime/online.c:823-834`). La interfaz hace lo mismo
  (`runtime/ui/online_ui.mm:258-259`).
- **Hilos del invitado escriben anillos sin protección.** `rt_online_xlink_emit` y `rt_online_arrow` anotan en un
  anillo desde cualquier hilo; el índice es atómico pero la entrada no, y el hilo del juego puede leerla a medias
  (`runtime/online_actors.c:820-825`, `868-898`).

## 4. Orden dentro de `rt_online_frame` hoy

Todo ocurre en `runtime/online_actors.c:5721-6468`. Este orden **es parte del comportamiento**: cambiarlo cambia lo
que pasa. Los pasos marcados "SALE" son `return` tempranos: lo que hay debajo no se ejecuta ese fotograma.

| Paso | Líneas | Qué hace |
|---|---|---|
| 1 | 5722-5725 | Lee `PlayerInfo`; atiende peticiones del canal de control (`find_actors`, `flag_query`, `event_query`) |
| 2 | 5726 | **SALE** si no existe el overlay de actores (`online_actors_on`, `:225-229`) |
| 3 | 5727-5745 | Calcula `me` (ranura local) y si hay mundo. Si hay mundo pero **no** sesión: borra los actores de los jugadores y **SALE** |
| 4 | 5746-5761 | Detecta una carga (sin Link, o salto de más de 100 m): `forget_all()` y 60 fotogramas de espera (`g_settle`) |
| 5 | 5762-5764 | Actualiza `g_online_actor[]`; **SALE** si no hay mundo |
| 6 | 5766 | **Muestreo local**: `sample_pose` y `sample_skeleton` (no en pausa, salvo "menús sin pausa") |
| 7 | 5767-5845 | Regla "menús sin pausa": limpia bits de pausa del juego, reanuda la física |
| 8 | 5851-5864 | Hora del día: publica la propia y adopta la del anfitrión |
| 9 | 5865-5869 | Mantenimiento: `pool_update`, `booms_update`, `fx_update`, `orphans_update` |
| 10 | 5870 | **SALE** mientras dure la espera tras una carga (`g_settle`) |
| 11 | 5871-5880 | **SALE** si el Link local está muerto, y espera 180 fotogramas al volver |
| 12 | 5881-5944 | Bloques de prueba activados por variables de entorno |
| 13 | 5948-5954 | `owns_left`, **botín** (`loot_update`), **flags** (`quests_update`) |
| 14 | 5955-5964 | Con una copia de los jugadores: clima, Luna Carmesí, **PvP** (`pvp_update`), **colisión** (`players_collide`, que además refresca las reglas de la sesión), golpes con menú abierto, guardia de enemigos |
| 15 | 5965 | Armas en el suelo (`ground_update`) |
| 16 | 5966-5972 | Con otra copia: **objetos**, **mecanismos**, **roturas** (`objs_update`, `movers_update`, `breaks_update`) |
| 17 | 5973-5980 | Con otra copia: **runas** (`grab_update`, `stop_update`, `ice_update`) y jefes (`boss_update`) |
| 18 | 5981-6001 | Bloqueo del botón A cerca de un jugador en un pedestal |
| 19 | 6002-6009 | **Enemigos**: el anfitrión `enemies_host` (1 de cada 3 fotogramas) y `enemies_show`; los demás `enemies_client` |
| 20 | 6011-6058 | **Bucle por jugador**: decide si se quiere su Link; comprueba `alive()` (`:6036`); lo crea; lo coloca con `FN_SET_MTX`; `pose_link` |
| 21 | 6059-6468 | Dentro del mismo bucle: armadura, equipo, caballo, materiales, disparos, efectos, explosiones |

Tres consecuencias de este orden que ya son fallos conocidos:

- **El paso 17 usa el Link remoto antes de que el paso 20 compruebe que sigue vivo.** Las runas llaman a
  `bone_mtx(c, g_act[i].link, …)` (`:4323`, `:4437`, `:4439`) y leen `link + 0x398` (`:4307`, `:4326`) con el puntero
  guardado del fotograma anterior; `alive()` no se mira hasta `:6036`. Si el juego borró ese actor entretanto, se
  usa memoria liberada.
- **La tabla de jugadores se copia hasta ocho veces por fotograma**, en instantes distintos (`online_players` en
  `:5777`, `:5957`, `:5968`, `:5975`, `:5983`, `:6003`, `:6012` y `:2820`): los subsistemas de un mismo fotograma
  pueden ver datos diferentes.
- **Las reglas de la sesión se refrescan dentro de `players_collide`, después de un `return` temprano**
  (`:2837-2840`): durante un evento, o con `SAVAGEWIND_ONLINE_NO_COLLIDE`, las reglas de revivir y de menús sin pausa
  se quedan con el valor anterior.

---

## 5. El patrón seguro de entrega entre hilos

Hay dos referencias. La primera está en este mismo repositorio y es la que se copia.

### 5.1 La cola de `runtime/items.c` (un productor cualquiera → el hilo del juego)

```
PRODUCTOR (cualquier hilo: red, interfaz)          CONSUMIDOR (hilo del juego, una vez por fotograma)
items_request            runtime/items.c:367       rt_item_tick              runtime/items.c:420
  lock(g_req_lock)                                   if (!atomic_load(&g_pending)) return;   // nada: coste casi nulo
  g_req[g_nreq++] = ...   (si cabe; si no, falla)     lock(g_req_lock)
  unlock                                               copiar la cola a variables locales; g_nreq = 0
  atomic_store(&g_pending, 1)                          atomic_store(&g_pending, 0)
                                                     unlock
                                                     // ya SIN cerrojo:
                                                     guardar el Cpu; llamar al juego; restaurar el Cpu
LIMPIEZA
  RT_ON_RESET(items_reset)  runtime/items.c:337      vacía la cola y el indicador al reiniciar
```

Las cuatro propiedades que hay que conservar al copiarlo:

1. **Cola de tamaño fijo con tope**: si está llena, el productor recibe un fallo; no crece, no bloquea
   (`runtime/items.c:330`, `370`).
2. **Indicador atómico** (`g_pending`): el consumidor no toma el cerrojo si no hay nada (`runtime/items.c:422`).
3. **Copiar bajo cerrojo, soltar, y solo entonces trabajar** (`runtime/items.c:423-429`). El cerrojo nunca está
   tomado durante una llamada al juego.
4. **`RT_ON_RESET`** que vacía la cola (`runtime/items.c:337-343`).

### 5.2 Dos colas, una por sentido (estilo "Anchor")

Es el esquema que usan otros mods cooperativos de juegos recompilados (por ejemplo Anchor, para Ship of Harkinian):
el hilo de red **solo** mete paquetes ya decodificados en una **cola de entrada**, y **solo** saca de una **cola de
salida**; el hilo del juego vacía la de entrada al principio del fotograma y llena la de salida al final. Los
detalles de esa implementación externa no se han leído para este trabajo `[sin verificar]`; aquí se toma solo la
forma:

```
            cola de ENTRADA (red → juego)                 cola de SALIDA (juego → red)
hilo de red:  recibir → validar → encolar                 desencolar → enviar
hilo juego:   al EMPEZAR el fotograma: vaciar y aplicar   al TERMINAR el fotograma: encolar lo propio
```

El PR ya se parece a esto (sección 3), con tres diferencias que hay que corregir: no valida antes de encolar, tiene
una cola distinta por tipo de mensaje repartida por el archivo, y el hilo de red además lee memoria del juego.

En este mismo repositorio hay otro ejemplo completo de ida y vuelta: `runtime/control.c` (un hilo con `select` que
solo lee y encola; los comandos se ejecutan en otro hilo; colas `requests` / `finished` con `queue_lock`,
`runtime/control.c:282-285`, `543-559`; y un contador de generación por conexión para descartar respuestas tardías,
`:623`).

---

## 6. Reglas para el código nuevo

Cada línea es comprobable leyendo el cambio. "DEBE" y "NO DEBE" son obligatorios.

### 6.1 Quién toca la memoria del juego

- **H-1.** Solo el código que corre dentro de `rt_online_frame` (o de otro gancho que corra en el hilo del juego)
  DEBE leer o escribir memoria del invitado.
- **H-2.** Un hilo de red (`cl_thread`, `srv_thread` o cualquiera nuevo) NO DEBE desreferenciar ninguna dirección
  del invitado ni usar `g_site.*` como puntero. Comprobación: en `runtime/online.c` no debe quedar ningún
  `*(… *)(uintptr_t)` sobre una dirección del juego.
- **H-3.** El código de interfaz (`runtime/ui/*.mm`) NO DEBE leer memoria del invitado; DEBE pedir el dato por una
  función de `runtime/online.h` que lo sirva desde una copia.
- **H-4.** NO DEBES llamar a `guest_call`, `call()`, `call_f()` ni a ninguna macro `FN_*` fuera del hilo del juego.
  `call_f` y `call_sret` usan una variable `static Cpu saved` (`runtime/online_actors.c:234`, `248`): no son
  reentrantes ni seguras entre hilos.
- **H-5.** Una función llamada desde un parche que puede ejecutarse en otro hilo del invitado (como
  `rt_online_arrow` o `rt_online_xlink_emit`) NO DEBE tocar estado compartido salvo para dejar un registro
  **completo** en una cola protegida (cerrojo, o escritura con número de secuencia que el lector comprueba).
- **H-6.** Los datos que el juego vaya a leer durante una llamada DEBEN estar en memoria del invitado (su pila o
  `hle_alloc`), no en la pila del anfitrión de máquina (`runtime/items.c:435-440`).

### 6.2 Qué hace un hilo de red

- **H-7.** Un hilo de red DEBE limitarse a: recibir bytes, trocear mensajes, validarlos
  (`contratos/validacion-red.md`), encolarlos; y desencolar y enviar.
- **H-8.** Un hilo de red NO DEBE guardar punteros del invitado, ni entre iteraciones ni en colas.
- **H-9.** Ningún envío DEBE poder bloquear indefinidamente a otros clientes (hoy `send_all`,
  `runtime/online.c:140-148`, lo hace). El código nuevo de envío DEBE usar cola por cliente y socket no bloqueante,
  o un plazo.

### 6.3 Cerrojos

- **H-10.** El hilo del juego NO DEBE esperar a la red: nada de `recv`, `send`, `select`, `connect`,
  `getaddrinfo` ni `pthread_join` dentro de `rt_online_frame`.
- **H-11.** NO DEBES mantener un cerrojo propio mientras llamas a una función del juego, haces una llamada al
  sistema del invitado o esperas. Patrón obligatorio: tomar cerrojo → copiar a variables locales → soltar cerrojo →
  trabajar. (Un reinicio desenrolla el hilo y dejaría el cerrojo tomado para siempre, `runtime/kernel.c:367-372`.)
- **H-12.** Una variable que leen dos hilos DEBE estar protegida por cerrojo o ser atómica (`_Atomic` /
  `atomic_*`). `volatile` NO es suficiente.
- **H-13.** Las funciones que hacen `pthread_join` (`stop_all`, y por tanto `online_host`, `online_join`,
  `online_leave`, `runtime/online.c:1322-1373`) NO DEBEN llamarse desde el hilo del juego.

### 6.4 Colas

- **H-14.** Toda cola DEBE tener tamaño fijo y un comportamiento definido cuando se llena (rechazar lo nuevo o
  descartar lo más viejo), escrito en un comentario junto a su declaración.
- **H-15.** Lo que entra en una cola red → juego DEBE estar ya validado. El consumidor NO DEBE volver a confiar en
  el emisor.
- **H-16.** El hilo del juego DEBE tomar **una sola** copia de la tabla de jugadores por fotograma y pasarla a todos
  los subsistemas. NO DEBES añadir otra llamada a `online_players` dentro de `rt_online_frame`.

### 6.5 Reinicio y fin de sesión

- **H-17.** Toda variable global o `static` (también las `static` **dentro** de funciones) que guarde un puntero del
  invitado, un handle, una cola o un estado de la partida DEBE limpiarse en una función registrada con
  `RT_ON_RESET`.
- **H-18.** Todo estado de sesión DEBE limpiarse además al cortar la sesión, en `stop_all`
  (`runtime/online.c:1322-1335`). Hoy sobreviven `g.quests`, `quests_have`, `quest_of[]`, `g.loots`, `g.en[]` y
  las colas pendientes de golpes, estados, botín y recogidas.
- **H-19.** Un módulo nuevo con estado propio DEBE aportar su función de limpieza; `forget_all`
  (`runtime/online_actors.c:316-336`) NO DEBE seguir creciendo con listas escritas a mano.
- **H-20.** NO DEBES añadir variables `static` dentro de funciones para guardar estado de sesión (temporizadores,
  "último valor", indicadores "ya hecho"). Van en una estructura con nombre que la limpieza pueda borrar.

### 6.6 Orden dentro del fotograma

- **H-21.** El orden de la sección 4 DEBE quedar escrito de forma explícita en `rt_online_frame` como una lista de
  llamadas. NO DEBES reordenar pasos en una tarea que no sea de reordenación.
- **H-22.** Antes de usar el actor de un jugador remoto (`g_act[i].link`) en un fotograma DEBES haber comprobado
  `alive(g_act[i].link, g_act[i].link_w)` (`runtime/online_actors.c:91`) **en ese mismo fotograma**. Regla práctica:
  la comprobación va antes del primer subsistema que lo use (hoy, antes del paso 17).
- **H-23.** Antes de llamar a `FN_BODY_TO` sobre un actor DEBES comprobar `has_body(actor)`
  (`runtime/online_actors.c:73`).
- **H-24.** Un subsistema nuevo DEBE respetar los `return` tempranos de los pasos 5, 10 y 11: NO DEBE crear ni
  tocar actores durante una carga, durante la espera `g_settle`, ni con el Link local muerto.
- **H-25.** NO DEBES llamar a `getenv` en código que se ejecuta cada fotograma; lee la variable una vez y guarda el
  resultado (macro `ENV()`, `runtime/hle.h:6`).

---

## 7. Errores típicos que no debes cometer

1. **Aplicar un mensaje al juego dentro de `cl_message`.** `cl_message` corre en el hilo de red
   (`runtime/online.c:967`): solo puede guardar el dato.
2. **Leer la posición de Link desde el hilo de red "porque solo es una lectura".** Es justo lo que hace hoy
   `sample_local` y lo que hay que eliminar. En Windows, leer una dirección obsoleta ni siquiera falla: reserva
   memoria en silencio (`runtime/platform/windows.c:85-104`).
3. **Usar un gancho "por fotograma presentado" para tocar el juego.** Esos ganchos (`control_frame` y similares,
   `runtime/nvn/vulkan/device.c:3343-3380`) corren en el hilo que presenta, no en el del juego.
4. **Llamar al juego con el cerrojo tomado.** Copia primero, suelta, y luego llama.
5. **Guardar `g_act[i].link` (o cualquier puntero de actor) en una `static` y usarlo al fotograma siguiente sin
   `alive()`.**
6. **Añadir una `static` dentro de una función para recordar algo de la sesión.** No se limpia al reiniciar ni al
   cambiar de sesión.
7. **Creer que `volatile` arregla una carrera.** No da atomicidad ni orden.
8. **Añadir "solo una llamada más" a `online_players`.** Cada una es una copia de ocho estructuras grandes tomada en
   otro instante.
9. **Meter un subsistema nuevo "donde quepa" en `rt_online_frame`.** Si usa actores remotos, va después de la
   comprobación de vida; si crea actores, va después de los `return` de carga y de muerte.
10. **Olvidar que en pausa hay dos actualizaciones por fotograma** y que el paso del juego no es fijo: no cuentes
    tiempo en "llamadas a `rt_online_frame`"; usa `plat_now_ns()`.
11. **Llamar a `online_leave` desde el hilo del juego.** Hace `pthread_join` y puede colgarlo.
12. **Dar por hecho qué hilo ejecuta un parche nuevo.** Compruébalo y anótalo; si no lo has comprobado, escribe
    `[sin verificar]`.
