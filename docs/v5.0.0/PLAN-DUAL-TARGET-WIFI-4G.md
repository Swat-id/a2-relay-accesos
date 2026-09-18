# Plan dual-target — WiFi (A2) + 4G (A2v3)

**Rama:** `v5.0.0`  
**Producto:** SWATID A2 Relay Accesos  
**Referencia de portación:** [GUIA-PORTABLE-WIFI-GSM-RED.md](GUIA-PORTABLE-WIFI-GSM-RED.md) (SWATID-A2-MODBUS v0.5 / v0.6)  
**Hardware:** [MATRIZ-HARDWARE-A2-A2V3.md](MATRIZ-HARDWARE-A2-A2V3.md)

---

## 1. Objetivo de producto

| Target | Hardware | Enfoque v5.0.0 | Motivo |
|--------|----------|----------------|--------|
| **A2 clásico** | ESP32 + LAN8720 + opcional SIM800L/SIM7600 | **WiFi AP/STA** + `net_manager` | Placa con menos flash/RAM; facilitar mantenimiento vía AP `192.168.77.1` sin cable |
| **A2v3** | ESP32-S3 N16R8 + W5500 + SIM7600E | **GSM 4G** (Fase 1 AT/estado) + WiFi opcional + `net_manager` | Más capacidad; gestión 4G (registro, CSQ, PIN, APN); datos MQTT por PPP = Fase 2 |

Reglas de campo (herencia guía):

- Relés / accesos / lógica local **no dependen** de red.
- MQTT espera `netHasConnectivity()` (no reiniciar MCU al caer ETH).
- WiFi/GSM en `loop()` con ticks ≥ 1 s (no bloquear setup ni I/O crítico).
- Feature flags por entorno PlatformIO.

---

## 2. Estado actual del firmware (baseline `v4.1.0`)

| Área | Situación |
|------|-----------|
| Sketch | `main.ino` monolítico (~5900+ líneas) |
| Red | Ethernet (LAN8720 A2); flags `ethConnected`; MQTT vía cliente TCP clásico |
| WiFi | No hay `wifi_manager` / AP de configuración |
| GSM | No hay `gsm_modem` |
| Web | Servidor HTTP propio (Basic auth) |
| BLE | Entorno `esp32dev_ble` (`ENABLE_BLE`) |
| Build | Solo `esp32dev` / `esp32dev_ble` (ESP32 clásico) |
| Docs red | Histórico en `docs/00 - docref/archivo/` (análisis WiFi/4G previos, no módulo portable) |

**Gap respecto a la guía portable:** faltan las 3 capas (`wifi_manager`, `net_manager`, `gsm_modem`) y el entorno S3.

---

## 3. Arquitectura objetivo (3 capas)

```text
┌─────────────────────────────────────────────────────────────┐
│  Aplicación (MQTT, HTTP, BLE, accesos, SNTP, …)             │
│  Usa: netHasConnectivity(), netMqttPreferred(), failover    │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│  net_manager  — ETH > WiFi STA  (> GSM datos Fase 2)        │
└───────┬───────────────────────────────┬─────────────────────┘
        │                               │
┌───────▼────────┐              ┌───────▼────────┐
│  Ethernet      │              │  wifi_manager  │
│  A2: LAN8720   │              │  AP + STA NVS  │
│  A2v3: W5500   │              └────────────────┘
└────────────────┘
        Paralelo:
┌─────────────────────────────────────────────────────────────┐
│  gsm_modem — FSM AT (SIM7600E); Absent sin módulo           │
└─────────────────────────────────────────────────────────────┘
```

### Feature flags (propuesta)

| Macro | A2 clásico | A2v3 |
|-------|------------|------|
| `A2_FEATURE_WIFI_MGMT` | **1** (prioridad) | 1 (recomendado; mantenimiento) |
| `A2_FEATURE_GSM_MODEM` | 0 (o 1 opcional Fase posterior) | **1** (prioridad) |
| `A2_FEATURE_ETHERNET` | 1 (LAN8720) | 1 (W5500) |
| `A2_BOARD_A2V3` | 0 | 1 |
| `ENABLE_BLE` | según env `*_ble` | según flash/particiones |

