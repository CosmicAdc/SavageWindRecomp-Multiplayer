# Bitácora · F1 · sesión con ventana del dueño (`T107`) y cierre inesperado al recargar

- Fecha: 2026-11-24
- Quién: dueño (jugó en ventana `Client`) + orquestador (Gemini 4 Hydrogen, `Host` en segundo plano)
- Rama y commit: `coop/main` en `e805e02`
- Juego y overlay: 1.9.0, `online_mod/v43`
- Hardware: Dell G15 5520 · i7-12700H · 15,69 GB RAM · RTX 3060 Laptop 6 GB · Windows 11

## Qué se montó
- **Host (`A`)**: lanzado en segundo plano (`SAVAGEWIND_HIDDEN=1`, `guardado-A`, `ajustes-A.ini`, puerto `52530`, control `7777`), posicionado en `goto tower 7` (`x=-1041.60 y=250.19 z=1812.10`, `god_mode true`). Log en `$SW/pruebas/logs/host-vivo.err.log`.
- **Client (`B`, dueño)**: lanzado en ventana (`1280x720`, `60 fps`, `guardado-B`, `ajustes-B.ini`, control `7778`) sin `SAVAGEWIND_INPUT`, mediante `$SW/pruebas/abrir_invitado.cmd`.

## Qué funcionó (observado en sesión y en `host-vivo.err.log`)
- Conexión manual desde `F1 → Online → Join session` a `127.0.0.1:52530`: `[online] server: Client joined (slot 1)` (`host-vivo.err.log:556`).
- Presencia y equipo del compañero: Host creó `OnlinePlayer`, `OnlineSlate`, `OnlineRuneFx`, `OnlineMagnetFx`, `OnlineArmor_Armor_Default_Head`, `OnlineArmor_Armor_Default_Lower`, `OnlineGear_Weapon_Sword_044` y `OnlineGear_Weapon_Lsheath_056` (`:671-682`). El dueño confirmó que vio al Host tras ir a `tower 7`.
- Golpes y bombas entre jugadores: registrados en Host (`Client hit us: 2 after our defence 0 (life 10 -> 8 -> 6)` y `boom 72ed7d2b50: go off`, `:693-703`).
- Al salir Client al menú de título y volver a cargar: Host registró `removed Client's Link` (`:713`), luego `Client: OnlinePlayer` y sus piezas `again` (`:753-761`), y sincronización de objetos rotos (`FldObj_MapTower_*`, `FldObj_Mound_A_01`, `FldObj_DownloadTerminal_A_01`).

## Incidencia observada: cierre inesperado de `Client` al recargar tras salir al título
- **Reproducción reportada por el dueño**:
  1. Unirse a la sesión → cargar partida → `goto tower 7` (ve al Host).
  2. Salir a la pantalla de título sin guardar.
  3. Volver a cargar la misma partida en el mismo proceso.
  4. Acercarse de nuevo al Host (también hubo `Alt+Tab`).
  5. `Client` se cerró de golpe.
- **Evidencia en Host (`host-vivo.err.log:778-785`)**: `removed Client's Link` → `slot 1 can't own enemies (loading 1, pose 0, as , same map 1)` → `server: Client left (slot 1)`. Host siguió corriendo sin colgarse hasta cerrarlo con `swctl exit`.
- **Evidencia en Visor de eventos de Windows (Application, Event ID 1000, `22:15:07`)**:
  - Aplicación: `savagewind.exe`
  - Módulo con error: `ucrtbase.dll` (`10.0.26100.9711`, desplazamiento `0x00000000000a527e`)
  - Código de excepción: `0xc0000409` (`abort()` / fallo rápido de CRT o pila; no quedó log de stderr de `Client` al cerrarse la consola).
- **Sospecha / pista técnica (sin verificar aún)**: al salir al título y recargar dentro del mismo proceso (`teardown` / reinicio en caliente), `runtime/online_actors.c` todavía no limpia su estado global con `RT_ON_RESET` (previsto en F2 `T212`).

## Estado posterior
- Host detenido limpiamente (`0` procesos `savagewind.exe`).
- Queda pendiente completar la lista visual de `verificacion.md` §3 (`T107`) y cerrar la lista priorizada (`T108`, incluyendo este crash `0xc0000409` al recargar tras salir al título).
