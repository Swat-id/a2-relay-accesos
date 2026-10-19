# Plan — Configuración completa por BLE (v5.0.6_BLE)

**Rama:** `v5.0.6_BLE` · **Fecha:** 16 Oct 2026  
**Baseline:** firmware v5.0.5 (NimBLE, FF01–FF0B, `wifi_manager` con scan async MQTT/web)  
**Docs previas:** [BLE_INTEGRATION.md](../v4.1/Firmware_description/BLE_INTEGRATION.md), [BLE_FF05](../00%20-%20docref/referencia/integration/BLE_FF05_NETWORK_CONFIG.md), [BLE_COMMANDS_ANALYSIS](../00%20-%20docref/referencia/integration/BLE_COMMANDS_ANALYSIS.md)

---

## 1. Objetivo

Hacer que la **App móvil** pueda instalar y administrar el equipo **sin depender del portal AP** (y sin pelearse con él), con el mismo alcance funcional que el portal web en campo:

- Identidad / info / estado  
- Relés, modo torno, duración  
- Códigos locales (y, si cabe, remotos con franjas)  
- Ethernet (DHCP/estática) — ya en FF05  
- **WiFi AP/STA + escaneo de redes** — gap crítico  
- Seguridad (bloqueo acceso, teclados)  
- Hora  
- Entradas digitales (pulsador / imán)  
- GSM (si el target lo tiene): estado + config básica (PIN/APN) sin exponer PIN en lectura  
- OTA: al menos disparar check / ver estado (upload binario por BLE = fuera de MVP o fase tardía)

---

## 2. Inventario BLE actual (partida)

| UUID | Rol hoy | Auth | Notas |
|------|---------|------|-------|
| FF01 | Auth (challenge-response / legacy key) | — | Token sesión |
| FF02 | Relé | Sí + `PERM_RELAY` | |
| FF03 | Modo (normal/torno) | Sí + `PERM_MODE` | |
| FF04 | Alta códigos | Sí + `PERM_CODES` | |
| FF05 | **Solo Ethernet** (binario 17 B WRITE / JSON READ) | Sí + `PERM_NETWORK` | Doc dice 0x02 perm; código usa `BLE_PERM_NETWORK_CONFIG` **0x08** — alinear |
| FF06 | Duración relé | Sí | |
| FF07 | Status | Sí | |
| FF08 | DevInfo | No | Público |
| FF09 | FullInfo | Sí | |
| FF0A | Listado códigos (páginas) | Sí | |
| FF0B | Challenge 16 B | No | |

**No existe hoy por BLE:**

- Escaneo WiFi / config STA / modos AP  
- GSM status/config  
- DI tipo door/button + open_level  
- Códigos remotos + time_slots  
- Seguridad block/unblock / disable keyboards  
- Sync hora  
- OTA check/config  
- Cambio password web / nombre dispositivo (parcial en FullInfo?)  
- Canal usuarios `/vinculo` (v5.0.5) por BLE (auth ya existe; altas MQTT-style no)

---

## 3. Inventario portal web → paridad BLE

| Área portal | Rutas / APIs | BLE hoy | Acción v5.0.6 |
|-------------|--------------|---------|----------------|
| Inicio / estado | `/`, status | FF07/FF09 parcial | Ampliar FF09 o FF07 con wifi/gsm/mqtt_iface |
| Config general | `/save` (nombre, DHCP ETH, duración, keepalive, pass) | FF05 ETH + FF06 | JSON config WRITE (nueva char o ampliar FF05) |
| Relé | `/rele` | FF02 | OK |
| Códigos locales | `/codes/*` | FF04 + FF0A | Completar delete/clear/list remoto |
| Códigos remotos | `/remote-codes/*` | — | **Nuevo** (FF0C o JSON en FF04 con `scope`) |
| Torno | `/turnstile/*` | FF03 parcial | Completar relays por teclado |
| DI | `/digital_inputs` | — | **Nuevo** |
| WiFi | `/wifi`, `/api/wifi/*` | — | **Nuevo (P0)** scan + set STA/AP |
| GSM | `/gsm`, `/api/gsm/*` | — | **Nuevo (P1)** status + set seguro |
| Tiempo | `/time/*` | — | **Nuevo (P1)** |
| Seguridad | `/security/*` | — | **Nuevo (P1)** |
| OTA | `/ota/*` | — | check/status P2; upload P3 |
| BLE users web | `/ble/*` | FF01 + MQTT tipo 6 / vinculo | App usa FF01; altas vía protocolo ya definido |
| Reboot/reset | web | — | Comando seguro P1 |

---

## 4. Diseño WiFi por BLE (P0)

### 4.1 Problema a evitar: AP + STA simultáneos conflictivos

Escenario real: instalador conectado al **AP** `192.168.x.1` y a la vez la App intenta `WiFi.begin(ssid)` por BLE → el radio ESP32 entra en `WIFI_AP_STA`, escaneos fallan o el AP se cae, portal pierde clientes, sensación de “no funciona”.

**Política propuesta (obligatoria):**

