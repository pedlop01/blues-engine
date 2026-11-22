# Plan de mejoras y seguimiento

Última actualización: **22 de noviembre de 2026**.

Este documento recoge el plan acordado y su estado real. Los porcentajes se
refieren a cada tarea; no representan una estimación del tiempo restante.

## Estado general

- Tareas completadas: **0 de 18**.
- En preparación: **01**.
- Pendiente de validación al jugar: **02**.
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
El ajuste de audio a 44,1 kHz está implementado y probado automáticamente,
pero falta confirmar si elimina el petardeo que se oía en Windows.

## Tabla de tareas

| ID | Tarea | Alcance y criterio de finalización | Dependencias | Estado | Progreso |
|---|---|---|---|---|---:|
| 01 | Consolidar el proyecto | Ordenar archivos generados, revisar rutas y separar los cambios en commits claros. La nueva estructura y sus ejemplos deben funcionar. | — | Limpieza, exclusión de archivos generados, atribución al origen y pruebas completas listas; publicación en GitHub en curso. | 75 % |
| 02 | Terminar de validar el audio | Confirmar la ausencia de petardeo DOS y comprobar música y efectos en ambos modos, en Windows. | — | Salida a 44,1 kHz y pruebas listas; pendiente de confirmación al escuchar. | 75 % |
| 03 | Suavizar personajes y objetos | Interpolar el desplazamiento a 60/120 Hz conservando física y colisiones a 30 Hz. Evitar saltos visuales en puertas, cambios de modo y zoom. | 01 | Pendiente | 0 % |
| 04 | Mejorar la cámara | Añadir anticipación hacia donde se camina y seguimiento suave, respetando bordes, habitaciones y cambios de dirección. | 01 | Pendiente | 0 % |
| 05 | Mantener legible el HUD | Conservar un tamaño cómodo de indicadores al alejar el encuadre, sin tapar innecesariamente el mapa. | 01 | Pendiente | 0 % |
| 06 | Crear el menú dentro del juego | Una tecla abre un cuadro centrado y pausa la acción. Navegación con teclado y mando; cerrar el menú devuelve el control sin entradas residuales. | 01 | Pendiente | 0 % |
| 07 | Diseñar el aspecto del menú | Fuente retro legible, colores que resalten la selección y distribución correcta en ventana, pantalla completa y distintos encuadres. | 06 | Pendiente | 0 % |
| 08 | Integrar opciones generales | DOS/Amiga, pantalla completa, frecuencia de presentación, encuadre y configuración de controles accesibles desde el menú. | 06, 07 | Pendiente | 0 % |
| 09 | Añadir controles de volumen | Ajustes independientes para música y efectos, incluido silencio, aplicados sin reiniciar el juego ni producir chasquidos. | 02, 06 | Pendiente | 0 % |
| 10 | Añadir ayudas de juego | Activar/desactivar invulnerabilidad, aumentar o reducir vidas y elegir vidas infinitas desde el menú. Mostrar con claridad qué ayudas están activas. | 06 | Pendiente | 0 % |
| 11 | Añadir selector de nivel | Elegir cualquiera de los seis niveles e iniciarlo con un estado coherente, conservando preferencias y modo gráfico. | 06 | Pendiente | 0 % |
| 12 | Recordar preferencias | Guardar opciones y controles entre ejecuciones; permitir restaurar valores predeterminados y tolerar configuraciones inválidas. | 08, 09, 10 | Pendiente | 0 % |
| 13 | Implementar guardado completo | Guardar y restaurar posición, vidas, inventario, enemigos, objetos, cambios del mapa, habitación, versión gráfica y encuadre. Formato versionado y validación de archivos. | 01 | Pendiente | 0 % |
| 14 | Crear ocho slots de guardado | Ocho posiciones con miniatura del estado guardado, nivel, fecha y vidas. Guardar, cargar y sobrescribir; distinguir slots vacíos o incompatibles. | 06, 07, 13 | Pendiente | 0 % |
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
2. Crear el menú e integrar sus opciones: **06–12**.
3. Implementar el estado guardable y los ocho slots con miniaturas: **13–14**.
4. Reutilizar esa base para rebobinar y añadir el efecto VHS: **15–16**.
5. Pulir interpolación, cámara y HUD: **03–05**.
6. Cerrar la campaña de pruebas y preparar la distribución: **17–18**.

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
