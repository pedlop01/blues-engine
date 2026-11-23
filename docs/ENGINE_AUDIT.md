# Auditoría técnica y primeras pruebas de motores

Fecha: **23 de noviembre de 2026**. Corresponde a TH-01 y TH-02 del
[plan de expansión](TITUS_PLATFORM_PLAN.md). Datos examinados en
[GAME_INVENTORY.md](GAME_INVENTORY.md).

## Alcance comprobado

Entorno: Linux AArch64, GCC, SDL2 **2.30.0**; SDL 1.2 mediante
**sdl12-compat 1.2.68-2** para el ensayo de OpenTitus. Los motores externos
se descargaron y compilaron en `/tmp/titus-engine-audit/`; no se incorporaron
sus fuentes o binarios al repositorio. No se modificaron los originales.

| Motor y datos | Resultado observado | Pendiente antes de ofrecerlo en la biblioteca |
|---|---|---|
| Nuestra base: Blues DOS y Amiga | Arranque, imagen no uniforme y cierre solicitado al motor: correctos. | Continúa la validación manual 17; este ensayo no la sustituye. |
| Nuestra base: Jukebox Adventure DOS | Arranque y cierre correctos tras corregir lectura de nombres de recursos. Probado con originales en mayúsculas y alias en minúsculas. | Partida, controles, audio, salida al futuro lanzador y compilación Windows multijuego. |
| Nuestra base: Prehistorik 2 DOS | Arranque, imagen no uniforme y cierre solicitado: correctos con la carpeta mixta actual. | Partida, controles, audio y compilación Windows multijuego. |
| OpenTitus: Fox DOS | Compila con ajustes temporales; ensayo sin audio: una presentación SDL y salida limpia al recibir `SDL_QUIT`. | Audio desde fuentes, portabilidad, entrar en partida y recorrido; todavía no es un motor integrado. |
| Olduvai: Prehistorik 1 DOS | Compila; nuestros datos no cargan. El nombre alternativo permite llegar al cargador, que rechaza las tablas del EXE. | Identificar/adaptar la variante `PRE1.EXE` antes de probar jugabilidad. |

Los ensayos nativos usan vídeo/audio `dummy`: comprueban carga inicial,
contenido dibujado y liberación normal de recursos. **No verifican sonido audible,
jugabilidad ni fidelidad visual**. No se ha validado todavía otro juego Amiga.

## Revisiones, dependencias y procedencia

