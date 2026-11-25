# Cómo continuar el trabajo (para cualquier modelo o persona que retome esto)

Este archivo es el punto de entrada. Léelo entero antes de tocar nada.

## Qué es este proyecto

Un fork de [Savage Wind Recomp](https://github.com/Revivedx/SavageWindRecomp) (recompilación estática de
*Breath of the Wild* de Switch 1.9.0 a PC) en la cuenta de GitHub **CosmicAdc**, para construir un **modo
cooperativo de 2 jugadores** (ampliable) sobre el PR #294 "Online mode" de byluki y mstampfli.

## Dónde está cada cosa

| Ruta | Qué es |
|---|---|
| `$SW/specs/` | Esta carpeta: toda la especificación |
| `$SW/pr294-online/` | Código del PR #294 (worktree de git, rama `pr-294-online`). Base del trabajo |
| `$SW/SavageWindRecomp/` | Clon de la rama `test` del proyecto original |
| `$SW/Game/` | NSP del dueño. **Nunca** entra en git ni se copia a ningún repo |
| `$SW/GUIA-*.md` | Dos guías de investigación previas (recomp en general y SDD) |
| `CosmicAdc/SavageWindRecomp-Multiplayer` (GitHub) | El fork real. El original (upstream) es `Revivedx/SavageWindRecomp` |

## Orden de lectura

1. `PLAN-APROBADO.md`: decisiones, fases y casos de uso aprobados por el dueño.
2. `constitution.md`: reglas no negociables.
3. `fases/README.md`: grafo de dependencias y qué tareas se pueden hacer ahora.
4. El `tasks.md` de la fase en curso, y el contrato de `contratos/` que toque tu tarea.
5. `mapa/` solo si necesitas más detalle del código (son 7 mapas largos).

## Decisiones del dueño (no se reabren sin preguntarle)

- Todo se trabaja en la cuenta **CosmicAdc**. Nada se sube al proyecto original. Al fork solo se sube
  cuando el dueño lo pide.
- Se construye sobre el PR #294, no desde cero.
- El mundo y el progreso son los del **anfitrión**. Cada jugador conserva su inventario, corazones y
  resistencia.
- **Conexión directa**: un jugador hospeda y el otro se conecta a su máquina. Sin servidor dedicado.
- Cooperativo por defecto; PvP activable por acuerdo.
- Cada jugador habla con los NPC por su cuenta; los momentos de historia requieren a los dos presentes.
- Orden de trabajo: **cimientos primero**. Antes de añadir funciones se prueba el sistema y se arregla
  lo rápido y esencial.

Las preguntas que siguen abiertas están en `casos-de-uso/PREGUNTAS-ABIERTAS.md`.

## Estado al 2026-11-25 (fin de jornada)

- **F0, F1 y F2 cerradas** (etiquetas `f0-cerrada`, `f1-cerrada`, `f2-cerrada`). F2 se cerró por decisión del dueño
  con verificaciones aplazadas: lista exacta en `bitacora/2026-11-25-F2-cierre.md` (tanda de firma, `distro_build`,
  pruebas en el juego de T215/T216/T217/T222/T454-b). Hacerlas antes de cerrar F3 (T340).
- Verificado con el dueño: el reinicio en caliente ("Salir al inicio") ya no cierra el juego y cierra la sesión (T212).
- **F3 en curso** (`fases/F3-red-robusta/tasks.md`): hechas T307, T320, T322, T325, T327; integradas T301 (arnés
  `tools/nettest/`), T321, T323 (LAN, Docker OK), T326. Ramas sin integrar al cerrar la jornada: `coop/T302` (modos
  del bot) y `coop/T306` (spike UPnP), con su estado en su bitácora. **Siguiente:** ejecutar
  `MSYS_NO_PATHCONV=1 sh tools/nettest/run.sh smoke` (se cortó compilando), integrar T302/T306, medir T303/T304 y
  presentar al dueño la tabla para decidir TCP/UDP (T305, ADR-027 n.º 4); después T308 y el núcleo T309-T319 en serie.
