# 08 · Contribuir a la comunidad: etiqueta, práctica e higiene legal

Objetivo: que lo que construyas pueda compartirse sin perjudicar a nadie: ni al proyecto original, ni a los
autores del PR #294, ni a ti.

Aviso: no soy abogado y esto no es asesoría legal. Es la postura prudente que el propio proyecto adopta, con
sus citas.

---

## 1. Cómo está organizado el proyecto original

- **Un mantenedor** con ayuda de colaboradores; cada contribución la revisa y prueba él antes de entrar
  (`CONTRIBUTING.md:31-32`).
- **Sin Discord ni canales públicos**: todo pasa por GitHub. Para coordinarse, el contacto del mantenedor está
  en `CONTRIBUTING.md:29-31`.
- **Ramas**: `test` es desarrollo; `main` solo se mueve en cada versión publicada. Se trabaja sobre `test` y los
  PR van contra `test` (`CONTRIBUTING.md:42-50`).
- **Hecho mayoritariamente con IA y declarado**; se aceptan contribuciones a mano, con IA o mixtas: lo que
  cuenta es que el cambio funcione, esté probado y siga las reglas (`CONTRIBUTING.md:5-8`).

## 2. La regla dura: nada de Nintendo

`CONTRIBUTING.md:10-25`. Nunca, en commits, issues ni PR:

- copias del juego en ninguna forma, claves, firmware;
- ficheros extraídos (`extracted/`), **código generado** (`generated/`), fuentes del sistema;
- recursos del juego: texturas, modelos, shaders, música, textos, niveles, "ni un solo fichero para probar";
- enlaces a cualquiera de lo anterior, **ni instrucciones para volcar o descifrar**;
- código, cabeceras o documentación copiados de proyectos de decompilación.

Sí se puede: capturas y clips cortos en un informe de fallo, y tu propio guardado adjunto **a un issue** (no en
un commit).

Los issues y comentarios que comparten o piden copias, claves o herramientas de volcado se borran, y la cuenta
reincidente se bloquea (`CONTRIBUTING.md:34-35`).

Lo que más se olvida: **el C generado y el binario compilado contienen código de Nintendo**. Son para tu uso
personal; no se comparten ni se suben (`README.md:148-149`). Eso incluye `swgame.dll`, la carpeta `online_mod/`
(sale de tus datos del juego) y cualquier volcado de memoria.

Para agentes, `AGENTS.md:23-31` añade: ante la duda de si una operación puede sobrescribir, borrar o exponer
datos del juego, claves o ficheros extraídos, **parar y preguntar**; y mantener rutas personales y secretos
fuera de los commits.

## 3. Cómo se integra un PR allí (no es el flujo habitual de GitHub)

`AGENTS.md:127-155` y `CONTRIBUTING.md:113-118`:

1. Rama temática sobre el `test` más reciente. **Un PR = un cambio**, pequeño.
2. Sigue la plantilla (`.github/pull_request_template.md`): qué cambia, **cómo se probó** (dónde en el juego,
   sistema, GPU, qué viste antes y después) y la lista: basado en `test`, compilado y comprobado en el juego,
   sin ficheros del juego.
3. Cambios de rendimiento o memoria: números antes y después.
4. Lo que cambie el comportamiento de todos en beneficio de una configuración va **detrás de una opción,
   apagada por defecto**.
5. **Los PR no se fusionan por GitHub.** Los mantenedores aplican los PR elegidos en una rama local, los
   prueban y los publican en `test` como **un commit limpio** con una línea `Co-authored-by` por autor; luego
   comentan y cierran cada PR con el commit de integración. Los colaboradores nunca empujan a `test` ni a
   `main`.
6. Abrir un PR no autoriza a fusionarlo, a cerrar PR o issues, ni a publicar una versión.
7. Antes de resolver un conflicto, busca en el historial de upstream si el arreglo ya está integrado. No lo
   reapliques.

Consecuencia: tu PR aparecerá "cerrado", no "fusionado". Es normal. El crédito va en el `Co-authored-by`.

## 4. Postura de upstream sobre el multijugador, y qué implica

- El FAQ responde "Multiplayer? No": es una petición abierta, pero no hay nada parecido en el juego
  (`docs/FAQ.md:582-585`).
- Las peticiones que el FAQ ya contesta, multijugador incluido, se cierran de inmediato (`CONTRIBUTING.md:64`).
- El PR #294 es de terceros (byluki, mstampfli) y no está integrado.

