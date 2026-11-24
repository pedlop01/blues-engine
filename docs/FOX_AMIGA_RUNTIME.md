# TH-14: Titus the Fox Amiga

Estado: **en curso, 54 %**. Lectura nativa de recursos y referencia gráfica
completadas; inicialización de niveles, consultas de colisión y transiciones
contrastadas con el original. El núcleo de movimiento sobre terreno está
contrastado por ticks, incluidas escaleras y movimiento agachado. Las 11 canciones
PAT tienen reproducción nativa contrastada por interrupciones. Se añaden agarre,
transporte y lanzamiento dentro del paso del personaje. **Todavía no es un port jugable** ni un adaptador registrado.
La edición DOS, los perfiles existentes y las capacidades del lanzador no cambian.

## Progreso

| Bloque | Criterio | Estado | Peso completado |
|---|---|---|---:|
| Recursos y referencia gráfica | Descompresores nativos contrastados, mapas/tiles/sprites/pantallas leídos, escenas exportadas y paletas obtenidas del original | Completado con los límites siguientes | 20 % |
| Inicialización y consultas Amiga | POIL, normalización de mapas/tablas, entidades iniciales, consultas de colisión, puertas y condiciones de salida contrastadas | Completado, 10 de los 20 puntos del bloque de lógica | 10 % |
| Movimiento sobre terreno | Caminar, frenar, cambiar dirección, saltar/caer y resolver paredes/techos; selección lógica de frames, sin actores | Completado como módulo aislado y acotado | 5 % |
| Simulación e interacciones restantes | Escaleras, agacharse, objetos y sus interacciones físicas contrastados; daño/recuperación del jugador incorporados. Pendientes IA, plataformas, anclajes/oclusión, campaña y final | Parcial, 1 punto de lógica pendiente | 4 % |
| Audio Amiga | Secuenciador PAT y muestras externas nativos; 132.000 interrupciones contrastadas y PCM portable. Pendientes efectos, integración y contraste auditivo | Parcial, 10 puntos de audio pendientes | 10 % |
| Adaptador e integración | Extracción de tablas nativa sin dependencias de investigación completada; faltan protocolo, preferencias por edición y retorno seguro. Conservar objetivo PX | Parcial, preparación nativa contrastada | 5 % |
| Validación jugable | Regresión DOS/Blues, campaña Amiga y comprobaciones Linux/Windows, controles, audio y final | Pendiente | 0 % |

Los pesos suman 100 puntos: recursos 20, inicialización 10, simulación 10, audio
20, adaptador 20 y validación jugable 20. Los diez puntos de simulación se
dividen en movimiento sobre terreno (5) e interacciones restantes (5). No son horas estimadas ni porcentaje de
compatibilidad. No se habilitará Jugar por tener ya un visor de mapas.

## Resultados del primer bloque

- Los **65 recursos comprimidos** generan exactamente el mismo SHA-256 que los
  descompresores del ejecutable original: mapas TBL, tiles BRIK, datos POIL,
  sprites, imágenes y canciones PAT. Descomprimir PAT no equivale a reproducirlo.
- Los **15 conjuntos de mapas** presentes (01–14 y 16) generan una vista nativa
  de 640×400 o la altura disponible. Los mapas tienen anchura de 256 tiles de
  16×16, con altura variable. No se inventa un nivel 15 ausente en esta copia.
- Se leen y exportan **398 sprites** en seis bancos: Fox 30, objetos 71,
  enemigos 232, fuente 42, secuencia de beso 19 y final 4. No se han asignado
  todavía sus animaciones a la lógica ni identificado todas sus colisiones.
- Se reconstruyen el logo Titus (4 planos), título/menú (5 planos) y presentación
  (6 planos, extra-half-brite), con sus propias paletas. Inspección visual local
  del título, presentación, Fox y escena del nivel 1; no se certifica cada frame
  ni toda la colocación del juego original.
- La paleta jugable RGB12 y sus 16 valores de color 14 por nivel se obtienen del
  ejecutable propio. No se usa la paleta del título para dibujar los niveles.
