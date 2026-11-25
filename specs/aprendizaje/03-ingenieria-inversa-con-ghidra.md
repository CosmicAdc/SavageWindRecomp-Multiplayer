# 03 · Ingeniería inversa con Ghidra (sobre tu propia copia)

Objetivo: un flujo de trabajo legítimo y repetible para contestar "¿dónde hace el juego X?" y convertir la
respuesta en un sitio por patrón. Lo usarás poco en F1-F4 y mucho en F5.

**Límites de este documento.** No contiene código del juego, ni pseudocódigo, ni direcciones u offsets que no
estén ya impresos en ficheros del repo. Lo que saques de Ghidra **se queda en tu máquina**.

---

## 1. Qué es y qué no es

**Ingeniería inversa** (RE) aquí significa: mirar el programa compilado de tu copia para entender qué hace una
función concreta, lo justo para engancharse a ella.

- **Ghidra** es un entorno de RE gratuito (licencia Apache 2.0). Tiene dos vistas que usarás siempre juntas:
  el **desensamblado** (instrucciones reales) y el **decompilador** (pseudo-C deducido, cómodo pero a veces
  equivocado).
- No buscamos reconstruir el juego. Buscamos **puntos de enganche**: una función, un campo, un global.
- El `main` del juego no tiene símbolos ni RTTI (`docs/ONLINE.md:99`): no hay nombres de funciones. Todo nombre
  que veas lo ha puesto alguien, y debe decir de dónde sale (§8).

## 2. Montar el proyecto (una vez)

La receta del repo está en `tools/ghidra/README.md:7-30`:

1. **Ghidra 12.1.2** con la extensión **Switch Loader** compilada para esa versión exacta, descomprimida en
   `<ghidra>/Ghidra/Extensions/` (`README.md:9-11`). Las extensiones solo cargan en la versión para la que se
   compilaron.
2. **JDK 21** o posterior (`README.md:12`).
3. El proyecto vive **junto** al repo, nunca dentro: `../ghidra/` (`README.md:3-4`).
4. Importa y analiza tu `extracted/exefs/main` de 1.9.0, y luego `sdk` y `subsdk0`. El análisis es largo (el
   README da unos 22, 6 y 3 minutos con 8 hilos en su máquina de referencia, `README.md:13-19`) y se hace con
   `analyzeHeadless` (sin interfaz).
5. Di a `gq.py` dónde está todo, por variables de entorno o en `tools/ghidra/gq.local.json`, que git ignora
   (`README.md:24-30`). Ahí van rutas de tu máquina: no las subas.

En tu equipo: el análisis pide memoria (el ejemplo usa 12 GB de máximo, `README.md:17`). Con 15,7 GB cierra
Docker y el navegador, o baja el máximo y acepta que tarde más **[sin verificar en este equipo]**.

### El desfase de direcciones

El Switch Loader coloca `main` en `0x7100000000`; el generador y el runtime usan `0x7000000000`
(`README.md:45-46`). `gq.py` convierte solo: le das una dirección de runtime (la que sale en un log o en un
crash) y la traslada a la de Ghidra, avisando por stderr. Si usas la interfaz gráfica, la conversión la haces
tú.

## 3. Las herramientas del repo

`tools/ghidra/gq.py` lanza Ghidra sin interfaz, en **solo lectura**, con el script `SWQuery.java`. Comandos
(`gq.py:5-12`):

| Comando | Para qué |
|---|---|
| `summary` | comprobar que el proyecto abre |
| `strings <regex> [max]` | cadenas que casan, **con las funciones que las usan** |
| `xrefs <addr\|nombre>…` | quién referencia una dirección (quién llama a una función) |
| `callees <addr\|nombre>…` | a quién llama una función |
| `decompile <addr\|nombre>…` | pseudocódigo de la función que contiene esa dirección |
| `disasm <addr\|nombre> [n]` | `n` instrucciones desde una dirección |
| `symbols <regex>`, `functions` | listados |

Dos normas de uso (`README.md:41-48`):

- Cada llamada arranca Ghidra (~medio minuto): pasa varias direcciones en una sola llamada.
- **La salida se imprime, no se guarda.** Deriva del código del juego y no puede entrar en el repositorio. Ni
  en un commit, ni en un issue, ni pegada en `specs/`.

