# Paridad de experiencia: Fox y Prehistorik 1 con Blues

Objetivo acordado tras TH-11: **las mismas opciones y funcionamiento de Blues**,
conservando cada juego y su HUD. La separación de procesos no limita este
objetivo; la cámara, el render, el estado y el audio deben adaptarse en cada
motor. Reconocer una capacidad en el launcher no la implementa en el juego.

Esta fase va después de validar la integración actual en TH-12. Se priorizan
scroll suave, cámara y encuadre antes de ampliar a nuevas ediciones Amiga.
TH-13–TH-15 conservan sus identificadores para investigación y soporte Amiga;
las tareas PX no las sustituyen ni dan por soportadas esas ediciones.
TH-16 está cancelada / no aplicable: Prehistorik 2 no salió para Amiga.

| ID | Tarea y criterio de aceptación | Fox DOS | Prehistorik 1 DOS | Dependencia |
|---|---|---:|---:|---|
| PX-01 | Scroll super suave: desacoplar lógica y presentación a 30/60/120 Hz, interpolar cámara y sprites; no alterar física, colisiones o velocidad; comprobar bordes, escaleras, plataformas, golpes y cambios de habitación | 0 % | 0 % | TH-12 |
| PX-02 | Cámara original/suavizada y zoom real del mapa, con transiciones suaves, límites correctos y HUD original independiente del encuadre | 0 % | 0 % | PX-01 |
| PX-03 | Menú in-game común centrado, misma navegación/tecla de pausa y retorno que Blues; controles editables, guardados/ayudas/accesos sin duplicados | 0 % | 0 % | PX-02 |
| PX-04 | Volúmenes música/efectos independientes y sin petardeos; persistencia por capas, mute/restauración, pausa y cambios de dispositivo probados | 0 % | 0 % | PX-03 |
| PX-05 | Invulnerabilidad, vidas ajustables/infinitas, selector de nivel y reinicio/selección de personaje cuando el juego la tenga; no inventar personajes o niveles | 0 % | 0 % | PX-03 |
| PX-06 | Ocho slots con miniaturas y resumen, carga desde juego/biblioteca, formato versionado y validación de recursos; guardar estado completo, no solo posición/nivel | 0 % | 0 % | PX-03 |
| PX-07 | Rebobinado de varios segundos, simulación visible marcha atrás y efecto VHS como Blues; RNG, objetos, enemigos, scroll y estado de audio coherentes tras reanudar | 0 % | 0 % | PX-06 |
| PX-08 | Regresión de paridad: matriz equivalente a Blues con rutas/campañas, temporización, controles, ayudas, ocho slots y rebobinado; Linux/Windows y dispositivos reales | 0 % | 0 % | PX-01–PX-07 |

Los porcentajes miden implementación **y verificación de paridad**, no la mera
presencia de un menú o una función parecida en el motor de origen. Olduvai tiene
opciones nativas propias, pero todavía no se anuncian como equivalentes a Blues.
Cada tarea se cerrará por juego con pruebas reproducibles y revisión visual.

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
  existan ambas ediciones implementadas y recursos compatibles**. Hasta entonces,
  las ediciones Amiga de Fox/Prehistorik no se simulan ni se etiquetan como listas.
- Se mantienen la procedencia, revisión y avisos de cada motor y los originales
  de solo lectura. El trabajo no convierte los motores en un único código base.
