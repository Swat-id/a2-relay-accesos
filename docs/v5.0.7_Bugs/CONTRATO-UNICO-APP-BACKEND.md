# Contrato único A2 / A2v3 — BLE (App) + MQTT (Backend)

**Versión:** v1.0 · **Fecha:** Octubre 2026 · **Firmware de referencia:** v5.0.7
**Empresa:** Smart World And Things SLU · info@swat-id.com · www.swat-id.com

Documento único de referencia: **toda** la mensajería del equipo —
**Parte I: canal BLE** (para la App) y **Parte II: canal MQTT** (para el
backend). Los detalles ampliados viven en los manuales anexos (§III), pero
este documento es autosuficiente para integrar ambos canales.

---

# PARTE 0 — Identidad del equipo (común)

| Campo | A2 clásico | A2v3 |
|-------|-----------|------|
| Nombre BLE / serial | `SWATID_<12 hex MAC>` | igual |
| Anuncio BLE: manufacturer data `0xFFFF` | `"A2"` | `"A2V3"` |
| `device_type` (FF08 / info MQTT) | `SWATID-A2` | **`SWATID-A2V3`** (desde v5.0.7) |
| `device_family` | `SWATID-A2` | `SWATID-A2` |
| `firmware_variant` | `BLE` | **`S3-BLE`** |
| Relés / teclados / DI | 2 / 2 / 2 | 2 / 2 / 2 |

**Clasificación correcta de un equipo (App), en orden:**
1. Manufacturer data `0xFFFF` del anuncio → modelo (`A2`, `A2V3`).
2. Sin mfg data → **no asumir modelo**: puede ser X1, B16, WS-1… Conectar y
   leer `FF08.device_type` (si no existe FF08, no es de la familia A2).

Anuncio BLE (reparto explícito de los 31 B por paquete, v5.0.7):
paquete ADV = flags + **nombre completo**; scan response = UUID `FF00` +
manufacturer data. Re-advertising automático tras cualquier desconexión
(+ watchdog de 5 s).

---

# PARTE I — Canal BLE (App)

Servicio `0000FF00-0000-1000-8000-00805F9B34FB`; características
`0000FFxx-…` (xx = 01..0B). Respuestas cortas: texto `OK:`/`ERROR:` por
notify (suscribirse ANTES de escribir). Respuestas largas (FF05, FF0A, FF08/
FF09 grandes): fragmentos por notify terminados en **EOT 0x04**.

## I.1 Autenticación (FF0B + FF01)

| Paso | Operación |
|------|-----------|
| TOFU (equipo virgen) | WRITE FF01 ← clave de 64 B generada por la App → `OK:SUPERADMIN_REGISTERED:TOKEN:<16hex>` |
| Sesión (siempre) | READ FF0B → nonce 16 B · WRITE FF01 ← `SHA256(clave ‖ nonce)` (32 B) → `OK:TOKEN:…` / `OK:AUTHENTICATED:TOKEN:…` |
| Caducidad | 5 min de inactividad o desconexión |
| Orden de verificación | plataforma → admin de placa → usuarios 1–5 |
| Errores | `ERROR:INVALID_RESPONSE` (clave de otro dueño) · `ERROR:NOT_AUTHENTICATED` |

Permisos (bitmask): `0x01` relés · `0x02` modo · `0x04` códigos · `0x08` red
· `0xFF` admin. La clave es única por móvil y NUNCA vuelve a viajar tras el
TOFU/alta. El mismo material firma en MQTT `/vinculo` (sincronización
bidireccional en firmware).

**Equipo con dueño**: el alta de un segundo móvil la hace el admin (II.4
`/vinculo link`), o se restablecen vínculos desde la web del equipo
(`/binding` → «Restablecer vínculos», deja la plataforma intacta y abre la
ventana TOFU).

## I.2 Características y formatos

