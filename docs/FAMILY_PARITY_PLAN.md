# Paridad de Jukebox Adventure y Prehistorik 2

Autorizada el 24-11-2026. Extiende TH-07; no reabre PX, Fox/Prehistorik 1 ni las
entregas Amiga aceptadas. Conserva las simulaciones `ja/` y `p2/` de cyxx/blues,
sus recursos originales y su HUD. No existe edición Amiga de Prehistorik 2.

Primera entrega de implementación; **pendiente de revisión jugando**. El bloque
completo permanece abierto: faltan los guardados persistentes y la validación
final de paridad. Los porcentajes no son una estimación de horas restantes.

| ID | Tarea y criterio | Jukebox DOS | Prehistorik 2 DOS |
|---|---|---:|---:|
| PF-01 | Presentación a 30/60/120 Hz, interpolación de cámara/actores y cortes en transiciones; comprobar también activación de actores fuera del encuadre nativo | 85 % | 85 % |
| PF-02 | Cámara clásica/suavizada, zoom real con transición, límites y HUD clásico/fijo usando el arte propio | 90 % | 90 % |
| PF-03 | Menú centrado con Esc/P, controles editables, preferencias aisladas y retorno confirmado a biblioteca | 90 % | 90 % |
| PF-04 | Música/efectos separados, pausa y mute/reanudación de rewind; revisar escucha, rampas y dispositivos | 80 % | 80 % |
| PF-05 | Invulnerabilidad, vidas 1–99/infinitas, selector/reinicio y selección de personaje donde exista | 85 % | 85 % |
| PF-06 | Ocho slots persistentes con miniaturas, estado completo, formato versionado, referencias portables, firma de recursos y carga desde biblioteca | 0 % | 0 % |
| PF-07 | Ocho segundos de rewind, acción/cámara en marcha atrás, efecto VHS y descarte del futuro; completar interacción con guardados | 85 % | 85 % |
| PF-08 | Matriz de niveles, zoom/HUD/FPS, continuidad determinista, memoria y regresiones Linux/Windows; revisión humana y aceptación | 60 % | 60 % |

## Primera entrega

- Host común en `engines/family_host/`; los módulos y sus datos siguen separados.
- La lógica conserva **320×200 y el paso original de 33 ms**. Se dibuja una copia
  temporal del estado con cámara y tamaño de presentación independientes.
  Las mutaciones originales del dibujo se ejecutan una sola vez en la lógica;
  la segunda pasada de presentación se descarta sin modificar la simulación.
- `-` amplía el área visible, `+` acerca y `0` restablece. Ocho pasos, hasta
  832×456, limitado al tamaño real del mapa; transición progresiva y selección clásica/suavizada. Fondos adaptados
  al alto ampliado, bordes de tiles disponibles durante la interpolación.
- HUD clásico y fijo conservan los píxeles/paletas de cada juego.
- Menú de Blues adaptado: selector paginado de **31 entradas jugables de
  Jukebox** (la siguiente entrada es el centinela de final) y **16 entradas de
  P2**, incluidos sus mapas especiales. P2 no ofrece personajes ficticios.
  Jukebox puede volver al selector original y reiniciar el nivel.
- Controles y opciones persisten por perfil/juego. `preferences.json` mantiene
  los ajustes gestionados; `preferences.ini` conserva las teclas nativas.
  Se conservan campos JSON desconocidos y se rechazan versiones futuras.
- Mantener `R` o el hombro izquierdo rebobina hasta ocho segundos. Historial
  acotado en memoria, restauración de variables, mapa mutable, referencias
  internas, paleta y cámara. Se limpia antes de recargar recursos. Tras soltar,
  se descartan el futuro y los efectos pendientes; la pista musical reinicia.
- **No hay guardados en disco todavía.** La biblioteca anuncia cero slots y el
  menú lo explica. Las instantáneas internas no se presentan como un formato
  portable ni se escriben punteros a un archivo.

## Verificación y límites

Pruebas reproducibles con datos propios, sin incluir recursos comerciales:

```sh
cmake --build build/th14-release --parallel 3
ctest --test-dir build/th14-release --output-on-failure
python3 tests/check_family_matrix.py --bin-dir build/th14-release \
  --orig-games ../orig-games --output build/pf-matrix-linux-assert
python3 tests/check_family_sessions.py --bin-dir build/th14-release \
  --orig-games ../orig-games
# Windows: cambiar bin-dir por build/th14-windows y añadir --windows.
```

`check_ja_presentation` y `check_p2_presentation` mantienen las aserciones activas
incluso en Release. Cada etapa recorre 30/60/120 Hz, ambos HUD y los nueve
encuadres; exige igualdad del estado nativo y mapa antes/después de presentar,
y verifica restauración del historial, mapa y selector. No son campañas jugadas.
Las rutas gestionadas comprueban movimiento, salto/acción, pausa, rewind,
preferencias, rechazo de formatos futuros, protocolo y PCM sin saturación;
además verifican que los 462 archivos originales no cambien.

La activación de enemigos conserva por ahora las reglas originales del módulo;
PF-01/PF-08 incluyen revisar los actores que se ven al alejar el zoom. No se
promete todavía una campaña completa, dispositivos físicos, guardados portables
ni paridad cerrada al 100 %. La tarea 17 de Blues continúa abierta y 18 pospuesta.

