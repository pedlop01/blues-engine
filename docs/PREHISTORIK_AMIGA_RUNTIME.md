# TH-15: Prehistorik 1 Amiga

Estado: **completada y aceptada por el usuario; 100 %**, el 24 de noviembre
de 2026, tras revisar las correcciones r2–r4. Runtime aceptado: `ce51a15`.

La biblioteca permite iniciar Prehistorik 1 Amiga con simulación y render C++
nativos, campaña de siete etapas y final, música PAT y efectos. Incluye cámara
original/suave, encuadre dinámico, HUD original/fijo, menú de pausa, ayudas,
selector de nivel, ocho partidas con miniaturas y rebobinado con efecto VHS.
Linux ARM64 y Windows x64 usan la misma simulación; no necesitan emulador,
Python ni Unicorn para jugar.

La carpeta local `orig-games/prehistorik2-amiga` contiene **Prehistorik 1**:
71 archivos contrastados con `Prehistorik.adf` en TH-13. No se renombra ni
modifica. Prehistorik 2 no salió para Amiga; TH-16 sigue cancelada. La edición
DOS conserva su proceso Olduvai y su preparación DIET; sus ampliaciones de
experiencia siguen en PX. No se promete cambiar DOS/Amiga durante la partida.

## Arquitectura y procedencia