- La herramienta genera BMP y metadatos fuera de los originales; no contiene
  ni instala recursos comerciales. Desde el bloque de inicialización, `--palette`
  es obligatorio: ese JSON incluye también las reglas originales de aceptación
  de enemigos. Se retira el modo gris inicial para no representar estados con
  reglas supuestas. Los JSON antiguos se regeneran con el comprobador.

### Formatos verificados

La cabecera externa tiene `método: u16le` y `longitud: u32le`.

- Método 2: LZW, códigos de 9–12 bits LSB primero; grupos de ocho códigos,
  alineación al cambiar anchura y al limpiar la tabla, código clear 256.
  No es el flujo MSB del SQZ DOS. Se limita la salida y se rechazan referencias
  inválidas y entradas agotadas.
- Método 1: árbol Huffman con nodos de dos aristas little endian con signo,
  bits LSB primero, hojas `-(valor+1)` y fin 256. Incluye firma `76 ff`, campo
  de dos bytes y nombre terminado en cero. El campo de dos bytes no se ha
  validado como checksum; no se promete comprobación de ese campo.
- BRIK: 256 tiles, 128 bytes por tile, cuatro bloques de plano por tile.
- SPR: contador y registros de anchura/altura/campo interno big endian,
  cuatro planos consecutivos; todas las muestras de esta copia tienen el campo
  interno cero. Una compresión interna desconocida se rechaza explícitamente.
- PC1: componentes de paleta en tres arrays separados; las imágenes de 4/5
  planos usan flags PackBits y literales/repeticiones por columnas. La de 6
  planos contiene datos planares directos. Se comprueban ambas longitudes
  declaradas, desbordamientos y consumos completos. EHB divide los componentes
  RGB12 antes de expandirlos a RGB24.

## Inicialización y consultas contrastadas

Se interpreta POIL completo (3060 bytes): tablas de 256 entradas, 40 objetos,
50 registros candidatos de enemigo, 100 bonus, 20 puertas y diez elevadores,
además de salida del personaje, meta y referencias de cámara. Los campos de dos
bytes son little endian, a diferencia de las cabeceras SPR. Se conservan los
parámetros de comportamiento aún sin simular; no se confunden con estados de
velocidad ya actualizados por la lógica original.

- Se reproducen la limpieza de marcadores ocultos de los niveles 1/4 y su
  reposición en bonus válidos. Las alturas vienen del mapa, no de los límites
  auxiliares de cámara, que pueden estar fuera del encuadre normal.
- Se normalizan los códigos horizontales, de suelo y techo, las ternas de tiles
  animados y las filas que requieren actualización. Esto prepara la animación;
  todavía no avanza ni reproduce toda su temporización.
- Se aplican los ajustes de un píxel del personaje, objetos y enemigos sujetos
  a gravedad. Las consultas de colisión reproducen los tres lectores originales
  en coordenadas válidas; fuera del mapa devuelven una frontera bloqueada
  explícita y nunca leen memoria ajena. Esa protección no pretende reproducir
  lecturas fuera de límites del original.
- La pertenencia a las animaciones válidas se obtiene del ejecutable, en vez de
  deducirla de que exista un dibujo. En nivel 11 esto evita activar tres registros
  con un frame que el original rechaza. La aceptación de enemigos no usa excepciones por nivel ni tablas de
  animación comerciales incrustadas en el lector.
- Las puertas calculan destino y cámara con los mismos ajustes del original,
  incluido `y - 2` al salir. Las condiciones de meta respetan su área de dos por
  dos tiles, el bloqueo de salida y el objeto requerido en el nivel 11. Son
  funciones nativas comprobadas, todavía sin bucle jugable que las invoque.
- Se confirma en la lógica original que después de 14 se pasa a 16. El final
  de campaña y la secuencia del nivel 16 siguen pendientes de integración.