Además tienes herramientas que **no** necesitan Ghidra y que a menudo son más rápidas:

| Herramienta | Qué da | Dónde |
|---|---|---|
| `actors <texto>` (control) | actores vivos cuyo nombre contiene el texto: posición, distancia, dirección | `docs/CONTROL.md:88` |
| `sites` (control) | las direcciones raíz conocidas y su valor ahora | `docs/CONTROL.md:97` |
| `mem read` / `mem scan` / `mem rescan` / `mem path` | leer memoria, buscar un valor, filtrar por "cambió / no cambió", y hallar la cadena de punteros desde una raíz | `docs/CONTROL.md:105-109` |
| `SAVAGEWIND_FNPROF=<desde>:<hasta>` | cuenta entradas a **cada** función del juego entre dos fotogramas | `runtime/fnprof.c:1` |
| `SAVAGEWIND_FNPROF_ARGS=<dir>` | registra `x0`-`x5` y quién llama, cada vez que se entra en esa función | `runtime/fnprof.c:47-49` |
| `SAVAGEWIND_FNPROF_BT=<dir>` | traza de llamadas la primera vez que se entra | `runtime/fnprof.c:17` |

## 4. Encontrar una función

### 4.1 Desde una cadena

Es el camino más fiable. Los juegos están llenos de nombres: actores, animaciones, flags, mensajes de error.

1. Piensa qué texto tiene que manejar el código que buscas. Ejemplo del propio README:
   `gq.py strings "Actor/.*bactorpack" 20` (`README.md:38`).
2. La salida te da las funciones que usan cada cadena.
3. `xrefs` sobre la candidata para ver quién la llama; `callees` para ver qué hace.
4. `decompile` solo de las dos o tres que queden.

Los nombres de flags y de actores los tienes en tus propios datos del juego (el RomFS), y el runtime ya trabaja
con ellos por nombre: mira `quests.txt` en `runtime/online_actors.c:3859-3877`.

### 4.2 Desde un comportamiento conocido

Cuando no hay cadena, se usa el juego en marcha como experimento. Tres técnicas que el PR documenta haber usado:

**a) Contar llamadas con y sin el comportamiento.** Graba con `SAVAGEWIND_FNPROF` un tramo de fotogramas normal
y otro con el comportamiento activo (un menú abierto, una habilidad). Las funciones que solo aparecen, o cuyo
contador cambia, en el segundo tramo son candidatas. Así se localizó lo que el juego hace al pausar con un menú
(lo cuenta `docs/ONLINE.md` en la sección de las reglas de sesión; referencia en
`../mapa/07-pr294-integracion-juego.md` §3.6).

**b) Mirar los argumentos reales.** Con una candidata, `SAVAGEWIND_FNPROF_ARGS` te dice con qué se la llama, sin
depurador. Si `x0` es siempre el actor de Link, ya sabes de quién es el método.

**c) Diferencias de memoria (*memory diffing*).** Es la receta de `docs/ONLINE.md:767-768` para los efectos de
una habilidad:

> diff the player's memory (...) before and while it is used; a handle that appears is the effect's

En pasos:

1. Localiza el objeto (el jugador sale del sitio `player_info`; `actors` te da otros).
2. Vuelca un rango de ese objeto en reposo (`mem read`).
3. Activa el comportamiento y vuelve a volcar.
4. Compara. Lo que cambia y vuelve a su valor al terminar es estado de ese comportamiento.
5. Repite para descartar ruido (contadores, temporizadores).
6. Interpreta cada cambio: ¿un puntero que aparece?, ¿un bit?, ¿un `float` que crece?

Para valores cuyo sitio no conoces, `mem scan <tipo> <valor>` seguido de `mem rescan changed|unchanged|…`
estrecha la búsqueda como un "buscador de trucos", y `mem path <raíz> <destino>` te da la cadena de punteros
desde una raíz conocida (`docs/CONTROL.md:108-109`).

Después vas a Ghidra **con una pregunta concreta**: "¿qué función escribe este campo?". Eso es mucho más barato
que leer funciones al azar.

## 5. Leer estructuras por patrones de acceso

No hay definiciones de tipos. Se reconstruyen así:

- **Un registro base con muchos desplazamientos** (`[x19, #0x10]`, `[x19, #0x48]`…) es un objeto; cada
  desplazamiento, un campo.
