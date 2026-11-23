# Titus Engine: plan de expansión

Actualización: **23 de noviembre de 2026**.
Estado: **expansión autorizada; TH-00–TH-03 completadas; siguiente implementación TH-04**.
Nombre de producto acordado: **Titus Engine** (`titus-engine`).

## Prioridades y punto de partida

Crear una aplicación al estilo de ScummVM para reunir los juegos instalados de
Titus bajo una biblioteca global, conservando la lógica y la presentación propias
de cada motor. El usuario autoriza iniciar la expansión mientras prueba Blues
Brothers y comunica errores de la tarea 17.

Orden acordado el 23 de noviembre:

1. Mantener los arreglos actuales publicados y un punto recuperable anterior a la
   expansión: commit `dd07383` y etiqueta `blues-baseline-2026-11-23`.
2. Avanzar con inventario, auditoría de motores y desarrollo por etapas de Titus
   Engine. La tarea 17 sigue abierta y sus errores se corregirán en paralelo.
3. Posponer la tarea 18 hasta después de la primera expansión validada. Su paquete
   final será el de Titus Engine con el catálogo inicial, no un lanzador exclusivo
   de Blues Brothers. Cerrar 17 será también requisito de esa distribución.

Este orden sustituye la condición del borrador anterior que exigía cerrar 17 y 18
antes de empezar. El inventario inicial está en [GAME_INVENTORY.md](GAME_INVENTORY.md).
Resultados y límites de las pruebas: [ENGINE_AUDIT.md](ENGINE_AUDIT.md).
Reglas acordadas de identificación: [GAME_DETECTION_RULES.md](GAME_DETECTION_RULES.md).
Validación de referencia TH-02: [ENGINE_REFERENCE_TESTS.md](ENGINE_REFERENCE_TESTS.md).

Se conserva por ahora el repositorio, remoto y directorio `blues-engine`, junto
con sus ejecutables y rutas de preferencias/guardado, para mantener la continuidad
de las pruebas. El cambio de producto, binario y organización interna se hará en
una tarea explícita; no se moverán originales ni se romperán partidas existentes.
La etiqueta de recuperación no implica que la campaña 17 haya terminado.

## Catálogo candidato y versiones

La siguiente tabla distingue el soporte de los motores del soporte validado en
nuestra aplicación. La existencia de una edición comercial Amiga no demuestra
que un motor pueda ejecutarla.

| Juego | Base candidata | DOS | Amiga | Situación para esta plataforma |
|---|---|---|---|---|
| The Blues Brothers | Nuestra extensión de cyxx/blues | Implementado y probado | Implementado; continúa la validación visual | Primer juego para integrar; conservar sus mejoras y datos actuales. |
| Blues Brothers: Jukebox Adventure | Módulo `ja/` de cyxx/blues | Soportado por el proyecto base | Sin soporte verificado | Ruta corta, controles/audio y cierre comprobados en Linux y Windows x64 sobre ARM64; integración pendiente. |
| Prehistorik 2 | Módulo `p2/` de cyxx/blues | Datos en `prehistorik2-dos/`; soportado por el proyecto base | Datos Amiga identificados como Prehistorik 1, no 2 | Ruta corta y audio comprobados en Linux/Windows; saturación registrada para TH-07. Sin datos Amiga de esta entrega identificados. |
| Titus the Fox | OpenTitus, revisión fijada en auditoría | Datos y audio del COM preparados; recorrido de referencia Linux/Windows comprobado | `fox-amiga/` y `fox.adf` idénticos; motor pendiente | Objetivo DOS inicial; edición Amiga sujeta a viabilidad. |
| Prehistorik 1 | Olduvai | DIET preparado; siete cargas y ruta/audio Linux/Windows comprobados | `Prehistorik.adf` y `prehistorik2-amiga/` coinciden: Prehistorik 1 | Objetivo DOS inicial; edición Amiga sujeta a viabilidad. |
| Moktar | OpenTitus | Declarado por OpenTitus | Sin soporte verificado | Posible ampliación posterior; fuera del mínimo inicial. |

