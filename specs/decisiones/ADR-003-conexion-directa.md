# ADR-003: Conexión directa, sin servidor intermedio

- **Estado:** aceptada (plan aprobado)
- **Fecha:** 2026-11-24
- **Afecta a:** F1 (CU-01), F3 (CU-04, CU-05)

## Contexto

Dos jugadores, normalmente amigos, cada uno en su casa o en la misma red. El PR #294 ya funciona así: un jugador
crea la sesión y su propio juego arranca un servidor TCP (puerto 52520 por defecto); el otro escribe su dirección y
se conecta (`runtime/online.c:1347-1368`). Lo que el PR **no** tiene es ninguna ayuda para llegar hasta esa máquina:
ni descubrimiento en red local, ni apertura automática de puerto, ni código de invitación, ni reconexión. La
interfaz se limita a pedir que se redirija el puerto en el router o se use una VPN (`runtime/ui/online_ui.mm:57-59`).

El proyecto no tiene presupuesto: ni donaciones ni versiones de pago. Un servidor intermedio costaría dinero y
habría que mantenerlo.

## Decisión

**Conexión directa**: un jugador hospeda, el otro se conecta a esa máquina. No hay servidor de encuentro, ni relé,
ni cuentas. El anfitrión de la partida es a la vez el servidor.

Para que conectar sea llevadero, F3 añade (CU-04): descubrimiento en red local, código de invitación, apertura
automática de puerto con UPnP donde el router lo permita, e IPv6 completo; y reconexión (CU-05).

## Alternativas descartadas

| Alternativa | Por qué no (por ahora) |
|---|---|
| Servidor de relé propio | Coste de alojamiento y mantenimiento; un punto de fallo; por él pasaría el tráfico de los jugadores |
| Servidor de encuentro + *hole punching* (los dos PC se abren paso con ayuda de un tercero) | Sigue necesitando un servidor público; funciona bien sobre UDP, y hoy el transporte es TCP (ADR-005) |
| Servicio de terceros (plataformas de juego, redes de relé) | Dependencia externa, cuentas y condiciones de uso ajenas |
| Servidor dedicado (`savagewind.exe --server`) | Existe, pero en ese modo nadie tiene autoridad y el mundo compartido queda inerte (`runtime/online.c:412-423`) |

## Consecuencias

- **El anfitrión tiene que ser alcanzable.** Sin UPnP, hay que abrir el puerto a mano o usar una VPN. Hay routers y
  proveedores (los que comparten una dirección pública entre muchos clientes) con los que la conexión directa por
  IPv4 **no es posible**; se documenta con claridad en vez de prometerlo.
- **El anfitrión acepta conexiones de Internet.** Por eso todo lo que entra por red es hostil
  (`contratos/validacion-red.md`) y los fallos del servidor que hoy permiten congelar o llenar la sesión se arreglan
  en F3.
- **La dirección IP del anfitrión la ve el cliente.** Aceptable entre amigos; se avisa en la interfaz.
- **Si el anfitrión se va, la sesión termina.** No hay migración de anfitrión. Es coherente con ADR-002: el mundo es
  suyo.
- **Las pruebas no necesitan infraestructura**: dos juegos en un PC, o el bot de pruebas, bastan.
- Si más adelante se quiere un relé o *hole punching*, el transporte (ADR-005) debe permitirlo; se reabriría esta
  decisión con un ADR nuevo.
