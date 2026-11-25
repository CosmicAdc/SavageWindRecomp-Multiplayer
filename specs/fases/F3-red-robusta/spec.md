# F3 — Red robusta: especificación

Estado: borrador para revisión del dueño. Depende de F1 (arreglos rápidos) y F2 (cimientos) terminadas.

Convenciones de este documento y de los otros tres de la fase:

- Las referencias `fichero:línea` son del checkout del PR #294 (`pr294-online`, rama `pr-294-online`, HEAD `350c5f8`).
  Tras la partición de F2 las líneas se mueven: se busca por nombre de función.
- `OA:n` = `runtime/online_actors.c` línea n. `DOC:n` = `docs/ONLINE.md` línea n.
- "[sin verificar]" = no se pudo comprobar leyendo el código o los mapas; hay que comprobarlo antes de construir encima.
- Nada de esta fase necesita ingeniería inversa del juego. Todo es código de red, UI y pruebas.

## 1. Objetivo

Que dos jugadores puedan encontrarse, conectarse y seguir conectados sin conocimientos de redes, y que un par lento,
caído o malicioso no pueda congelar la sesión ni cambiar el mundo del anfitrión.

En una frase por bloque:

1. **No congelarse**: ningún envío bloquea al hilo servidor ni al hilo de UI.
2. **Autoridad**: el servidor sabe quién es el anfitrión y solo él puede enviar el estado del mundo.
3. **Entrada controlada**: límites de tasa, de conexiones sin autenticar y de intentos de contraseña; la contraseña no
   se guarda ni viaja en claro.
4. **Misma versión o nada**: se rechaza con un mensaje claro a quien tenga otro protocolo, otra compilación, otra
   versión del juego u otro estado de DLC.
5. **Encontrarse**: descubrimiento en red local, código de invitación, apertura automática de puerto con mensajes de
   alternativa, IPv6.
6. **Volver**: reconexión automática que conserva la ranura y la sesión.
7. **Ver cómo va la red**: indicador de latencia y pérdida en pantalla.
8. **Decidir el transporte con datos**: TCP actual frente a UDP con ENet, medido en Docker; decisión en un ADR.
9. **Idiomas**: los 12 idiomas que hoy no tienen ningún texto de Online.

## 2. Casos de uso que cubre

| CU | Qué aporta F3 |
|---|---|
| CU-03 (30 min sin crashes) | La parte de red: sin bloqueo global (H-1), sin agotamiento de ranuras (H-4), sin amplificación (H-5), sin suplantación del anfitrión (H-2a) |
| CU-04 (encontrar la partida) | Lista de partidas en LAN, código de invitación, UPnP/NAT-PMP con mensajes de alternativa, IPv6, guía de Tailscale/ZeroTier |
| CU-05 (reconectar) | Reintento automático con reanudación de sesión: misma ranura, mismo nombre, sin volver a escribir nada |

Prepara además a CU-06 y CU-12 (F4): sin autoridad en el servidor no se puede hacer "mundo del anfitrión" ni "daño
validado".

## 3. Alcance

### 3.1 Dentro

| # | Elemento | Comportamiento actual (verificado) |
|---|---|---|
| A | Colas de envío por cliente, no bloqueantes, con contrapresión y plazos | `send_all` bloquea hasta enviar todo (`runtime/online.c:140-148`); `srv_broadcast` lo llama para cada cliente en el único hilo servidor (`:213-216`); un par que no lee congela a todos y `stop_all` se queda en `pthread_join` (`:1322-1335`) |
| B | Autoridad en el servidor | El servidor reenvía ENEMIES, LOOTS, QUESTS y PICK de cualquiera sin mirar (`online.c:278-298`); el cliente anfitrión acepta QUEST de cualquier ranura válida (`:1065-1070`); no existe campo "host" en `Server`/`SClient` (`:194-211`) |
| C | Límite de tasa, de conexiones sin autenticar y de intentos de contraseña | La ranura se ocupa en el `accept`, antes del HELLO (`online.c:359-370`); no hay plazo para el HELLO; no hay contador de tasa ni de intentos |
| D | Contraseña | Viaja en claro en HELLO (`online.c:105`), se compara con `strcmp` (`:241`), se guarda en claro en `online.ini` (`:1549-1551`) |
| E | Comprobación estricta de versión | Solo `magic` y `PROTOCOL 38` (`online.c:39-40`, `:240`). No se compara la compilación, la versión del juego, la del overlay `online_mod` ni el DLC |
| F | Descubrimiento en LAN | No existe (no hay UDP en `online.c`) |
| G | Código de invitación | No existe; se teclea IP y puerto |
| H | UPnP / NAT-PMP | No existe; la UI pide redirigir el puerto o usar una VPN (`runtime/ui/online_ui.mm:57-59`) |
| I | IPv6 | El servidor ya escucha en doble pila (`online.c:318-333`) y el cliente usa `AF_UNSPEC` (`:1131`); la lista de direcciones propias solo da IPv4 (`:1462-1480`) y no se comprueba `setsockopt(IPV6_V6ONLY)` |
| J | Reconexión con reanudación | Tras un fallo el cliente queda en `ONLINE_FAILED` (`online.c:1198-1200`, `:1318`); quien vuelve es un jugador nuevo |
| K | Indicador de calidad de red | Solo la latencia propia (PING/PONG 1 Hz, `online.c:1292-1295`) en la lista de jugadores |
| L | Decisión TCP frente a ENet/UDP | TCP único, todo fiable, bloqueo de cabeza de línea; la marca de tiempo de cada instantánea es la hora de recepción (`online.c:910`) |
| M | Traducciones | 63 textos solo en `runtime/ui/i18n/es.inc`; los otros 12 idiomas, 0 (comprobado con `grep -c "Create session"`) |

