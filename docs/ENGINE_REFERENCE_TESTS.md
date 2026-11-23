# TH-02: motores de referencia comprobados

Fecha: **23 de noviembre de 2026**. **TH-02 completada**: compilaciones separadas,
arranque, ruta corta de controles, generación de audio y salida acotada de
Jukebox Adventure, Prehistorik 2, OpenTitus y Olduvai con nuestros datos.
Resultados estructurados: [audit/th02-results.json](audit/th02-results.json).

## Alcance y resultados

Linux es **AArch64**. Los ejecutables **Windows x64 se ejecutaron en Windows
ARM64 mediante su compatibilidad x64**, usando la interoperabilidad WSL; no se
presentan como pruebas sobre una CPU Windows x86-64 física. Se usaron controladores
SDL `dummy`, con capturas de imagen y PCM. Hay sonido sintetizado verificable,
pero **no escucha humana, prueba de altavoces ni validación de latencia real**.

| Juego / motor | Linux y Windows | Controles / referencia | Audio |
|---|---|---|---|
| Jukebox Adventure / módulo `ja` | Compila, entra en partida y cierra | 140 pasos: derecha +114 px, regreso 80 px, salto 68 px; la acción provoca un efecto | PCM no silencioso; sin muestras saturadas en esta ruta |
| Prehistorik 2 / módulo `p2` | Compila, entra en partida y cierra | 140 pasos: derecha +135 px, regreso 70 px, salto 60 px; tres efectos durante la acción | PCM no silencioso; saturación breve registrada, véase limitaciones |
| Fox / OpenTitus | Compila con OPL desde fuentes y los ajustes adjuntos; entra en nivel 1 y cierra | 140 actualizaciones: derecha +110 px, regreso 64 px, salto 50 px; se introduce Space para la acción | Audio extraído del COM del usuario, PCM no silencioso y sin saturación en esta ruta |
| Prehistorik 1 / Olduvai | Compila y carga el MZ preparado desde DIET | Ruta de 140 fotogramas: derecha +148 px, regreso 102 px, salto 31 px y garrote activo. Estados idénticos Linux/Windows | Backend AdLib/OPL; captura del mixer en ambas plataformas. Los tres efectos del catálogo producen WAV idénticos byte a byte en ambas plataformas |

En Fox no se ha probado recoger y lanzar un objeto concreto: introducir Space
sin un objeto al alcance no demuestra esa interacción. Las rutas cortas tampoco
demuestran completar niveles, todos los enemigos, transiciones o finales.
Estas pruebas fijan una **base técnica de referencia**, no cierran la campaña
manual de Blues 17 ni la validación de la futura plataforma TH-12.

Además:

- Blues DOS/Amiga, Jukebox y Prehistorik 2 pasan arranque/contenido visible/salida
  en Linux y Windows. Jukebox también arranca con alias de datos en minúsculas.
  Blues DOS pasa además una ruta de 140 pasos con audio DOS en el mismo build
  Windows multijuego, comprobando que su estado también permanece aislado.
- Los siete niveles de Prehistorik 1 pasan carga y captura al fotograma 30.
  Se corrigió la receta: Olduvai termina al guardar la captura, por lo que
  `--play-shot-frame` debe coincidir con el límite deseado; su valor inicial 1
  interrumpía antes el ensayo aunque se pidieran 30 fotogramas.
- AddressSanitizer pasa las rutas de Jukebox, Prehistorik 2 y Fox en Linux,
  con comprobación de fugas desactivada. No se afirma ausencia de fugas ni
  validación exhaustiva de todos los recorridos.
- Se revisaron capturas de los cuatro juegos. WAV, BMP/PNG, PCM, fuentes externas
  y binarios permanecen en `build/references/` o el área temporal de auditoría,
  fuera de Git. No se han modificado los originales.

## Cambios que hizo falta realizar

### Windows multijuego