- **El tamaño del acceso da el tipo**: carga de 32 bits (`w`) = entero o `float`; de 64 (`x`) = puntero o entero
  largo; a registro `s`/`d` = coma flotante.
- **La primera palabra de un objeto C++ suele ser su tabla de métodos virtuales (vtable).** Un
  `ldr x8, [x0]` seguido de `ldr x8, [x8, #N]` y `blr x8` es "llamar al método virtual número N/8". El propio
  runtime aprovecha esto para dar pistas en un crash: imprime a qué apunta cada registro si es una dirección del
  módulo del juego (`runtime/dispatch.c:234-236`, `:248-253`).
- **Un bucle con paso fijo** revela un array y el tamaño de su elemento.
- **Comprueba en vivo.** Una hipótesis ("este campo es la vida") se confirma leyendo el campo con `mem read`
  mientras el valor cambia en pantalla. Sin esa comprobación es una conjetura.

Cómo queda documentado un hallazgo en este repo: un comentario junto al uso, con qué es y cómo se supo. Buen
ejemplo, la explicación de por qué existe `has_body()` (`runtime/online_actors.c:67-72`): cuenta qué campo es,
en qué clases significa otra cosa y qué cuelgue lo demostró.

## 6. Anotar con evidencia

Regla de trabajo del repo (`tools/ghidra/README.md:50-55`): toda afirmación sobre lo que hace una función cita
su evidencia (dirección, instrucciones relevantes, llamadores) y todo nombre dice de dónde salió.

Plantilla para tus notas **locales** (fuera del repo):

```
Pregunta:     ¿qué función decide X?
Candidata:    <dirección>          (versión 1.9.0)
Evidencia:    cadena "<…>" referenciada; llamada desde <…>; FNPROF: 0 entradas sin X, N con X
Argumentos:   x0 = <qué>, x1 = <qué>   (visto con FNPROF_ARGS)
Comprobado:   <qué hice en el juego y qué vi>
Sin comprobar:<qué supongo>
```

Al repo solo sube la **conclusión en tus palabras** (el comentario del sitio y del parche), nunca el pseudocódigo.

## 7. Del hallazgo a un sitio por patrón

Escribir `#define FN_ALGO 0x70…` funciona hoy y es deuda mañana (módulo 02 §7). El camino bueno:

1. **Genera el patrón** con la herramienta del repo (`tools/game_sites.py:1106-1111`):

   ```
   cd tools
   ../tools/venv/Scripts/python game_sites.py pattern <tu exefs>/main <dirección> [code|call|ref|got|imm]
   ```

   Imprime una línea `Site('<name>', '<kind>', '<patrón>', <at>),  # -> <valor>` lista para pegar. La
   herramienta se queda dentro de la función, enmascara lo que se mueve y comprueba que el patrón es único.

2. **Elige bien el tipo**:
   - quieres **sustituir** una instrucción → `code`;
   - quieres **llamar** a una función → `call` sobre un `bl` que la llame (el sitio vale el destino);
   - quieres la dirección de un **global** → `ref` sobre el `adrp+add`, o `got` si es un puntero en tabla;
   - quieres un **offset de campo** que puede moverse → `imm`.

3. **Regístralo** en `SITES`, añade su dirección a `KNOWN['1.9.0']` y mételo en un **grupo opcional propio** en
   `OPTIONAL_GROUPS` (`game_sites.py:500`). Así, si el patrón falla en una versión futura, se apaga tu función
   y no el arranque del juego.

4. Si el runtime necesita la dirección, decláralo en `runtime/game_sites.h` (en la lista opcional) y úsalo como
   `g_site.<nombre>`, **comprobando que no sea 0** antes de usarlo. Modelo: `runtime/online.c:829`.

5. **Verifica**: `game_sites.py find <tu main>` debe listar el sitio como `ok` (`game_sites.py:1112-1128`), y
   `tools/test_game_sites.py` debe seguir pasando.

6. Si hay parche, escribe su prueba sintética copiando `tools/test_act_site.py` (instrucciones inventadas, sin
   ficheros del juego).

Receta completa con plantillas de parche: `../mapa/01-recompilador.md` §4.

