# Rama v4.1.0

Rama de estabilización sobre v4.1: **ambos entornos PlatformIO compilan** desde el mismo `src/main.ino`, y la documentación de diferencias queda centralizada.

## Alcance

- Corrección dual-build (`esp32dev` + `esp32dev_ble`)
- Forward declarations BLE para prototipos PlatformIO
- Protección `#ifdef ENABLE_BLE` en `loop()`
- Reorganización documental: referencia e histórico en `docs/00 - docref/`

## Contexto

- **Git**: rama `v4.1.0`
- **Entornos**: `esp32dev` (v3.0.2) y `esp32dev_ble` (v4.1.0-BLE)
- Documentación técnica de protocolo: [docs/v4.1/](../v4.1/)
- Referencia estable: [docs/00 - docref/](../00%20-%20docref/)

## Documentos

| Documento | Descripción |
|-----------|-------------|
| [ENTORNOS_COMPILACION.md](ENTORNOS_COMPILACION.md) | Diferencias `esp32dev` vs `esp32dev_ble` |
