# TH-05: biblioteca global

Implementada el **23 de noviembre de 2026**. `titus-engine` es el frontend gráfico
SDL2/C++17 de la biblioteca; utiliza el catálogo y los detectores de TH-04.
`blues`/`blues.exe` conservan su funcionamiento, HUD, opciones y partidas.

## Qué está disponible

- Lista de juegos instalados, agrupando sus ediciones bajo una misma ficha.
- Selección DOS/Amiga, instalación concreta y ubicación alternativa, incluyendo
  ADF asociados. Se recuerda la elección de cada juego al cambiar de ficha.
- Estados de recursos incompletos, sin verificar, ubicaciones desaparecidas,
  discos pendientes de extraer y ediciones Amiga sin motor compatible.
- Añadir carpetas/ADF mediante explorador, ruta escrita/pegada o arrastre;
  exploración de subcarpetas opcional, activada inicialmente.
- Verificar recursos, volver a localizar una instalación y quitar referencias
  con confirmación. Se conservan los originales y las partidas.
- Ventana redimensionable, pantalla completa y navegación con teclado, ratón
  o un mando SDL GameController, incluido conectar/desconectar durante el uso.
- Texto Unicode con DejaVu Sans, independiente de las fuentes/HUD de los juegos.

**Jugar está disponible para Blues Brothers DOS y Amiga** desde TH-06, con
importación inicial reversible y retorno a biblioteca; ver [runtime Blues](BLUES_RUNTIME.md).
Desde [TH-07](FAMILY_RUNTIME.md), también están disponibles Jukebox Adventure y
Prehistorik 2 DOS. [TH-08](FOX_RUNTIME.md) añade Titus the Fox DOS con preparación
nativa privada. [TH-09](PREHISTORIK_RUNTIME.md) añade Prehistorik 1 DOS mediante Olduvai y DIET nativo. Las fuentes ADF se registran sin extraerse ni arrancarse.

## Abrir y añadir los juegos

El ejecutable preparado en esta máquina es:

```text
build/launcher-windows/titus-engine.exe
```

Mantener junto a él `SDL2.dll`, `SDL2_ttf.dll`, `assets/fonts/` y `engines/`. La aplicación
Windows abre una ventana gráfica sin consola auxiliar. Linux usa
`build/launcher/titus-engine` con sus bibliotecas SDL instaladas.

La primera apertura muestra una biblioteca vacía si todavía no existe catálogo.
Pulsa **Añadir juegos**. Con Ctrl+L puedes pegar la carpeta del inventario:

```text
Linux:   /home/plopez/proj/titus/orig-games
Windows: \\wsl.localhost\Ubuntu\home\plopez\proj\titus\orig-games
```

Con **Incluir subcarpetas** y **Usar esta carpeta**, se registran cinco juegos,
ocho instalaciones y doce ubicaciones. Para añadir solo un juego, entra en su
carpeta. Para un ADF concreto, selecciónalo y usa **Usar imagen ADF** o Enter.
El explorador no selecciona automáticamente el primer ADF al abrir una carpeta.

Opciones útiles:

```sh
build/launcher/titus-engine --browse-dir /ruta/a/mis/juegos
build/launcher/titus-engine --user-dir /ruta/absoluta/a/un/perfil/de/prueba
```

`--browse-dir` configura la ubicación inicial del explorador, sin registrar datos
automáticamente. `--user-dir` y `TITUS_USER_DIR` siguen las prioridades de TH-04.
Los recursos del frontend se resuelven respecto al ejecutable, no al CWD.

## Controles

| Acción | Teclado | Mando |
|---|---|---|
| Elegir juego | Arriba/abajo en la lista | Cruceta o stick izquierdo |
| Cambiar control | Tab / Shift+Tab, flechas fuera de la lista | Cruceta; LB/RB en cuadros de diálogo |
| Pasar de lista a instalación | Derecha | Derecha |
| Cambiar instalación / ubicación | Izquierda/derecha sobre su selector | Izquierda/derecha; LB/RB cambian instalación desde la ficha |
| Activar | Enter / espacio | A |
| Añadir juegos | A | Y |
| Cancelar / volver / salir | Esc | B |
| Pantalla completa / ventana | F11 o botón Pantalla | Botón Pantalla mediante navegación |
| Actualizar catálogo | F5 o botón Actualizar | Botón Actualizar |