### 3.2 Fuera

- Cifrado del tráfico (TLS/DTLS/Noise). Se documenta el riesgo y se deja para un ADR posterior.
- Servidor de encuentro (rendezvous) y perforación de NAT. Queda como último peldaño de la escalera, fuera de F3.
- Migración de anfitrión: si el anfitrión se va, la sesión termina.
- Servidor dedicado con mundo compartido (`--server`): hoy no tiene autoridad (nadie es `g.hosting`). En F3 solo se
  documenta y se marca como "solo relé de posiciones"; no se arregla.
- Compresión por deltas y filtrado por área (F5, escalado a N jugadores).
- Cambiar el transporte en el juego. F3 **mide y decide**; la migración, si se aprueba, es una fase propia.
- Cualquier cambio en `tools/patches.py` o `tools/game_sites.py` (F3 no debe cambiar `GAMELIB_ID`).

## 4. Requisitos

Cada requisito tiene un identificador que usan `tasks.md` y `verificacion.md`.

### Envío y contrapresión

- **R3-01** Ninguna llamada de envío del hilo servidor puede bloquear más de 0 ms: sockets no bloqueantes y cola de
  salida por cliente.
- **R3-02** Los mensajes de estado continuo (STATE, SKEL, ENEMIES) son **reemplazables**: si el cliente destino va
  atrasado, se conserva solo el último de cada origen. Los eventos (JOIN, LEAVE, CHAT, HIT, LOOT_*, QUEST*, PICK) son
  **fiables**: nunca se descartan en silencio.
- **R3-03** Un cliente cuya cola fiable supera su tope, o que no acepta ni un byte durante el plazo de escritura, se
  expulsa con LEAVE para los demás. Los demás no notan nada.
- **R3-04** "Terminar sesión" y "Salir" devuelven el control a la UI en menos de 1 s en cualquier estado de la red.
- **R3-05** El cliente tampoco bloquea: su hilo de red usa cola de salida, y `online_join` nunca espera a
  `getaddrinfo`/`connect` en el hilo de UI.

### Autoridad y validación

- **R3-06** El servidor conoce la ranura del anfitrión. El cliente del anfitrión se identifica con un secreto de un
  solo uso creado en `online_host` y nunca enviado fuera del proceso.
- **R3-07** Tabla de autoridad por tipo de mensaje aplicada en el servidor (quién puede enviar, a quién se reenvía).
  Un mensaje que la incumple se descarta y cuenta como infracción.
- **R3-08** Todos los mensajes pasan por la validación única de F2 (longitudes, contadores, cadenas terminadas,
  números finitos y acotados) **también en el servidor**, antes de reenviar.
- **R3-09** El cliente comprueba que ENEMIES/LOOTS/QUESTS/TIME vienen marcados por el servidor como del anfitrión y
  rechaza un segundo WELCOME.

### Límites

- **R3-10** Cubo de fichas por cliente y por tipo de mensaje. Superarlo descarta el mensaje; reincidir expulsa.
- **R3-11** La ranura de jugador se asigna **después** de un HELLO válido. Las conexiones sin autenticar tienen cupo
  propio, plazo para el HELLO y tope por dirección IP.
- **R3-12** Intentos de contraseña: tope por IP con espera creciente. El rechazo no revela si falló la versión o la
  contraseña hasta haber pasado la comprobación de versión.
