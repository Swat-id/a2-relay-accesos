# KC868A2 - Sistema de Control de Acceso Dual Wiegand

## Información general

Controlador de acceso KinCony KC868-A2 (ESP32): Wiegand dual, MQTT, web, Ethernet LAN8720, OTA y BLE (v4.x).

| Campo | Valor |
|-------|-------|
| Versión actual | **4.1.0-BLE** — rama `v4.1.0` |
| Anterior | 4.0.0-BLE |
| Fecha | Agosto 2026 |
| Estado | Producción |

## Estructura del repositorio (desarrollo)

```
01 - CURSOR/
├── platformio.ini
├── partitions_ble.csv
├── src/main.ino                 # Código activo
├── firmware/                    # .bin, .sha256, manifest*.json
├── docs/
│   ├── README.md                # Este índice
│   ├── 00 - docref/             # Referencia, versiones, archivo, sketches, csv
│   ├── v3.0/ … v4.1.0/          # Documentación por rama Git
└── .cursor/rules/
```

## Listado de ramas

| Rama | Enfoque | Entorno PIO | Documentación |
|------|---------|-------------|---------------|
| `v3.0` | Sin BLE, EEPROM | `esp32dev` | [v3.0/](v3.0/) |
| `v4.0` | BLE inicial | `esp32dev_ble` | [v4.0/](v4.0/) |
| `v4.1` | Challenge-Response, HKDF | `esp32dev_ble` | [v4.1/](v4.1/) |
| **`v4.1.0`** | Dual-build estable | ambos | [v4.1.0/](v4.1.0/) |

Diferencias entre entornos: **[v4.1.0/ENTORNOS_COMPILACION.md](v4.1.0/ENTORNOS_COMPILACION.md)**

## Documentación de referencia

Índice completo: **[00 - docref/README.md](00%20-%20docref/README.md)**

| Área | Ruta |
|------|------|
| Compilación | [00 - docref/referencia/compilacion/](00%20-%20docref/referencia/compilacion/) |
| Integración BLE/MQTT | [00 - docref/referencia/integration/](00%20-%20docref/referencia/integration/) |
| Mensajería | [00 - docref/referencia/messaging/](00%20-%20docref/referencia/messaging/) |
| Hardware | [00 - docref/referencia/hardware/](00%20-%20docref/referencia/hardware/) |
| API | [00 - docref/referencia/api/](00%20-%20docref/referencia/api/) |
| OTA | [00 - docref/referencia/OTAA/](00%20-%20docref/referencia/OTAA/) |
| Release notes | [00 - docref/versiones/](00%20-%20docref/versiones/) |
| Histórico / sketches / CSV | [00 - docref/archivo/](00%20-%20docref/archivo/), [sketches/](00%20-%20docref/sketches/), [csv/](00%20-%20docref/csv/) |

## Documentación técnica v4.1

| Documento | Descripción |
|-----------|-------------|
| [v4.1/Firmware_description/](v4.1/Firmware_description/) | BLE, MQTT, EEPROM (detalle firmware) |
| [BLE_INTEGRATION_GUIDE.md](00%20-%20docref/referencia/integration/BLE_INTEGRATION_GUIDE.md) | App móvil |
| [MQTT_INTEGRATION_GUIDE.md](00%20-%20docref/referencia/integration/MQTT_INTEGRATION_GUIDE.md) | Backend |
| [IMPLEMENTATION_CHECKLIST.md](00%20-%20docref/referencia/integration/IMPLEMENTATION_CHECKLIST.md) | Checklist v4.1 |

## Compilación

```bash
pio run -e esp32dev          # sin BLE (v3.x)
pio run -e esp32dev_ble      # con BLE (v4.1.x)
pio run -e esp32dev -e esp32dev_ble
```

Manual: [MANUAL_COMPILACION.md](00%20-%20docref/referencia/compilacion/MANUAL_COMPILACION.md)

## Hardware y red (resumen)

- Placa KC868-A2, ESP32, PHY LAN8720, EEPROM 4 KB
- MQTT: `188.245.213.181:1883` (usuario `swatidhome`)
- Web: puerto 80 (admin / admin por defecto — cambiar en producción)
- Serial: 115200

---

**Última actualización**: 26 Agosto 2026  
**Versión del documento**: 2.0 (reorganización `00 - docref`)