| Char | Operación | Formato |
|------|-----------|---------|
| **FF02 relé** | WRITE | `[relay 1-2][action 0=OFF,1=ON,2=PULSE][durMs BE 2B opc.]` → `OK:RELAY_ON/PULSE/OFF` |
| **FF03 modo** | READ/WRITE | 1 byte: 0=normal, 1=torno → `OK:MODE_*` |
| **FF04 alta código** | WRITE | `[tipo 0=PIN,1=TAG][teclado 0-2][relé 1-2][código ASCII 1-16]` → `OK:CODE_ADDED:<total>` · `ERROR:CODE_EXISTS/STORAGE_FULL/INVALID_*` |
| **FF05 red** | READ / WRITE | READ → estado unificado. WRITE JSON `{"cmd":…}`: `status`, `wifi_scan_start` (1/10 s), `wifi_scan_get`, `wifi_sta_set{ssid,pass}`, `wifi_sta_forget`, `wifi_ap_set{mode,timeout_s,pass}`, `wifi_ap_stop`, `eth_set{dhcp,ip,gw,mask,dns}` (reinicia 2 s), `exit_config`. WRITE binario 17 B = legado ETH |
| **FF06 duración** | READ/WRITE | uint32 little-endian, ms → `OK:RELAY_TIME_SET` |
| **FF07 estado** | READ | `{relay1,relay2,mode,auth,user,eth_connected,mqtt_connected}` |
| **FF08 devinfo** | READ (pública) | `{device_type, device_family, serial, firmware_version, firmware_variant, protocol_version, capabilities{relays,wiegand_inputs,digital_inputs,ble_users}, superadmin_registered, active_users}` |
| **FF09 full info** | READ | `{type,sn,name,fw, net{ip,gw,mask,dhcp,eth}, st{r1,r2,dur,mode,mqtt}, códigos}` |
| **FF0A códigos** | WRITE `{"page":N}` | `{n,m,p,ps,tp,more,c:[{i,t,v,k,r}]}` (3/página, EOT) |
| **FF0B challenge** | READ (pública) | nonce 16 B |

**Lock AP↔BLE** (FF05): `wifi_scan_start`/`wifi_sta_set` apagan el AP del
equipo y bloquean el portal web de red (HTTP 409) mientras dura la sesión;
`exit_config`/desconexión restauran la política del AP.

## I.3 Banco de pruebas detección→acceso

Suite automatizada `banco_ble.py` (casos D1–D4 detección/clasificación,
C1–C5 contrato GATT/identidad, A1–A5 acceso con negativo de clave incorrecta,
R1–R2 reconexión) — ver [BANCO-PRUEBAS-BLE.md](BANCO-PRUEBAS-BLE.md).

---

# PARTE II — Canal MQTT (Backend)

| Parámetro | Valor |
|-----------|-------|
| Broker (credenciales del equipo) | autentican TODOS los comandos tipos 0–7 |
| El equipo se suscribe a | `swatidhome/command/<serial>/#` |
| Respuestas a comandos | `swatidhome/response/<serial>/rx` |
| Envelope de comando | `{timestamp, message_id, device:"<serial>", message_type, message_info}` |
| Buffer | 4096 B · QoS 0 · sin retain |

## II.1 Mensajes que EMITE el equipo

| Topic | Contenido |
|-------|-----------|
| `keepalive/<id>` | `{status:"online", uptime}` cada 60 s |
| `command/<id>/access` | **Solicitud de validación remota** (PIN/TAG no resuelto localmente): `message_type:0`, `message_info{source,code_type,code_value,keyboard_id,keyboard_name,keyboard_pins,request_relay,max_duration}` — responder en <5 s (II.3) |
| `events/<id>/access` · `events/<id>/failed_access` | Accesos OK/KO (`source: LOCAL/REMOTE_APPROVED/…`, `reason`) |
| `events/<id>/turnstile` | Eventos de torno |
| `<id>/relay_enable` | Estado de relés ON/OFF (`origen: SYSTEM/TIMEOUT`) |
| `<id>/digital_input` | `event:"digital_input_trigger"` (pulsador) y `event:"door_contact"` (imán: `door_state`, `reason: boot/change/sync`) |
| `<id>/info` | Info completa (respuesta a tipo 1) |
| `response/<id>/rx` | ACK de todo comando: `{message_id(eco), response_type 0/1, response_info, data{}}` |
| `errors/<id>/rx` | Errores/avisos (`error_code` 3=arranque, 8=memoria…) |
| `events/<id>/ble` | `AUTH_*`, `RELAY_*`, `BINDING_ADDED/UPDATED/CLEARED`, `CODE_ADDED` |
| `<id>/events` | `TIME_SYNC` |
| `<id>/vinculo` | Respuestas del canal de vinculación (II.4) |

## II.2 Comandos que ACEPTA (tipos 0–7)

