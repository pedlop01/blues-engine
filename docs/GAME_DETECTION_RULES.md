# Reglas de identificación acordadas en TH-01

Fecha: **23 de noviembre de 2026**. Especificación para TH-04, no un catálogo
persistente implementado. Firmas de referencia en [GAME_INVENTORY.md](GAME_INVENTORY.md)
y resultados reproducibles en [audit/th01-results.json](audit/th01-results.json).

## Reconocer datos y ofrecer un motor son decisiones distintas

El detector devolverá todos los conjuntos reconocidos de la carpeta, con
`game_id`, `edition_id`, variante, firma de recursos y estado de compatibilidad.
No escogerá el primer ejecutable encontrado. La interfaz distinguirá:

- **Reconocido:** coincide con un perfil de datos conocido.
- **Incompleto:** pertenece a una familia conocida pero faltan recursos concretos.
- **Variante sin verificar:** familia plausible, firma desconocida; no seleccionar
  un layout por defecto ni presentarla como compatible.
- **Necesita preparación:** perfil reconocido cuyo motor requiere transformación
  de recursos en caché, por ejemplo Prehistorik 1 DIET.
- **Motor pendiente:** datos identificados sin adaptador ejecutable, como Fox y
  Prehistorik Amiga en esta primera etapa.

Solo un adaptador validado podrá anunciar que se puede jugar; identificar datos
o ejecutar unos fotogramas no cierra la validación TH-02/TH-12.

## Perfiles y recursos

Los nombres siguientes se comparan sin distinguir mayúsculas/minúsculas. Los
marcadores son entradas de identificación, **no una comprobación suficiente de
integridad**: verificar también firmas conocidas y recursos del modo solicitado.

| `game_id` / edición | Marcadores del perfil disponible | Validación adicional / decisión |
|---|---|---|
| `blues` / DOS | `MAGASIN.BIN` y `SPRITE.SQV`, con sus firmas inventariadas | Revisar conjuntos por nivel según `_levels` de `bb/level.c`: BIN, CK1/CK2, SQL, ENEMI y AVT cuando corresponda. `1.EXE` reconocido habilita audio DOS; sin él no prometer ese audio. |
| `blues` / Amiga | `mag.tbl`, `sprite`, `objet`; comprobar firmas, no solo `MAGASIN.BIN`, compartido con DOS | Usar conjuntos `_levels_amiga` de `bb/level.c`: BLK, TBL, BIN, máscaras y enemigos. Conservar la dependencia DOS para Concierto cuando se use el fallback actual. `sound`, módulos e instrumentos determinan capacidades de audio por separado. |
| `jukebox` / DOS | `JARDIN.EAT` (24876 bytes y SHA-256 conocido), `FONT256.EAT`, `SPRITES.EAT` | Comunes: BOARD, MOTIF, SAMPLES; pantallas TITUS, TINY, SELECT, FOND, PASSWORD, MENU, WIN, END; fondos BACK0–3, mapas JARDIN/PRISON/ENTREPOT/EGOUT y DIF01–04, todos EAT. Música: ALMOST/EVERY/BARTENDE/PETERGUN/SHOOT.MOD. Ejecutable DOS innecesario para este motor. |
| `prehistorik2` / DOS | `MOTIF.SQZ` (9396 bytes y SHA-256 conocido), `MAP.SQZ`, `SPRITES.SQZ` | Comunes MAP/MOTIF/ALLFONTS/SPRITES/FRONT/UNION/KEYB y SAMPLE en edición completa; pantallas y BACK0–5/LEVEL1–9/LEVELA–G según `p2/game.c` y `p2/level.c`. `JOYSTICK.SQZ` activa la rama demo: no confundirla con la edición completa verificada. |
| `fox` / DOS | `SPREXP.SQZ`, `LEVEL0.SQZ` con firmas inventariadas; `FOX.COM` como referencia de variante | OpenTitus usa los 15 niveles declarados por `bin/titus.conf`, FONTS, TITUS, TITRE, MENU y LEVELA.SQZ. `FOX.COM` no es requisito de su cargador gráfico. El audio necesita adaptación: espera `music.bin`, ausente de nuestra carpeta; el COM disponible declara un empaquetador propio de Eric Zmiro. No prometer extracción por el offset de un FOX.EXE distinto. |
| `prehistorik` / DOS, DIET | `PRE1.EXE` y los cuatro `FILESA/B.CUR/VGA`, **todos con las firmas verificadas** | Preparación descrita debajo. El mismo conjunto existe dentro de `prehistorik2-dos`; identificarlo independientemente. |
| `prehistorik` / Amiga | `main`, `H1.MAT`, `TITRE1.PC1` con firmas inventariadas; familia L1–7SPR.MAT/LEVEL*.DUR | Identificado como **Prehistorik 1**, aunque la carpeta se llame `prehistorik2-amiga`. Sus 71 archivos coinciden con `Prehistorik.adf`. Motor Amiga pendiente; no registrarlo como Prehistorik 2. |
| `fox` / Amiga | `main`, `LEVEL01.TBL`, familia BRIK*.BIN/POIL*.BIN | Los 89 archivos coinciden con `fox.adf`. Identidad del conjunto fijada por firmas; no se atribuye un número de revisión comercial que no se ha comprobado. Motor Amiga pendiente. |

Esta auditoría reconoce los perfiles suministrados. No da por compatibles todas
las demos, idiomas, reediciones o modificaciones con los mismos nombres.
Olduvai contempla además `HISTORIK.EXE` y `PREH.SQZ`; se documentan como familias
admitidas por el motor original, pendientes de datos propios y prueba local.
Lo mismo se aplica a Moktar, fuera del catálogo inicial y sin datos aportados.

