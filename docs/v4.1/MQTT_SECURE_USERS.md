# Gestión Segura de Usuarios BLE via MQTT v4.1

## Resumen

La versión 4.1 introduce el comando `add_user_derived` que permite añadir usuarios BLE sin transmitir sus claves por MQTT, usando derivación de claves HKDF.

---

## El Problema (v4.0)

En v4.0, para añadir un usuario BLE se enviaba la clave en texto claro:

```json
{
    "message_type": 4,
    "message_info": {
        "action": "add_user",
        "slot": 1,
        "key": "A1B2C3D4...128 caracteres hex..."  // ¡EXPUESTO!
    }
}
```

**Riesgos:**
- Clave visible en logs del broker MQTT
- Interceptable si MQTT no usa TLS
- Almacenable en históricos del servidor

---

## La Solución (v4.1): HKDF

### Clave Maestra del Dispositivo

Cada dispositivo tiene una **clave maestra de 32 bytes** generada en el primer boot:

```
device_master_key = random(32 bytes)
// Almacenada en EEPROM offset 3200
```

### Derivación de Clave con HKDF

```
user_key = HKDF-SHA256(
    IKM  = device_master_key,
    salt = serial_number,
    info = "user_key:" + user_id,
    len  = 64 bytes
)
```

### Comando MQTT Seguro

```json
{
    "message_type": 4,
    "message_id": "abc123",
    "message_info": {
        "action": "add_user_derived",
        "slot": 1,
        "user_id": "user_unique_123",
        "name": "Juan García",
        "permissions": 15
    }
}
```

**Nota:** La clave NO viaja en el mensaje. Tanto el servidor como el dispositivo la calculan.

---

## Flujo Completo

### 1. Registro Inicial del Dispositivo

```
Servidor                              Dispositivo
   │                                       │
   │  El servidor conoce:                  │
   │  - serial_number (público)            │
   │  - device_master_key (compartido      │
   │    durante registro inicial)          │
   │                                       │
```

### 2. Añadir Usuario

```
Servidor                              Broker                              Dispositivo
   │                                       │                                    │
   │  Calcula:                             │                                    │
   │  user_key = HKDF(                     │                                    │
   │    master_key,                        │                                    │
   │    serial,                            │                                    │
   │    "user_key:user123"                 │                                    │
   │  )                                    │                                    │
   │                                       │                                    │
   │  Publica (SIN la clave):              │                                    │
   │  { "action": "add_user_derived",      │                                    │
   │    "slot": 1,                         │──────────────────────────────────>│
   │    "user_id": "user123" }             │                                    │
   │                                       │                                    │
   │                                       │  Dispositivo calcula la MISMA      │
   │                                       │  clave usando los mismos          │
   │                                       │  parámetros:                       │
   │                                       │  user_key = HKDF(...)              │
   │                                       │                                    │
   │                                       │  Guarda en EEPROM                  │
   │                                       │                                    │
```

### 3. Usuario se Autentica

```
APP                                  Dispositivo
   │                                       │
   │  APP tiene la misma clave:            │
   │  (derivada por el servidor)           │
   │                                       │
   │  Usa Challenge-Response (v4.1)        │
   │  o clave directa (v4.0 legado)        │
   │──────────────────────────────────────>│
   │                                       │
```

---

## Respuesta MQTT

### Éxito

```json
{
    "status": 0,
    "message_id": "abc123",
    "response": "user added (derived): {
        \"slot\": 1,
        \"user_id\": \"user123\",
        \"name\": \"Juan García\",
        \"permissions\": 15,
        \"enabled\": true,
        \"method\": \"hkdf_derived\",
        \"salt\": \"SWATID_D8F7B4BF1388\",
        \"info_prefix\": \"user_key:\"
    }"
}
```

### Evento Publicado

```json
{
    "event": "USER_ADDED_DERIVED",
    "source": "MQTT",
    "device": "SWATID_D8F7B4BF1388",
    "slot": 1,
    "user_id": "user123",
    "name": "Juan García",
    "permissions": 15,
    "method": "hkdf",
    "timestamp": "2026-02-05T10:30:00Z"
}
```

---

## Implementación en el Servidor

### Ejemplo en Python

```python
import hashlib
import hmac

def hkdf_extract(salt: bytes, ikm: bytes) -> bytes:
    """HKDF-Extract: PRK = HMAC(salt, IKM)"""
    return hmac.new(salt, ikm, hashlib.sha256).digest()

def hkdf_expand(prk: bytes, info: bytes, length: int) -> bytes:
    """HKDF-Expand: OKM = T(1) || T(2) || ..."""
    t = b""
    okm = b""
    counter = 1
    
    while len(okm) < length:
        t = hmac.new(prk, t + info + bytes([counter]), hashlib.sha256).digest()
        okm += t
        counter += 1
    
    return okm[:length]

def derive_user_key(device_master_key: bytes, serial: str, user_id: str) -> bytes:
    """Deriva la clave de usuario usando HKDF-SHA256"""
    salt = serial.encode('utf-8')
    info = f"user_key:{user_id}".encode('utf-8')
    
    prk = hkdf_extract(salt, device_master_key)
    user_key = hkdf_expand(prk, info, 64)  # 64 bytes = BLE_KEY_SIZE
    
    return user_key

# Ejemplo de uso
device_key = bytes.fromhex("a1b2c3d4...")  # 32 bytes
serial = "SWATID_D8F7B4BF1388"
user_id = "user123"

user_key = derive_user_key(device_key, serial, user_id)
print(f"User key (hex): {user_key.hex()}")
```

### Ejemplo en TypeScript

```typescript
async function deriveUserKey(
  deviceMasterKey: Uint8Array,
  serial: string,
  userId: string
): Promise<Uint8Array> {
  const encoder = new TextEncoder();
  const salt = encoder.encode(serial);
  const info = encoder.encode(`user_key:${userId}`);

  // Import master key
  const keyMaterial = await crypto.subtle.importKey(
    'raw',
    deviceMasterKey,
    { name: 'HKDF' },
    false,
    ['deriveBits']
  );

  // Derive user key
  const derivedBits = await crypto.subtle.deriveBits(
    {
      name: 'HKDF',
      hash: 'SHA-256',
      salt: salt,
      info: info,
    },
    keyMaterial,
    64 * 8  // 64 bytes en bits
  );

  return new Uint8Array(derivedBits);
}
```

---

## Comparativa de Métodos

| Aspecto | add_user (v4.0) | add_user_derived (v4.1) |
|---------|-----------------|-------------------------|
| Clave en mensaje | Sí (128 hex chars) | No |
| Seguridad MQTT | Baja | Alta |
| Requisito servidor | Conocer clave | Conocer device_master_key |
| Complejidad | Baja | Media |
| Recomendado | No | Sí |

---

## Permisos de Usuario

| Permiso | Valor | Descripción |
|---------|-------|-------------|
| RELAY_CONTROL | 0x01 | Control de relés |
| MODE_CHANGE | 0x02 | Cambio de modo |
| ADD_CODES | 0x04 | Añadir códigos |
| NETWORK_CONFIG | 0x08 | Configuración de red |
| ADMIN | 0xFF | Todos los permisos |

Ejemplo: `permissions: 15` = 0x0F = RELAY + MODE + CODES + NETWORK