Entrega de revisión: `build/pf-package-windows-r3/bin/titus-engine.exe`, con todas
sus dependencias al lado; no es un ejecutable autónomo ni una distribución final.

Resultados de la primera entrega: 47 etapas en Linux (31 JA + 16 P2), siete
representativas en Windows (JA 1/16/31; P2 índices 0/5/9/15), ocho CTest,
contratos, menú compartido y rutas gestionadas de ambos juegos en Linux/Windows.
ASan/UBSan/LSan sin incidencias en las pruebas completas de zoom/HUD/FPS e
historial de JA 1 y P2 0. Regresión de Blues: 15 operaciones de sesión y cuatro
sesiones de juego, BBS v1/v2 y ocho previews. Detalle de la entrega en
[pf-review-validation.json](measurements/pf-review-validation.json).

## Revisión r2: parpadeo, fondo y muerte (24-11-2026)

El usuario detecta parpadeo del personaje, mal desplazamiento del fondo y pantalla
corrupta tras morir. Cuatro regresiones específicas reproducen fallos de r1:

- `sprites`: el final del tick borraba la lista antes de que SDL presentara los
  fotogramas intermedios. Se conserva hasta preparar el siguiente tick. La
  corrección compartida también se aplica a P2; el menú mantiene la escena.
- `resize`: al volver a 320×200 se conservaban las dimensiones del último zoom.
  Ahora se invalida esa caché al entrar y se reconstruye la vista tras recargar.
  El backend también recorta de forma segura bitmaps mayores que su destino.
- `backdrop`: el fondo ya compuesto se desplazaba y volvía a su origen cada tick.
  Jukebox separa el fondo original fijo de los tiles transparentes interpolados.
- `music`: reanudar la música después del rewind cargaba el módulo sobre el
  búfer de tiles/mapa. Se preserva y restaura ese búfer durante la carga.

Las animaciones bloqueantes de transformación/pérdida del poder actualizan su
instantánea en cada frame, conservando el dibujo de la secuencia original.
Dos desplazamientos de enteros negativos pasan a multiplicaciones equivalentes
para evitar comportamiento indefinido en la animación/velocidad nativas.

`check_jukebox_regressions.py` prueba estas rutas y **32 muertes/reapariciones
reales del bucle original en el nivel 1**, con y sin poder, 60/120 Hz, ambos HUD y zoom
0/1/4/8. Comprueba que el jugador continúa moviéndose tras reaparecer, las vidas,
las dimensiones y los sprites de la animación. La prueba del fondo compara
píxeles durante el desplazamiento; la de música exige conservar todo el búfer.

```sh
python3 tests/check_jukebox_regressions.py --bin-dir build/th14-release   --datapath ../orig-games/blues2-dos --output build/pf-r2-regression-linux
# Windows: build/th14-windows y --windows.
# Sanitizadores: build/th14-sanitize (ASan/UBSan/LSan).
```

R2 corrige estos errores; **no cierra PF ni implementa los guardados pendientes**.
La aceptación jugando sigue pendiente. La evidencia de r1 se conserva como
histórica en el informe anterior.

Evidencia de r2: [pf-r2-validation.json](measurements/pf-r2-validation.json),
con las reproducciones anteriores, matrices, sanitizadores y comprobación del
paquete Windows final (cinco motores y revisión `afe8a49-pf2`).

## Revisión r3: precisión de cámara y bordes (24-11-2026)

El usuario valida que r2 elimina el parpadeo, pero todavía detecta oscilaciones
y cortes en el lado izquierdo. Se reproducen dos problemas adicionales:

- La cámara se suavizaba en coma flotante, pero se entregaba al renderer la
  posición ajustada a grupos de cuatro píxeles del raster original de Jukebox.
  Una prueba detecta `257.2` calculado frente a `256` dibujado. Ahora el raster
  conserva sus restricciones y la presentación recibe la posición fraccionaria;
  tiles y anclas de los sprites comparten la misma transformación interpolada.
- El margen fijo de 32 píxeles no cubría algunos intervalos válidos de cámara.
  La prueba reproduce un borde sin mapa durante una transición de menos de
  128 píxeles. El margen se calcula para todo el intervalo y conserva su máximo
  hasta recargar el nivel, evitando huecos y realocaciones continuas.

La cámara clásica y la simulación original no cambian. P2 y Blues mantienen
su API de desplazamiento anterior; la nueva proyección se activa en Jukebox.
La escena del menú conserva el desplazamiento fraccionario al congelarse.

La regresión incorpora ocho grupos: sprites, resize, backdrop, music, death,
scroll, camera y motion. Compara píxeles de ambos bordes a 1×/2×/3×, fases
intermedias y posiciones finales, e interpola las anclas del personaje. Recorre
cuatro rutas nativas de 360 fotogramas moviéndose a derecha/izquierda a 60/120 Hz,
además de las 32 muertes y reapariciones de r2. Las lecturas de píxeles de la
prueba gráfica se hacen **antes** de `SDL_RenderPresent`, como exige SDL, para
no comparar un backbuffer descartado por Direct3D.

R3 sigue pendiente de revisión jugando; no cierra PF ni añade guardados.

Evidencia de r3: [pf-r3-validation.json](measurements/pf-r3-validation.json),
con reproducciones, ocho grupos de regresión, matrices Linux/Windows,
sanitizadores y pruebas gráficas de cámara/fondo con Direct3D 11.