- F4 adelantada: T452, T454-b, T455, T465a integradas; investigaciones T449, T450, T451, T454, T540. Decisiones del
  dueño de esta jornada: **ADR-025** (estados del juego, santuarios, Eventide, hora, Luna de Sangre, amiibo, giro) y
  **ADR-027** (F3: contraseña, mods, `--server`, solo mismo sistema, DLC distinto se rechaza, T305 lo decide el dueño).
- Pruebas: dos juegos en un PC con `tools/coop/dos_juegos.sh` (necesitan ~7 GB libres: como mucho un agente abierto)
  y `tools/coop/recorrido.py` (lanzarlo en un panel de Herdr, no como tarea de fondo de Claude Code). Docker solo sin
  los juegos abiertos.

## Equipo de desarrollo

Dell G15 5520 · i7-12700H (20 hilos) · 15,7 GB de RAM · RTX 3060 Laptop 6 GB · Windows 11 · Docker
Desktop 28.3 (límite 6,2 GB) y WSL.

- Compilar y jugar: nativo, con MSYS2 UCRT64.
- Docker: pruebas automáticas en Linux, comprobación de compilación en Linux y pruebas de red con el bot.
  No sirve para jugar.
- Dos juegos a la vez ocupan ~9,5 GB de RAM: cerrar Docker Desktop y el navegador antes de esa prueba.

## Cómo coger una tarea

1. Elige en `tasks.md` una tarea sin marcar cuyas dependencias estén hechas.
2. Lee su `contexto:`, `pasos:`, `hecho cuando:` y `no hagas:`. Si algo no está claro, **para y pregunta**;
   no rellenes huecos con suposiciones.
3. Trabaja en tu propio worktree de git y toca solo los archivos de `files:`.
4. Ejecuta el `verify:`. Si no puedes ejecutarlo, dilo; no declares la tarea hecha.
5. Escribe una entrada en `bitacora/` (fecha, commit, qué hiciste, qué comprobaste y **qué no**).
6. Marca la tarea en `tasks.md`.

### Plantilla de encargo para un agente

```
Tarea: <ID> de $SW/specs/fases/<fase>/tasks.md
Lee antes: specs\COMO-CONTINUAR.md, specs\constitution.md y el contrato que cite la tarea.
Código: $SW/pr294-online (trabaja en un worktree propio).
Haz solo esa tarea y toca solo sus archivos. Ejecuta su verify.
Entrega: el diff, la salida del verify y una lista de lo que NO pudiste verificar.
Si la tarea pide algo que contradice constitution.md, para y avisa.
```

## Qué modelo usar para qué

Cada tarea lleva `modelo sugerido:`.

- **alto**: concurrencia entre hilos, punteros a memoria del juego, seguridad de red, ingeniería inversa,
  refactors grandes de `online_actors.c`, cambios de protocolo.
- **medio**: funciones nuevas con patrón ya existente en el código, pruebas unitarias, interfaz.
- **bajo**: traducciones, documentación, scripts de entorno, tareas mecánicas con `verify` claro.

Una tarea marcada `alto` no se da a un modelo menor aunque parezca corta: un error ahí crashea el juego
o abre un agujero de seguridad.

## Lo que nunca se hace

- Subir o copiar archivos del juego, claves, guardados o código generado (`generated/`, `extracted/`).
- Copiar código de proyectos de decompilación o de mods con licencia cerrada.
- Editar el código generado: se arregla el generador o la capa de parches.
- Aceptar por red direcciones de memoria, rutas de archivo o nombres de actor sin lista de permitidos.
- Tocar el estado del juego desde un hilo que no sea el del juego.
- Declarar algo "hecho" porque compila. Hecho es haberlo visto funcionar.
- Hacer `push` sin que el dueño lo pida.