Es una extensión de [cyxx/blues](https://github.com/cyxx/blues), con un nuevo
adaptador `pre1-amiga-native` aislado en otro proceso. Los sistemas puros
seleccionados de [Olduvai](https://github.com/ksokolowski/olduvai), de Krzysztof
Sokołowski, revisión `a69e9204fc311c8810d696a6b799000d1bcd1642`, GPL-3.0-or-later,
están en `engines/pre1_amiga/logic`. Se conservan licencias, atribución y hashes
de los fuentes originales en `UPSTREAM.json`; los cambios Amiga están separados
del motor DOS. No se copian el render, host SDL o lector del ejecutable DOS.

Los lectores acotados interpretan Hunk/DAT/DUR/MAT/PC1 y las tablas del juego
aportado por el usuario. El Hunk reconocido tiene SHA-256
`d48f952eca0d74547d9e99531b54ac05beaabb42a55203fec510279bd9bd5295`.
No se ejecuta código 68k en el runtime ni se distribuyen tablas comerciales.
El lector PC1 y el secuenciador PAT comparten primitivas con Fox, con dialectos
explícitos: eso no convierte la física de Prehistorik en la de Fox o Blues.

El ritmo normal usa **20 pasos/s**, elegido tras la revisión del usuario.
Opciones → Ritmo permite conservar la cadencia Amiga de **12,5 pasos/s**; la
elección persiste entre sesiones y no altera la velocidad de la música. La
presentación interpola posiciones y cámara entre pasos. En uniones continuas
se conserva el desplazamiento completo sobre 320 píxeles, en vez de recolocar
al personaje en las coordenadas del antiguo cambio de pantalla. Las sondas de
colisión consultan también el terreno vecino; puertas y teletransportes
conservan sus transiciones propias. El zoom muestra
más mapa donde existe continuidad; las cuevas, jefes y secuencia del tronco
conservan su composición propia. El orden interno original de las etapas se
traduce a los números 1–7 del menú.

## Opciones y controles

| Acción | Tecla / opción |
|---|---|
| Movimiento, salto, entrar en cuevas | Flechas; mando direccional / stick |
| Golpear | Espacio; botón de acción configurable |
| Menú de pausa | Esc o P; Start en mando |
| Encuadre | `-` aleja, `+` acerca |
| Rebobinar | Mantener R; Back en mando; hasta diez segundos |
| Pantalla completa | F11 |
| Guardar/cargar | Menú, ocho slots con miniatura de la escena |
| Ayudas | Invulnerabilidad, vidas 0–99, vidas infinitas y selector de nivel |
| Presentación y sonido | Cámara original/suave, HUD original/fijo, ritmo normal 20 / Amiga 12,5 y volúmenes separados |

El menú consume sus entradas y detiene la simulación. Los cambios de nivel,
carga y reinicio vacían el historial de rebobinado. La reproducción marcha
atrás restaura estados completos, con distorsión de cinta; no intenta invertir
muestras de audio. Las preferencias y partidas se escriben en el perfil privado,
nunca junto a los originales. Los guardados incluyen estado de actores, RNG,
colisiones, habitaciones, jefes y secuencias; tienen versión, fingerprint y hash
del contenido, además de hash independiente para la miniatura. Archivos dañados
o incompatibles se conservan y se aíslan.

Una instalación registrada con el catálogo antiguo puede requerir **Verificar**
una vez para actualizar su motor. Se conservan UUID, ubicaciones y perfil al
verificar, añadir de nuevo o localizar la instalación.

## Validación y límites

La evidencia resumida está en
[measurements/th15-final-validation.json](measurements/th15-final-validation.json).
Los originales, volcados de investigación, imágenes y PCM quedan únicamente en
`build/`, fuera de Git y de los paquetes.

| Comprobación | Alcance |
|---|---|
| Recursos | 22 bancos MAT / 997 sprites, 10 PC1 y 13 muestras 8SVX; lectores independientes y hashes entre plataformas |
| Escenas | 76 pantallas exteriores y 53 cuevas: comandos de dibujo ordenados y bitmap de colisión completo frente al original |
| Jugador | 304 casos / 36.480 pasos de terreno y movimiento frente a rutinas originales aisladas |
| RNG y actores | 1.792 llamadas acotadas e inicialización; 76 casos / 9.120 pasos y 40 casos de estados / 4.800 pasos de actores, con jugador distante |
| Jefes | Tres jefes, cuatro patrones cada uno, 2.880 pasos: movimiento, daño, proyectiles, muerte y reaparición |
| Tronco del bosque | 44 + 21 fotogramas, posiciones, humo, colisión final y RNG; guardado y cruce real hacia la salida |
| Audio | Nueve PAT / 540.000 interrupciones: control, volumen y período base frente al original; regresión de los once PAT de Fox |
| Campaña y estado | Siete niveles, 8.400 pasos, 84 puntos de guardado/continuación, 55 habitaciones y siete salidas/final mediante fixtures |
| Integración | Ocho slots, miniaturas, carga, retorno, opciones, movimiento, pausa, rebobinado, audio y recuperación de fallos en Linux/Windows |

Las pruebas de salidas posicionan al jugador o preparan el golpe final: **no
sustituyen una campaña completada manualmente**. Las trazas de actores no
certifican todas las combinaciones de contacto con el jugador. El usuario
ha aceptado el resultado y autorizado el cierre; esa aceptación no añade
recorridos completos ni pruebas de dispositivos a la evidencia automática.
Los informes parciales conservan el porcentaje histórico de su momento;
el resumen final registra el cierre al 100 %.

Existe una corrección de audio deliberada: arpegios fuera de las 36 entradas de
la tabla original podían leer memoria ajena a la tabla. Se extiende el período
por octavas de forma acotada. En la prueba larga de `rik6.pat` afecta a 641
períodos; el control, volumen y período base coinciden, pero no se declara audio
bit a bit idéntico a esa lectura indefinida. Los otros ocho PAT no presentan
esa diferencia en la ventana comprobada.

## Compilar y repetir

```sh
cmake -S . -B build/pre1-amiga \
  -DTITUS_BUILD_PRE1_AMIGA_RUNTIME=ON -DTITUS_BUILD_PRE1_AMIGA_PROBE=ON
cmake --build build/pre1-amiga -j
ctest --test-dir build/pre1-amiga --output-on-failure
build/pre1-amiga/engines/pre1_amiga/titus-pre1-amiga \
  --datapath ../orig-games/prehistorik2-amiga
build/pre1-amiga/titus-pre1-amiga-game-probe ../orig-games/prehistorik2-amiga
python3 tests/check_pre1_amiga_data.py \
  --probe build/pre1-amiga/titus-pre1-amiga-probe \
  --datapath ../orig-games/prehistorik2-amiga --output build/pre1-data-new
```

El launcher activa este runtime por defecto. El adaptador también puede
compilarse solo con SDL2; no necesita los fuentes DOS exportados. Los probes
no se instalan como motores. `tools/generate_pre1_amiga_state.py --output NEW.cpp`
permite revisar la serialización explícita al cambiar los structs de simulación;
los cambios de formato requieren revisar compatibilidad y versión. `tests/check_pre1_amiga_sessions.py` acepta
`--bin-dir`, `--datapath`, `--output` y `--windows` para las sesiones gestionadas.
`tests/check_launcher_sessions.py --pre1-amiga` prueba Jugar y el retorno desde
la biblioteca. Los validadores `tools/validate_pre1_amiga_*.py` son herramientas
de desarrollo: sus oráculos necesitan Unicorn y secciones privadas generadas
por `tools/validate_pre1_amiga_levels.py`, nunca incluidas en la distribución.

## Correcciones tras la primera revisión

Se elimina la recolocación horizontal de los cambios por pantallas al recorrer
uniones continuas. El terreno se recorta a los límites de cada tramo; se pintan
todos los fondos antes de los actores, para que un fondo vecino no los borre.
Los guardados anteriores siguen siendo legibles: la colisión de los vecinos se
reconstruye desde los recursos. Ver [evidencia de las correcciones](measurements/th15-scroll-review-fixes.json).

## Actualizar el paquete de revisión en Windows

Extraer **todo** el ZIP completo de la revisión entregada en una carpeta nueva y abrir
`INICIAR-TITUS.cmd`. Copiar solo `titus-engine.exe` no actualiza los motores:
el lanzador de los dos paquetes tiene el mismo hash, y las correcciones están
en `bin/engines/pre1_amiga/titus-pre1-amiga.exe`. Como referencia, la r2 corregida emite
`build_revision: 1eee7c5edef08ca0de32264ac134c93fc3d0bb4a` en el evento `hello`;
`10178b8` corresponde al motor anterior. Se comprueba también el ZIP extraído,
con un perfil privado, antes de entregarlo. Los perfiles habituales no se
incluyen ni se reemplazan con el paquete.

## r3: actores de los tramos vecinos

Los enemigos, plataformas y animaciones de los tramos contiguos se actualizan
aunque el personaje siga en otro tramo. Se usa un horizonte fijo de hasta tres
tramos por lado, con las mismas barreras de continuidad del render; cubre el
encuadre máximo más un margen. El zoom y la frecuencia de dibujo no cambian
qué simulación avanza ni el orden de llamadas al RNG. Se conserva la cadencia
seleccionada (20 / 12,5 pasos/s) y se interpolan también los actores vecinos.

La posición del jugador se transforma a las coordenadas de cada tramo para la
IA. El modo continuo permite activar un enemigo a la derecha aunque esa
coordenada local sea negativa; el modo de referencia conserva la condición
original. Las cuevas, teletransportes y secuencias especiales no activan
escenarios desconectados. Los contactos y el proyectil global permanecen
ligados al tramo actual; los proyectiles contenidos en cada entidad avanzan
con ella. No se cambia el formato de los guardados: sus listas de actores,
posiciones anteriores y RNG ya conservan lo necesario para cargar o rebobinar.

Comprobaciones específicas: activación y movimiento de un enemigo vecino,
plataforma/interpolación/guardado, un único paso al cruzar, separación por
teletransporte y separación de una cueva respecto al exterior. Evidencia:
[th15-neighbour-validation.json](measurements/th15-neighbour-validation.json).

## r4: paleta de la introducción

Las tres pantallas de introducción (`TITUS`, `TITRE1`, `TITRE2`) usan
directamente la paleta RGB24 que devuelve el lector PC1. Se elimina una
segunda conversión RGB12 que alteraba esos colores. La composición de las
pantallas se centraliza en `picture_frame`, con doce comprobaciones de todos
los píxeles frente a las componentes RGB de los archivos originales (tres
imágenes, escalas 1–4). Se han revisado también las imágenes renderizadas.
Evidencia: [th15-intro-validation.json](measurements/th15-intro-validation.json).
