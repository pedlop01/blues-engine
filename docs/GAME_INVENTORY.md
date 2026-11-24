# Inventario inicial de juegos para Titus Engine

Inspección de solo lectura: **23 de noviembre de 2026**.
Raíz local: `../orig-games/`, externa al repositorio. Este informe conserva
nombres, tamaños y firmas; no incluye los recursos de los juegos.
Auditoría TH-01 completada: [ENGINE_AUDIT.md](ENGINE_AUDIT.md).
Reglas acordadas: [GAME_DETECTION_RULES.md](GAME_DETECTION_RULES.md).

## Fuentes encontradas

| Carpeta o imagen | Juego/edición candidata | Archivos | Comprobación realizada | Pendiente |
|---|---|---:|---|---|
| `blues-dos/` | Blues Brothers DOS | 50 de juego + 50 metadatos Windows | `MAGASIN.BIN`, `SPRITE.SQV`, `1.EXE` presentes; datos utilizados durante el desarrollo. | Continúa la campaña manual 17. |
| `blues-amiga/` | Blues Brothers Amiga | 58 | Mapas, máscaras, sprites y audio presentes; datos utilizados durante el desarrollo. | Continúa la campaña manual 17. |
| `blues2-dos/` | Jukebox Adventure DOS | 32 | `JARDIN.EAT` de 24876 bytes coincide con la identificación del motor base. | Ruta corta, controles/audio y salida Linux/Windows probados en TH-02; integración pendiente. |
| `prehistorik2-dos/` | Prehistorik 2 DOS y copia de recursos de Prehistorik 1 | 63 | `MOTIF.SQZ` de 9396 bytes coincide con la identificación del motor base; también están los cuatro `FILES*` y `PRE1.EXE`. | Distinguir dos instalaciones lógicas sin duplicar ni mezclar datos. |
| `fox-dos/` | Titus the Fox DOS | 27 | `FOX.COM`, `SPREXP.SQZ` y `LEVEL*.SQZ` presentes; perfil de datos conocido para OpenTitus; revisión comercial exacta no atribuida. | Referencia con audio y controles Linux/Windows probada; integración TH-08 pendiente. |
| `prehistorik-dos/` | Prehistorik 1 DOS | 10 | Presentes los cuatro archivos `FILESA/B.CUR/VGA` y `PRE1.EXE` DIET v1.02–1.20; CRC y descompresión verificados; Olduvai carga siete niveles. | Referencia Linux/Windows y audio probados en TH-02; integrar preparación privada en TH-09. |
| `fox-amiga/` | Titus the Fox Amiga | 89 | Los 89 archivos coinciden byte a byte con `fox.adf`, incluidos arranque/subdirectorios; revisión comercial exacta no atribuida. | Investigar motor Amiga en TH-13. |
| `prehistorik2-amiga/` | Prehistorik 1 Amiga, identificado por contenido | 71 | Título/créditos de Prehistorik 1 y 71 archivos idénticos a `Prehistorik.adf`; conservar el nombre de carpeta actual. | Investigar motor Amiga en TH-13; no identificar como Prehistorik 2. |
| `blues.adf` | Fuente Amiga de Blues Brothers | 901120 bytes | Cabecera `DOS\0`; edición ya disponible extraída en `blues-amiga/`. | Conservar como fuente; evitar registro duplicado por verla también como disco. |
| `Prehistorik.adf` | Fuente de Prehistorik 1 Amiga | 901120 bytes | Cabecera `DOS\0`; 71 archivos coinciden con `prehistorik2-amiga/`. | Registrar la imagen y su extracción como fuentes del mismo perfil. |
| `fox.adf` | Fuente de Titus the Fox Amiga | 901120 bytes | Cabecera `DOS\0`; los 89 archivos de `fox-amiga/` coinciden byte a byte. | Registrar imagen y extracción como fuentes del mismo perfil. |

Los nombres de carpetas son pistas aportadas por la instalación local; los nuevos
juegos no se declaran jugables solo por encontrar esos nombres o una firma de
entrada. La validación de un motor exige cargar recursos y comprobar el juego.

