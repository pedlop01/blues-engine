# TH-07: Jukebox Adventure y Prehistorik 2

La biblioteca permite iniciar las ediciones **DOS auditadas** de los tres juegos
de la base [cyxx/blues](https://github.com/cyxx/blues): Blues Brothers, Jukebox
Adventure y Prehistorik 2. Se conserva el código de juego original del proyecto,
con el supervisor y protocolo de [TH-06](BLUES_RUNTIME.md).

## Uso y capacidades reales

Abrir `build/launcher-windows/titus-engine.exe` (Windows) o
`build/launcher/titus-engine` (Linux), seleccionar el juego y pulsar **Jugar**.
En la carpeta mixta de Prehistorik 2 se ejecuta su módulo P2, aunque también
contenga archivos de Prehistorik 1: se usa la identidad/hash del catálogo, no una
heurística por nombre de ejecutable.

| Juego | Edición integrada | Pausa y salida | Funciones anunciadas |
|---|---|---|---|
| Blues Brothers | DOS y Amiga con complemento DOS | Menú de pausa y retorno confirmado | Mantiene las capacidades de TH-06, ocho slots, miniaturas y rebobinado |
| Jukebox Adventure | DOS | P pausa/reanuda; Esc o cerrar la ventana vuelve a biblioteca | Retorno, pausa nativa, pantalla completa y volúmenes separados |
| Prehistorik 2 | DOS | P pausa/reanuda; Esc o cerrar la ventana vuelve a biblioteca | Retorno, pausa nativa, pantalla completa y volúmenes separados |

En Jukebox/P2, flechas para mover, Shift para saltar y Espacio/Enter para acción
y menús. Se mantienen los controles nativos de mando de la base. La salida de
estos dos juegos es directa, sin el cuadro de confirmación específico de Blues.
No se anuncian guardados, miniaturas, rebobinado, zoom, cámara suavizada, cambio
DOS/Amiga ni ayudas de Blues. El protocolo rechaza ajustes de Blues y capacidades
no disponibles. Los índices de guardados tienen cero slots.

Los datos Amiga identificados como Prehistorik son **Prehistorik 1**. TH-07 no
habilita una edición Amiga de P2. Fox DOS se añade mediante OpenTitus en
[TH-08](FOX_RUNTIME.md); Prehistorik 1 sigue pendiente.

## Preferencias y datos separados

Los originales siguen siendo de solo lectura. El runtime vuelve a verificar las
firmas antes de cargar y confirma `ready` al mostrar el menú nativo cargado, con
el dispositivo de audio activo. Cada juego tiene su bloqueo y espacio privado:

```text
data/profiles/<uuid>/games/jukebox/jukebox.full.audited/native/blues-family/preferences.json
data/profiles/<uuid>/games/prehistorik2/prehistorik2.full.audited/native/blues-family/preferences.json
```

Por ahora se guardan pantalla completa y volúmenes en un archivo v1 independiente:

```json
{"schema_version":1,"fullscreen":false,"music_volume":100,"effects_volume":100,"action_button":0,"jump_button":1}
```

Los valores de volumen van de 0 a 100. Se pueden editar con el juego cerrado;
TH-10 incorpora el [formulario de opciones por capas](SETTINGS_AND_SAVES.md)
y los botones de acción/salto por capacidad. Los ajustes explícitos de
una petición tienen prioridad. Una versión futura o un valor inválido impide el
inicio y conserva el archivo. Jukebox/P2 no importan opciones ni slots de Blues,
ni crean guardados ficticios. La importación de Blues permanece igual.

La biblioteca se oculta solo tras `ready`, vuelve a abrirse al terminar el proceso
y conserva la selección. La solicitud de retorno funciona también durante la
pausa nativa. Se mantienen el bloqueo heredado, logs y tratamiento de errores del
supervisor anterior.

## Mezcla de P2

La ruta de referencia TH-02 registró muestras saturadas al combinar música y
efectos. El mezclador sumaba dos señales capaces de alcanzar escala completa y
recortaba el resultado a 16 bits. P2 ahora reserva margen antes de sumar: aplica
48 % a cada bus, manteniendo sus proporciones y los volúmenes configurables.
Incluso con las dos señales al máximo, la suma queda por debajo de los límites.
El volumen de salida de P2 es por tanto menor; no se modifican sus recursos ni
se añade compresión dinámica.

Se configura únicamente en `p2/sound.c`, también para el ejecutable clásico.
Blues y Jukebox conservan ganancia unitaria. El margen se restaura al finalizar
el mixer. `check_mixer_headroom` verifica señales extremas, la mezcla por defecto
y su restauración. Las rutas reales de P2 con música/acción registran cero muestras
saturadas tanto en Linux como en Windows. Esto comprueba el desbordamiento de la
mezcla; la escucha humana y una campaña completa siguen pendientes en TH-12.

## Compilación y validación

El mismo paquete `engines/blues-family/` incluye los tres módulos. CMake enlaza
los objetos de Jukebox/P2 por separado y usa `tools/namespace_coff_module.py` para
separar símbolos privados tanto en ELF como en COFF, manteniendo compartidas las
referencias a SDL/mixer/supervisor. Evita colisiones de `g_vars`, `g_res` y funciones
con el mismo nombre. No se reemplaza la simulación de los juegos.

Las dependencias de TH-06 se mantienen; la compilación del runtime necesita además
Python 3 para esta operación sobre objetos. El core independiente no lo necesita.
La opción histórica `TITUS_BUILD_BLUES_RUNTIME` ahora compila la familia completa;
con `OFF` no se instala su motor. MinGW requiere gcc-posix/g++-posix de la misma
instalación. Véanse [frontend](LIBRARY_FRONTEND.md) y [runtime Blues](BLUES_RUNTIME.md).

```sh
cmake --build build/launcher --parallel 2
ctest --test-dir build/launcher --output-on-failure
python3 tests/check_family_sessions.py --bin-dir build/launcher --orig-games ../orig-games
python3 tests/check_launcher_sessions.py --bin-dir build/launcher --orig-games ../orig-games --family
python3 tests/check_blues_sessions.py --bin-dir build/launcher --orig-games ../orig-games
# PE ejecutados realmente desde WSL; añadir --windows a las pruebas Python:
ctest --test-dir build/launcher-windows --output-on-failure
python3 tests/check_family_sessions.py --bin-dir build/launcher-windows --orig-games ../orig-games --windows
python3 tests/check_launcher_sessions.py --bin-dir build/launcher-windows --orig-games ../orig-games --windows --family
```

Las rutas de Jukebox/P2 usan teclado SDL real para avanzar por menús, mover a ambos
lados, saltar, atacar, pausar (incluido audio) y volver durante la pausa. Se miden
coordenadas y PCM del callback de producción y se generan capturas en `build/`.
Se valida el protocolo, capacidades, preferencias distintas, rechazo de versiones
futuras y conservación de tamaño/fecha/SHA-256 de los 462 originales. El recorrido
de interfaz abre Blues DOS → Jukebox → P2 → Blues Amiga y restaura la biblioteca
cuatro veces. Las regresiones de Blues comprueban formatos v1/v2, ocho miniaturas,
F6/HUD/zoom/rebobinado, migración, bloqueo huérfano y fallos de protocolo.

No se incorporan datos comerciales ni binarios de referencia al repositorio.
Las campañas completas, escucha y mandos físicos siguen en TH-12 y las pruebas
manuales de Blues en 17. Créditos y límites de licencia en [CREDITS.md](../CREDITS.md).