| Regla | Comportamiento |
|-------|----------------|
| **R1** | Tras auth BLE con permiso `NETWORK`, si la App inicia flujo WiFi (`wifi_scan` / `wifi_sta_set`), el firmware **apaga el SoftAP** (o lo deja en `disabled` temporal) y prioriza STA/scan. |
| **R2** | Mientras haya sesión BLE autenticada en “modo config red”, el portal **rechaza** cambios WiFi (HTTP 409 / mensaje claro) o el AP no se rearma por `boot_window`. |
| **R3** | Al desconectar BLE o timeout de sesión: restaurar `ap_mode` NVS (p. ej. volver a `boot_window` solo si no hay STA up, según política). |
| **R4** | Nunca aceptar config STA desde web AP y desde BLE en la misma ventana de 30 s (lock `net_config_owner = ble\|web\|none`). |
| **R5** | Scan siempre **async** (`wifiScanStart` ya existe); BLE hace poll `wifi_scan_get` — no bloquear loop de accesos. |

### 4.2 Flujo App recomendado

```text
1. Connect BLE → FF08 → FF0B/FF01 auth (perm NETWORK)
2. WRITE: {"cmd":"wifi_scan_start"}
3. Poll READ/NOTIFY: {"cmd":"wifi_scan_get"} → {scanning, networks[{ssid,rssi,enc}]}
4. Usuario elige SSID → WRITE: {"cmd":"wifi_sta_set","ssid":"...","pass":"..."}
5. READ status: {"sta":{connected,ip,rssi}, "ap":{active}, "eth_up", ...}
6. Opcional: wifi_ap_set mode/timeout/pass — solo si se necesita mantenimiento
```

Reutilizar JSON de `wifiStatusJson()` / `wifiScanJson()` (`wifi_manager`) para no duplicar contratos MQTT/web.

### 4.3 Dónde vive en GATT

**Opción A (preferida):** ampliar **FF05** a protocolo JSON unificado de red:

- READ → status ETH + WiFi + (flags GSM up)  
- WRITE → JSON `cmd` discriminador: `eth_set` \| `wifi_scan_start` \| `wifi_scan_get` \| `wifi_sta_set` \| `wifi_sta_forget` \| `wifi_ap_set`  
- Mantener WRITE binario 17 B como **legado Ethernet** (si `length==17`)

**Opción B:** nueva característica **FF0C NetworkExt** solo WiFi/GSM; FF05 intacto. Más limpio para App nueva, más UUID que documentar.

**Decisión de plan:** Opción A con legado binario, salvo que el buffer/notify de FF05 se quede corto → entonces FF0C.

---

## 5. Seguridad y robustez

| Tema | Requisito |
|------|-----------|
| Auth | Todo WRITE de config exige sesión válida; timeout sesión 5 min (ya) |
| Permisos | Network=0x08, Codes=0x04, …; corregir docs que digan 0x02 para red |
| Secretos | READ nunca devuelve `sta_pass` / PIN SIM / claves BLE |
| PIN GSM | Solo WRITE; un intento; misma política que `gsm_modem` |
| Chunking | Respuestas largas (scan, list codes) con EOT + fragmentación existente `sendBLEChunked` |
| MTU | Negociar MTU; documentar tamaño máx. por notify |
| Rate limit | Scan: no más de 1 start / 10 s; evita DoS radio |
| Acceso local | Config BLE no debe bloquear Wiegand/relés; ticks red en background |

---

## 6. Mapa de comandos BLE propuestos (resumen)

### 6.1 Red (FF05 JSON o FF0C)

| cmd | Dirección | Body |
|-----|-----------|------|
| *(read)* | ← | status eth+wifi (+gsm flags) |
| `eth_set` | → | dhcp / ip / gw / mask / dns (equiv. binario) |
| `wifi_scan_start` | → | — |
| `wifi_scan_get` | → o ← | networks[] |
| `wifi_sta_set` | → | ssid, pass, enabled |
| `wifi_sta_forget` | → | — |
| `wifi_ap_set` | → | mode, timeout_s, pass (≥8) |
| `wifi_ap_stop` | → | fuerza off (útil al entrar en config BLE) |

### 6.2 Nuevas áreas (fase 2+)

| Área | Propuesta GATT | cmds |
|------|----------------|------|
| DI | FF0D o JSON en FF09 WRITE | get/set type, open_level, relay, inverse, enabled |
| Remotos | ampliar FF04/FF0A | add/list/del con time_slots |
| Seguridad | FF0E o FF03 extend | block/unblock, keyboards on/off, max_attempts |
| Tiempo | FF0E | set_time ISO / sync_sntp |
| GSM | FF0F (solo builds GSM) | get_status, set_apn, set_pin (write-only), rescan |
| Sistema | FF07 WRITE | reboot, reset (confirm flag) |

Mantener **mínimo de UUIDs nuevos**; preferir JSON con `cmd` sobre proliferar características.

---

## 7. Gaps y prioridades

