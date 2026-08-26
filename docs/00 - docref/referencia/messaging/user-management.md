# Gestión de Usuarios BLE - MQTT y BLE Protocol

> **Versión:** 4.0.0  
> **Última actualización:** 2026-02-05  
> **Dispositivo:** SWATID-A2

Este documento describe todos los comandos disponibles para la gestión de usuarios BLE tanto por MQTT como por BLE directo.

---

## Índice

1. [Modelo de Seguridad](#modelo-de-seguridad)
2. [Estructura de Datos](#estructura-de-datos)
3. [Comandos MQTT](#comandos-mqtt)
4. [Comandos BLE](#comandos-ble)
5. [Eventos Publicados](#eventos-publicados)
6. [Ejemplos de Integración](#ejemplos-de-integración)

---

## Modelo de Seguridad

### Jerarquía de Usuarios

```
┌─────────────────────────────────────────────────────────────┐
│                      SUPERADMIN                              │
│  • Primer usuario en vincular (automático)                  │
│  • Permisos completos (0xFF)                                │
│  • Único que puede gestionar otros usuarios                 │
│  • Solo puede haber 1 superadmin                            │
└─────────────────────────────────────────────────────────────┘
                          │
                          │ Gestiona
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    USUARIOS (slots 1-5)                      │
│  • Máximo 5 usuarios adicionales                            │
│  • Creados por superadmin vía BLE o servidor vía MQTT       │
│  • Permisos configurables por usuario                       │
│  • NO pueden auto-registrarse si ya existe superadmin       │
└─────────────────────────────────────────────────────────────┘
```

### Flujo de Vinculación

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   DISPOSITIVO   │     │   PRIMER USER   │     │   SIGUIENTES    │
│   SIN VINCULAR  │────▶│  = SUPERADMIN   │────▶│   USERS 1-5     │
└─────────────────┘     └─────────────────┘     └─────────────────┘
        │                       │                       │
        │                       │                       │
  Auto-registro            Registra            Solo autenticación
  (primer user)            usuarios            (clave previa)
```

### Reglas de Seguridad

1. **Auto-registro**: Solo permitido cuando NO hay superadmin
2. **Usuarios adicionales**: Solo se añaden vía:
   - Superadmin autenticado (BLE)
   - Servidor (MQTT)
3. **Autenticación**: Usuarios con clave previa pueden autenticarse, nunca auto-registrarse

---

## Estructura de Datos

### Configuración BLE en EEPROM

| Campo | Tamaño | Descripción |
|-------|--------|-------------|
| `validMarker` | 4 bytes | Marcador: `0xBLE4C0DE` |
| `superadmin_key` | 64 bytes | Clave del superadmin |
| `user_keys[5]` | 320 bytes | Claves de usuarios (5 × 64 bytes) |
| `user_enabled[5]` | 5 bytes | Estado de cada usuario (0/1) |
| `user_permissions[5]` | 5 bytes | Permisos por usuario |
| `user_names[5]` | 80 bytes | Nombres (5 × 16 chars) |
| `superadmin_registered` | 1 byte | Flag superadmin (0/1) |
| `checksum` | 4 bytes | Verificación de integridad |

### Permisos de Usuario

| Permiso | Valor | Descripción |
|---------|-------|-------------|
| `BLE_PERM_RELAY_CONTROL` | `0x01` | Control de relés |
| `BLE_PERM_MODE_CHANGE` | `0x02` | Cambio de modo operación |
| `BLE_PERM_ADD_CODES` | `0x04` | Añadir códigos locales |
| `BLE_PERM_NETWORK_CONFIG` | `0x08` | Configuración de red |
| `BLE_PERM_ADMIN` | `0xFF` | Todos los permisos (superadmin) |

### Permisos por Defecto

- **Superadmin**: `0xFF` (todos)
- **Usuario nuevo**: `0x03` (relés + modo)

---

## Comandos MQTT

### Topic de Comandos

```
swatidhome/cmd/{serial_number}
```

### Estructura Base del Mensaje

```json
{
  "message_type": 6,
  "message_id": <número>,
  "message_info": {
    "action": "<acción>",
    ...parámetros específicos
  }
}
```

---

### 1. Añadir Usuario (`add_user`)

Añade un nuevo usuario BLE en un slot específico.

**Request:**
```json
{
  "message_type": 6,
  "message_id": 1001,
  "message_info": {
    "action": "add_user",
    "slot": 1,
    "key": "A1B2C3D4E5F6...128 caracteres hex (64 bytes)",
    "name": "Juan García",
    "permissions": 3
  }
}
```

**Parámetros:**

| Campo | Tipo | Requerido | Descripción |
|-------|------|-----------|-------------|
| `slot` | int | ✓ | Slot del usuario (1-5) |
| `key` | string | ✓ | Clave de 64 bytes en hexadecimal (128 caracteres) |
| `name` | string | ✗ | Nombre del usuario (máx. 15 chars). Default: "UsuarioN" |
| `permissions` | int | ✗ | Permisos (bitmask). Default: 3 (relés + modo) |

**Response OK:**
```json
{
  "message_type": 0,
  "message_id": 1001,
  "error_code": 0,
  "message_info": "user added: {\"slot\":1,\"name\":\"Juan García\",\"permissions\":3,\"enabled\":true}"
}
```

**Response Error:**
```json
{
  "message_type": 0,
  "message_id": 1001,
  "error_code": 1,
  "message_info": "invalid slot (1-5) or key length (must be 128 hex chars)"
}
```

---

### 2. Establecer Superadmin (`set_superadmin`)

Establece o sobrescribe la clave del superadmin.

> ⚠️ **CUIDADO**: Sobrescribe el superadmin existente sin confirmación.

**Request:**
```json
{
  "message_type": 6,
  "message_id": 1002,
  "message_info": {
    "action": "set_superadmin",
    "key": "A1B2C3D4E5F6...128 caracteres hex (64 bytes)"
  }
}
```

**Parámetros:**

| Campo | Tipo | Requerido | Descripción |
|-------|------|-----------|-------------|
| `key` | string | ✓ | Clave de 64 bytes en hexadecimal (128 caracteres) |

**Response OK:**
```json
{
  "message_type": 0,
  "message_id": 1002,
  "error_code": 0,
  "message_info": "superadmin key set from server"
}
```

---

### 3. Eliminar Usuario (`clear_user`)

Elimina un usuario específico de un slot.

**Request:**
```json
{
  "message_type": 6,
  "message_id": 1003,
  "message_info": {
    "action": "clear_user",
    "slot": 2
  }
}
```

**Parámetros:**

| Campo | Tipo | Requerido | Descripción |
|-------|------|-----------|-------------|
| `slot` | int | ✓ | Slot del usuario a eliminar (1-5) |

**Response OK:**
```json
{
  "message_type": 0,
  "message_id": 1003,
  "error_code": 0,
  "message_info": "BLE user slot 2 cleared"
}
```

---

### 4. Eliminar Superadmin (`clear_superadmin`)

Elimina el superadmin, permitiendo nuevo auto-registro.

**Request:**
```json
{
  "message_type": 6,
  "message_id": 1004,
  "message_info": {
    "action": "clear_superadmin"
  }
}
```

**Response OK:**
```json
{
  "message_type": 0,
  "message_id": 1004,
  "error_code": 0,
  "message_info": "superadmin BLE binding cleared"
}
```

---

### 5. Eliminar Todas las Vinculaciones (`clear_all`)

Elimina superadmin y todos los usuarios.

**Request:**
```json
{
  "message_type": 6,
  "message_id": 1005,
  "message_info": {
    "action": "clear_all"
  }
}
```

**Response OK:**
```json
{
  "message_type": 0,
  "message_id": 1005,
  "error_code": 0,
  "message_info": "all BLE bindings cleared"
}
```

---

### 6. Obtener Estado (`get_status`)

Obtiene información general del estado BLE.

**Request:**
```json
{
  "message_type": 6,
  "message_id": 1006,
  "message_info": {
    "action": "get_status"
  }
}
```

**Response:**
```json
{
  "message_type": 0,
  "message_id": 1006,
  "error_code": 0,
  "message_info": "{\"name\":\"SWAT-A2-00112233\",\"connected\":false,\"authenticated\":false,\"connected_user\":\"\",\"superadmin_registered\":true,\"users\":[{\"slot\":1,\"name\":\"Juan\",\"permissions\":3,\"enabled\":true}],\"active_users\":1,\"max_users\":5}"
}
```

---

### 7. Listar Usuarios Detallado (`list_users`)

Obtiene información detallada de todos los usuarios.

**Request:**
```json
{
  "message_type": 6,
  "message_id": 1007,
  "message_info": {
    "action": "list_users"
  }
}
```

**Response:**
```json
{
  "message_type": 0,
  "message_id": 1007,
  "error_code": 0,
  "message_info": "{\"superadmin_registered\":true,\"superadmin_key_preview\":\"A1B2C3D4E5F60708...\",\"users\":[{\"slot\":1,\"enabled\":true,\"name\":\"Juan\",\"permissions\":3,\"key_preview\":\"11223344556677889...\"},{\"slot\":2,\"enabled\":false,\"name\":\"\",\"permissions\":0},{\"slot\":3,\"enabled\":false,\"name\":\"\",\"permissions\":0},{\"slot\":4,\"enabled\":false,\"name\":\"\",\"permissions\":0},{\"slot\":5,\"enabled\":false,\"name\":\"\",\"permissions\":0}]}"
}
```

> **Nota de seguridad**: `key_preview` muestra solo los primeros 8 bytes de la clave para verificación, nunca la clave completa.

---

## Comandos BLE

### Característica de Autenticación (FF02)

**UUID:** `0000FF02-0000-1000-8000-00805f9b34fb`  
**Propiedades:** Write, Notify

---

### 1. Autenticación

Envía la clave de 64 bytes para autenticar.

**Formato:** 64 bytes raw (la clave)

**Responses:**
- `OK:SUPERADMIN_REGISTERED` - Nuevo superadmin registrado
- `OK:SUPERADMIN` - Autenticado como superadmin
- `OK:USER:<nombre>` - Autenticado como usuario
- `ERROR:INVALID_KEY` - Clave no válida
- `ERROR:NOT_ALLOWED` - Auto-registro no permitido

---

### 2. Añadir Usuario (solo Superadmin)

**Formato:** String ASCII

```
ADD_USER:<slot>:<key_hex_128chars>:<permissions>:<nombre>
```

**Ejemplo:**
```
ADD_USER:1:A1B2C3D4...128chars...:3:Juan
```

**Parámetros:**

| Campo | Descripción |
|-------|-------------|
| `slot` | Número 1-5 |
| `key_hex` | Clave de 64 bytes en hexadecimal (128 caracteres) |
| `permissions` | Permisos (bitmask). Opcional, default: 3 |
| `nombre` | Nombre del usuario. Opcional, default: "UsuarioN" |

**Responses:**
- `OK:USER_ADDED:<slot>:<nombre>` - Usuario añadido
- `ERROR:INVALID_PARAMS` - Parámetros inválidos
- `ERROR:NOT_AUTHORIZED` - No es superadmin

---

### 3. Eliminar Usuario (solo Superadmin)

**Formato:** String ASCII

```
DEL_USER:<slot>
```

**Ejemplo:**
```
DEL_USER:2
```

**Responses:**
- `OK:USER_DELETED:<slot>` - Usuario eliminado
- `ERROR:INVALID_SLOT` - Slot inválido

---

### 4. Listar Usuarios (solo Superadmin)

**Formato:** String ASCII

```
LIST_USERS
```

**Response:**
```
USERS:1:Juan,3:Pedro,
```

(Lista de usuarios habilitados en formato `slot:nombre,`)

---

## Eventos Publicados

### Topic de Eventos

```
swatidhome/events/{serial_number}/ble
```

---

### Evento: Usuario Añadido

```json
{
  "event": "USER_ADDED",
  "source": "MQTT",
  "device": "SWAT-A2-00112233",
  "slot": 1,
  "name": "Juan García",
  "permissions": 3,
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

### Evento: Superadmin Establecido

```json
{
  "event": "SUPERADMIN_SET",
  "source": "MQTT",
  "device": "SWAT-A2-00112233",
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

### Evento: Usuario Eliminado

```json
{
  "event": "USER_CLEARED",
  "source": "MQTT",
  "device": "SWAT-A2-00112233",
  "slot": 2,
  "previous_name": "Pedro",
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

### Evento: Superadmin Eliminado

```json
{
  "event": "SUPERADMIN_CLEARED",
  "source": "MQTT",
  "device": "SWAT-A2-00112233",
  "message_id": 1004,
  "details": {
    "action": "superadmin_removed",
    "awaiting_new_superadmin": true
  },
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

### Evento: Todas las Vinculaciones Eliminadas

```json
{
  "event": "ALL_BINDINGS_CLEARED",
  "source": "MQTT",
  "device": "SWAT-A2-00112233",
  "message_id": 1005,
  "details": {
    "superadmin_cleared": true,
    "users_cleared": 5
  },
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

### Evento: Autenticación BLE

```json
{
  "event": "BLE_AUTH",
  "source": "BLE",
  "device": "SWAT-A2-00112233",
  "user": "Juan",
  "user_type": "USER",
  "permissions": 3,
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

### Evento: Desconexión BLE

```json
{
  "event": "BLE_DISCONNECT",
  "device": "SWAT-A2-00112233",
  "user": "Juan",
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

## Ejemplos de Integración

### Python - Añadir Usuario vía MQTT

```python
import paho.mqtt.client as mqtt
import json

def add_ble_user(client, device_serial, slot, key_hex, name, permissions=3):
    """Añade un usuario BLE al dispositivo."""
    topic = f"swatidhome/cmd/{device_serial}"
    payload = {
        "message_type": 6,
        "message_id": 1001,
        "message_info": {
            "action": "add_user",
            "slot": slot,
            "key": key_hex,
            "name": name,
            "permissions": permissions
        }
    }
    client.publish(topic, json.dumps(payload))

# Ejemplo de uso
client = mqtt.Client()
client.connect("broker.example.com", 1883)

# Generar clave de 64 bytes (128 hex chars)
import secrets
key = secrets.token_hex(64)  # 128 caracteres hex

add_ble_user(client, "SWAT-A2-00112233", 1, key, "Juan García", 0x03)
```

### Python - Listar Usuarios

```python
def list_ble_users(client, device_serial):
    """Solicita lista de usuarios BLE."""
    topic = f"swatidhome/cmd/{device_serial}"
    payload = {
        "message_type": 6,
        "message_id": 1002,
        "message_info": {
            "action": "list_users"
        }
    }
    client.publish(topic, json.dumps(payload))
```

### JavaScript - App Móvil BLE

```javascript
// Autenticación con clave
async function authenticateBLE(device, key64bytes) {
  const service = await device.gatt.getPrimaryService('0000ff00-0000-1000-8000-00805f9b34fb');
  const authChar = await service.getCharacteristic('0000ff02-0000-1000-8000-00805f9b34fb');
  
  // Suscribirse a notificaciones
  await authChar.startNotifications();
  authChar.addEventListener('characteristicvaluechanged', (event) => {
    const response = new TextDecoder().decode(event.target.value);
    console.log('Auth response:', response);
  });
  
  // Enviar clave
  await authChar.writeValue(key64bytes);
}

// Añadir usuario (solo superadmin)
async function addUser(device, slot, keyHex, permissions, name) {
  const authChar = await getAuthCharacteristic(device);
  const command = `ADD_USER:${slot}:${keyHex}:${permissions}:${name}`;
  await authChar.writeValue(new TextEncoder().encode(command));
}
```

---

## Códigos de Error

| Código | Mensaje | Descripción |
|--------|---------|-------------|
| 0 | - | Éxito |
| 1 | `missing slot parameter` | Falta parámetro slot |
| 1 | `missing key parameter` | Falta parámetro key |
| 1 | `invalid slot (1-5)` | Slot fuera de rango |
| 1 | `invalid key length` | Clave no es 128 caracteres hex |
| 1 | `unknown BLE action` | Acción no reconocida |

---

## Consideraciones de Seguridad

1. **Claves**: Siempre usar claves aleatorias de 64 bytes. Nunca claves predecibles.

2. **Transporte**: Las claves se transmiten en hexadecimal (128 chars) por MQTT/BLE.

3. **Almacenamiento**: Las claves se almacenan en EEPROM con checksum de integridad.

4. **Key Preview**: En `list_users`, solo se muestran los primeros 8 bytes para verificación.

5. **Auto-registro**: Deshabilitado cuando existe superadmin para prevenir accesos no autorizados.

6. **Permisos**: Cada usuario tiene permisos granulares. Asignar el mínimo necesario.

7. **Timeout de autenticación**: Dispositivos conectados que no se autentiquen en 30 segundos serán desconectados automáticamente.

8. **Eventos de auditoría**: Todos los intentos de autenticación (exitosos y fallidos) generan eventos MQTT.

---

## Eventos de Seguridad

### Evento: Autenticación Fallida

```json
{
  "event": "AUTH_FAILED",
  "device": "SWAT-A2-00112233",
  "reason": "invalid_key",
  "superadmin_exists": true,
  "timestamp": "2026-02-05T12:30:00Z"
}
```

### Evento: Timeout de Autenticación

```json
{
  "event": "AUTH_TIMEOUT",
  "device": "SWAT-A2-00112233",
  "reason": "no_authentication_received",
  "timeout_ms": 30000,
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

## Changelog

### v4.0.0-rev2 (2026-02-05)
- Añadido timeout de autenticación (30 segundos)
- Desconexión automática de dispositivos no autenticados
- Eventos MQTT para intentos de autenticación fallidos
- Verificación de integridad (checksum) antes de autenticar
- Logs detallados para debugging de autenticación

### v4.0.0 (2026-02-05)
- Implementación inicial de gestión de usuarios BLE
- Comandos MQTT: `add_user`, `set_superadmin`, `clear_user`, `clear_superadmin`, `clear_all`, `get_status`, `list_users`
- Comandos BLE: `ADD_USER`, `DEL_USER`, `LIST_USERS`
- Eventos de auditoría para todas las operaciones
- Sistema de permisos granular
- Protección contra auto-registro no autorizado
