# TH-15: Prehistorik 1 Amiga

Estado: **en curso, 5 %**. Primer bloque: lectores nativos acotados y exportación
diagnóstica de gráficos. No hay todavía simulación Amiga ni adaptador jugable;
la biblioteca conserva esta edición como no disponible para jugar.

La carpeta local se llama `orig-games/prehistorik2-amiga`, pero contiene
**Prehistorik 1**, contrastado en TH-13 con `Prehistorik.adf` (71 archivos).
No se renombra ni modifica. Prehistorik 2 no salió para Amiga; TH-16 sigue
cancelada. La edición DOS continúa usando Olduvai sin cambios.

## Seguimiento

| Bloque | Resultado y siguiente comprobación | Estado |
|---|---|---|
| Lectores y diagnóstico | LZSS, MAT/PackBits, cuatro planos y máscara; PC1 y muestras 8SVX | Implementado y contrastado con lectores Python independientes |
| Paletas y render original | Pantallas PC1 con su paleta; falta asignar y contrastar paletas de actores y colocación original | Parcial |
| Nivel y colisiones | DAT/DUR, elementos, anclajes y capas; comprobar diferencias con DOS | Pendiente |
| Simulación y campaña | Movimiento, actores, interacciones, niveles y final, contrastados con Amiga | Pendiente |
| Audio | Frecuencias/loops y PCM leídos; faltan secuenciador PAT y reproducción integrada | Pendiente |
| Adaptador y opciones comunes | Biblioteca, cámara suave/encuadre, HUD, menú, ayudas, ocho slots y rebobinado | Pendiente |
| Validación jugable | Linux/Windows, regresión, campaña, retorno seguro y revisión del usuario | Pendiente |

El 5 % registra el bloque inicial implementado; no expresa compatibilidad del
juego ni da por completado el primer hito de TH-13. Un visor no es un port.

## Resultados del bloque inicial

- 21 MAT comprimidos coinciden con el lector LZSS estricto de investigación.
  Se añade el charset monocromo sin comprimir: **22 bancos y 997 registros**.
- Se comprueban todos los registros, tanto planos directos como PackBits,
  dimensiones, longitudes y consumo completo. Se conservan por separado color
  y máscara: negro opaco no se confunde con transparencia. Los atlas muestran
  hasta los primeros 64 sprites de cada banco, con damero y escala de grises.
  **No son las paletas definitivas del juego.**
- Las **10 pantallas PC1** se decodifican completas con cuatro planos y su paleta
  RGB12. Se reutiliza el lector PC1 acotado ya validado en Fox; compartir esta
  primitiva no implica compartir la física ni el motor de Fox. Se ha inspeccionado
  visualmente el título y el atlas de L1SPR.
- Las **13 muestras IFF 8SVX** pasan validación de FORM/chunks/padding, VHDR,
  frecuencias, octava, PCM sin comprimir y límites de repetición. Se conserva el
  PCM original, sin declarar todavía música o efectos reproducibles en el juego.
- Los hashes de descompresión, colores, máscaras y PCM se contrastan con
  lectores Python separados y entre Linux, Windows y ASan/UBSan. Esto demuestra
  consistencia de lectura, **no equivalencia de simulación con el original**.
- Los originales conservan bytes, tamaños y fechas. Imágenes y datos extraídos
  quedan solo en `build/`, nunca en Git ni en los paquetes jugables.

Evidencia distribuible: [measurements/th15-resource-validation.json](measurements/th15-resource-validation.json).

## Compilar y repetir

```sh
cmake -S . -B build/pre1-amiga -DTITUS_BUILD_PRE1_AMIGA_PROBE=ON
cmake --build build/pre1-amiga -j
ctest --test-dir build/pre1-amiga --output-on-failure
python3 tests/check_pre1_amiga_data.py \
  --probe build/pre1-amiga/titus-pre1-amiga-probe \
  --datapath ../orig-games/prehistorik2-amiga \
  --output build/pre1-amiga-check-new
```

El probe no necesita SDL, no se instala ni registra como juego. Su salida incluye
BMP privados y un JSON con hashes; rechaza un destino existente o dentro de los
originales. El comprobador admite `--windows` y requiere un directorio de salida
nuevo bajo `build/`. Python y sus lectores son herramientas de desarrollo.

Semántica LZSS/MAT basada en Olduvai de Krzysztof Sokołowski,
revisión `a69e9204fc311c8810d696a6b799000d1bcd1642`, GPL-3.0-or-later;
se conservan atribución y licencia en los fuentes. Nuestros lectores son
estrictos y rechazan truncamientos, en vez de completar entradas con ceros.
No se distribuyen recursos ni tablas comerciales.

El siguiente bloque es identificar paletas de actores y mapas DAT/DUR y
representar un nivel con colocación y colisiones verificadas. Después se
incorporará la lógica de juego y la misma experiencia común prevista en el plan;
no se habilitan capacidades antes de implementarlas.
