# Manual de Comandos MQTT — Controladora SWATID A2 / A2v3

**Versión:** v1.0 · **Fecha:** Octubre 2026 · **Firmware de referencia:** v5.0.5
**Empresa:** Smart World And Things SLU · info@swat-id.com · www.swat-id.com

Referencia completa de **todos los comandos** que acepta la controladora por
MQTT, con especial detalle de la **gestión de usuarios** (canal `/vinculo`,
v5.0.5). Los ejemplos de respuesta son capturas reales de placa
(`SWATID_CC641BEABF8C`, 29-Sep / 01-Oct 2026). Complementa al
[Manual de Integración MQTT v1.2](MANUAL-INTEGRACION-MQTT.md) (que incluye
además todos los mensajes que el equipo emite).

---

## Índice

1. [Convenciones](#1-convenciones)
2. [Tipo 0 — Activación de relé](#2-tipo-0--activación-de-relé)
3. [Tipo 1 — Solicitud de información](#3-tipo-1--solicitud-de-información)
4. [Tipo 2 — Comandos de sistema](#4-tipo-2--comandos-de-sistema)
5. [Tipo 3 — Seguridad](#5-tipo-3--seguridad)
6. [Tipo 4 — Sincronización de hora](#6-tipo-4--sincronización-de-hora)
7. [Tipo 5 — Gestión de códigos (PIN/TAG)](#7-tipo-5--gestión-de-códigos-pintag)
8. [Tipo 6 — Gestión BLE (LEGADO)](#8-tipo-6--gestión-ble-legado)
9. [Tipo 7 — Gestión de red](#9-tipo-7--gestión-de-red)
10. [GESTIÓN DE USUARIOS — canal /vinculo](#10-gestión-de-usuarios--canal-vinculo)
11. [Respuesta de validación remota](#11-respuesta-de-validación-remota)
12. [Apéndices: errores, bitmasks](#12-apéndices)

---

## 1. Convenciones

| Concepto | Valor |
|----------|-------|
| `<id>` | Serial fijo del equipo, p. ej. `SWATID_CC641BEABF8C` |
| Topic de comandos | `swatidhome/command/<id>/cmd` (cualquier subtopic sirve) |
| Topic de respuestas | `swatidhome/response/<id>/rx` |
| Canal de usuarios | comandos: `swatidhome/command/<id>/vinculo` · respuestas: `swatidhome/<id>/vinculo` |
| Formato | JSON UTF-8, QoS 0, sin retain |
| Buffer del equipo | 4096 bytes |

**Envelope de comando** (tipos 0–7; el canal `/vinculo` NO usa envelope):

```json
{
  "timestamp": "2026-10-01T10:15:00+01:00",
  "message_id": 123,
  "device": "<id>",
  "message_type": 0,
  "message_info": { }
}
```

- `device` debe coincidir con el serial fijo (o el nombre del equipo).
- Toda ejecución responde en `response/<id>/rx` con `response_type` (0=OK,
  1=error), `message_id` (eco para correlar), `response_info` (texto) y —
  en la gestión de códigos — el objeto estructurado `data`.

---

## 2. Tipo 0 — Activación de relé

```json
"message_type": 0,
"message_info": { "relay_number": 1, "duration": 3.5 }
```

`relay_number` 1|2 · `duration` ≥ 0.5 s. Además de la respuesta, genera un
evento de acceso (`source: REMOTE_COMMAND`) y mensajes `relay_enable`.

## 3. Tipo 1 — Solicitud de información

```json
"message_type": 1
```

Publica el estado completo en `swatidhome/<id>/info` y lo copia en la
respuesta: red, seguridad, pines de teclados, códigos locales y remotos
(con franjas), capacidades.

## 4. Tipo 2 — Comandos de sistema

`message_info` es un **string** (o un objeto con campos extra para `update`):

| Comando | Efecto |
|---------|--------|
| `"reboot"` | Reinicio del equipo |
| `"reset"` | Configuración de fábrica |
| `"update"` | Aplica configuración (`device_name`, `relay_duration`, `use_dhcp`, `static_ip/gateway/subnet/dns`) |
| `"ota_config"` | Configura OTA (`update_url`, `check_interval`) |
| `"ota_check"` | Fuerza comprobación de actualización |

## 5. Tipo 3 — Seguridad

`message_info.security_command`:

| Comando | Parámetros | Efecto |
|---------|------------|--------|
| `block_local_access` | — | Bloquea la validación local |
| `unblock_local_access` | — | Desbloquea y resetea intentos |
| `disable_keyboard_reading` / `enable_keyboard_reading` | — | Ignora / reactiva los teclados |
| `set_block_duration` | `duration_seconds` | Duración del bloqueo automático |
| `set_max_failed_attempts` | `max_attempts` | Umbral de intentos fallidos |

## 6. Tipo 4 — Sincronización de hora

```json
"message_type": 4,
"message_info": { "time_string": "2026-10-01 10:15:00" }
```

En A2v3 escribe también el RTC DS3231. Emite el evento `TIME_SYNC`.

## 7. Tipo 5 — Gestión de códigos (PIN/TAG)

`message_info.action`. Validaciones comunes: `code_type` ∈ {PIN, TAG},
`code_value` 1–16 caracteres, `keyboard_id` 0 (ambos)|1|2, `relay` 1|2.
Las respuestas incluyen el objeto **`data`** además del texto legacy.

**Locales** (siempre activos — `time_slots` se rechaza):

| Acción | Parámetros |
|--------|------------|
| `list_local_codes` | `offset` (0), `limit` (50 máx.) |
| `add_local_code` | `code_type`, `code_value`, `relay`, `keyboard_id` opc. |
| `remove_local_code` | `code_type`, `code_value`, `keyboard_id` opc. |
| `clear_local_codes` | — |

**Remotos** (caché con hasta 4 franjas horarias; sin `time_slots` → siempre activo):

| Acción | Parámetros |
|--------|------------|
| `list_remote_codes` | `offset` (0), `limit` (8, máx. 10) |
| `add_remote_code` | como local + `time_slots[]` opc. (cada franja COMPLETA y válida) |
| `remove_remote_code` | `code_type`, `code_value` |
| `clear_remote_codes` | — |

Franja horaria (todos los campos obligatorios):

```json
{ "start_hour": 8, "start_minute": 0, "end_hour": 20, "end_minute": 0,
  "days_of_week": 31 }
```

Ejemplo real de alta remota con franja L–V 08–20 → respuesta `data`:

```json
{ "scope": "remote", "code_type": "PIN", "code_value": "6666",
  "keyboard_id": 0, "relay": 2, "always_active": false,
  "time_slots_count": 1, "total_codes": 2 }
```

Paginación en listados: `truncated: true` + `next_offset` → repetir con
`offset = next_offset`.

## 8. Tipo 6 — Gestión BLE (LEGADO)

> ⚠️ **Deprecado desde v5.0.5** para integraciones nuevas: la gestión de
> usuarios se hace por el canal `/vinculo` (sección 10). El tipo 6 se
> mantiene por compatibilidad (solo en variante -BLE; su única
> autenticación son las credenciales del broker).

Acciones: `set_superadmin`, `add_user`, `add_user_derived`, `clear_user`,
`clear_superadmin`, `clear_all`, `get_status`, `list_users`.

## 9. Tipo 7 — Gestión de red

`message_info.action`:

| Acción | Resumen |
|--------|---------|
| `get_wifi` | Estado AP/STA/ETH/GSM/MQTT con IPs |
| `set_wifi` | `ap_mode` (0 off / 1 ventana / 2 siempre), `ap_timeout_s`, `ap_pass`, `sta_ssid`+`sta_pass`, `sta_enabled`, `forget_sta` |
| `ap_start` / `ap_stop` | Enciende (con `seconds`) / apaga el AP |
| `wifi_scan` / `get_wifi_scan` | Escaneo asíncrono de redes |
| `get_gsm` | Estado completo del módulo 4G (SIM, registro, señal, datos) |
| `set_gsm` | `enabled`, `pin` (un intento por valor), `pin_disable`, `apn`+`apn_mode`+`apn_user`+`apn_pass` |
| `gsm_rescan` | Reinicia el módulo 4G y re-detecta SIM |
| `get_rtc` | `{present, osf, seeded}` (A2v3) |

---

## 10. GESTIÓN DE USUARIOS — canal /vinculo

Canal dedicado (v5.0.5), uniforme con el resto de productos SWATID.
Disponible en **todas** las variantes, con o sin BLE. **No usa el envelope**
de los tipos 0–7: el payload es el comando directamente.

| Dirección | Topic |
|-----------|--------|
| Backend → equipo | `swatidhome/command/<id>/vinculo` |
| Equipo → backend | `swatidhome/<id>/vinculo` |
| Eventos `BINDING_ADDED/UPDATED/CLEARED` | `swatidhome/events/<id>/ble` |

### 10.1 Modelo de roles

| Rol | Slot | Alta | Baja | Permisos |
|-----|------|------|------|----------|
| **Superadmin de plataforma** | — | Procedimiento de fábrica: web `/binding` del equipo (auth web) | Solo fábrica (web) o rotación firmada por sí misma | Plenos. **Siempre accede**: ningún `unlink`/limpieza de admin o usuarios la borra |
| **Admin de placa** | 0 | TOFU (`link` público con ventana abierta) | Propia (limpia también usuarios) o por la plataforma | Plenos (255) |
| **Usuarios** | 1–5 | Firmada por admin o plataforma | Propia, o por admin/plataforma | Bitmask (ver 12.2) |

Las claves son de **64 bytes (128 hex)**; solo viajan en el alta. Después,
todo se autoriza por firma HMAC sobre un nonce del equipo.

### 10.2 Comandos públicos (sin firma)

#### `devinfo` — estado y capacidades

```json
{"command":"devinfo","type":"GET"}
```

Respuesta real:

```json
{ "ok": true, "serial": "SWATID_CC641BEABF8C", "firmware_version": "v5.0.5",
  "capabilities": { "board": "KC868-A2v3 (ESP32-S3)", "relays": 2,
    "keyboards": 2, "digital_inputs": 2, "slots": 6, "ble": false,
    "gsm": true, "mqtt": true },
  "superadmin_registered": false, "admin_registered": false,
  "platform_superadmin_registered": false,
  "available_for_linking": true, "slots_used": 0, "slots_available": 6,
  "remote_link_window_open": true, "remote_link_window_s": 540 }
```

`superadmin_registered` es el **admin de placa** (alias de compatibilidad
X1); la plataforma se informa aparte y nunca entra en `slots`.

#### `challenge` — nonce para firmar

```json
{"command":"challenge","type":"GET"}
```

→ `{"ok":true,"serial":"<id>","nonce":"<64 hex>","expires_s":60}`

Un solo nonce vivo por equipo; pedir otro invalida el anterior; **se consume
al primer intento de uso** (válido o no) — anti-replay. Flujo recomendado:
pedir challenge e inmediatamente enviar el sobre firmado.

#### `link` (TOFU) — alta del admin de placa

```json
{"command":"link","type":"POST","key":"<128 hex>"}
```

Solo si **no hay admin** y la **ventana remota está abierta**. La ventana se
abre 10 min al arrancar el equipo, o manualmente desde la web `/binding`
(botón «Abrir 10 min»); se cierra sola al completarse el TOFU.

Éxito (captura real):

```json
{"ok":true,"serial":"SWATID_CC641BEABF8C","linked":true,"slot":0,
 "role":"admin","role_alias":"superadmin"}
```

Errores: `device_already_owned`, `remote_linking_window_closed`,
`invalid_key`, `missing_key`.

### 10.3 Sobre autenticado (resto de comandos)

Todos los demás comandos viajan FIRMADOS dentro de este sobre:

```json
{"nonce":"<64 hex>","hmac":"<64 hex>","payload":"<comando JSON como string>"}
```

```text
hmac = HMAC-SHA256( clave_del_firmante, unhex(nonce) || utf8(payload) )
```

El equipo prueba las claves en orden **plataforma → admin → usuarios 1..5**;
la primera que verifica identifica al firmante y sus privilegios.
⚠️ `payload` debe ser **exactamente** el string firmado (sin reformatear:
usar separadores compactos y no re-serializar).

Firma de ejemplo (Python):

```python
import hashlib, hmac, json
payload = json.dumps({"command": "link", "type": "POST", "slot": 2,
                      "new_key": NEW_KEY, "name": "Juan", "permissions": 3},
                     separators=(",", ":"))
mac = hmac.new(bytes.fromhex(KEY_FIRMANTE),
               bytes.fromhex(nonce) + payload.encode(),
               hashlib.sha256).hexdigest()
envelope = {"nonce": nonce, "hmac": mac, "payload": payload}
```

### 10.4 Comandos firmados

#### Alta / actualización de usuario (firma: admin o plataforma)

```json
{"command":"link","type":"POST","slot":2,"new_key":"<128 hex>",
 "name":"Juan","permissions":3}
```

`slot` 1–5. Si el slot ya estaba ocupado se actualiza (evento
`BINDING_UPDATED` en lugar de `BINDING_ADDED`). Respuesta real:

```json
{"ok":true,"serial":"<id>","linked":true,"slot":2,"role":"user",
 "name":"Juan","permissions":3}
```

Si firma un usuario normal: `{"ok":false,"error":"admin_required"}`.

#### Rotación de clave propia (firma: cualquiera)

```json
{"command":"update_vinculo","new_key":"<128 hex>"}
```

Rota la clave del firmante (usuario, admin o plataforma). Respuesta:
`{"ok":true,"updated":true,"slot":N,"role":"<rol>"}`.

#### Bajas (`unlink`)

| Payload | Firma | Efecto |
|---------|-------|--------|
| `{"command":"unlink"}` | usuario | Baja propia |
| `{"command":"unlink"}` | admin | Baja del admin **y de todos los usuarios**. La plataforma queda INTACTA |
| `{"command":"unlink"}` | plataforma | Rechazado: `factory_procedure_required` (la plataforma solo se gestiona en fábrica) |
| `{"command":"unlink","slot":N}` (1–5) | admin o plataforma | Baja del usuario N |
| `{"command":"unlink","slot":0}` | **solo plataforma** | Quita al admin (y usuarios) |

Respuestas reales: `{"ok":true,"unlinked":"admin_and_users"}` ·
`{"ok":true,"unlinked":"2"}`.

#### Listado (firma: admin o plataforma)

```json
{"command":"list"}
```

```json
{ "ok": true, "serial": "<id>", "platform_superadmin_registered": true,
  "bindings": [
    { "slot": 0, "role": "admin", "name": "admin", "permissions": 255 },
    { "slot": 2, "role": "user", "name": "Juan", "permissions": 3 }
  ] }
```

Nunca incluye claves (ni completas ni parciales).

### 10.5 Flujo completo de puesta en marcha (validado en placa)

```text
1. (Equipo recién arrancado: ventana TOFU abierta 10 min)
2. devinfo                        → available_for_linking: true
3. link POST key=<clave admin>    → linked slot 0 (la ventana se cierra)
4. challenge → sobre firmado por admin:
     link slot 1..5 new_key=<clave usuario> name=... permissions=...
5. (Opcional, fábrica) web /binding → provisionar clave de plataforma
6. Operación normal: cada comando = challenge + sobre firmado
```

Recuperación si el admin se pierde: la **plataforma** puede
`unlink slot 0` y re-abrir el ciclo TOFU (la ventana se abre desde la web
`/binding` o reiniciando el equipo). Si tampoco hay plataforma: botón web
«Abrir 10 min» y nuevo TOFU.

### 10.6 Procedimiento de fábrica (web `/binding`)

Página autenticada (contraseña web del equipo) con: estado de los tres
roles, apertura/cierre de la ventana remota y **provisión / rotación /
borrado de la clave de superadmin de plataforma** (128 hex). Es la única
vía de alta y de baja de la plataforma (además de su propia rotación
firmada por MQTT).

### 10.7 BLE (variante -BLE)

Las **mismas claves** valen para el challenge-response BLE (FF0B/FF01,
`SHA256(key ‖ nonce16)`): plataforma (se prueba primero; sesión
`PLATFORM` con permisos plenos), admin (`SUPERADMIN`) y usuarios. La clave
«superadmin» BLE de equipos antiguos se migra automáticamente como admin
de placa en el primer arranque v5.0.5.

### 10.8 Eventos

En `swatidhome/events/<id>/ble`:

```json
{"event":"BINDING_ADDED","device":"<id>","slot":2,"role":"user",
 "name":"Juan","source":"MQTT"}
```

`event` ∈ `BINDING_ADDED` | `BINDING_UPDATED` | `BINDING_CLEARED`.
`slot` −1 = plataforma, 0 = admin, 1–5 = usuarios.

---

## 11. Respuesta de validación remota

La plataforma responde a la solicitud de acceso
(`swatidhome/command/<id>/access`) publicando en cualquier subtopic de
`command/<id>/` (recomendado `.../granted`):

```json
{ "device": "<id>", "message_id": 456, "response": "APPROVED",
  "code_type": "PIN", "code_value": "1234",
  "relay_number": 1, "duration": 3.0, "reason": "Usuario autorizado" }
```

`response` APPROVED|DENIED · responder en **< 5 s** · `message_id` = eco de
la solicitud. (Formato legado `access_granted` con `duration` en ms: solo
compatibilidad.)

---

## 12. Apéndices

### 12.1 Errores del canal /vinculo

| Error | Significado |
|-------|-------------|
| `missing_key` / `invalid_key` | TOFU sin clave o clave ≠ 128 hex |
| `invalid_json` | Mensaje o payload no parseable |
| `auth_required` | `link` sin TOFU posible y sin firma |
| `invalid_signature` | Ninguna clave verifica el HMAC |
| `nonce_expired` | Nonce caducado, consumido o no emitido |
| `device_already_owned` | Ya hay admin de placa |
| `remote_linking_window_closed` | Ventana TOFU cerrada |
| `invalid_new_key_or_slot` | Slot fuera de 1–5 o clave mal formada |
| `admin_required` | El comando exige admin o plataforma |
| `platform_required` | Solo la plataforma (p. ej. `unlink slot 0`) |
| `factory_procedure_required` | La plataforma no se autodesvincula por MQTT |
| `not_linked` | El slot indicado está vacío |
| `nvs_write_failed` | Error de persistencia |
| `unknown_command` | Comando no reconocido |

### 12.2 Bitmask de permisos de usuario

| Bit | Permiso |
|-----|---------|
| `0x01` | Relés |
| `0x02` | Modo (torno…) |
| `0x04` | Códigos |
| `0x08` | Red |
| `0xFF` | Plenos (admin) |

### 12.3 Bitmask de días (franjas horarias, tipo 5)

| Bit | Día |
|-----|-----|
| 1 | Lunes |
| 2 | Martes |
| 4 | Miércoles |
| 8 | Jueves |
| 16 | Viernes |
| 32 | Sábado |
| 64 | Domingo |

31 = L–V · 96 = fin de semana · 127 = todos.

---

*Manual de Comandos MQTT v1.0 — Octubre 2026 — Smart World And Things SLU.*
