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
