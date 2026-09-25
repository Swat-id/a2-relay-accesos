# Matriz hardware — KC868-A2 vs KC868-A2v3 (accesos + red)

Referencias fabricante:

- A2: [KC868-A2 ESP32 4G/2G relay](https://www.kincony.com/esp32-4g-gps-arduino-relay.html)
- A2v3: [KC868-A2v3 ESP32-S3](https://www.kincony.com/kincony-kc868-a2v3-esp32-s3-2-channel-relay-module-released.html)

Pines de red/GSM alineados con **A2-MODBUS** (`include/config.h`) y foro KinCony.

---

## Comparativa rápida

| Aspecto | KC868-A2 (actual firmware) | KC868-A2v3 |
|---------|----------------------------|------------|
| MCU | ESP32-WROOM (4 MB típ.) | ESP32-S3-WROOM-1 **N16R8** (16 MB + 8 MB PSRAM) |
| Ethernet | **LAN8720** RMII | **W5500** SPI |
| WiFi / BT | Integrado ESP32 | Integrado ESP32-S3 |
| Socket celular | SIM800L / SIM7600 (placa A2 con 4G) | SIM7600E / SIM800L |
| LCD | No (firmware accesos) | SSD1306 I2C (opcional) |
| RTC | No | DS3231 |
| SD | No | SPI microSD |
| Relés | GPIO **15**, **2** | GPIO **40**, **39** |
| DI | GPIO **36**, **39** (firmware actual) | GPIO **16**, **17** |
| RS485 | GPIO 35/32 | On-board MAX485 (pines distintos) |
| Arduino-ESP32 | Proyecto hoy: **2.0.x** (PIO 5.4) | Requiere **3.x** (PIO 6.x) por W5500 |

---

## A2 clásico — red y GSM (referencia)

### Ethernet (firmware accesos actual)

| Señal | GPIO |
|-------|------|
| MDC | 23 |
| MDIO | 18 |
| CLK | 17 (`ETH_CLOCK_GPIO17_OUT`) |
| PWR | 5 (firmware actual; MODBUS usa -1 en algunas revisiones) |
| PHY | LAN8720, ADDR 0 |

### GSM UART (MODBUS / guía portable)

| Señal | GPIO |
|-------|------|
| ESP TX → módem RX | **13** |
| ESP RX ← módem TX | **34** |
| Baud | 115200 8N1, UART2 |

> En **v5 A2 accesos** el foco es **WiFi**, no 4G (`A2_FEATURE_GSM_MODEM=0`). El socket 4G de la placa A2 queda fuera de alcance salvo decisión explícita.

### Accesos (sin cambio de lógica)

| Función | GPIO |
|---------|------|
| Relé 1 / 2 | 15 / 2 |
| Wiegand1 D0/D1 | 33 / 14 |
| Wiegand2 D0/D1 | 4 / 16 |
| DI1 / DI2 | 36 / 39 |

---

## A2v3 — red y GSM

### Ethernet W5500 (SPI)

| Señal | GPIO |
|-------|------|
| CS | 41 |
| INT | 2 |
| RST | 1 |
| SCK | 42 |
| MISO | 44 |
| MOSI | 43 |

### GSM SIM7600E (UART)

| Señal | GPIO | Notas |
|-------|------|--------|
| ESP TX → RXD | **10** | Auto-detect swap con 9 |
| ESP RX ← TXD | **9** | Strapping S3 — cuidado en boot |
| PWRKEY | socket | Arranque ~10–15 s hasta AT OK |

### Relés / DI (distintos del A2)

| Función | GPIO A2v3 |
|---------|-----------|
| Relé 1 / 2 | 40 / 39 (activo HIGH) |
| DI1 / DI2 | 16 / 17 |

### Wiegand en A2v3 (RESUELTO en v5.0.2, validado con lector real 25-Sep-2026)

Mapeo verificado con **sniffer de flancos** en placa real (firma 13 flancos D0
/ 7 flancos D1 al teclear `1111#`). El rotulado del PCB **no** coincide con la
lista "free GPIO" del foro tid=7958:

| Teclado | Conector PCB | Chip ESP32-S3 |
|---------|--------------|----------------|
| Wiegand 1 D0/D1 | GPIO1 / GPIO2 | **18 / 8** (los "TMP1/TMP2 OneWire" del foro) |
| Wiegand 2 D0/D1 | IO4 / IO5 | **4 / 5** (libres, sin pull-up en PCB → pull-up interna) |

Quedan libres los conectores **IO6** e **IO38** (chips 6/38).

⚠️ **El conector rotulado "SDA/SCL/GND/3V3" NO está conectado al MCU** en esta
revisión de PCB (verificado con toques a GND vigilando 25 GPIOs: cero flancos;
el botón BOOT/GPIO0 sí contaba). El bus I2C real (48/47) solo es accesible en
los periféricos soldados (SSD1306/DS3231/24C02).

⚠️ **Alimentar los lectores Wiegand SIEMPRE a 12 V** con GND común con la
placa. Nunca usar el pin 3V3 de ningún conector: un cruce de 12 V con ese pin
destruyó el carril de 3,3 V de una placa (S3 sin enumerar por USB, LED 12 V
encendido).

---

## Implicaciones para PlatformIO

| Target | Platform | Board | Notas |
|--------|----------|-------|-------|
| A2 | Mantener 5.4 **o** subir a 6.8.1 como MODBUS | `esp32dev` | WiFi mgmt cabe; vigilar flash con BLE (~76 % hoy en `esp32dev_ble`) |
| A2v3 | espressif32 **6.8.1** + board custom N16R8 | `kincony_kc868_a2v3` (traer de MODBUS) | W5500 + NimBLE + GSM |

---

**Actualizado:** 18 Sep 2026 · Rama `v5.0.0`
