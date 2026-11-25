# Recursos externos, de básico a avanzado

Criterio de inclusión: cada URL de esta página **aparece en una de las dos guías de investigación**
(`../../GUIA-RECOMPILACION.md`, `../../GUIA-EXPERTO-RECOMP-Y-SDD.md`), **aparece en un fichero del repo**, o **la
abrí y comprobé** el 24-11-2026. La marca indica cuál:

- **[G]** viene de las guías (verificado allí);
- **[R]** está impreso en el repo;
- **[V]** lo comprobé abriéndolo;
- **[sin URL]** recurso real que no pude abrir desde aquí: búscalo por su nombre.

Dentro de cada bloque, de más fácil a más difícil. Una línea sobre por qué importa.

---

## 1. Redes para juegos

| Nivel | Recurso | Por qué importa |
|---|---|---|
| Básico | **Gabriel Gambetta, "Fast-Paced Multiplayer"**, parte I: <https://www.gabrielgambetta.com/client-server-game-architecture.html> **[V]** | La mejor introducción que existe: servidor con autoridad y por qué no se confía en el cliente. Cuatro partes cortas |
| Básico | Parte III, interpolación de entidades: <https://www.gabrielgambetta.com/entity-interpolation.html> **[V, listada en la parte I]** | Es exactamente lo que hace `online_players` (`runtime/online.c:1406-1418`) |
| Básico | Parte II (predicción y reconciliación) y parte IV (compensación de lag), en la misma web: `client-side-prediction-server-reconciliation.html`, `lag-compensation.html` **[V, listadas]** | Lo que este juego **no** hace; conviene saber por qué (módulo 04 §10). La web trae una demo interactiva |
| Medio | **Gaffer On Games** (Glenn Fiedler), índice: <https://gafferongames.com/> **[V]** | La referencia práctica. Lee en este orden los cuatro siguientes |
| Medio | "UDP vs. TCP": <https://gafferongames.com/post/udp_vs_tcp/> **[V, listado]** | El argumento del bloqueo de cabeza de línea, para el ADR de F3 |
| Medio | "Snapshot Interpolation": <https://gafferongames.com/post/snapshot_interpolation/> **[V, listado]** | Colchón de interpolación, pérdida, ráfagas: nuestro caso |
| Medio | "Snapshot Compression": <https://gafferongames.com/post/snapshot_compression/> **[V, listado]** | "Los tres menores" del cuaternión y punto fijo: lo que hace `skel_encode` (`online.c:531-554`) |
| Medio | "Reliable Ordered Messages": <https://gafferongames.com/post/reliable_ordered_messages/> **[V, listado]** | Cómo se construye fiabilidad sobre UDP; para entender qué te da ENet |
| Medio | **Valve, "Source Multiplayer Networking"** (wiki de desarrolladores de Valve) **[sin URL: devolvió 403 al comprobarlo]** | El documento clásico sobre ticks, interpolación de 100 ms y compensación de lag en un motor real |
| Medio | **"How NAT traversal works"** (Tailscale, David Anderson): <https://tailscale.com/blog/how-nat-traversal-works> **[V]** | Todo NAT en un artículo: cortafuegos con estado, STUN, NAT fáciles y difíciles, UPnP/NAT-PMP/PCP, relés. Base de CU-04 |
| Medio | **ENet**, UDP fiable: <https://github.com/lsalzman/enet> **[V]** (licencia MIT) | La candidata del ADR de F3. Pequeña, en C, compatible con la licencia del proyecto |
| Avanzado | **"'Overwatch' Gameplay Architecture and Netcode"** (Timothy Ford, GDC 2017): ficha en <https://gdcvault.com/play/1024001> **[V]**; vídeo: <https://www.youtube.com/watch?v=W3aieHjyNvw> **[V, título comprobado]** | Cómo un juego comercial aísla el código de red en pocos sistemas y usa determinismo y predicción. Sirve para ver qué exige ese enfoque y por qué aquí no es viable |

## 2. Proyectos de referencia: cooperativo en juegos de un jugador

Se estudian por sus **ideas y su documentación**. Antes de abrir código, mira la licencia (módulo 08 §7.3).

