# Plan de mejoras y seguimiento

Última actualización: **22 de noviembre de 2026**.

Este documento recoge el plan acordado y su estado real. Los porcentajes se
refieren a cada tarea; no representan una estimación del tiempo restante.

## Estado general

- Tareas completadas: **13 de 18**.
- Completadas: **01–13**.
- En validación: **14**.
- Corrección de escaleras validada por el usuario.
- Las demás tareas están pendientes de implementación.

## Base disponible

Estas funciones ya existen y no deben confundirse con las ampliaciones pendientes:

- Compilación Linux/WSL y Windows x64.
- Scroll con presentación a 30/60/120 Hz.
- Encuadre dinámico con `-`, `+` y `0`, con transiciones de unos 300 ms.
- Audio DOS AdLib y audio Amiga.
- Cambio completo DOS/Amiga mediante `F6`, conservando la partida.
- Ayudas por argumentos: evitar daño, vidas/energía ilimitadas y nivel inicial.
- Pruebas de audio, cambios de modo, puertas, zoom y enemigos derrotados.

Se han corregido los problemas detectados en habitaciones, desaparición de
enemigos con zoom y alineación de los sprites al cambiar de versión.
El ajuste de audio a 44,1 kHz está implementado, probado automáticamente y
confirmado por el usuario: el audio DOS funciona correctamente en Windows.

## Tabla de tareas

