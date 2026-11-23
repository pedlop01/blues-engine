# TH-08: Titus the Fox DOS

La biblioteca incorpora **Titus the Fox DOS**, mediante un proceso OpenTitus
separado del frontend y del runtime Blues/Jukebox/P2. Fox Amiga y Moktar no se
habilitan: la entrada admite únicamente el conjunto DOS auditado del catálogo.

## Jugar y controles

Abrir `build/launcher-windows/titus-engine.exe` o `build/launcher/titus-engine`,
seleccionar **Titus the Fox → DOS → Jugar**. La primera apertura prepara sus datos;
las posteriores verifican y reutilizan la copia privada.

| Entrada | Acción nativa |
|---|---|
| Izquierda/derecha | Mover |
| Arriba | Saltar / subir |
| Abajo | Agacharse / bajar |
| Espacio | Acción y elección en menús |
| P | Pausar/reanudar, también el audio |
| Esc o cerrar la ventana | Salir a la biblioteca |
| F3 | Alternar audio nativo |

Se conservan los niveles, contraseñas y pantallas originales implementados por
OpenTitus. El arranque usa logo/título de Fox; omite los dos textos de broma de
OpenTitus dedicados a Moktar. La resolución lógica sigue siendo 320×200. El SDK
SDL 1.2 compatible amplía la ventana 2× con píxeles enteros; no se amplía el lienzo
del juego, lo que dejaría zonas negras. El modo de desarrollo está desactivado.

Se anuncian retorno a biblioteca, pausa nativa y pantalla completa. No se anuncian
slots, miniaturas, rebobinado, ayudas, cámara/zoom, mando propio del adaptador ni
volúmenes separados: el OPL original mezcla música y efectos. Los mandos físicos
y su unificación corresponden a TH-10/12; esta entrega valida teclado.

## Preparación nativa y almacenamiento

```text
data/profiles/<uuid>/games/fox/fox.full.audited/native/opentitus/
  preferences.json
  prepared/<fingerprint>.fox-lzw-v1/
    manifest.json
    music.bin
    FONTS.SQZ, SPREXP.SQZ, LEVEL*.SQZ, MENU.SQZ, TITRE.SQZ, TITUS.SQZ
    titus.conf
```

Los originales son de solo lectura. Primero se ejecuta `--inspect-request`, sin
inicializar vídeo/audio ni preparar archivos. Devuelve `preparation_required` y
`fox-lzw-v1` si falta una copia válida. El supervisor ejecuta un job distinto
`--prepare-request`, que consume el mismo formato de petición de inspección,
y después **repite la inspección** antes de lanzar. Cada job tiene UUID y logs
propios; no se declara `ready` por completar la preparación.

`platform/fox_prepare.cpp` implementa LZW MSB de 9–12 bits con diccionario acotado,
códigos de reinicio/fin, límite de salida e iteraciones. Acepta solo la firma
conocida de `FOX.COM`. No ejecuta el COM/EXE, no emula instrucciones y no depende
de Python, Unicorn ni DOSBox durante el juego. Contraste con la referencia TH-02:

| Resultado | Tamaño | SHA-256 |
|---|---:|---|
| MZ reconstruido en memoria | 63 985 | `f398540b9bd65ef64fcd7898285cafb30f95e456d8b3d7a5123d2337c0c0df41` |
| Audio extraído | 18 749 | `386dbe1cad7a67e20ec3edb98bdb116c035e68cc762cd85762d2bedbb1215986` |

Solo se conserva el audio necesario y una copia verificada de los SQZ; el COM y
el MZ no se copian al workspace. La copia se prepara en un directorio temporal,
se valida íntegramente y se publica por renombrado. Cada reutilización comprueba
el manifiesto y todos los archivos contra las firmas auditadas. Una caché dañada
se conserva con sufijo `.invalid.<uuid>` y se reconstruye; directorios temporales
incompletos no se reutilizan. Se rechazan alias/enlaces de la caché que pudieran
redirigir escrituras fuera de su ubicación privada.

El motor recibe nombres relativos cortos; el puente de I/O los resuelve mediante
lecturas de su copia privada. En Windows usa `_wfopen` y rutas UTF-16 extendidas,
sin depender del límite de longitud del directorio de trabajo del código antiguo.
No hay renombrado de originales, enlaces privilegiados ni rutas con espacios
introducidas en el parser antiguo de `titus.conf`.

`preferences.json` contiene `{"schema_version":1,"fullscreen":false}`. Puede
editarse con el juego cerrado; el formulario global llegará en TH-10. Una versión
futura se rechaza sin sobrescribirla. No se importan preferencias ni saves de
Blues. La inspección devuelve índice de slots `null` y contador cero.