NVS al portar (evitar colisión con Modbus):

- WiFi: namespace `a2acc_wifi`
- GSM: namespace `a2acc_gsm`

---

## 4. Entornos PlatformIO previstos

| Env | MCU | Flags clave | Uso |
|-----|-----|-------------|-----|
| `esp32dev` | ESP32 | `A2_FEATURE_WIFI_MGMT=1`, GSM=0 | Producción A2 + WiFi |
| `esp32dev_ble` | ESP32 | WiFi + `ENABLE_BLE` | A2 con BLE (vigilar flash ~76 %) |
| `esp32dev_s3` *(nuevo)* | ESP32-S3 | `A2_BOARD_A2V3=1`, `A2_FEATURE_GSM_MODEM=1`, WiFi=1 | Producción A2v3 + 4G |
| `esp32dev_s3_ble` *(opcional)* | ESP32-S3 | GSM + BLE | Solo si cabe en N16R8 |

Cambios típicos en `platformio.ini` (borrador):

- Board S3: `esp32-s3-devkitc-1` o equivalente KinCony; flash 16 MB, PSRAM 8 MB si aplica.
- Pines ETH W5500 vs LAN8720 detrás de `#if A2_BOARD_A2V3`.
- UART GSM: TX/RX 10/9 (S3) vs 13/34 (A2 clásico).
- Librerías: Arduino-ESP32 3.x en S3 (NetworkClient / `ETH.setDefault()`).

---

## 5. Ficheros a portar (desde A2-MODBUS)

### 5.1 WiFi + red (prioridad A2)

| Origen Modbus | Destino Relay | Notas |
|---------------|---------------|--------|
| `src/net_manager.h/.cpp` | `src/net_manager.*` o integrar en árbol actual | Renombrar logs `DEBUG_SERIAL` al macro del proyecto |
| `src/wifi_manager.h/.cpp` | igual | NVS → `a2acc_wifi` |
| `src/wifi_server.h/.cpp` | igual | Adaptar auth Basic al WebServer actual |
| Constantes AP | `config` / defines | `WIFI_AP_*` §3.1 guía |

### 5.2 GSM (prioridad A2v3)

| Origen Modbus | Destino Relay | Notas |
|---------------|---------------|--------|
| `src/gsm_modem.h/.cpp` | igual | Pines por `A2_BOARD_A2V3` |
| `src/gsm_server.h/.cpp` | igual | Rutas `/gsm`, `/api/gsm/*` |
| PIN policy | obligatoria | Un intento; nunca PIN en JSON |

### 5.3 Integración en sketch / main

Checklist (guía §2):

1. Tras ETH + `buildDeviceId()`: `wifiManager.begin(deviceId)`.
2. Handlers ETH: `netOnEthGotIp()` / `netOnEthDown()`.
3. `loop()`: `wifiManager.loop()`; `gsmModem.loop()` si GSM.
4. MQTT: `netHasConnectivity()` + `netMqttIfaceChanged()` + transporte `NetworkClient` vs `WiFiClient` (S3).
5. Registrar `WifiServer` / `GsmServer`.
6. LCD (si A2v3 SSD1306): fila WIFI / 4G (opcional fase UI).

Estructura de código recomendada (evolución del monolito):

```text
main.ino (o src/main.cpp)
  ├── net_manager
  ├── wifi_manager + wifi_server
  ├── gsm_modem + gsm_server     // solo S3 / flag
  ├── mqtt_* (existente, adaptado)
  ├── ble_* (existente, ifdefs)
  └── accesos / relés (sin dependencia de red)
```

---

## 6. Fases de implementación

### Fase 0 — Documentación y esqueleto build *(esta entrega)*

- [x] Rama `v5.0.0`
- [x] `docs/v5.0.0/` (README, matriz, plan, guía)
- [x] Actualizar listado en `docs/README.md`
- [ ] Añadir envs S3 en `platformio.ini` (stub compile-safe, sin lógica aún) — *siguiente commit de código*