| Recurso | Por qué importa |
|---|---|
| **sm64coopdx**: <https://github.com/coop-deluxe/sm64coopdx> **[V]** | Multijugador en línea para el port de PC de Super Mario 64: sincroniza entidades y niveles. El ejemplo más maduro de "propiedad de entidades" en un juego que no era en red. Su README no declara licencia y se apoya en una decompilación: **ideas sí, código no** |
| **Anchor**: <https://github.com/garrettjoecox/anchor> **[V]** | Cooperativo para el port de PC de Ocarina of Time: cliente/servidor que sincroniza **flags y objetos entregados** y carga el estado del otro al unirse. El paralelo más cercano a nuestro problema de misiones (CU-06, CU-07). El servidor está en Go. Su página no declara licencia |
| **Port de Perfect Dark**: <https://github.com/fgsfdsfgs/perfect_dark> **[V]** (MIT) | Port desde una decompilación; el README remite a una rama `port-net` con juego en red. Útil para ver cómo se añade red a un juego de consola ya portado. Exige ROM propia |
| **Milk Bar Launcher**: <https://github.com/MilkBarModding/MilkBarLauncher> **[R: `docs/ONLINE.md:3`] [V]** | El multijugador de BotW que inspiró la idea del PR #294 (para Cemu, según `docs/ONLINE.md:3-4`). Otro juego (Wii U), otra técnica (una DLL inyectada en el emulador). Su README restringe la redistribución: **no copiar código** |

## 3. Recompilación estática

| Nivel | Recurso | Por qué importa |
|---|---|---|
| Básico | Entrevista con el autor de N64Recomp (Software Engineering Daily): <https://softwareengineeringdaily.com/wp-content/uploads/2024/09/SED1735-N64-Recomp.txt> **[G]** | Qué es y qué no es un recomp, contado por quien lo popularizó |
| Básico | Entrevista en Read Only Memo: <https://www.readonlymemo.com/n64-recompiled-interview-majoras-mask-emulation-native-pc-port> **[G]** | Contexto y motivación, sin jerga |
| Básico | **Retro Reversing, "How to create an N64 recomp"**: <https://www.retroreversing.com/how-to-create-an-n64-recomp> **[G]** | La guía de principio a fin más completa, conceptual. Diez pasos |
| Básico | **Retro Reversing, "N64 Reversing Introduction"**: <https://www.retroreversing.com/N64Reversing> **[G]** | Introducción a la ingeniería inversa de consolas |
| Medio | **Retro Reversing, "Testing and Automating an N64 Recomp"**: <https://www.retroreversing.com/testing-an-n64-recomp> **[G]** | Pruebas diferenciales y CI sin ROM: la misma filosofía que `test_lift.py` |
| Medio | **README de N64Recomp**: <https://github.com/N64Recomp/N64Recomp> **[G]** | El diseño original: una función por función, contexto de CPU, tabla para saltos indirectos |
| Medio | **README de XenonRecomp**: <https://github.com/hedge-dev/XenonRecomp> **[G]** | El mejor texto sobre saltos indirectos, tablas de salto y "primero correcto, luego optimizar". Lo citan todos |
| Medio | Guía de modding de N64Recomp: <https://hackmd.io/@Wiseguy/HyhUXwiV0> **[G]** | Por qué un gancho es mejor que reescribir una función: la base de la crítica a los "slopcomps" |
| Medio | `docs/SAVAGE-WIND.md` y `docs/RECOMP-LOCALS.md` del propio repo **[R]** | El diario técnico de este proyecto. Imprescindible; algunas cifras están desfasadas (lo avisa el mapa 01) |
| Medio | **LAS Recomp**: <https://github.com/peiikai/LASRecomp> **[G]** | El proyecto del que desciende Savage Wind. Mismo esquema, juego más simple |
| Avanzado | **smo-recomp**: <https://github.com/coz-0/smo-recomp> **[G]** | Otro recomp de Switch con límites de función por tablas de desenrollado y diferencial contra Unicorn |
| Avanzado | **nxnative**, `docs/agents/`: <https://github.com/seedds/nxnative> **[G]** | La guía paso a paso más completa para Switch (extraer, sondear, arrancar, gráficos, depurar). Otra técnica, mismo método. La guía de investigación avisa de revisar su historial |
| Índice | awesome-switch-recomp: <https://github.com/AIshihzong/awesome-switch-recomp> **[G]** | Catálogo; úsalo como índice, no como fuente de verdad |

## 4. La consola y el juego

| Recurso | Por qué importa |
|---|---|
| **switchbrew**: <https://switchbrew.org/wiki/Main_Page> **[G]** | La documentación comunitaria del sistema de la Switch: llamadas al sistema, servicios, formatos |
| switchbrew, formato NSO: <https://switchbrew.org/wiki/NSO> **[V]** | La cabecera que lee `tools/nso.py`: segmentos, identificador de módulo, compresión por segmento |
| **Ryujinx** (MIT): <https://github.com/Ryujinx/Ryujinx> **[G]** | Emulador con licencia permisiva; referencia de comportamiento de servicios y de shaders que el propio repo acredita |
| **ZeldaMods wiki**: <https://zeldamods.org/wiki/Main_Page> **[V]** | Documentación de **formatos de datos** de BotW (categoría de formatos de fichero), útil para entender qué genera `tools/online_mod.py`. **Cuidado**: la portada enlaza también a repositorios de decompilación y a guías de volcado. Usa solo la parte de formatos; lo demás queda fuera por las reglas del proyecto (módulo 03 §8) |

