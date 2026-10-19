# Manual de Integración App ↔ BLE — Controladora SWATID A2 / A2v3

**Versión:** v2.0 · **Fecha:** Octubre 2026 · **Firmware de referencia:** v5.0.6
**Empresa:** Smart World And Things SLU · info@swat-id.com · www.swat-id.com

Documento completo para el equipo de la **App móvil**: conexión, gestión de la
clave y usuarios, códigos de acceso, relés, modos, duraciones, estado y
configuración de red. Los flujos marcados ✅ están **validados en placa real**
(A2v3 `SWATID_CC641BEABF8C`, 19-Oct-2026, cliente BLE desde macOS).

> **El backend NO se ve afectado**: todo lo descrito viaja por BLE entre la
> App y el equipo. Los contratos MQTT permanecen idénticos; el equipo solo
> publica eventos informativos (`AUTH_*`, `RELAY_*`, `BINDING_*`, `CODE_ADDED`)
> en `swatidhome/events/<serial>/ble` cuando tiene broker.

---

## Índice

1. [Descubrimiento y conexión](#1-descubrimiento-y-conexión)
2. [Protocolo de respuestas (notify + EOT)](#2-protocolo-de-respuestas)
3. [Gestión de la clave y autenticación](#3-gestión-de-la-clave-y-autenticación) ✅
4. [Gestión de usuarios](#4-gestión-de-usuarios)
5. [Relés: prueba y control (FF02)](#5-relés-ff02)
6. [Modo normal / torno (FF03)](#6-modo-ff03)
7. [Códigos de acceso (FF04 alta, FF0A listado)](#7-códigos-de-acceso)
8. [Duración del relé (FF06)](#8-duración-del-relé-ff06)
9. [Estado: FF07, FF08, FF09](#9-estado) ✅
10. [Red completa: FF05 (ETH + WiFi)](#10-red-ff05) ✅
11. [Flujos completos recomendados](#11-flujos-completos)
12. [Errores y resolución de problemas](#12-errores)

---

## 1. Descubrimiento y conexión

| Parámetro | Valor |
|-----------|-------|
| Nombre anunciado | El serial del equipo: `SWATID_XXXXXXXXXXXX` |
| Servicio GATT | `0000FF00-0000-1000-8000-00805F9B34FB` |
| Características | `0000FFxx-0000-1000-8000-00805F9B34FB` (FF01–FF0B) |
| Targets | A2 clásico (build -BLE) y **A2v3 (build -S3-BLE, desde v5.0.6)** |
| MTU | Negociar al conectar; las respuestas largas llegan fragmentadas |

Al conectar, el equipo solicita parámetros de conexión tolerantes
(intervalo 30–60 ms, supervision timeout 6 s) para que el enlace sobreviva
a la actividad WiFi — la App debe **aceptar la actualización de parámetros**
(los stacks iOS/Android lo hacen solos).

### Tabla de características

| UUID | Función | Acceso | Requiere |
|------|---------|--------|----------|
| FF01 | Autenticación / registro TOFU | WRITE + notify | — |
| FF02 | Control de relés | WRITE + notify | auth + perm `0x01` |
| FF03 | Modo normal/torno | READ/WRITE + notify | auth + perm `0x02` |
| FF04 | Alta de código PIN/TAG | WRITE + notify | auth + perm `0x04` |
| FF05 | Red ETH + WiFi (v5.0.6) | READ/WRITE + notify | auth + perm `0x08` |
| FF06 | Duración del relé | READ/WRITE + notify | auth + perm `0x01` |
| FF07 | Estado resumido | READ | auth |
| FF08 | DevInfo | READ | pública |
| FF09 | Info completa | READ | auth |
| FF0A | Listado de códigos (paginado) | READ/WRITE + notify | auth |
| FF0B | Challenge (nonce 16 B) | READ | pública |

---

## 2. Protocolo de respuestas

Dos estilos, según la característica:

- **Texto plano por notify** (FF01, FF02, FF03, FF04, FF06): respuestas
  cortas `OK:...` / `ERROR:...` en un solo notify, **sin EOT**. La App debe
  suscribirse (start_notify) a la característica **antes** de escribir.
- **JSON con EOT** (FF05, FF0A; lecturas de FF08/FF09 según tamaño): la
  respuesta puede llegar en varios fragmentos de notify; el final se marca
  con el byte **EOT = 0x04**. Concatenar fragmentos hasta el EOT y parsear.

---

## 3. Gestión de la clave y autenticación ✅

El equipo **no tiene clave precargada**: el modelo es **TOFU** — la App
genera la clave y el equipo la adopta en el primer uso. Si la App "pide una
clave que no tiene", está ante un equipo que **ya tiene dueño** (ver 3.4).

### 3.1 Alta del primer administrador (equipo virgen) ✅

```text
1. La App GENERA 64 bytes aleatorios (CSPRNG) y los guarda en
   Keychain/Keystore asociados al serial del equipo.
2. start_notify(FF01)
3. WRITE FF01 ← los 64 bytes en claro (SOLO esta primera vez)
4. Respuesta: "OK:SUPERADMIN_REGISTERED:TOKEN:<16 hex>"
```

Validado en placa: la clave queda registrada como **admin de placa** también
en el almacén de vinculación unificado (v5.0.6): la misma clave firma después
en el canal MQTT `/vinculo` y sobrevive a reinicios.

### 3.2 Autenticación de sesión (cada conexión) ✅

```text
1. READ FF0B → nonce de 16 bytes
2. response = SHA256( clave(64 B) ‖ nonce(16 B) )   // 32 bytes
3. WRITE FF01 ← los 32 bytes
4. Respuesta: "OK:TOKEN:<16 hex>"  (token de sesión de 8 bytes)
```

- El equipo prueba el response contra: **clave de plataforma** (soporte
  SWATID) → **admin de placa** → **usuarios 1–5**.
- La sesión caduca a los **5 min de inactividad** o al desconectar.
- Si un equipo con dueño recibe 64 B que no coinciden → error (la App NO
  debe reintentar en bucle: mostrar "equipo ya vinculado").

### 3.3 Permisos (bitmask por usuario)

`0x01` relés · `0x02` modo · `0x04` códigos · `0x08` red · `0xFF` admin.
(Corrección histórica: el permiso de RED siempre fue **0x08**; 0x02 es modo.)

### 3.4 Equipo con dueño: cómo entra un segundo móvil

1. **Como usuario** (recomendado): el admin lo da de alta (sección 4).
2. **Recuperación**: reset de vínculos desde el portal web del equipo, por
   MQTT `/vinculo` (unlink firmado) o por la clave de plataforma (soporte).
   Tras ello, TOFU de nuevo.

---

## 4. Gestión de usuarios

Modelo unificado v5.0.5/v5.0.6 — tres roles con el **mismo material de clave
en BLE y MQTT** (sincronización bidireccional en firmware):

| Rol | Alta | Autentica por BLE |
|-----|------|-------------------|
| Superadmin de plataforma | Fábrica (web `/binding` del equipo) | ✅ (sesión `PLATFORM`, permisos plenos) |
| Admin de placa (slot 0) | TOFU BLE (3.1) **o** TOFU MQTT `/vinculo` | ✅ (`SUPERADMIN`) |
| Usuarios (slots 1–5) | Firmados por admin/plataforma | ✅ (su nombre, sus permisos) |

**Alta de usuarios desde la App**: cada móvil genera SU clave de 64 B y el
administrador la registra. Vías disponibles:

- **MQTT `/vinculo`** (recomendada, misma geometría que X1): comando `link`
  firmado por el admin con `slot`, `new_key`, `name`, `permissions` — ver
  [MANUAL-COMANDOS-MQTT.md §10](../v5.0.5_users/MANUAL-COMANDOS-MQTT.md).
- **MQTT tipo 6** (legado, builds -BLE): `add_user` / `add_user_derived` /
  `clear_user` / `list_users`.

Cualquier alta/baja hecha por una vía se refleja en la otra (y en el
challenge-response BLE) automáticamente.

**Baja / rotación**: `unlink` y `update_vinculo` firmados (MQTT). La baja del
admin limpia también los usuarios; la clave de plataforma nunca se ve
afectada.

---

## 5. Relés (FF02)

Prueba de relé / apertura manual desde la App. **WRITE binario**:

| Byte | Contenido |
|------|-----------|
| 0 | `relay_id`: 1 o 2 |
| 1 | `action`: 0=OFF inmediato · 1=ON · 2=PULSE (1 y 2 equivalen: activan con auto-apagado) |
| 2–3 | (opc.) duración en **ms, big-endian**; 0/ausente = duración configurada |

Respuestas: `OK:RELAY_ON` · `OK:RELAY_PULSE` · `OK:RELAY_OFF` ·
`ERROR:INVALID_RELAY` · `ERROR:NOT_AUTHORIZED`.

Ejemplos: `01 01` = relé 1 con duración configurada · `02 01 0B B8` = relé 2
durante 3000 ms · `01 00` = relé 1 OFF ya.

El equipo publica además el evento MQTT `RELAY_ACTIVATED`/`RELAY_DEACTIVATED`
(fuente BLE, con el usuario) si tiene broker.

## 6. Modo (FF03)

- **READ** → 1 byte: `0` normal · `1` torno.
- **WRITE** → 1 byte: `0`/`1`. Respuesta `OK:MODE_NORMAL` / `OK:MODE_TURNSTILE`.

En modo torno cada teclado abre su relé asignado (configuración de relés por
teclado: portal web o MQTT; no expuesta aún por BLE — fase 2 del plan).

## 7. Códigos de acceso

### 7.1 Alta (FF04) — WRITE binario

| Byte | Contenido |
|------|-----------|
| 0 | tipo: 0=PIN · 1=TAG |
| 1 | teclado: 0=ambos · 1 · 2 |
| 2 | relé: 1 o 2 |
| 3… | el código en ASCII (1–16 caracteres; PIN: 4–6 dígitos recomendado) |

Ejemplo: PIN `1234`, ambos teclados, relé 1 → `00 00 01 31 32 33 34`.

Respuestas: `OK:CODE_ADDED:<total>` · `ERROR:CODE_EXISTS` ·
`ERROR:STORAGE_FULL` (máx. 50) · `ERROR:INVALID_TYPE|KEYBOARD|RELAY|CODE_LENGTH`.

La respuesta llega al instante; la persistencia en EEPROM se completa en el
siguiente ciclo (≤1 s). El equipo publica `CODE_ADDED` por MQTT si hay broker.

### 7.2 Listado (FF0A) — paginado JSON

**WRITE** `{"page":N}` → notify con EOT:

```json
{ "n": 7, "m": 50, "p": 0, "ps": 3, "tp": 3, "more": true,
  "c": [ { "i": 0, "t": "PIN", "v": "1234", "k": 0, "r": 1 }, … ] }
```

`n` total · `m` máximo · `p` página · `ps` tamaño página (3) · `tp` páginas ·
`more` hay más · `c[]` códigos (índice, tipo, valor, teclado, relé).
Iterar `page` 0..tp-1. Errores: `{"error":"INVALID_PAGE|NOT_AUTHORIZED|…"}`.

### 7.3 Borrado y códigos remotos con franjas

No expuestos por BLE todavía (fase 2 del plan v5.0.6). Disponibles por el
portal web y por MQTT tipo 5 (`remove_local_code`, `add_remote_code` con
`time_slots`, `list_remote_codes` — ver manual MQTT v1.2).

## 8. Duración del relé (FF06)

- **READ** → `uint32` little-endian: duración en ms.
- **WRITE** → `uint32` little-endian en ms (p. ej. 3000 = `B8 0B 00 00`).
  Respuesta `OK:RELAY_TIME_SET`.

## 9. Estado ✅

### FF08 — DevInfo (pública, antes de autenticar)

```json
{ "device_type": "SWATID-A2", "serial": "SWATID_CC641BEABF8C",
  "firmware_version": "v5.0.6", "firmware_variant": "BLE",
  "protocol_version": 1, "capabilities": { … },
  "superadmin_registered": true, "active_users": 1 }
```

Usarla para identificar el equipo y saber si está virgen (TOFU posible).

### FF07 — Estado resumido (autenticada)

```json
{ "relay1": false, "relay2": false, "mode": "normal", "auth": true,
  "user": "SUPERADMIN", "eth_connected": true, "mqtt_connected": false }
```

### FF09 — Info completa (autenticada, nombres cortos)

`type, sn, name, fw` + `net{ip,gw,mask,dhcp,eth}` +
`st{r1,r2,dur,mode,mqtt}` + resumen de códigos.

## 10. Red (FF05) ✅

Contrato completo en [CONTRATO-FF05-RED.md](CONTRATO-FF05-RED.md). Resumen:

- **READ** → estado unificado: campos ETH legados + `sta{connected,ssid,ip,rssi}`,
  `ap{active,clients,remaining_s}`, `ble_net_lock`. Sin contraseñas.
- **WRITE JSON** `{"cmd":…}`: `status` · `wifi_scan_start` (máx. 1/10 s) ·
  `wifi_scan_get` (poll ~2 s) · `wifi_sta_set {ssid,pass}` ·
  `wifi_sta_forget` · `wifi_ap_set {mode,timeout_s,pass}` · `wifi_ap_stop` ·
  `eth_set {dhcp,ip,gw,mask,dns}` (**reinicia en 2 s**) · `exit_config`.
- **WRITE binario 17 B**: legado Ethernet (compat Apps v4.1).

**Lock AP↔BLE**: al iniciar un flujo WiFi el equipo apaga su AP y bloquea el
portal de red (HTTP 409); al desconectar/`exit_config` restaura su política.
Validado en placa: **escaneo de 13 redes con el enlace BLE estable**.

## 11. Flujos completos

### Instalación desde cero ✅ (validado extremo a extremo)

```text
1. Scan BLE → conectar a SWATID_<serial>
2. READ FF08 → virgen (superadmin_registered: false)
3. TOFU: WRITE FF01 ← clave generada (64 B) → OK:SUPERADMIN_REGISTERED
4. WRITE FF05 {"cmd":"wifi_scan_start"} → poll {"cmd":"wifi_scan_get"}
5. Usuario elige red → {"cmd":"wifi_sta_set","ssid":"…","pass":"…"}
6. Poll {"cmd":"status"} hasta sta.connected → {"cmd":"exit_config"}
7. (Opc.) FF06 duración, FF04 primeros códigos, FF02 prueba de relé
```

### Operación diaria

```text
Conectar → FF0B+FF01 (challenge-response) → token
  · Abrir puerta: FF02 [relé][1]
  · Alta de código: FF04
  · Revisar: FF07 / FF0A por páginas
Desconectar (la sesión se invalida sola)
```

### Mantenimiento

Cambio de red (FF05), modo torno (FF03), duración (FF06), alta de usuarios
(sección 4), estado completo (FF09).

## 12. Errores

| Respuesta | Causa / acción |
|-----------|----------------|
| `ERROR:NOT_AUTHENTICATED` / `NOT_AUTHORIZED` | Sin sesión o sin permiso: re-autenticar (FF0B+FF01) |
| `ERROR:INVALID_RESPONSE` | Clave incorrecta en challenge-response (equipo con otro dueño) |
| `ERROR:CODE_EXISTS` / `STORAGE_FULL` | Duplicado / 50 códigos |
| `{"ok":false,"error":"scan_rate_limited"}` | Esperar 10 s entre escaneos |
| `{"error":"INVALID_PAGE"}` | Página FF0A fuera de rango |
| Desconexión durante escaneo WiFi | Mitigada en v5.0.6 (conn params + scan 120 ms/canal); si ocurre: reconectar + re-auth y pedir `wifi_scan_get` (el scan siguió en el equipo) |
| El equipo no aparece en el escaneo | ¿Otro móvil ya conectado? (1 conexión simultánea); ¿alimentación? |

### Checklist de la App

1. Clave de 64 B por equipo en almacenamiento seguro; TOFU solo en virgen.
2. start_notify antes de cada WRITE con respuesta.
3. Concatenar hasta EOT (0x04) en FF05/FF0A.
4. Tras `wifi_sta_set`, poll de `status` (2–10 s).
5. Cerrar con `exit_config` o desconectar.
6. Un móvil = una clave (slots 1–5 para el resto).
7. Manejar la reconexión tras `eth_set` (el equipo se reinicia).

---

*Manual de Integración App-BLE v2.0 — Octubre 2026 — Smart World And Things SLU.*
