# CU-12 · PvP activable por acuerdo, con daño validado

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Por defecto los jugadores no pueden hacerse daño. Si los dos lo aceptan y el anfitrión no lo ha prohibido, se
activa el combate entre ellos, solo con armas cuerpo a cuerpo y bombas. El daño lo **valida el anfitrión** antes de
que la víctima lo aplique, para que un cliente defectuoso o manipulado no pueda matar de un golpe (ADR-008,
decisión 17). Hoy existe un PvP básico: cada uno tiene su interruptor y el daño lo declara el atacante sin
validación.

## Actores

- **Atacante** y **víctima** (los dos jugadores, en ambos papeles).
- **Anfitrión**: puede prohibir el PvP en su sesión y es quien valida el daño (ADR-008, decisión 17).

## Precondiciones

- Sesión conectada con dos jugadores, en el mismo mapa.
- PvP apagado por defecto (`g_online.pvp` no está en los valores iniciales: NET:110; interruptor en UI:110-112).

## Flujo principal

1. Un jugador elige **Proponer duelo** en la pestaña Online.
2. Al otro le aparece "<nombre> propone activar el daño entre jugadores. ¿Aceptar?".
3. Si acepta, los dos ven "PvP activado" y un indicador permanente junto al nombre del otro (CU-10).
4. Los golpes de arma cuerpo a cuerpo y las bombas remotas del otro hacen daño, reducido por la armadura; el escudo
   en guardia bloquea.
5. Cualquiera de los dos pulsa **Terminar duelo**: el PvP se apaga para los dos al instante, sin pedir permiso.

## Flujos alternativos y de error

| Situación | Comportamiento esperado | Hoy |
|---|---|---|
| El otro rechaza o no responde en 20 s | Sigue apagado; se avisa a quien propuso | No hay propuesta: cada uno activa su interruptor |
| Solo uno lo tiene activado | No hay daño en ningún sentido | Así es: el atacante solo envía si lo tiene activo (OA:2881) y la víctima solo aplica si lo tiene activo (OA:2913) |
| Víctima en otro mapa | Sin daño | El atacante exige `same_map` (OA:2892-2893); la víctima **no** lo comprueba |
| Víctima cargando, en pausa, en menú o en escena | Sin daño [PROPUESTA] | El atacante descarta al que carga (OA:2893). Con "menús sin pausa" la víctima en menú sí recibe el golpe |
| Víctima ya muerta o caída | Sin daño | `*life <= 0` se descarta (OA:2913) |
| Víctima en guardia con escudo | Bloqueado | AS con `Guard` (OA:2914-2918) |
| Daño declarado negativo o cero | Se ignora | `pvp_damage <= 0` se descarta (OA:2913) |
| Daño declarado enorme | Se recorta a lo que puede hacer el arma que lleva el atacante | **Se acepta tal cual** hasta 32 767 (`int16_t`, NETH:107) menos la defensa (OA:2919-2920) |
| Golpe declarado desde lejos | Se ignora | La víctima no comprueba distancia |
| Ráfaga de golpes imposibles | Límite de golpes por segundo | Sin límite |
| Muerte por PvP | Aplica la regla de revivir (CU-11); el PvP se apaga solo al morir [PROPUESTA] | Reanimación por hada si la regla está activa |
| Desconexión o cambio de sesión | El PvP vuelve a apagado | El interruptor se guarda en `online.ini` y persiste (NET:1522, NET:1549-1552) |
| El anfitrión prohíbe el PvP | No se puede proponer | No existe esa regla |
| Flechas y runas | Sin daño entre jugadores: fuera de la primera entrega (ADR-008, decisión 17) | Así es: "Sin flechas PvP" (mapa 07 §2.2) |

## Estado actual en el PR: **PARCIAL**

- Función `pvp_update` (OA:2874-2942), llamada desde `rt_online_frame` (OA:5960).
- **Atacante** (OA:2880-2905): al empezar un golpe (estela del arma, `g_fx_trail_t`), elige al jugador más cercano
  dentro del alcance (2,2 m + 0,8 m, o + 1,5 m con lanza o mandoble) y delante; publica `pvp_seq`, `pvp_target` y
  `pvp_damage` = poder de ataque del arma según `power.txt` (`attack_power`). Viajan dentro del estado (NETH:105-107;
  NET:864, NET:937).
- **Víctima** (OA:2906-2928): si cambia `pvp_seq`, el objetivo es ella, tiene PvP activo, el daño es positivo y está
  viva: bloquea si su AS contiene `Guard`; si no, resta `daño − defensa` (mínimo 1) de su vida en `player + 0x848`
  (`our_defense`, OA:2675) y se estremece con `OnlineShock`.
