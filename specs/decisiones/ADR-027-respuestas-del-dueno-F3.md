# ADR-027 · Respuestas del dueño para F3 (preguntas abiertas P3-2, P3-3, P3-4 y forma de decidir T305)

- Fecha: 2026-11-25
- Estado: aceptado
- Decide: el dueño, en diálogo con el orquestador
- Fuente: `specs/fases/F3-red-robusta/spec.md` §7

| # | Pregunta | Respuesta del dueño | Afecta |
|---|---|---|---|
| 1 | P3-2 · ¿Se recuerda la contraseña del invitado entre sesiones? | **Solo con una casilla "recordar"**; por defecto no. Si se recuerda, nunca en claro (como pide S6) | T331 |
| 2 | P3-3 · ¿Se comparan los mods cargados al conectar? | **Avisar, no rechazar**: si no coinciden, se conecta igual con el aviso "mods distintos: puede haber fallos" | T325 (campo de mods), T314 (aviso, no rechazo), T329 (texto) |
| 3 | P3-4 · ¿Qué hacer con `--server`? | **Dejarlo como relé sin mundo** y decirlo claramente en la ayuda (el anfitrión sigue siendo un jugador) | T339 |
| 4 | T305 · ¿Cómo se decide TCP o UDP? | **El orquestador mide y presenta tabla y recomendación; decide el dueño** | T303, T304, T305 |

Junto con ADR-025 n.º 6 (Master Mode distinto: se rechaza al conectar, T314/T325).
