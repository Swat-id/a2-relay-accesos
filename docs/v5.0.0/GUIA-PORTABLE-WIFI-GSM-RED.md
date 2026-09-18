> **Copia de trabajo para `a2-relay-accesos` rama `v5.0.0`.**  
> Origen: SWATID-A2-MODBUS. Plan de adaptación: [PLAN-DUAL-TARGET-WIFI-4G.md](PLAN-DUAL-TARGET-WIFI-4G.md).  
> Namespaces NVS sugeridos al portar: `a2acc_wifi`, `a2acc_gsm` (evitar colisión con Modbus).

---

# Guía portable — WiFi (AP/STA), módem 4G y capa de red

**Documento único** para replicar en **otro proyecto ESP32 / ESP32-S3** la gestión de:

- Punto de acceso WiFi de configuración y cliente STA
- Módulo celular (SIM7600E u homólogo AT)
- Prioridad de conectividad IP y failover MQTT

**Referencia de implementación:** firmware **SWATID-A2-MODBUS** (ramas `v0.5` GSM, `v0.6` WiFi/red).  
**Audiencia:** integrador que copia módulos sin arrastrar todo el firmware Modbus/MQTT.

---

## 1. Arquitectura recomendada (3 capas)

```text
┌─────────────────────────────────────────────────────────────┐
│  Aplicación (MQTT, HTTP, SNTP, …)                           │
│  Usa: netHasConnectivity(), netMqttPreferred(), failover    │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│  net_manager  — estado agregado + prioridad ETH > WiFi STA    │
│  (GSM datos = fase posterior, misma API)                    │
└───────┬───────────────────────────────┬─────────────────────┘
        │ eventos ETH (main)            │ eventos WiFi STA
        │                               │
┌───────▼────────┐              ┌───────▼────────┐
│  Ethernet      │              │  wifi_manager  │
│  (ETH.begin)   │              │  AP + STA NVS  │
└────────────────┘              └────────────────┘

        Paralelo (no bloquea Modbus / lógica local):
┌─────────────────────────────────────────────────────────────┐
│  gsm_modem — FSM AT no bloqueante, estado para UI/MQTT      │
│  (detección opcional; sin módulo → estado Absent, sin spam) │
└─────────────────────────────────────────────────────────────┘
```

### Reglas de diseño (validadas en campo)

| Regla | Motivo |
|-------|--------|
| **Modbus / SD / relés no dependen de red** | Instalación sigue operativa sin IP |
| **MQTT espera `netHasConnectivity()`** | Reintento con backoff; no reiniciar MCU al caer ETH |
| **WiFi/GSM en `loop()` con ticks ≥ 1 s** | No bloquear `setup()` ni tramas RS485 |
| **NVS por namespace** | `a2wifi`, `a2gsm` (renombrar prefijo al portar) |
| **Feature flags** | `A2_FEATURE_WIFI_MGMT`, `A2_FEATURE_GSM_MODEM` |

---

## 2. Ficheros a copiar (mínimo viable)

### WiFi + red (v0.6)

| Fichero | Rol |
|---------|-----|
| `src/net_manager.h` / `.cpp` | API unificada, default netif (S3) |
| `src/wifi_manager.h` / `.cpp` | AP/STA, NVS, escaneo async |
| `src/wifi_server.h` / `.cpp` | HTTP `/wifi`, `/api/wifi/*`, bloque `/system` |
| `include/target_features.h` | `#define A2_FEATURE_WIFI_MGMT 1` |
| `include/config.h` | Constantes AP (§3.1) |

### GSM 4G (v0.5)

| Fichero | Rol |
|---------|-----|
| `src/gsm_modem.h` / `.cpp` | UART, FSM, NVS, AT |
| `src/gsm_server.h` / `.cpp` | `/gsm`, `/api/gsm/*` |
| `include/config.h` | Pines UART, baud, `GSM_UART_NUM` |
| `include/target_features.h` | `#define A2_FEATURE_GSM_MODEM 1` |

### Integración en `main.cpp` (checklist)