- **Bombas** (OA:2929-2941): bomba ajena a menos de 4 m con PvP activo: 8 cuartos menos la defensa y derribo con
  `OnlineKnock`. Probado por los autores (DOC:806-808).
- Interruptor por jugador "Damage between players", apagado por defecto, guardado en `online.ini`; también
  `SAVAGEWIND_ONLINE_PVP` para pruebas (NET:1537-1538).
- **No hay** propuesta ni aceptación, ni indicador de quién tiene PvP activo (el interruptor no viaja en el estado),
  ni regla del anfitrión, ni validación del daño, la distancia o el ritmo (mapa 06 H-2, H-6; mapa 07 §2.2).
- El servidor reenvía el estado sin mirarlo (mapa 06 §2.4).

## Qué hay que construir

Todo el trabajo de juego va en `pvp_update` (futuro módulo `oa_rules`, mapa 07 §8.1).

1. **Validación del daño por el anfitrión** (ADR-008, decisión 17). Hoy el daño lo declara el atacante y el
   servidor reenvía el estado sin mirarlo; debe pasar a comprobarlo el anfitrión, que conoce el arma, la posición
   y el mapa de los dos jugadores, y solo un golpe validado llega a la víctima. La víctima sigue siendo quien resta
   la vida, en su juego (D4), antes de OA:2919. Cómo viaja el golpe validado (el estado actual o un mensaje nuevo)
   lo decide el dueño de `online.h`; si cambia el protocolo, sube su versión y se repite en el bot. Las
   comprobaciones son estas; la víctima conserva las de *estado propio*, que solo ella conoce:
   - *Daño máximo*: recalcular el poder con el arma que el atacante lleva según su EQUIP (`pl[i].gear[0]`, NETH:143)
     y `attack_power()`; usar `min(declarado, recalculado)`. Si no lleva arma, daño 0.
   - *Distancia*: la posición recibida del atacante (`pl[i].pos`) debe estar dentro del alcance + un margen por
     latencia (propuesta: alcance + 2 m).
   - *Mapa*: `same_map(&pl[i])` (OA:1484-1486).
   - *Ritmo*: como mucho N golpes por segundo por atacante (propuesta: 4).
   - *Estado propio*: sin daño si estoy cargando, en evento (`framepace_in_event`) o en los fotogramas de espera tras
     revivir.
   - Cada rechazo se registra con su motivo.
2. **Acuerdo explícito**: mensajes nuevos `PVP_ASK`, `PVP_ANSWER`, `PVP_END` (cambia el protocolo). El estado "PvP
   activo con el jugador X" vive solo en memoria de la sesión, **no** en `online.ini`, y se borra en `stop_all`
   (NET:1322-1335).
3. **Indicador**: bit "PvP activo" en el estado para que el overlay lo muestre junto al nombre (CU-10).
4. **Regla del anfitrión** "permitir PvP" junto a `collide` / `revive` / `nopause` (`OnlineSettings`, NETH:148-159;
   viaja en el estado del anfitrión como las otras: OA:2838-2840).
5. **Interfaz**: sustituir el interruptor (UI:110-112) por "Proponer duelo" / "Terminar duelo" y el diálogo de
   aceptación en el overlay. Traducciones.
6. Mantener `SAVAGEWIND_ONLINE_PVP` como atajo de pruebas que activa el acuerdo sin diálogo.
7. Bombas: aplicar las mismas comprobaciones de mapa, estado y ritmo a OA:2929-2941.

## Reglas y decisiones de diseño

- **[DUEÑO] D8**: cooperativo por defecto; el PvP se activa solo si los dos lo aceptan.
- **[DUEÑO] D4**: los corazones son de cada uno: el daño se aplica en el juego de la víctima.
- **[DUEÑO] (ADR-008, decisión 17; Q-12-3)** En el PvP solo cuentan los ataques **cuerpo a cuerpo y las bombas**.
  Flechas y runas quedan fuera de la primera entrega.
- **[DUEÑO] (ADR-008, decisión 17; Q-12-1)** El anfitrión **puede prohibir el PvP** en su sesión.
- **[DUEÑO] (ADR-008, decisión 17, consecuencias)** El daño de PvP lo **valida el anfitrión** (hoy lo declara el
  atacante). El alcance de las armas no cambia.
- **[PROPUESTA]** Cualquiera puede terminar el duelo en cualquier momento sin permiso del otro.
- **[PROPUESTA]** El acuerdo no se recuerda entre sesiones ni tras una reconexión.
- **[PROPUESTA]** Al morir en un duelo, el PvP se apaga y se aplica CU-11.

## Criterios de aceptación

- **CA-1** Dado el montaje común sin acuerdo de PvP, cuando A golpea a B con un arma a 1 m, entonces la vida de B no
  cambia y el registro de B no contiene `hit us`.
