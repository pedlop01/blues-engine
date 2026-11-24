# TH-12: validación de la primera expansión

La validación distingue la evidencia automática de la pasada manual en vídeo,
audio y dispositivos reales. **Una ruta corta reproducible no certifica una
campaña completa.** TH-12 queda cerrada por aceptación expresa del usuario el 24 de noviembre de
2026. No se infiere una campaña ni una prueba física concreta de esa aceptación.
La distribución 18 requiere además cerrar las pruebas de Blues 17.

## Catálogo real

| Juego / edición | Biblioteca | Motor | Funciones verificadas por automatización |
|---|---|---|---|
| Blues DOS | Disponible | blues-family | Arranque/retorno, ocho BBS v1/v2/previews, importación/carga nativa, HUD/cámara/zoom/F6/rewind y opciones |
| Blues Amiga | Disponible; requiere complemento DOS compatible | blues-family | Mismo grupo de slots, edición guardada, arranque/retorno, F6/HUD/zoom y aislamiento |
| Jukebox Adventure DOS | Disponible | blues-family | Movimiento/salto/acción, pausa, PCM, volúmenes, botones de mando virtual y retorno |
| Prehistorik 2 DOS | Disponible | blues-family | Movimiento/salto/acción, pausa, PCM, volúmenes, botones de mando virtual y retorno |
| Titus the Fox DOS | Disponible | opentitus | Preparación LZW, movimiento/salto/acción, pausa, PCM y retorno; no slots gestionados |
| Prehistorik 1 DOS | Disponible | olduvai | Preparación DIET, movimiento/salto/ataque, pausa, PCM y retorno; no slots gestionados |
| Fox / Prehistorik Amiga | Detectadas, sin arranque | Pendiente TH-13–16 | No se presentan como jugables ni reciben opciones de Blues |

No se promete todavía la paridad de los dos motores nuevos con Blues. La fase
acordada tiene seguimiento en [EXPERIENCE_PARITY_PLAN.md](EXPERIENCE_PARITY_PLAN.md),
con scroll super suave y cámara/encuadre como primera prioridad.

## Resultado actual

**TH-12 cerrada (100 %) por el usuario el 24/11/2026. Batería automática
superada en Linux y Windows.** La checklist inferior conserva los casos manuales
sin evidencia individual registrada; los JSON históricos no se alteran. Informes: [Linux](measurements/th12-linux.json),
[Windows](measurements/th12-windows.json) y
[regresión clásica de Blues](measurements/th12-blues-baseline.json).

Linux pasa 11 grupos de integración y Windows 10 (el esquema JSON común se
valida una vez). La matriz nueva valida 24 sesiones de protocolo por plataforma:
18 arranques correctos y seis fallos recuperados, además de rechazos de fuente,
slot, motor ausente y Amiga no implementada. Cada interfaz pasa seis arranques
por el botón Jugar con 12 comprobaciones de retorno/selección. Blues añade 22
grupos de regresión clásica, además de su compilación, con los originales intactos.

Los informes se generaron sobre la base `afaef1e` con los cambios de este hito en
el árbol de trabajo. Conservan fingerprint de los binarios y hashes de logs;
no son una certificación del paquete distribuible de la tarea 18. Windows
reutiliza siete grupos aprobados de la primera pasada: logs verificados y
binarios anteriores al comienzo; el informe registra ese checkpoint. Se repite
el caso corregido y los dos grupos de interfaz restantes, sin presentar una
repetición innecesaria como evidencia nueva.

## Matriz automática reproducible

`tools/validate_expansion.py` ejecuta las pruebas existentes más la matriz nueva,
con directorios temporales y logs/resultados propios. Comprueba los 462 originales
antes/después. Linux es AArch64; Windows es PE x64 ejecutado en Windows ARM64 por
interop WSL. Los drivers de vídeo/audio son dummy: no se afirma escucha humana,
latencia de dispositivo real ni GPU/fullscreen físico por esos resultados.

```sh
python3 tools/validate_expansion.py --bin-dir build/launcher \
  --orig-games ../orig-games --platform linux --output build/th12/linux
python3 tools/validate_expansion.py --bin-dir build/launcher-windows \
  --orig-games ../orig-games --platform windows --output build/th12/windows \
  --import-fixtures build/th12/linux/fixtures
```

La regresión de la CLI clásica de Blues tiene además su propia batería Linux:

```sh
python3 tests/check_blues_baseline.py --orig-games ../orig-games \
  --output build/th12/blues-baseline
```

Compila sus harnesses y comprueba sprites, pausa, preferencias, mixer,
presentación/intermisión normal y ancha, ambos HUD/cámaras, bordes de scroll,
capas/ascensores Amiga, estados y rewind en los seis niveles. Los scripts
resuelven las carpetas actuales `blues-dos`/`blues-amiga` y admiten los nombres
antiguos `dos`/`amiga`; ya no dependen de una ruta relativa obsoleta.

