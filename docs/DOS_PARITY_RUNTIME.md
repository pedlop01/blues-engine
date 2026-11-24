# PX-01–PX-08: experiencia común en Fox y Prehistorik DOS

Estado: **PX-01–PX-08 completadas y aceptadas al 100 %** el 24-11-2026.
El usuario confirma: «funcionan perfecto!». No se cierran Blues 17 ni 18.
La aceptación no añade evidencia de campañas completas ni de dispositivos
físicos que no se hayan probado y documentado.

Los adaptadores conservan sus identidades `opentitus` y `olduvai`, sus perfiles y
sus cachés auditadas. Fox usa la lógica DOS de OpenTitus; Prehistorik usa un
núcleo DOS aislado de Olduvai. No se ejecuta la simulación Amiga con gráficos DOS.
Las ediciones Amiga aceptadas en TH-14/TH-15 conservan sus motores separados.

## Funciones implementadas

| Bloque | Resultado |
|---|---|
| PX-01 | Presentación a 30/60/120 FPS; sprites interpolados y cámara suave. La lógica conserva 29 ms por paso en Fox y 18 Hz en Prehistorik. Los vecinos de Prehistorik y los enemigos próximos de Fox siguen activos independientemente del FPS de presentación. |
| PX-02 | Cámara original/suave, encuadre del mapa con `-`/`+`, `0` para restablecerlo, transición gradual y HUD original/fijo independiente del zoom. Cortes en cambios de nivel y teletransportes. |
| PX-03 | Menú centrado con `Esc` o `P`; continuar, opciones, guardar, cargar, controles y biblioteca. Teclado editable y botones de mando en una única pantalla de controles. |
| PX-04 | Música/FX en buses independientes, rampas de volumen y pausa, silencio al rebobinar, cancelación de efectos del futuro y recuperación del dispositivo. MDI/OPL y VOC originales en Prehistorik; secuenciador DOS y dos chips OPL en Fox. |
| PX-05 | Invulnerabilidad, 0–99 vidas, vidas infinitas, selector/reinicio. Fox muestra sus entradas 1–14 y 16; Prehistorik sus siete etapas en el orden de campaña. No se añade selección de personajes a juegos que no la tienen. |
| PX-06 | Ocho slots por juego/edición, miniatura y resumen, carga desde menú y biblioteca, hash de contenido y validación de recursos. Estado de enemigos, objetos, mapa, colisiones, salas, jefes, contadores y RNG; referencias de Fox codificadas como índices comprobados. |
| PX-07 | Hasta diez segundos de historia en memoria, acción visible hacia atrás y efecto VHS. Al continuar se descarta el futuro y se cancelan sus efectos pendientes. La música es acompañamiento: se pausa durante el rebobinado; no se reproduce audio invertido ni se guarda una captura PCM de la canción. |
| PX-08 | Pruebas gestionadas Linux/Windows: preparación y recuperación de errores, ocho slots y miniaturas, carga individual, opciones temporales, teclado/FPS persistentes, audio, pausa y rewind; igualdad del estado tras 180 pasos a 30/60/120 FPS; sondas de niveles y sanitizadores. Las partidas se han cargado en ambos
  sentidos entre Linux y Windows. Informes en `docs/measurements/px-validation.json`. |

Fox conserva las tres pantallas DOS de presentación y el final animado original,
ahora sin bucles que bloqueen el menú. El resumen al completar un nivel entrega
las vidas correspondientes a cada diez bonus. Los flags originales pueden ser
máscaras, como `enemy.trigger == 0x2000`; su almacenamiento sigue siendo de
32 bits y no se trata como un booleano restringido a cero/uno.

El HUD original de Fox conserva la barra de energía temporal del juego DOS;
el modo fijo muestra información persistente. El HUD de Prehistorik conserva
su tipografía y recursos DOS. El selector DOS/Amiga de la biblioteca funciona
antes de iniciar la partida; el cambio de edición durante la acción sigue
siendo una capacidad de Blues y no se anuncia para estos dos juegos.

## Estado, archivos y compatibilidad

