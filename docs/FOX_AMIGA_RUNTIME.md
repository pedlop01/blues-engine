# TH-14: Titus the Fox Amiga

Estado: **en curso, 20 %**. Primer bloque completado: lectura nativa de recursos y
visores estáticos. **Todavía no es un port jugable** ni un adaptador registrado.
La edición DOS, los perfiles existentes y las capacidades del lanzador no cambian.

## Progreso

| Bloque | Criterio | Estado | Peso completado |
|---|---|---|---:|
| Recursos y referencia gráfica | Descompresores nativos contrastados, mapas/tiles/sprites/pantallas leídos, escenas exportadas y paletas obtenidas del original | Completado con los límites siguientes | 20 % |
| Lógica Amiga | Interpretar POIL y correspondencias de sprites; física, objetos, enemigos, capas, niveles, transiciones y final verificados | Pendiente | 0 % |
| Audio Amiga | Reproductor PAT, instrumentos/efectos y mezcla estable, contraste auditivo | Pendiente | 0 % |
| Adaptador e integración | Preparación privada nativa, protocolo, preferencias por edición, retorno seguro; exponer solo capacidades implementadas y conservar objetivo PX | Pendiente | 0 % |
| Validación jugable | Regresión DOS/Blues, campaña Amiga y comprobaciones Linux/Windows, controles, audio y final | Pendiente | 0 % |

El porcentaje cuenta bloques terminados, no horas estimadas ni porcentaje de
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
  ni instala recursos comerciales. Sin paleta extraída usa **grises de diagnóstico**,
  declarados en su informe, para no presentar colores inventados como originales.

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

POIL se descomprime pero sus registros todavía no se convierten a estados de
OpenTitus. No se asume que DOS y Amiga compartan índices de sprites, velocidades
ni semántica de colisiones. La prioridad siguiente es un nivel con objetos y
colisiones reales antes de extenderlo a toda la campaña.

## Construcción y uso

El lector es C++17 sin SDL, Unicorn, amitools ni intérprete. El probe es opcional,
no se instala en el paquete de juego y no figura en el registro de motores.

```sh
cmake -S . -B build/fox-amiga -DTITUS_BUILD_FOX_AMIGA_PROBE=ON
cmake --build build/fox-amiga -j 4
ctest --test-dir build/fox-amiga --output-on-failure
build/fox-amiga/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --output build/fox-amiga-gray
```

La salida debe ser una carpeta nueva fuera de la carpeta de originales. Para
paleta real y contraste independiente del descompresor se usa la herramienta de
investigación, con dependencias externas que no pasan al motor:

```sh
python3 -m venv build/fox-amiga-audit-env
build/fox-amiga-audit-env/bin/pip install unicorn==2.1.4
build/fox-amiga-audit-env/bin/python tools/validate_fox_amiga_resources.py --datapath ../orig-games/fox-amiga --output build/fox-amiga-reference --amitools ../tools/amitools --probe build/fox-amiga/titus-fox-amiga-probe
build/fox-amiga/titus-fox-amiga-probe --datapath ../orig-games/fox-amiga --output build/fox-amiga-level1 --palette build/fox-amiga-reference/palette.json --level 1 --x 0 --y 192
```

Amitools usado: `521ce0525eb602e40df1a0cc894e76db84ce3ce2`. El script solo
acepta el SHA-256 de `main` auditado en TH-13 y su disposición Hunk exacta.
Ejecuta únicamente el empaquetador aislado y las rutinas de descompresión bajo
Unicorn, con límites de instrucciones, memoria y destinos de control. Intercepta
lectura/reserva en buffers privados; no arranca AmigaOS, juego, trainer ni hardware.
No se guardan sus instrucciones en el repositorio. La salida comercial descodificada
se mantiene en memoria; solo los previews privados y hashes salen a disco.

## Evidencia y límites

[Comparación con el original](measurements/th14-resources.json) y
[escenas generadas](measurements/th14-scenes.json). Las pruebas sintéticas
comprueban códigos LZW especiales/dañados, árboles Huffman inválidos/fin/tamaño,
SPR truncados/sobrantes, límites de mapa, datos planares, flags PackBits y EHB.
Los informes de esta fase no sustituyen la validación visual y auditiva jugable.
La preparación futura del producto tampoco dependerá del emulador de auditoría.

Comprobaciones realizadas: seis grupos CTest en Linux, unitarias Windows x64
sobre Windows ARM64, los 65 recursos y todos los tipos de preview bajo
AddressSanitizer/UndefinedBehaviorSanitizer (`-O1`,
`ASAN_OPTIONS=detect_stack_use_after_return=0`). En Windows se ha corregido la
normalización UNC usando la infraestructura de rutas del catálogo y entrada
Unicode `wmain`; no se modifica el lanzador. Los 65 hashes y diez BMP del nivel
1 (mapa, seis bancos y tres PC1) son idénticos entre Linux y Windows desde UNC.
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