Qué hacemos nosotros (`../PLAN-APROBADO.md`, reglas de trabajo): **todo vive en el fork**; nada se sube al
proyecto original salvo que tú lo pidas. Si algún día se propone algo, que sea lo que upstream puede aceptar:
un arreglo pequeño y general (por ejemplo, un fallo del runtime encontrado por el camino), de un solo cambio,
con su prueba. Un "PR de cooperativo" gigante no es una contribución: es una carga de revisión para una sola
persona.

No abras issues para pedir multijugador ni para compartir código (`CONTRIBUTING.md:37-40`).

## 5. Hablar con los autores del PR #294

Construimos sobre su trabajo. Lo correcto y lo útil coinciden:

- **Atribución.** Conserva su autoría en el historial y en los créditos. La licencia MIT obliga a mantener el
  aviso de copyright; la cortesía obliga a más.
- **Escríbeles pronto y en corto.** Quién eres, que partes de su PR en un fork, qué objetivo tienes
  (cooperativo de 2, anfitrión con autoridad), y una pregunta concreta. El plan lo deja como decisión tuya, no
  bloqueante.
- **Ofrece antes de pedir.** Los mapas 06 y 07 contienen hallazgos concretos con línea (envío bloqueante,
  `shield` sin terminar, estado que sobrevive a la sesión). Un informe breve, reproducible y sin tono de
  auditoría es el mejor saludo.
- **Canal.** Comentarios en el propio PR o el contacto que ellos publiquen. Sin mensajes insistentes.
- **No des por hecho que quieren lo mismo.** Su diseño es "hasta 8, cada uno con su mundo, progreso unido"; el
  nuestro, "2, mundo del anfitrión". Divergir es legítimo; hacerlo sin avisar es feo.
- **Devuelve los arreglos generales** en un formato que puedan aplicar: parches pequeños, uno por problema.

Plantilla mínima:

```
Hola. Estoy trabajando en un fork sobre vuestro PR #294 para un cooperativo de 2 jugadores con el mundo
del anfitrión como autoridad. Gracias por el trabajo: la copia de esqueleto es una solución muy buena.

Leyendo online.c encontré <1 hallazgo concreto, fichero:línea, cómo reproducirlo>.
Tengo un parche de <N> líneas, ¿os lo paso aquí o preferís otra vía?

Una pregunta: <1 pregunta concreta>.
```

## 6. Higiene legal

### 6.1 Tu copia, y solo tu copia

- Necesitas tu propio juego obtenido legalmente, en 1.9.0 (`README.md:27-29`).
- La extracción la haces tú con tu consola. El proyecto no lo documenta y esta guía tampoco.
- Volcar un juego propio para uso personal es legal en unos países y no en otros: es tu responsabilidad
  (`README.md:152-153`).

### 6.2 Qué no sale nunca de tu máquina

`extracted/`, `generated/`, `build/` con la biblioteca del juego, `online_mod/`, el proyecto de Ghidra y su
salida, volcados de memoria, guardados (salvo adjuntos a un issue), claves.

Protecciones: `.gitignore`, el check de CI "no game files" (`../mapa/05-ui-input-build-ci.md` §4.2) y **mirar
el diff antes de publicar** (`AGENTS.md:130-131`). En F0 se restauran los flujos de CI en el fork precisamente
para tener ese check.

### 6.3 Perfil bajo, y el precedente de 2023

En abril de 2023 se publicó un mod multijugador de *Breath of the Wild* para la versión de Wii U sobre el
emulador Cemu (el mismo proyecto que el PR cita como inspiración, `docs/ONLINE.md:3-6`). Según la prensa,
Nintendo emitió en pocos días decenas de reclamaciones de copyright contra **vídeos** de YouTube que lo
mostraban, y los responsables retiraron los enlaces de descarga. El titular de PC Gamer lo resume: las
reclamaciones "empujaron a los modders a retirar el mod" (`recursos.md`, sección legal). Los detalles (cifras,
qué se restauró después) varían entre medios y no los he contrastado de primera mano **[sin verificar]**.

Lecciones prácticas:

- **La visibilidad es el riesgo.** No fue el código lo que se atacó primero, fueron los vídeos virales.
- No hagas campañas, tráilers ni "lanzamientos". Si enseñas algo, que sea sobrio y técnico.
- Nunca dinero de por medio: ni donaciones "por el mod", ni recompensas, ni acceso anticipado.
- Nada de marcas ni logotipos de Nintendo en la presentación.
- Nunca binarios con código del juego. El modelo "trae tu copia y se recompila en tu máquina" es la línea que
  separa este tipo de proyecto de la distribución de un juego.
- Un modo online **privado, por conexión directa, entre personas que tienen cada una su copia** es el caso más
  defendible. Un servicio público con servidores y listas de partidas es otra cosa.

### 6.4 Lo que una retirada significaría

