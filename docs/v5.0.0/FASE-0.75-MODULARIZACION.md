# Fase 0.75 — Modularización del firmware (paso intermedio)

**Rama:** `v5.0.0` · **Fecha:** 18 Sep 2026
**Precede a:** Fase 1 (WiFi + net_manager completo) · **Sigue a:** [Fase 0.5 saneamiento](FASE-0.5-SANEAMIENTO.md)
**Compilación verificada:** `esp32dev` (flash 92,4 %) y `esp32dev_ble` (flash 76,4 %) — RAM sin cambio

---

## 1. Objetivo

Romper el monolito `main.ino` (10.4k líneas) en ficheros con responsabilidad clara, **creando primero la arquitectura de conectividad** (WiFi / 4G / red agregada) que las Fases 1–3 rellenarán, y extrayendo las funcionalidades existentes de forma incremental y compilable en cada paso.

## 2. Estructura resultante

```text
include/
  target_features.h    Feature flags por target (A2_FEATURE_WIFI_MGMT,
                       A2_FEATURE_GSM_MODEM, A2_FEATURE_ETHERNET, A2_BOARD_A2V3)
src/
  main.ino             Orquestación: setup/loop, MQTT, web, BLE, accesos (10.0k)
  hw_config.h          Pines y constantes de placa (A2; hueco A2v3 con #error)
  eeprom_layout.h      TODAS las estructuras persistentes + offsets EEPROM/NVS
                       + static_assert del mapa (única fuente de verdad)
  net_manager.h/.cpp   Estado agregado de red: netHasConnectivity(),
                       netMqttPreferred() (ETH > WiFi STA), netMqttIfaceChanged()
  wifi_manager.h/.cpp  WiFi de gestión — hoy: AP de emergencia (ex setupAPMode);
                       Fase 1: AP boot_window/always_on + STA + NVS a2acc_wifi
  gsm_modem.h/.cpp     Esqueleto FSM SIM7600E (estados y GsmStatus definidos);
                       no-op con flag=0; #error si se activa sin implementar
  remote_codes.h/.cpp  Códigos remotos: NVS, altas/bajas, franjas horarias
                       (primer módulo funcional extraído)
  local_codes.h/.cpp   Códigos locales (EEPROM 512): load/save/add/isCodeStored
                       — tranche 1 ✔
  digital_inputs.h/.cpp DI1/DI2: config EEPROM, proceso Normal/Inverso,
                       evento MQTT — tranche 2 ✔
  web_common.h/.cpp    CSS único del portal + webPageBegin()/webAuth()
                       (reformulación HTML, ver §7)
```

### Feature flags (declarados en `platformio.ini` de cada entorno)

| Entorno | `WIFI_MGMT` | `GSM_MODEM` | Notas |
|---------|-------------|-------------|-------|
| `esp32dev` | 1 | 0 | A2 producción |
| `esp32dev_ble` | 1 | 0 | A2 + BLE |
| `esp32dev_s3` *(futuro)* | 1 | 1 | A2v3 — activar GSM obliga a implementar Fase 3 (`#error` guard) |

## 3. Cambios de cableado en `main.ino`

- **Eventos ETH → net_manager:** `WiFiEvent()` llama a `netOnEthGotIp()` / `netOnEthDown()` además de mantener el flag legacy `ethConnected` (los ~10 usos restantes en web/MQTT status se migrarán al extraer esos módulos).
- **MQTT usa la capa agregada:** `connectToMqtt()` y toda la lógica de reconexión del `loop()` consultan `netHasConnectivity()` en vez de `ethConnected`. Cuando la Fase 1 añada WiFi STA, **el failover MQTT no requerirá tocar esta lógica**.
- **Detección de cambio de interfaz:** el `loop()` ya invoca `netMqttIfaceChanged()` y fuerza `mqttClient.disconnect()` + reconexión al cambiar la interfaz preferida (patrón §4.4 de la guía portable). Hoy inerte (solo hay ETH); operativo en cuanto exista STA.
- **AP de emergencia:** movido a `wifi_manager` (`wifiStartEmergencyAp()`); el fallback diferido del `loop()` lo invoca. Mismo comportamiento de campo (SSID/pass/IP idénticos).
- **GSM:** `setup()`/`loop()` ya llaman `gsmModemBegin()`/`gsmModemLoop()` — no-op en A2; punto de anclaje listo para el A2v3.

## 4. Criterio de equivalencia (verificado)

| Métrica | Antes (0.5) | Después (0.75) |
|---------|-------------|----------------|
| `esp32dev` flash | 1.210.181 B (92,3 %) | 1.210.581 B (92,4 %) |
| `esp32dev_ble` flash | 1.500.753 B (76,3 %) | 1.501.217 B (76,4 %) |
| RAM (ambos) | idéntica | idéntica |

El delta (+~450 B) es la capa `net_manager` + detección de cambio de interfaz. Sin cambios funcionales: mismo SSID/IP del AP, mismos logs, misma persistencia.