- **R3-13** La contraseña no se guarda en claro en `online.ini` ni viaja en claro: desafío-respuesta con un valor
  aleatorio del servidor. Limitación aceptada y documentada: sin cifrado del canal, quien escuche el tráfico ve el
  resto de la sesión.

### Versión

- **R3-14** El HELLO lleva: protocolo, identificador de compilación del código del juego (`GAMELIB_ID`), versión del
  juego (`g_game_version`), versión del overlay `online_mod`, estado de DLC y versión del propio modo cooperativo.
- **R3-15** Cualquier diferencia rechaza la conexión con un motivo específico y un texto que dice qué difiere y en
  qué lado.

### Encontrarse

- **R3-16** En la pestaña Online, "Buscar en red local" lista las partidas de la LAN en menos de 2 s con nombre del
  anfitrión, jugadores, si pide contraseña y si la versión coincide.
- **R3-17** "Crear sesión" muestra un código de invitación que el otro pega en "Unirse". El código solo contiene
  dirección y puerto (nunca la contraseña).
- **R3-18** "Crear sesión" intenta abrir el puerto con UPnP y, si falla, NAT-PMP/PCP, en un hilo aparte con plazo.
  El resultado se muestra con un texto concreto y la alternativa siguiente de la escalera.
- **R3-19** IPv6 funciona de extremo a extremo: direcciones propias listadas, entrada `[dirección]:puerto`, código de
  invitación con IPv6.
- **R3-20** Ni DNS, ni UPnP, ni el descubrimiento se ejecutan en el hilo del juego ni en el de presentación.

### Reconexión

- **R3-21** Si la conexión cae, el cliente reintenta solo con espera creciente durante un plazo configurable; la UI
  muestra "Reconectando…" y permite cancelar.
- **R3-22** El servidor reserva la ranura del caído durante el plazo de gracia; al volver con su testigo de sesión
  recupera ranura y nombre y recibe de nuevo el estado inicial.
- **R3-23** Durante la gracia, los demás ven al jugador como "reconectando" y sus actores no se destruyen y recrean.
- **R3-24** Una reconexión nunca aplica estado de una sesión anterior (depende de la limpieza de `stop_all` de F1).

### Calidad de red

- **R3-25** Indicador en pantalla por jugador: latencia (ms), pérdida efectiva (%) y nivel (bueno/regular/malo), con
  forma además de color.
- **R3-26** El STATE lleva número de secuencia y hora del emisor; la interpolación usa la hora del emisor.

### Transporte

- **R3-27** Existe un experimento reproducible en Docker que mide TCP actual y un prototipo ENet con los mismos
  mensajes bajo latencia y pérdida simuladas, y un ADR con la decisión firmada por el dueño.

### Idiomas

- **R3-28** `tools/i18n_check.py` sale con 0: los 13 idiomas tienen todos los textos de Online, viejos y nuevos.

## 5. Criterios de salida (medibles)

| # | Criterio | Cómo se mide |
|---|---|---|
| S1 | Un bot que deja de leer no afecta a los demás | Prueba `h1_noread` en Docker: el intervalo máximo entre STATE recibidos por el segundo bot no supera 150 ms y el bot que no lee es expulsado antes de 15 s |
| S2 | "Terminar sesión" con un par colgado | Vuelve en menos de 1 s (medido en la prueba y a mano) |
| S3 | Suplantación del anfitrión | Un bot invitado que envía ENEMIES, LOOTS, QUESTS: 0 mensajes llegan al otro cliente; el servidor los cuenta como infracción |
| S4 | QUEST con todos los bits a 1 desde un invitado | El servidor lo limita por tasa y tamaño; la política de flags es de F4, aquí basta con que no pase más de 1 cada 2 s y con longitud exacta |
| S5 | Agotamiento previo a autenticación | 20 conexiones que no envían HELLO: un jugador legítimo entra en menos de 2 s |
| S6 | Contraseña | 5 intentos fallidos desde una IP bloquean esa IP el tiempo configurado; `online.ini` no contiene la contraseña en claro (se busca la cadena) |
| S7 | Versión | Matriz de 5 diferencias (protocolo, compilación, juego, overlay, DLC): cada una da su motivo y su texto |
| S8 | LAN | En Docker, con anfitrión y cliente en la misma red, el cliente lista la partida en menos de 2 s; en redes distintas, no |
| S9 | Código de invitación | Ida y vuelta IPv4 e IPv6 en prueba unitaria; un código con una letra cambiada se rechaza por la suma de control |
| S10 | UPnP | Contra `miniupnpd` en Docker: abre y cierra la redirección; sin IGD: mensaje de alternativa en menos de 5 s y el juego no se detiene (sin saltos de frame mayores que la línea base) |
| S11 | Reconexión | Corte de 10 s con `tc netem loss 100%`: el cliente vuelve solo, misma ranura; el otro jugador ve "reconectando" y luego al compañero, sin crash |
| S12 | Calidad de red | Con `netem delay 80ms loss 2%` el indicador muestra 150-190 ms de ida y vuelta y pérdida efectiva distinta de 0 |
| S13 | ADR de transporte | `specs/decisiones/` contiene el ADR con la tabla de medidas y la decisión |
| S14 | Idiomas | `tools/i18n_check.py` → `0 missing` en los 13 |
| S15 | Regresión | El recorrido de dos instancias de F1 (CU-01 a CU-03, 30 min) sigue pasando; resultado en `bitacora/` |
| S16 | Compila en los tres sistemas | `make runtime-objs` en Windows (MSYS2) y Linux (Docker); macOS por CI si está habilitado en el fork |

