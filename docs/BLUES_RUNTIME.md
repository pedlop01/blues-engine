# TH-06: Blues desde Titus Engine

La biblioteca ya permite **Jugar** con Blues Brothers DOS y Amiga mediante un
runtime nativo separado. Los demás juegos conservan la detección, pero sus
botones de juego esperan a TH-07–09. El ejecutable clásico sigue disponible.

## Uso y comprobación manual

Abrir `build/launcher-windows/titus-engine.exe` en Windows o
`build/launcher/titus-engine` en Linux. Mantener la carpeta `engines/` junto al
frontend, además de las DLL y fuentes descritas en [la biblioteca](LIBRARY_FRONTEND.md).

1. Seleccionar Blues, edición y una ubicación **carpeta** verificada; pulsar Jugar.
2. La biblioteca muestra la preparación y se oculta cuando el juego confirma
   recursos iniciales cargados y dispositivo de audio activo.
3. Jugar conserva controles, cámaras, HUD original/fijo, ayudas, zoom, los ocho
   slots con miniaturas y rebobinado. Con ambas ediciones, F6 alterna en partida.
4. En el menú de pausa, **Volver a biblioteca** pide confirmación. Cancelar permite
   seguir jugando; confirmar cierra el runtime y restaura selección y ventana.

Amiga necesita también una instalación DOS reconocida para los recursos de
Concierto. El supervisor comprueba de nuevo los hashes antes de iniciar. Los ADF
son fuentes de archivo: no se extraen ni ejecutan desde la biblioteca.

La carga de slots se hace en el menú **Partida** del juego. La selección global
de guardados desde la biblioteca corresponde a TH-10; este runtime rechaza una
acción `load_slot` en vez de ignorarla.

## Perfil e importación reversible

El perfil queda bajo la raíz privada de Titus (`--user-dir`, `TITUS_USER_DIR` o
las rutas por usuario del core):

```text
data/profiles/<profile-uuid>/games/blues/blues.full.1991/
  native/blues-family/preferences.ini
  saves/slot01.bbs … slot08.bbs
  legacy-initial/                 # copia inicial exacta, cuando había archivos
  migration-receipt.json
```

En el primer arranque se copian las preferencias y slots clásicos desde
`SDL_GetPrefPath("pedlop01", "blues-engine")`, respetando `BLUES_CONFIG_FILE` y
`BLUES_SAVE_DIR` si se especifican. La copia se prepara en un directorio temporal
hermano; se comprueban hashes y el lector nativo inspecciona los slots antes de
publicar el perfil mediante renombrado. Los originales no se mueven ni escriben.
Un perfil gestionado no vacío **no vuelve a importarse ni se sobrescribe**.

El recibo registra procedencia, hashes y resultado de inspección. Los guardados
v1/v2 se conservan; los dañados o de versiones futuras también se copian y se
señalan como dañados/incompatibles, sin inventar nivel o miniatura. Se rechazan
archivos enlazados y tamaños excesivos. Los directorios privados no pueden estar
dentro de una fuente original.

Para volver al estado anterior basta ejecutar el juego clásico, que conserva sus
archivos. Para restaurar la copia inicial del perfil, cerrar el juego y copiar
explícitamente los archivos de `legacy-initial/` sobre sus destinos relativos,
conservando antes los guardados actuales. No hay restauración automática ni
sincronización bidireccional.

## Procesos, errores y diagnóstico

`platform/` implementa supervisión y bloqueo; `engines/blues-family/` contiene el
puente al código Blues existente. El paquete declara protocolo y revisión en
`engines/blues-family/registry.json`; las rutas de ejecutables son relativas al
paquete, no comandos de shell. Se usan argv UTF-8 en Linux y CreateProcessW con
argumentos Unicode en Windows, incluidas rutas UNC/largas.

Cada sesión escribe petición, eventos JSONL, resultado y logs separados en
`cache/sessions/<session-uuid>/`. Se valida identidad, secuencia y capacidades.
El retorno requiere resultado coherente **y salida real del proceso**; un código
cero sin resultado o con resultado fallido no es éxito. stdout/stderr rotan al
superar 1 MiB en los puntos de consulta del runtime.

Un bloqueo de kernel por perfil/juego/compatibilidad se hereda al hijo: si termina
el frontend, el juego mantiene el bloqueo y puede seguir funcionando. Otra
biblioteca no puede escribir ese perfil mientras viva el hijo. El frontend no
mata procesos automáticamente. Si faltan señales de arranque/cierre, informa del
bloqueo y permite confirmar un cierre forzado, advirtiendo de la partida no
guardada. Un error recupera la biblioteca sin modificar datos originales.

El binario auxiliar `titus-session` usa el mismo supervisor, por ejemplo:

```sh
build/launcher/titus-session --user-dir /ruta/privada --installation UUID
# Inspeccionar/importar sin abrir el juego:
build/launcher/titus-session --user-dir /ruta/privada --installation UUID --inspect-only
```

`--legacy-root` permite señalar un directorio con `preferences.ini` y `saves/`
en diagnósticos. Las opciones `--test-*` y `--gameplay-probe` son ensayos explícitos;
el frontend normal no las activa.

## Compilación y pruebas

El runtime se habilita por defecto con `TITUS_BUILD_LAUNCHER=ON`; necesita un
compilador C, SDL2 y libmodplug, además del C++17/SDL_ttf del frontend. Se puede
compilar solo la biblioteca con `-DTITUS_BUILD_BLUES_RUNTIME=OFF` (Jugar queda
inactivo). El core independiente sigue sin depender de SDL ni de compilador C.

Para SDK externos: `TITUS_MODPLUG_ROOT`, `TITUS_MODPLUG_INCLUDE` y
`TITUS_MODPLUG_LIBRARY`. En MinGW usar **gcc-posix y g++-posix de la misma
instalación**. Mezclar los modelos win32/posix puede provocar fallos dentro de
libstdc++; CMake comprueba la pareja. Al cambiar compiladores, configurar un
directorio nuevo o usar `cmake --fresh`.

```sh
cmake --build build/launcher --parallel 2
ctest --test-dir build/launcher --output-on-failure
python3 tests/check_blues_sessions.py --bin-dir build/launcher --orig-games ../orig-games
python3 tests/check_launcher_sessions.py --bin-dir build/launcher --orig-games ../orig-games
# Desde WSL, ejecutar los PE reales (no Wine):
python3 tests/check_blues_sessions.py --bin-dir build/launcher-windows --orig-games ../orig-games --windows --import-fixtures build/th06-fixtures
python3 tests/check_launcher_sessions.py --bin-dir build/launcher-windows --orig-games ../orig-games --windows
```

Consultar `--help` para exportar fixtures de slots y ensayos cruzados. Los fixtures,
capturas, logs y binarios quedan en `build/`, excluidos de Git; no se distribuyen
recursos comerciales.

Validación TH-06: Linux AArch64 y Windows x64 sobre Windows ARM64; rutas Unicode,
importación desde Linux a Windows, formatos BBS v1/v2, ocho miniaturas, preferencias,
F6/HUD/zoom/rebobinado, retorno cancelado/confirmado, bloqueo tras terminar el
padre, errores de protocolo y rotación de logs. La interfaz abre DOS y Amiga y
recupera la biblioteca dos veces. Se comparan tamaño, fecha y SHA-256 de los 462
originales, así como los archivos clásicos importados. También se mantienen las
pruebas del core, contratos, biblioteca y regresiones clásicas de menú,
preferencias, slots y rebobinado. Son recorridos cortos; campañas completas,
escucha y mandos físicos siguen en TH-12 y en las pruebas de Blues 17.