En el explorador, arriba/abajo recorren entradas, derecha/Enter abren la
selección e izquierda sube una carpeta. Al pasar el final de la lista se llega a
las acciones inferiores. LB/RB permiten cambiar de control sin recorrer la lista.
Ctrl+L selecciona la ruta; Ctrl+A, Ctrl+V y retroceso admiten UTF-8. Un clic
selecciona una entrada y doble clic abre una carpeta. La rueda desplaza la lista.
El botón Unidades/raíz permite salir de una ruta UNC a los discos de Windows.

Se descartan entradas sin foco; al recuperarlo se exige soltar teclas/botones
que ya estuvieran pulsados. El stick tiene zona muerta y requiere volver a
neutral entre movimientos. La navegación del mando virtual está probada;
la validación con modelos físicos de mando sigue dentro de TH-12.

## Guardado y catálogo

`config/library.json` contiene únicamente preferencias de biblioteca v1:

- juego elegido y UUID de instalación preferida para cada juego;
- clave de ubicación elegida por instalación;
- modo de pantalla, tamaño de ventana y última carpeta del explorador.

No contiene configuraciones, trucos o slots de los motores. El archivo sigue las
rutas privadas XDG/APPDATA de TH-04. Sus cambios pasan por el bloqueo del catálogo,
reemplazo atómico y copia `library.json.bak`. Una preferencia corrupta o de versión
no compatible se conserva y se comunica en pantalla; no se sobrescribe con valores
por defecto. Quitar instalaciones elimina selecciones obsoletas al reconciliar
el catálogo, sin borrar sus fuentes o datos privados.

Las operaciones del catálogo se ejecutan en un trabajo de fondo; la ventana
sigue dibujando mientras se comprueban archivos. Usan la revisión que se estaba
mostrando: si otra ventana cambia el catálogo, se solicita actualizar, evitando
sobrescribir ese cambio. El cierre espera a que termine una operación iniciada.
Los cuadros de quitar/salir/restaurar empiezan en Cancelar; al cambiar de cuadro
se reconstruyen los controles antes del siguiente evento para evitar clics a
través del diálogo. Un catálogo corrupto se muestra como error, sin reemplazarlo;
Restaurar copia pide confirmación y conserva el documento dañado.

La disponibilidad de la ruta se consulta al mostrar la ficha; **Verificar**
comprueba el conjunto de recursos. Un estado previamente reconocido no reemplaza
la comprobación por hashes que hace el adaptador Blues antes de arrancar.

## Compilar

El core y sus pruebas siguen sin necesitar SDL. El runtime usa Python 3 para
separar los símbolos privados de los módulos al compilar. La GUI se habilita explícitamente:

```sh
# Desarrollo Linux: SDL2 >= 2.30, SDL2_ttf 2.x y libmodplug, con headers.
cmake -S . -B build/launcher -DTITUS_BUILD_LAUNCHER=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/launcher --parallel 2
```

Con dependencias fuera de las rutas del compilador, usar `SDL2_DIR`,
`TITUS_TTF_INCLUDE` y `TITUS_TTF_LIBRARY`. La comprobación local usa el header
oficial SDL_ttf 2.22.0 y `/lib/aarch64-linux-gnu/libSDL2_ttf-2.0.so.0`; no se instalan
paquetes del sistema durante el build ni se descarga nada desde CMake.

Windows x64 desde WSL, con SDK MinGW de SDL2 2.30.11 y SDL_ttf 2.22.0:

