# TH-04: catálogo y detección

Implementado el **23 de noviembre de 2026**. Biblioteca `titus_core` en C++17,
sin SDL, y herramienta `titus-catalog` para gestionar el catálogo v1 de TH-03.
El frontend gráfico `titus-engine` corresponde a TH-05; esta herramienta todavía
no prepara ni arranca motores y no importa preferencias o partidas de Blues.

## Compilar y probar

```sh
cmake -S . -B build/core -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core --parallel 2
ctest --test-dir build/core --output-on-failure
python3 tests/check_catalog_cli.py --binary build/core/titus-catalog
make check-contracts
```

`make check-catalog` agrupa las tres primeras comprobaciones y la prueba CLI.
Python necesita las dependencias de `tests/requirements-contracts.txt` para la
validación del esquema. El ejecutable y las pruebas C++ no necesitan Python,
SDL, acceso de red ni datos originales. nlohmann/json 3.11.3 está incorporado con
su licencia MIT; SHA-256 se calcula en streaming con código del core.

Prueba adicional opcional con el inventario local, en solo lectura:

```sh
python3 tests/check_catalog_cli.py --binary build/core/titus-catalog \
  --orig-games ../orig-games
```

Windows x64 desde WSL/MinGW:

```sh
cmake -S . -B build/core-windows -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix -DCMAKE_BUILD_TYPE=Release
cmake --build build/core-windows --parallel 2
build/core-windows/check_catalog.exe \
  "$(wslpath -w "$PWD/contracts/fingerprint-v1-vector.json")"
python3 tests/check_catalog_cli.py --binary build/core-windows/titus-catalog.exe \
  --windows --orig-games ../orig-games
```

La CLI usa `wmain`, argumentos UTF-16 de Windows y JSON UTF-8. Se han comprobado
rutas de disco y UNC `\\wsl.localhost\Ubuntu\…`, espacios y acentos. MinGW no
clasifica correctamente estas raíces UNC con `filesystem::is_absolute`; el core
resuelve rutas existentes, junctions y ancestros mediante Win32. La CLI enlaza
estáticamente sus dependencias C++ y no requiere las DLL de SDL del juego.

## Uso

Ejemplo aislado, sin tocar el catálogo habitual del usuario:

```sh
CATALOG_USER_DIR="$PWD/build/catalog-demo"
build/core/titus-catalog --user-dir "$CATALOG_USER_DIR" scan ../orig-games --recursive
build/core/titus-catalog --user-dir "$CATALOG_USER_DIR" add ../orig-games --recursive
build/core/titus-catalog --user-dir "$CATALOG_USER_DIR" list
```

`scan` solo lee; devuelve candidatos y diagnósticos de archivos ausentes,
modificados o ambiguos. El campo `installation` de un candidato carece de UUID
hasta el registro y **no es por sí mismo un catálogo**. Una fuente sin identidad
confiable, como un ADF desconocido o nombres de recursos en conflicto, tiene
`installation=null` y un diagnóstico. No se escanea el disco automáticamente.
`--recursive` es explícito, omite `.git` y enlaces simbólicos, y limita el
recorrido a 32 niveles y 50 000 entradas; un error cancela la operación completa.

| Orden | Resultado |
|---|---|
| `init` | Crea catálogo vacío y perfil Jugador, o devuelve el existente. |
| `add RUTA [--recursive]` | Registra todos los conjuntos detectados en una transacción; las copias idénticas añaden ubicaciones al mismo UUID. |
| `list` | Devuelve el documento de catálogo v1; no asegura que una ruta siga disponible. |
| `relocate UUID RUTA` | Comprueba identidad de los nuevos recursos, añade la ruta y descarta las ubicaciones locales desaparecidas. No mueve archivos. |
| `recheck UUID` | Comprueba recursos actuales y actualiza su estado. Conserva identidad y compatibilidad previstas cuando un conjunto conocido se daña o desaparece. |
| `remove UUID` | Quita la referencia; conserva originales, partidas y directorios privados. |
| `recover` | Recupera una copia válida cuando el catálogo falta o está corrupto; conserva el archivo dañado con otro nombre. No retrocede un catálogo válido o de esquema desconocido. |
| `paths` | Muestra las tres rutas privadas resueltas, sin crearlas. |

