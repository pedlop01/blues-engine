# TH-13: viabilidad de las ediciones Amiga

Investigación completada el 24 de noviembre de 2026. No incorpora un runtime
Amiga nuevo ni cambia las capacidades del catálogo. Los originales se leen, nunca
se ejecutan ni se redistribuyen. La viabilidad es una evaluación por etapas, no
una promesa de que el motor DOS pueda cargar directamente estos datos.

## Resultado por juego

| Juego | Evidencia local | Decisión y primer hito |
|---|---|---|
| Titus the Fox | 89 archivos coinciden byte a byte con `fox.adf`; ejecutable Hunk y recursos propios BIN/TBL/SPR/PAT. | Candidato a TH-14, con riesgo alto: resolver primero descompresión y representar una escena Amiga estática. Continuar al juego solo tras comprobar ese hito. |
| Prehistorik 1 | 71 archivos coinciden con `Prehistorik.adf`. Los 21 MAT comprimidos pasan LZSS estricto y sus registros completos de sprites. Hay 13 efectos IFF 8SVX. | Vía parcial demostrada para reutilizar lectura de recursos en TH-15; faltan formatos y lógica. Primer hito: visor de sprites e imágenes con paletas y máscaras verificadas. |
| Prehistorik 2 | No salió para Amiga. `prehistorik2-amiga` contiene Prehistorik 1. | TH-16 cancelada / no aplicable; no requiere buscar datos ni implementar un adaptador. |

Se conserva el nombre de la carpeta `prehistorik2-amiga` para no romper rutas.
Ambos ADF tienen 901120 bytes y firma `DOS\0` (OFS). Las comparaciones prueban
los archivos de las carpetas contra el disco; permiten archivos extra en el ADF
y no certifican que la extracción contenga todo el disco ni una edición limpia.

## Motores y reutilización

