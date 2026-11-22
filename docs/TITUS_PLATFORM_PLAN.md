# Plataforma de juegos Titus: plan inicial

Fecha: **22 de noviembre de 2026**.

Estado: **propuesta documentada; implementación sin iniciar**.
Nombre de trabajo: **Plataforma Titus**. Nombre definitivo pendiente.

## Objetivo y condición de inicio

Crear una aplicación al estilo de ScummVM para reunir juegos de Titus bajo una
biblioteca y un menú principal comunes. Cada juego conservará su motor, lógica,
recursos originales y presentación propia. La aplicación permitirá registrar
carpetas de juegos, detectar las ediciones disponibles y elegir cuál ejecutar.

**Primero se terminará blues-engine:** cerrar y validar las tareas 17 y 18 de
[PLAN.md](../PLAN.md), incluida la campaña de jugabilidad y la distribución
Windows probada desde una carpeta nueva. Conservar una versión estable y
reproducible antes de iniciar esta ampliación.

Este documento prepara el trabajo futuro. Su creación no inicia integraciones,
migraciones, cambios de nombre, reorganización del repositorio ni nuevas tareas
de programación. Las tareas actuales de blues-engine conservan su numeración,
estado y prioridad.

## Catálogo candidato y versiones

La siguiente tabla distingue el soporte de los motores del soporte validado en
nuestra aplicación. La existencia de una edición comercial Amiga no demuestra
que un motor pueda ejecutarla.

| Juego | Base candidata | DOS | Amiga | Situación para esta plataforma |
|---|---|---|---|---|
| The Blues Brothers | Nuestra extensión de cyxx/blues | Implementado y probado | Implementado; continúa la validación visual | Primer juego para integrar; conservar sus mejoras y datos actuales. |
| Blues Brothers: Jukebox Adventure | Módulo `ja/` de cyxx/blues | Soportado por el proyecto base | Sin soporte verificado | Fuentes conservadas; pendiente probar ejecución y preparar Windows. |
| Prehistorik 2 | Módulo `p2/` de cyxx/blues | Soportado por el proyecto base | Sin soporte verificado | Fuentes conservadas; pendiente probar ejecución y preparar Windows. |
| Titus the Fox | OpenTitus o un fork evaluado | Declarado por OpenTitus | Sin soporte verificado | Candidato; todavía no compilado ni validado por nosotros. |
| Prehistorik 1 | Olduvai | Declarado por Olduvai | Sin soporte verificado | Candidato; todavía no compilado ni validado por nosotros. |
| Moktar | OpenTitus | Declarado por OpenTitus | Sin soporte verificado | Posible ampliación posterior; fuera del mínimo inicial. |

