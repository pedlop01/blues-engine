# TH-14: Titus the Fox Amiga

Estado: **en curso, 30 %**. Lectura nativa de recursos y referencia gráfica
completadas; inicialización de niveles, consultas de colisión y transiciones
contrastadas con el original. **Todavía no es un port jugable** ni un adaptador registrado.
La edición DOS, los perfiles existentes y las capacidades del lanzador no cambian.

## Progreso

| Bloque | Criterio | Estado | Peso completado |
|---|---|---|---:|
| Recursos y referencia gráfica | Descompresores nativos contrastados, mapas/tiles/sprites/pantallas leídos, escenas exportadas y paletas obtenidas del original | Completado con los límites siguientes | 20 % |
| Inicialización y consultas Amiga | POIL, normalización de mapas/tablas, entidades iniciales, consultas de colisión, puertas y condiciones de salida contrastadas | Completado, 10 de los 20 puntos del bloque de lógica | 10 % |
| Simulación Amiga | Física, animaciones, objetos/enemigos/elevadores en movimiento, capas definitivas, campaña y final | Pendiente, 10 puntos restantes de lógica | 0 % |
| Audio Amiga | Reproductor PAT, instrumentos/efectos y mezcla estable, contraste auditivo | Pendiente | 0 % |
| Adaptador e integración | Preparación privada nativa, protocolo, preferencias por edición, retorno seguro; exponer solo capacidades implementadas y conservar objetivo PX | Pendiente | 0 % |
| Validación jugable | Regresión DOS/Blues, campaña Amiga y comprobaciones Linux/Windows, controles, audio y final | Pendiente | 0 % |

Los pesos suman 100 puntos: recursos 20, inicialización 10, simulación 10, audio
20, adaptador 20 y validación jugable 20. No son horas estimadas ni porcentaje de
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

- Portar el paso de jugador (`0x70b4`) y sus auxiliares de colisión/interacción,
  contrastando trazas de caminar, frenar, saltar, caer y cambiar de zona. Fijar la
  frecuencia lógica por evidencia; presentar a 60 Hz no debe acelerar la física.
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