## 6. Riesgos

| # | Riesgo | Mitigación |
|---|---|---|
| 1 | Todo el trabajo de autoridad, colas, saludo y reconexión toca `runtime/online.c` | Un solo carril ("núcleo") con dueño único de ese fichero; la lógica nueva va en ficheros nuevos y puros que el núcleo solo llama |
| 2 | Descartar STATE viejos cambia lo que ve el interpolador (anillo de 8 instantáneas, 100 ms de retraso) | R3-26 (hora del emisor) antes de activar el descarte; medir saltos en el experimento |
| 3 | UPnP falla en muchos routers (CGNAT, IGD apagado, doble NAT) | Es un peldaño, no la solución: mensajes claros y siguiente alternativa; nunca bloquear "Crear sesión" por ello |
| 4 | El aviso del cortafuegos de Windows aparece al escuchar por primera vez y, si se niega, nadie de fuera entra | Texto de ayuda en la pestaña y en la guía; prueba manual obligatoria en la verificación |
| 5 | Comparar `GAMELIB_ID` exacto puede rechazar a dos jugadores legítimos con CPU distinta (existe `GAMELIB_ID_COMPAT` para CPU sin AVX2/FMA, `runtime/gamelib.c:43-59`) | Tarea de investigación previa: qué identificador es igual entre dos compilaciones del mismo código fuente |
| 6 | Una dependencia nueva (ENet, miniupnpc) cuesta mucho en este proyecto: tres plataformas, `tools/setup_windows.ps1`, flujos de CI y `docs/third-party.md` | Preferir código propio pequeño o fuente incluida en `third_party/` con licencia permisiva; decidir en los ADR |
| 7 | STATE mide 3375 bytes: no cabe en un datagrama de ~1200 bytes | Si el ADR elige UDP, el plan debe incluir partir o reducir STATE; por eso F3 solo decide |
| 8 | La reconexión aplica estado viejo (hallazgo T-1 del mapa 06) | Requisito previo: limpieza completa en `stop_all` (F1) y prueba específica |
| 9 | El descubrimiento LAN abre un puerto UDP que responde a cualquiera de la red local | Respuesta de tamaño fijo, sin datos sensibles, con límite de tasa; se puede apagar |
| 10 | Dos instancias en un PC comparten `online.ini` (se abre en el directorio de trabajo, `online.c:1506`) | Las pruebas usan `SAVAGEWIND_ONLINE_NAME` (que evita guardar, `online.c:1544-1545`) o raíces de datos distintas |
| 11 | Reservar la ranura 0 al anfitrión cambia un supuesto de `online_actors.c` (las reglas se leen de `pl[0]`, OA:2838-2840) | Hoy el supuesto ya existe sin garantía; F3 lo convierte en garantía y lo prueba |

## 7. Preguntas abiertas

| # | Pregunta | Quién | Bloquea |
|---|---|---|---|
| P3-1 | ¿Plazo de gracia de reconexión? Propuesta: 60 s | Dueño | T317-T319 (valor por defecto) |
| P3-2 | ¿La contraseña del invitado se recuerda entre sesiones? Propuesta: no, salvo casilla "recordar" | Dueño | T331 |
| P3-3 | ¿Se comparan también los mods cargados (lista o suma) al conectar? | Dueño | T325 (campo opcional) |
| P3-4 | ¿Qué hacer con `--server`? Propuesta: dejarlo como relé sin mundo y decirlo en la ayuda | Dueño | T339 |
| P3-5 | ¿Se acepta incluir código de terceros (ENet, miniupnpc) en `third_party/` si los spikes lo recomiendan? | Dueño | T305, T306 |
