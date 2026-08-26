# Gestión de Códigos - MQTT y BLE Protocol

> **Versión:** 4.1.0  
> **Última actualización:** 2026-02-05  
> **Dispositivo:** SWATID-A2

Este documento describe todos los comandos disponibles para la gestión de códigos de acceso tanto locales como remotos.

## Cambios en v4.1.0

- Mejoras en la persistencia de códigos en EEPROM
- Logging detallado para diagnóstico de problemas
- Verificación de integridad post-guardado
- Corrección del parsing de códigos BLE (manejo de bytes nulos)

---

## Índice

1. [Tipos de Códigos](#tipos-de-códigos)
2. [Comandos MQTT - Códigos Locales](#comandos-mqtt---códigos-locales)
3. [Comandos MQTT - Códigos Remotos](#comandos-mqtt---códigos-remotos)
4. [Comandos BLE](#comandos-ble)
5. [Eventos Publicados](#eventos-publicados)
6. [Ejemplos de Integración](#ejemplos-de-integración)

---

## Tipos de Códigos

### Códigos Locales

- Almacenados en EEPROM del dispositivo
- Validación inmediata sin conexión a servidor
- Máximo: 50 códigos
- Sin restricciones de horario

### Códigos Remotos

- Almacenados en EEPROM del dispositivo
- Pueden tener franjas horarias
- Máximo: 30 códigos con hasta 4 franjas horarias cada uno
- Validación local con restricción de tiempo

### Estructura de un Código

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `code_type` | string | `"PIN"` o `"TAG"` |
| `code_value` | string | Valor del código (1-16 caracteres) |
| `keyboard_id` | int | `0` = ambos, `1` = teclado 1, `2` = teclado 2 |
| `relay` | int | Relé a activar: `1` o `2` |

---

## Comandos MQTT - Códigos Locales

### Topic de Comandos

```
swatidhome/cmd/{serial_number}
```

### Estructura Base del Mensaje

```json
{
  "message_type": 5,
  "message_id": <número>,
  "message_info": {
    "action": "<acción>",
    ...parámetros específicos
  }
}
```

---

### 1. Añadir Código Local (`add_local_code`)

**Request:**
```json
{
  "message_type": 5,
  "message_id": 1001,
  "message_info": {
    "action": "add_local_code",
    "code_type": "PIN",
    "code_value": "123456",
    "keyboard_id": 0,
    "relay": 1
  }
}
```

**Parámetros:**

| Campo | Tipo | Requerido | Descripción |
|-------|------|-----------|-------------|
| `code_type` | string | ✓ | `"PIN"` o `"TAG"` |
| `code_value` | string | ✓ | Código (1-16 caracteres) |
| `keyboard_id` | int | ✗ | Teclado (0=ambos, 1, 2). Default: 0 |
| `relay` | int | ✓ | Relé a activar (1 o 2) |

**Response OK:**
```json
{
  "message_type": 0,
  "message_id": 1001,
  "error_code": 0,
  "message_info": "local code added: {\"code_type\":\"PIN\",\"code_value\":\"123456\",\"keyboard_id\":0,\"relay\":1,\"total_codes\":5}"
}
```

**Response Error:**
```json
{
  "message_type": 0,
  "message_id": 1001,
  "error_code": 1,
  "message_info": "failed to add local code (may exist or memory full)"
}
```

---

### 2. Eliminar Código Local (`remove_local_code`)

**Request:**
```json
{
  "message_type": 5,
  "message_id": 1002,
  "message_info": {
    "action": "remove_local_code",
    "code_type": "PIN",
    "code_value": "123456",
    "keyboard_id": 0
  }
}
```

**Parámetros:**

| Campo | Tipo | Requerido | Descripción |
|-------|------|-----------|-------------|
| `code_type` | string | ✓ | `"PIN"` o `"TAG"` |
| `code_value` | string | ✓ | Código a eliminar |
| `keyboard_id` | int | ✗ | Si se omite, elimina la primera coincidencia |

**Response OK:**
```json
{
  "message_type": 0,
  "message_id": 1002,
  "error_code": 0,
  "message_info": "local code removed: PIN 123456"
}
```

---

### 3. Eliminar Todos los Códigos Locales (`clear_local_codes`)

**Request:**
```json
{
  "message_type": 5,
  "message_id": 1003,
  "message_info": {
    "action": "clear_local_codes"
  }
}
```

**Response OK:**
```json
{
  "message_type": 0,
  "message_id": 1003,
  "error_code": 0,
  "message_info": "all local codes cleared"
}
```

---

### 4. Listar Códigos Locales (`list_local_codes`)

**Request:**
```json
{
  "message_type": 5,
  "message_id": 1004,
  "message_info": {
    "action": "list_local_codes"
  }
}
```

**Response:**
```json
{
  "message_type": 0,
  "message_id": 1004,
  "error_code": 0,
  "message_info": "{\"count\":3,\"max\":50,\"validation_mode\":\"local_first\",\"codes\":[{\"id\":0,\"type\":\"PIN\",\"value\":\"123456\",\"keyboard\":0,\"relay\":1},{\"id\":1,\"type\":\"TAG\",\"value\":\"A1B2C3D4\",\"keyboard\":1,\"relay\":2}]}"
}
```

> **Nota:** La respuesta está limitada a 30 códigos. Si hay más, incluirá `"truncated": true`.

---

## Comandos MQTT - Códigos Remotos

Los códigos remotos permiten franjas horarias para control de acceso temporal.

### 1. Añadir Código Remoto (`add_remote_code`)

**Request:**
```json
{
  "message_type": 5,
  "message_id": 2001,
  "message_info": {
    "action": "add_remote_code",
    "code_type": "PIN",
    "code_value": "999888",
    "keyboard_id": 1,
    "relay": 1,
    "time_slots": [
      {
        "start_hour": 8,
        "start_minute": 0,
        "end_hour": 18,
        "end_minute": 0,
        "days_of_week": 31
      }
    ]
  }
}
```

**Parámetros de `time_slots`:**

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `start_hour` | int | Hora de inicio (0-23) |
| `start_minute` | int | Minuto de inicio (0-59) |
| `end_hour` | int | Hora de fin (0-23) |
| `end_minute` | int | Minuto de fin (0-59) |
| `days_of_week` | int | Bitmask de días (1=Lun...64=Dom, 127=todos) |

**Bitmask de días:**
- `1` = Lunes
- `2` = Martes
- `4` = Miércoles
- `8` = Jueves
- `16` = Viernes
- `32` = Sábado
- `64` = Domingo
- `31` = Lunes a Viernes
- `127` = Todos los días

---

### 2. Eliminar Código Remoto (`remove_remote_code`)

**Request:**
```json
{
  "message_type": 5,
  "message_id": 2002,
  "message_info": {
    "action": "remove_remote_code",
    "code_type": "PIN",
    "code_value": "999888"
  }
}
```

---

### 3. Eliminar Todos los Códigos Remotos (`clear_remote_codes`)

**Request:**
```json
{
  "message_type": 5,
  "message_id": 2003,
  "message_info": {
    "action": "clear_remote_codes"
  }
}
```

---

## Comandos BLE

### Característica FF04 - Añadir Código

**UUID:** `0000FF04-0000-1000-8000-00805f9b34fb`  
**Propiedades:** Write, Notify  
**Requiere:** Autenticación + Permiso `BLE_PERM_ADD_CODES` (0x04)

### Formato del Comando (Binario)

```
Byte 0: Tipo (0=PIN, 1=TAG)
Byte 1: Teclado (0=ambos, 1=teclado1, 2=teclado2)
Byte 2: Relé (1 o 2)
Bytes 3+: Código (hasta 16 caracteres ASCII)
```

### Ejemplo - Añadir PIN "123456" para teclado 1, relé 1

```
Bytes: [0x00, 0x01, 0x01, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36]
        tipo  keyb  relay  '1'   '2'   '3'   '4'   '5'   '6'
```

### Ejemplo - Añadir TAG "ABCD1234" para ambos teclados, relé 2

```
Bytes: [0x01, 0x00, 0x02, 0x41, 0x42, 0x43, 0x44, 0x31, 0x32, 0x33, 0x34]
        tipo  keyb  relay  'A'   'B'   'C'   'D'   '1'   '2'   '3'   '4'
```

### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:CODE_ADDED:N` | Código añadido, N = total de códigos |
| `ERROR:NOT_AUTHENTICATED` | No autenticado por BLE |
| `ERROR:NO_PERMISSION` | Sin permiso para añadir códigos |
| `ERROR:INVALID_TYPE` | Tipo debe ser 0 (PIN) o 1 (TAG) |
| `ERROR:INVALID_KEYBOARD` | Teclado debe ser 0-2 |
| `ERROR:INVALID_RELAY` | Relé debe ser 1 o 2 |
| `ERROR:INVALID_CODE_LENGTH` | Código vacío o >16 caracteres |
| `ERROR:CODE_EXISTS_OR_FULL` | Código duplicado o memoria llena |
| `ERROR:INVALID_LENGTH` | Mensaje muy corto (mínimo 4 bytes) |

### Permisos Necesarios

Para añadir códigos por BLE, el usuario debe tener el permiso `BLE_PERM_ADD_CODES` (0x04):

```
Permisos recomendados para usuario con acceso a códigos:
- BLE_PERM_RELAY_CONTROL (0x01)
- BLE_PERM_MODE_CHANGE (0x02)
- BLE_PERM_ADD_CODES (0x04)
Total: 0x07
```

---

## Eventos Publicados

### Topic de Eventos

```
swatidhome/events/{serial_number}/codes
```

### Evento: Código Añadido por BLE

```json
{
  "event": "CODE_ADDED",
  "source": "BLE",
  "user": "SUPERADMIN",
  "code_type": "PIN",
  "code_value": "123456",
  "keyboard": 0,
  "relay": 1,
  "total_codes": 5,
  "timestamp": "2026-02-05T12:30:00Z"
}
```

### Evento: Código Añadido por MQTT

Se publica en el topic de respuesta estándar.

---

## Ejemplos de Integración

### Python - Añadir Código Local vía MQTT

```python
import paho.mqtt.client as mqtt
import json

def add_local_code(client, device_serial, code_type, code_value, relay, keyboard_id=0):
    """Añade un código local al dispositivo."""
    topic = f"swatidhome/cmd/{device_serial}"
    payload = {
        "message_type": 5,
        "message_id": 1001,
        "message_info": {
            "action": "add_local_code",
            "code_type": code_type,
            "code_value": code_value,
            "keyboard_id": keyboard_id,
            "relay": relay
        }
    }
    client.publish(topic, json.dumps(payload))

# Ejemplo de uso
client = mqtt.Client()
client.connect("broker.example.com", 1883)

# Añadir PIN
add_local_code(client, "SWAT-A2-00112233", "PIN", "123456", 1)

# Añadir TAG
add_local_code(client, "SWAT-A2-00112233", "TAG", "ABCD1234", 2, keyboard_id=1)
```

### Python - Listar Códigos

```python
def list_local_codes(client, device_serial):
    """Solicita lista de códigos locales."""
    topic = f"swatidhome/cmd/{device_serial}"
    payload = {
        "message_type": 5,
        "message_id": 1002,
        "message_info": {
            "action": "list_local_codes"
        }
    }
    client.publish(topic, json.dumps(payload))
```

### JavaScript - App Móvil BLE

```javascript
// Añadir código por BLE (FF04)
async function addCodeBLE(device, codeType, keyboardId, relay, codeValue) {
  const service = await device.gatt.getPrimaryService('0000ff00-0000-1000-8000-00805f9b34fb');
  const addCodeChar = await service.getCharacteristic('0000ff04-0000-1000-8000-00805f9b34fb');
  
  // Suscribirse a notificaciones para respuesta
  await addCodeChar.startNotifications();
  addCodeChar.addEventListener('characteristicvaluechanged', (event) => {
    const response = new TextDecoder().decode(event.target.value);
    console.log('Add code response:', response);
  });
  
  // Construir comando
  const type = codeType === 'PIN' ? 0 : 1;
  const codeBytes = new TextEncoder().encode(codeValue);
  const command = new Uint8Array([type, keyboardId, relay, ...codeBytes]);
  
  await addCodeChar.writeValue(command);
}

// Ejemplo de uso
await addCodeBLE(device, 'PIN', 0, 1, '123456');
```

---

## Códigos de Error

| Error | Descripción |
|-------|-------------|
| `missing parameters` | Faltan parámetros requeridos |
| `invalid code_type` | Tipo debe ser "PIN" o "TAG" |
| `invalid code_value` | Código vacío o muy largo |
| `invalid keyboard_id` | Teclado fuera de rango (0-2) |
| `invalid relay` | Relé fuera de rango (1-2) |
| `failed to add local code` | Código duplicado o memoria llena |
| `local code not found` | Código a eliminar no encontrado |

---

## Capacidad de Almacenamiento

| Tipo | Máximo | Tamaño por código |
|------|--------|-------------------|
| Códigos locales | 50 | ~24 bytes |
| Códigos remotos | 30 | ~40 bytes (con franjas horarias) |

---

## Changelog

### v4.1.0 (2026-02-05)
- **Corregido problema de persistencia de códigos en EEPROM**
  - Verificación de integridad post-guardado con lectura de comprobación
  - Corrección automática del marcador de validación si está corrupto
  - Logging exhaustivo durante todo el proceso de guardado
- **Mejorado parsing de códigos BLE (FF04)**
  - Extracción correcta del código sin depender de null terminator
  - Validación de `storedCodes` antes de cualquier operación
  - Buffer seguro para códigos de hasta 16 caracteres
- **Diagnóstico mejorado**
  - Logs detallados en `addCode()` y `saveStoredCodes()`
  - Verificación del último código guardado en EEPROM
  - Información de estado antes y después de cada operación

### v4.0.0 (2026-02-05)
- Añadidos comandos MQTT para códigos locales: `add_local_code`, `remove_local_code`, `clear_local_codes`, `list_local_codes`
- Mejorado callback BLE (FF04) con logs detallados y validaciones
- Eventos MQTT al añadir códigos por BLE
- Documentación completa del protocolo