## Preparación definida para Prehistorik 1 DOS

1. Resolver nombres sin distinguir mayúsculas/minúsculas y rechazar coincidencias
   ambiguas. Verificar el EXE y los cuatro FILES contra el perfil conocido.
2. Detectar el MZ comprimido con DIET de la familia **v1.02–1.20**. La identificación
   no permite inferir una versión exacta del compresor o de la distribución.
3. Descomprimir en caché privada, nunca sobre los originales. Deark en la revisión
   auditada produce una imagen MZ de **164166 bytes** y confirma CRC **`2f77`**.
   Su SHA-256 es `5cea262f9610657af8b55c10b277ec30ffe99df35294d5f8c9f520a30e029c96`.
4. Proporcionar esa imagen a Olduvai bajo su entrada `HISTORIK.EXE`, mediante un
   adaptador de carga o una carpeta privada de ejecución. La receta de auditoría
   usa enlaces temporales a los FILES; la implementación multiplataforma se hará
   en TH-09 y deberá funcionar con originales de solo lectura.
5. Verificar resultado/diagnóstico de carga. Olduvai devuelve 0 incluso cuando
   falla la carga de tablas; el código de salida por sí solo no es suficiente.

El MZ descomprimido usa cabecera de 8192 bytes y layout canónico (`ds_delta=0`).
Su FNV-1a64 es `3820d2a9e2b4e4f6`, **distinto** de las dos identidades incluidas
en `kKnownBuilds` de Olduvai. Por ello, el adaptador reconocerá nuestra firma
explícitamente; no generalizará el fallback del motor a cualquier EXE desconocido.
Los siete niveles cargaron 30 fotogramas y produjeron capturas. Falta validar
recorridos, física y audio en TH-02: esto demuestra una vía de adaptación, no
equivalencia completa con otro ejecutable.

## Carpetas mixtas, imágenes y duplicados

- `prehistorik2-dos` produce **dos resultados**: Prehistorik 2 y Prehistorik 1
  DIET. El segundo coincide con el conjunto de `prehistorik-dos`. No mover,
  borrar o separar sus archivos para resolverlo.
- Las rutas son ubicaciones, no identidad suficiente. Mantener firmas por
  juego/edición/variante y conservar ubicaciones alternativas de datos idénticos.
- `Prehistorik.adf` y `prehistorik2-amiga` representan el mismo conjunto de
  Prehistorik 1; `fox.adf` y `fox-amiga` también coinciden. Mostrar una fuente y
  su extracción relacionadas; no ofrecer dos juegos distintos por ese motivo.
- Los ADF son fuentes, no instalaciones ejecutables por un motor DOS. Su lectura
  y eventual extracción será explícita y de solo lectura sobre la imagen.
- Ignorar `:Zone.Identifier`, documentos y herramientas de arranque como pruebas
  de identidad. Si hay nombres que solo difieren en mayúsculas y contienen datos
  diferentes, mostrar ambigüedad en lugar de escoger uno silenciosamente.
- Los 71/89 archivos incluyen subdirectorios de arranque. La comparación ADF es
  byte a byte, además de las firmas de marcadores; no se identificó el juego por
  el nombre de la carpeta ni por la etiqueta modificada del volumen.

## Bases elegidas y límites de distribución

| Familia | Base fijada para la siguiente etapa | Decisión |
|---|---|---|
| Blues, Jukebox y Prehistorik 2 | Nuestra extensión de cyxx/blues | Conservar módulos y mejoras de Blues; ampliar el build Windows después de la validación. |
| Fox DOS | OpenTitus `6540e8f2867f99aff54ad1a710ae6379ecb9c590` | Base de TH-08, proceso separado; corregir portabilidad y compilar OPL desde fuentes. Audio y partida pendientes de TH-02. |
| Prehistorik 1 DOS | Olduvai `a69e9204fc311c8810d696a6b799000d1bcd1642` | Base de TH-09, proceso separado y preparación DIET conocida. |
| Fox/Prehistorik Amiga | Ninguna base ejecutable validada | Datos identificados, investigación TH-13 antes de habilitar estas ediciones. |

El alcance de TH-01 es identificar y registrar licencias/obstáculos, no cambiar
la licencia heredada ni declarar publicable un paquete. OpenTitus es GPL-3.0-or-later;
su OPL/Chocolate Doom declara GPL-2.0-or-later. Olduvai tiene fuentes propias
GPL-3.0-or-later, pero xBRZ es GPL-3.0-only y su aviso indica distribución del
binario combinado bajo GPL-3.0. Conservar sus avisos de SDL2, FluidSynth, gcem,
munt, RtMidi, stb y fuentes tipográficas, según `THIRD-PARTY-NOTICES.md`.

Deark es una herramienta externa de auditoría (MIT-style; módulo DIET y sus
dependencias por revisar si se incorporan). Amitools se usa externamente para
leer ADF y declara GPL-2.0-or-later. Ninguno se ha incorporado al runtime.
Tampoco se importaron los `music.bin`/`music_mok.bin` ni las DLL que OpenTitus
incluye en `bin/`; el audio del producto se obtendrá de datos del usuario.

TH-03 debe conservar esta separación y resolver la estrategia de licencia del
código heredado antes de combinar motores; la tarea 18 revisará fuentes, avisos y
componentes realmente incluidos al preparar la distribución. La falta de licencia
general en el snapshot de cyxx/blues sigue registrada, sin asignarle otra.
