# CU-01 · Hospedar una partida y que el otro se una por IP

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Un jugador crea una sesión en su PC y el otro entra escribiendo la dirección IP, el puerto y, si la hay, la
contraseña. Es la base de todo lo demás y ya existe en el PR #294: en F1 se prueba y se arreglan sus fallos rápidos.

## Actores

- **Anfitrión**: el jugador que crea la sesión. Su juego corre el servidor y un cliente conectado a sí mismo.
- **Invitado**: el jugador que se une.

## Precondiciones

- Los dos tienen el mismo build del fork y el juego en versión 1.9.0.
- Los dos tienen generada la carpeta `online_mod/` (se genera sola en el primer arranque, DOC:29-39).
- Los dos están dentro del juego (la pestaña Online no existe en la pantalla de inicio: `runtime/ui/menu.mm:2242-2248`).
- El invitado puede alcanzar el puerto TCP del anfitrión (misma red, puerto redirigido o red virtual; DOC:13-14).

## Flujo principal

1. El anfitrión abre el menú (F1) → pestaña **Online**, escribe su nombre, deja el puerto 52520 y, si quiere, una
   contraseña.
2. Pulsa **Create session**. La descripción del botón le muestra las direcciones IPv4 de su PC.
3. El anfitrión le pasa al invitado la dirección, el puerto y la contraseña por fuera del juego.
4. El invitado abre la pestaña Online, escribe su nombre, la dirección en **Host address**, el mismo puerto y la
   contraseña, y pulsa **Join session**.
5. En pocos segundos el estado pasa a conectado. Los dos ven el aviso "se unió" y la lista de jugadores.
6. Para terminar, el invitado pulsa **Leave session** o el anfitrión **End session**.

## Flujos alternativos y de error

| Situación | Qué pasa hoy | Evidencia |
|---|---|---|
| Puerto ocupado al hospedar | Estado fallido: "Can't host on port N (in use?)" | NET:1350-1355 |
| Dirección vacía al unirse | "Type the host's address first" | NET:1366 |
| Versión de protocolo distinta | El servidor rechaza con `R_VERSION` → "Another version of Savage Wind". Solo compara el número de protocolo (38) | NET:240, NET:975-983 |
| Contraseña incorrecta | Rechazo `R_PASSWORD`. Sin límite de intentos | NET:241, mapa 06 H-3 |
| Sesión llena (8) | Rechazo `R_FULL` en el `accept` | NET:364-367 |
| El puerto acepta pero nadie contesta | 10 s en "conectando" y luego "Lost the connection" | NET:1196-1199 |
| Pulsar Join o Leave mientras conecta | La pantalla se congela hasta 6 s por dirección: `stop_all` espera al hilo en el hilo de UI | NET:1322-1335, mapa 06 §5.2 |
| El anfitrión termina la sesión | El invitado ve "Disconnected by the server". No hay aviso previo ni reintento | NET:1318 |
| Distinta versión del juego o del `online_mod` | **No se comprueba**. Ver CU-03 (puerta de versión) | OA:225-229, `runtime/main.c:507` |

## Estado actual en el PR: **EXISTE**

- Crear sesión: `online_host()` abre el servidor, lanza `srv_thread` y un cliente contra `127.0.0.1` (NET:1347-1361).
- Unirse: `online_join()` (NET:1363-1368). Atajos de prueba: `SAVAGEWIND_ONLINE=host` o `=<dirección>`
  (`runtime/main.c:525-529`).
- Saludo HELLO → WELCOME / REJECT (NET:235-261 en el servidor, NET:971-983 en el cliente).
- Pestaña con Nombre, Puerto, Contraseña, Dirección y botones (UI:38-102). Puerto por defecto 52520 (NETH:18).
- Ajustes guardados en `online.ini` (NET:1506-1554).
- Problemas conocidos que afectan a este caso (mapa 06 §4 y §6):
  - La contraseña viaja y se guarda en claro y se ve en pantalla (H-3; UI:64-66).
  - Solo se muestran direcciones IPv4 del PC (NET:1464 y siguientes), aunque el servidor escucha también en IPv6.
  - Los textos de Online solo están traducidos al español; los otros 12 idiomas no tienen ninguno (mapa 06 §6.3).
  - `docs/ONLINE.md` está desfasado en detalles: dice "protocol 9" (DOC:52) y "tab 10" (DOC:46).

## Qué hay que construir

En F1 (probar y arreglos rápidos del plan):