`Makefile.win` admite `MULTIGAME=1` y utiliza por defecto
`build/windows-x64-multigame/`, separado de `build/windows-x64/` para Blues solo.
El ejecutable sigue llamándose `blues.exe` hasta decidir la transición TH-03.
Selecciona el juego por sus datos; todavía no tiene biblioteca global.

COFF/MinGW no tiene la visibilidad oculta de ELF. Se sustituyó la localización
de símbolos por un namespace por motor, incluyendo los símbolos y secciones
COMDAT `.refptr` privados; las referencias compartidas a sistema/mixer/opciones
se conservan. Las pruebas Windows de Jukebox/P2 enlazan **los tres módulos juntos**,
de modo que verifican también que no compartan accidentalmente `g_vars`/`g_res`.

### Jukebox

AddressSanitizer encontró una lectura después del último registro de la tabla de
animación de tiles en `ja/level.c`. El bucle comprobaba el contador después de
leer. Ahora comprueba el contador primero, conservando los registros válidos y
el resultado del recorrido.

### OpenTitus y audio de Fox

[Parche separado](../patches/opentitus-reference.patch), sobre
`6540e8f2867f99aff54ad1a710ae6379ecb9c590`: tipos de 32 bits explícitos,
llamada de inicialización de audio protegida y límites de tablas/instrumentos
corregidos. Se compila el C antiguo con `-std=gnu89 -fcommon -fsigned-char`.
No se incorporaron las fuentes completas al motor de Blues.

El OPL se recompila desde Chocolate Doom **1.6.0**, commit
`822664b4ff873d462370e9e96a9d91e6066c221d`, con el DBOPL de OpenTitus que incluye
su ajuste de percusión. Se usa únicamente el controlador SDL de software:
no hay acceso a puertos OPL físicos ni instalación de controladores de Windows.

`FOX.COM` tiene un desempaquetador de Eric Zmiro. La herramienta
`tools/extract_fox_audio.py` acepta solo la firma conocida y emula únicamente ese
desempaquetador, con límite de instrucciones/tiempo y sin servicios DOS/BIOS.
Se detiene antes de reubicar/ejecutar el juego. Verifica el MZ reconstruido
(63985 bytes, SHA-256 `f398540b9bd65ef64fcd7898285cafb30f95e456d8b3d7a5123d2337c0c0df41`)
y extrae 18749 bytes en el offset 28677. El audio obtenido tiene SHA-256
`386dbe1cad7a67e20ec3edb98bdb116c035e68cc762cd85762d2bedbb1215986`.
No se ha copiado el `music.bin` comercial del repositorio de OpenTitus.

Unicorn **2.1.4** se utiliza como herramienta de preparación de referencia;
no se ha convertido en dependencia del runtime. TH-08 decidirá su preparación
definitiva y distribución.

### Olduvai

Revisión `a69e9204fc311c8810d696a6b799000d1bcd1642`, con
[parche MinGW](../patches/olduvai-mingw-loader.patch): usar el cargador Win32
también con MinGW, evitando la dependencia adicional `dlfcn-win32`.
Para C++17 se usa **`x86_64-w64-mingw32-g++-posix`**, ya que la variante win32 de
este compilador no aportaba las primitivas de `std::thread` requeridas.
El MZ preparado de Prehistorik 1 sigue siendo el identificado en TH-01.

## Repetir las pruebas del proyecto base

Datos externos bajo `../orig-games`, o sustituir por las instalaciones propias:

```sh
bash tests/check_engine_startup.sh ../orig-games
make build/tests/check_gameplay_ja build/tests/check_gameplay_p2
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/tests/check_gameplay_ja ../orig-games/blues2-dos
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/tests/check_gameplay_p2 ../orig-games/prehistorik2-dos
```

`TITUS_PROBE_PCM` permite escribir PCM s16le mono/44100 Hz y `TITUS_PROBE_SHOT`
una captura BMP. El callback real del mixer genera el audio. Se usan eventos
de teclado SDL; las comprobaciones observan el desplazamiento, salto y efectos,
sin escribir las coordenadas del jugador. Se omiten los fundidos para acortar
el recorrido. Las pruebas no leen/escriben las preferencias del usuario.

