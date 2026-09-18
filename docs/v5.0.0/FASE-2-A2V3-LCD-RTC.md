# Fase 2 — Target A2v3 (ESP32-S3) + LCD de estado + RTC (implementada)

**Rama:** `v5.0.0` · **Fecha:** 18 Sep 2026
**Módulos nuevos:** [status_display](../../src/status_display.cpp) · [rtc_time](../../src/rtc_time.cpp)
**Compilación verificada (4 entornos):** `esp32dev` 92,4 % · `esp32dev_4g` 92,9 % · `esp32dev_ble` 76,2 % · **`esp32dev_s3` 1.447 KB (22,1 % de app 16 MB)**

---

## 1. Entorno `esp32dev_s3` (Fase 2 del plan)

El firmware **completo** (accesos + WiFi + GSM + LCD + RTC + W5500, sin BLE)
compila para ESP32-S3 con core Arduino **3.0.7**.

### Setup del entorno (una vez por máquina)

```bash
python3 scripts/bootstrap_pio_s3_platform.py
~/.platformio/penv/bin/pio run -e esp32dev_s3
```

- `boards/kincony_kc868_a2v3.json` — placa KinCony N16R8 (16 MB QIO + 8 MB PSRAM OPI), tomada del proyecto SWATID-A2-MODBUS (validada en hardware).
- El bootstrap crea `.pio-kincony-platform` desde `espressif32@6.8.1` añadiendo el paquete de libs que exige Arduino 3.x.
- ⚠️ Compilar con el core PIO del **penv** (`~/.platformio/penv/bin/pio`, Python ≥3.10). El core antiguo bajo Python 3.9 no soporta el builder de Arduino 3.x.

### Pines A2v3 aplicados ([hw_config.h](../../src/hw_config.h))

| Función | GPIO | Notas |
|---------|------|-------|
| Relés 1/2 | 40 / 39 | activo HIGH (igual que el código actual) |
| DI 1/2 | 16 / 17 | |
| Wiegand 1 D0/D1 | **5 / 6** | **PROVISIONAL** — matriz: fijar en laboratorio |
| Wiegand 2 D0/D1 | **38 / 18** | se evita GPIO4 (posible DE/RE del MAX485) |
| RS485 | RX=15, TX=7 | **UART1** (en A2 clásico era UART2) |
| GSM SIM7600E | TX=10, RX=9 | **UART2**, swap auto (verificado en A2v3 real por A2-MODBUS) |
| W5500 (SPI) | CS=41 INT=2 RST=1 SCK=42 MISO=44 MOSI=43 | pulso RST + `SPI.begin` antes de `ETH.begin(ETH_PHY_W5500,…)` |
| I2C placa | SDA=48, SCL=47 | DS3231 @0x68, SSD1306 @0x3C, 24C02 @0x50 |

### Prioridad ETH > WiFi cerrada en S3

`net_manager` llama ahora a `ETH.setDefault()` / `WiFi.STA.setDefault()` en los
eventos de red cuando compila con core ≥3 — la limitación de prioridad
simultánea documentada en Fase 1 queda **resuelta en el A2v3** (en A2 clásico
persiste, solo afecta cuando ETH y STA están levantadas a la vez).

## 2. Pantalla de estado SSD1306 (`A2_FEATURE_LCD`)

128×64, fuente 6×8 (21×8 caracteres), refresco 1 Hz desde `loop()` (~2-3 ms),
detección runtime en 0x3C — sin panel: un log y no-op.

```text
┌─────────────────────┐
│18/09/26  14:32:05   │  fecha + hora (RTC/sistema)
│E:192.168.1.40       │  Ethernet + IP (o "sin enlace")
│W:192.168.1.55       │  WiFi STA + IP (o "--")
│AP:ON 2cli 45s       │  AP + clientes + ventana restante ("fijo" = always_on)
│4G:attached CSQ21    │  estado FSM GSM + cobertura
│R1:ON  R2:off        │  relés (lectura real del pin)
│D1:H D2:L  M:ok      │  entradas digitales + MQTT
│SWATID_XXXXXXXX      │  id del equipo
└─────────────────────┘
```

Librerías: Adafruit SSD1306 + GFX (solo en el env S3).

## 3. RTC DS3231 (`A2_FEATURE_RTC`) — hora propia mantenida

- **Arranque:** si el DS3231 responde y su hora es válida (sin flag OSF), se
  **siembra el reloj del sistema** antes de tocar la red → franjas horarias de
  códigos remotos, logs y LCD tienen hora correcta **sin conectividad**.
- **Mantenimiento:** cuando el sistema obtiene hora fiable (SNTP al llegar IP,
  o sincronización MQTT `message_type: 4`), el DS3231 se reescribe (primera vez
  ~60 s tras sincronizar; después cada 6 h). Protección anti-realimentación:
  si la única fuente fue el propio RTC, no se reescribe.
- **Sincronización MQTT mejorada en A2v3:** el `time_string` ahora fija
  también el reloj del sistema y el RTC (`rtcSetFromLocalString`), no solo la
  cadena de presentación legacy.
- **Criterio de hora:** el RTC guarda **hora local** (coherente con web/LCD).
  Conversión tm→epoch propia independiente de la TZ del sistema (evita el
  doble offset tras `configTime`).