1. Tras `ETH.begin` + `buildDeviceId()`: `wifiManager.begin(gDeviceId)`.
2. En handler ETH: `netOnEthGotIp()` / `netOnEthDown()` además de flags locales.
3. En `loop()`: `wifiManager.loop()` cada segundo; `gsmModem.loop()` si GSM.
4. En `mqttLoop()`: sustituir «solo ethConnected» por `netHasConnectivity()`; si `netMqttIfaceChanged()` → reconectar con cliente WiFi o NetworkClient según interfaz.
5. Registrar rutas: `WifiServer`, `GsmServer` (patrón `GsmServer` en v0.5).
6. **S3 + W5500:** TCP Ethernet = `NetworkClient`; TCP WiFi STA = `WiFiClient` / `NetworkClientSecure` según core 3.x.

Documentación detallada por rama: [v0.6/WIFI-AP-STA.md](v0.6/WIFI-AP-STA.md), [v0.5/GSM-4G.md](v0.5/GSM-4G.md), [v0.6/RESILIENCIA-RED-OFFLINE.md](v0.6/RESILIENCIA-RED-OFFLINE.md).

---

## 3. WiFi — especificación portable

### 3.1 Constantes (`config.h`)

| Define | Valor típico | Notas |
|--------|--------------|--------|
| `WIFI_AP_IP_ADDR` | `192,168,77,1` | Gateway = misma IP |
| `WIFI_AP_SUBNET` | `255,255,255,0` | |
| `WIFI_AP_DEFAULT_TIMEOUT_S` | `120` | Ventana `boot_window` |
| `WIFI_AP_DEFAULT_PASS` | `admin1234!` | WPA2; mín. 8 caracteres |
| `WIFI_AP_SSID_PREFIX` | `SWATID-` | Fallback si no hay `deviceId` |

**SSID del AP:** en producción se usa el **identificador del equipo** (`SWATID_<MAC>`), no solo sufijo MAC WiFi — coherencia con MQTT/BLE.

### 3.2 NVS namespace `a2wifi`

| Clave | Tipo | Default | Descripción |
|-------|------|---------|-------------|
| `ap_mode` | u8 | `1` | `0`=disabled, `1`=boot_window, `2`=always_on |
| `ap_timeout_s` | u32 | `120` | 10…3600 |
| `ap_extend` | u8 | `1` | Reinicia timer mientras `softAPgetStationNum() > 0` |
| `ap_pass` | string | factory | WPA2 PSK |
| `sta_en` | u8 | `0` | STA habilitada |
| `sta_ssid` | string | `""` | |
| `sta_pass` | string | `""` | |

### 3.3 Modos AP

| Modo | Comportamiento |
|------|----------------|
| **boot_window** | AP al arranque; cuenta atrás `ap_timeout_s`; se **resetea** mientras haya cliente |
| **always_on** | AP siempre activo (`apRemainingS = -1`) |
| **disabled** | Sin AP; solo STA si `sta_en` |

### 3.4 Modo radio (`applyMode`)

```text
needAp  = AP activo
needSta = sta_en && ssid no vacío

WIFI_OFF     si ninguno
WIFI_AP      solo AP
WIFI_STA     solo STA
WIFI_AP_STA  ambos (instalación típica: configurar por AP mientras ETH cableado)
```

### 3.5 Eventos STA (registrar en `begin`)

- `ARDUINO_EVENT_WIFI_STA_GOT_IP` → log IP/RSSI, `netOnWifiStaGotIp()`, opcional SNTP (`timeServiceOnEthernetGotIp()` en A2).
- `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` → `netOnWifiStaDown()`; `WiFi.setAutoReconnect(true)` + `WiFi.begin()` en background.

### 3.6 Escaneo asíncrono

1. `startScan()` → `WiFi.scanNetworks(true)`.
2. `scanStatus()` → `WiFi.scanComplete()`: `-1` en curso, `-2` idle, `n` redes.
3. HTTP: `GET /api/wifi/scan?start=1` lanza; GET sin `start` devuelve JSON `{ scanning, networks[] }`.