Windows (rutas SDK de esta auditoría como ejemplo):

```sh
make -f Makefile.win MULTIGAME=1 \
  SDL2_ROOT=/tmp/blues-win-deps/sdl/x86_64-w64-mingw32 \
  MODPLUG_ROOT=/tmp/blues-win-deps/modplug-install all \
  build/windows-x64-multigame/check_engine_startup.exe \
  build/windows-x64-multigame/check_gameplay_ja.exe \
  build/windows-x64-multigame/check_gameplay_p2.exe
python3 tools/run_windows_probe.py \
  --program build/windows-x64-multigame/check_gameplay_ja.exe \
  --output-dir build/references/windows -- "$(realpath ../orig-games/blues2-dos)"
```

Repetir para `check_gameplay_p2.exe` con los datos de Prehistorik 2. La utilidad
WSL traduce las rutas, fija SDL dummy y utiliza preferencias privadas en `build/`.
Es una herramienta de desarrollo, no la implementación futura del lanzador.

## Repetir Fox desde fuentes externas

Con checkouts externos fijados a las revisiones anteriores y SDK SDL 1.2 + mixer:

```sh
python3 tools/build_fox_reference.py \
  --opentitus /tmp/titus-engine-audit/opentitus \
  --chocolate /tmp/titus-engine-audit/chocolate-doom \
  --sdl-prefix /tmp/titus-engine-audit/sdl12-root/usr \
  --output build/references/fox-native --asan
```

La herramienta exporta las revisiones, aplica el parche en copias privadas,
recompila OPL y genera `opentitus-reference` y `fox-gameplay`; no usa archivos
modificados del checkout externo ni sus bibliotecas OPL precompiladas. Para Windows:

```sh
python3 tools/build_fox_reference.py \
  --opentitus /tmp/titus-engine-audit/opentitus \
  --chocolate /tmp/titus-engine-audit/chocolate-doom \
  --sdl-prefix /tmp/titus-engine-audit/sdl12-windows-sdk \
  --mixer-prefix /tmp/titus-engine-audit/mixer12-windows-sdk \
  --cc x86_64-w64-mingw32-gcc --windows \
  --sdl2-dll /tmp/blues-win-deps/sdl/x86_64-w64-mingw32/bin/SDL2.dll \
  --output build/references/fox-windows
```

SDK Windows utilizado: sdl12-compat `release-1.2.68`, commit
`35ef746cf1a4fde8c442dc92fe9b18c3f90d7855`, compilado con CMake, SDL2 headers
y `CMAKE_SYSTEM_NAME=Windows`. SDL_mixer **1.2.12**, tarball oficial SHA-256
`1644308279a975799049e4826af2cfc787cad2abb11aa14562e402521f86992a`:
actualizar `build-scripts/config.guess`/`config.sub` para reconocer AArch64;
configurar `--host=x86_64-w64-mingw32 --disable-sdltest --disable-static`,
con `SDL_CONFIG` del SDK y los backends `music-cmd/wave/mod/midi/ogg/flac/mp3`
desactivados. OPL aporta su propio callback; no necesita esos decodificadores.
`--disable-sdltest` evita ejecutar sin sus DLL el conftest cruzado desde WSL.

En un entorno Python separado, instalar `unicorn==2.1.4` y ejecutar:

```sh
python3 tools/extract_fox_audio.py ../orig-games/fox-dos/FOX.COM build/references/fox-native/music.bin
```

Generar `titus.conf` privado partiendo del formato de `bin/titus.conf` de OpenTitus,
con las rutas absolutas de los SQZ propios (rutas Windows para ese binario).
El archivo y `music.bin` deben estar en el directorio de ejecución. No copiar los
datos comerciales de `bin/` del upstream. `fox-gameplay` omite presentación/estado
de nivel, introduce estados de teclas en la API SDL 1.2, observa coordenadas y
captura el callback OPL. `TITUS_PROBE_PCM` aquí produce s16le **estéreo**/44100 Hz;
`TITUS_PROBE_SHOT` produce BMP. En el build ASan usar `ASAN_OPTIONS=detect_leaks=0`.

