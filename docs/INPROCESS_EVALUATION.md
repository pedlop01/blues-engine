# TH-11: evaluación de integración en proceso

**Decisión: mantener todos los adaptadores en procesos separados para la primera
expansión.** Ningún juego cambia su forma de arranque desde la biblioteca.
El piloto se conserva como herramienta de desarrollo, desactivada por defecto.

## Piloto realizado

Se usa Olduvai/Prehistorik 1 DOS, el candidato más cercano al frontend por usar
C++17/SDL2. Un host SDL representativo crea ventana, renderer, textura, evento
propio y subsistemas de audio/mando. Invoca el **mismo bootstrap y motor real**
en el mismo proceso o mediante `Process` en otro proceso. No es una integración
de la interfaz completa del launcher ni un emulador o adaptador de presentación.

Se conservan las validaciones de request, hashes, preparación DIET privada y
bloqueo del perfil. Antes del ensayo, una sesión del producto prepara los datos.
Cada caso usa un request/UUID/logs privados. El driver compara al terminar hashes,
tamaños y fechas de los 462 originales y de los datos preparados.

La secuencia normal → error antes de ready → normal → excepción después de ready
→ normal prueba tres entradas en el mismo host y recuperación tras fallos.
Cada entrada normal llega a gameplay, mueve a derecha/izquierda, salta, ataca,
abre la pausa y entrega PCM audible sin clipping en ese recorrido. La repetición
reinicia explícitamente **nuestro bootstrap**; no demuestra un reset completo de
los globals internos del motor ni de todos sus dispositivos físicos.

Casos adicionales abren audio de host mientras el juego intenta abrir su propio
dispositivo, e inyectan una salida abrupta `_Exit(86)` tras 40 frames de juego.
La caída se ejecuta exclusivamente en un binario de prueba y almacenamiento
temporal. El producto no incorpora esas ramas de fallo.

## Resultado de propiedad y fallos

| Caso | Motor hijo | Motor en proceso compartido |
|---|---|---|
| Retorno normal | Ventana/textura y subsistemas del host siguen válidos | `SDL_Quit()` del motor invalida ventana/textura, audio y mando del host; hay que reconstruirlo |
| Eventos propios del host | Se conservan en su cola SDL | El bucle del motor consume también el evento que no le pertenece |
| Error antes de ready | Sin perder el host | Sin perder el host en el punto de fallo probado |
| Excepción después de ready | El host sigue válido; el SO libera el proceso hijo | El motor deja su ventana viva; el piloto fuerza limpieza SDL y reconstruye el host |
| Salida abrupta después de ready | Se recoge código 86; el host continúa | Termina el host entero; solo queda el marcador previo, sin resultado completo |
| Host con audio abierto, backend dummy | Ambos procesos mantienen su audio | El segundo dispositivo no abre; el motor falla antes de ready y conserva el host |

El último resultado depende del backend dummy; no afirma que todos los drivers
reales solo permitan un dispositivo. Sí muestra que compartir proceso exige un
contrato explícito sobre audio, eventos, hints y propiedad de subsistemas.
En las secuencias hay **0 reconstrucciones del host con proceso hijo**, frente
a **4 en el caso compartido**: tres retornos normales y una excepción.

Fuentes concretas de la observación: `GameSession::run()` destruye su ventana y
llama a `SDL_Quit()` al retornar; su destructor por defecto no ejecuta esa misma
limpieza cuando una excepción evita el final de `run()`. El piloto comprueba los
IDs de ventana y la capacidad de renderizar realmente, sin usar handles que SDL
ya haya invalidado. No se presenta reconstruir el host como preservación de su
estado. Además, el bootstrap de Windows necesita mantener de acuerdo el entorno
del CRT del EXE, el SO y el CRT de SDL para conservar la verificación del bloqueo.

## Rendimiento y alcance de las medidas

