# TH-10: opciones y partidas de la biblioteca

**Opciones** abre un cuadro con cuatro capas. Guarda los cambios antes de pasar
a otra pestaña; Cancelar conserva los valores anteriores.

| Capa | Alcance |
|---|---|
| Globales | Defaults para los juegos que soporten cada opción |
| Juego | Perfil, juego y grupo de compatibilidad; compartido entre sus ediciones |
| Edición | Excepciones DOS o Amiga del mismo perfil/juego |
| Esta sesión | Solo la próxima ejecución de esa instalación desde esta ventana |

Cada valor puede volver a **Heredar**. El orden es: defaults del adaptador,
opciones nativas importadas, globales compatibles, juego, edición y sesión.
Los botones recorren los valores, incluido Heredar. Música y efectos admiten
0–100; las preferencias importadas mantienen valores intermedios como 42 o 73.
La configuración del frontend (su ventana/pantalla) sigue siendo independiente
de la pantalla completa del juego.

## Capacidades reales

| Motor/juegos | Opciones comunes | Partidas en biblioteca |
|---|---|---|
| Blues DOS/Amiga | Pantalla, música/efectos, botones de acción/salto, cámara, HUD, encuadre, ayudas, vidas, nivel inicial y modo de audio | Ocho slots BBS con miniaturas; carga nativa |
| Jukebox / Prehistorik 2 DOS | Pantalla, música/efectos y botones de acción/salto | Sin slots gestionados |
| Titus the Fox DOS | Pantalla completa; mantiene sus controles/audio nativos | Sin slots gestionados |
| Prehistorik 1 DOS | Pantalla completa; mantiene sus controles/audio nativos | Sin slots gestionados |

Globales puede mostrar ajustes que solo algunos juegos aplican. Las pestañas
particulares muestran únicamente los soportados por ese adaptador. Fox no
ofrece dos buses de volumen independientes en su puente OPL; no se inventan
sliders ni se trasladan trucos, zoom o guardados de Blues a otros motores.

Los botones de mando usan la nomenclatura SDL: A/B/X/Y, L3/R3 y RB. Acción y
salto deben ser distintos. Las direcciones y la pausa siguen siendo nativas; los menús del motor pueden
usar su botón de acción configurado. La navegación del frontend conserva
sus propios controles al cambiar los botones del juego.
Los mandos físicos y otras distribuciones de hardware siguen en la matriz TH-12.
Las pruebas de mando virtual recorren salto/acción de Jukebox y P2, usando X/Y
en lugar de A/B. Jukebox ahora reserva el mismo margen de mezcla que P2 para
que dos buses al máximo no saturen en ese recorrido.

## Persistencia y propiedad

`config/game-settings.json` (schema 1, revisión) contiene globales y grupos por
UUID de perfil/juego/compatibilidad, con base nativa y excepciones de edición.
Se escribe atómicamente bajo bloqueo y se rechaza una revisión desactualizada.
Una versión futura, JSON inválido o valores desconocidos se conservan y se
rechazan. Los archivos INI/JSON privados de los motores mantienen sus claves
propias; el frontend no los edita mientras el proceso nativo está abierto.

Antes de lanzar se resuelven y validan las capacidades. Al cerrar correctamente,
el adaptador traduce los cambios nativos comunes a excepciones de edición.
Los valores que no cambiaron mantienen la herencia. Las excepciones de sesión
no se guardan como defaults, y los ajustes capturados por un BBS cargado tampoco
se convierten en preferencias de futuras partidas.

Un diario privado `native/<engine>/managed-settings-pending.json` permite
restaurar los defaults persistentes en el siguiente arranque si el juego o el
supervisor falló antes de normalizar las preferencias. Solo se retira después
de la recuperación. No modifica los saves. Cambios externos detectados en
preferencias privadas se recogen en el siguiente inicio, comparando con la
última configuración materializada, sin congelar nuevos defaults globales.

## Partidas e importación de Blues

**Partidas** consulta el inspector nativo en un job sin vídeo/audio. En Blues
muestra ocho slots, edición, nivel, vidas y miniaturas cuando existen. Vacíos,
dañados, ilegibles e incompatibles se distinguen; únicamente un slot válido
habilita Cargar. El inspector vuelve a comprobar el formato, CRC y recursos
antes de arrancar; el decoder y restaurador nativos vuelven a validar al cargar.
Se restaura la edición del payload si sus recursos están disponibles.

Los archivos originales `slot01.bbs`–`slot08.bbs`, BBS v1/v2, CRC y previews
128×80 no se convierten ni se envuelven. `save-index.json` es caché reconstruible.
Las imágenes/resúmenes salen del inspector nativo a un sidecar de sesión,
relacionado con el hash del payload; el frontend no interpreta estructuras BBS.
No es necesario conservar esa caché para recuperar las partidas.

La primera importación de Blues sigue siendo automática, atómica y de solo
lectura para el origen. **Partidas → Revisar legado** compara las opciones y
los ocho slots con el Blues clásico (incluidos overrides BLUES_CONFIG_FILE y
BLUES_SAVE_DIR). No sincroniza por sí sola. **Importar con copia** sustituye solo
los archivos presentes en el legado tras confirmar; antes conserva el perfil
completo en `<grupo>.before-import-<uuid>`. Los slots ausentes del legado se
conservan. La instantánea `legacy-initial` original no se sobrescribe. Si falla
la validación no se publica la nueva copia, y si falla la publicación se intenta
restaurar el perfil anterior. En Windows, la copia y retirada de archivos usan
API Win32 con rutas Unicode/UNC, evitando la clasificación de directorios
de MinGW que falla en esas rutas. El origen siempre permanece intacto.

## Comprobación

- `check_settings`: precedencia, herencia, aislamiento, revisión y schema futuro.
- `check_unified_sessions.py`: carga real del frame guardado frente a overrides
  distintos, ocho previews, rechazo de archivos ajenos/dañados, edición guardada,
  preferencias temporales e importación con copia reversible; interfaz de
  opciones/slots/comparación mediante los botones y teclado reales.
- `check_family_sessions.py`: movimiento, salto/acción con mando virtual,
  pausa y PCM final, preferencias propias y rechazo de versiones futuras.
- Regresiones de los otros runtimes, protocolo y catálogo, en Linux y Windows.

Las pruebas usan directorios temporales ignorados y verifican los hashes,
tamaños y fechas de los 462 archivos de `orig-games`. No sustituyen campañas,
escucha ni pruebas físicas pendientes de TH-12/17. La distribución 18 continúa
pospuesta.