### 3.7 API HTTP (autenticación Basic = usuario web)

| Método | Ruta | Uso |
|--------|------|-----|
| GET | `/api/wifi/status` | `{ ap, sta, eth_up, mqtt_iface }` |
| GET | `/api/wifi/scan` | Escaneo (ver §3.6) |
| POST | `/wifi` | `action=save_ap \| set_ap_pass \| connect_sta \| forget_sta` → redirect `/system?wifimsg=` |

**UI:** bloque en `/system` vía `wifiServerSendSystemBlock()`; inicio (`/`) card WiFi; LCD fila `WIFI <ip>` si `A2_FEATURE_LCD_SSD1306`.

---

## 4. Capa `net_manager` — conectividad y MQTT

### 4.1 API

| Función | Significado |
|---------|-------------|
| `netEthUp()` | Flag GOT_IP + `ETH.linkUp()` + IPv4 ≠ 0 |
| `netWifiStaUp()` | STA conectada + IPv4 ≠ 0 |
| `netHasConnectivity()` | OR de las anteriores |
| `netMqttPreferred()` | **Ethernet** si up; si no **WifiSta**; si no `None` |
| `netMqttIfaceChanged(&now)` | Detectar failover/failback → forzar `mqttClient.disconnect()` + `mqttEnsureTransport()` |

### 4.2 Default netif (ESP32-S3, Arduino 3.x)

Tras ETH GOT_IP: `ETH.setDefault()`.  
Tras ETH down y WiFi up: `WiFi.STA.setDefault()`.  
Sin esto, `NetworkClient` puede seguir intentando salir por interfaz caída.

### 4.3 Prioridad (documentada; GSM fase 2)

```text
MQTT / TCP saliente:  Ethernet  >  WiFi STA  >  GSM (PPP, pendiente)
HTTP WebServer:       escucha en todas las interfaces con IPv4 (80)
```

### 4.4 Patrón `mqttLoop()` (pseudocódigo)

```cpp
if (!netHasConnectivity()) return;  // log cada 15 s

if (netMqttIfaceChanged(&iface))
    mqttApplyRuntimeConfig(true, "cambio interfaz");

mqttEnsureTransport();  // elige NetworkClient vs WiFiClient según netMqttPreferred()
// reconnect PubSubClient con backoff
```

---

## 5. GSM 4G — especificación portable

### 5.1 Hardware (referencia KinCony)

**KC868-A2v3 (ESP32-S3):**

| Señal | GPIO | Notas |
|-------|------|--------|
| ESP TX → RXD módulo | **10** | Auto-detect swap con GPIO 9 |
| ESP RX ← TXD módulo | **9** | |
| PWRKEY | socket | Módulo arranca solo; 10–15 s hasta AT |

**KC868-A2 (ESP32 clásico):** `A2_GSM_TX_PIN=13`, `A2_GSM_RX_PIN=34`.

**UART:** `GSM_UART_NUM` (2), **115200 8N1**.

### 5.2 NVS namespace `a2gsm`

| Clave | Tipo | Default | Descripción |
|-------|------|---------|-------------|
| `en` | u8 | `1` | `0` = no probe al boot |
| `pin_swap` | u8 | `0` | `1` = TX/RX invertidos (aprendido en probe) |
| `apn_mode` | u8 | `1` | `1`=auto (bearer red), `0`=manual + `CGDCONT` |
| `apn` | string | | Solo manual |
| `apn_user` / `apn_pass` | string | | `CGAUTH` opcional |
| `pin` | string | | PIN SIM (nunca exponer en API) |
| `data_mode` | u8 | `0` | `0`=backup, `1`=solo 4G, `2`=off (fase datos) |

### 5.3 Política PIN (obligatoria al portar)

- `AT+CPIN=` **una sola vez por arranque y por valor** almacenado.
- Error → estado `PinError`, **sin reintentos automáticos** (riesgo PUK).
- PIN **nunca** en JSON web/MQTT (solo `pin_set: true/false`).
- PUK: aviso al operador; no gestionar desde firmware.

