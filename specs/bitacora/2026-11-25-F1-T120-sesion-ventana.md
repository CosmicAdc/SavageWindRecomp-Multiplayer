# Bitácora · F1 · T120 sesión con ventana (dueño como invitado, anfitrión sin ventana)

- Fecha: 2026-11-24/25 (de noche)
- Quién: dueño (juega en ventana el invitado B) + orquestador (Claude Opus; anfitrión A sin ventana, manejado con `swctl`)
- Commit del juego en ejecución: `f57533e` (F1 completo + T201/T202/T203 + módulos de F3/F4 sin usar; **sin** T206/T213,
  que se integraron en el código durante la sesión pero no se compilaron porque el juego estaba en uso)
- Juego 1.9.0, overlay `online_mod/v43`; Dell G15 5520, i7-12700H, 15,69 GB (4,4 GB libres al empezar, con Edge y
  Discord cerrados), RTX 3060 Laptop 6 GB
- A: `dos_juegos.sh relanzar A` (sin ventana, `guardado-A`, control 7777, sesión 52530). B: `pruebas/abrir_invitado.cmd`
  (ventana, `guardado-B`, control 7778), unión manual por F1 → Online a `127.0.0.1:52530`.

## Incidencias del montaje (arregladas)

1. `dos_juegos.sh relanzar A` lanzaba **B**: `preparar()` recorría `for x in A B` sin `local x` y pisaba la variable del
   llamador. Arreglado (`a22fca1`).
2. El juego arrancaba **sin** las variables `SAVAGEWIND_*` (con ventana, con `savagewind.ini` y la carpeta `save/` por
   defecto de `coop/`): el `nohup` de MSYS2 lanzado desde Git Bash pierde el entorno (comprobado: `nohup python -c
   'os.environ'` → `None`; sin `nohup` → valor correcto). Arreglado lanzando en segundo plano con `& disown`
   (`e59bab4`). Ese primer arranque creó `coop/save/` (partida nueva, carpeta ignorada por git); **no tocó guardados del
   dueño**. Queda en disco; se puede borrar a mano.
3. Tras un `goto tower 7` que acabó en `Position verification failed`, Link de A quedó **congelado** (no respondía a
   `stick`, la posición no cambiaba ni por gravedad). `teleport x y z` lo desbloqueó. Pendiente de investigar en
   `runtime/control_tp.c` (fase destino: F2, carril de herramientas, o F3 T334).

## Resultados (dueño = B; evidencia de `A.log` y `swctl` en los dos puertos)

| Prueba | Resultado | Evidencia |
|---|---|---|
| Unión | PASA | A: `server: Client joined (slot 1)`, `Client: OnlinePlayer` |
| Ver al compañero, posición | PASA | B ve a A a 2 m en su posición real (`actors OnlinePlayer` en 7778 = `pos` de 7777) |
| Colisión entre jugadores | PASA (dueño) | "ya hay colisión, ya no le atravieso" |
| Equipo de B en A | PASA | A: `OnlineArmor_Armor_043_Upper/_Lower`, `Armor_Default_Head`, `OnlineGear_Weapon_Lsword_032` + funda |
| Movimiento de A visto en B (caminar en cuadrado, dos saltos) | PASA (dueño) | "se sentía creíble la animación" |
| Botín B → A (B suelta, A recoge) | PASA | A: `loot 2 (Weapon_Lsword_032) made here`, luego `picked up here` / `taken`; desaparece en B |
| Botín A → B (A lanza la espada con R, B recoge) | PASA | A: `loot of ours: Weapon_Sword_044`, luego `picked up elsewhere`; A ve a B con `OnlineGear_Weapon_Sword_044` |
| Golpes y bombas de B a A | PASA | A: `Client hit us: 2 after our defence 0 (life 10 → 8 → 6)`, `boom … go off` |
| Enemigo pasa al jugador cercano (ADR-012 (a)) | PASA | A: `Enemy_Golem_Junior 2144: slot 1's now (owner far, owner at 98 m, new one at 79 m)`; el Talus despertó y atacó a B (dueño) |
| Muerte de B con la regla de revivir | PASA (dueño) | B murió dos veces y lo revivió un hada |
| Muerte de A ×3 (caída desde 250 m, `god_mode` apagado) con B conectado | PASA | A: 3 × `Link died: revived by a fairy`, `a long jump: the players' actors still here, kept`; B respondió a `state` tras cada caída |
| Cerrar el anfitrión con el invitado dentro | PASA | `swctl exit` en A: B siguió vivo 18 s (responde a `state`), su `OnlinePlayer` pasó de 1 a 0 al instante y el dueño confirmó el aviso de desconexión en F1 → Online. Sin reconexión automática (F3 T317-T319) |
| FPS | estables (dueño) | "se han mantenido estables, sin ninguna caída"; A a 51-60 fps sin ventana |
| Enemigo ataca a A cuando es de B (ADR-012 (b)) y traspaso al alejarse (c) | NO HECHO | Para la próxima sesión, desde el campamento |
| Magnesis / Stasis; T113 con Magnesis sostenida | NO SE PUEDE | El guardado de prueba aún no tiene las runas |
| Chat | APLAZADA | El dueño: "el chat tampoco es vital ahora". A no tiene ventana y `swctl` no tiene orden de chat |
| Contraseña con asteriscos (T119) | APLAZADA | El dueño la deja para después |

## Hallazgos nuevos (para la lista priorizada)

| # | Fallo | Gravedad | Fase destino |
|---|---|---|---|
| N1 | El primer golpe de B a A registró `life 0` con A en modo dios y A siguió vivo; además los golpes del compañero matan a A aunque tenga `god_mode` (8 muertes revividas en el log mientras B le pegaba) | incorrecto (prueba) | F4 (CU-12 PvP / daño validado); anotar que `god_mode` no protege del daño online |
| N2 | El compañero **no ve la animación del hada** al revivir: solo ve caer al otro y levantarse | cosmético | F4 (CU-11) |
| N3 | `goto` fallido deja a Link congelado (ver incidencia 3) | incorrecto (herramienta) | F2/F3 herramientas |
| N5 | Al golpear al otro Link con un arma, el empujón a veces lo lleva hacia delante y a veces **hacia el atacante** (dueño: "en general funciona bien") | incorrecto | F4 (CU-12: la dirección del retroceso debe salir de la posición del atacante) |
| N4 | Revisión de cada animación del compañero para mejorarla (pedido del dueño) | mejora | spec nueva, F4 o F5 |

## Decisión del dueño en la sesión

Compañero en el minimapa: opción A (flecha de su color dibujada encima del minimapa, marca en el borde si está
fuera), medir ya y programar en F4; mapa grande como investigación para después; pines reales descartados. Se
registra en ADR-022 y CU-16 (en redacción por `specs-minimapa`; investigación de código T445 por `minimapa-t445`).

## NO verificado

- Esta sesión no prueba T206/T213 (no estaban compiladas). Contraseña, chat, Magnesis/Stasis y ADR-012 (b) y (c)
  quedan pendientes.
