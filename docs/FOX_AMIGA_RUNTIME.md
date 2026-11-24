# TH-14: Titus the Fox Amiga

Estado: **completada, 100 %, aceptada por el usuario el 24 de noviembre de 2026**. La biblioteca
abre la edición Amiga mediante `fox-amiga-native`, un proceso C++/SDL2 propio.
Incluye simulación, render, campaña y final, música y efectos, opciones comunes,
rebobinado y ocho guardados con miniaturas. El cierre incorpora la aceptación jugable del usuario; las pruebas automáticas
no certifican por sí solas todas las rutas ni dispositivos físicos.

## Progreso

| Bloque | Resultado | Completado |
|---|---|---:|
| Recursos y render | Descompresión nativa, mapas, sprites, HUD, pantallas, animación de tiles, prioridades y Copper | 20/20 |
| Inicialización y consultas | POIL, colisiones, puertas, salas, puntos de control y condiciones de salida | 10/10 |
| Simulación e interacciones | Personaje, objetos, agarre/lanzamiento de enemigos, 19 comportamientos, proyectiles, plataformas, daño y vidas | 10/10 |
| Audio | 11 canciones PAT, cuatro canales Paula, cinco efectos, volúmenes independientes y pausa | 20/20 |
| Adaptador y opciones | Protocolo gestionado, biblioteca, preferencias, mando, cámaras/HUD, zoom, ayudas, slots y rebobinado | 20/20 |
| Validación | Linux, Windows, sanitizadores, regresión automática y aceptación del usuario | 20/20 |
| **Total** | **Cerrada tras la aceptación del usuario** | **100/100** |

## Abrir y jugar

En la biblioteca, verificar de nuevo la carpeta `orig-games/fox-amiga` si fue
registrada antes de instalar este adaptador. Seleccionar **Titus the Fox → Amiga →
Jugar**. Los perfiles antiguos con motor ausente siguen siendo legibles y se
actualizan mediante la verificación; no se mezclan con Fox DOS. La migración
actualiza estado, motor y compatibilidad juntos; conserva UUID, ubicaciones y
archivos de perfil. Se prueba también al volver a añadir o localizar la carpeta.

Paquetes locales de revisión, sin datos comerciales:

- Linux ARM64: `build/th14-package-linux/run-titus-engine.sh`.
- Windows x86-64: `build/th14-package-windows/bin/titus-engine.exe`.

Los paquetes conservan los otros runtimes ya disponibles. Linux requiere SDL2,
SDL2_ttf y libmodplug; el script de revisión configura las bibliotecas SDL 1.2/SDL_mixer
locales que necesita Fox DOS. El
paquete Windows contiene sus DLL. El empaquetado público definitivo sigue en
la tarea 18. Para arrancar solo el motor nativo:

```sh
build/th14-release/engines/fox_amiga/titus-fox-amiga \
  --datapath ../orig-games/fox-amiga
```

| Control | Acción |
|---|---|
| Flechas | Mover, saltar/subir y agacharse/bajar |
| Espacio | Acción; abajo + acción recoge objetos o enemigos transportables |
| Abajo mantenido | Entrar en una puerta tras 30 ticks, sin carga |
| Esc / F1 / P | Menú centrado y pausa del audio |
| R mantenida / Back del mando | Rebobinar hasta diez segundos con efecto VHS |
| − / + | Ampliar/reducir el área visible en ocho pasos hasta 3× |
| F11 | Pantalla completa |
| Enter | Confirmar; avanzar tras completar un nivel o reiniciar tras el final |

El menú principal contiene continuar, opciones, guardar, cargar, controles y
volver a la biblioteca. Controles aparece una sola vez. Las opciones incluyen
cámara original/suavizada, HUD original/fijo, invulnerabilidad, vidas finitas o
infinitas, cantidad de vidas, selector 1–14/16, música/efectos y botones de mando.
La cámara y los actores se interpolan entre ticks de simulación de 50 Hz. El
zoom modifica el área del mundo visible; las habitaciones conservan su encuadre.

## Datos y ejecución

Se admite el conjunto auditado de 89 archivos, con `main` SHA-256
`18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45`.
Los 65 recursos comprimidos coinciden con la salida del descompresor original.
Hay quince mapas: 01–14 y 16; no se inventa un nivel 15 ausente en estos datos.
El final sucede en el mapa 16 después del 14.

Los lectores acotados implementan LZW/Huffman, BRIK, SPR, PC1/EHB, POIL, INDIC y
PAT con sus muestras externas. Extraen en memoria las tablas de reglas del
Hunk empaquetado reconocido. No distribuyen tablas extraídas ni ejecutan código
68k durante el juego. Python, Unicorn, amitools y DOSBox no son dependencias del
runtime. Las revisiones no reconocidas se rechazan explícitamente.

Se reproducen las prioridades originales: Fox usa sprites hardware por delante
del playfield (`BPLCON2=0x24`), con paleta y anclaje propios; objetos, enemigos y
plataformas usan los cuatro planos de su banco. COLOR14 cambia cada línea según
el Copper original, con su alternancia de siete líneas, anclado a la pantalla.
La interpretación de BPLCON2 sigue el [Amiga Hardware Reference Manual](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0159.html).
El HUD original conserva los caracteres monocromos INDIC y sus segmentos de
energía. El final usa las imágenes originales de beso, corazón y «The End».

