# ADR-001: Construir el cooperativo sobre el PR #294

- **Estado:** aceptada (plan aprobado)
- **Fecha:** 2026-11-24
- **Afecta a:** todas las fases

## Contexto

Queremos un modo cooperativo de 2 jugadores para Savage Wind Recomp. El proyecto original no tiene multijugador y
no piensa tenerlo (su FAQ responde "Multiplayer? No", `docs/FAQ.md:593-596`). Existe un PR sin fusionar, el #294
"Online mode" (autores byluki y mstampfli), que ya consigue lo más difícil:

- pone a otro jugador en el mundo como un actor real, con su esqueleto, armadura y equipo
  (`runtime/online_actors.c`, 6468 líneas; `tools/online_mod.py`, 943 líneas);
- sincroniza 27 sistemas (enemigos, botín, runas, objetos, hora, clima, flags…);
- trae red, interfaz y un bot de pruebas (`runtime/online.c`, `runtime/ui/online_ui.mm`, `tools/online_bot.c`);
- documenta 59 entradas entre intentos fallidos, cuelgues y pendientes (`docs/ONLINE.md`), conocimiento que costaría
  meses redescubrir.

También tiene problemas serios, todos leídos en el código: 62 direcciones fijas de la versión 1.9.0 sin comprobar la
versión; ningún `RT_ON_RESET`; ninguna prueba automática; un servidor que no valida quién envía qué; un cliente
lento congela a todos; y un archivo de 6468 líneas con estado compartido entre cinco áreas.

## Decisión

Construimos **sobre el PR #294**, en un fork de la cuenta CosmicAdc, en la rama `coop/main` creada desde
`pr-294-online`. No reescribimos desde cero. Antes de añadir funciones arreglamos sus cimientos (ADR-004).

## Alternativas descartadas

| Alternativa | Por qué no |
|---|---|
| Empezar de cero sobre la rama `test` | Habría que redescubrir cómo crear un segundo Link, copiar el esqueleto, vestirlo y no colgar el juego: 47 funciones del juego localizadas y decenas de callejones sin salida ya documentados |
| Esperar a que el PR se fusione | El proyecto original no fusiona multijugador; puede no ocurrir nunca |
| Tomar solo "ideas" del PR y reimplementar | El valor está en los detalles exactos (offsets, orden de llamadas, esperas tras una carga); reimplementarlos a ojo reintroduce los cuelgues |
| Portar otro mod multijugador (el PR dice inspirarse en uno para otra plataforma) | Otra plataforma, otras direcciones; el propio PR afirma que nada de ese código se traslada (`docs/ONLINE.md:3-6`) |

## Consecuencias

- **A favor:** partimos de algo que, según su documentación, ya funciona entre dos juegos. F1 puede empezar
  probando, no programando.
- **En contra:** heredamos su deuda. Los contratos de `contratos/` y las fases F1-F3 existen para pagarla.
- Heredamos la dependencia de **una sola versión del juego (1.9.0)**.
- Hay que **restaurar `.github/workflows/`** desde `test`: la rama del PR los pierde por cómo se hicieron sus
  fusiones, no a propósito.
- Hay que **rebasar con frecuencia** sobre `test` del proyecto original y mantener nuestros cambios en archivos
  propios siempre que se pueda. Archivos que chocarán primero: `Makefile`, `runtime/main.c`,
  `runtime/ui/menu.mm`, `runtime/ui/game_options.mm`, `runtime/hle/input.c`, `runtime/nvn/vulkan/exec.c`,
  `runtime/ui/i18n/es.inc`.
- El código es de terceros bajo licencia MIT: se conservan los avisos de copyright existentes.
- Escribir o no a los autores del PR queda a criterio del propietario; no bloquea nada.
- **Nada se sube al proyecto original** (constitución, regla 3).