## Casos que debe contemplar el detector

- Comparación de nombres sin distinguir mayúsculas/minúsculas cuando el formato
  lo permita; muchos archivos DOS están en mayúsculas.
- Ignorar `:Zone.Identifier` y otros metadatos/atajos/documentos que acompañan a
  los datos. Se han inventariado sin borrarlos.
- La carpeta de Prehistorik 2 contiene los mismos cuatro archivos `FILES*` y el
  mismo `PRE1.EXE` que `prehistorik-dos/`: sus SHA-256 coinciden. Detectar el
  conjunto de cada título y ofrecer al usuario registrar ambas instalaciones
  lógicas; evitar elegir un juego solo por el primer ejecutable encontrado.
- Preservar el nombre `PRE1.EXE`. Si el motor necesita otro nombre, investigar
  primero identificación por contenido y un adaptador; no renombrar originales.
- El conjunto `prehistorik2-amiga/` se identifica como **Prehistorik 1** por su
  ejecutable y correspondencia byte a byte con el ADF. No registrar Prehistorik 2
  sobre esos datos, ni renombrar la carpeta para que funcione la detección.
- Importar una imagen ADF será una capacidad específica y de solo lectura. Tener
  un ADF no proporciona automáticamente un motor Amiga para ese juego.
- Una imagen y su carpeta extraída pueden representar la misma edición. Su
  identidad debe incluir juego/edición y firma de recursos, además de la ruta.
- Las rutas observadas han cambiado de los ejemplos antiguos `dos/` y `amiga/`
  a `blues-dos/` y `blues-amiga/`. Los futuros scripts y adaptadores deberán
  aceptar rutas configurables en vez de depender del nombre local de carpeta.

## Puntos de recuperación y bases candidatas

- Código previo a la expansión: `dd07383e5d7602db56be49600ae674823326d5e9`.
- Etiqueta: `blues-baseline-2026-11-23`. Es un punto recuperable, no un anuncio de
  versión final: la tarea 17 continúa abierta y la 18 se ha pospuesto.
