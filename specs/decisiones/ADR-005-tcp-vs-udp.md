# ADR-005: Transporte — seguir en TCP o pasar a UDP

- **Estado:** **pendiente (se decide en F3)**
- **Fecha de apertura:** 2026-11-24
- **Afecta a:** F3; `contratos/protocolo.md` (trama y reparto de mensajes)

> Este ADR **no toma la decisión**. Expone las opciones y fija qué medidas la decidirán. Hasta que se cierre, el
> transporte sigue siendo TCP y nadie debe implementar otro.

## Contexto

Hoy todo viaja por **una única conexión TCP** por cliente: el estado del jugador y su esqueleto 20 veces por
segundo, los enemigos 10 veces por segundo, y también los sucesos sueltos (golpes, botín, chat)
(`runtime/online.c:1-7`).

TCP entrega todo y en orden. Eso es bueno para los sucesos y malo para las posiciones:

- **Bloqueo de cabeza de línea.** Si se pierde un paquete, todo lo que llegó después espera a que se reenvíe, aunque
  sea una posición ya caducada. Luego llega todo de golpe.
- **El receptor fecha cada instantánea con su hora de llegada**, no con la del emisor (`runtime/online.c:910`). Una
  ráfaga produce varias instantáneas con casi la misma hora y el movimiento interpolado **salta**.
- **Los envíos son bloqueantes y no hay cola de salida** (`runtime/online.c:140-148`): un cliente lento detiene al
  servidor entero. (Esto se arregla en F3 con cualquier opción.)

Datos que pesan en la decisión:

- Un STATE mide **3375 bytes**: no cabe en un solo datagrama UDP típico (unos 1200-1400 bytes útiles). Con UDP hay
  que fragmentarlo o reducirlo.
- Cada jugador sube unos **100 kB/s** porque se envía el estado completo aunque nada cambie (calculado, no medido).
- La conexión es **directa** (ADR-003): no hay un tercero que ayude a atravesar routers.
- El repositorio ya tiene un ejemplo de cada cosa: TCP con hilo y `select` (`runtime/control.c`) y UDP no bloqueante
  sondeado (`runtime/dsu.c`).

## Opciones

### A. Seguir en TCP y arreglarlo

Envío no bloqueante con cola por cliente; **descartar el estado viejo** en la cola (si hay un STATE sin enviar y
llega otro, se sustituye); marca de tiempo del emisor; reconexión.

- A favor: el cambio más pequeño; sin dependencias; los sucesos siguen llegando seguros; abrir un puerto TCP es lo
  que la gente ya sabe hacer.
- En contra: el bloqueo de cabeza de línea **sigue ahí**; solo se suavizan sus efectos.

### B. ENet sobre UDP

ENet es una biblioteca pequeña en C que, sobre UDP, ofrece canales fiables y no fiables, con fragmentación.
STATE, SKEL y ENEMIES irían por un canal no fiable; los sucesos, por uno fiable.

- A favor: elimina el bloqueo de cabeza de línea para el estado; fiabilidad por canal sin escribirla nosotros.
- En contra: dependencia nueva en `third_party/` (licencia y documentación a actualizar; compilar en Windows, Linux
  y macOS); se reescribe la trama; un mensaje no fiable fragmentado se pierde entero si se pierde un fragmento, así
  que conviene reducir STATE primero; cómo encaja el bucle de ENet con el hilo de servidor actual está
  `[sin verificar]`.

### C. Híbrido: TCP para sucesos, UDP propio para el estado

Mantener la conexión TCP para lo fiable y abrir un socket UDP aparte para STATE, SKEL y ENEMIES.

- A favor: sin dependencia externa; los sucesos no se tocan.
- En contra: dos puertos que abrir y dos caminos que mantener; hay que escribir numeración, fragmentación y la
  asociación de cada datagrama con su sesión; más superficie que validar.

### D. UDP propio para todo

Escribir nuestra propia capa fiable sobre UDP.

- En contra: es reescribir lo que ENet ya hace, con más riesgo. Solo se consideraría si B se descarta por motivos de
  licencia o de compilación.

## Qué medidas decidirán

Se miden **A** (como línea base, ya con los arreglos de F3) y **B** en un prototipo mínimo. Montaje: Docker en
Linux, servidor en bucle local, dos bots (`tools/online_bot.c`), red degradada con `tc netem`.

Condiciones de red a probar (cada una, al menos 5 minutos):

| Caso | Retardo | Variación | Pérdida |
|---|---|---|---|
| Red local | ~1 ms | ~0 | 0 % |
| Internet buena | 30 ms | 5 ms | 0,1 % |
| Internet normal | 60 ms | 15 ms | 1 % |
| Wi-Fi mala | 80 ms | 30 ms | 3 % |
| Muy mala | 150 ms | 50 ms | 5 % |

Qué se anota en `bitacora/`, por opción y por caso:

1. **Intervalo entre instantáneas en el receptor**: mediana, percentil 95 y percentil 99 (lo ideal son 50 ms).
2. **Huecos**: cuántas veces por minuto pasan más de 150 ms sin instantánea nueva (el interpolador se queda sin
   datos, porque dibuja 100 ms por detrás).
3. **Ráfagas**: cuántas veces por minuto llegan 3 o más instantáneas en menos de 10 ms.
4. **Retardo de un suceso fiable** (un HIT, un mensaje de chat): mediana y percentil 99.
5. **Sucesos perdidos**: debe ser 0 en todas las opciones.
6. **Ancho de banda** de subida por jugador.
7. **Coste**: líneas cambiadas, dependencias añadidas, y si compila en las tres plataformas.

Además, una prueba **en el juego real** con las dos condiciones intermedias: ¿se ve saltar al compañero? Una
diferencia que no se nota jugando no justifica el cambio.

Aviso: el bot actual no envía SKEL ni PING y su buffer de recepción es de 8192 bytes, menor que un ENEMIES grande
(`tools/online_bot.c:107`). Hay que ampliarlo antes de medir.

## Criterio de decisión

Los umbrales numéricos exactos se fijan en el `plan.md` de F3, **antes** de medir, para no ajustarlos al resultado.
La forma del criterio será:

- Si **A** mantiene los huecos y las ráfagas por debajo del umbral en "Internet normal" y no se ven saltos en el
  juego: **nos quedamos en TCP** y este ADR se cierra como "A".
- Si **A** falla ahí y **B** lo cumple, con sucesos perdidos = 0 y compilando en las tres plataformas: **B**.
- Si ninguna cumple: primero reducir el tamaño de STATE (enviar solo lo que cambia) y repetir la medida.
- **C** y **D** solo se evalúan si **B** se descarta por licencia o compilación.

## Lo que no depende de esta decisión (se hace igual)

- Envío no bloqueante con cola por cliente.
- Marca de tiempo del emisor en el estado.
- Validación de entradas y autoridad en el servidor.
- Reconexión.
- Una única definición de las estructuras de cable, compartida con el bot.

## Consecuencias previstas según el resultado

- **Si A:** `contratos/protocolo.md` cambia poco; se documenta el bloqueo de cabeza de línea como limitación
  conocida.
- **Si B:** cambia la trama completa y hay que repartir cada mensaje entre canal fiable y no fiable (por definir en
  F3); UPnP y el descubrimiento pasan a ser sobre UDP; la apertura manual de puerto cambia de TCP a UDP y hay que
  explicárselo al jugador; nueva entrada en la documentación de terceros.
- En ambos casos, el resultado y las medidas quedan en `bitacora/` y este archivo pasa a "aceptada" con la opción
  elegida.