1. Recorrer este flujo con el montaje común y anotar cada fallo en `bitacora/`.
2. Ocultar la contraseña en pantalla (campo enmascarado en UI:64-66).
3. Limpiar todo el estado de sesión en `stop_all` (NET:1322-1335; detalle en CU-03).

Más adelante (no bloquea este caso):

4. F3: comprobación estricta de versión en el HELLO (protocolo + compilación del fork + versión del juego + versión
   del `online_mod`), con un mensaje de error distinto para cada motivo.
5. F3: no bloquear la interfaz al conectar o salir (mover `stop_all` fuera del hilo de UI o hacerlo asíncrono).
6. F3: límite de intentos de contraseña y contraseña no guardada en claro.
7. Traducir los textos nuevos y pasar `tools/i18n_check.py`.

## Reglas y decisiones de diseño

- **[DUEÑO] D7**: conexión directa; un jugador hospeda. No hay servidor central ni cuentas.
- **[DUEÑO] D1**: la sesión objetivo es de 2 jugadores. El límite técnico actual es 8 (`ONLINE_MAX`, NETH:17).
- **[PROPUESTA]** Limitar la sesión a 2 jugadores por ahora (opción del anfitrión), para simplificar las pruebas y
  la propiedad de enemigos (mapa 07 §7).
- **[PROPUESTA]** El servidor dedicado (`--server`) queda fuera de alcance: hoy no tiene anfitrión con autoridad
  y el mundo compartido queda inerte (mapa 06 §1.1).

## Criterios de aceptación

- **CA-1** Dado el montaje común, cuando arrancan los dos juegos, entonces en menos de 15 s el registro de A
  contiene `[online] hosting on port 52530` y cada juego lista 2 jugadores en la pestaña Online.
- **CA-2** Dado B conectado, cuando se ejecuta `swctl.py --port 7778 actors OnlinePlayer` con A a menos de 400 m y en
  el mismo mapa, entonces aparece un actor `OnlinePlayer` (enlaza con CU-02).
- **CA-3** Dado un anfitrión con contraseña `x`, cuando un cliente entra con otra contraseña, entonces el cliente
  queda en estado fallido con el texto de contraseña incorrecta y el anfitrión no lo lista. Se puede probar con el
  bot [sin verificar: el bot no tiene parámetro de contraseña documentado; puede hacer falta añadirlo].
- **CA-4** Dado un cliente con número de protocolo distinto (bot recompilado con otra versión), cuando se conecta,
  entonces recibe el rechazo de versión y la sesión sigue funcionando para los demás.
- **CA-5** Dado el campo Contraseña, cuando se escribe, entonces no se lee en pantalla (prueba visual a mano).
- **CA-6** Dado A y B conectados, cuando A pulsa **End session**, entonces B muestra el aviso de desconexión en
  menos de 2 s y ninguno de los dos juegos se cuelga.

## Qué NO hacer

- No cambiar el número de protocolo ni la estructura del HELLO (`runtime/online.h`, `runtime/online.c:105`) en F1:
  eso es de F3 y tiene dueño único.
- No llamar a `online_host`, `online_join` ni `online_leave` desde el hilo del juego o el de red: hoy solo desde la
  interfaz y el arranque (mapa 06 T-6).
- No guardar ni registrar la contraseña en ningún sitio nuevo (ni en `LOG`).
- No usar el puerto 52520 ni el guardado real en las pruebas: puerto 52530 y `SAVAGEWIND_SAVE_DIR` con una copia.
- No añadir texto visible sin su traducción y sin pasar `tools/i18n_check.py`.
- No tocar `runtime/ui/menu.mm` (id de pestaña 13, tablas `names[]` / `order[]`) salvo el dueño de ese fichero.
- No arreglar aquí el servidor dedicado (`--server`): está fuera de alcance.

## Riesgos y preguntas abiertas

- Riesgo: sin puerta de versión, un jugador con otra versión del juego salta a direcciones equivocadas (mapa 07 A1).
- Riesgo: el borrado del CI en la rama del PR (mapa 06 §8.4) hace que nada compruebe estos flujos automáticamente
  hasta restaurarlo en F0.
- **Pregunta al dueño**: ¿la sesión se limita a 2 jugadores desde el principio o se deja abierta a 8 como está?
- **Pregunta al dueño**: ¿contraseña obligatoria cuando el puerto está abierto a Internet, o sigue siendo opcional?

## Fase y dependencias

- **Fase**: F1 (probar y arreglos rápidos). Mejoras de versión y contraseña en F3.
- **Depende de**: F0 (compilar, generar `online_mod`, dos carpetas de guardado).
- **Lo necesitan**: todos los demás casos.