Los JSON de [Linux](measurements/th11-linux.json) y
[Windows](measurements/th11-windows.json) conservan resultados y resúmenes.
| Plataforma | Ready hijo / compartido (ms) | CPU del motor hijo / compartido (ms) | Pico host con hijo / motor hijo / compartido (MiB) |
|---|---:|---:|---:|
| linux | 538.63 / 544.98 | 1834.51 / 1995.33 | 18.00 / 32.28 / 33.38 |
| windows | 1240.66 / 1041.47 | 984.38 / 1062.50 | 28.53 / 49.40 / 51.18 |

Son tres ejecuciones normales por modo/plataforma, caché preparada, renderer CPU
y vídeo/audio dummy. Linux es ELF AArch64 y Windows PE x86-64, ejecutado mediante
interop de WSL. No se compara el rendimiento de una arquitectura con la otra.

`ready_ms` va desde la invocación del host hasta el primer cuadro de gameplay:
incluye el arranque del proceso hijo y la validación del request/datos; excluye
el escaneo del catálogo y la preparación DIET previa. En el hijo, observar ready
tiene una granularidad de polling de aproximadamente 5 ms. La duración total
incluye el recorrido y su pausa; no es FPS máximo ni tiempo de carga puro.
CPU usa `getrusage` en Linux y `GetProcessTimes` en Windows, incluidos los hilos
del motor. Los picos residentes son los reportados por cada SO: en modo compartido
incluyen el host, y en modo hijo se muestran separados. No deben sumarse picos
no simultáneos como si fueran una medición de memoria total.

Las mediciones son orientativas, con la carga habitual del host de desarrollo;
parte de las verificaciones de plataforma se ejecutó en paralelo. No permiten
atribuir pequeñas diferencias a la frontera de proceso ni extrapolar rendimiento
con vídeo real, vsync, fullscreen, mandos físicos o la caché TTF del launcher.
El arranque compartido resultó unos 199 ms menor en la muestra Windows; la
muestra Linux no mostró esa diferencia. Estas observaciones no demuestran una
ganancia estable que compense los cambios de propiedad y recuperación requeridos.

## Decisión por adaptador

| Adaptador | Decisión | Trabajo necesario antes de reconsiderarlo |
|---|---|---|
| Blues / Jukebox / P2 | Proceso separado | Contextos y reset completo de globals, ownership SDL/mixer y eliminación de salidas fatales de proceso; regresión de guardados, cámara y física |
| OpenTitus / Fox | Proceso separado | Separar globals y audio SDL 1.2/SDL_mixer/OPL, retirar `exit()` en errores y definir convivencia con SDL2 del frontend |
| Olduvai / Prehistorik | Proceso separado | RAII completo también en errores, recursos SDL prestados, un único despacho de eventos, coordinación de audio y reset de dispositivos/cachés |

La revisión de Blues confirma `SDL_Quit()` en `sys_sdl2.c`, singletons de sistema,
mixer y juego, y `exit(-1)` en `util.c`; OpenTitus conserva salidas fatales en audio.
Estas son observaciones de código, no pilotos de esos dos adaptadores. El piloto
Olduvai enlaza las bibliotecas/licencias inventariadas en sus avisos; el frontend
actual sigue sin enlazarlas. La compatibilidad/licencia del binario completo y
su paquete de fuentes deben resolverse antes de cualquier cambio de ese límite,
sin adelantar la distribución pendiente de la tarea 18.

## Reproducción y siguiente paso

[Instrucciones del piloto](../experiments/inprocess/README.md).
`TITUS_BUILD_INPROCESS_PILOT` es OFF por defecto; no hay entrada de registro,
instalación ni opción nueva para jugadores. Las comprobaciones de símbolos del
frontend confirman que no contiene símbolos `olduvai::` en Linux ni Windows.

TH-11 se cierra con esta decisión, sin migraciones de perfiles ni formato de
partidas. **TH-12** validará la expansión completa en los procesos existentes,
incluidas campañas, dispositivos reales, fallos y regresiones de Blues. La tarea
17 permanece abierta y la distribución 18 continúa pospuesta.