### 5.4 Máquina de estados (resumen)

```text
Disabled ──(en=1)──▶ Probing ──AT OK──▶ SimCheck ──READY──▶ Registering ──▶ Attached
                        │                    │
                        ▼                    ├── PinRequired / PinError / PukRequired / NoSim
                     Absent (rescan manual)
```

- Tick desde `loop()` cada **500 ms–1 s**; cada AT timeout **300–1000 ms**.
- Snapshot `GsmStatus` para web, LCD, MQTT `get_gsm`.
- CSQ/CEREG/COPS: poll ~10 s solo si módulo presente.

### 5.5 Detección UART / swap

Probe alterna mapeo directo e invertido; tras éxito persiste `pin_swap` en NVS.  
Sin respuesta tras N rondas → `Absent`; **no** saturar serie ni bloquear poll.

### 5.6 Comandos AT (SIM7600E — lista mínima)

| Comando | Uso |
|---------|-----|
| `AT` / `ATI` | Vida / modelo |
| `AT+GSN` | IMEI |
| `AT+CPIN?` / `AT+CPIN=` | SIM |
| `AT+SPIC` o `AT+QPINC` | Intentos PIN restantes |
| `AT+CSQ` | Cobertura 0–31 (dBm ≈ −113 + 2·CSQ) |
| `AT+COPS?` (+ `AT+COPS=3,0`) | Operador |
| `AT+CEREG?` / `AT+CREG?` | Registro LTE/2G |
| `AT+CPSI?` | Tecnología / banda |
| `AT+CGDCONT` / `AT+CGAUTH` | APN manual |
| `AT+CGATT?` / `AT+CGPADDR` | Attach / IP PDP |

**Fase 2 (datos):** PPP o stack modem; MQTT backup cuando ETH+WiFi caen y `data_mode=backup`.

### 5.7 API HTTP

| Método | Ruta | Uso |
|--------|------|-----|
| GET | `/gsm` | Página estado (poll 3 s) + config |
| GET | `/api/gsm/status` | JSON completo `GsmStatus` + config flags |
| POST | `/gsm` | `action=save_cfg` \| `action=set_pin` |
| POST | `/api/gsm/rescan` | Re-probe manual |

**MQTT (opcional):** `get_gsm` / `set_gsm` en `mqtt_commands.cpp` (mismo contrato que JSON status).

### 5.8 Presentación UI (CSQ)

| CSQ | dBm (aprox.) | Etiqueta |
|-----|--------------|----------|
| 0 / 99 | — | Sin señal |
| 1–7 | ≤ −99 | Mala |
| 8–12 | | Regular |
| 13–19 | | Buena |
| ≥ 20 | | Excelente |

---

## 6. Orden de arranque sugerido (nuevo proyecto)

```text
1. Serial / EEPROM / NVS app
2. Modbus / lógica local (independiente de red)
3. ETH.begin() + eventos → netOnEthGotIp/Down
4. buildDeviceId()
5. wifiManager.begin(deviceId)     // no bloquear > unos ms
6. mqttSetup()                     // transport según net (post-IP)
7. webServer routes + begin        // cuando lwIP listo (S3: tras ETH)
8. gsmModem.begin()                // probe async; último o paralelo
9. loop: serviceLoopIdle → wifiManager.loop → gsmModem.loop → resto
```

---

## 7. Checklist de portación

### WiFi

- [ ] Constantes AP y namespace NVS renombrados si colisionan
- [ ] `WiFi.setSleep(false)` en entornos industriales
- [ ] AP password ≥ 8 chars; rechazo en UI
- [ ] STA: credenciales en NVS; `forget_sta` borra y desconecta
- [ ] Escaneo no bloqueante en handler HTTP (`yield` / `handleClient` si scan largo)
- [ ] Card sistema + `/api/wifi/status` probados desde móvil en 192.168.77.1

### Red / MQTT