- Se conserva la procedencia y el historial de [cyxx/blues](https://github.com/cyxx/blues).
- [OpenTitus](https://github.com/stople/OpenTitus/tree/6540e8f2867f99aff54ad1a710ae6379ecb9c590):
  HEAD consultado `6540e8f2867f99aff54ad1a710ae6379ecb9c590`; su documentación
  describe Titus the Fox/Moktar DOS y archivos `SPREXP.SQZ`/`LEVEL*.SQZ`.
- [Olduvai](https://github.com/ksokolowski/olduvai/tree/a69e9204fc311c8810d696a6b799000d1bcd1642):
  HEAD consultado `a69e9204fc311c8810d696a6b799000d1bcd1642`; su documentación
  contempla archivos originales DOS o `PREH.SQZ`, con distinta entrada para cada
  formato. Nuestro conjunto debe verificarse por contenido antes de adaptarlo.

Estos commits fijan las bases elegidas para la integración DOS tras TH-01;
no significan que ya se hayan integrado o validado completamente. No se han
importado sus fuentes en esta revisión.

## Firmas de referencia

SHA-256 de archivos de entrada seleccionados e imágenes. Son una referencia de
este inventario, no una lista exhaustiva de versiones admitidas por el detector.

| Ruta relativa a `orig-games/` | Bytes | SHA-256 |
|---|---:|---|
| `blues-dos/MAGASIN.BIN` | 2560 | `72158766739aa3244bc05267144bf44d56003cdbb549a273f6fdc0d377140d79` |
| `blues-dos/SPRITE.SQV` | 37622 | `8f6222115f2675d4a755df217ba33f8d79faafcf65081ff87dccc8ad510cbb36` |
| `blues-dos/1.EXE` | 196608 | `769ee254b154a9444205524ec4b28a8147a372ac5b2cee2e25fcd83aa90d0add` |
| `blues-amiga/magasin.bin` | 2560 | `72158766739aa3244bc05267144bf44d56003cdbb549a273f6fdc0d377140d79` |
| `blues-amiga/mag.tbl` | 10880 | `bb4f9e7899e361b18a4ae2ea2104aed4ab73f5ba6bbf082997e9ef44045cd3dc` |
| `blues-amiga/sprite` | 35566 | `f6f765140764f973448ee1fd0dadd631efe14d34495fb1f5f230062ccc658a84` |
| `blues2-dos/JARDIN.EAT` | 24876 | `bbb5c6b7bb8bcb543cfbfe539f4eae8d03d72fcafa88345a784b63d3d528b344` |
| `prehistorik2-dos/MOTIF.SQZ` | 9396 | `fa3a9785e9fa1288e71e4edf91e45db3dd0b0eacac968062d5c5edba0d2ab8d1` |
| `prehistorik-dos/PRE1.EXE` | 77839 | `b03ce6913d07a4c5b66dfc4843d185ffdff6561e9bf91f3333b096016690406f` |
| `prehistorik-dos/FILESA.CUR` | 54304 | `eeb2e85ab2440ef91d54c808bd93fa6e6962b48d422beed1c06e32d684b962ec` |
| `prehistorik-dos/FILESB.CUR` | 61207 | `088806e36c4f836862989c5679a68c98b922cdac31c923d4e189cce37dcff098` |
| `prehistorik-dos/FILESA.VGA` | 208705 | `ec0b7a025ff1ef2391afc3675d3a4079f93b9df780d2bdf9715a1d2d5987b0fa` |
| `prehistorik-dos/FILESB.VGA` | 295166 | `6bfebd59c28e01878ce7575652a5e2a1f9fc0a9b6d3cf68039b4070ecb463d1a` |
| `fox-dos/FOX.COM` | 44389 | `a47ab6e4f8016db90e727df2eef5450edf1e16ecb6c90f5cb31ad214fef65f7b` |
| `fox-dos/SPREXP.SQZ` | 115339 | `546d4f6943e10d47ba1831547f778d4193627919020d5247a4129aca3769ce07` |
| `fox-dos/LEVEL0.SQZ` | 23664 | `b4481353cab30c60e64eaa858a778e194cfbc7b917cf3977088a910a1474356f` |
| `fox-amiga/main` | 51048 | `18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45` |
| `fox-amiga/LEVEL01.TBL` | 3578 | `b2b3be320e6b111e36b4c5ff29c7ee9a5fc78ff282f3cd0d228f83c591d2fb29` |
| `prehistorik2-amiga/main` | 117760 | `d48f952eca0d74547d9e99531b54ac05beaabb42a55203fec510279bd9bd5295` |
| `prehistorik2-amiga/H1.MAT` | 15511 | `215b34300c70201fc2aeeba3d1bbdf8369b739cfcfb7135c77fdbbc1be0332b8` |
| `Prehistorik.adf` | 901120 | `20dfe5534106ab4bd14eca3077cd3cbb6a3ca2453dd9b6139dac3f9f60a1c3a2` |
| `blues.adf` | 901120 | `2fba6446987049f69b0d786176fc230d76f080360abf9258a030b0298e81c1ea` |
| `fox.adf` | 901120 | `b90d055d9c710ac1aae0043db037387e233a555a1cefdfcb30c6bb29106f2aaa` |

La preparación DIET genera temporalmente un MZ de **164166 bytes**, SHA-256
`5cea262f9610657af8b55c10b277ec30ffe99df35294d5f8c9f520a30e029c96`.
No se incluye en Git ni se sustituye `PRE1.EXE`. `TITRE1.PC1` de Prehistorik
Amiga coincide con el ADF: SHA-256
`0d7c742c16af80ba3f9bd6550291018e136fb42ea61e27113706969ffed1683d`.

## Investigación Amiga posterior (TH-13)

Resultado y límites actualizados en [AMIGA_FEASIBILITY.md](AMIGA_FEASIBILITY.md):
comparación ADF reproducida y reutilización LZSS/MAT de Prehistorik demostrada.
No habilita todavía nuevos juegos Amiga.