### Fase 0.5 — Saneamiento de la base *(implementada — ver [FASE-0.5-SANEAMIENTO.md](FASE-0.5-SANEAMIENTO.md))*

- [x] C1: códigos remotos a NVS `a2acc_rc` (la EEPROM 4 KB no los contenía; solape con zonas BLE)
- [x] C1b: `static_assert` del mapa EEPROM completo
- [x] C2: test EEPROM no destructivo en zona scratch (antes corrompía `deviceName`)
- [x] A1: arranque no bloqueante (AP de emergencia diferido al `loop()`)
- [x] A2: eliminado `ESP.restart()` por broker MQTT caído
- [x] M1: versión unificada → `v4.1.1-BLE` / `v3.0.3-EEPROM`
- [x] M2: parser JSON MQTT 1024→2048
- [ ] Validación en hardware (plan de pruebas §3 del documento de la fase)

### Fase 1 — WiFi + net_manager en A2 clásico

**Objetivo:** mantenimiento directo por AP; STA opcional; MQTT con failover ETH↔WiFi.

| # | Tarea | Criterio de hecho |
|---|--------|-------------------|
| 1.1 | Copiar/adaptar `net_manager` | Compila en `esp32dev` |
| 1.2 | Copiar/adaptar `wifi_manager` + NVS `a2acc_wifi` | AP boot_window en `192.168.77.1` |
| 1.3 | `wifi_server` + bloque `/system` | Status/scan/save desde móvil |
| 1.4 | Cablear eventos ETH existentes → `netOnEth*` | Flags unificados |
| 1.5 | MQTT: `netHasConnectivity` + cambio interfaz | Failover cable↔WiFi sin reboot |
| 1.6 | `WiFi.setSleep(false)` | Entorno industrial |
| 1.7 | Flash budget `esp32dev_ble` | Si no cabe: WiFi solo en env sin BLE o trim |

**Pruebas:**

- [ ] AP visible SSID = deviceId; web Basic auth
- [ ] STA conecta; web por IP STA
- [ ] Desenchufar ETH → MQTT pasa a WiFi
- [ ] Reenchufar ETH → failback MQTT a ETH
- [ ] Relés/MQTT comandos locales OK **sin** WiFi ni ETH

### Fase 2 — Target A2v3 (build + Ethernet W5500)

| # | Tarea | Criterio |
|---|--------|----------|
| 2.1 | Env `esp32dev_s3` + `A2_BOARD_A2V3` | Compila |
| 2.2 | Abstraction ETH (LAN8720 vs W5500) | GOT_IP en ambos |
| 2.3 | Default netif S3 (`ETH.setDefault` / `WiFi.STA.setDefault`) | Ping broker correcto |
| 2.4 | LCD IP (si se porta UI) | Opcional |

### Fase 3 — GSM Fase 1 (estado AT, sin PPP MQTT)

| # | Tarea | Criterio |
|---|--------|----------|
| 3.1 | `gsm_modem` + pines S3 9/10 | Absent sin módulo < 30 s boot |
| 3.2 | FSM: probe, SIM, registro, CSQ | `/api/gsm/status` |
| 3.3 | NVS `a2acc_gsm`, PIN policy | Un intento; `pin_set` bool |
| 3.4 | `gsm_server` UI | Poll 3 s |
| 3.5 | MQTT opcional `get_gsm` / `set_gsm` | Contrato JSON sin PIN |
| 3.6 | `data_mode` reservado | Sin PPP aún |

**Pruebas:**

- [ ] Sin módulo: Absent, sin spam UART
- [ ] Con SIM7600E: IMEI, CSQ, CEREG en API
- [ ] PIN erróneo → PinError, sin auto-retry
- [ ] UART no compartido con RS485/debug

### Fase 4 — GSM datos / MQTT backup *(fuera del MVP v5.0.0 o tag v5.1)*

- PPP o stack modem; `netMqttPreferred`: ETH > WiFi > GSM.
- Solo cuando ETH+WiFi caídos y `data_mode=backup`.

