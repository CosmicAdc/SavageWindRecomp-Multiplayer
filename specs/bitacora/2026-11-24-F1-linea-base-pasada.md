# Bitácora · F1 · integración (I18N, CU15, NOMBRES) y pasada con dos juegos (T101, T103–T106)

- Fecha: 2026-11-24
- Quién: orquestador (Gemini 4 Hydrogen)
- Rama y commit base de la prueba: `coop/main` en `fe68382`
- Overlay: `online_mod/v43` (`weapons.txt` 6463 B, `items.txt` 8971 B, `rupees.txt` 118 B)
- Hardware: Dell G15 5520 · i7-12700H · 15,69 GB RAM · RTX 3060 Laptop 6 GB · Windows 11

## 1. Integración y verificación de ramas pendientes

| Rama | Commit integrado | Qué se comprobó | Resultado |
|---|---|---|---|
| `coop/I18N` | `179fc76` (incluye `caaa473`) | Se añadió `T("%s  %.1f km", "")` que faltaba en los 13 `.inc`; `tools/i18n_check.py` | `0 missing` en los 13 idiomas, código 0 |
| `coop/CU15` | `bc120f9` (`42261fd`) | Revisión de `CU-15-amenaza-de-los-enemigos.md`, `casos-de-uso/README.md` y `T443`–`T444` en F4 | Sin conflictos, solo documentación |
| `coop/NOMBRES` | `fe68382` (`33f68fd`) | `runtime/main.c:507` actualizado a `online_mod/v43`; `make test-online-names`; regenerado `online_mod` | `39 checks, 0 failures` |

Tras integrar las tres ramas en `coop/main`:
- `make test-online-names test-online-gate test-online-net`: `test_online_names` 39/39, `test_online_gate` ok, `test_online_net` 162/162.
- `make -j8`: compila (`build/savagewind.exe`, `build/swgame.dll`), aviso preexistente en `runtime/online_actors.c:6045` (`for loop has empty body`).

## 2. Preparación (`T101`) y prueba con dos juegos (`T103`–`T106`)

- `T101`: `pruebas/guardado-solo` copiado a `pruebas/guardado-A` y `pruebas/guardado-B`; `ajustes-A.ini` y `ajustes-B.ini` creados; etiqueta `f0-cerrada` presente (`58fb6b3`).
- RAM libre antes de arrancar (con Docker Desktop y navegador cerrados por el dueño): **7,17 GB / 15,69 GB**.
- RAM durante la pasada (`pruebas/logs/ram-pasada.log`): los procesos llegaron a `3615 MB` y `3672 MB` al cargar (mínimo libre `0,14 GB`), tras lo cual Windows recortó los working sets hasta `446–795 MB` con paginación, sin caídas ni cuelgues.
- Hallazgo de arranque desatendido: `SAVAGEWIND_INPUT="900:A:8,1400:A:8,1900:A:8"` dejó ambos juegos en la pantalla de DLC (*Paquetes descargados / Contenido adquirido*) en vez de cargar la partida (`No world loaded`). Para correr `recorrido.py` se envió `b` + `a` por `swctl.py` hasta que ambos devolvieron `pos`.

### Resultados de `T103` (CU-01)
- **Sin contraseña**: `PASO 1: PASA - Arrancar A y B (CU-01): hosting 1, entradas 2`.
- **Con `password = prueba` en `coop/online.ini`** (quitada después): `PASO 1: PASA - Arrancar A y B (CU-01): hosting 1, entradas 2` (código 0). En `A.log` quedó registrado `hosting on port 52530`, `Host joined (slot 0)`, `Client joined (slot 1)` y `[online] Client: OnlinePlayer (actor 72ecccf910)`.

### Resultados de `tools/coop/recorrido.py --enemigos` (`T104`–`T106`)

| Paso | Resultado | Detalle observado |
|---|---|---|
| 1. Arrancar A y B (CU-01) | **PASA** | `hosting 1, entradas 2` |
| 2. `goto tower 7` en ambos (CU-02) | **FALLA** | `A: 'actors OnlinePlayer' devuelve 0 (se espera 1)` *(en `B.log` sí apareció `[online] Host: OnlinePlayer`)* |
| 3. B camina (CU-02) | **FALLA** | `A no ve a OnlinePlayer` |
| 4. Campamento del acantilado (CU-03) | **PASA** | `3 traspasos` |
| 5. B se aleja >400 m y vuelve | **FALLA** | `A no recreo a OnlinePlayer al volver B` |
| 6. A cambia la hora (CU-03) | **PASA** | `1 lineas de hora en B` |
| 7. A abre y cierra pausa (CU-03) | **PASA** | `pose en A: 0 lineas nuevas` |
| 8. Ambos saltan lejos a la vez | **FALLA** | `A: 'goto hateno' fallo (Teleport in progress)` |
| 9. Matar B (CU-03) | **FALLA** | `dos_juegos.sh matar B fallo: /bin/bash: C:Users...cooptoolscoopdos_juegos.sh (ruta de Windows sin barras): No such file or directory` *(ruta Windows sin `/` en `subprocess.run(["bash", str(DOS_JUEGOS), ...])` de `recorrido.py:157`)* |
| 10. Relanzar B (CU-01) | **FALLA** | Mismo error de ruta en `recorrido.py:157` |
| ENEMIGOS (a), (b), (c) (ADR-012) | **FALLA** | `B no ve ningun 'Enemy_' en el campamento` (arrastrado tras el fallo previo / sin enemigos presentes en esa pasada) |

## Pendiente y notas para `T107` (sesión con ventana del dueño)
1. **No usar `SAVAGEWIND_INPUT` en la sesión manual (`T107`)**: `runtime/hle/input.c:586-598` ignora teclado, ratón y mando cuando `SAVAGEWIND_INPUT` está definida (`[input] scripted run: keyboard, mouse and controllers ignored`), y `dos_juegos.sh ventana` hoy la sigue exportando.
2. En `recorrido.py:157` hace falta pasar la ruta POSIX (`DOS_JUEGOS.as_posix()` o relativa desde `RAIZ`) para que MSYS2 `bash` encuentre `dos_juegos.sh` en pasos 9 y 10.
3. Investigar por qué en la pasada sin contraseña `A` devolvió 0 `OnlinePlayer` en el paso 2 mientras en `B.log` (y en `A.log` de la prueba con contraseña) sí se creó `OnlinePlayer`.