| ID | Tarea | Alcance y criterio de finalización | Dependencias | Estado | Progreso |
|---|---|---|---|---|---:|
| 01 | Consolidar el proyecto | Ordenar archivos generados, revisar rutas y separar los cambios en commits claros. La nueva estructura y sus ejemplos deben funcionar. | — | Completada: fuentes y documentación publicadas en pedlop01/blues-engine, rama main verificada; historial y autoría de cyxx/blues conservados. | 100 % |
| 02 | Terminar de validar el audio | Confirmar la ausencia de petardeo DOS y comprobar música y efectos en ambos modos, en Windows. | — | Completada: pruebas técnicas superadas y audio DOS confirmado por el usuario en Windows. | 100 % |
| 03 | Suavizar personajes y objetos | Interpolar el desplazamiento a 60/120 Hz conservando física y colisiones a 30 Hz. Evitar saltos visuales en puertas, cambios de modo y zoom. | 01 | Completada: pruebas técnicas superadas a 60/120 Hz, física a 30 Hz y fluidez confirmada por el usuario al jugar. | 100 % |
| 04 | Mejorar la cámara | Añadir anticipación hacia donde se camina y seguimiento suave, respetando bordes, habitaciones y cambios de dirección. Conservar cámara clásica y suavizada seleccionables; integrar el selector en el menú mediante la tarea 08. | 01 | Completada: selección al arrancar, pruebas técnicas superadas y seguimiento validado al jugar por el usuario. Selector en menú pendiente en 08. | 100 % |
| 05 | Mantener legible el HUD | Ofrecer HUD clásico que escala con el mapa y HUD fijo legible, conservando los estilos DOS/Amiga. El HUD fijo debe reservar bandas propias y mostrar el encuadre completo sin ocultar ni deformar el mapa. | 01 | Completada: HUD clásico/fijo, encuadre corregido y ambos estilos conservados. Pruebas técnicas superadas y resultado validado al jugar por el usuario. | 100 % |
| 06 | Crear el menú dentro del juego | Una tecla abre un cuadro centrado y pausa la acción. Navegación con teclado y mando; cerrar el menú devuelve el control sin entradas residuales. | 01 | Completada: pruebas técnicas superadas y menú validado al jugar por el usuario. Esc/P/Start, navegación con teclado/mando, pausa de simulación/audio y reanudación sin entradas residuales. | 100 % |
| 07 | Diseñar el aspecto del menú | Fuente retro legible, colores que resalten la selección y distribución correcta en ventana, pantalla completa y distintos encuadres. | 06 | Completada: aspecto validado por el usuario y pruebas de tipografía, selección y distribución superadas en ventana y pantalla completa. | 100 % |
| 08 | Integrar opciones generales | DOS/Amiga, cámara clásica o suavizada, HUD clásico o fijo, pantalla completa, frecuencia de presentación, encuadre y configuración de controles accesibles desde el menú. | 06, 07 | Completada: ajustes y navegación simplificada validados por el usuario. Persistencia entre ejecuciones prevista en 12. | 100 % |
| 09 | Añadir controles de volumen | Ajustes independientes para música y efectos, incluido silencio, aplicados sin reiniciar el juego ni producir chasquidos. | 02, 06 | Completada: pruebas técnicas superadas y niveles/silencio validados al escuchar por el usuario. Persistencia entre ejecuciones prevista en 12. | 100 % |
| 10 | Añadir ayudas de juego | Activar/desactivar invulnerabilidad, aumentar o reducir vidas y elegir vidas infinitas desde el menú. Mostrar con claridad qué ayudas están activas. | 06 | Completada: ayudas, escaleras y recuperación tras el tubo verificadas técnicamente y validadas por el usuario. | 100 % |
| 11 | Añadir selector de nivel | Elegir cualquiera de los seis niveles e iniciarlo con un estado coherente, conservando preferencias y modo gráfico. | 06 | Completada: selector de seis niveles validado por el usuario. Añadida vuelta a la selección original de personaje con reinicio del nivel actual y preferencias conservadas; ampliación pendiente de validar al jugar. | 100 % |
| 12 | Recordar preferencias | Guardar opciones y controles entre ejecuciones; permitir restaurar valores predeterminados y tolerar configuraciones inválidas. | 08, 09, 10 | Completada: persistencia y restauración de opciones y controles verificadas técnicamente y validadas por el usuario. | 100 % |
| 13 | Implementar guardado completo | Guardar y restaurar posición, vidas, inventario, enemigos, objetos, cambios del mapa, habitación, versión gráfica y encuadre. Formato versionado y validación de archivos. | 01 | Completada: guardado completo y restauración validados por el usuario. Formato portable, CRC y compatibilidad de recursos; base utilizada por los slots de 14. | 100 % |
| 14 | Crear ocho slots de guardado | Ocho posiciones con miniatura del estado guardado, nivel, fecha y vidas. Guardar, cargar y sobrescribir; distinguir slots vacíos o incompatibles. | 06, 07, 13 | Integrada y probada: ocho slots con miniatura real, nivel/modo, fecha y vidas; confirmaciones, estados diferenciados y guardados antiguos conservados. Pendiente de validación al jugar. | 75 % |
| 15 | Implementar el rebobinado | Mantener un historial acotado de unos segundos y mostrar la acción marcha atrás mientras se restauran estados completos. Al soltar, continuar desde ese punto sin duplicar objetos ni conservar un futuro inválido. | 13 | Pendiente | 0 % |
| 16 | Crear el efecto VHS | Durante el rebobinado, añadir bandas de ruido, distorsión y una indicación visual, manteniendo reconocible la acción que retrocede. El efecto desaparece al volver al juego normal. | 15 | Pendiente | 0 % |
| 17 | Revisar fidelidad y regresiones | Recorrer los seis niveles y comprobar saltos, escaleras, enemigos, puertas, DOS/Amiga, ayudas, guardados y rebobinado. Mantener pruebas reproducibles para los errores encontrados. | Validación continua; cierre tras 03–16 | Pendiente de la campaña ampliada | 0 % |
| 18 | Preparar la distribución Windows | Paquete con ejecutable y dependencias, lanzador para seleccionar ambas carpetas de datos e instrucciones breves. Verificarlo desde una carpeta nueva. | 01, 12, 14, 16, 17 | Pendiente | 0 % |

## Cómo actualizar los porcentajes

| Progreso | Significado |
|---:|---|
| 0 % | Pendiente; todavía no se ha implementado la ampliación. |
| 25 % | Estructura y diseño preparados; trabajo iniciado. |
| 50 % | Primera versión funcional. |
| 75 % | Integrada y con las pruebas técnicas pertinentes superadas. Falta la validación final. |
| 100 % | Cumple el criterio de finalización y está verificada para su uso. |

No marcar una tarea como terminada solo porque compile. En cambios visuales,
de sonido o de jugabilidad, incluir una comprobación al jugar cuando corresponda.
Si aparece una regresión, actualizar el estado y dejar constancia del bloqueo.

No se calcula una media global de porcentajes: las tareas tienen tamaños muy
distintos. Para el resumen se utiliza el número de tareas completadas.

## Orden de trabajo propuesto