### Fase 5 — Documentación de usuario y release

- Guías en `docs/v5.0.0/` + notas en `docs/00 - docref/versiones/`.
- Binarios en `firmware/` (`.bin`, `.sha256`, `manifest`).
- Sync clon `A2-RELAY_ACCESOS` vía git (no scp).

---

## 7. Orden de arranque sugerido (nuevo)

```text
1. Serial / EEPROM / NVS app / usuarios accesos
2. Relés / lógica local (independiente de red)
3. ETH.begin() + eventos → netOnEthGotIp/Down
4. buildDeviceId()
5. wifiManager.begin(deviceId)     // si A2_FEATURE_WIFI_MGMT
6. mqttSetup()                     // transport según net
7. webServer routes + WifiServer (+ GsmServer)
8. gsmModem.begin()                // si A2_FEATURE_GSM_MODEM
9. BLE begin                       // si ENABLE_BLE
10. loop: idle → wifi → gsm → mqtt → web → ble → accesos
```

---

## 8. Riesgos y mitigaciones

| Riesgo | Impacto | Mitigación |
|--------|---------|------------|
| Flash A2 + BLE + WiFi | No enlaza | Medir tamaños; WiFi en `esp32dev`, BLE en env separado o recortar strings web |
| Monolito `main.ino` | Diff enorme / errores | Extraer módulos a `.cpp` incrementalmente; flags |
| Core Arduino 2.x vs 3.x | API ETH/WiFi distinta | A2 clásico mantener core actual; S3 en 3.x |
| GPIO9/10 S3 strapping | Boot raro | Respetar diseño KinCony; documentar en matriz |
| PIN SIM / PUK | Bloqueo SIM | Política guía §5.3 estricta |
| Colisión NVS con otros firmwares | Config borrada | Prefijos `a2acc_*` |
| Doble WebServer / rutas | Conflicto | Un solo servidor; registrar handlers como en Modbus |

---

## 9. Checklist de portación (resumen)

### WiFi (A2)

- [ ] Constantes AP + NVS `a2acc_wifi`
- [ ] `WiFi.setSleep(false)`
- [ ] AP pass ≥ 8; STA save/forget
- [ ] Escaneo async
- [ ] Card sistema + `/api/wifi/status` desde 192.168.77.1

### Red / MQTT

- [ ] `netOnEth*` / `netOnWifi*` en DHCP y estática
- [ ] Failover / failback ETH↔WiFi
- [ ] S3: default netif verificado

### GSM (A2v3)

- [ ] Absent limpio sin módulo
- [ ] Status completo con SIM7600E
- [ ] PIN un intento
- [ ] `rescan` hot-plug best-effort
- [ ] UART dedicado

### Regresión

- [ ] Accesos/relés OK offline
- [ ] Web por ETH, STA y AP
- [ ] BLE (si env) no roto
- [ ] Compilan todos los envs activos

---

## 10. Criterio de “v5.0.0 listo para campo” (MVP)

1. **A2:** WiFi AP/STA + net_manager + MQTT failover ETH/WiFi documentado y probado.
2. **A2v3:** Build S3 + Ethernet W5500 + GSM Fase 1 (estado/UI/API) operativo.
3. Documentación en `docs/v5.0.0/` actualizada; listado en `docs/README.md`.
4. Sin PPP MQTT por 4G (explicitado como Fase 4 / v5.1).

---

## 11. Próximos pasos inmediatos (código)

Tras validar este plan:

1. Stub `platformio.ini`: env `esp32dev_s3` + `include/target_features.h`.
2. Portar `net_manager` + `wifi_manager` sobre `esp32dev` (Fase 1).
3. Integrar MQTT/web existentes.
4. Portar `gsm_modem` bajo flag solo S3 (Fase 3).

No usar scp ni editar remoto: cambios locales → commit → push → pull en `A2-RELAY_ACCESOS`.

---

*Plan v5.0.0 — 2026-09-18 — a2-relay-accesos*
