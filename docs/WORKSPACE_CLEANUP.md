# Compilaciones y limpieza del espacio de trabajo

`build/` contiene únicamente dos entregas listas para ejecutar:

```text
build/
  linux/bin/titus-engine
  windows/bin/titus-engine.exe
```

Cada una incluye los cinco motores, sus dependencias, fuentes, imágenes y
licencias. Los perfiles y partidas normales se guardan fuera de esas carpetas.

## Compilar desde cero

```sh
python3 tools/build_releases.py --replace
```

Se puede elegir `--platform linux` o `--platform windows`, y ajustar `--jobs`.
El script crea una compilación temporal nueva, ejecuta las pruebas e instala la
entrega Release. Los intermedios se eliminan al terminar correctamente;
`--keep-work` permite conservarlos expresamente para investigar un fallo.
Las DLL de Windows se distribuyen sin símbolos de depuración innecesarios.

Dependencias:

- Linux: CMake, compilador C/C++, Python 3 y SDL2/SDL2_ttf/libmodplug de desarrollo.
- Windows desde WSL: MinGW x64 con C y C++ POSIX, SDK SDL2 2.30.12,
  SDL2_ttf 2.22.0 y libmodplug estático compatible.

Los SDK Windows se buscan en `$XDG_CACHE_HOME/titus-engine/deps`:
`SDL2-2.30.12/x86_64-w64-mingw32`,
`SDL2_ttf-2.22.0/x86_64-w64-mingw32` y `libmodplug-windows`.
Un `include/SDL2/SDL_ttf.h` opcional sirve como cabecera Linux cuando ya está
instalada la biblioteca. Las imágenes locales opcionales están en `artwork/*.bmp`.
`--deps-dir` permite usar otra ubicación.

Los trabajos temporales están en `$XDG_CACHE_HOME/titus-engine/work`; los registros
quedan en `$XDG_STATE_HOME/titus-engine/build-logs`. Por defecto son `~/.cache`
y `~/.local/state`, respectivamente. El código de los motores modernos está
incluido en el repositorio; ya no se conservan exports antiguos en `build/`.

## Guardados y evidencia preservados

La limpieza inicial del 24-11-2026 recuperó **21,85 GB** y respaldó **520 registros**
de slots, preferencias y configuración de los ensayos, verificando SHA-256.
Estos respaldos y los perfiles de ensayo r6 están ahora fuera del repositorio,
en `$XDG_STATE_HOME/titus-engine/workspace-backups`.

Los directorios `retained-state`, `retained-validation` y `retained-configs`
dentro de cada respaldo contienen los estados, informes y configuraciones
anteriores. `retained-state/manifest.json` relaciona cada ruta antigua con su
contenido en `objects/<sha256>`, tamaño y fecha. Para recuperar un archivo,
busca su ruta en el manifiesto y copia ese objeto a la ubicación deseada.

Las rutas antiguas de `build/` en informes históricos corresponden a aquellas
ejecuciones; los originales de los juegos permanecen en `../orig-games`.

## Retirar nuevas salidas obsoletas

```sh
python3 tools/clean_workspace.py          # Mostrar el plan
python3 tools/clean_workspace.py --apply  # Respaldar, verificar y limpiar
```

La limpieza conserva `build/linux` y `build/windows`. Limita el borrado a salidas
ignoradas y preserva los estados antes de borrar. Los respaldos nuevos se guardan
fuera de `build/`, en `workspace-backups/cleanup-<identificador-del-repositorio>`.
Los enlaces se tratan como enlaces, sin recorrer sus destinos externos.