Una reclamación contra el fork puede arrastrar atención hacia el proyecto original. Proteger el perfil bajo es
proteger el trabajo de otros.

## 7. Licencias

### 7.1 La del proyecto

**MIT** (`LICENSE`), con copyright de Revivedx y colaboradores, y de Peiikai por LAS Recomp, sobre el que se
construye. Contribuir implica aceptar que tu aportación queda bajo MIT (`CONTRIBUTING.md:120-122`).

MIT permite usar, modificar y redistribuir con una condición: **conservar el aviso de copyright y el texto de
la licencia**. En el fork: no borres ni edites los avisos existentes; añade el tuyo si procede.

Ejemplo dentro del repo de cómo se toma código ajeno correctamente: el bloque de flags locales viene de otro
recomp con licencia MIT, y lo dice en el propio fichero con proyecto, commit, autor y ruta de la licencia
(`runtime/cpu.h:165-166`; `third_party/totk-pc-recomp/`).

### 7.2 Qué código ajeno puedes traer

| Origen | ¿Se puede copiar al fork? |
|---|---|
| MIT, BSD, zlib, ISC, Apache 2.0 | Sí, conservando avisos y anotándolo en `docs/third-party.md` (Apache 2.0 pide además indicar cambios) |
| LGPL | Solo como biblioteca enlazada aparte; no pegar su código |
| **GPL** | **No.** Mezclar código GPL obligaría a relicenciar el conjunto, incompatible con el MIT del proyecto y con volver a upstream |
| **Sin licencia** (= "todos los derechos reservados") | **No.** Que el código sea público en GitHub no da permiso para copiarlo |
| Proyectos de decompilación del juego | **No**, tengan la licencia que tengan (módulo 03 §8) |
| Fragmentos de foros, gists, respuestas sin licencia clara | No, salvo permiso expreso |

Cómo usa el repo las herramientas GPL sin contaminarse: Unicorn es GPL 2.0, pero es una **dependencia de las
herramientas de generación y de prueba**, no código copiado dentro del runtime (`docs/third-party.md:37`,
`56-58`). Usar un programa GPL no es lo mismo que incorporar su código.

### 7.3 Los proyectos de referencia: se estudian, no se copian

sm64coopdx, el port de Perfect Dark, Anchor, el lanzador del mod de 2023… sirven para aprender **ideas**
(propiedad de entidades, sincronización de flags, formato de sesión). Antes de abrir el código de cualquiera,
mira su licencia. Varios no declaran ninguna en su README (comprobado para dos de ellos, `recursos.md`), y los
basados en decompilaciones arrastran además ese problema.

Regla práctica: **lee la documentación y el diseño; escribe el código tú**. Si una idea viene de un proyecto,
cítalo en un comentario o en un ADR. Es lo que hace `docs/ONLINE.md:3-6`: nombra la inspiración y declara que
nada del código se traslada.

### 7.4 Dependencias nuevas

ENet (candidata para F3) es MIT: compatible. Aun así, toda dependencia nueva pasa por un ADR en
`../decisiones/` con: licencia, tamaño, quién la mantiene y cómo se integra (carpeta en `third_party/` con su
`LICENSE`, como las existentes).

## 8. Construir para la comunidad, más adelante

- **Lo primero es que sea fiable para dos personas.** Nadie agradece una función que rompe guardados.
- **Protege el guardado ajeno.** El diseño de autoridad del anfitrión (CU-06) existe en parte por esto: hoy un
  invitado puede cambiar para siempre el progreso de otro.
- **Documenta para el siguiente.** Esta carpeta y los contratos son parte del producto.
- **Declara cómo está hecho** (con IA, revisado por una persona) y qué está probado y qué no.
- **Informes de fallo útiles**, tanto los que escribas como los que pidas: plataforma, hardware, versión del
  juego, qué hiciste, qué pasó, log (`CONTRIBUTING.md:69-78`).
- **No prometas** fechas ni funciones.

## 9. Lista antes de publicar cualquier cosa

- [ ] `git status` y `git diff --stat` revisados: ningún fichero del juego, generado, guardado ni ruta personal.
- [ ] Ninguna salida de Ghidra ni pseudocódigo en código, commits, issues o `specs/`.
- [ ] Avisos de copyright y licencias intactos; código ajeno anotado.
- [ ] El cambio se ha visto funcionar en el juego, y está escrito cómo.
- [ ] Va al fork. Si fuera a upstream: contra `test`, un solo cambio, plantilla completa, y **lo has pedido tú**.
- [ ] Nada de marcas, vídeos promocionales ni enlaces a copias.

Siguiente: [Ejercicios](ejercicios.md) · [Recursos](recursos.md).