- [ ] `netOnEth*` / `netOnWifi*` cableados en todos los caminos (DHCP + estática)
- [ ] Failover ETH↓ WiFi↑ probado (desenchufar cable)
- [ ] Failback WiFi→ETH probado (enchufar cable)
- [ ] S3: `ETH.setDefault()` / `WiFi.STA.setDefault()` verificado con ping broker

### GSM

- [ ] Sin módulo: boot < 30 s, sin bucle AT infinito
- [ ] Con SIM7600E: IMEI, CSQ, registro en `/api/gsm/status`
- [ ] PIN erróneo: un intento, mensaje claro, sin auto-retry
- [ ] `rescan` recupera módulo hot-plug (mejor esfuerzo)
- [ ] UART no compartido con RS485 / debug

### Regresión local

- [ ] Poll Modbus OK **sin** Ethernet, WiFi ni GSM
- [ ] Web accesible por ETH, por STA y por AP (si activo)

---

## 8. Adaptación a otro producto (nombres y flags)

| En A2-MODBUS | Sugerencia al portar |
|--------------|----------------------|
| Namespace `a2wifi` / `a2gsm` | `myprod_wifi`, `myprod_gsm` |
| `A2_FEATURE_*` | `PRODUCT_FEATURE_WIFI`, `PRODUCT_FEATURE_GSM` |
| `DEBUG_SERIAL` | Mismo macro de log del proyecto |
| `WebServer` + Basic auth | Reutilizar o sustituir por API REST |
| `deviceId` | Cualquier string estable (MAC-based) para SSID AP |

**No copiar ciegamente:** tamaños de pila, particiones flash (A2 clásico ~76 % flash con WiFi), pines strapping S3 en GPIO9/10.

---

## 9. Dependencias PlatformIO

| Componente | Librería |
|------------|----------|
| WiFi / ETH | Core Arduino-ESP32 (espressif32 6.x) |
| NVS | `Preferences` (incluida) |
| HTTP JSON | `ArduinoJson` (opcional, ya usada en wifi_server / gsm_server) |
| GSM Fase 1 | **Sin TinyGSM** (AT propio) |
| GSM Fase 2 | Evaluar TinyGSM / PPP según módulo |

Entornos de referencia: `kincony_a2`, `kincony_a2_s3` (`platformio.ini`).

---

## 10. Mapa rápido — rutas y estados

```text
                    ┌──────────────┐
   Usuario móvil ──▶│ AP 192.168.77.1│──▶ HTTP :80 /system /wifi
                    └──────────────┘
   Router WiFi ────▶│ STA DHCP       │──▶ misma web por IP STA
   Cable RJ45 ─────▶│ ETH DHCP/static│──▶ misma web por IP ETH
   (fase 2) ───────▶│ GSM PDP        │──▶ MQTT backup

Estado GSM web:  /gsm  +  GET /api/gsm/status  (3 s)
Estado WiFi web: /system + GET /api/wifi/status
Estado agregado: mqtt_iface = eth | wifi | none (GSM futuro)
```

---

## 11. Referencias en este repositorio

| Tema | Ruta |
|------|------|
| Plan WiFi v0.6 | [docs/v0.6/WIFI-AP-STA.md](v0.6/WIFI-AP-STA.md) |
| Resiliencia offline | [docs/v0.6/RESILIENCIA-RED-OFFLINE.md](v0.6/RESILIENCIA-RED-OFFLINE.md) |
| Plan GSM v0.5 | [docs/v0.5/GSM-4G.md](v0.5/GSM-4G.md) |
| LCD fila WiFi/4G | [docs/v0.5/LCD-SSD1306.md](v0.5/LCD-SSD1306.md) |
| Código WiFi | `src/wifi_manager.*`, `src/wifi_server.*` |
| Código red | `src/net_manager.*` |
| Código GSM | `src/gsm_modem.*`, `src/gsm_server.*` |
| Integración | `src/main.cpp` (búsqueda `wifiManager`, `gsmModem`, `netMqtt`) |

---

*Guía portable v1 — 2026-09-18 — SWATID-A2-MODBUS (v0.5 GSM + v0.6 WiFi/red).*
