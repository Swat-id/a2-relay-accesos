# Referencia — Canal de vinculación X1 (portable a A2)

Resumen del contrato **SWATID Relay X1** v1.2.0-rc1 (§5 del documento de integración MQTT), como base de uniformidad para `v5.0.5_users`.  
Validado en placa `SWATID_A085E3CCAC98` (01/10/2026) en el producto X1.

---

## Topics

| Dirección | Topic |
|-----------|--------|
| → equipo | `swatidhome/command/<serial>/vinculo` |
| ← equipo | `swatidhome/<serial>/vinculo` |
| Eventos | `swatidhome/events/<serial>/ble` |

En X1 el admin de placa se llama `role: "superadmin"` (slot 0). En A2 v5.0.5 se mapeará a **admin de placa** y se añadirá **superadmin plataforma** (ver plan).

---

## Modelo X1 (1 + 5)

- Slot **0**: administrador (TOFU).
- Slots **1–5**: usuarios.
- Claves: 64 B → 128 hex; nunca viajan salvo en el alta.
- Autorización posterior: HMAC sobre nonce del equipo.

---

## Públicos

### `devinfo`

```json
{"command":"devinfo","type":"GET"}
```

Respuesta (campos relevantes): `ok`, `serial`, `firmware_version`, `capabilities{relays,slots,ble,mqtt}`, `superadmin_registered`, `available_for_linking`, `slots_used`, `slots_available`, `remote_link_window_open`, `remote_link_window_s`.

### `challenge`

```json
{"command":"challenge","type":"GET"}
```

→ `nonce` (64 hex), `expires_s: 60`. Un nonce vivo; pedir otro invalida el anterior; se consume al usarlo.

---

## TOFU administrador

```json
{"command":"link","type":"POST","key":"<128 hex>"}
```

Solo si no hay admin y la **ventana remota** está abierta (≤10 min desde boot o abierta desde web).  
Éxito: `linked`, `slot: 0`, `role: "superadmin"`.  
Errores: `device_already_owned`, `remote_linking_window_closed`, …

---

## Sobre autenticado

```json
{"nonce":"<64 hex>","hmac":"<64 hex>","payload":"<comando JSON como string>"}
```

```text
hmac = HMAC-SHA256(key_slot, unhex(nonce) || utf8(payload))
```

El equipo prueba todos los slots; el que verifique es el firmante.

| Acción | Quién | Payload (idea) |
|--------|-------|----------------|
| Alta usuario | Admin | `link` POST + `slot` + `new_key` + `name` + `permissions` |
| Rotar clave propia | Cualquiera | `update_vinculo` + `new_key` |
| Baja tercero | Admin | `unlink` + `slot` |
| Baja propia | Cualquiera | `unlink` sin slot |
| Baja admin | Admin slot 0 | `unlink` slot 0 → limpia usuarios |

Permisos: `0x01` relé, `0x02` modo, `0x04` códigos, `0x08` red, `0xFF` admin.


## Errores frecuentes

`missing_key`, `invalid_key`, `auth_required`, `invalid_signature`, `nonce_expired`, `device_already_owned`, `remote_linking_window_closed`, `invalid_new_key_or_slot`, `superadmin_required`, `not_linked`, `unknown_command`, `nvs_write_failed`.

---

## Relación con message_type 6

En X1 el tipo 6 sigue vivo (credenciales del broker) pero la **gestión nueva** es `/vinculo`. Misma política propuesta para A2.

---

## Ejemplo de firma (Python, idéntico espíritu X1 §8)

```python
import hashlib, hmac, json
payload = json.dumps(
    {"command": "link", "type": "POST", "slot": 2,
     "new_key": NEW_KEY, "name": "Juan", "permissions": 3},
    separators=(",", ":"),
)
mac = hmac.new(
    bytes.fromhex(KEY_ADMIN),
    bytes.fromhex(nonce) + payload.encode(),
    hashlib.sha256,
).hexdigest()
envelope = {"nonce": nonce, "hmac": mac, "payload": payload}
```

---

*Referencia condensada para portación — el contrato completo X1 permanece en el repo del producto X1.*
