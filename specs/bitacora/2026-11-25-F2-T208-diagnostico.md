# Bitácora · F2 T208 · diagnóstico de la pasada (enemigos en bucle, pasos 4 y 5)

- Fecha: 2026-11-25
- Quién: agente Opus (`coop/DIAG208`) bajo el orquestador (ADR-006): solo lectura de código y logs; **no compila ni
  ejecuta** nada del juego
- Datos (fuera del repositorio, no copiados): `pruebas/logs/T208-pasada-A.log` (anfitrión, 599 líneas) y
  `T208-pasada-B.log` (invitado, 1417 líneas), pasada sin ventana del build de T208 (`23051ec`). Resultado de
  `recorrido.py`: pasos 1 PASA, 4 FALLA ("sin lineas de traspaso de enemigos"), 5 FALLA ("A no recreó a OnlinePlayer
  al volver B"), 6 y 7 PASA
- Código comparado: `7b53d18:runtime/online_actors.c` (antes de T206) contra `runtime/oa_*.c` de `23051ec`

Niveles de evidencia (constitución, regla 16): **[L]** lectura del código; **[log]** visto en los dos logs;
**[S]** prueba sintética (un script sin el juego); **[H]** hipótesis; **[sin verificar]** no se pudo comprobar.

## Veredicto

**No es una regresión de T206, T207 ni T208.** Los tres síntomas tienen otra causa:

| Síntoma | Causa | Nivel |
|---|---|---|
| Paso 4: 0 traspasos | El `teleport` al campamento **no movió a ninguno de los dos Links**: siguieron juntos en el punto exacto de `TEST_WARP`. Con los dos en el mismo punto, el código nunca cambia de dueño (empate a favor del anfitrión), y además B estaba a 113 m de los enemigos, más que `OWN_SAFE` = 80 m | [log] + [L] |
| Bucle "made again (6562 m away)" en B | B en el establo 2 (paso 5), a 6,5-7 km de A. El anfitrión manda los enemigos que tiene cerca de **cualquier** jugador, él incluido, y el invitado crea una marioneta de cada uno **sin mirar a qué distancia está de su Link**. El juego de B borra los actores creados tan lejos y el código los vuelve a crear. El código es idéntico al de antes de T206 | [log] + [L]; que la causa del borrado sea la distancia: [H] |
| Paso 5: "A no recreó a OnlinePlayer" | Error de `tools/coop/recorrido.py`: `Ctx.actores()` no sabe leer la respuesta de `actors`, devuelve siempre una lista vacía, y el paso 5 falla siempre. A.log sí registra `Client: OnlinePlayer (actor 72ecc71358)` al volver B | [S] + [log] |

## 1. Línea de tiempo

Los relojes `t=` de los dos procesos no coinciden. Con eventos emparejados (B salta y A borra su Link; B vuelve y A
lo rehace; el botín `Weapon_Sword_500`), **t(A) ≈ t(B) + 18-20 s**. Coordenadas en metros (x, y, z).

| t A | t B | Qué pasa | Dónde está cada Link |
|---|---|---|---|
| 18 | — | `Client joined (slot 1)` | — |
| 44-52 | 45-49 | Los dos cargan; `[warp] Link starts at -1041.6, 250.2, 1812.1` en los dos | A y B en **el mismo punto** (meseta) |
| 68 | 49 | A: `slot 1 can own enemies`. B: `4 of our enemies gone: the host's are there` y marionetas de `Enemy_Bokoblin_Junior` 460, 466 y 467 del anfitrión, en (-1150, 256, 1768) y alrededores, a **113 m** del punto | igual |
| 68-133 | 49-114 | **Paso 4.** Ningún `[place]` nuevo en ninguno de los dos logs (se escribe cada 30 s si Link se movió 1 m o más: `control_game.c:control_place_frame`), ni `a long jump`, ni `a load`. Nadie llegó al campamento (-1203, 198, 1641), que está a 235 m | igual: los dos quietos en el punto de `TEST_WARP` |
| 134 | 114,7 | **Paso 5.** B `goto stable 2`: `a long jump: the players' actors still here, kept`, `removed Host's Link`. A: `removed Client's Link`. **Empieza el bucle** (`made again (6992 m away)`) | A en la meseta; B saltando al establo 2 |
| ≈148-150 | ≈129-131 | A `goto tower 7` (el número de *draws* de A cambia). La lista del anfitrión pasa a ser el campamento de la torre: `Bokoblin_Guard_Junior` 1127, 1141, 1144 y 1175, `Bokoblin_Junior` 1163, 1172 y 1174, y su arco, escudo y espada. El bucle sigue con `6562 m away` | A en la torre 7; B en el establo 2 |
| ≈157 | 138,8 | B: `[place] x=4249.79 y=231.44 z=-2756.86`, exactamente el establo 2 (`runtime/control_places_data.inc:156`) | B en el establo 2 |
| 162 | 144,8 | B `goto tower 7`. B: `Host: OnlinePlayer`; A: `Client: OnlinePlayer (actor 72ecc71358)`. **Termina el bucle** (el último `made again` está en t≈142,8) | los dos en la torre 7, prácticamente en el mismo punto |
| 172 | 169 | A en (-568.96, 195.47, 1697.96); B en (-568.49, 177.69, 1697.16). B escribe otra línea `[warp]` sin carga (los *draws* no cambian): el juego volvió a leer `game_data.sav` [H] | — |
| — | 221 | Paso 6: `the host's time: 18.02` | — |
| 243-262 | — | Paso 7: ocho veces la pausa, sin cuelgue | — |

Dueño de los enemigos durante toda la pasada: **el anfitrión (slot 0)**. Ningún enemigo cambió de dueño, y ninguno
pasó a ser real en B. No hay descartes de la validación de T213 (`dropped a ... that failed validation`) en ninguno
de los dos logs [log].

Los pasos 2 y 3 no se ejecutaron: en ningún log aparece el `goto tower 7` inicial ni el paseo de B [log].

## 2. "made again (6562 m away)"

**Qué es el número.** `oa_g_dist` (`runtime/oa_core.c:407`) lo escribe solo el bucle de jugadores de
`rt_online_frame` (`runtime/oa_frame.c:328`): es la distancia XZ de **nuestro Link al último jugador remoto
recorrido** en el fotograma anterior. `oa_piece_update` (`runtime/oa_core.c:418-421`) la imprime para cualquier pieza:
Link remoto, armadura o marioneta de enemigo. En B, el único jugador remoto es el anfitrión, así que el número es la
distancia **de B a A**, no la del enemigo [L]:

- establo 2 (4249.79, -2756.86) → meseta (-1041.6, 1812.1): √(5291,4² + 4569,0²) = **6991 m** (el log dice 6992);
- establo 2 → torre 7 (-568.66, 1697.44): √(4818,5² + 4454,3²) = **6562 m**.

Los enemigos están a menos de 150 m de A, así que la cifra también vale, de forma aproximada, para ellos; pero es
casualidad. El original hace lo mismo con `g_dist` (`7b53d18:runtime/online_actors.c:671`, `:684`, `:6152`) [L].

**Por qué hay bucle** [L], salvo donde se indica:

1. El anfitrión registra y manda los enemigos que están a menos de `ENEMY_NEAR` = 150 m de **cualquier** jugador, él
   incluido (`runtime/oa_enemies.c:485-490`). A tenía enemigos cerca, así que la lista no se vació aunque B estuviera
   a 7 km.
2. El invitado (`oa_enemies_client` → `oa_enemies_show`, `runtime/oa_enemies.c:792`) recorre la lista y llama a
   `oa_piece_update` para cada enemigo, su arma y su escudo. **No compara la posición del enemigo con la de su
   Link.** El filtro de 400 m que sí tiene el Link remoto (`runtime/oa_frame.c:327`) no existe para los enemigos.
3. La marioneta se crea a 6,5-7 km de B, fuera de lo que el juego de B tiene cargado, y el juego la borra
   enseguida [H]. El propio código lo da por sabido en otro sitio: "his game would remove it as it is made"
   (`runtime/oa_enemies.c:598`).
4. En el fotograma siguiente, `oa_piece_update` ve el actor muerto, escribe `made again`, pone `actor = 0` y la pide
   otra vez (`runtime/oa_core.c:419-421`).

Medida [log]: 290 borrados y 317 creaciones entre t(B) 114,7 y 142,8, unos 10 por segundo. El bucle dura
exactamente lo que B estuvo lejos.

**¿Es lo de siempre?** Sí en el código: tras normalizar los prefijos (`oa_`, `oa_g_` y `oa_pup(i)->` frente a
`g_act[i].`), son **idénticas** al original estas funciones: `enemies_show`, `enemies_client`, `enemies_host`,
`piece_update`, `can_own` y `same_map`. `rt_online_frame` solo difiere en una línea (`oa_rune_grab(i)` por
`g_act[i].grab`, el accesor de T208) [L]. En la línea base de F1, el paso 5 también mandó a B al establo 2 con
enemigos junto a A, así que el bucle debió de ocurrir también. Los logs de esa pasada se perdieron (`A.log` y `B.log`
de `pruebas/logs/` son ya los de esta pasada), así que eso queda **[sin verificar]**. No es un caso nuevo: es la
misma familia que HE-02 (el invitado no filtra por mapa) y HE-03 (radios 150/170 m) del mapa 08, ahora por
**distancia**.

## 3. Por qué A no tiene ninguna línea `Enemy_` y no hubo traspasos

En el anfitrión, registrar un enemigo no escribe nada en el log (`runtime/oa_enemies.c:478-505`). Las líneas con
`Enemy_` solo salen al cambiar de dueño (`slot N's now`), al morir o al volver del juego de otro [L]. Para que un
enemigo cambie de dueño hace falta todo esto (`runtime/oa_enemies.c:561-616`):

- que el otro sea estrictamente más cercano (`d < best`, `:569`), y el anfitrión se evalúa primero (`i = -1`), así
  que **un empate es suyo**;
- que el dueño esté lejos (`cur > 40 && best + 10 < cur`) o que el otro lleve 50 pasadas 15 m más cerca (`:596`);
- y que el nuevo dueño esté a `OWN_SAFE` = 80 m o menos (`:600`).

En el paso 4, A y B estaban en el **mismo punto** de `TEST_WARP` y los enemigos 460/466/467 a 113 m. Empate y más de
80 m: ningún traspaso es posible, con este código y con el original. En la torre 7 vuelve a pasar lo mismo: los dos
usan el mismo `goto`, llegan prácticamente al mismo punto, y el enemigo más cercano, el 1127, está a unos 28 m [L] +
[log].

**Por qué la línea base sí pasó** [H, sin verificar]: en F1 se ejecutaron antes los pasos 2 y 3, y el `teleport` al
campamento sí movió a los Links. Lo más probable es que los "3 traspasos" salieran del tiempo en que uno había
llegado y el otro no (dueño lejos: `owner far`). Un paso que solo pasa en ese intervalo es frágil: depende del orden
de llegada, no del código.

**Por qué el teleport no movió a nadie** [H]: `teleport` es asíncrono. `recorrido.py` solo comprueba que la orden se
acepte (`c.ok`), y `esperar_quieto` toma un *failed* por terminado (mapa 08, N3 y HE-13). Es el problema de
herramienta que investiga otro agente (`wt/GOTO`). Los logs no dicen el motivo: `control_tp.c` no escribe el estado
final en el log.

## 4. Paso 5: la herramienta, no el juego

`cmd_actors` (`runtime/oa_control.c:142-146`) escribe cada actor en el campo `actors` como
`"<nombre> <x> <y> <z> <d>m <actor hex>"` (6 palabras), igual que el original (`7b53d18:...:5830`).
`Ctx.actores` (`tools/coop/recorrido.py:92-104`) espera 5 palabras numéricas al final (`p[-5]`, `float(v)` de
`p[-4:]`). `float("1m")` lanza `ValueError` y la fila se descarta, así que **la lista sale siempre vacía**. Prueba
sintética [S] con la fila `OnlinePlayer -568.5 177.7 1697.2 1m 72ecc71358`: `ValueError: could not convert string
to float: '1m'` y `lista: []`.

Consecuencias [L]:

- `esperar_actor` devuelve siempre `[]`, así que el paso 5 falla siempre. Explica también el paso 2 de la línea base
  ("A: 'actors OnlinePlayer' devuelve 0") y el punto 2 abierto de `2026-11-24-F1-linea-base.md`.
- `esperar_ausencia` devuelve siempre `True`, así que la primera mitad del paso 5 pasa sin comprobar nada.
- El paso 3 y `enemigos()` dependen de la misma función.

## 5. Propuestas

No hay arreglo de T208 que escribir. Propongo (no aplicado, fuera de los `files:` de este encargo):

**P1 · `tools/coop/recorrido.py:Ctx.actores`** (herramienta, F2): leer el formato real.

```python
for fila in str(d.get("actors", "")).split(";"):
    p = fila.split()
    if len(p) >= 5:
        try:
            lista.append((p[0], float(p[1]), float(p[2]), float(p[3]), float(p[4].rstrip("m"))))
        except ValueError:
            pass
```

**P2 · `recorrido.py:paso4`** (herramienta): después de cada `teleport`, exigir `teleport status` = *done* y que
`pos` esté a menos de 5 m del destino; si no, FALLA con "teleport no llegó" en vez de "sin traspasos". Además, dejar
a B junto a un enemigo y a A lejos (ver la prueba T-1), para que el traspaso no dependa del orden de llegada.

**P3 · invitado: no crear marionetas de enemigos lejanos** (cambio de comportamiento, tarea propia; propongo
**HE-23**, junto a HE-02 y HE-03): en `oa_enemies_show`, saltar y borrar la marioneta de un enemigo a más de 400 m
del Link local (el mismo límite que el Link remoto, `oa_frame.c:327`). No pertenece a T208, que no debía cambiar
comportamiento.

**P4 · log** (cosmético): `made again (%.0f m away)` debería dar la distancia de la pieza, o decir "host at %.0f m".

## 6. Cómo comprobarlo en el juego

Con `PY=tools/venv/Scripts/python.exe` y el build de T208. Antes: `bash tools/coop/dos_juegos.sh restaurar`, luego
`bash tools/coop/dos_juegos.sh arrancar`, y esperar a que `$PY tools/swctl.py --port 7777 --wait 180 pos` y lo mismo
en 7778 respondan.

**T-1 · traspaso con posiciones distintas (paso 4 bien hecho).**
1. `$PY tools/swctl.py --port 7777 teleport -1203 198 1641`; repetir `teleport status` hasta *done* o *failed*;
   luego `pos`. Si falla, es el problema de la herramienta (HE-13), no de T208.
2. `$PY tools/swctl.py --port 7777 actors Enemy_`: elegir un enemigo E en (ex, ey, ez).
3. `$PY tools/swctl.py --port 7778 teleport <ex+10> <ey+3> <ez>` (B a 10 m de E) y
   `$PY tools/swctl.py --port 7777 teleport <ex-60> <ey+3> <ez>` (A a 60 m). Esperar *done* en los dos y 15 s.
4. Esperado: A.log `[online] Enemy_... <id>: slot 1's now (was 0's; owner far, owner at ~60 m, new one at ~10 m)`;
   en 7778, `actors Enemy_` lo muestra real y `actors OnlineEnemy` ya no tiene su copia.
5. Control: `teleport` de A y B a la misma coordenada exacta. Esperado: 0 traspasos (el empate es del anfitrión).

**T-2 · el bucle a distancia.**
1. Con A en el campamento (T-1): `$PY tools/swctl.py --port 7778 goto stable 2`; esperar *done*.
2. Durante 30 s: `grep -c "made again" pruebas/logs/B.log` sube unas 10 veces por segundo, y
   `$PY tools/swctl.py --port 7778 actors OnlineEnemy` cambia de `actor` entre dos llamadas, o sale vacío.
3. `$PY tools/swctl.py --port 7778 goto tower 7`: el contador deja de subir.
4. Para confirmar que ya pasaba antes [sin verificar]: repetir 1-3 con un build de `7b53d18`. Solo cambia `runtime/`;
   `GAMELIB_ID` debería ser el mismo, comprobarlo con la orden de `AGENTS.md`.

**T-3 · paso 5.** Tras P1: `$PY tools/coop/recorrido.py --pasos 5`. A mano, sin P1: con B de vuelta en la torre 7,
`$PY tools/swctl.py --port 7777 actors OnlinePlayer` debe dar 1 actor.

**T-4 · lo propio de T208** (su `verify`, que esta pasada no cubre): Magnesis, Stasis y Cryonis de B vistos en A, y
`firma_log.py --comparar` en 0 tras el recorrido completo.
