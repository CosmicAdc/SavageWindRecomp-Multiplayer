# F1 — Probar el sistema y arreglos rápidos esenciales · verificación

F1 se declara hecha cuando todas las casillas tienen evidencia o un motivo escrito, y el dueño lo aprueba.
Tres niveles de evidencia, que no se mezclan: **L** = verificado por lectura del código; **P** = prueba automática;
**J** = visto en el juego. Cada arreglo dice cuál alcanzó.

## 1. Antes de empezar

- [ ] F0 cerrada; etiqueta local `f0-cerrada` creada.
- [ ] Carpetas `guardado-A`, `guardado-B` (copias) y `logs` fuera del repositorio.
- [ ] Docker Desktop y navegador cerrados; memoria libre antes de arrancar: ______ GB.
- [ ] Commit de `coop/main` de la línea base: ________.

## 2. Línea base — recorrido (rellenar ANTES de cualquier arreglo, y otra vez al final)

| Paso | Qué | Caso | Línea base | Final desatendida | Final con ventana |
|---|---|---|---|---|---|
| 1 | Arrancar A y B; dos líneas `joined (slot` en A | CU-01 | | | |
| 2 | `goto tower 7`; `actors OnlinePlayer` en ambos | CU-02 | | | |
| 3 | B camina; su copia en A le sigue | CU-02 | | | |
| 4 | Campamento del acantilado (`teleport -1203 198 1641`) | CU-03 | | | |
| 5 | B se aleja más de 400 m y vuelve | CU-02, CU-03 | | | |
| 6 | A cambia la hora; B la sigue | CU-03 | | | |
| 7 | Pausa abierta y cerrada varias veces | CU-03 | | | |
| 8 | Salto lejano de ambos (`goto hateno`) | CU-03 | | | |
| 9 | Matar B; A sigue un minuto | CU-03 | | | |
| 10 | Relanzar B; vuelve a entrar | CU-01 | | | |

Unión con contraseña (T103): ______. RAM por proceso durante la pasada: A ______ / B ______. VRAM: ______.

## 3. Lista del dueño (sesión con ventana)

- [ ] El compañero aparece con nombre encima.
- [ ] Camina, corre, salta y trepa con animación creíble.
- [ ] Lleva su armadura (cabeza, torso, piernas) y su arma, escudo y arco.
- [ ] Al cambiar de equipo, el cambio se ve en el otro juego.
- [ ] Enemigos compartidos: un golpe en un juego baja la vida en el otro.
- [ ] Botín: lo que suelta uno lo puede recoger el otro.
- [ ] Magnesis y Stasis del compañero se ven.
- [ ] Menú F1 → Online: lista de jugadores, chat, botón de salir.
- [ ] Contraseña (tras T119): asteriscos en la fila y al escribir.
- [ ] 30 minutos sin cuelgue. FPS habitual: ______ / mínimo: ______.

Defectos vistos (cada uno: qué, dónde, cuántas veces):

## 4. Los nueve arreglos

| # | Arreglo | Tarea | Repro "antes" | "Después" | Nivel (L/P/J) | No verificado |
|---|---|---|---|---|---|---|
| 1 | Puerta de versión | T111 | no reproducible (solo hay 1.9.0) | `make test-online-gate`: ____ ; log `actors on`: ____ | | |
| 2 | `has_body` en mecanismos | T112 | | mecanismo probado: ________ | | |
| 3 | Runas y Link vivo | T113 | | Magnesis + muerte ×3: ____ | | |
| 4 | Nombres permitidos | T114 | bot `BOT_LOOT`: ____ | log de rechazo: ____ ; botín normal: ____ | | |
| 5 | Números no finitos | T115 | bot `BOT_NAN`: ____ | `make test-online-net`: ____ | | matrices internas de `WireState` (F2) |
| 6 | Estado de sesión | T116 | arnés: ____ | arnés: ____ | | |
| 7 | `SIGPIPE` | T117 | Docker, `docker kill` ×10: ____ | servidor vivo ×10: ____ | | |
| 8 | `shield` sin terminar | T118 | arnés: ____ | arnés: ____ | | |
| 9 | Contraseña oculta | T119 | captura: visible | captura: asteriscos | | teclado en pantalla |

## 5. Pruebas automáticas y compilación

- [ ] `make test-online-gate` → 0.
- [ ] `make test-online-net` → 0. Número de casos: ____.
- [ ] Pruebas de la línea base de F0 (`F0/verificacion.md` §3): mismo resultado o mejor. Diferencias: ________.
- [ ] `make -j8` en Windows → termina.
- [ ] `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13` → 0.
- [ ] `grep -n "define PROTOCOL" runtime/online.c` → 38.
- [ ] `git diff f0-cerrada -- runtime/online.h` → vacío.
- [ ] `git diff f0-cerrada --stat -- tools/patches.py tools/game_sites.py` → vacío.
- [ ] Comprobación de trampas de nombres (`F0/plan.md` §8) limpia.

## 6. Criterios de salida de `spec.md`

| # | Cumplido | Evidencia |
|---|---|---|
| S1 | | |
| S2 | | |
| S3 | | |
| S4 | | |
| S5 | | |
| S6 | | |
| S7 | | |
| S8 | | |

## 7. Plantilla de bitácora (una entrada por pasada y una por arreglo)

Copiar a `specs/bitacora/<aaaa-mm-dd>-F1-<tema>.md`. Sin rutas personales ni logs crudos.

```markdown
# Bitácora · F1 · <tema>

- Fecha:
- Quién: dueño / agente (modelo)
- Tarea: T1xx
- Commit (`git rev-parse --short HEAD`) y rama:
- Versión del juego (build id) y versión del overlay (`online_mod/v42`):
- Hardware: CPU, RAM total y libre al empezar, GPU y controlador:
- Ajustes: resolución, límite de FPS, con o sin ventana, variables `SAVAGEWIND_*` usadas:
- FPS (habitual / mínimo) de cada juego:

## Qué se hizo
(pasos y comandos, en orden)

## Antes
(cómo se reproduce, cuántas veces de cuántas; o "no reproducible" y por qué)

## Después
(mismo procedimiento; resultado)

## Qué se observó
| Paso | Juego A | Juego B | Resultado |
|---|---|---|---|

## Nivel de verificación
(L lectura / P prueba automática / J visto en el juego) y **lo que NO se verificó**

## Cuelgues
(hora, paso, últimas líneas `[online]` de cada log, reproducibilidad: siempre / a veces / una vez)

## Logs adjuntos
(nombre de los ficheros fuera del repositorio y líneas citadas)
```

## 8. Plantilla de la lista priorizada de pendientes

```markdown
| # | Fallo | Gravedad (cuelgue / incorrecto / cosmético) | Repro | Frecuencia | Fase destino | Hallazgo del mapa |
|---|---|---|---|---|---|---|
```
