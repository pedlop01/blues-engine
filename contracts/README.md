# Contratos de diseño v1 — TH-03

Estos archivos especifican la siguiente implementación. **No registran juegos,
no crean perfiles y no ejecutan motores.** El estado de los adaptadores del
ejemplo es `planned`; el registro tiene `role=design`.

- [Arquitectura y decisiones](../docs/TITUS_ARCHITECTURE.md).
- [Esquema JSON Schema 2020-12](titus-v1.schema.json): catálogo, registro de motores,
  inspección, launch, eventos, comandos, resultado e índice de slots.
- [Ejemplos](examples): diez documentos ilustrativos con ubicaciones ficticias y
  fingerprints sintéticos. El índice no incluye una partida real.
- [Vector de fingerprint](fingerprint-v1-vector.json): bytes/hash canónicos de un
  manifiesto parcial, para trasladar la definición a las pruebas C++ de TH-04.

Validación de desarrollo, sin SDL ni datos originales:

```sh
python3 tools/validate_contract.py contracts/examples/*.json
make check-contracts
```

Si falta la dependencia, instalar `tests/requirements-contracts.txt` en un entorno
Python de desarrollo. No se requiere Python/jsonschema en el producto previsto.
El validador comprueba también relaciones entre documentos; los contratos no se
reducen a la forma JSON. La implementación C++ deberá pasar los mismos casos al
incorporarse y añadir pruebas reales de archivos, procesos y migración.

TH-10 implementa la acción `load_slot` para Blues y amplía las capacidades con
`controller_bindings` y el grupo `settings.input` para los botones nativos
soportados. El frontend filtra defaults globales por capacidad y el runtime
rechaza opciones ajenas. Los ejemplos siguen siendo fixtures de diseño.

TH-12 unifica los errores emitidos por los tres runtimes con el enum v1; C++
y JSON rechazan códigos internos ajenos al contrato. La matriz valida también
los paquetes de fallo y su arranque posterior, no solo las salidas correctas.

TH-14 incorpora `fox-amiga-native` para `fox/amiga`, con compatibilidad
`fox.amiga.full.audited`, ocho slots `fox.amiga.json` versión 1 y capacidades
propias de cámara, HUD, zoom, ayudas, rebobinado y audio. La selección de motor y
capacidades distingue edición; Fox DOS conserva OpenTitus y cero slots. El
catálogo anterior de Amiga sin motor admite reverificación sin perder perfiles.
Las pruebas reales están en `tests/check_fox_amiga_sessions.py`; los ejemplos
de diseño no se convierten en partidas ni recursos comerciales.