**Cuándo un patrón es malo:** si incluye offsets de estructura que cambian entre versiones, si es demasiado
corto (varias coincidencias) o si cruza el final de la función. La herramienta evita lo segundo y lo tercero; lo
primero lo ves tú.

## 8. La regla contra copiar de proyectos de decompilación

**La regla.** `CONTRIBUTING.md:19-21`: ningún código, cabecera ni documentación copiados de proyectos de
decompilación; el código de Savage Wind se escribe a partir de lo que se encuentra en la copia propia.
`AGENTS.md:25-26` lo repite para agentes: "Do not copy from decompilation projects. Use synthetic test
fixtures."

**Por qué existe:**

1. **Derechos de autor.** Una decompilación reconstruye el código fuente de una obra ajena. Copiar de ahí es
   copiar, a través de un intermediario, expresión que pertenece al titular del juego. Un recomp que solo
   contiene herramientas y código propio está en una posición mucho más defendible que uno con trozos de un
   decomp dentro.
2. **Contaminación.** Un solo fichero copiado compromete el repositorio entero y a todos sus forks. No se
   arregla borrando el fichero: queda en el historial.
3. **Licencias.** Aunque el proyecto de origen declare una licencia, no puede conceder derechos sobre lo que no
   es suyo.
4. **Supervivencia.** El proyecto mantiene un perfil bajo a propósito (módulo 08). Dar un motivo claro para una
   retirada perjudica a todos los que dependen de él.

**Qué significa en la práctica para ti:**

- No abras un decomp "solo para mirar un nombre de campo" mientras trabajas en una función. El riesgo no es solo
  legal: acabas escribiendo lo que leíste.
- No pegues código del juego ni salida del decompilador **en un prompt** de un agente que vaya a escribir código
  del repo, ni en commits, issues o `specs/`.
- Si un nombre viene de una cadena del propio binario o de tus datos del juego, dilo. Si es un nombre que has
  inventado por el comportamiento, dilo también.
- Matiz honesto: el README de Ghidra del repo admite que un nombre pueda anotarse como coincidencia con un
  proyecto de decompilación (`tools/ghidra/README.md:53-55`), y los comentarios del repo usan nombres con
  espacio de nombres (`ksys::…`, `uking::…`) cuya procedencia los ficheros no explican
  (`../mapa/07-pr294-integracion-juego.md` §3.6). **Para nuestro fork la regla es la conservadora**: nombres
  propios o derivados de cadenas, con su procedencia escrita.
- Una wiki comunitaria de formatos de datos (cómo es un fichero de parámetros, qué es la tabla de tamaños de
  recursos) es documentación de **formatos**, y se usa para `tools/online_mod.py`. Aun así, entra a ella con
  cuidado: algunas enlazan a repositorios de decompilación (`recursos.md`).

## 9. Errores típicos del principiante

| Error | Antídoto |
|---|---|
| Creer al decompilador | Contrasta siempre con el desensamblado en las líneas que importan |
| Leer una función de 2.000 líneas | Llega con una pregunta concreta (§4.2) |
| Dar por bueno un offset visto una vez | Compruébalo en vivo en dos situaciones distintas |
| Guardar un puntero del juego en un global | Se invalida al volver al título: `RT_ON_RESET` (`runtime/hle.h:40`) |
| Suponer que un campo existe en todas las clases de actor | El caso `has_body()` (`online_actors.c:67-72`) |
| Subir "solo un trocito" de salida de Ghidra | No. Nunca. |

## 10. Autoevaluación

1. Te sale en un crash una dirección `0x70…` del juego. ¿Qué haces para verla en Ghidra?
2. Quieres llamar desde el runtime a una función del juego. ¿Qué tipo de sitio usas y sobre qué instrucción?
3. ¿Por qué un grupo opcional propio para tus sitios?
4. ¿Qué parte de una sesión de Ghidra puede acabar en un commit?

Respuestas: (1) se la pasas a `gq.py`, que la traslada; en la interfaz sumas el desfase tú. (2) `call`, sobre un
`bl` existente que la invoque. (3) Para que un patrón fallido desactive tu función y no impida arrancar.
(4) Solo conclusiones redactadas por ti: el `Site(...)`, su comentario y el parche.

Siguiente: [04 · Redes para juegos](04-redes-para-juegos.md).