```sh
cmake -S . -B build/launcher-windows -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc-posix \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix \
  -DCMAKE_BUILD_TYPE=Release -DTITUS_BUILD_LAUNCHER=ON \
  -DSDL2_DIR=/ruta/SDL2/x86_64-w64-mingw32/lib/cmake/SDL2 \
  -DTITUS_TTF_INCLUDE=/ruta/SDL2_ttf/x86_64-w64-mingw32/include/SDL2 \
  -DTITUS_TTF_LIBRARY=/ruta/SDL2_ttf/x86_64-w64-mingw32/lib/libSDL2_ttf.dll.a \
  -DTITUS_TTF_DLL=/ruta/SDL2_ttf/x86_64-w64-mingw32/bin/SDL2_ttf.dll \
  -DTITUS_MODPLUG_ROOT=/ruta/modplug-install
cmake --build build/launcher-windows --parallel 2
```

Se copian las dos DLL y la fuente al directorio de salida. Los runtimes C++ de
MinGW se enlazan estáticamente. `cmake --install` conserva fuente y licencia, y
copia las DLL cuando se proporciona `TITUS_TTF_DLL`. Esto es un build de desarrollo;
la distribución final con todos los motores y avisos sigue en la tarea 18.

## Dependencias y procedencia

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.30.11), zlib;
  SDL2 2.30.0 en Linux y SDK 2.30.11 en Windows.
- [SDL_ttf 2.22.0](https://github.com/libsdl-org/SDL_ttf/releases/tag/release-2.22.0),
  zlib. El SDK usa FreeType y HarfBuzz. Portions of this software are copyright
  © 2013 The FreeType Project (www.freetype.org). All rights reserved.
- [DejaVu Sans 2.37](../assets/fonts/README.md), fuente sin modificar, con licencia
  Bitstream Vera y cambios DejaVu en dominio público; avisos incluidos.
- El parser sigue siendo nlohmann/json 3.11.3 de TH-04.

Los SDK y sus binarios permanecen fuera de Git. La fuente se incluye porque es
un recurso redistribuible del frontend, no un dato original de los juegos.
El archivo oficial `SDL2_ttf-devel-2.22.0-mingw.tar.gz` usado tiene SHA-256
`7e3edf46630bbfb1cb8bffd69a5456b037469ed7a20a75232caee0b1752dc65b`.
Créditos, versiones y avisos de dependencias deben acompañar al paquete final.

## Pruebas

```sh
make check-catalog
make check-contracts
make check-launcher
python3 tests/check_launcher.py --binary build/launcher/titus-engine \
  --orig-games ../orig-games
python3 tests/check_launcher.py --binary build/launcher-windows/titus-engine.exe \
  --windows --orig-games ../orig-games
```

Las pruebas del modelo verifican agrupación, elección DOS/Amiga y ADF/carpeta,
persistencia tras reinicio, desaparición de ubicaciones, referencias eliminadas
y rechazo de preferencias futuras. La prueba SDL abre una ventana real en Windows
(y usa SDL dummy por defecto en Linux), inyecta teclado/texto y conecta un mando
virtual: navega, cambia pantalla, redimensiona, pierde/recupera foco, escribe y
borra UTF-8, cancela cuadros, recorre acciones con mando y prueba stick retenido.
Con `--orig-games`, añade/verifica/quita/vuelve a añadir desde los controles reales
de la interfaz y comprueba catálogo, preferencias y originales antes/después.
Los directorios privados de prueba son temporales bajo `build/`.

La prueba completa pasa **32 comprobaciones de interfaz**, con cinco juegos,
ocho instalaciones y doce ubicaciones. Los **462 archivos originales** conservan
SHA-256, tamaño y fecha en Linux y Windows x64 sobre Windows ARM64. Se han revisado
capturas de la ventana, explorador, confirmación, redimensionado y pantalla completa.
Las capturas BMP quedan en `build/library-linux*.bmp` / `build/library-windows*.bmp`.
No son recursos incluidos en el producto. Campañas, mandos físicos y distribución
limpia de todo el conjunto siguen las tareas TH-12/17/18.

TH-10 añade **Opciones** por capas y **Partidas**, con ocho previews/carga nativa
de Blues y comparación/importación explícita del legado. Los juegos sin slots
lo indican sin simular guardados. Ver [ajustes y partidas](SETTINGS_AND_SAVES.md).