- **CA-2** Dado solo A con PvP activo, cuando A golpea a B, entonces la vida de B no cambia.
- **CA-3** Dado el acuerdo activo (`SAVAGEWIND_ONLINE_PVP=1` en los dos), cuando A golpea a B a 1 m con un arma de
  poder P y B tiene defensa D, entonces B pierde `max(P − D, 1)` cuartos y su registro lo dice
  (`hit us: … after our defence …`).
- **CA-4** Dado el acuerdo activo y un bot en el papel de atacante que declara `pvp_damage = 30000` con un arma cuyo
  poder en `power.txt` es P, entonces el anfitrión lo acota y la víctima pierde como mucho `max(P − D, 1)`. (Requiere ampliar el bot para
  enviar `pvp_*` y EQUIP elegido.)
- **CA-5** Dado un bot atacante a 100 m de la víctima, entonces la vida no cambia y el registro del anfitrión dice
  "rechazado: distancia".
- **CA-6** Dado un bot que declara 50 golpes en un segundo, entonces se aplican como mucho los permitidos por el
  límite.
- **CA-7** Dado un bot con `pvp_damage` negativo, entonces la vida de la víctima no sube (ya es así hoy: OA:2913; el
  criterio protege frente a regresiones).
- **CA-8** Dado A que propone y B que no responde en 20 s, entonces el PvP sigue apagado en los dos.
- **CA-9** Dado el acuerdo activo, cuando B pulsa "Terminar duelo", entonces en menos de 1 s los golpes de A dejan de
  hacer daño.
- **CA-10** Dado el acuerdo activo, cuando B sale y vuelve a entrar en la sesión, entonces el PvP está apagado.
- **CA-11** Dado B con escudo en guardia, cuando A golpea de frente, entonces el registro de B dice `blocked` y no
  pierde vida.
- **CA-12** Dado el anfitrión con la regla "permitir PvP" desactivada, cuando cualquiera propone un duelo, entonces
  la propuesta se rechaza con el motivo y ningún golpe hace daño; cuando la desactiva con un duelo en curso, el
  duelo termina en menos de 2 s.
- **CA-13** Dado el acuerdo activo, cuando un jugador dispara una flecha o usa una runa (que no sea la bomba) contra
  el otro, entonces la vida del otro no cambia.

## Qué NO hacer

- No confiar en ningún número que venga del atacante: lo recalcula o lo acota el anfitrión (ADR-008, decisión 17).
- No aplicar daño en el juego del atacante ni en el servidor: el anfitrión valida, pero solo la víctima escribe su
  propia vida, en el hilo del juego.
- No guardar el acuerdo de PvP en `online.ini`.
- No activar PvP por defecto ni con una sola aceptación.
- No añadir flechas, runas ni derribos nuevos en este caso.
- No cambiar `online.h` (mensajes nuevos, bit de estado) sin el dueño del protocolo.
- No quitar la comprobación `pvp_damage <= 0` existente.
- No usar las variables de prueba que matan (`SAVAGEWIND_ONLINE_DIE_TEST`, `_KILL_TEST`) fuera de pruebas.

## Riesgos y preguntas abiertas

- Riesgo: el golpe es una heurística de cono y alcance, no una colisión real: habrá golpes que "no deberían" dar y al
  revés (mapa 07 §2.2). Un golpe con colisión real necesita ingeniería inversa.
- Riesgo: con latencia alta, la comprobación de distancia puede rechazar golpes legítimos; hay que medir con
  `tc netem`.
- Riesgo: `power.txt` y `defense.txt` salen de los ficheros de cada jugador; con mods distintos, valores distintos.
- Riesgo: cuando el atacante o la víctima es el propio anfitrión, valida su propio juego; un anfitrión manipulado
  no se puede detectar (el anfitrión manda, D3).
- Preguntas ya contestadas por el dueño (Q-12-1 y Q-12-3): ver ADR-008 (decisión 17) en `../decisiones/`.
- **Pregunta al dueño** (Q-12-2, abierta; por defecto: el duelo se apaga y se aplica la reanimación normal de
  CU-11): al morir en un duelo, ¿hay otra regla?
- **Pregunta al dueño** (Q-12-4, abierta; por defecto: no se puede abrir en santuarios ni durante escenas): ¿se
  puede abrir un duelo en cualquier sitio?

## Fase y dependencias

- **Fase**: F4. La validación (punto 1) pasa al anfitrión (ADR-008, decisión 17); se apoya en la autoridad del
  servidor de F3 y puede cambiar el protocolo, así que ya no se adelanta a F2.
- **Depende de**: CU-03, CU-10 (indicador), CU-11 (qué pasa al morir).
- **Lo necesitan**: ninguno.
