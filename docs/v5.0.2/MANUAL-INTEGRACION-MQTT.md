# Manual de Integración MQTT — Controladora SWATID A2 / A2v3

**Versión del manual:** v1.0 · **Fecha:** Septiembre 2026
**Firmware de referencia:** v5.0.2 (variantes: base, -4G, -BLE, -S3)
**Empresa:** Smart World And Things SLU · info@swat-id.com · www.swat-id.com

Documento de referencia para integradores: **toda** la mensajería MQTT del
equipo — lo que publica el dispositivo y los comandos que acepta. Extraído y
verificado contra el firmware v5.0.2.

---

## Índice

1. [Convenciones generales](#1-convenciones-generales)
2. [Mapa de topics](#2-mapa-de-topics)
3. [Mensajes que EMITE el dispositivo](#3-mensajes-que-emite-el-dispositivo)
   - 3.1 Keepalive
   - 3.2 Solicitud de validación remota (acceso)
   - 3.3 Eventos de acceso
   - 3.4 Eventos de acceso fallido
   - 3.5 Eventos de torno
   - 3.6 Estado de relés
   - 3.7 Entradas digitales: pulsador (`digital_input_trigger`)
   - 3.8 Entradas digitales: puerta (`door_contact`)
   - 3.9 Información del dispositivo
   - 3.10 Respuestas a comandos
   - 3.11 Errores y avisos del sistema
   - 3.12 Eventos de códigos
   - 3.13 Eventos BLE
   - 3.14 Evento de sincronización de hora
4. [Comandos que RECIBE el dispositivo](#4-comandos-que-recibe-el-dispositivo)
   - 4.0 Tipo 0 — Activación de relé
   - 4.1 Tipo 1 — Solicitud de información
   - 4.2 Tipo 2 — Comandos de sistema
   - 4.3 Tipo 3 — Seguridad
   - 4.4 Tipo 4 — Sincronización de hora
   - 4.5 Tipo 5 — Gestión de códigos
   - 4.6 Tipo 6 — Gestión BLE
   - 4.7 Tipo 7 — Gestión de red (WiFi / 4G / RTC)
5. [Respuesta de validación remota (plataforma → equipo)](#5-respuesta-de-validación-remota)
6. [Buenas prácticas de integración](#6-buenas-prácticas-de-integración)
7. [Compatibilidad y versionado](#7-compatibilidad-y-versionado)

---

## 1. Convenciones generales

| Concepto | Valor |
|----------|-------|
| Identificador del equipo `<id>` | Serial fijo derivado de la MAC, p. ej. `SWATID_A0461CA0ED30`. Inmutable. |
| Prefijo de topics | `swatidhome/` |
| Suscripción del equipo | `swatidhome/command/<id>/#` |
| QoS | 0 (publicación simple), sin retain |
| Formato | JSON UTF-8 |
| `timestamp` | ISO-8601 local, p. ej. `2026-09-22T10:15:00+01:00` |
| `message_id` | Contador del emisor; en respuestas, eco del id del comando |
| Buffer MQTT del equipo | 2048 bytes (payloads mayores se descartan) |

**Envelope de comando** (todo lo que la plataforma envía al equipo):

```json
{
  "timestamp": "2026-09-22T10:15:00+01:00",
  "message_id": 123,
  "device": "SWATID_A0461CA0ED30",
  "message_type": 0,
  "message_info": { }
}
```

- `device` debe ser el serial fijo (o el nombre editable del equipo); si no
  coincide, el comando se ignora.
- `message_type` selecciona la familia de comando (sección 4).
- `message_info` lleva los parámetros (objeto o string según el tipo).

## 2. Mapa de topics

### El equipo PUBLICA en

| Topic | Contenido | Sección |
|-------|-----------|---------|
| `swatidhome/keepalive/<id>` | Latido cada 60 s | 3.1 |
| `swatidhome/command/<id>/access` | Solicitud de validación remota (¡el equipo publica en su propio árbol command!) | 3.2 |
| `swatidhome/events/<id>/access` | Accesos concedidos | 3.3 |
| `swatidhome/events/<id>/failed_access` | Accesos fallidos | 3.4 |
| `swatidhome/events/<id>/turnstile` | Eventos del modo torno | 3.5 |
| `swatidhome/<id>/relay_enable` | Cambios de estado de relés | 3.6 |
| `swatidhome/<id>/digital_input` | Entradas digitales (trigger y door_contact) | 3.7, 3.8 |
| `swatidhome/<id>/info` | Información completa del equipo | 3.9 |
| `swatidhome/response/<id>/rx` | Respuestas a comandos | 3.10 |
| `swatidhome/errors/<id>/rx` | Errores y avisos | 3.11 |
| `swatidhome/events/<id>/codes` | Eventos de gestión de códigos | 3.12 |
| `swatidhome/events/<id>/ble` | Eventos BLE (solo variante -BLE) | 3.13 |
| `swatidhome/<id>/events` | Evento TIME_SYNC | 3.14 |

### El equipo SE SUSCRIBE a

| Topic | Uso |
|-------|-----|
| `swatidhome/command/<id>/#` | Todos los comandos (sección 4) y las respuestas de validación (sección 5). La plataforma puede usar cualquier subtopic; se recomienda `.../cmd` para comandos y `.../granted` para respuestas de validación. |

## 3. Mensajes que EMITE el dispositivo

### 3.1 Keepalive — `swatidhome/keepalive/<id>` (cada 60 s)

```json
{ "status": "online", "uptime": 1234567 }
```

`uptime` en milisegundos desde el arranque. Recomendación: marcar el equipo
offline si no llegan 2-3 latidos consecutivos.

### 3.2 Solicitud de validación remota — `swatidhome/command/<id>/access`

Emitida cuando un código presentado no se resuelve localmente (o el modo de
validación es «remoto primero»). La plataforma debe responder en **< 5 s**
(sección 5).

Modo normal:

```json
{
  "timestamp": "…", "message_id": 456,
  "device": "SWATID_A0461CA0ED30", "device_name": "Puerta Norte",
  "message_type": 0,
  "message_info": {
    "source": "AUTO",
    "code_type": "PIN",          // "PIN" | "TAG"
    "code_value": "1234",
    "keyboard_id": 1,            // 1 | 2
    "keyboard_name": "WIEGAND1",
    "keyboard_pins": "5/6",
    "request_relay": 1,
    "max_duration": 2.0
  }
}
```

Modo torno: añade `"mode": "turnstile"`, `message_info.actual_relay` (relé
que abrirá realmente) y el objeto `turnstile_info`
(`enabled`, `keyboard1_relay`, `keyboard2_relay`, `timeout`, `pending_request`).

### 3.3 Evento de acceso — `swatidhome/events/<id>/access`

```json
{
  "timestamp": "…", "message_id": 457,
  "device": "Puerta Norte", "serial": "SWATID_A0461CA0ED30",
  "event_type": "ACCESS_EVENT",
  "success": true,
  "source": "LOCAL",             // LOCAL | REMOTE | REMOTE_APPROVED | WEB | MQTT_RELAY | REMOTE_COMMAND
  "code_type": "PIN", "code_value": "1234",
  "keyboard_id": 1, "keyboard_name": "WIEGAND1"   // si aplica
}
```

### 3.4 Acceso fallido — `swatidhome/events/<id>/failed_access`

Como 3.3 con `event_type: "FAILED_ACCESS"` y además:
`reason` (`INVALID_CODE` | `BLOCKED` | `REMOTE_DENIED: <detalle>` …),
`failed_attempts`, `max_attempts`.

### 3.5 Evento de torno — `swatidhome/events/<id>/turnstile`

```json
{
  "timestamp": "…", "message_id": 458,
  "device": "SWATID_A0461CA0ED30", "device_name": "Puerta Norte",
  "message_type": 1, "mode": "turnstile",
  "event_type": "ACCESS_GRANTED",      // ACCESS_GRANTED | ACCESS_DENIED
  "event_info": {
    "code_type": "PIN", "code_value": "1234",
    "keyboard_id": 1, "keyboard_name": "WIEGAND1",
    "success": true,
    "reason": "TORNO_LOCAL",           // TORNO_LOCAL | TORNO_REMOTE | TORNO_REMOTE_LOCAL | TORNO_LOCAL_FALLBACK_MQTT_FAILED | TIMEOUT | MQTT_PUBLISH_FAILED | …
    "relay_opened": 1, "duration": 2.0
  },
  "turnstile_info": { "enabled": true, "keyboard1_relay": 1, "keyboard2_relay": 2, "timeout": 5000 }
}
```

### 3.6 Estado de relés — `swatidhome/<id>/relay_enable`

En cada activación y en el apagado automático:

```json
{ "timestamp": "…", "resultado": "OK", "rele": 1,
  "estado": "ON",                 // ON | OFF
  "origen": "SYSTEM",             // SYSTEM (activación) | TIMEOUT (apagado)
  "message_id": "88", "request_id": "12" }
```

### 3.7 Entrada digital tipo pulsador — `swatidhome/<id>/digital_input`

`event: "digital_input_trigger"` (contrato estable desde v3.x):

```json
{ "timestamp": "…", "event": "digital_input_trigger",
  "input": 1, "gpio": 16, "relay": 1,
  "duration_ms": 2000, "duration": 2.0, "message_id": "89" }
```

Casos especiales del modo Inverso: activación mantenida
`duration_ms: 4294967295`; desactivación `duration_ms: 0`.

### 3.8 Entrada digital tipo puerta — `swatidhome/<id>/digital_input` (v5.0.2)

`event: "door_contact"` — **mismo topic, evento distinto**; nunca incluye
`relay` ni `duration`:

```json
{ "timestamp": "…", "event": "door_contact",
  "input": 2, "gpio": 17,
  "door_state": "open",          // open | closed
  "raw_level": "HIGH", "open_level": "HIGH",
  "reason": "change",            // boot | change | sync
  "message_id": "90" }
```

`reason=boot`/`sync` es un estado absoluto (no una transición): `boot` al
arrancar; `sync` tras recuperar el broker (pudieron perderse cambios).

### 3.9 Información del dispositivo — `swatidhome/<id>/info`

Respuesta al comando tipo 1 (también se copia en `response/<id>/rx`). Campos
principales: `device`, `serial`, `ip`, `mac`, `firmware_version`, `use_dhcp`,
`relay_duration`; objeto `security` (bloqueos, intentos); objeto `keyboards`
(pines Wiegand); array `stored_codes` (códigos locales completos); objeto
`local_codes_info` (`count`, `max_capacity`, `available`); array
`stored_remote_codes` (con `time_slots` y `days_string`); objeto
`remote_codes_info`. Con IP estática añade `static_ip/gateway/subnet/dns`.

### 3.10 Respuesta a comandos — `swatidhome/response/<id>/rx`

Toda ejecución de comando produce una respuesta:

```json
{ "timestamp": "…", "response_id": 31, "message_id": 123,
  "device": "Puerta Norte", "serial": "SWATID_A0461CA0ED30",
  "response_type": 0,             // 0 = OK, 1 = error
  "response_info": "relay 1 activated",
  "relay": 1 }                    // solo en comandos de relé
```

`message_id` es el eco del comando original — usarlo para correlar.
`response_info` puede ser texto o un JSON serializado (get_wifi, get_gsm,
list_local_codes, get_status BLE…).

### 3.11 Errores y avisos — `swatidhome/errors/<id>/rx`

```json
{ "timestamp": "…", "message_id": 91, "device": "Puerta Norte",
  "serial": "SWATID_A0461CA0ED30", "error_code": 3,
  "description": "Dispositivo iniciado - Dual Wiegand v5.0.2" }
```

| `error_code` | Significado |
|--------------|-------------|
| 3 | Arranque del equipo (informativo) |
| 4 | Cambio de seguridad desde web (bloqueos) |
| 5 | Bloqueo temporal levantado automáticamente |
| 7 | Mensaje de validación remota inválido |
| 8 | Memoria baja |
| 9 | (reservado histórico) |

### 3.12 Eventos de códigos — `swatidhome/events/<id>/codes`

Alta de código desde BLE (variante -BLE): `event: "CODE_ADDED"` con
`type`, `value`, `keyboard_id`, `relay`, `source: "BLE"`, `timestamp`.

### 3.13 Eventos BLE — `swatidhome/events/<id>/ble` (solo variante -BLE)

`event` ∈ `AUTH_SUCCESS`, `AUTH_FAILED`, `AUTH_TIMEOUT`, `RELAY_ACTIVATED`,
`RELAY_DEACTIVATED`, `USER_ADDED`, `USER_ADDED_DERIVED`, `SUPERADMIN_SET`,
`BINDINGS_CLEARED`. Campos habituales: `device`, `user`/`slot`/`name` según
evento, `source` (`BLE`|`MQTT`|`WEB`), `timestamp`.

### 3.14 Sincronización de hora — `swatidhome/<id>/events`

Tras aceptar un comando tipo 4: `event_type: "TIME_SYNC"` con `time_string` y
`source: "MQTT"`.

## 4. Comandos que RECIBE el dispositivo

Publicar en `swatidhome/command/<id>/cmd` (cualquier subtopic sirve) con el
envelope de la sección 1. Todas las familias responden por
`response/<id>/rx` (3.10).

### 4.0 Tipo 0 — Activación de relé

```json
"message_type": 0,
"message_info": { "relay_number": 1, "duration": 3.5 }
```

`relay_number` 1|2; `duration` ≥ 0.5 s. Respuesta: `"relay 1 activated"`.
Genera además evento de acceso (`source: REMOTE_COMMAND`) y mensajes
`relay_enable`.

### 4.1 Tipo 1 — Solicitud de información

```json
"message_type": 1
```

Publica el estado completo en `<id>/info` (3.9) y lo copia en la respuesta.

### 4.2 Tipo 2 — Comandos de sistema (`message_info` es un **string**)

| `message_info` | Efecto |
|----------------|--------|
| `"reboot"` | Reinicio del equipo |
| `"reset"` | Restaura configuración de fábrica |
| `"update"` | Aplica configuración: el mismo mensaje puede llevar campos extra en un objeto `message_info` con `device_name`, `relay_duration`, `use_dhcp`, `static_ip/gateway/subnet/dns` |
| `"ota_config"` | Configura OTA (`update_url`, `check_interval` en un objeto) |
| `"ota_check"` | Fuerza comprobación de actualización |

### 4.3 Tipo 3 — Seguridad

`message_info.security_command`:

| Comando | Parámetros | Efecto |
|---------|------------|--------|
| `block_local_access` | — | Bloquea validación local |
| `unblock_local_access` | — | Desbloquea y resetea intentos |
| `disable_keyboard_reading` | — | Ignora los teclados |
| `enable_keyboard_reading` | — | Reactiva los teclados |
| `set_block_duration` | `duration_seconds` | Duración del bloqueo automático |
| `set_max_failed_attempts` | `max_attempts` | Umbral de intentos fallidos |

### 4.4 Tipo 4 — Sincronización de hora

```json
"message_type": 4,
"message_info": { "time_string": "2026-09-22 10:15:00" }
```

Actualiza la hora del equipo (en A2v3 también reloj del sistema + RTC
DS3231). Emite el evento TIME_SYNC (3.14).

### 4.5 Tipo 5 — Gestión de códigos

`message_info.action`:

| Acción | Parámetros | Notas |
|--------|------------|-------|
| `add_remote_code` | `code_type`, `code_value`, `keyboard_id` (0=ambos), `relay`, `time_slots[]` opcional | Hasta 40; cada slot: `start_hour`, `start_minute`, `end_hour`, `end_minute`, `days_of_week` (bitmask 1=Lun … 64=Dom); máx. 4 slots |
| `remove_remote_code` | `code_type`, `code_value` | |
| `clear_remote_codes` | — | Borra todos los remotos |
| `add_local_code` | `code_type`, `code_value`, `relay`, `keyboard_id` opcional | Hasta 50 |
| `remove_local_code` | `code_type`, `code_value` | |
| `clear_local_codes` | — | |
| `list_local_codes` | — | Respuesta con JSON: `count`, `max`, `validation_mode`, `codes[]` (máx. 30, `truncated` si hay más) |

### 4.6 Tipo 6 — Gestión BLE (solo variante -BLE)

`message_info.action`:

| Acción | Parámetros | Notas |
|--------|------------|-------|
| `set_superadmin` | `key` (hex 128 chars) | Clave del superadministrador |
| `add_user` | `slot` 1-5, `key` (hex 128), `name`, `permissions` | Permisos: bitmask 1=relés, 2=modo, 4=códigos, 8=red, 255=admin |
| `add_user_derived` | `slot`, `user_id`, `name`, `permissions` | Clave derivada por HKDF; la respuesta incluye `salt` e `info_prefix` para que la app derive la misma clave |
| `clear_user` | `slot` | |
| `clear_superadmin` | — | |
| `clear_all` | — | Borra todas las vinculaciones |
| `get_status` | — | Respuesta: superadmin, usuarios activos, máx. |
| `list_users` | — | Detalle de usuarios (con vista previa de claves, nunca completas) |

### 4.7 Tipo 7 — Gestión de red (WiFi / 4G / RTC)

`message_info.action`:

| Acción | Parámetros | Respuesta |
|--------|------------|-----------|
| `get_wifi` | — | JSON de estado: `ap{active,mode,timeout_s,remaining_s,clients,ssid,ip,pass_default}`, `sta{enabled,ssid,connected,ip,rssi}`, `eth_up`, `eth_ip`, `gsm_up`, `mqtt_iface`, `mqtt_connected` |
| `set_wifi` | `ap_mode` (0=off,1=ventana,2=siempre), `ap_timeout_s`, `ap_pass` (≥8), `sta_ssid`+`sta_pass`, `sta_enabled`, `forget_sta` — todos opcionales | Resumen de lo aplicado |
| `ap_start` | `seconds` (0=ventana configurada) | Activa el AP ya |
| `ap_stop` | — | Apaga el AP |
| `wifi_scan` | — | Lanza escaneo async |
| `get_wifi_scan` | — | `{scanning, networks[{ssid,rssi,enc}]}` |
| `get_gsm` | — | JSON: `state` (probing/absent/sim_check/pin_required/pin_error/puk_required/no_sim/registering/attached), `model`, `imei`, `operator`, `csq` (99=sin señal), `rssi_dbm`, `pin_set`, `pin_attempts` (PIN1,PIN2,PUK1,PUK2), `reg_stat`+`reg_stat_text`, `radio` (CPSI), `data_started`, `data_up`, `data_ip`, `apn_active`, `apn_mode`, `apn`, `last_error` |
| `set_gsm` | `enabled`, `pin` (solo escritura; UN intento por valor), `pin_disable` (desbloquea y quita el PIN de la SIM permanentemente), `apn`+`apn_mode`+`apn_user`+`apn_pass` | Resumen |
| `gsm_rescan` | — | Reinicia el módulo 4G (AT+CRESET) y re-detecta SIM |
| `get_rtc` | — | `{present, osf, seeded}` (solo A2v3) |

## 5. Respuesta de validación remota

La plataforma responde a la solicitud 3.2 publicando en
`swatidhome/command/<id>/granted` (o cualquier subtopic de command). Dos
formatos aceptados:

### Formato recomendado (`response`)

```json
{
  "device": "SWATID_A0461CA0ED30",
  "message_id": 456,               // eco del message_id de la solicitud
  "response": "APPROVED",          // APPROVED | DENIED
  "code_type": "PIN", "code_value": "1234",
  "relay_number": 1,               // relé a abrir (modo normal)
  "duration": 3.0,                 // segundos (opcional)
  "reason": "Usuario autorizado"
}
```

- **Timeout: 5 s.** Sin respuesta, en modo torno se registra TIMEOUT (y el
  equipo aplicó fallback local si el código existía localmente).
- En **modo torno** el relé lo decide el equipo (`actual_relay` de la
  solicitud); `message_id` debe coincidir con el de la solicitud pendiente.

### Formato legado (`access_granted`) — solo compatibilidad

```json
{ "access_granted": true, "code_type": "PIN", "code_value": "1234",
  "relay_number": 1, "duration": 3000, "reason": "…" }
```

Nota: aquí `duration` va en **milisegundos**. No usar en integraciones nuevas.

## 6. Buenas prácticas de integración

1. **Filtrar siempre por `event`/`event_type`** — un mismo topic puede
   transportar varios contratos (p. ej. `digital_input`: trigger y
   door_contact). Ignorar eventos desconocidos sin error.
2. **Tolerar campos nuevos**: las ampliaciones añaden claves, nunca cambian
   las existentes.
3. **Correlación**: usar `message_id` (comando) ↔ `message_id` (respuesta).
4. **Validación remota**: responder en < 5 s; tratar `reason=boot|sync` de
   door_contact como estado absoluto.
5. **Presencia**: keepalive 60 s + LWT no configurado — usar ausencia de
   keepalives.
6. **No asumir orden** entre evento de acceso y relay_enable.
7. El equipo funciona sin broker (validación local); al reconectar
   sincroniza estados de puerta (`reason: sync`) pero **no** reenvía eventos
   de acceso perdidos.

## 7. Compatibilidad y versionado

| Versión firmware | Cambios de mensajería |
|------------------|----------------------|
| v3.x | Base: validación, eventos, relés, `digital_input_trigger`, tipos 0-5 |
| v4.x (-BLE) | Tipo 6 BLE + eventos BLE |
| v5.0.0/v5.0.1 | Tipo 7 red (WiFi/AP/4G/RTC); `get_gsm` ampliado (reg_stat, radio, data_*, pin_attempts) |
| v5.0.2 | `door_contact` (nuevo evento, mismo topic); campos `diN_type`/`open_level` en APIs; `set_gsm.pin_disable` |

Regla general: los contratos existentes no se modifican; toda ampliación es
un `event` nuevo o campos añadidos.

---

*Manual de Integración MQTT v1.0 — Septiembre 2026 — Smart World And Things SLU.*