## Repetir Olduvai

Usar el build Linux descrito en [ENGINE_AUDIT.md](ENGINE_AUDIT.md). Para Windows,
aplicar el parche a una copia de la revisión fijada y configurar CMake con:

```sh
cmake -S /tmp/titus-engine-audit/olduvai -B /tmp/titus-engine-audit/olduvai-win-posix \
  -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc-posix \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres -DCMAKE_BUILD_TYPE=Release \
  -DSDL2_DIR=/tmp/blues-win-deps/sdl/x86_64-w64-mingw32/lib/cmake/SDL2 -DOLDUVAI_LTO=OFF
cmake --build /tmp/titus-engine-audit/olduvai-win-posix --target olduvai -j4
```

Preparar el MZ y FILES en una carpeta privada según TH-01. El replay propio es
`tests/reference_walk_jump.jsonl`. Arrancar cada binario con:

```text
--game-dir CARPETA_PRIVADA --play --level 1 --display-mode cpu --sound-card adlib --audio-rate 44100
--replay RUTA_REPLAY --trace RUTA_TRACE --play-frames 140
--play-shot RUTA_BMP --play-shot-frame 140
```

Cada ejecución tendrá su XDG_CONFIG_HOME privado. Comparar las trazas con:

```sh
python3 tests/check_reference_trace.py build/references/native/prehistorik-trace.jsonl build/references/windows/prehistorik-trace.jsonl
```

Para capturar el audio real, usar la función ya incluida en ese motor:
`OLDUVAI_AUDIO_CAPTURE=RUTA_WAV`. Escribe el WAV con su formato y frecuencia
correctos al cerrar el dispositivo. En Windows se puede proporcionar mediante
`--env OLDUVAI_AUDIO_CAPTURE=RUTA_LINUX_ABSOLUTA` a `run_windows_probe.py`.

`--render-sfx all --render-audio-out PREFIJO.wav` genera los tres efectos;
comparar cabeceras y PCM en ambas plataformas. Su igualdad es una prueba de
determinismo del motor de referencia, no una comparación con un DOS emulado.

## Límites y seguimiento

- **TH-03:** contrato del lanzador y capacidades reales; preservar motores como
  procesos separados, sus licencias y los datos originales de solo lectura.
- **TH-07:** integrar Jukebox/P2, preferencias independientes y revisar el margen
  de mezcla de P2. La ruta registró 317 muestras saturadas de 204800 en Linux y
  530 de 217088 en Windows. Es un hallazgo de la referencia, no audio perfecto.
  No se cambió el volumen de Blues para ocultarlo.
- **TH-08/09:** convertir estas preparaciones/builds de Fox/Prehistorik en
  adaptadores mantenibles, sin exigir herramientas de desarrollo al jugador.
- **TH-12/17:** campañas manuales, transiciones, mandos, fullscreen, latencia y
  escucha humana. No dar por habilitados GM/MT-32 u otros backends no probados.
- **Tarea 18:** empaquetado, avisos/fuentes/licencias y dependencias del producto.
  Los parches externos conservan GPL-3.0-or-later; no relicencian el código
  heredado. Los binarios/datos de referencia no forman un paquete distribuible.

## Resolución en TH-07

El hallazgo de saturación de P2 anterior describe la referencia previa. TH-07
reserva margen por bus antes de sumar música y efectos; la misma ruta clásica
de 140 pasos registra `peak=17952`, `clipped=0`. Las nuevas rutas gestionadas
en Linux y Windows también verifican cero muestras saturadas, sin modificar la
mezcla de Blues/Jukebox. Detalles y límites en [FAMILY_RUNTIME.md](FAMILY_RUNTIME.md).

TH-09 incorpora ahora la preparación DIET nativa y el runtime gestionado de
Prehistorik 1 DOS; las herramientas externas de referencia no son dependencias
del jugador. Ver [PREHISTORIK_RUNTIME.md](PREHISTORIK_RUNTIME.md).
