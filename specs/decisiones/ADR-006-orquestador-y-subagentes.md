# ADR-006 · Un orquestador integra y verifica; los subagentes solo escriben

- Estado: aceptada por el dueño (2026-11-24)
- Afecta a: `fases/README.md` §5 (protocolo de relevo), pasos 7, 8 y 12

## Contexto

El protocolo de relevo original pedía que cada agente ejecutara el `verify` de su tarea en su propio worktree. En
esta máquina (15,7 GB de RAM, ver `COMO-CONTINUAR.md`) varias compilaciones, contenedores o juegos a la vez agotan la
memoria, y dos agentes compilando en paralelo se estorban por CPU, disco y `build/`.

## Decisión

1. Un **orquestador** (un modelo Claude Opus en la sesión principal) reparte las tareas de `tasks.md` y lanza
   subagentes Claude en paneles de **Herdr**: **Sonnet** para tareas `bajo`/`medio`, **Opus** para tareas `alto`.
2. Cada subagente trabaja en `$SW/wt/<tarea>` (rama `coop/<tarea>` desde `coop/main`), toca solo sus `files:`,
   hace commit y entrega diff, resumen y lista de "NO verificado". **No compila, no ejecuta pruebas, no lanza el
   juego ni Docker, no instala nada y no hace push.** Puede ejecutar comprobaciones de solo lectura baratas (`grep`,
   `git diff`).
3. El orquestador **integra** cada rama en `coop/main` (merge `--no-ff`), resuelve conflictos, ejecuta los `verify`
   **de uno en uno**, marca la tarea en `tasks.md` y escribe la entrada de `bitacora/`.
4. El orquestador puede aprobar en el panel de un subagente los permisos que solo leen o editan dentro de su
   worktree. Lo demás (push, instalaciones, borrados fuera del worktree, cambios de modo de permisos) se pregunta al
   dueño.
5. **Push al fork solo al cerrar cada fase** (T024, T121, T221...), por HTTPS con la cuenta CosmicAdc; nunca
   `--force`, nunca a `Revivedx`. La única excepción aprobada es subir la rama `test` del original al fork (T001).

## Consecuencias

- La constitución sigue vigente: la regla 21 ("un agente no declara terminado lo que no ha visto") se cumple porque el
  subagente marca todo como "no verificado" y la verificación la hace y anota el orquestador.
- La regla 19 (sin `verify:` no hay paralelo) sigue aplicando: el paralelismo es de escritura; la verificación es en
  serie.
- El cuello de botella pasa a ser el orquestador. Se acepta a cambio de no agotar la RAM.
- Los subagentes no deben usar `specs/` como directorio de trabajo: el hook de auditoría del equipo del dueño escribe
  `governance/audit/` en el directorio actual. `governance/` está excluido en `.git/info/exclude` del clon.
