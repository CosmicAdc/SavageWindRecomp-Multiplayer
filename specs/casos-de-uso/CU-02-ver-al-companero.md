# CU-02 · Ver al compañero moverse, con su equipo

Abreviaturas de referencias y montaje común de prueba: ver [README](README.md).

## Resumen

Cada jugador ve al otro como un Link completo en su mundo: se mueve, trepa, pelea y lleva la armadura y las armas
que lleva en su propio juego. Ya existe en el PR #294; en F1 se prueba.

## Actores

- **Jugador local**: quien mira.
- **Compañero**: el otro jugador, representado por el actor `OnlinePlayer` y sus piezas (`OnlineArmor_*`,
  `OnlineGear_*`).

## Precondiciones

- Sesión conectada (CU-01).
- `online_mod/` generado en los dos juegos. Sin él solo se ven figuras esquemáticas del overlay (DOC:38-39).
- Los dos en el mismo mapa y a menos de 400 m en horizontal (OA:6014-6018).

## Flujo principal

1. El compañero entra en la sesión y aparece en la lista de jugadores, a la izquierda bajo los corazones.
2. Cuando está a menos de 400 m y en el mismo mapa, su Link aparece en el mundo.
3. El jugador lo ve caminar, correr, trepar, planear, blandir armas, cubrirse y tensar el arco, con unos 100 ms de
   retraso.
4. Lleva su armadura (cabeza, torso, piernas) y su arma, escudo, arco, vaina y carcaj, en la mano o a la espalda.
5. Si el compañero cambia de equipo, el cambio se ve en pocos segundos.
6. Sobre él se ve su nombre y la distancia. Si queda fuera de la pantalla, una flecha en el borde apunta hacia él.
7. También se ven sus bombas, sus flechas en vuelo, su caballo y los efectos de sus runas.

## Flujos alternativos y de error

| Situación | Qué pasa hoy | Evidencia |
|---|---|---|
| Compañero en otro mapa (santuario, Bestia Divina) | No tiene Link en mi mundo | OA:1484-1486, OA:6014, DOC:402-407 |
| Compañero a más de 400 m | No se crea su Link (el mundo de allí no está cargado); el overlay sigue mostrando nombre y distancia hasta 3000 m | OA:6018, UI:266 |
| Compañero cargando o sin enviar estado 3 s | Se marca "loading" y no se dibuja | NET:1402-1405, UI:89, UI:263 |
| Compañero en pausa o en un menú | Si los menús pausan, deja de enviarse la pose y se repite la última; en la lista sale "(paused)" | OA:5766, OA:1907-1911, UI:231 |
| Compañero en una escena | Sin tratamiento especial salvo pedestales Sheikah (CU-14) | OA:1625-1633 |
| Compañero muerto | Su AS es `Dead*`; no colisiona conmigo. En el juego del que muere no se crea nada hasta 180 fotogramas después de volver | OA:2849-2850, OA:5873-5880 |
| Yo cargo o me teletransporto lejos | Se olvidan los actores remotos y se rehacen tras 60 fotogramas; si siguen vivos tras un salto largo, se conservan | OA:5748-5761 |
| El juego borra el Link remoto | Se olvida todo y se rehace 180 fotogramas después | OA:6036-6040 |
| Salgo de la sesión o se corta | Los actores de los jugadores se borran | OA:5733-5746 |
| El compañero usa un bot o un cliente sin esqueleto | Tras 60 fotogramas sin esqueleto se usa una animación aproximada | OA:1971-1985 |
| El jugador ya usa su propio overlay de RomFS (un mod) | El modo online se queda sin actores: solo figuras 2D | mapa 07 §1.2 |

## Estado actual en el PR: **EXISTE**

- Creación del Link remoto con `create()` (OA:6042-6051), en el hilo del juego dentro de `rt_online_frame` (OA:5721).
- Copia del esqueleto: se muestrean 121 huesos en local (`sample_skeleton`, OA:1871-1893) y se escriben cada
  fotograma en el remoto (`pose_link`, OA:1901-1942). Viajan 20 veces por segundo en el mensaje SKEL (DOC:55-60).
- Armadura remota (OA:6063-6094) y equipo remoto (OA:6275-6317).
- Nombre, distancia y flecha de borde en el overlay (UI:252-303).
- Limitaciones conocidas:
  - El tinte de armadura viaja pero no está comprobado que se vea (DOC:1003-1008).
  - El Link remoto no tiene cuerpo físico: no te puedes subir encima y los golpes del mundo no le afectan
    (DOC:1009-1010). El empuje entre jugadores lo hace el runtime (`players_collide`, OA:2833-2872).
  - El haz de la Espada Maestra nunca se ha visto funcionar (DOC:1011-1012).
  - Fallos de estabilidad relacionados (A5, M1): ver CU-03.