Las mutaciones admiten `--expected-revision N`: un cliente con una revisión
antigua recibe `E_REVISION`, sin sobrescribir cambios de otro cliente. `add` es
idempotente si no cambia nada. `recheck`/`relocate` registran una revisión nueva.
`relocate` requiere el mismo fingerprint; nunca convierte una instalación
verificada en otra variante. `recheck` puede completar un registro inicialmente
`unverified`, conservando su UUID. Si ese conjunto ya está registrado con otro
UUID, exige resolverlo explícitamente (`E_DUPLICATE_IDENTITY`), para conservar
las referencias existentes. No se revisan desde Windows ubicaciones que solo
tienen una ruta Linux, ni viceversa (`E_NO_LOCAL_SOURCE`).

## Perfiles admitidos

`core/data_profiles.json` contiene **solo nombres, tamaños y SHA-256**, sin datos
del juego. Se incorpora al compilar y no depende del CWD o de un archivo externo.
Cada perfil enumera el conjunto auditado de recursos: gráficos, niveles,
presentaciones y audio; también el ejecutable fuente cuando se necesita para
preparar recursos/audio DOS. Las anclas solo seleccionan familias candidatas:
para reconocer el perfil deben coincidir **todos** sus recursos.

| Juego | Edición | Recursos del perfil | Estado del conjunto completo |
|---|---|---:|---|
| Blues Brothers | DOS | 47 | `recognized` |
| Blues Brothers | Amiga | 56 | `recognized` |
| Jukebox Adventure | DOS | 30 | `recognized` |
| Prehistorik 2 | DOS | 51 | `recognized` |
| Titus the Fox | DOS | 22 | `needs_preparation` |
| Prehistorik 1 | DOS | 5 | `needs_preparation` |
| Titus the Fox | Amiga | 85 | `missing_engine`, motor nulo |
| Prehistorik 1 | Amiga | 66 | `missing_engine`, motor nulo |

`full-audited-dos`/`full-audited-amiga` identifican estos conjuntos comprobados;
no atribuyen una revisión comercial desconocida. Otro conjunto con las mismas
anclas pero contenido distinto es `unverified`; si solo faltan recursos es
`incomplete`. Sus grupos `unverified.*` no se confunden con partidas verificadas.
Las ediciones Amiga sin motor también pueden comunicar esos daños; nunca pasan
a un estado jugable ni se les asigna un motor DOS.

Los archivos adicionales, documentación, lanzadores BAT/COM no utilizados,
configuración de usuario, arranque Amiga y `:Zone.Identifier` quedan fuera del
fingerprint. La lista exacta está en el manifiesto. Los nombres se comparan en
minúsculas ASCII; copias con distinta capitalización y mismo contenido se
reducen a una entrada. Contenidos distintos bajo el mismo nombre normalizado
producen `E_AMBIGUOUS` al registrar; no se elige un archivo arbitrariamente.

El fingerprint sigue la serialización canónica v1 de TH-03, comprobada contra
su vector y contra una implementación Python independiente con todos los
perfiles reales. Mover o copiar archivos no cambia la identidad. Blues DOS y
Amiga comparten `blues.full.1991`, sin modificar preferencias/slots anteriores.

Los tres ADF auditados se reconocen por hash completo y se enlazan al perfil
extraído correspondiente. La coincidencia de Blues se ha comprobado contra sus
58 archivos; Fox y Prehistorik ya tenían verificados sus 89/71 archivos en TH-01.
**No se extrae ni monta una imagen en esta tarea.** Blues ADF solo queda
`needs_preparation`; Fox/Prehistorik ADF, `missing_engine`. Si también está
presente la carpeta exacta, su estado tiene preferencia al revisar. Una imagen
ADF desconocida se informa sin inventar juego o variante y no se registra.

