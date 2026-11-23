# TH-09: Prehistorik 1 DOS

Seleccionar **Prehistorik → DOS → Jugar** en `build/launcher/titus-engine`
o `build/launcher-windows/titus-engine.exe`. Olduvai corre en un proceso nativo
separado. No se ejecuta su CLI independiente ni el EXE DOS. La edición Amiga
continúa pendiente de viabilidad.

| Control | Acción |
|---|---|
| Flechas | Movimiento; arriba salta |
| Espacio / Ctrl izquierdo | Ataque |
| Esc | Menú de pausa nativo |
| Cerrar ventana / salir del menú | Volver a la biblioteca |

Se conservan las pantallas, menús y renderizado nativos. Las capacidades
anunciadas son retorno, pausa y pantalla completa. No hay slots gestionados
(contador cero, índice null), rebobinado ni ayudas heredadas de Blues. La ruta
de quicksave permanece vacía. Los [ajustes globales de TH-10](SETTINGS_AND_SAVES.md) ya están disponibles; la
comprobación de mandos físicos/campañas completas a TH-12.

## Preparación y aislamiento

La identificación por contenido encuentra PRE1.EXE y FILES* incluso dentro de
la carpeta mixta `prehistorik2-dos`. La inspección no inicializa SDL ni escribe
la caché. Si falta preparación, el supervisor ejecuta un job nativo distinto y
repite la inspección antes de lanzar el juego.

`platform/pre1_prepare.cpp` implementa únicamente el formato DIET auditado,
con límites de salida/iteraciones, CRC16/ARC `2f77`, reconstrucción MZ y hashes.
Acepta PRE1.EXE de 77 839 bytes con SHA-256
`b03ce6913d07a4c5b66dfc4843d185ffdff6561e9bf91f3333b096016690406f`.
Produce HISTORIK.EXE de 164 166 bytes con SHA-256
`5cea262f9610657af8b55c10b277ec30ffe99df35294d5f8c9f520a30e029c96`.
No ejecuta código DOS ni requiere Python, Deark, Unicorn o DOSBox al jugar.

```text
data/profiles/<uuid>/games/prehistorik/prehistorik.full.audited/native/olduvai/
  preferences.json
  prepared/<fingerprint>.pre1-diet-v1/
    HISTORIK.EXE
    FILESA.CUR, FILESB.CUR, FILESA.VGA, FILESB.VGA
    manifest.json
  reports/
```

Los originales son de solo lectura. La publicación usa una carpeta temporal,
verificación completa y renombrado; una caché dañada se aparta y reconstruye.
Se rechazan alias/symlinks que escaparían del workspace privado. Windows usa
rutas Unicode extendidas y consulta directa de archivos para las rutas UNC
largas que la iteración de directorios de MinGW no resuelve correctamente.

Las preferencias propias tienen schema 1, fullscreen y un mapa de ajustes
nativos; una versión futura se conserva y rechaza. No se importa configuración
independiente de Olduvai. Los informes nativos permanecen en reports, sin
fallback al directorio personal. Se verifica el bloqueo heredado del perfil.

El puente enlaza las bibliotecas del motor directamente. `ready` se emite tras
presentar el menú o gameplay real, con dispositivo de audio válido; una ventana
abierta o exit code cero sin carga no se considera éxito. El supervisor recoge
el código real de salida y restaura la biblioteca.

## Fuentes y comprobación

Olduvai está fijado en `a69e9204fc311c8810d696a6b799000d1bcd1642`:
https://github.com/ksokolowski/olduvai. Ver [avisos](../engines/olduvai/NOTICE.md).

```sh
python3 tools/prepare_pre1_sources.py --olduvai /ruta/al/checkout --output build/pre1-sources
```

Configurar el proyecto con `TITUS_BUILD_PRE1_RUNTIME=ON` y
`TITUS_PRE1_SOURCES` apuntando a esa exportación, además de SDL2. El helper
Python es solo de compilación. No importa el CLI ni las políticas de instalación
del proyecto externo. Los avisos/licencias acompañan el runtime; la distribución
final con fuentes correspondientes sigue pospuesta a tarea 18.

`tests/check_pre1_sessions.py` comprueba Linux y Windows: preparación fallida,
hashes, reparación de caché, inspección sin vídeo, movimiento/salto/ataque,
pausa, captura, PCM final sin clipping en el recorrido, retorno y fallos de
protocolo. Verifica que los 462 archivos originales no cambien. No sustituye
una campaña ni la escucha manual. El ciclo de seis arranques se prueba mediante
`check_launcher_sessions.py --pre1`; Windows admite `--windows --headless`
cuando no hay pantalla disponible en el host.
