# Limpieza del espacio de trabajo

La limpieza del 24-11-2026 eliminó **1.531 rutas de salidas obsoletas** y recuperó
**21.850.041.212 bytes (21,85 GB)**. Los archivos locales de `build/` pasaron de
22,67 GB a unos 815 MB de contenido. Los originales y los paquetes r6 conservaron
su tamaño, fecha y SHA-256: **531 archivos verificados** en total.

## Entregas y desarrollo conservados

- Windows: `build/library-package-windows-r6/bin/titus-engine.exe`.
- Linux: `build/library-package-linux-r6/bin/titus-engine`.
- Compilaciones actuales: `build/th14-release` y `build/th14-windows`.
- SDK x64, fuentes preparadas, libmodplug y entorno Python usados por esas compilaciones.
- Imágenes, perfiles de ensayo y capturas de la última revisión r6.
- Fuentes, pruebas de regresión, documentación, informes versionados y capturas
  de upstream utilizadas por el README.

Se retiraron paquetes anteriores y ZIP duplicados, compilaciones intermedias,
trazas voluminosas, capturas antiguas, objetos Make, bytecode Python y SDK de
32 bits que no usan las compilaciones actuales.

## Guardados y evidencia local

Antes de borrar las carpetas de ensayo se preservaron **520 registros** de slots,
preferencias y configuración. Sus contenidos se guardan una sola vez por SHA-256
en `build/retained-state/objects/`; `manifest.json` relaciona cada ruta anterior
con el objeto, tamaño y fecha. Se verificaron todas las copias antes del borrado.
Para recuperar un archivo, busca su ruta en el manifiesto y copia el objeto de
ese SHA-256 a la ubicación deseada.

Los informes JSON y los registros recientes están en `build/retained-validation/`.
Las configuraciones CMake descartadas, incluido el build de sanitizadores, están
en `build/retained-configs/`. Las rutas antiguas de `build/` citadas en informes
históricos corresponden a las ejecuciones originales y pueden haberse retirado.
Estos respaldos locales permanecen ignorados por Git.

## Repetir la limpieza

```sh
python3 tools/clean_workspace.py          # Mostrar el plan
python3 tools/clean_workspace.py --apply  # Respaldar, verificar y limpiar
```

El script limita el borrado a salidas ignoradas, conserva la entrega más reciente
por plataforma y comprueba que `build/` no contenga archivos versionados. Los
archivos de estado se preservan antes de empezar a borrar. Los enlaces simbólicos
se tratan como enlaces; sus destinos externos se mantienen.

Tras la limpieza pasaron las compilaciones de desarrollo Linux/Windows, los ocho
tests CTest y el smoke test del lanzador Windows empaquetado. La comprobación
posterior del plan no encontró más salidas obsoletas.


## Organización actual: dos entregas

La revisión siguiente sustituye los directorios anteriores por:

```text
build/
  linux/bin/titus-engine
  windows/bin/titus-engine.exe
```

Cada carpeta contiene los cinco motores, fuentes, imágenes, licencias y utilidades
necesarios para esa plataforma. Los directorios de compilación se crean bajo
`$XDG_CACHE_HOME/titus-engine/work` y se eliminan al terminar correctamente.
Los SDK compartidos están en `$XDG_CACHE_HOME/titus-engine/deps`, y los registros
de compilación en `$XDG_STATE_HOME/titus-engine/build-logs`.

Los respaldos anteriores se trasladan a
`$XDG_STATE_HOME/titus-engine/workspace-backups` (por defecto `~/.local/state`).
`tools/clean_workspace.py` conserva ahora únicamente `linux` y `windows`; cualquier
nuevo respaldo se guarda fuera de `build/`.

Para compilar desde cero ambas versiones usando los SDK de la caché:

```sh
python3 tools/build_releases.py --replace
```

Puede seleccionarse `--platform linux` o `--platform windows`. El script compila
en Release, verifica las pruebas, instala la entrega y elimina símbolos de
depuración de los binarios distribuidos. `--keep-work` permite conservar
explícitamente los intermedios si se está investigando un fallo.