## 5. Ingeniería inversa

| Recurso | Por qué importa |
|---|---|
| **Ghidra**: <https://github.com/NationalSecurityAgency/ghidra> **[V]** (Apache 2.0) | La herramienta. El "Getting Started" viene en la instalación y en `GhidraDocs/` del repositorio |
| **Ghidra Switch Loader**: <https://github.com/Adubbz/Ghidra-Switch-Loader> **[R: `tools/ghidra/README.md:9`] [V]** (ISC) | La extensión que permite a Ghidra abrir ejecutables de Switch. Debe coincidir con la versión exacta de Ghidra |
| `tools/ghidra/README.md` del repo **[R]** | El flujo y la regla de evidencia de este proyecto |
| Capstone y Unicorn **[sin URL: las guías los nombran sin enlace]** | Desensamblador y emulador de CPU que usan `gen.py` y las pruebas. Sus tutoriales de Python bastan |

## 6. Desarrollo con specs y agentes

| Recurso | Por qué importa |
|---|---|
| **GitHub Spec Kit**: <https://github.com/github/spec-kit> **[V]**; referencia: <https://github.github.com/spec-kit/reference/agentic-sdd.html> **[G]** | El esquema constitución → especificar → plan → tareas. Ojo: no paraleliza por ti (lo aclara la guía experta) |
| Worktrees en Claude Code: <https://code.claude.com/docs/en/worktrees.md> **[G]** | Cómo aislar cada agente en su carpeta |
| Anthropic, "Building a C compiler" con agentes en paralelo: <https://anthropic.com/engineering/building-c-compiler> **[G]** | La lección central: sin un verificador casi perfecto y una CI que frene regresiones, el paralelismo no funciona |
| Martin Fowler, comparativa de herramientas SDD: <https://www.martinfowler.com/articles/exploring-gen-ai/sdd-3-tools.html> **[G]** | Visión crítica y sobria del enfoque |

## 7. Comunidad, polémica y legal

| Recurso | Por qué importa |
|---|---|
| Kotaku, la disputa por los recomps hechos con IA (05-10-2026): <https://kotaku.com/zelda-and-banjo-recompiled-devs-go-to-war-with-ai-vibe-coded-slopcomps-2000739936> **[G]** | La crítica técnica concreta a la que responde el módulo 07 §6 |
| PC Gamer, el "campo de batalla" del vibe-coding: <https://www.pcgamer.com/gaming-industry/pc-ports-of-old-console-games-are-the-new-ai-vibe-coding-battleground/> **[G]** | Contexto del debate |
| PC Gamer, retirada del mod multijugador de BotW (2023): <https://www.pcgamer.com/aggressive-nintendo-copyright-claims-on-youtube-push-breath-of-the-wild-multiplayer-modders-into-taking-down-mod> **[V: titular comprobado; el cuerpo del artículo no cargó]** | El precedente directo de lo que le pasa a un multijugador de BotW con mucha visibilidad (módulo 08 §6.3) |
| `CONTRIBUTING.md`, `AGENTS.md` y la sección "Respect copyright" del `README.md` del repo **[R]** | Las reglas que de verdad te obligan. Léelas enteras una vez |

## 8. Qué leer primero si solo tienes una tarde

1. Gambetta, partes I y III (40 minutos).
2. Gaffer, "Snapshot Interpolation" (30 minutos).
3. Módulo 05 de esta carpeta, con `runtime/online.c` abierto (2 horas).
4. README de XenonRecomp, solo la parte de cómo funciona (30 minutos).

## 9. Recursos que se pidieron y no pude comprobar

- **Valve, "Source Multiplayer Networking"** y **"Latency Compensating Methods…"** (Yahn Bernier): la wiki de
  Valve devolvió 403 a la comprobación automática. Existen y son clásicos; ábrelos desde tu navegador
  buscándolos por título.
- **Página de ENet con la documentación** (`enet.bespin.org`): fallo de certificado al comprobar. El
  repositorio de GitHub sí está verificado y enlaza su documentación.
- Detalles del caso de 2023 (número de reclamaciones, fechas exactas): salen de resultados de búsqueda de
  prensa, no de artículos leídos completos. Trátalos como **[sin verificar]**.