Fuentes consultadas: [cyxx/blues](https://github.com/cyxx/blues),
[OpenTitus](https://github.com/stople/OpenTitus) y
[Olduvai](https://github.com/ksokolowski/olduvai). La información externa procede
de sus repositorios/documentación; las afirmaciones de funcionamiento de esos
proyectos deberán comprobarse con nuestros datos y compilaciones. La auditoría
registrará los commits concretos y los forks elegidos antes de integrar código.

La primera versión priorizará Windows x64 y mantendrá Linux/WSL como entorno de
desarrollo y pruebas. Amiga se ofrecerá por juego cuando exista un adaptador
comprobado; los motores candidatos de Titus the Fox y Prehistorik 1 aportan una
base DOS, y cualquier soporte Amiga requerirá una investigación independiente.

## Experiencia del usuario

1. **Biblioteca:** mostrar los juegos registrados y sus instalaciones utilizables,
   con nombre, edición, versión y estado. Una imagen será opcional; habrá una
   presentación válida sin carátula.
2. **Añadir juego:** el usuario selecciona una carpeta o una raíz que quiere
   examinar. El detector identifica archivos, formato y firmas conocidas y muestra
   lo que ha reconocido antes de registrarlo. Evitar búsquedas generales por el
   disco y la confusión entre dos juegos con archivos de nombres parecidos.
3. **Ficha del juego:** elegir instalación y edición, por ejemplo DOS o Amiga.
   Mostrar únicamente opciones ejecutables como seleccionables. Explicar qué
   archivo falta o qué versión no se reconoce cuando una instalación está incompleta.
4. **Jugar:** iniciar el motor con esa instalación y sus ajustes. Ofrecer continuar
   una partida solo cuando el motor y el guardado sean compatibles.
5. **Pausa:** conservar la identidad del juego, su HUD y sus opciones específicas.
   Una ruta clara permitirá volver a la biblioteca, con confirmación cuando se
   vaya a descartar progreso sin guardar. Controles tendrá una sola entrada.
6. **Volver a la biblioteca:** liberar o cerrar el motor, restaurar audio, vídeo,
   pantalla completa y mando, y devolver la selección al juego anterior.

Flujo previsto:

```text
Biblioteca → Juego → Instalación/edición DOS o Amiga → Jugar
     ↑                                                ↓
     └──────────────── Pausa → Volver a biblioteca ───┘
```

El selector de edición del lanzador elige cómo se inicia el juego. El intercambio
en caliente DOS/Amiga durante la partida es una capacidad específica: Blues
Brothers ya la ofrece con F6. En los demás juegos se añadirá únicamente si se
demuestra que sus recursos y estados permiten conservar la partida. La interfaz
no prometerá esa función por tener ambas carpetas registradas.

## Arquitectura propuesta

### Biblioteca y servicios comunes

- Catálogo de juegos y motores, detección de instalaciones y selección de edición.
- Menú global navegable con teclado y mando; gestión de foco, ventana y errores.
- Ajustes globales con valores particulares por juego/edición: controles, vídeo,
  volumen y accesibilidad cuando el motor admita esas funciones.
- Almacenamiento de preferencias y catálogo por usuario; rutas Unicode, relativas
  cuando corresponda, y recuperación de carpetas que se hayan movido.
- Selección de motor mediante un registro de adaptadores. La física y los recursos
  de cada juego permanecerán dentro de su motor.

Cada instalación tendrá identidad propia y una referencia a `game_id`,
`engine_id`, `edition_id`, idioma/variante detectada y firma de recursos.
La ruta será su ubicación, no su única identidad: mover una carpeta no debería
mezclar instalaciones o perder sus partidas. Se podrán registrar DOS y Amiga
para un mismo juego, y varias instalaciones de una misma edición.

### Contrato de los motores

Definir un contrato pequeño antes de trasladar funciones comunes:

- Identificar recursos y anunciar capacidades disponibles.
- Iniciar una sesión con instalación y ajustes explícitos.
- Ejecutar, pausar, reanudar y cerrar limpiamente una sesión.
- Solicitar regreso a la biblioteca y comunicar errores recuperables.
- Consultar/aplicar ajustes compatibles con ese motor.
- Capturar imagen, guardar/cargar y capturar/restaurar estado solamente cuando
  exista implementación para ese juego.

Las capacidades incluirán guardado, miniaturas, rebobinado, encuadre dinámico,
cámara suavizada, ayudas, volumen separado y cambio de edición en caliente.
La interfaz se construirá con las capacidades reales; no se simulará una opción
inexistente ni se mostrarán controles que el motor ignore.

### Estrategia de integración

**Propuesta pendiente de auditoría:** construir primero la biblioteca con motores
ejecutables separados y un protocolo de arranque/salida sencillo. Esto permite
comprobar cada juego en su motor original y limita los conflictos entre versiones
de SDL, C/C++, bucles de eventos, audio y variables globales.

Esa primera etapa será un lanzador unificado. El objetivo posterior es integrar
adaptadores en el mismo proceso donde resulte conveniente, con servicios comunes
y retorno directo al menú. No se dará por hecha esa integración hasta comprobar
la propiedad e inicialización de SDL, las dependencias, la compatibilidad de las
licencias y el cierre de cada motor. Los ejecutables separados podrán mantenerse
para los motores cuya integración completa no aporte una ventaja suficiente.

Conservar los motores de origen identificables, sus historiales/revisiones y las
modificaciones propias separadas. Decidir repositorio, estructura y nombre después
de la auditoría, manteniendo blues-engine estable como referencia de regresiones.

## Preferencias, partidas y recursos

- Las carpetas de datos originales se leerán sin modificarlas. Se admitirá una
  instalación de solo lectura; la aplicación almacenará su información por usuario.
- Cada motor conservará inicialmente su formato de guardado. Compartir una
  biblioteca de partidas no exige compartir el formato interno de sus estados.
- Identificar partidas por juego, motor, edición, versión de esquema y firma de
  recursos. Cada adaptador definirá explícitamente qué variantes son compatibles.
- Conservar los ocho slots, miniaturas y preferencias de Blues Brothers y su
  compatibilidad DOS/Amiga actual. Cualquier migración tendrá respaldo, lectura
  de los archivos anteriores y una estrategia de reversión.
- El rebobinado necesita un estado completo y restaurable en cada motor. Será
  una ampliación posterior por juego, validada frente a duplicación de objetos,
  cambios de nivel, enemigos, RNG y audio; no un requisito para registrar el juego.
- Los perfiles podrán partir de valores globales y conservar cambios por juego.
  Cambiar de título no deberá arrastrar ayudas, vidas o ajustes exclusivos de otro.
- El paquete incluirá los motores y componentes permitidos para su distribución,
  con créditos y avisos correspondientes. Los datos originales se aportarán aparte.

## Hitos y seguimiento inicial

Los identificadores `TH-*` pertenecen a esta propuesta. El porcentaje mide
implementación comprobada, no el tiempo invertido en escribir este plan. Todas
las tareas nuevas permanecen **sin iniciar** hasta cerrar blues-engine.

| ID | Tarea | Resultado y criterio de cierre | Depende de | Estado | Progreso |
|---|---|---|---|---|---:|
| TH-00 | Cerrar blues-engine | Tareas 17 y 18 cerradas, ejecutable/distribución Windows validados y versión estable identificada. | PLAN.md | Bloque previo; trabajo actual | — |
| TH-01 | Auditar motores y ediciones | Registrar commits, forks, licencias/créditos, dependencias, formatos de datos y funciones declaradas frente a verificadas. Elegir bases y orden definitivo. | TH-00 | Sin iniciar | 0 % |
| TH-02 | Establecer pruebas originales | Compilar y jugar Jukebox Adventure, Prehistorik 2, OpenTitus y Olduvai por separado con datos originales. Registrar fallos y recorrido de referencia. | TH-01 | Sin iniciar | 0 % |
| TH-03 | Definir contrato y almacenamiento | Acordar identidad de instalaciones, capacidades, inicio/salida y aislamiento de preferencias/partidas. Decidir procesos separados e integración posterior. | TH-01, TH-02 | Sin iniciar | 0 % |
| TH-04 | Implementar catálogo y detección | Añadir, localizar de nuevo y quitar instalaciones sin borrar datos; distinguir ediciones y rechazar carpetas incompletas o ambiguas. | TH-03 | Sin iniciar | 0 % |
| TH-05 | Crear biblioteca global | Menú con teclado/mando, ficha de juego, selección de instalación/edición y estados claros; probar distintas ventanas y pantalla completa. | TH-04 | Sin iniciar | 0 % |
| TH-06 | Integrar Blues Brothers | Arrancar DOS/Amiga desde biblioteca, conservar mejoras, saves y preferencias, y volver al menú sin entradas residuales ni audio activo. | TH-05 | Sin iniciar | 0 % |
| TH-07 | Incorporar juegos del motor base | Jukebox Adventure y Prehistorik 2 ejecutables desde Windows/biblioteca; comportamiento y datos separados de Blues Brothers. | TH-02, TH-06 | Sin iniciar | 0 % |
| TH-08 | Incorporar Titus the Fox | Adaptador OpenTitus con edición DOS detectada, arranque, controles, cierre y retorno comprobados. Moktar queda como ampliación opcional. | TH-02, TH-06 | Sin iniciar | 0 % |
| TH-09 | Incorporar Prehistorik 1 | Adaptador Olduvai con datos DOS detectados, arranque, controles, cierre y retorno comprobados. | TH-02, TH-06 | Sin iniciar | 0 % |
| TH-10 | Unificar opciones y catálogo de partidas | Preferencias globales/particulares, capacidades por motor y partidas correctamente asociadas; migración de Blues Brothers comprobada. | TH-07, TH-08, TH-09 | Sin iniciar | 0 % |
| TH-11 | Evaluar integración en proceso | Piloto con un motor; comprobar ciclo SDL/audio/vídeo y comparar con la ejecución separada. Integrar solo los adaptadores aprobados por la auditoría técnica. | TH-03, TH-10 | Sin iniciar | 0 % |
| TH-12 | Validar y distribuir | Matriz por juego/edición, regresiones de Blues Brothers y paquete Windows probado desde cero; manual y créditos de todos los motores. | TH-10; TH-11 si entra en esa entrega | Sin iniciar | 0 % |

### Entregas propuestas

1. **Primera entrega:** biblioteca y Blues Brothers DOS/Amiga; importar sus
   instalaciones actuales y regresar al menú conservando la versión estable.
2. **Segunda entrega:** Jukebox Adventure y Prehistorik 2, sujetos al resultado de
   sus pruebas originales. No exigir todas las mejoras nuevas para incorporarlos.
3. **Tercera entrega:** Titus the Fox y Prehistorik 1. Su orden podrá cambiar si la
   auditoría encuentra una base más sencilla o un bloqueo en alguno de ellos.
4. **Evolución posterior:** integración más profunda de servicios, mejoras por
   motor, otras ediciones y Moktar. Cualquier port Amiga adicional tendrá su
   propio análisis y plan, además de recursos originales para verificarlo.

No se asignan fechas ni estimaciones de esfuerzo hasta completar TH-01 y TH-02.

## Validación necesaria

| Área | Comprobación |
|---|---|
| Detección | Carpetas DOS/Amiga separadas, archivos incompletos, variante desconocida, instalaciones duplicadas y rutas Unicode/con espacios. |
| Biblioteca | Teclado/mando, foco, pantalla completa, selección recordada, cancelación y opciones inaccesibles correctamente explicadas. |
| Ciclo de sesión | Abrir y cerrar cada motor repetidamente, cambiar de juego y recuperar ventana, cursor, audio y mandos; recuperar el lanzador si falla un proceso. |
| Aislamiento | Cambiar entre juegos/versiones sin compartir recursos, estados, trucos, volumen específico o entradas que sigan pulsadas. |
| Partidas | Guardados anteriores de Blues Brothers, ocho slots/miniaturas, recursos incompatibles, fallos de escritura y rechazo de estados de otro juego. |
| Fidelidad | Recorridos reproducibles por niveles, colisiones, escaleras, plataformas, gráficos, audio, transiciones y final, con referencias del original cuando haya dudas. |
| Rendimiento | Fluidez y latencia, memoria estable tras varias sesiones y ausencia de recursos o hilos de audio pendientes al volver al menú. |
| Distribución | Windows limpio con dependencias completas, datos externos, preferencias por usuario, versiones identificadas y documentación de procedencia. |

## Decisiones abiertas para el inicio futuro

- Nombre e identidad visual de la aplicación y ubicación/repositorio definitivos.
- Motores/forks y commits seleccionados tras probarlos; no asumir equivalencia
  entre dos forks por compartir nombre.
- Diseño visual final de la biblioteca y origen de sus imágenes opcionales.
- Qué adaptadores merece la pena integrar en proceso y cuáles seguirán separados.
- Qué mejoras adicionales recibirá cada juego después de asegurar su fidelidad.

Ninguna de estas decisiones bloquea terminar blues-engine. El siguiente paso
actual sigue siendo su validación y distribución, según [PLAN.md](../PLAN.md).