La segunda orden se ejecuta después de generar los fixtures Linux y verifica que
Windows lea los mismos BBS nativos. No usar una carpeta de originales o un perfil
real como salida. El runner no construye ni distribuye un paquete: la tarea 18
conserva ese alcance. `--resume` reutiliza únicamente resultados aprobados cuyo
log conserva su hash y cuyos ejecutables/base coinciden. Para el primer informe,
sin fingerprint inicial, exige que los ejecutables sean anteriores al comienzo
de la batería y registra que el fingerprint se obtuvo al reanudar. Un cambio de
binarios exige una validación nueva. Los casos fallidos se vuelven a ejecutar.

| Bloque de aceptación | Evidencia |
|---|---|
| 1. Detección y capacidades | Cinco juegos, ocho conjuntos, seis arranques, carpeta P1/P2 mixta y Amiga pendiente distinguida |
| 2. Arranques/retornos repetidos | Dos rondas por edición y otro arranque tras fallo: tres éxitos por cada uno de los seis casos |
| 3. Aislamiento y opciones | Un perfil persistente, defaults globales filtrados por capacidad; ningún arranque cambia los archivos nativos de otro juego |
| 4. Partidas | Ocho slots, v1/v2, previews, frame/edición restaurados, índice perdido reconstruido, reimportación con copia y lectura Windows de BBS Linux |
| 5. Controles/audio automáticos | Rutas observadas sin inyectar coordenadas; PCM audible; mando virtual en los adaptadores que lo implementan |
| 6. Fallos y recuperación | Error antes de ready en los seis casos, arranque posterior, schemas futuros, caché dañada, fuente modificada y runtime ausente; sin falsos éxitos |
| 7. Biblioteca | Selección, teclado, mando virtual/hotplug/foco, CRUD del catálogo, opciones/partidas y seis retornos por el botón Jugar |
| 8. Regresión de Blues | Cámara/HUD/encuadre, transición DOS/Amiga, rewind, slots/importación y bloqueo heredado contra el binario actual |
| 9. Contratos/plataformas/originales | CTest/core sin SDL, validadores C++/JSON y conservación de hashes/tamaños/fechas de originales; rutas Unicode y almacenamiento largo |
| 10. Validación manual | Campañas, escucha, mando físico y pantalla real: pendiente de la checklist inferior |

Antes de la aceptación del usuario, los nueve primeros bloques representaban **90 %**; no es una
estimación de horas. La pasada manual es el bloque final y no se marca por el
hecho de que un test headless pase. Los informes `report.json` conservan el
estado automático y `manual_status: pending`.

## Correcciones surgidas de esta validación

- Se rechaza `--load-slot 0` en el CLI de diagnóstico: antes coincidía con el cero
  interno que significa nueva partida. El selector gráfico ya limitaba 1–8.
- Los tres runtimes traducen errores internos a los códigos del contrato v1.
  Antes podían emitir `E_RUNTIME`/`E_NATIVE`, aceptados por un validador C++ más
  permisivo pero ajenos al enum JSON. Ahora ambos validadores exigen el mismo
  vocabulario y los seis fallos/recuperaciones pasan validación estricta.
  Los mensajes quedan acotados y preservan UTF-8 válido.
- La interfaz de prueba Windows puede usar dummy explícitamente; así el estado
  fullscreen/navegación no se confunde con una prueba de pantalla física.

## Pasada manual para cerrar TH-12

Realizar desde la biblioteca habitual y anotar juego, edición, nivel/lugar y
resultado. Solo hace falta registrar fallos o confirmar las filas cuando estén
completadas; no cambiar el estado de una campaña por una partida de unos minutos.

| Caso | Recorrido y comprobación | Estado |
|---|---|---|
| Blues DOS | Campaña/transiciones; escaleras nivel 2, salida por tubo, habitaciones, enemigos al morir, ascensores y nivel Concierto. Alternar cámara/HUD/zoom; guardar/cargar y rebobinar | Pendiente; continúa la tarea 17 |
| Blues Amiga | Campaña/transiciones; scroll a 1×/60 Hz y bordes, capas del personaje detrás de tubos/columnas, fondo inferior y ascensores de niveles 1/4/5/Concierto. F6 ida/vuelta y ambos HUD | Pendiente; continúa la tarea 17 |
| Jukebox DOS | Campaña: plataformas, enemigos, transiciones/final; teclado, acción/salto/pausa y escucha de música/efectos | Pendiente |
| Prehistorik 2 DOS | Campaña: escaleras/plataformas, enemigos, bosses/transiciones/final; teclado y escucha de música/efectos | Pendiente |
| Fox DOS | Campaña y transiciones/final; comprobar recoger y lanzar un objeto real, saltos, daños, pausa y escucha | Pendiente |
| Prehistorik 1 DOS | Campaña y transiciones/final; garrote contra enemigos, plataformas/scroll, pausa y escucha | Pendiente |
| Dispositivos/pantalla | Mando físico en cada juego que lo admita; hotplug/foco, pausa, fullscreen/F11/Alt+Enter según motor y retorno a biblioteca; sin audio residual | Pendiente |

La diferencia de scroll/opciones de Fox y Prehistorik respecto a Blues se
registra como paridad PX pendiente, no como una función ya implementada.
Los errores de fidelidad, fallos o pérdida de estado sí deben corregirse antes
de aceptar esa fila. La decisión TH-11 de usar procesos separados se conserva.