## Qué hay que construir

Nada nuevo para que el caso exista. En F1:

1. Recorrer la lista de comprobación de abajo con dos juegos reales y anotar lo que falle.
2. Arreglo rápido del plan: usar el Link remoto en runas solo después de comprobar que sigue vivo (OA:5976 frente a
   OA:6036; mapa 07 A5).
3. Rechazar NaN e infinito en la posición y la orientación del jugador antes de `FN_SET_MTX` (OA:6054-6056;
   mapa 07 M4).

En F2: mover este código a `oa_puppet` sin cambiar comportamiento (mapa 07 §8.1), con una función
`oa_puppet_link(i)` que devuelva el actor ya validado.

## Reglas y decisiones de diseño

- **[DUEÑO] D2**: cada jugador es un Link completo; solo cambia el color. El color en pantalla (nombre, marcador) es
  CU-10; el color en el modelo 3D es CU-13.
- **[PROPUESTA]** Mantener el retraso de interpolación de 100 ms (`INTERP_NS`, NET:45) hasta tener medidas reales.
- **[PROPUESTA]** Documentar como limitación conocida que el modo cooperativo y un mod de RomFS propio no pueden
  usarse a la vez mientras el overlay sea un único hueco.

## Criterios de aceptación

- **CA-1** Dado el montaje común y A y B a menos de 50 m en el campo, cuando se ejecuta
  `swctl.py --port 7778 actors OnlinePlayer`, entonces devuelve exactamente 1 actor y su distancia a Link coincide
  (±2 m) con la distancia entre los `pos` de A y B.
- **CA-2** Dado lo anterior, cuando A se mueve con `swctl.py --port 7777 stick l 0 1 120`, entonces la posición del
  actor `OnlinePlayer` en B cambia en menos de 1 s.
- **CA-3** Dado A con una pieza de armadura puesta, cuando se consulta `actors OnlineArmor` en B, entonces aparece la
  pieza correspondiente; cuando A cambia de pieza (a mano), el nombre cambia en B en menos de 10 s.
- **CA-4** Dado A y B juntos, cuando A hace `goto` a un lugar a más de 400 m, entonces `actors OnlinePlayer` en B
  queda vacío y, al volver A, reaparece sin duplicados (1 actor, no 2).
- **CA-5** Dado B dentro de un santuario y A fuera, entonces ninguno tiene `OnlinePlayer` del otro. (Entrar al
  santuario exige ir andando: DOC:747-748; prueba a mano.)
- **CA-6** Dado el bot (`tools/online_bot.c`) conectado a A, entonces A muestra un `OnlinePlayer` que camina en
  círculo y no hay errores `can't create` en el registro de A.
- **CA-7** Prueba visual a mano con las dos ventanas visibles: trepar, planear, nadar, escudo, arco y montar a
  caballo se ven correctos; cada resultado se anota en `bitacora/`.

## Qué NO hacer

- No crear un segundo actor de la misma pieza para un jugador: nunca se dibuja (OA:338-342; DOC:136-145). Las piezas
  se esconden y se reutilizan (pool).
- No crear ni tocar actores remotos durante la muerte del Link local ni en la espera posterior (OA:5873-5880).
- No usar `g_act[i].link` sin haber comprobado `alive()` en ese mismo fotograma.
- No intentar reproducir las animaciones (AS) del jugador en el Link remoto: ya se probó y falló (DOC:76-78); se
  copian huesos.
- No añadir actores a la lista de residentes: solo caben diez más y el nº 59 cuelga el arranque (DOC:90-91).
- No cambiar `tools/online_mod.py` (tamaños RSTB, `traverseDist`) dentro de este caso.
- No añadir campos a `OnlinePose` / `WireState` sin el dueño de `online.h`.
- No subir al repositorio nada de `online_mod/`.

## Riesgos y preguntas abiertas

- Riesgo: todas las direcciones de creación y de huesos son fijas de 1.9.0 (mapa 07 §3.3).
- Riesgo: el coste de red por jugador es de unos 100 kB/s sin compresión (mapa 06 §2.5). Con 2 jugadores es
  asumible.
- **[DUEÑO] (ADR-011, decisión 44; Q-02-1)** Los menús **no pausan**: el mundo sigue y el Link del compañero se ve
  quieto mientras el otro está en un menú (como hoy).
- **[DUEÑO] (ADR-011, decisión 45; Q-02-2)** La colisión entre jugadores está **activada**; el anfitrión puede apagarla
  (como hoy).

## Fase y dependencias

- **Fase**: F1 (probar). Reordenación del código en F2.
- **Depende de**: CU-01.
- **Lo necesitan**: CU-08, CU-10, CU-11, CU-12, CU-13.