Se conserva la economía de bonus: energía sobrante acumula créditos; diez
créditos al terminar el nivel producen una vida. El estado incluye salas,
puertas, puntos de control, actores, proyectiles, mapa mutable y progreso del
recuento de bonus. Cuatro registros de plataformas de estos datos tienen IDs de
sprite fuera del banco: se desactivan con límites explícitos; las pruebas de
referencia aplican la misma restricción en esos registros.

## Adaptador, preferencias y guardados

La inspección no inicializa vídeo/audio ni prepara copias de los originales.
`ready` se emite después de cargar datos y abrir el dispositivo SDL de audio.
El supervisor conserva el bloqueo de perfil y exige resultado y salida del
proceso para recuperar la biblioteca. También se prueban fallo previo a ready,
resultado ausente y posterior recuperación.

Motor: `fox-amiga-native`. Compatibilidad: `fox.amiga.full.audited`.
Workspace: `profiles/<perfil>/games/fox/fox.amiga.full.audited/native/fox-amiga-native`.
Fox DOS conserva `fox.full.audited` y OpenTitus, con sus propias capacidades.
La configuración admite las capas global/juego/edición/sesión; los overrides
temporales se restauran al cerrar. Las preferencias futuras se preservan y
rechazan, sin sobrescribirlas.

Hay ocho `slot-N.json` y sus BMP de 320×176. Se guarda el estado completo,
versión y SHA-256 del payload; el documento vincula también el SHA-256 de la
miniatura. La biblioteca muestra nivel, vidas y miniatura reducida a 128×80.
Un slot dañado o de otra instalación no bloquea los demás y se conserva.
No son guardados de OpenTitus, del juego Amiga original ni de Blues.

## Validación y límites de la evidencia

El resumen distribuible está en
[measurements/th14-final-validation.json](measurements/th14-final-validation.json).
Las imágenes, PCM, trazas y snapshots comerciales permanecen solo en `build/`.

- Referencia original aislada: 453 registros iniciales de enemigos; 90.000 ticks
  de IA y 9.000 ticks de plataformas, con registros completos. Movimiento/daño:
  47 casos y 2.934 ticks. Objetos: 182 casos y 9.609 operaciones.
- Audio: 11 canciones y 132.000 interrupciones con efectos; trazas y PCM idénticos
  en Linux, Windows y ASan/UBSan. El callback SDL también produce audio no nulo
  sin saturación en las pruebas de sesión.
- Copper: 2.816 colores de línea coinciden con la rutina original en 16 mapas.
- Simulación/render final: 1.200 ticks en cada uno de los quince mapas y 6.000
  ticks recorriendo actores en cada uno de los catorce niveles jugables. Se
  comparan estado, hashes de imagen y restauración de guardados entre las tres
  compilaciones; ambos renderizadores coinciden en coordenadas enteras.
- Campaña: catorce salidas hasta el mapa 16 y cuatro estados del final. Esta
  prueba posiciona al personaje en las salidas y prepara sus requisitos; **no es
  un recorrido humano completo** ni demuestra todas las rutas de cada nivel.
- Sesiones Linux/Windows: ocho slots y miniaturas, carga desde biblioteca,
  opciones temporales, rutas Unicode, movimiento/salto, pausa, rebobinado,
  captura de audio, retorno y recuperación. Regresión de Blues y de Fox DOS /
  Prehistorik DOS con sus adaptadores separados.
- Pruebas de contrato, unidades C++ y sanitizadores; 462 originales conservan
  bytes, tamaños y fechas. No se certifican audición humana, mandos físicos ni
  todas las máquinas Windows por pasar las pruebas bajo WSL.

Para revisión: jugar con y sin ayudas, probar salas/objetos/enemigos/elevadores,
comparar cámara y HUD, guardar/cargar, rebobinar, escuchar música/efectos y revisar
el final. El usuario ha aceptado TH-14 y autorizado su cierre al 100 %; se
conservan los límites de cobertura automática descritos arriba.
La paridad PX de los otros motores sigue pendiente; TH-15 y Blues 17 permanecen
abiertas, TH-16 está cancelada porque Prehistorik 2 no salió para Amiga.

## Compilar y repetir pruebas

```sh
cmake -S . -B build/fox-amiga -DTITUS_BUILD_LAUNCHER=ON \
  -DTITUS_BUILD_FOX_AMIGA_RUNTIME=ON -DTITUS_BUILD_FOX_AMIGA_PROBE=ON
cmake --build build/fox-amiga -j
ctest --test-dir build/fox-amiga --output-on-failure
python3 tests/check_engine_contracts.py
python3 tests/check_fox_amiga_sessions.py --bin-dir build/fox-amiga \
  --datapath ../orig-games/fox-amiga --output build/fox-amiga-sessions-new
python3 tests/check_launcher_sessions.py --bin-dir build/fox-amiga \
  --orig-games ../orig-games --headless --fox-amiga
```

Las pruebas de referencia requieren snapshots privados obtenidos mediante las
herramientas de investigación documentadas en TH-13. El probe es diagnóstico,
no instalado ni anunciado como juego; la edición jugable usa otro ejecutable.
Los lectores y el adaptador nativo están bajo GPL-3.0-or-later; véase
[NOTICE](../engines/fox_amiga/NOTICE.md). Se conserva la atribución a cyxx/blues
sin asignar una licencia nueva al código heredado.