Fuentes consultadas: [cyxx/blues](https://github.com/cyxx/blues),
[OpenTitus](https://github.com/stople/OpenTitus) y
[Olduvai](https://github.com/ksokolowski/olduvai). La información externa procede
de sus repositorios/documentación; las afirmaciones de funcionamiento de esos
proyectos deberán comprobarse con nuestros datos y compilaciones. La auditoría
registra las revisiones elegidas y sus límites antes de integrar código.

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

## Arquitectura acordada en TH-03

Decisión y contrato completos: [TITUS_ARCHITECTURE.md](TITUS_ARCHITECTURE.md).
Esquema y ejemplos verificables: [contracts](../contracts/README.md).
La definición está cerrada; el lanzador y sus adaptadores todavía no están implementados.

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

El contrato v1 define, antes de trasladar funciones comunes:

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

**Decisión TH-03:** construir primero la biblioteca con motores
ejecutables separados y un protocolo v1 de request/eventos/comandos/resultado en archivos privados. Esto permite
comprobar cada juego en su motor original y limita los conflictos entre versiones
de SDL, C/C++, bucles de eventos, audio y variables globales.

Esa primera etapa será un lanzador unificado. El objetivo posterior es integrar
adaptadores en el mismo proceso donde resulte conveniente, con servicios comunes
y retorno directo al menú. No se dará por hecha esa integración hasta comprobar
la propiedad e inicialización de SDL, las dependencias, la compatibilidad de las
licencias y el cierre de cada motor. Los ejecutables separados podrán mantenerse
para los motores cuya integración completa no aporte una ventaja suficiente.

Conservar los motores de origen identificables, sus historiales/revisiones y las
modificaciones propias separadas. El frontend será `titus-engine[.exe]`, C++17/SDL2;
la CLI clásica `blues[.exe]`, el remoto y el directorio `blues-engine` se mantienen
durante la expansión. La estructura nueva se introduce por tareas sin mover ahora
`bb/ja/p2`. Conservar el punto de recuperación de Blues como referencia de regresiones.

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

Los identificadores `TH-*` amplían el seguimiento sin renumerar las tareas
01–18 de [PLAN.md](../PLAN.md). El porcentaje mide trabajo comprobado: el
inventario no equivale a haber integrado o validado un nuevo juego.

| ID | Tarea | Resultado y criterio de cierre | Depende de | Estado | Progreso |
|---|---|---|---|---|---:|
| TH-00 | Conservar el punto de recuperación | Código anterior publicado, árbol limpio, sin datos/binarios de usuario en Git y etiqueta `blues-baseline-2026-11-23` identificada. 17 sigue abierta y 18 queda pospuesta. | Arreglos actuales de Blues | Completada: corte en `dd07383` | 100 % |
| TH-01 | Auditar datos y motores | Inventario con firmas, conjuntos/variantes admitidos, commits/forks, dependencias, licencias/créditos y funciones declaradas frente a verificadas. Resolver detección de `PRE1.EXE` y carpeta mixta de Prehistorik 2. | TH-00 | Completada: variantes identificadas, preparación DIET demostrada, reglas de detección y bases/licencias documentadas | 100 % |
| TH-02 | Validar motores originales | Compilar y probar Jukebox Adventure, Prehistorik 2, OpenTitus y Olduvai por separado con nuestros datos. Registrar arranque, controles, audio, recorrido y salida de referencia. | TH-01 | Completada: cuatro motores compilan y pasan rutas cortas/control/audio/salida en Linux y Windows; límites registrados en ENGINE_REFERENCE_TESTS.md | 100 % |
| TH-03 | Definir arquitectura y transición | Contrato de motores, capacidades, catálogo/partidas, rutas configurables y organización C/C++/SDL. Plan de nombres/binarios/repositorio de Titus Engine con compatibilidad de Blues. Elegir estrategia de ejecución. | TH-01, TH-02 | Completada: C++17/SDL2, runtimes separados, contratos v1, perfiles/importación y transición documentados y comprobados como especificación | 100 % |
| TH-04 | Implementar catálogo y detección | Añadir, volver a localizar y quitar instalaciones sin borrar datos. Reconocer conjuntos, variantes y ambigüedad de carpetas mixtas; separar fuente ADF y edición extraída. Catálogo persistente con versiones de esquema. | TH-03 | Sin iniciar | 0 % |
| TH-05 | Crear biblioteca global | Menú principal para juegos instalados, ficha, selector de instalación/DOS/Amiga, estados claros, teclado/mando y preferencias de selección. Verificar ventana y pantalla completa. | TH-04 | Sin iniciar | 0 % |
| TH-06 | Integrar Blues Brothers | Arrancar DOS/Amiga desde biblioteca, conservar controles, HUD, ayudas, cámara, zoom, slots/miniaturas y rebobinado. Volver a biblioteca limpiamente; importación inicial reversible de opciones/slots a perfil privado sin sobrescribir originales. | TH-05 | Sin iniciar | 0 % |
| TH-07 | Incorporar motores del proyecto base | Jukebox Adventure y Prehistorik 2 desde Windows/biblioteca, con datos y preferencias separados. Comprobar que las opciones no implementadas no se anuncien como disponibles; revisar la saturación de mezcla observada en P2 sin afectar Blues. | TH-02, TH-06 | Sin iniciar | 0 % |
| TH-08 | Incorporar Titus the Fox DOS | Adaptador OpenTitus con nuestros archivos, controles/audio comprobados, configuración externa al original, cierre y retorno. Mantener créditos y revisión del motor de origen. | TH-02, TH-06 | Sin iniciar | 0 % |
| TH-09 | Incorporar Prehistorik 1 DOS | Adaptador Olduvai con identificación del conjunto `FILES*`/`PRE1.EXE` por contenido, sin renombrar originales. Arranque, controles, audio, cierre y retorno comprobados. | TH-02, TH-06 | Sin iniciar | 0 % |
| TH-10 | Unificar ajustes y partidas | Valores globales y particulares por juego/edición; capacidades reales, volúmenes, mandos y catálogo de partidas. Migración reversible de Blues, rechazo de saves ajenos y conservación de formatos por motor. | TH-07, TH-08, TH-09 | Sin iniciar | 0 % |
| TH-11 | Evaluar integración en proceso | Piloto con un motor; propiedad y ciclo de SDL/audio/vídeo, errores y rendimiento comparados con procesos separados. Registrar qué adaptadores se integran y cuáles permanecen aislados. | TH-03, TH-10 | Sin iniciar | 0 % |
| TH-12 | Validar la primera expansión | Matriz por juego/edición, arranque/salida repetidos, aislamiento de datos/ajustes, fallos recuperables, regresiones de Blues y documentación del catálogo realmente soportado. Habilita la tarea 18 tras cerrar también 17. | TH-10; decisión TH-11 | Sin iniciar | 0 % |
| TH-13 | Investigar datos Amiga adicionales | Examinar imágenes y carpetas Amiga extraídas, identificar Prehistorik 1/2, formato de disco y recursos sin modificarlos. Reutilizar herramientas disponibles si son aplicables; identificar motores/código o trabajo de ingeniería inversa y entregar viabilidad por juego. | TH-01 | Sin iniciar | 0 % |
| TH-14 | Añadir Titus the Fox Amiga | Si TH-13 confirma una vía viable, implementar su adaptador y verificar gráficos, física, niveles, audio y final. Registrar la edición en el selector tras validarla. | TH-13, TH-08 | Pendiente de viabilidad | 0 % |
| TH-15 | Añadir Prehistorik 1 Amiga | Si TH-13 confirma una vía viable, implementar su adaptador y verificar gráficos, física, niveles, audio y final. Registrar la edición en el selector tras validarla. | TH-13, TH-09 | Pendiente de viabilidad | 0 % |
| TH-16 | Añadir Prehistorik 2 Amiga si se confirma el conjunto | Tras identificar los recursos y la vía de motor, implementar el adaptador y verificar gráficos, física, niveles, audio y final. No confundirlo con Prehistorik 1 ni asumir compatibilidad del motor DOS. | TH-13, TH-07 | Pendiente de identificación/viabilidad | 0 % |

La disponibilidad de ADF no permite dar TH-14/TH-15/TH-16 por implementadas. La primera
expansión tiene como mínimo los cinco juegos DOS y Blues Brothers Amiga. Las
ediciones Amiga adicionales tienen seguimiento propio y no bloquean esa primera
entrega si la investigación determina que requieren un port independiente.

La tarea **18** se mantiene en el plan principal: preparar después el paquete
Windows de Titus Engine, con ejecutables/dependencias, lanzador, instrucciones,
catálogo soportado y prueba desde una carpeta nueva. Evitar duplicarla en TH-12.

### Entregas propuestas

1. **Primera entrega:** biblioteca y Blues Brothers DOS/Amiga; importar sus
   instalaciones actuales y regresar al menú conservando la versión de referencia.
2. **Segunda entrega:** Jukebox Adventure y Prehistorik 2, sujetos al resultado de
   sus pruebas originales. No exigir todas las mejoras nuevas para incorporarlos.
3. **Tercera entrega:** Titus the Fox y Prehistorik 1. Su orden podrá cambiar si la
   auditoría encuentra una base más sencilla o un bloqueo en alguno de ellos.
4. **Ediciones Amiga adicionales:** investigar las imágenes y carpetas extraídas disponibles mediante
   TH-13 y, cuando sea viable, incorporar Fox y las ediciones de Prehistorik
   identificadas mediante TH-14/TH-15/TH-16.
5. **Distribución:** tarea 18, después de la primera expansión validada y el cierre
   de 17. Otras mejoras por motor y Moktar podrán plantearse en entregas posteriores.

No se asignan fechas ni estimaciones de esfuerzo por haber completado las referencias;
TH-04 inicia la implementación del catálogo definido en TH-03.

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

## Decisiones cerradas y pendientes posteriores

- TH-03 fija frontend `titus-engine`, runtimes separados y compatibilidad de CLI,
  historial, créditos y rutas anteriores; no renombra ahora directorio/remoto.
- TH-01/02 fijan motores, revisiones y límites comprobados; TH-06–09 implementan
  los adaptadores, sin asumir que el motor de referencia ya hable el protocolo.
- Diseño visual final de la biblioteca y origen de sus imágenes opcionales.
- Qué adaptadores merece la pena integrar en proceso y cuáles seguirán separados.
- Qué mejoras adicionales recibirá cada juego después de asegurar su fidelidad.

Siguiente trabajo de expansión: **TH-04**, implementar el core del catálogo y los
detectores a partir del contrato TH-03 y del inventario real. En paralelo, atender
los errores que el usuario encuentre en la tarea 17.
La distribución 18 se retoma después de la primera expansión validada.