[OpenTitus](https://github.com/stople/OpenTitus/tree/6540e8f2867f99aff54ad1a710ae6379ecb9c590)
se presenta como motor de las versiones DOS de Fox/Moktar. Su lector
[SQZ](https://github.com/stople/OpenTitus/blob/6540e8f2867f99aff54ad1a710ae6379ecb9c590/src/sqz.c)
no demuestra compatibilidad con los BIN/TBL/SPR Amiga. En las muestras locales
hay seis bytes iniciales distintos del encabezado DOS de cuatro bytes. Interpretar
los dos primeros como método y los siguientes cuatro como longitud little endian
es una **hipótesis**: no se ha validado un descompresor Fox Amiga. No conectar el
lector DOS a esos datos ni asumir que coinciden índices, colisiones o animaciones.

La revisión fijada de Olduvai es `a69e9204fc311c8810d696a6b799000d1bcd1642`.
Su lector [LZSS](https://github.com/ksokolowski/olduvai/blob/a69e9204fc311c8810d696a6b799000d1bcd1642/src/formats/lzss.cpp)
y los registros [MAT](https://github.com/ksokolowski/olduvai/blob/a69e9204fc311c8810d696a6b799000d1bcd1642/src/formats/mat.cpp)
permiten una prueba concreta sobre Prehistorik Amiga: longitud big endian,
ventana de 256 bytes, literales y referencias de 2–5 bytes. Nuestro lector de
investigación rechaza entradas agotadas, con límite de salida de 1 MiB. Los 21
archivos se descomprimen sin suplir bytes, con límites válidos de cada registro
y cero bytes sobrantes en la estructura MAT. Ejemplos: `H1.MAT`, 17691 bytes y
un registro; `L1SPR.MAT`, 65212 bytes y 147 registros. Esto verifica estructura,
**no** colores, máscaras, PackBits interno, colocación ni animación renderizados.
El charset sin comprimir se excluye de esa prueba por tener disposición distinta.

Los trece efectos Prehistorik tienen firma IFF 8SVX; todavía no se validan todos
sus chunks, frecuencias ni reproducción. Las canciones PAT requieren identificar
su secuenciador e instrumentos; no equivalen a MOD ni al audio AdLib DOS por
compartir nombres. Los PC1 locales no se deben tratar como ILBM solo por la
extensión. Mapas DAT y colisiones DUR requieren comprobar orden de bytes,
coordenadas, referencias y diferencias respecto de las tablas del ejecutable DOS.
Los ejecutables `main` empiezan por HUNK_HEADER, no por MZ: no son ejecutables
DOS intercambiables. No se ejecutaron sus trainers ni startup-sequence.

Las bases existentes conservan sus licencias y créditos GPL. El script de prueba
indica la procedencia del algoritmo; no se añade código comercial ni arte. No se
ha encontrado en los motores fijados una ruta ya validada que ejecute completos
estos dos juegos Amiga; esto no constituye un inventario exhaustivo de proyectos
externos. El soporte Amiga de Blues es específico de Blues.

## Secuencia de implementación y criterios de salida

TH-14 y TH-15 deben avanzar por hitos comprobables, sin habilitar el botón Jugar
hasta completar la validación del adaptador:

1. Lectores acotados y firmas: descompresión, imágenes, sprites, paletas y máscaras.
   Comparar escenas reales con la edición original; no basta que un parser termine.
2. Un nivel: mapa, colisiones, objetos y capas; identificar correspondencias con
   la lógica DOS antes de reutilizar física o tablas. Documentar las diferencias.
3. Movimiento, cámara, enemigos, entradas/salidas, niveles y final. Comparar
   tiempos y trayectorias con la edición Amiga, manteniendo lógica independiente
   de los fotogramas de presentación.
4. Efectos, instrumentos y secuenciador Amiga, con mezcla estable y verificación
   auditiva. No sustituir silenciosamente por la banda sonora DOS.
5. Adaptador aislado y preferencias por edición; paridad PX con Blues: scroll
   suave, cámara/zoom, menú/controles, volúmenes, ayudas, ocho slots y rebobinado,
   cada capacidad habilitada cuando esté implementada y comprobada.
6. Regresión Linux/Windows, campaña y retorno seguro a biblioteca. No prometer
   intercambio en caliente DOS/Amiga sin demostrar correspondencia de estados.

Fox tiene más incertidumbre de descompresión; Prehistorik ofrece una entrada más
concreta por los MAT. Si falla el primer hito, registrar el bloqueo antes de
extender la lógica. No se estima un porcentaje de port a partir de archivos leídos.
La fase PX sigue vigente; investigar Amiga no la sustituye. La tarea 17 de Blues
sigue abierta y la distribución 18 continúa pospuesta.

## Reproducción y evidencia

Desde la raíz del repositorio, usando las copias locales y el checkout externo de
amitools (revisión usada `521ce0525eb602e40df1a0cc894e76db84ce3ce2`):

```sh
python3 tools/compare_amiga_source.py ../orig-games/fox.adf ../orig-games/fox-amiga --amitools ../tools/amitools
python3 tools/compare_amiga_source.py ../orig-games/Prehistorik.adf ../orig-games/prehistorik2-amiga --amitools ../tools/amitools
python3 tools/audit_amiga_data.py ../orig-games
python3 tests/check_amiga_audit.py
```

Informes de metadatos: [Fox ADF](measurements/th13-fox-adf.json),
[Prehistorik ADF](measurements/th13-pre1-adf.json) y
[formatos y hashes](measurements/th13-formats.json). No incluyen recursos
descomprimidos. Las pruebas sintéticas cubren literales, solapamiento, vuelta de
ventana, ventana inicial vacía, truncamiento, tamaño excesivo y límites MAT.

## Corrección de alcance

Prehistorik 2 tuvo versiones originales DOS y Amstrad CPC, no Amiga; véase el
[registro de ediciones](https://www.mobygames.com/game/525/prehistorik-2/releases/).
Se corrige la hipótesis inicial de TH-16: no está bloqueada por falta de datos,
sino retirada del alcance como no aplicable. No se renumeran las demás tareas.