El arnés compara 18 firmas/valores por nivel en los 15 mapas: **1980 puntos de
consulta de colisión** (tres lectores cada uno), **82 transiciones de puerta**
con entrada dentro del mapa y **540 escenarios de condición de salida**.
Compara también las proyecciones iniciales de objetos/enemigos/elevadores; no
certifica todos sus parámetros internos, hitboxes, IA o movimiento.

El probe añade `scene.bmp`, con entidades quietas en sus posiciones iniciales, y
`collisions.bmp`, que muestra qué tablas devuelven valores distintos de cero:
rojo horizontal, verde suelo y azul techo, combinados cuando corresponda. Son
**vistas de diagnóstico**: los frames y el orden de dibujo no sustituyen al
render jugable, a la animación ni a la oclusión definitiva de cada entidad.

## Construcción y uso

El lector es C++17 sin SDL, Unicorn, amitools ni intérprete. El probe es opcional,
no se instala en el paquete de juego y no figura en el registro de motores.

```sh
cmake -S . -B build/fox-amiga -DTITUS_BUILD_FOX_AMIGA_PROBE=ON
cmake --build build/fox-amiga -j 4
ctest --test-dir build/fox-amiga --output-on-failure
```

La salida debe ser una carpeta nueva fuera de la carpeta de originales. Para
paleta real y contraste independiente del descompresor se usa la herramienta de
investigación, con dependencias externas que no pasan al motor:

```sh
python3 -m venv build/fox-amiga-audit-env
build/fox-amiga-audit-env/bin/pip install unicorn==2.1.4
build/fox-amiga-audit-env/bin/python tools/validate_fox_amiga_resources.py --datapath ../orig-games/fox-amiga --output build/fox-amiga-reference --amitools ../tools/amitools --probe build/fox-amiga/titus-fox-amiga-probe --check-levels
build/fox-amiga/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --output build/fox-amiga-level1 --palette build/fox-amiga-reference/palette.json --level 1 --spawn-view 1
```

Amitools usado: `521ce0525eb602e40df1a0cc894e76db84ce3ce2`. El script solo
acepta el SHA-256 de `main` auditado en TH-13 y su disposición Hunk exacta.
Ejecuta únicamente el empaquetador aislado, los lectores, inicializadores y
consultas seleccionadas bajo
Unicorn, con límites de instrucciones, memoria y destinos de control. Intercepta
lectura/reserva en buffers privados; no arranca AmigaOS, juego, trainer ni hardware.
No se guardan sus instrucciones en el repositorio. La salida comercial descodificada
se mantiene en memoria; solo los previews privados y hashes salen a disco.

## Evidencia y límites

[Comparación con el original](measurements/th14-resources.json) y
[escenas iniciales](measurements/th14-scenes.json). La continuación tiene
[informe de inicialización](measurements/th14-initialization.json) y
[matriz Linux/Windows/sanitizadores](measurements/th14-initialization-validation.json). Las pruebas sintéticas
comprueban códigos LZW especiales/dañados, árboles Huffman inválidos/fin/tamaño,
SPR truncados/sobrantes, límites de mapa, datos planares, flags PackBits y EHB.
Los informes de esta fase no sustituyen la validación visual y auditiva jugable.
La preparación futura del producto tampoco dependerá del emulador de auditoría.

Comprobaciones realizadas: seis grupos CTest en Linux, unitarias Windows x64
sobre Windows ARM64, los 65 recursos y todos los tipos de preview bajo
AddressSanitizer/UndefinedBehaviorSanitizer (`-O1`,
`ASAN_OPTIONS=detect_stack_use_after_return=0`). En Windows se ha corregido la
normalización UNC usando la infraestructura de rutas del catálogo y entrada
Unicode `wmain`; no se modifica el lanzador. El primer bloque contrastó 65 hashes y diez BMP del nivel 1. La continuación
compara los estados y doce BMP de cada uno de los 15 mapas entre Linux y
Windows desde UNC, y repite esa matriz con sanitizadores.
La matriz reproducible genera los 15 mapas y rechaza solicitudes inválidas:

```sh
python3 tests/check_fox_amiga_probe.py --probe build/fox-amiga/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --palette build/fox-amiga-reference/palette.json --output build/fox-amiga-scenes
```

## Referencias para el siguiente bloque

Offsets dentro de los segmentos expandidos de la revisión fijada, no direcciones
portables ni datos que deban codificarse en el futuro runtime:

| Función/dato observado | Segmento | Offset |
|---|---:|---:|
| Paleta jugable RGB12 | 3 | `0x02aa` |
| Color 14 por nivel | 3 | `0x0340` |
| Inicialización / lectura LZW | 4 | `0x3582` / `0x384c` |
| Inicialización / lectura Huffman | 4 | `0x3b98` / `0x3c90` |
| Lectura y preparación de bancos SPR | 4 | `0x1f78`, `0x29e8` |
| Lectura PC1 con flags / planar directo | 4 | `0x310a` / `0x3318` |
| Lectura con PackBits interno | 4 | `0x1be6` |

El comprobador coloca el segmento 4 en `0x0e0000`. Su lectura LZW original puede
superar la petición de 512 bytes para terminar una cadena; el arnés reserva margen
y compara el resultado completo. El lector nativo valida el límite global de
salida y rechaza desbordamientos. No copiar esa falta de límite por bloque al
adaptador nuevo. La diferencia DOS/Amiga de los mapas y registros POIL todavía
requiere una comparación semántica, no solo de tamaños o archivos.

Desde la ampliación, el descriptor privado de referencia contiene la pertenencia
a la tabla de animaciones original (segmento 3, `0x12da`, hasta el centinela
`0xfc18`). Solo describe si un frame es admisible; no incluye las secuencias ni
implementa su reproducción. Para el contraste se inicializan buffers privados y
se ejecutan el lector de mapa `0x46e2` y el inicializador POIL `0x7b38`,
además de las consultas `0x65d4`/`0x65ec`/`0x6604`, puertas
`0x6716` y salida `0x6fb4`. No se inicia el bucle de juego ni audio/hardware.
El bloque siguiente debe conectar física y animación reales, conservando estas
firmas como referencia y la separación entre lógica y presentación exigida por PX.

### Siguiente bloque de simulación

- El núcleo sobre terreno del paso `0x70b4` ya está contrastado por ticks (ver
  bloque siguiente). Completar sus estados e interacciones y fijar la frecuencia
  lógica por evidencia; presentar a 60 Hz no debe acelerar la física.
- Recuperar las referencias/recortes de los sprites que preparan `0x1f78` y
  `0x29e8`; el visor usa un anclaje provisional al centro/pies, no certifica
  hitboxes ni referencias animadas. Evitar corregir saltos visuales alterando
  posiciones físicas.
- Conectar IA, coger/lanzar objetos, bonus y plataformas. La escena actual
  superpone Fox, objetos y enemigos quietos; los elevadores están interpretados,
  pero su representación/movimiento aún no se superponen. En niveles 2/3 hay
  cuatro registros de elevador con sprite/velocidad/dirección fuera de los rangos
  renderizables (slots 6 y 9). Se conservan en la proyección del cargador original;
  no deben usarse como índices sin validar al implementar la simulación.
- Integrar oclusión, temporización y final antes de cerrar el bloque. Los renders
  diagnósticos y sus hashes no sustituyen a una campaña ni permiten cerrar TH-14.

El arnés omite únicamente las tres llamadas de interfaz de progreso del lector
original de mapas; las transformaciones de tiles sí se ejecutan en su código.
Se identifican además tres destinos de puerta del nivel 5 fuera de la altura del
mapa (slots 9, 10 y 12). El cálculo nativo conserva los valores originales para
el contraste; el futuro bucle deberá validar el destino antes de mover al jugador,
sin convertir un registro inaccesible o residual en una lectura fuera del mapa.
Las 82 comprobaciones verifican cálculos, no 82 recorridos jugables de habitaciones.

## Movimiento sobre terreno: 35 %