1. Consolidar el proyecto y cerrar la validación de audio: **01–02**.
2. Suavizar personajes y objetos, siguiente tarea elegida por el usuario: **03**.
3. Crear el menú e integrar sus opciones: **06–12**.
4. Implementar el estado guardable y los ocho slots con miniaturas: **13–14**.
5. Reutilizar esa base para rebobinar y añadir el efecto VHS: **15–16**.
6. Pulir cámara y HUD: **04–05**.
7. Cerrar la campaña de pruebas y preparar la distribución: **17–18**.

La tarea 17 acompaña cada etapa; no se aplaza toda la validación hasta el final.

## Decisiones pendientes

- Tecla para abrir el menú y asignación de mando. Mantener los controles actuales
  de cambio de versión y encuadre salvo decisión expresa.
- Duración del historial de rebobinado y su control: mantener pulsado o activar
  un modo. Propuesta inicial para evaluar: **5–10 segundos**.
- Tratamiento del audio durante el rebobinado: silencio o efecto de cinta,
  evitando cortes al volver al juego normal.
- Estilo y fuente definitivos del menú, con una licencia adecuada para distribuirlos.
- Política al cambiar de nivel o cargar una partida: reiniciar el historial de
  rebobinado es la propuesta inicial para evitar mezclar partidas y niveles.

## Ubicación del proyecto

```text
/home/plopez/proj/titus/
├── blues-engine/       # Repositorio y este plan
├── orig-games/
│   ├── dos/
│   └── amiga/
└── tools/
```

Los ejemplos de [README.md](README.md) se ejecutan desde `blues-engine`.
El ejecutable Windows se genera en `build/windows-x64/blues.exe`.
Las rutas anteriores bajo `titus/blues` ya no son válidas.

## Registro de seguimiento

