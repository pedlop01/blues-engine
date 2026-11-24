# Paridad de experiencia: Fox y Prehistorik 1 con Blues

Objetivo acordado tras TH-11: **las mismas opciones y funcionamiento de Blues**,
conservando cada juego y su HUD. La separación de procesos no limita este
objetivo; la cámara, el render, el estado y el audio deben adaptarse en cada
motor. Reconocer una capacidad en el launcher no la implementa en el juego.

TH-14 y TH-15 están implementadas y aceptadas. PX amplía ahora las ediciones
DOS, con sus simulaciones propias; no sustituye ni modifica esas entregas Amiga.
TH-16 está cancelada / no aplicable: Prehistorik 2 no salió para Amiga.

**24-11-2026: PX-01–PX-08 cerradas al 100 % por aceptación del usuario:**
«funcionan perfecto!».
Detalle, controles, límites y pruebas en [DOS_PARITY_RUNTIME.md](DOS_PARITY_RUNTIME.md).
La aceptación cierra este bloque; no equivale a documentar campañas completas
o pruebas de todos los dispositivos físicos. Ese repaso queda para la distribución.

| ID | Tarea y criterio de aceptación | Fox DOS | Prehistorik 1 DOS | Dependencia |
|---|---|---:|---:|---|
| PX-01 | Scroll super suave: desacoplar lógica y presentación a 30/60/120 Hz, interpolar cámara y sprites; no alterar física, colisiones o velocidad; comprobar bordes, escaleras, plataformas, golpes y cambios de habitación | 100 % | 100 % | TH-12 |
| PX-02 | Cámara original/suavizada y zoom real del mapa, con transiciones suaves, límites correctos y HUD original independiente del encuadre | 100 % | 100 % | PX-01 |
| PX-03 | Menú in-game común centrado, misma navegación/tecla de pausa y retorno que Blues; controles editables, guardados/ayudas/accesos sin duplicados | 100 % | 100 % | PX-02 |
| PX-04 | Volúmenes música/efectos independientes y sin petardeos; persistencia por capas, mute/restauración, pausa y cambios de dispositivo probados | 100 % | 100 % | PX-03 |
| PX-05 | Invulnerabilidad, vidas ajustables/infinitas, selector de nivel y reinicio/selección de personaje cuando el juego la tenga; no inventar personajes o niveles | 100 % | 100 % | PX-03 |
| PX-06 | Ocho slots con miniaturas y resumen, carga desde juego/biblioteca, formato versionado y validación de recursos; guardar estado completo, no solo posición/nivel | 100 % | 100 % | PX-03 |
| PX-07 | Rebobinado de varios segundos, simulación visible marcha atrás y efecto VHS como Blues; RNG, objetos, enemigos, scroll y estado de audio coherentes tras reanudar | 100 % | 100 % | PX-06 |
| PX-08 | Regresión de paridad: matriz equivalente a Blues con rutas/campañas, temporización, controles, ayudas, ocho slots y rebobinado; Linux/Windows y dispositivos reales | 100 % | 100 % | PX-01–PX-07 |

Los porcentajes miden implementación **y verificación de paridad**, no la mera
presencia de un menú o una función parecida en el motor de origen. Las funciones anunciadas están implementadas en los adaptadores DOS y el usuario
ha aceptado el resultado tras las pruebas reproducibles y su revisión jugando.

## Límites que deben conservarse

- Las simulaciones originales siguen a su frecuencia nativa; aumentar los frames
  de presentación no acelera al personaje ni mueve colisiones entre ticks.
- La cámara interpola posiciones del mundo y hace cortes explícitos en puertas,
  teletransportes, respawns y selección de nivel. El HUD conserva el estilo de
  cada versión y su encuadre al hacer zoom.
- Guardados y rewind necesitan conocer todos los estados y reconstruir sus
  referencias; no reutilizar BBS, offsets o punteros de Blues para otros juegos.
- Las opciones globales se habilitan después de implementar/probar el adaptador;
  el menú compartido no puede prometer funciones que el motor ignore.
- Selección/cambio DOS–Amiga tendrá el mismo comportamiento que Blues **cuando
  existan estados equivalentes comprobados**. Las ediciones DOS y Amiga ya se
  eligen en la biblioteca; el cambio durante la acción sigue reservado a Blues.
- Se mantienen la procedencia, revisión y avisos de cada motor y los originales
  de solo lectura. El trabajo no convierte los motores en un único código base.