`terrain_step` es una función C++17 independiente del reloj, la ventana, SDL y
el emulador de auditoría. Implementa los estados quieto, caminar, salto y caída
sobre el mapa inicializado, junto con fricción, aceleración, gravedad, paredes,
suelo y golpes de cabeza. Conserva el orden de las consultas y el redondeo del
original: al integrar se descartan los bits fraccionarios de `|v|`, mientras que
al deshacer una colisión se usa desplazamiento aritmético. No añade una posición
subpíxel acumulada que cambie distancias o alturas de salto.

También conserva la selección lógica del frame y los contadores de paseo/salto;
las 14 entradas de paseo y cinco impulsos de salto se extraen del ejecutable
propio al regenerar el descriptor. La fase 5 de salto queda retenida al pasar a
caída, como hace el original; no se reinicia para "simplificar" el estado. El
anclaje gráfico, hardware sprites, interpolación y render animado final siguen
pendientes: comparar el índice de frame no certifica su dibujo en pantalla.

Se contrastan **1178 ticks, en 15 casos**, con once columnas por tick: posición,
velocidades, estado, frame, fase de paseo, fase de salto, contador de reposo,
fricción y orientación. Hay ocho escenarios sobre el terreno real del nivel 1,
cuatro geometrías sintéticas para paredes/techo/borde de plataforma, y tres
fixtures de suelo deslizante. Las geometrías generadas se suministran por igual
al código nativo y a las rutinas originales; no sustituyen la campaña de 15 mapas.

El arnés ejecuta el paso original `0x70b4` y sus auxiliares de colisión/fricción,
con los objetos cargados, sin ejecutar su simulación. Solo omite el consumo de
bonus (`0x6d10`) y la comprobación de final (`0x6fb4`) en estas trazas para aislar
la física. No entra en el bucle de juego, AmigaOS, audio o hardware. El motor
nativo no ejecuta esas instrucciones; solo el comprobador externo las usa como
referencia. En aquel bloque se rechazaban escaleras y agacharse; el bloque siguiente los incorpora. Daño, límites de mapa no cubiertos y estados
ajenos al subconjunto se rechazan expresamente, sin fingir que están soportados.
No se habilita Jugar ni se declara esta fase un juego completo.

```sh
build/fox-amiga-audit-env/bin/python tools/validate_fox_amiga_resources.py --datapath ../orig-games/fox-amiga --output build/fox-amiga-motion-reference --amitools ../tools/amitools --probe build/fox-amiga/titus-fox-amiga-probe --check-motion
python3 tests/check_fox_amiga_motion.py --probe build/fox-amiga/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --reference build/fox-amiga-motion-reference --output build/fox-amiga-motion-replay
```

Para repetir todas las pruebas del cargador junto con las de movimiento, añadir
`--check-levels` al comprobador. El descriptor anterior sin `walk_frames` y
`jump_impulses` sirve para los visores estáticos, pero debe regenerarse para
`--motion SCRIPT.json`. Los scripts y trazas completas quedan en `build/`; el
informe versionado contiene únicamente resultados, recuentos y firmas.

Evidencia: [contraste original](measurements/th14-motion.json) y
[Linux/Windows/sanitizadores](measurements/th14-motion-validation.json).
Los siguientes pasos son completar las interacciones, recuperar los anclajes de
sprites y conectar una presentación interactiva, manteniendo la lógica separada
de la frecuencia de dibujado y el objetivo PX de scroll muy suave.


## Escaleras y agacharse (ampliación del módulo de terreno)

Se añaden los estados 4 y 5, la entrada a la escalera desde arriba o al caer,
subida/bajada, salida superior, salida lateral y movimiento agachado con salto.
La escalera utiliza aceleración de 8 y velocidad horizontal máxima de 32;
agachado utiliza la aceleración normal con límite de 32. Se conserva la
alineación al centro del tile y el estado de salida superior que impide volver
a engancharse inmediatamente. El temporizador del salto agachado modifica los
frames posteriores, como en el original.