| Proyecto | Revisión examinada | Tecnología / licencia observada |
|---|---|---|
| [cyxx/blues](https://github.com/cyxx/blues), nuestra extensión | Punto recuperable `dd07383`, etiqueta `blues-baseline-2026-11-23`; cambios de este hito encima | C, SDL2, libmodplug, Nuked OPL3. El snapshot heredado no incluye licencia general: véase [CREDITS.md](../CREDITS.md). |
| [OpenTitus](https://github.com/stople/OpenTitus) | `6540e8f2867f99aff54ad1a710ae6379ecb9c590` | C, SDL 1.2/SDL_mixer, OPL de Chocolate Doom. README y cabeceras: GPL-3.0-or-later; texto `gpl3.txt`. |
| [Olduvai](https://github.com/ksokolowski/olduvai) | `a69e9204fc311c8810d696a6b799000d1bcd1642` (0.9.11) | C++17, CMake ≥3.21, SDL2 y componentes de audio. GPL-3.0-or-later; revisar también `THIRD-PARTY-NOTICES.md` y `licenses/` al distribuir. |

Se mantienen separados los motores durante esta evaluación. Antes de combinar
código o publicar paquetes habrá que resolver la licencia del código heredado y
conservar los avisos, fuentes y condiciones de cada dependencia. Esta auditoría
no asigna una licencia nueva al repositorio ni convierte los datos comerciales
en recursos distribuibles.

## Hallazgos que condicionan los adaptadores

### Jukebox Adventure

El detector encontraba `JARDIN.EAT`, pero el descompresor abría literalmente
`font256.eat` y fallaba en Linux ante `FONT256.EAT`. Ahora recibe directorio y
nombre separados y usa `fopen_nocase`, como los otros cargadores del proyecto.
Esto cubre también el resto de recursos EAT, sin renombrar los originales.

### Prehistorik 2 y detección de instalaciones

La carpeta DOS contiene los recursos de Prehistorik 2 y una copia idéntica de
`PRE1.EXE` y los cuatro `FILESA/B.CUR/VGA` de Prehistorik 1. El arranque actual
elige Prehistorik 2 por `MOTIF.SQZ`; eso no identifica todas las instalaciones.
El detector futuro deberá evaluar conjuntos completos y admitir dos resultados
lógicos para una misma carpeta, con compatibilidad independiente por motor.

El detector actual declara tamaños en su tabla pero solo comprueba existencia.
No debe reutilizarse como verificación suficiente de formato/variante en TH-04.

### Olduvai y nuestro ejecutable de Prehistorik 1

El cargador examinado admite `HISTORIK.EXE` o `PREH.SQZ`, no `PRE1.EXE`.
En una carpeta temporal se enlazaron los cuatro FILES y se creó el alias
`HISTORIK.EXE → PRE1.EXE`. No se alteró el archivo original.

Nuestro EXE mide **77839 bytes**, SHA-256
`b03ce6913d07a4c5b66dfc4843d185ffdff6561e9bf91f3333b096016690406f`.

El error observado con el alias fue:

```text
game data: EXE tables not installed (read past end of executable)
game: failed to parse game files ...: read past end of executable
game: could not load level data ...
```

El proceso devuelve **0 incluso cuando no carga el nivel**: un adaptador debe
comprobar el diagnóstico de carga, no solo el código de salida. El alias por sí
solo no resuelve la compatibilidad. Falta determinar el formato/variante y si
necesita desempaquetado; no se atribuye todavía a un compresor concreto.

### OpenTitus

- La biblioteca `opl/libopl_linux.a` contiene objetos **ELF x86-64**; no enlaza
  nativamente en AArch64. Su documentación remite a Chocolate Doom 1.6.0 y un
  parche local de DBOPL. El adaptador debe compilar audio desde fuentes.
- `uint32` e `int32` usan `long`, de 64 bits en este entorno. Para el ensayo
  temporal se sustituyeron por `uint32_t` e `int32_t` de `<stdint.h>`.
- Se compiló con `-std=gnu89 -fcommon`, SDL 1.2 compatible con SDL2 y **audio
  desactivado**. Se protegió la llamada incondicional a `initaudio` y se añadió
  un stub temporal de `startmusic`; el interruptor de audio original no basta.
- Se generó `titus.conf` en una carpeta temporal con rutas absolutas a Fox DOS.
  El programa depende de este archivo en su directorio de trabajo.
- El compilador informa de declaraciones antiguas y posibles desbordamientos
  de formato. Quedan por revisar antes de integrar; arrancar la introducción
  no valida la seguridad o jugabilidad del motor completo.

## Repetir las pruebas

### Motores ya incluidos

Desde el repositorio, con SDL2/libmodplug de desarrollo y los datos propios:

```sh
bash tests/check_engine_startup.sh ../orig-games
```

El script compila `build/tests/check_engine_startup`, prueba las cuatro carpetas
y repite Jukebox con alias en minúsculas. Cada proceso recibe una solicitud de
salida tras 2,5 segundos y tiene límite externo de 20 segundos. Comprueba que
se presenten píxeles de colores distintos y que termine normalmente después
de solicitar salir. No lee/escribe las preferencias del jugador. Los alias se
eliminan al salir; las carpetas originales solo se leen.

### Olduvai

En una copia de trabajo externa fijada al commit anterior:

```sh
cmake -S /tmp/titus-engine-audit/olduvai -B /tmp/titus-engine-audit/olduvai-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/titus-engine-audit/olduvai-build -j4
```

Con los alias de datos descritos arriba en `pre1-staging/`, el ensayo acotado es:

```sh
XDG_CONFIG_HOME=/tmp/titus-engine-audit/xdg SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout 15s /tmp/titus-engine-audit/olduvai-build/olduvai \
  --game-dir /tmp/titus-engine-audit/pre1-staging --play --level 1 \
  --display-mode cpu --play-frames 30 --sound-card off
```

### OpenTitus

El ensayo fue exploratorio: fuentes y cambios temporales permanecen en
`/tmp/titus-engine-audit/opentitus/`, no constituyen todavía una receta de
distribución. Se extrajeron los paquetes `libsdl1.2-dev`/`libsdl1.2debian` a
`sdl12-root/`, sin instalar dependencias del sistema. Tras los ajustes anteriores,
la compilación usa todos los `src/*.c`, `opentitus.c`, el stub de audio, las
cabeceras y `-lSDL -lm` del SDK local. El ensayo interceptó `SDL_Flip` para contar
presentaciones y `SDL_PollEvent` para entregar `SDL_QUIT` a los tres segundos.
Resultado: `flips=1`, solicitud entregada y salida **0**; sin audio. TH-02 debe
convertirlo en un build mantenible con sonido y recorrido real.

## Siguiente paso

Completar TH-01 con identificación de las variantes pendientes; continuar TH-02
con partidas de referencia y audio. Resolver el EXE de Prehistorik 1 y el build
completo de Fox antes de dar por admitidos esos juegos. TH-03 concretará el
contrato del lanzador, conservando estos motores como procesos separados en la
primera etapa y las capacidades exclusivas de Blues dentro de su adaptador.