## 5. Hoja de ruta de extracción restante (tranches siguientes)

Orden recomendado (de menos a más acoplado), compilando tras cada uno:

| # | Módulo destino | Contenido a mover de main.ino | Dependencias clave |
|---|----------------|-------------------------------|--------------------|
| 1 | ✔ `local_codes.h/.cpp` | `StoredCodes` init/load/save/add/isCodeStored | EEPROM, storedCodes |
| 2 | ✔ `digital_inputs.h/.cpp` | config DI + `processDigitalInput` + evento MQTT | externs a mqttClient/relés (documentados en el .cpp) |
| 3 | `ota_service.h/.cpp` | OTA config, check, download, rollback | HTTPClient, Update, versión |
| 4 | `mqtt_service.h/.cpp` | connect/callback/processCommand/publicaciones | casi todos los globals — hacerlo tras 1–3 |
| 5 | `access_logic.h/.cpp` | Wiegand ISRs+process, processKey, torno, seguridad | mqtt_service, códigos |
| 6 | `web_portal.h/.cpp` | setupWebServer + todos los handlers/HTML | todo lo anterior |
| 7 | `ble_service.h/.cpp` | initBLE, callbacks, auth HKDF (env BLE) | mqtt, códigos, eeprom_layout |

Regla para cada extracción: los **globals compartidos** se declaran `extern` en el header del módulo que los posee (patrón `remote_codes.h` → `storedRemoteCodes`), nunca en un "globals.h" cajón de sastre; el módulo que escribe el dato es su dueño.

## 7. Reformulación HTML — presupuesto flash para el WiFi de Fase 1

**Objetivo:** que el target de menor capacidad (`esp32dev`, app OTA limitada a
1.310.720 B por la tabla de particiones de los equipos en campo) tenga margen
para `wifi_manager` completo (AP/STA + UI).

Medición previa (binario esp32dev): `.rodata` 262 KB, de los cuales ~58 KB
eran literales HTML/CSS repartidos en 13 bloques `<style>` casi duplicados
(~14,7 KB de CSS bruto), 4 links a font-awesome por CDN y 674 sentencias
`+=` de literales (código en `.text`).

| Intervención | Técnica | Riesgo |
|--------------|---------|--------|
| Colapso de 344 `+=` consecutivos | Concatenación de literales adyacentes en compilación (contenido byte-idéntico) | Nulo |
| Eliminación font-awesome | 4 `<link>` CDN + 82 iconos `<i class='fa…'>` (además resuelve M3: páginas lentas sin internet) | Cosmético |
| CSS único (`web_common`) | Unión normalizada de los 13 bloques en un `PROGMEM`; cabecera común `webPageBegin(título)`; los cuerpos HTML no cambian | Cosmético (aspecto unificado; botones/colores normalizados) |
| `webAuth()` | 46 bloques de Basic auth → helper | Nulo |

**Resultado (compilado):**

| Métrica | Antes | Después | Δ |
|---------|-------|---------|---|
| `esp32dev` flash | 1.211.617 B (92,4 %) | **1.194.305 B (91,1 %)** | **−17,3 KB** |
| `esp32dev` libre | 99,1 KB | **116,4 KB** | +17,3 KB |
| `esp32dev_ble` flash | 1.502.237 B (76,4 %) | 1.481.857 B (75,4 %) | −20,4 KB |

Estimación de la Fase 1 WiFi (lógica AP/STA + NVS + página `/wifi` usando el
CSS común): **10–15 KB** — cabe con holgura. La radio WiFi, `Preferences`,
DNSServer y mDNS ya están enlazados en el binario actual, así que el coste
marginal es solo la lógica y la UI propia.

Palancas restantes si hiciera falta más (no aplicadas): recorte de logs serie
(~19 KB en cadenas con emoji), y tabla de particiones de 1,875 MB para equipos
nuevos flasheados por cable (**no** sirve para el parque OTA existente: la
tabla de particiones no se actualiza por OTA).

## 8. Notas para Fase 1 (WiFi)

- `wifi_manager` ya es el único dueño del radio WiFi: la portación del módulo completo (AP 192.168.77.1, STA, NVS `a2acc_wifi`, escaneo async) sustituye el contenido de `wifi_manager.cpp` sin tocar `main.ino` más que para añadir `wifiManagerLoop()` y retirar el AP de emergencia legacy.
- Los eventos STA (`ARDUINO_EVENT_WIFI_STA_GOT_IP/DISCONNECTED`) deben llamar a `netOnWifiStaGotIp()/Down()` — el failover MQTT ya está cableado y reaccionará solo.
- **Flash `esp32dev` al 91,1 %** tras la reformulación HTML (§7): la página `/wifi` debe construirse con `webPageBegin()` y el CSS común, sin `<style>` propio; medir tras portar.

---

*Fase 0.75 completada — pendiente validación en hardware junto al plan de pruebas de la Fase 0.5.*