El contraste crece a **24 escenarios, 1578 ticks y 12 campos por tick**, contando
los 15 casos previos. Los nueve escenarios nuevos usan geometría sintética para
aislar cada transición; no equivalen a probar las escaleras de toda la campaña.
Linux, Windows x64 ejecutado y ASan/UBSan producen las mismas trayectorias.
Evidencia: [original](measurements/th14-ladders.json) y
[validación](measurements/th14-ladders-validation.json).

## Música PAT nativa

`fox_amiga_audio.cpp` interpreta las 15 descripciones de instrumentos externos,
los patrones de cuatro canales y la lista de órdenes. Los nombres se resuelven
sin distinguir mayúsculas y se rechazan rutas, muestras truncadas, bucles fuera
de límites y referencias inválidas. Las muestras se cargan reservando los dos
primeros bytes a cero, igual que el cargador original.

El secuenciador reproduce arpegio, deslizamiento de período, volumen, velocidad,
salto de posición y cambio de patrón. Conserva la peculiaridad de volumen
compartido entre canales durante la lectura de una fila. El efecto 4 se ignora,
como en el reproductor original. Las canciones suministradas no emplean E
(filtro analógico); su emulación analógica no está implementada.

Se comparan **12000 interrupciones por canción, 132000 en las 11 canciones**:
orden, siguiente fila, subtick, velocidad y período/volumen/período base de los
cuatro canales. El comprobador ejecuta únicamente el inicializador y el replay
originales; los registros de audio están en memoria privada, sin AmigaOS.
Las interrupciones provienen de VBlank (`0x5c56`); el modo PAL usa 50 Hz.
Esta frecuencia del audio no certifica todavía la frecuencia de la simulación.

El mezclador estéreo usa avance de muestra de punto fijo y reloj Paula PAL de
3546895 Hz. Produce WAV de 10 segundos por canción; sus bytes coinciden en Linux,
Windows y sanitizadores, sin saturación en estos fragmentos. El procesamiento
mantiene fase entre bloques de tamaños distintos. **La comparación de PCM es
entre nuestras compilaciones, no contra la salida analógica de un Amiga**.
No se ha realizado todavía una escucha comparada, ni se han conectado los
efectos sonoros, el dispositivo SDL o la música al runtime jugable.

```sh
build/th14-venv/bin/python tools/validate_fox_amiga_resources.py --datapath ../orig-games/fox-amiga --output build/fox-amiga-audio-reference --amitools ../tools/amitools --probe build/th14-native/titus-fox-amiga-probe --check-audio
python3 tests/check_fox_amiga_audio.py --probe build/th14-native/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --reference build/fox-amiga-audio-reference --output build/fox-amiga-audio-replay
build/th14-native/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --output build/fox-amiga-audio-preview --audio JEU1.PAT --audio-ticks 12000
```

El último comando genera `audio.wav`, `audio.json` y `report.json`. No necesita
el descriptor privado de paleta. `--check-audio`, `--check-motion` y
`--check-levels` pueden combinarse. Para Windows, el replay acepta `--windows`.
Las muestras privadas de esta sesión están en `build/th14-pat-linux2/`.
Evidencia: [replay original](measurements/th14-audio.json) y
[validación multiplataforma](measurements/th14-audio-validation.json).

## Trabajo necesario para entregar TH-14 a revisión final

La petición vigente es completar TH-14 sin supervisión y reservar el 100 % a la
revisión del usuario. **Aún no se ha alcanzado esa entrega**: el 54 % describe
los bloques implementados, no una tarea terminada pendiente solo de aceptación.