El supervisor conserva el bloqueo heredado de perfil durante inspección,
preparación y partida. Cancelar el inicio espera a que termine el job acotado en
curso y evita abrir el juego. La biblioteca se oculta solo después de confirmar
menú cargado y dispositivo de audio activo; el retorno requiere resultado y salida
real del proceso. También funciona durante la pausa. Los logs se mantienen
separados del protocolo y rotan al superar 1 MiB en los puntos de consulta.

## Fuentes, licencias y compilación

OpenTitus y OPL se compilan desde las revisiones de TH-02, sin los archivos
comerciales de `bin/` ni bibliotecas OPL precompiladas. Se conservan los ajustes de
anchura de tipos y límites del [parche de referencia](../patches/opentitus-reference.patch).
El [parche gestionado](../patches/opentitus-managed.patch) añade puntos de consulta,
confirmación de menú, observación de controles y pausa/audio.

El bootstrap/puente y parche son GPL-3.0-or-later; OpenTitus es GPL-3.0-or-later y
Chocolate OPL/DBOPL GPL-2.0-or-later. Véanse [NOTICE.md](../engines/opentitus/NOTICE.md)
y [CREDITS.md](../CREDITS.md). El frontend no enlaza estos motores. El build copia
avisos y textos completos de GPL al directorio del adaptador. Para distribuir
binarios en 18 habrá que acompañar las fuentes correspondientes y avisos; este
build local no es el paquete final de distribución.

```sh
python3 tools/prepare_fox_sources.py \
  --opentitus /ruta/OpenTitus --chocolate /ruta/chocolate-doom \
  --output build/fox-sources

# Sobre la configuración de la biblioteca de TH-05/07:
cmake -S . -B build/launcher -DTITUS_BUILD_FOX_RUNTIME=ON \
  -DTITUS_FOX_SOURCES="$PWD/build/fox-sources" \
  -DTITUS_FOX_SDL_ROOT=/ruta/sdl12-sdk \
  -DTITUS_FOX_MIXER_ROOT=/ruta/mixer12-sdk
cmake --build build/launcher --parallel 2
```

La herramienta de preparación de **fuentes**, solo para compilar, necesita Python
3 y `patch`. Comprueba las revisiones y exporta únicamente C/headers/licencias.
La preparación de **datos del usuario** es C++ nativo dentro del runtime.

SDK local Linux: `/tmp/titus-engine-audit/sdl12-root/usr`. En Windows x64 desde
WSL usar gcc-posix/g++-posix y los SDK `sdl12-windows-sdk` y `mixer12-windows-sdk`
documentados en [ENGINE_REFERENCE_TESTS.md](ENGINE_REFERENCE_TESTS.md), además del
SDK SDL2 de la biblioteca. El runtime mantiene C GNU89, `-fcommon` y
`-fsigned-char`; OPL se compila como C99. Linux usa RPATH para las dependencias
transitivas de su SDK. La opción `TITUS_BUILD_FOX_RUNTIME` es independiente de la
de la familia Blues. El core sigue compilando sin SDL.

En Windows conservar juntos `engines/opentitus/opentitus-runtime.exe`, `SDL.dll`,
`SDL_mixer.dll`, `SDL2.dll`, `NOTICE.md` y `licenses/`, además de la biblioteca y
su registro `engines/registry.json`. No se necesitan Python ni un intérprete de
órdenes en el equipo donde se juega.

## Validación

```sh
ctest --test-dir build/launcher --output-on-failure
python3 tests/check_fox_sessions.py --bin-dir build/launcher --orig-games ../orig-games
python3 tests/check_launcher_sessions.py --bin-dir build/launcher --orig-games ../orig-games --fox
# Ejecutar los PE reales desde WSL, sin Wine:
python3 tests/check_fox_sessions.py --bin-dir build/launcher-windows --orig-games ../orig-games --windows
python3 tests/check_launcher_sessions.py --bin-dir build/launcher-windows --orig-games ../orig-games --windows --fox
```

Las rutas Linux/Windows verifican inspección sin SDL, fallo de preparación sin
publicar, hashes exactos, reparación de caché, preferencias futuras conservadas,
146 frames de juego, movimiento a ambos lados, salto, acción, pausa/audio y
retorno. El PCM es no silencioso y sin muestras saturadas en esta ruta. Se
inspeccionan capturas del juego; la GUI recorre Blues DOS → Jukebox → P2 → Fox →
Blues Amiga, restaurando ventana/selección cinco veces. Se conservan las regresiones
de la familia, contratos y core, y tamaño/fecha/SHA-256 de los 462 originales.
Campaña completa, escucha humana y mandos físicos siguen en TH-12/17.
