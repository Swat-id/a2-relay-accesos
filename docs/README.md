# KC868A2 - Sistema de Control de Acceso Dual Wiegand

## Información general

Controlador de acceso KinCony KC868-A2 (ESP32): Wiegand dual, MQTT, web, Ethernet LAN8720, OTA y BLE (v4.x).

| Campo | Valor |
|-------|-------|
| Versión en desarrollo | **5.0.1** — rama `v5.0.1` (verificación SIM / 4G) |
| Anterior en curso | **5.0.0** — WiFi A2 + GSM/A2v3 |
| Versión estable | **4.1.0-BLE** — rama `v4.1.0` |
| Fecha | Septiembre 2026 |
| Estado | `v4.1.0` producción · `v5.0.0` base · `v5.0.1` diagnóstico 4G |

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
│   ├── v5.0.0/                  # Dual-target WiFi + 4G
│   └── v5.0.1/                  # Verificación SIM / 4G
└── .cursor/rules/
```

## Listado de ramas

| Rama | Enfoque | Entorno PIO | Documentación |
|------|---------|-------------|---------------|
| `v3.0` | Sin BLE, EEPROM | `esp32dev` | [v3.0/](v3.0/) |
| `v4.0` | BLE inicial | `esp32dev_ble` | [v4.0/](v4.0/) |
| `v4.1` | Challenge-Response, HKDF | `esp32dev_ble` | [v4.1/](v4.1/) |
| `v4.1.0` | Dual-build estable | ambos | [v4.1.0/](v4.1.0/) |
| `v5.0.0` | WiFi (A2) + GSM/4G (A2v3) | `esp32dev*` + `esp32dev_s3*` | [v5.0.0/](v5.0.0/) |
| **`v5.0.1`** | Diagnóstico detección SIM / 4G | `esp32dev_4g`, `esp32dev_s3` | [v5.0.1/](v5.0.1/) |

Diferencias entre entornos v4: **[v4.1.0/ENTORNOS_COMPILACION.md](v4.1.0/ENTORNOS_COMPILACION.md)**  
Plan dual-target v5: **[v5.0.0/PLAN-DUAL-TARGET-WIFI-4G.md](v5.0.0/PLAN-DUAL-TARGET-WIFI-4G.md)**  
Verificación SIM/4G: **[v5.0.1/VERIFICACION-SIM-4G.md](v5.0.1/VERIFICACION-SIM-4G.md)**

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
pio run -e esp32dev          # A2 sin BLE
pio run -e esp32dev_ble      # A2 con BLE (v4.1.x / base v5)
# v5 (cuando existan): pio run -e esp32dev_s3
```

Manual: [MANUAL_COMPILACION.md](00%20-%20docref/referencia/compilacion/MANUAL_COMPILACION.md)

## Hardware y red (resumen)

- **A2:** KC868-A2, ESP32, PHY LAN8720 — [ficha KinCony](https://www.kincony.com/esp32-4g-gps-arduino-relay.html)
- **A2v3 (v5):** ESP32-S3, W5500, socket SIM7600E — [ficha KinCony](https://www.kincony.com/kincony-kc868-a2v3-esp32-s3-2-channel-relay-module-released.html)
- MQTT: `188.245.213.181:1883` (usuario `swatidhome`)
- Web: puerto 80 (admin / admin por defecto — cambiar en producción)
- Serial: 115200

---

**Última actualización**: 18 Septiembre 2026  
**Versión del documento**: 2.2 (rama `v5.0.1` verificación SIM/4G)