Reconocer recursos **no habilita Jugar**: faltan el registro de motores y los
adaptadores de TH-06–09. El catálogo no sustituye la inspección previa a lanzar.
Perfiles de otras ediciones se incorporarán tras auditar sus recursos; esta
versión no promete reconocer todas las distribuciones históricas.

## Persistencia y protección de fuentes

`--user-dir ABS` tiene prioridad sobre `TITUS_USER_DIR`, y ambos sobre las rutas
por plataforma de [TH-03](TITUS_ARCHITECTURE.md). Un root alternativo usa
`config/`, `data/`, `cache/`. Linux respeta XDG y Windows APPDATA/LOCALAPPDATA.
Los comandos de lectura no crean directorios. Solo las mutaciones crean el
almacén privado; `profile_directory` calcula las rutas de futuros datos por
perfil/juego/grupo sin crear ni migrar partidas.

`config/catalog.json` tiene esquema 1, UUID estables y revisión creciente.
El parser rechaza claves duplicadas, números fuera de JSON, UTF-8 inválido,
anidamiento superior a 64 y documentos superiores a 1 MiB; comprueba campos,
tipos, identificadores, motores, rutas y referencias de instalaciones. Límites
del core: 128 perfiles, 1024 instalaciones y 64 ubicaciones por instalación.
Catálogos futuros o corruptos provocan un error; nunca se sustituyen por uno vacío.

Cada escritura toma un bloqueo del sistema (`flock` o handle exclusivo Windows),
vuelve a leer bajo el bloqueo y realiza un reemplazo atómico desde un temporal
en el mismo directorio. Vacía buffers antes del reemplazo y conserva el catálogo
válido anterior en `catalog.json.bak`. El bloqueo se libera al cerrar el proceso;
un archivo `catalog.lock` residual no bloquea por sí solo. `recover` es explícito
y conserva el documento corrupto en `catalog.corrupt.UUID.json`.

Antes de registrar una fuente o modificar un catálogo con ubicaciones locales,
el core rechaza solapamientos entre originales y cualquiera de sus tres raíces
privadas, en ambas direcciones. No borra ni cambia permisos de los originales.
El usuario debe dedicar el `--user-dir` al programa; `init` no puede identificar
por adelantado recursos que todavía no se hayan registrado. No hay migración de
preferencias o partidas, modificación de archivos del juego ni ejecución de sus
EXE/COM. Las pruebas toman un inventario antes/después de los originales,
incluyendo SHA-256, tamaño y fecha.

## Validación de cierre

- Pruebas C++ sintéticas: SHA-256, vector v1, carpetas mixtas, mayúsculas,
  duplicados/ambigüedad, incompletos, cambios, identidad al mover, ADF,
  recuperación, revisión, bloqueo, protección de fuentes y conservación de saves.
- CLI real y esquema/semántica Python en Linux y Windows, incluyendo Unicode,
  carpetas movidas, ADF antes/después de extracción y error de esquema futuro.
- Inventario real: **8 instalaciones / 12 ubicaciones**. Prehistorik DOS se
  agrupa en dos carpetas; las tres ediciones Amiga se enlazan con sus ADF.
- **462 archivos originales sin cambios** de contenido, tamaño o fecha, en las
  pruebas de ambas plataformas. Los directorios temporales se crean bajo `build/`.
- Pruebas de contratos v1 ampliadas para estados de daño Amiga sin motor.

TH-04 queda completada. TH-05 usará este core para construir la biblioteca;
las tareas de lanzamiento, preparación, migración y vuelta desde motores siguen
sus dependencias originales. Los binarios y catálogos de prueba no se versionan.