| Fecha | Tarea | Actualización | Validación pendiente |
|---|---|---|---|
| 2026-11-22 | 01 | Localizada la nueva estructura, actualizados ejemplos del README y reparada la referencia de la copia auxiliar de Git. Compilación y pruebas desde las nuevas rutas verificadas. | Limpiar archivos generados y organizar commits. |
| 2026-11-22 | 02 | Salida de audio a 44,1 kHz; comprobadas continuidad entre bloques, ausencia de saturación en las pruebas y tiempo de generación en Windows. | Confirmar al escuchar si desapareció el petardeo. |
| 2026-11-22 | Todas | Creado este documento a partir del plan acordado en el chat. | Actualizar filas y registro conforme se implemente cada tarea. |
| 2026-11-22 | 01 | Preparada la publicación en pedlop01/blues-engine. Historial original conservado y remoto upstream configurado; añadidos .gitignore y CREDITS.md, limpieza de compilación y README con mejoras actuales e ideas pendientes. Compilaciones Linux/Windows desde cero y pruebas de audio, modos, puertas, enemigos y zoom superadas. | Finalizar commits, autenticar la escritura y verificar la publicación remota. |
| 2026-11-22 | 01 | Publicada y verificada la rama main en [pedlop01/blues-engine](https://github.com/pedlop01/blues-engine). Conservados los 49 commits originales; separados los cambios de implementación y documentación. Descripción del repositorio y README enlazan a cyxx/blues y reconocen a Gregory Montoir. | Ninguna para esta tarea. El audio continúa pendiente de confirmación en la tarea 02. |
| 2026-11-22 | 02 | El usuario confirma que el audio DOS funciona bien. Tarea cerrada al 100 %. | Ninguna. |
| 2026-11-22 | 03 | Inicio de la interpolación visual de personajes y objetos, priorizada por el usuario antes del menú. | Implementación, pruebas de continuidad y validación al jugar a 60/120 Hz. |
| 2026-11-22 | 03 | Interpolación por objeto y parte del sprite, con anclaje al centro de los pies para los cambios de animación. Pruebas deterministas a 60/120 Hz y regresiones de puertas, zoom, enemigos y cambio DOS/Amiga superadas. Compilaciones Linux y Windows actualizadas. | Validar al jugar la fluidez en ambos modos antes de cerrar al 100 %. |
| 2026-11-22 | 03 | El usuario confirma que el movimiento es mucho más suave. Validación visual aceptada y tarea cerrada al 100 %. | Ninguna para esta tarea. |
| 2026-11-22 | 04, 08 | A petición del usuario se conserva la cámara clásica y se añade una cámara suavizada con anticipación horizontal y tolerancia vertical. Selección provisional mediante --camera=classic/smooth; selector en el menú comprometido en 08. | Validar el seguimiento al jugar; integrar el selector al implementar el menú. |
| 2026-11-22 | 04 | Pruebas de anticipación, inversión de dirección, parada, tolerancia vertical, caídas rápidas, límites, cortes y cámara clásica superadas en seis niveles. Regresiones de puertas y zoom comprobadas en DOS y Amiga; compilaciones Linux/Windows y comprobación con AddressSanitizer superadas. | Confirmación visual del usuario para cerrar el seguimiento de cámara. |
| 2026-11-22 | 04, 05 | El usuario valida la cámara y se cierra 04 al 100 %. Comienza 05 con el requisito explícito de conservar los HUD distintos de DOS y Amiga. | Implementar escala independiente y comprobar ambos estilos. |
| 2026-11-22 | 05 | HUD presentado en bandas a escala nativa independiente del zoom, con los gráficos y paletas originales de DOS y Amiga. Comparación de píxeles superada en ambos modos, uno/dos jugadores, tres tamaños de ventana y todos los encuadres, incluidos tamaños intermedios. Compilaciones Linux/Windows y AddressSanitizer superados. | Confirmar legibilidad y conservación de ambos estilos al jugar. |
| 2026-11-22 | 05, 08 | El usuario prefiere el HUD anterior y detecta que el HUD fijo oculta parte del encuadre. Se recupera el clásico por defecto y se conservan ambas opciones (--hud=classic/fixed y F7). El fijo reserva bandas y ajusta el área del mundo sin recortar ni deformar. Selector previsto en 08. | Validar de nuevo ambos modos al jugar. |
| 2026-11-22 | 05 | Pruebas superadas en los seis niveles con HUD clásico y fijo: zoom, puertas, cambios DOS/Amiga y alternancia F7 durante el zoom. Comprobadas las cuatro esquinas visibles del mundo y los píxeles originales del HUD fijo; AddressSanitizer y compilaciones Linux/Windows superados. | Validación al jugar del encuadre corregido y de la elección entre HUD clásico/fijo. |
| 2026-11-22 | 05, 06 | El usuario valida ambos HUD y el zoom corregido: se cierra 05 al 100 %. Comienza el menú de pausa dentro del juego con teclado y mando. | Comprobar congelación de simulación/audio, navegación y ausencia de entradas residuales. |
| 2026-11-22 | 06 | Menú centrado con Continuar, Controles y Salir con confirmación. Esc/P/Start, navegación por teclado, cruceta y stick; audio y escena congelados, sin entradas residuales al volver. Pruebas unitarias e integración en seis niveles, DOS/Amiga, ambos HUD y 30/60/120 Hz superadas. Compilaciones Linux/Windows y AddressSanitizer superados. | Validar apertura, navegación y reanudación al jugar. El diseño ampliado y las opciones se mantienen en 07–08. |
| 2026-11-22 | 06, 07 | El usuario valida el menú y se cierra 06 al 100 %. Comienza el pulido visual, conservando la estructura aceptada. | Revisar tipografía, selección, controles y distribución en distintos tamaños. |
| 2026-11-22 | 07 | Pulido visual sin cambiar la navegación aceptada: fuente con sombra, selección con franja dorada, confirmación de salida en otro color y controles en columnas. Pruebas de las tres pantallas y ambas selecciones de salida, cinco tamaños de ventana y pantalla completa, con escalado a píxeles enteros. Integración DOS/Amiga con ambos HUD y compilaciones Linux/Windows verificadas; AddressSanitizer superado. | Confirmar el aspecto al jugar antes de cerrar al 100 %. |
| 2026-11-22 | 07, 08 | El usuario valida el aspecto del menú y se cierra 07 al 100 %. Comienzan los ajustes generales y la reasignación de controles. La persistencia entre ejecuciones sigue en 12. | Pruebas de cambios de presentación, recursos y captura de controles. |
| 2026-11-22 | 08 | Opciones generales integradas en el menú. Configuración de seis acciones de teclado y botones de acción/salto del mando, intercambio de duplicados y restauración de controles. Cambios preparados en pausa y aplicados al continuar; pantalla completa con vista inmediata. Pruebas de cambios reales en los seis niveles, ambas versiones y HUD, captura de controles, compilaciones Linux/Windows y AddressSanitizer superados. | Validar los ajustes al jugar. Guardar preferencias entre ejecuciones continúa en 12. |
| 2026-11-22 | 08 | A petición del usuario se elimina la entrada duplicada de Controles dentro de Opciones. La entrada exterior reúne teclado, mando, consulta de asignaciones y restauración; volver devuelve al menú principal. | Validar la navegación simplificada. |
| 2026-11-22 | 08, 09 | El usuario da por completada 08. Comienzan los volúmenes independientes de música y efectos para DOS y Amiga, con silencio y rampas para evitar saltos de amplitud. | Pruebas de separación, continuidad, volumen por defecto y menú. |
| 2026-11-22 | 09 | Opciones → Sonido con niveles independientes, barras y silencio/restauración. Rampas de 20 ms y reproducción que continúa al silenciar. Pruebas de mezcla, PCM original DOS al 100 %, secuenciador original, continuidad por bloques y audio real DOS/Amiga en seis niveles superadas. Compilaciones Linux/Windows y AddressSanitizer verificados. | Validar al escuchar música, efectos y silencio en ambas versiones. Persistencia entre ejecuciones pendiente en 12. |
| 2026-11-22 | 09, 10 | El usuario valida el audio y cierra 09 al 100 %. Comienzan invulnerabilidad, ajuste de vidas y vidas infinitas desde el menú, separando energía y consumo de vidas. | Pruebas de daño, muerte/reintento, dos jugadores y presentación. |
| 2026-11-22 | 10 | Opciones → Ayudas implementado, con estado visible en el margen del HUD y aplicación al continuar. Corregida la antigua mezcla entre vidas infinitas, energía y contador fijo de tres vidas. Verificados golpes, peligros, reintentos, última vida, dos jugadores y límite de 99; menú probado en ambos modos y seis niveles. Compilaciones Linux/Windows y AddressSanitizer superados. | Validar las ayudas al jugar antes de cerrar al 100 %. |
| 2026-11-22 | 10, 17 | Durante la validación del nivel 2 se detectan deriva lateral en escaleras, bajada bloqueada en plataformas y estado de golpe persistente tras el tubo. Corregidos controles de subida con salto separado, inercia y parada vertical, respeto de la dirección del jugador y descenso mantenido. Corregido el recentrado que se detenía a cinco píxeles y no liberaba el estado de golpe. | Validar de nuevo las escaleras y el tubo del almacén al jugar. |
| 2026-11-22 | 17 | Pruebas de recorrido añadidas sobre recursos reales: escaleras de los seis niveles y tubo del almacén con ambas cámaras, encuadres, ayudas activadas/desactivadas y HUD clásico/fijo en DOS/Amiga. Regresiones de cámara, puertas, ayudas, modos y zoom superadas; compilaciones Linux/Windows y AddressSanitizer verificados. | Continuar validación de jugabilidad tras la prueba del usuario. |
| 2026-11-22 | 10, 11 | El usuario valida las correcciones y cierra 10 al 100 %. Selector de seis niveles añadido al menú principal, con confirmación y niveles sin datos deshabilitados. Reinicia mapa, enemigos, inventario, daño y puntos de retorno; conserva personajes, vidas, ayudas, versión y preferencias. | Validar selección y reinicio al jugar para cerrar 11. |
| 2026-11-22 | 11 | Pruebas superadas de seis destinos y reinicio del actual en DOS/Amiga, dos jugadores y cambio de versión preparado en el menú. Verificados estado limpio, conservación de ajustes, cancelación, archivos ausentes y distribución visual. Regresiones de pausa, opciones, ayudas, escaleras/tubo y HUD superadas; AddressSanitizer y compilaciones Linux/Windows verificados. | Confirmar el selector al jugar. |
| 2026-11-22 | 11, 17 | El usuario valida el selector de nivel. Corregida la entrada por la mitad derecha de la escalera, que redondeaba a la columna contigua y podía acabar en la cinta del almacén; se elimina el impulso residual de cinta al escalar. Añadida opción Elegir personaje con confirmación y reinicio limpio del nivel actual conservando ajustes y vidas. | Validar escaleras y selección de personaje al jugar. |
| 2026-11-22 | 11, 17 | Pruebas ampliadas de escaleras superadas en los seis niveles DOS/Amiga: 16 desplazamientos de entrada y tres impulsos de cinta por escalera; 144 combinaciones en el almacén, además de subida/bajada/parada y tubo. Selección original de Jake, Elwood y dos jugadores verificada en ciclos repetidos, con modo DOS/Amiga preparado en el menú. Navegación, cancelación, distribución visual, regresiones, AddressSanitizer y compilaciones Linux/Windows superados. | Confirmación al jugar de ambos cambios. |
| 2026-11-22 | 17 | Nueva captura del usuario confirma deriva al pulsar Arriba. Eliminado el alineado horizontal del jugador al escalar: su posición representa el centro del sprite, y llevarla al borde de la casilla provocaba el salto a la izquierda. Pruebas anteriores aceptaban erróneamente ese cambio. Nuevas pruebas con teclado real exigen conservar X desde el primer fotograma en escaleras cortas y largas; fallo reproducido contra la versión anterior y pruebas superadas en los seis niveles DOS/Amiga. | Validar la subida de la captura con el ejecutable actualizado. |
| 2026-11-22 | 12, 17 | El usuario confirma solucionada la deriva en escaleras. Implementadas preferencias persistentes por usuario, carga antes de argumentos, guardado mediante archivo temporal, formato versionado y recuperación de valores inválidos. Añadido Restaurar ajustes con confirmación. Las vidas ganadas/perdidas no cambian la cantidad inicial elegida. | Validar conservación y restauración al cerrar y abrir en Windows. |
| 2026-11-22 | 12 | Pruebas de procesos independientes superadas: restauración de opciones, controles, versión, ayudas, vidas elegidas y reinicio de valores; prioridad de argumentos y rutas Unicode. Verificados valores inválidos, duplicados, formato futuro, fallo de escritura sin perder el archivo anterior, silencio/restauración y salud conservada al restablecer. Distribución visual revisada; regresiones de audio, modos, HUD, ayudas y escaleras, AddressSanitizer y compilaciones Linux/Windows superados. | Validar en Windows al cerrar y volver a abrir. |
| 2026-11-22 | 12, 13 | El usuario valida 12, cerrada al 100 %. Implementado guardado completo: objetos, enemigos, inventario, mapas y triggers, puertas/habitaciones, modo nativo, cámara y transición de zoom. Referencias estáticas por identificador, formato portable con versión/CRC y firma de recursos; escritura temporal y confirmaciones. | Validar guardar/cargar al jugar en Windows. Ocho slots con miniaturas reservados para 14. |
| 2026-11-22 | 13, 17 | Pruebas de guardado superadas en seis niveles DOS/Amiga, habitaciones, objetos transportados, dos jugadores y jugador eliminado. Verificados igualdad del estado y de píxeles tras restaurar, repetición determinista, cargas desde otros niveles/modos/procesos, encuadre base distinto, zoom en curso y cámara clásica/suavizada. Rechazados archivos truncados/corruptos/futuros y recursos ausentes/incompatibles; guardado anterior conservado ante fallo de escritura. Menú y regresiones de ayudas, escaleras, HUD, modos, audio y preferencias superados; AddressSanitizer y compilaciones Linux/Windows verificados. | Confirmar guardar/avanzar/cargar y cerrar/abrir/cargar en Windows. La música reinicia la pista guardada. |
| 2026-11-22 | 13, 14 | El usuario valida 13, cerrada al 100 %. Añadido navegador de ocho slots con miniatura de la escena sin menú, nivel, versión, fecha local y vidas. Sobrescritura/carga con confirmación; vacíos, dañados, incompatibles y errores de lectura diferenciados. Imagen y estado en un único archivo con CRC y escritura atómica; guardados v1 conservados como slots sin miniatura. | Validar el navegador y varios slots en Windows. |
| 2026-11-22 | 14 | Ocho archivos independientes verificados en DOS/Amiga, ambos HUD, dos jugadores, ventana de tamaño irregular y procesos nuevos. Las miniaturas coinciden exactamente con la escena restaurada y excluyen el menú. Comprobados guardados v1, metadatos, estados incompatibles/corruptos y protección del archivo anterior ante fallo de captura. Menú, preferencias, regresiones de guardado completo, AddressSanitizer y compilaciones Linux/Windows superados. | Validar selección, sobrescritura y carga de varios slots en Windows. |
