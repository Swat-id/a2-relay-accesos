# Rama v5.0.0

Dual-target de red sobre el firmware de accesos SWATID-A2 (base v4.1.0):

| Target | Hardware KinCony | Objetivo de red en v5 |
|--------|------------------|------------------------|
| **A2 clásico** | [KC868-A2 ESP32 + LAN8720](https://www.kincony.com/esp32-4g-gps-arduino-relay.html) | Gestión **WiFi AP/STA** para mantenimiento y conectividad sin cable |
| **A2v3** | [KC868-A2v3 ESP32-S3 + W5500 + socket 4G](https://www.kincony.com/kincony-kc868-a2v3-esp32-s3-2-channel-relay-module-released.html) | Misma base + gestión **GSM/4G** (SIM7600E) |

## Alcance

- Preparar **dos líneas de compilación** PlatformIO (mismo código / feature flags).
- Portar la capa portable WiFi + `net_manager` (+ GSM en S3) desde la guía validada en **SWATID-A2-MODBUS** (`v0.5` GSM / `v0.6` WiFi).
- Mantener operativa la lógica local (Wiegand, relés, códigos, BLE) **sin depender de IP**.
- Planificar, no implementar aún los módulos completos en este commit documental.

## Contexto

- **Base Git:** `v4.1.0` (dual-build BLE `esp32dev` / `esp32dev_ble`).
- **Repo:** [Swat-id/a2-relay-accesos](https://github.com/Swat-id/a2-relay-accesos).
- **Referencia de código a portar:** `/Volumes/SWAT_WORK/01 - DESARROLLOS/04 - CURSOR/A2-MODBUS-1` (`wifi_manager`, `net_manager`, `gsm_modem`, …).

## Documentos de esta rama

| Documento | Descripción |
|-----------|-------------|
| [PLAN-DUAL-TARGET-WIFI-4G.md](PLAN-DUAL-TARGET-WIFI-4G.md) | Análisis, gaps, fases, entornos PIO, riesgos y checklist |
| [GUIA-PORTABLE-WIFI-GSM-RED.md](GUIA-PORTABLE-WIFI-GSM-RED.md) | Guía portable (fuente de diseño) integrada en este repo |
| [MATRIZ-HARDWARE-A2-A2V3.md](MATRIZ-HARDWARE-A2-A2V3.md) | Pines y diferencias A2 vs A2v3 relevantes a accesos + red |
| [AUDITORIA-FIRMWARE-Y-PLAN.md](AUDITORIA-FIRMWARE-Y-PLAN.md) | Auditoría de la baseline v4.1.0 (bugs C1/C2 EEPROM, robustez) y validación del plan |
| [FASE-0.5-SANEAMIENTO.md](FASE-0.5-SANEAMIENTO.md) | Saneamiento implementado: códigos remotos a NVS, EEPROM segura, arranque no bloqueante, sin reboot por MQTT |
| [FASE-0.75-MODULARIZACION.md](FASE-0.75-MODULARIZACION.md) | Estructura en ficheros: net_manager/wifi_manager/gsm_modem, hw_config, eeprom_layout, remote_codes; hoja de ruta de extracción |
| [FASE-1-WIFI-GSM.md](FASE-1-WIFI-GSM.md) | WiFi AP/STA completo (ventana 60 s, web `/wifi`, MQTT type 7) + GSM Fase 1 (FSM AT, PIN, env `esp32dev_4g`) |
| [FASE-2-A2V3-LCD-RTC.md](FASE-2-A2V3-LCD-RTC.md) | Target A2v3 (`esp32dev_s3`, core 3.0.7, W5500) + pantalla SSD1306 de estado + RTC DS3231 |

## Entornos PIO (operativos)

```text
esp32dev / esp32dev_ble  → A2 clásico + WiFi mgmt (BLE en el segundo)
esp32dev_4g              → A2 clásico + WiFi + GSM (SIM en el socket 4G)
esp32dev_s3              → A2v3: core 3.0.7 + W5500 + GSM + LCD + RTC
                           (setup: python3 scripts/bootstrap_pio_s3_platform.py)
esp32dev_s3_ble          → pendiente evaluar en v5.1 (NimBLE vs core 3.x)
```

## Estado

Planificación y documentación listas. Implementación de código: Fases 1–3 del [plan](PLAN-DUAL-TARGET-WIFI-4G.md).