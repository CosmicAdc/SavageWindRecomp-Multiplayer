# F1 — Probar el sistema y arreglos rápidos esenciales · verificación

F1 se declara hecha cuando todas las casillas tienen evidencia o un motivo escrito, y el dueño lo aprueba.
Tres niveles de evidencia, que no se mezclan: **L** = verificado por lectura del código; **P** = prueba automática;
**J** = visto en el juego. Cada arreglo dice cuál alcanzó.

## 1. Antes de empezar

- [x] F0 cerrada; etiqueta local `f0-cerrada` creada (`58fb6b3`).
- [x] Carpetas `guardado-A`, `guardado-B` (copias) y `logs` fuera del repositorio (`$SW/pruebas/`).
- [x] Docker Desktop y navegador cerrados; memoria libre antes de arrancar: `7,17` GB / `15,69` GB.
- [x] Commit de `coop/main` de la línea base: `fe68382` (con arreglos adelantados + `I18N`, `CU15`, `NOMBRES` v43).

## 2. Línea base — recorrido (rellenar ANTES de cualquier arreglo, y otra vez al final)

| Paso | Qué | Caso | Línea base (`fe68382`) | Final desatendida | Final con ventana |
|---|---|---|---|---|---|
| 1 | Arrancar A y B; dos líneas `joined (slot` en A | CU-01 | PASA (`hosting 1, entradas 2`) | | |
| 2 | `goto tower 7`; `actors OnlinePlayer` en ambos | CU-02 | FALLA en A (`0` en `recorrido.py`; B sí vio a A; en prueba con contraseña A sí creó `OnlinePlayer`) | | |
| 3 | B camina; su copia en A le sigue | CU-02 | FALLA (por paso 2) | | |
| 4 | Campamento del acantilado (`teleport -1203 198 1641`) | CU-03 | PASA (`3 traspasos`) | | |
| 5 | B se aleja más de 400 m y vuelve | CU-02, CU-03 | FALLA (`A no recreo a OnlinePlayer`) | | |
| 6 | A cambia la hora; B la sigue | CU-03 | PASA (`1 lineas de hora en B`) | | |
| 7 | Pausa abierta y cerrada varias veces | CU-03 | PASA (`0` fallos) | | |
| 8 | Salto lejano de ambos (`goto hateno`) | CU-03 | FALLA (`A: Teleport in progress`; guion ajustado en `e805e02`) | | |
| 9 | Matar B; A sigue un minuto | CU-03 | FALLA (ruta Windows en `recorrido.py`; arreglado en `e805e02`) | | |
| 10 | Relanzar B; vuelve a entrar | CU-01 | FALLA (mismo fallo de ruta; arreglado en `e805e02`) | | |

Unión con contraseña (T103): PASA (`password = prueba` en `coop/online.ini`, quitada después). RAM por proceso durante la pasada: pico `3615 MB` / `3672 MB` (mín. libre `0,14 GB`, Windows recortó working sets a `446–795 MB` sin cuelgues). VRAM: sin medir.

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

- [x] `make test-online-gate` → 0 (`online gate: ok`, `fe68382`).
- [x] `make test-online-net` → 0. Número de casos: `162 checks, 0 failed` (`fe68382`).
- [x] `make test-online-names` → 0 (`39 checks, 0 failures`, `fe68382`).
- [x] `tools/i18n_check.py` → 0 (`0 missing` en los 13 idiomas, `fe68382`).
- [ ] Pruebas de la línea base de F0 (`F0/verificacion.md` §3): mismo resultado o mejor. Diferencias: pendiente de re-ejecutar todas juntas en `T120` (en `coop/TESTS` ya se corrigieron `launch-flags` y `gyro`; `test-actions` pendiente de confirmar en Windows).
- [x] `make -j8` en Windows → termina (`fe68382`).
- [ ] `ENGINE=docker JOBS=4 tools/linux/distro_build.sh debian:13` → 0 (pasó antes en `2026-11-24-F1-arreglos-adelantados.md`; pendiente de repetir tras `fe68382` en `T120`).
- [x] `grep -n "define PROTOCOL" runtime/online.c` → `38` (verificado en `064d68a`).
- [x] `git diff f0-cerrada -- runtime/online.h` → vacío (verificado en `064d68a`).
- [x] `git diff f0-cerrada --stat -- tools/patches.py tools/game_sites.py` → vacío (verificado en `064d68a`).
- [ ] Comprobación de trampas de nombres (`F0/plan.md` §8) limpia (pendiente de repetir en `T120`).

## 6. Criterios de salida de `spec.md`

| # | Cumplido | Evidencia |
|---|---|---|
| S1 | parcial (`[~]`) | Línea base corrida en `bitacora/2026-11-24-F1-linea-base-pasada.md` + `bitacora/2026-11-24-F1-prueba-ventana-dueno.md` (los 9 arreglos ya habían sido adelantados antes de `T108`) |
| S2 | parcial | `T110`, `T117`, `T118` con pruebas automáticas/Docker; faltan pruebas en juego de `T109`, `T112`, `T113`, `T114`, `T115`, `T116`, `T119` |
| S3 | sí (`fe68382`) | `test-online-gate` ok, `test-online-net` 162/162, `test-online-names` 39/39 |
| S4 | pendiente (`T120`) | Faltan re-ejecutar todas las pruebas de F0 juntas tras `fe68382` |
| S5 | parcial (`fe68382`) | `make -j8` ok en Windows; falta repetir `distro_build.sh debian:13` tras `fe68382` |
| S6 | parcial | Pasada automática de `recorrido.py` ejecutada con fallos de guion arreglados en `e805e02` (falta repetir); sesión en ventana de `T107` tuvo crash `0xc0000409` al recargar desde el título (`F2 T212`) |
| S7 | sí | `bitacora/2026-11-24-F1-linea-base.md`, aprobada por el dueño el 2026-11-24 |
| S8 | sí (`064d68a`) | `PROTOCOL = 38`, `git diff f0-cerrada -- runtime/online.h` vacío |

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