| Bloque pendiente | Criterio de entrega |
|---|---|
| Interacciones | Daño de enemigos, bonus y puertas activados por controles reales; integrar objetos, muerte y recuperación en el ciclo completo |
| Actores | IA, animaciones, impactos y plataformas; filtrar registros inválidos sin accesos fuera del mapa |
| Presentación | Anclajes y capas contrastados, cámara y temporización reales; presentación interactiva y final |
| Audio integrado | Efectos y prioridad del cuarto canal, dispositivo y pausa, volúmenes; escucha comparada |
| Preparación y adaptador | Reglas extraídas sin dependencias de investigación, proceso gestionado, preferencias Amiga y retorno al lanzador |
| Campaña y revisión | Pruebas de niveles/final y regresión DOS/Blues en Linux/Windows; revisión del usuario antes del 100 % |

Las capacidades públicas y las tareas PX siguen sin cambios. No se ha habilitado
el botón Jugar para esta edición ni sustituido música Amiga por audio DOS.
Los 462 archivos originales conservan tamaño, fecha de modificación y SHA256.


## Manipulación de objetos y paso del personaje

Se conserva un estado de 24 bytes por objeto, equivalente al original: tipo,
posición, velocidades, parámetros físicos, temporizador, agarre, flags y tamaños
de colisión. Los 71 conjuntos de parámetros se obtienen del ejecutable original
en el descriptor privado; no se deducen del tamaño de los dibujos. La
inicialización de los **40 objetos en cada uno de los 15 mapas** coincide en los
960 bytes completos, incluidas las entradas deshabilitadas.

Las operaciones de objeto reproducen el agarre por cercanía, su distinta frontera
horizontal según la orientación, el intercambio con la última posición de la
lista, los objetos especiales, el desplazamiento junto al personaje, la suelta
con corrección de paredes/suelo y los lanzamientos horizontal y vertical. Se
mantienen las diferencias por flags, la velocidad horizontal retenida al lanzar
hacia arriba y los cambios de tipo/altura al recoger ciertos objetos.

El contraste aislado comprende **94 casos y 578 operaciones**: las 15
inicializaciones reales, 71 tipos en estados sintéticos y ocho escenarios de
frontera/orden de selección. Los estados sintéticos no certifican que esos 71
tipos aparezcan o sean recogibles en una partida normal. No hay IA ni simulación
autónoma de objetos en estas pruebas.

`object_player_step` incorpora las acciones al paso del personaje: poses de
carga al caminar, saltar, caer, agacharse y usar escaleras; suelta bajo techo y
bloqueo del botón tras recoger hasta soltarlo. El contraste del movimiento crece
a **35 escenarios y 2252 ticks**; once escenarios nuevos comprueban también
los 960 bytes de objetos y los siete campos de control de carga en cada tick.
Solo se ejecuta el paso original del personaje y sus auxiliares; no se ejecutan
los bucles originales de física de objetos o enemigos. En aquel bloque todavía faltaba el vuelo autónomo y los impactos; se incorporan
en el bloque siguiente.
La variante especial de carga lateral asociada al flag original `0x8048a` tampoco
está implementada; los escenarios usan el modo normal, con ese flag a cero.

```sh
build/th14-venv/bin/python tools/validate_fox_amiga_resources.py --datapath ../orig-games/fox-amiga --output build/fox-amiga-objects-reference --amitools ../tools/amitools --probe build/th14-native/titus-fox-amiga-probe --check-objects
python3 tests/check_fox_amiga_objects.py --probe build/th14-native/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --reference build/fox-amiga-objects-reference --output build/fox-amiga-objects-replay
build/th14-venv/bin/python tools/validate_fox_amiga_resources.py --datapath ../orig-games/fox-amiga --output build/fox-amiga-carry-reference --amitools ../tools/amitools --probe build/th14-native/titus-fox-amiga-probe --check-motion
```

Linux, Windows x64 ejecutado y ASan/UBSan producen los mismos estados y firmas.
El replay de objetos acepta `--windows`; seis casos inválidos comprueban índices,
tipos, doble agarre, desbordamiento y acciones desconocidas. El replay de
movimiento compara también los estados de objetos cuando están presentes.
Evidencias: [objetos](measurements/th14-objects.json),
[su validación](measurements/th14-objects-validation.json),
[paso con carga](measurements/th14-carry.json) y
[su validación](measurements/th14-carry-validation.json).