| Tipo | Función | `message_info` |
|------|---------|----------------|
| 0 | **Relé** | `{relay_number: 1-2, duration: ≥0.5}` → ACK + evento + relay_enable. ⚠️ El A2 NO valida sobres firmados `swatid-mqtt/2`: la orden es este JSON plano |
| 1 | Info | — (publica `<id>/info`) |
| 2 | Sistema | string: `reboot`, `reset`, `update` (+campos), `ota_config`, `ota_check` |
| 3 | Seguridad | `security_command`: `block/unblock_local_access`, `disable/enable_keyboard_reading`, `set_block_duration{duration_seconds}`, `set_max_failed_attempts{max_attempts}` |
| 4 | Hora | `{time_string:"YYYY-MM-DD HH:MM:SS"}` (A2v3 también RTC) |
| 5 | **Códigos** | acciones con respuesta estructurada en `data`: `list_local_codes{offset,limit≤50}`, `add_local_code{code_type,code_value,relay,keyboard_id}` (time_slots → error), `remove_local_code`, `clear_local_codes`, `list_remote_codes{offset,limit≤10}`, `add_remote_code{…,time_slots[≤4]}` (franja completa: start/end hour+minute, days_of_week 1–127; sin franjas = siempre activo), `remove_remote_code`, `clear_remote_codes` |
| 6 | BLE legado (builds -BLE) | **Deprecado** para integraciones nuevas → usar `/vinculo` |
| 7 | Red | `get_wifi`, `set_wifi{ap_mode,ap_timeout_s,ap_pass,sta_ssid,sta_pass,sta_enabled,forget_sta}`, `ap_start{seconds}`, `ap_stop`, `wifi_scan`, `get_wifi_scan`, `get_gsm`, `set_gsm{enabled,pin,pin_disable,apn,apn_mode,…}`, `gsm_rescan`, `get_rtc` |

Validaciones tipo 5: `code_type`∈{PIN,TAG}; `code_value` 1–16; `keyboard_id`
0/1/2; `relay` 1/2. Paginación: `truncated:true` + `next_offset`.

## II.3 Respuesta de validación remota (plataforma → equipo)

Publicar en `command/<id>/granted`:

```json
{ "device":"<serial>", "message_id": <eco>, "response":"APPROVED|DENIED",
  "code_type":"PIN", "code_value":"1234", "relay_number":1,
  "duration":3.0, "reason":"…" }
```

Timeout del equipo: 5 s (torno); formato legado `access_granted` (ms) solo
compatibilidad.

## II.4 Canal de vinculación `/vinculo` (gestión de usuarios)

| Dirección | Topic |
|-----------|--------|
| → equipo | `command/<serial>/vinculo` |
| ← equipo | `<serial>/vinculo` |

Sin envelope de tipos. **Públicos**: `{"command":"devinfo","type":"GET"}` ·
`{"command":"challenge","type":"GET"}` (nonce 64 hex, 60 s, se consume al
primer uso) · `{"command":"link","type":"POST","key":"<128hex>"}` (TOFU admin
si no hay dueño y la ventana remota está abierta: 10 min tras boot o web).

**Firmados** — sobre `{"nonce","hmac","payload":"<cmd JSON string exacto>"}`
con `hmac = HMAC-SHA256(clave, unhex(nonce) ‖ utf8(payload))`:

| Payload | Firma | Efecto |
|---------|-------|--------|
| `link POST {slot 1-5, new_key, name, permissions}` | admin/plataforma | Alta/actualización de usuario |
| `update_vinculo {new_key}` | cualquiera | Rotación propia |
| `unlink` | usuario / admin | Baja propia (admin → limpia usuarios; plataforma intacta) |
| `unlink {slot}` | admin/plataforma (`slot:0` solo plataforma) | Baja de tercero |
| `list` | admin/plataforma | Vínculos sin claves |

Roles: plataforma (fábrica, **siempre accede**, solo gestionable en la web
`/binding`), admin de placa (slot 0, TOFU), usuarios 1–5. Errores:
`device_already_owned`, `remote_linking_window_closed`, `invalid_signature`,
`nonce_expired`, `admin_required`, `platform_required`,
`factory_procedure_required`, `not_linked`, `nvs_write_failed`, …

---

# PARTE III — Anexos y manuales de detalle

| Documento | Contenido ampliado |
|-----------|--------------------|
| [MANUAL-INTEGRACION-APP-BLE.md](../v5.0.6_BLE/MANUAL-INTEGRACION-APP-BLE.md) | BLE completo con ejemplos y checklist App |
| [CONTRATO-FF05-RED.md](../v5.0.6_BLE/CONTRATO-FF05-RED.md) | FF05 en detalle (lock AP, errores) |
| [MANUAL-INTEGRACION-MQTT.md](../v5.0.5_users/MANUAL-INTEGRACION-MQTT.md) (v1.2 + PDF) | MQTT completo: todos los JSON de ejemplo |
| [MANUAL-COMANDOS-MQTT.md](../v5.0.5_users/MANUAL-COMANDOS-MQTT.md) | Referencia de comandos con capturas reales |
| [RESPUESTA-INCIDENCIAS-APP.md](RESPUESTA-INCIDENCIAS-APP.md) | Análisis EQ-01…EQ-05 + evidencia GATT |
| [BANCO-PRUEBAS-BLE.md](BANCO-PRUEBAS-BLE.md) | Suite detección→acceso (D/C/A/R) |

---

*Contrato único App+Backend v1.0 — Octubre 2026 — Smart World And Things SLU.*