- Estado consultable por MQTT: `message_type 7`, action **`get_rtc`** →
  `{present, osf, seeded}`.
- Sin RTC o pila agotada (OSF): log claro y comportamiento igual que el A2.

## 4. Flags y entornos resultantes

| Env | Board | Core | WIFI | GSM | LCD | RTC | BLE |
|-----|-------|------|------|-----|-----|-----|-----|
| `esp32dev` | esp32dev | 2.x (5.4.0) | ✓ | — | — | — | — |
| `esp32dev_4g` | esp32dev | 2.x | ✓ | ✓ | — | — | — |
| `esp32dev_ble` | esp32dev | 2.x | ✓ | — | — | — | ✓ |
| `esp32dev_s3` | kincony_kc868_a2v3 | **3.0.7** | ✓ | ✓ | ✓ | ✓ | — |

## 5. Plan de pruebas A2v3 (laboratorio)

| # | Prueba | Esperado |
|---|--------|----------|
| 1 | Boot con cable en el W5500 | `ETH.begin(W5500)` OK; IP por DHCP; web accesible |
| 2 | LCD conectada | Splash y pantalla de estado 1 Hz con las 8 filas |
| 3 | RTC con pila y en hora | Log "Hora sembrada desde DS3231"; hora correcta sin red |
| 4 | RTC con pila agotada | Log OSF; tras sincronizar por MQTT/SNTP, RTC reescrito y OSF limpio |
| 5 | Corte de alimentación 1 h | Al arrancar, hora correcta desde RTC (deriva < 2 s) |
| 6 | ETH + WiFi STA ambas up | Tráfico MQTT por **ETH** (`ETH.setDefault`); failover a STA al quitar cable |
| 7 | SIM7600E en socket | `attached`, CSQ en LCD fila 4 |
| 8 | **Wiegand en pines provisionales 5/6/38/18** | Validar lecturas; fijar matriz definitiva y actualizar `hw_config.h` + matriz |
| 9 | RS485 en UART1 (15/7) | Teclado RS485 operativo |
| 10 | Regresión A2 clásico | Envs `esp32dev*` sin cambios de comportamiento |

## 6. Validación en placa A2v3 real (18 Sep 2026) ✅

Firmware `v5.0.0-S3` flasheado por USB nativo y verificado en dos arranques
(inicial + reset software):

| Subsistema | Resultado |
|------------|-----------|
| W5500 Ethernet | ✅ IP DHCP 192.168.5.77, eventos OK |
| MQTT | ✅ conectado en ~80 ms vía W5500 |
| AP WiFi | ✅ SSID `SWATID_A0461CA0ED30`, 192.168.4.1, ventana 60 s (se apaga sola sin clientes — comportamiento de diseño) |
| RTC DS3231 | ✅ hora sembrada al arrancar, también tras reset SW |
| LCD SSD1306 | ✅ inicializada, pantalla de estado 1 Hz |
| GSM (socket sin SIM) | ✅ módulo detectado en UART2 10/9 → `no_sim` (FSM correcta) |
| EEPROM/NVS | ✅ test de arranque OK y persistencia verificada entre reinicios |
| Relés vía web | ✅ activación + timeout + evento MQTT |

### Bugs de hardware detectados en placa y corregidos

1. **Persistencia EEPROM intermitente (crítico):** en core 3.x la EEPROM
   emulada es un blob NVS de 4 KB; con los 20 KB de NVS del
   `default_16MB.csv`, los `commit()` fallaban de forma intermitente y la
   configuración se perdía. **Fix:** tabla propia
   [partitions_s3_16MB.csv](../../partitions_s3_16MB.csv) con **NVS de 256 KB**
   colocado al final del flash — otadata/app0 se mantienen en 0xe000/0x10000
   porque el builder de PlatformIO asume esos offsets fijos (`ESP32_APP_OFFSET`
   y `boot_app0.bin`; mover la app a otro offset deja la placa sin arrancar).
2. **RTC/LCD no detectados tras reset software:** el DS3231 quedaba reteniendo
   SDA a mitad de transacción y el bus I2C colgado. **Fix:** rutina de
   recuperación (9 pulsos SCL + STOP) antes de `Wire.begin` en `rtcTimeBegin`.

Nota de manejo: el primer intento de subida por USB nativo puede fallar
esporádicamente — reintentar (`pio run -e esp32dev_s3 -t upload`).

## 7. Pendiente conocido

- **Pines Wiegand A2v3 provisionales** (prueba 8): la matriz los marca "a fijar
  en laboratorio". Si GPIO18/8 (OneWire) o 38 dan conflicto, quedan libres 4
  (si el DE/RE no lo usa) y los del bus SD (11-14) si no se usa SD.
- Subida a placa: el env usa esptool por USB nativo; si falla, el proyecto
  A2-MODBUS tiene el flujo OpenOCD/JTAG (`esp-builtin`) documentado.
- BLE en S3: fuera de alcance v5.0.0 (NimBLE 1.4 vs core 3.x); evaluar en v5.1.

---

*Fase 2 implementada a nivel de código — pendiente validación en placa A2v3.*