## Física autónoma y preparación completamente nativa

`step_objects` aplica los pasos originales a objetos libres: gravedad, frenado,
rebotes, paredes, techos, agua/fuego, objetos apilados, contacto con el jugador,
alfombras y generación de humo. Mantiene la lista de objetos capaces de golpear
enemigos; su aplicación a la IA es trabajo pendiente. También conserva la
animación de tipos especiales y la caducidad fuera de pantalla de los proyectiles.

La comprobación conjunta contiene **182 escenarios y 9609 operaciones/pasos de
objetos**. De ellos, **88 escenarios y 9031 pasos** corresponden a física autónoma.
Se comparan objetos, estado físico del jugador, lista de impactos y humo. El
comprobador ejecuta el recorte original de pantalla y sustituye únicamente la
operación de blitter por su resultado de visibilidad: no certifica el dibujo
final ni la oclusión. Linux, Windows y ASan/UBSan coinciden en estos escenarios.
Evidencias: [contraste](measurements/th14-physics.json) y
[replays](measurements/th14-physics-validation.json).

El lector del ejecutable Hunk y de su empaquetado es ahora C++ acotado. Produce
los mismos **87400 bytes en cuatro secciones** que el desempaquetador original,
con las mismas reubicaciones virtuales. `native_rules` verifica la revisión SHA256
antes de extraer paletas, anclajes, tamaños, reglas de objetos, caminar/salto y
aceptación de animaciones. No ejecuta instrucciones del juego y no incorpora su
código o datos comerciales en el programa distribuible. Las secciones originales
solo se leen en memoria; los informes versionados contienen firmas.

El probe puede funcionar **sin `--palette`**. La referencia con Unicorn/amitools
sigue siendo exclusivamente una herramienta de comprobación, no una dependencia
del runtime. Los doce BMP de la preparación nativa coinciden con los del
descriptor de referencia. Tres revisiones inválidas se rechazan antes de usar
las tablas. Evidencia: [reglas nativas](measurements/th14-rules-validation.json).

```sh
build/th14-native/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --output build/fox-amiga-native-rules --rules-only 1
build/th14-venv/bin/python tools/validate_fox_amiga_resources.py --datapath ../orig-games/fox-amiga --output build/fox-amiga-simulation-reference --amitools ../tools/amitools --probe build/th14-native/titus-fox-amiga-probe --check-levels --check-objects --check-object-physics --check-motion
python3 tests/check_fox_amiga_objects.py --probe build/th14-native/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --reference build/fox-amiga-simulation-reference --output build/fox-amiga-physics-replay --physics-only
python3 tests/check_fox_amiga_rules.py --probe build/th14-native/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --reference build/fox-amiga-simulation-reference --output build/fox-amiga-native-rules-replay
```

## Daño, recuperación y límites del personaje

El paso completo del jugador incorpora el bloqueo temporal tras golpes, la
animación de recuperación, la muerte por superficies, su trayectoria y la señal
de reinicio. También contempla la salida inferior del mapa, el límite superior
y la variante de carga lateral. El paso de terreno aislado conserva sus límites
explícitos; el paso con objetos usa un contexto de cámara y ayudas.

La batería crece a **47 escenarios y 2934 ticks**, incluidos los anteriores.
Los doce casos nuevos comprueban bloqueo, estado de retorno, frame de golpe,
energía, límite de caída y señal de reinicio, además de posición/velocidad y
objetos. La invulnerabilidad se comprueba aparte como una ayuda del motor; no se
presenta como comportamiento original. Falta enlazar reinicios, vidas y daño de
IA con la sesión jugable. Evidencias: [original](measurements/th14-damage.json) y
[validación](measurements/th14-damage-validation.json).

La instrucción vigente es continuar hasta entregar TH-14, sin detenerse en estos
hitos. El siguiente bloque es la inicialización completa de enemigos, IA e
impactos, seguido del ciclo jugable, render, audio integrado y adaptador. El
100 % sigue reservado a la revisión del usuario.