- `fox.dos.json` y `prehistorik.dos.json`, versión 1, separados de los formatos
  Amiga y de BBS. Las direcciones de memoria y el relleno de estructuras no se
  escriben en los slots. El estado del generador DOS de Prehistorik se conserva.
- Miniaturas BMP de 320×200, con hash asociado al slot. Guardados dañados o de
  otros recursos se muestran por separado y se conservan sin sobrescribirlos.
- Las preferencias nativas desconocidas se conservan. Las opciones temporales
  del lanzador se restauran al regresar; teclado y FPS elegidos dentro del
  juego son preferencias nativas persistentes.
- Los originales se leen sin modificar. Las extracciones siguen en las cachés
  privadas `fox-lzw-v1` y `pre1-diet-v1`. Las capturas de Fox evitan formatear nombres de campos y no restauran
  el mismo estado en cada render. El formato portable mantiene el mismo orden
  comprobado de campos. Fox ya no requiere generar `titus.conf`
  durante una inspección ni depende de SDL 1.2/SDL_mixer.
- La historia de rewind está limitada a diez segundos; su memoria depende del
  tamaño del mapa y de las entidades. No se conserva esa historia al cargar
  un slot o cambiar de nivel.

## Compilación y pruebas reproducibles

Los lectores mínimos y la síntesis OPL necesarios están incluidos con sus
licencias y procedencia. Los originales no se distribuyen. Para compilar los
adaptadores actuales ya no hace falta exportar los motores completos a `build`:

```sh
cmake -S . -B build/px -DTITUS_BUILD_LAUNCHER=ON \
  -DTITUS_BUILD_FOX_RUNTIME=ON -DTITUS_BUILD_PRE1_RUNTIME=ON
cmake --build build/px -j
cmake --install build/px --prefix "$PWD/build/px-package"
ctest --test-dir build/px --output-on-failure
```

Se necesitan los SDK SDL2/SDL2_ttf y las dependencias ya documentadas de Blues
si se compila también ese motor. Los exports antiguos quedan como referencia
histórica de TH-08/TH-09; `TITUS_BUILD_DOS_PARITY` debe permanecer activado.

```sh
python3 tests/check_dos_parity_sessions.py --bin-dir build/px-package/bin \
  --game fox --datapath /ruta/original/fox-dos --output build/px-test-fox
python3 tests/check_dos_parity_sessions.py --bin-dir build/px-package/bin \
  --game prehistorik --datapath /ruta/original/prehistorik-dos \
  --output build/px-test-pre1
python3 tests/check_dos_parity_render.py --bin-dir build/px-package/bin \
  --fox-data /ruta/cache/fox-lzw-v1 --pre1-data /ruta/cache/pre1-diet-v1 \
  --output build/px-test-fps
```

La prueba `tests/check_dos_save_portability.py` permite repetir la carga cruzada
con los perfiles privados generados por esas baterías, sin tocar perfiles reales.

Añadir `--windows` para ejecutar los EXE desde WSL con perfiles privados Unicode.
Las sondas `titus-fox-dos-game-probe`, `titus-pre1-dos-game-probe` y
`titus-dos-audio-probe` aceptan rutas a datos ya preparados; Fox admite un tercer
argumento `--stress` para recorridos prolongados y restauraciones intermedias.

## Comprobaciones recomendadas antes de la distribución

- Jugar campañas completas: puertas, escaleras, objetos, plataformas, daño,
  jefes, bonus y finales; ambas cámaras y varios encuadres.
- Revisar suavidad y capas con pantalla real a 30/60/120 FPS, especialmente
  límites del mapa y entradas/salidas de salas.
- Escuchar música y efectos durante juego, pausa y rewind; probar mando,
  pantalla completa y desconexión/reconexión del dispositivo físico.
- Guardar/cargar y rebobinar situaciones de combate y puzzles elegidas jugando.

La distribución final y su cierre siguen en la tarea 18 pospuesta. La entrega
para esta revisión es el enlace al EXE dentro de su carpeta completa, sin ZIP.