| ID | Trabajo | Pri |
|----|---------|-----|
| B1 | Política lock AP/BLE + apagado AP en flujo WiFi BLE | P0 |
| B2 | `wifi_scan_*` + `wifi_sta_*` + `wifi_ap_*` por BLE | P0 |
| B3 | Status unificado READ (eth/wifi/mqtt_iface) | P0 |
| B4 | Corregir permiso/docs FF05; legado binario | P0 |
| B5 | DI get/set por BLE | P1 |
| B6 | Códigos remotos + franjas por BLE | P1 |
| B7 | Seguridad + tiempo | P1 |
| B8 | GSM status/config (targets 4G) | P1 |
| B9 | OTA check/status | P2 |
| B10 | Manual App + vectores de prueba | P1 |
| B11 | Regresión: accesos locales OK durante scan | P0 |

---

## 8. Fases de implementación

| Fase | Contenido | Criterio |
|------|-----------|----------|
| **0** | Este plan | Docs OK |
| **1** | B1–B4 WiFi/ETH por BLE + lock AP | App configura WiFi con lista de redes; sin portal |
| **2** | B5–B7 DI, remotos, seguridad, hora | Paridad portal “día a día” |
| **3** | B8–B9 GSM + OTA status | Targets 4G / mantenimiento |
| **4** | B10 manual + cierre pruebas App | Release notes |

---

## 9. Plan de pruebas

### WiFi / AP

| # | Caso | Esperado |
|---|------|----------|
| W1 | Auth + scan_start + scan_get | Lista SSIDs con rssi/enc |
| W2 | sta_set red válida | STA up; IP en status; MQTT puede usar wifi |
| W3 | Scan con AP boot_window activo | AP se apaga o lock; scan OK; sin crash portal |
| W4 | Web intenta save WiFi durante sesión BLE red | Rechazo o owner=ble |
| W5 | Desconexión BLE | Política restore AP documentada |
| W6 | Acceso teclado durante scan | Relé abre &lt; 300 ms |

### Paridad

| # | Caso | Esperado |
|---|------|----------|
| P1 | ETH DHCP/static por BLE (legado + JSON) | Igual que web |
| P2 | DI door + button | Igual que web |
| P3 | add/list/del código local | Igual que web |
| P4 | add remoto con franja | Respeta horario |

### Seguridad

| # | Caso | Esperado |
|---|------|----------|
| S1 | WRITE sin auth | ERROR |
| S2 | User sin PERM_NETWORK | ERROR permisos |
| S3 | READ status | sin passwords |

---

## 10. Ficheros candidatos

| Fichero | Cambio |
|---------|--------|
| `src/main.ino` | Callbacks FF05/nuevos; lock owner; parse JSON cmds |
| `src/wifi_manager.*` | API para “ble config mode”, stop AP, scan ya listo |
| `src/gsm_modem.*` | Exponer status JSON ya usado en web |
| `src/digital_inputs.*` | get/set config por API C |
| `docs/v5.0.6_BLE/` | Contrato BLE final + ejemplos App |
| Docs integración v4.1 | Enlace a este plan / deprecaciones |

---

## 11. Fuera de alcance

- Sustituir MQTT `/vinculo` (rama users) — BLE auth de usuarios ya existe; altas masivas pueden seguir MQTT.
- Upload OTA firmware completo por BLE (lento/frágil).
- Cambiar UUIDs de servicio FF00 (rompe Apps desplegadas).

---

## 12. Estado

| Ítem | Estado |
|------|--------|
| Rama `v5.0.6_BLE` | Creada |
| Plan | Este documento |
| Implementación B1–B4 (Fase 1) | **Hecha** (16-Oct): FF05 JSON unificado (scan/STA/AP/eth_set + legado binario), lock AP↔BLE R1–R5, web 409, restore de política al cerrar sesión, permiso 0x08 corregido en docs — ver [CONTRATO-FF05-RED.md](CONTRATO-FF05-RED.md) |
| Compilación | 4 entornos OK (BLE: 77,5 % flash) |
| Validación W1–W6 | **W1/W2(parcial)/W3/W5/S1-S3 validadas en placa A2v3** (19-Oct, cliente BLE real desde macOS): TOFU → `OK:SUPERADMIN_REGISTERED`, re-auth challenge-response → `OK:AUTHENTICATED`, FF07/FF05 status, `wifi_scan_start` → **13 redes con el enlace BLE ESTABLE** durante el scan, `exit_config` OK. Clave TOFU-BLE verificada en el almacén de vinculación tras reinicio (`admin=sí`, prefijo de clave coincidente) |
| Fixes añadidos en la validación | **Sync inversa BLE→binding** (TOFU/tipo 6 → `bindingSyncFromBle` desde `saveBLEAuthConfig`, guard anti-eco) · **Coexistencia radio**: `updateConnParams` (intervalo 30–60 ms, latencia 4, supervision 6 s) al conectar + scan WiFi a 120 ms/canal — sin ellos el scan tiraba el enlace BLE (reproducido y corregido) |
| Validación A2 clásico | Compila (esp32dev_ble); flasheo+prueba pendiente de conectar la placa |
| Fases 2–4 (B5–B10) | Pendientes |
| Push | Cuando el usuario lo indique |

---

*Planificación a partir del BLE v4.1/v5 existente y del portal/wifi_manager v5 — sin implementación en este paso.*
