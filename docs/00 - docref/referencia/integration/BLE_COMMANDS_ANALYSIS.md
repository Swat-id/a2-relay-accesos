# Análisis de Comandos BLE - SWATID-A2

> **Versión:** 1.0  
> **Fecha de análisis:** 5 Febrero 2026  
> **Firmware analizado:** v4.1.0-BLE

Este documento analiza la documentación BLE proporcionada y la compara con la implementación real en el firmware para identificar inconsistencias y proporcionar correcciones.

---

## Resumen de Problemas Encontrados

| Sección | Severidad | Problema |
|---------|-----------|----------|
| FF01 Auth | 🔴 ALTA | Respuestas incompletas/incorrectas |
| FF02 Relay | 🔴 ALTA | Formato de comando no documentado |
| FF03 Mode | 🟡 MEDIA | Valores de modo incompletos |
| FF04 Codes | 🟡 MEDIA | Formato binario no documentado |
| FF06 RelayTime | 🔴 ALTA | Completamente no documentado |
| Errores | 🟡 MEDIA | Lista incompleta de errores |

---

## 1. Detección de Versión de Firmware

### ✅ Documentación Correcta

El método de detección mediante lectura de FF0B es correcto.

### ⚠️ Mejora Sugerida

```typescript
async function detectFirmwareVersion(deviceId: string): Promise<'v4.1' | 'v4.0'> {
  try {
    const challenge = await BLE.read(deviceId, 'FF0B');
    if (challenge && challenge.length === 16) {
      return 'v4.1';
    }
  } catch (error) {
    // FF0B no existe o error de lectura
  }
  return 'v4.0';
}
```

---

## 2. Autenticación (FF01)

### 🔴 Problemas Encontrados

#### 2.1 Respuestas Incorrectas/Incompletas

**Documentación original:**
```
- OK:SUPERADMIN_REGISTERED
- OK:AUTHENTICATED:TOKEN:XXXXXXXX
- ERROR:INVALID_KEY
```

**Implementación real en firmware:**

| Escenario | Respuesta Real |
|-----------|----------------|
| v4.1 Challenge-Response exitoso | `OK:TOKEN:XXXXXXXX` |
| v4.0 Nuevo Superadmin | `OK:SUPERADMIN_REGISTERED:TOKEN:XXXXXXXX` |
| v4.0 Usuario autenticado | `OK:AUTHENTICATED:TOKEN:XXXXXXXX` |
| Challenge-Response fallido | `ERROR:INVALID_RESPONSE` |
| Clave incorrecta (v4.0) | `ERROR:INVALID_KEY` |
| Longitud inválida | `ERROR:INVALID_LENGTH` |

### ✅ Corrección: Flujo de Autenticación v4.0 (Legacy)

```typescript
// Respuestas posibles:
// - "OK:SUPERADMIN_REGISTERED:TOKEN:XXXXXXXX" (nuevo superadmin)
// - "OK:AUTHENTICATED:TOKEN:XXXXXXXX" (usuario existente)
// - "ERROR:INVALID_KEY" (clave incorrecta)
// - "ERROR:INVALID_LENGTH" (no son 64 bytes)

interface AuthResponseV40 {
  success: boolean;
  isSuperadmin: boolean;
  token: string | null;  // 16 caracteres hex = 8 bytes
  error: string | null;
}

function parseAuthResponseV40(response: string): AuthResponseV40 {
  if (response.startsWith('OK:SUPERADMIN_REGISTERED:TOKEN:')) {
    return {
      success: true,
      isSuperadmin: true,
      token: response.substring(31, 47),  // 16 chars
      error: null
    };
  }
  
  if (response.startsWith('OK:AUTHENTICATED:TOKEN:')) {
    return {
      success: true,
      isSuperadmin: false,
      token: response.substring(23, 39),  // 16 chars
      error: null
    };
  }
  
  return {
    success: false,
    isSuperadmin: false,
    token: null,
    error: response
  };
}
```

### ✅ Corrección: Flujo de Autenticación v4.1 (Challenge-Response)

```typescript
// Paso 1: Leer challenge
// FF0B devuelve 16 bytes (nonce aleatorio)

// Paso 2: Calcular response
// response = SHA256(key_64_bytes + nonce_16_bytes) = 32 bytes

// Paso 3: Enviar a FF01
// Escribir 32 bytes

// Respuestas posibles:
// - "OK:TOKEN:XXXXXXXX" (autenticación exitosa)
// - "ERROR:INVALID_RESPONSE" (hash incorrecto)
// - "ERROR:INVALID_LENGTH" (no son 32 bytes)

interface AuthResponseV41 {
  success: boolean;
  token: string | null;  // 16 caracteres hex = 8 bytes
  error: string | null;
}

function parseAuthResponseV41(response: string): AuthResponseV41 {
  if (response.startsWith('OK:TOKEN:')) {
    return {
      success: true,
      token: response.substring(9, 25),  // 16 chars
      error: null
    };
  }
  
  return {
    success: false,
    token: null,
    error: response
  };
}
```

---

## 3. Control de Relés (FF02)

### 🔴 Problema: Formato No Documentado

La documentación original no especifica el formato del comando.

### ✅ Corrección: Formato de Comando FF02

```
Formato WRITE:
┌─────────┬─────────┬─────────────────────────────┐
│ Byte 0  │ Byte 1  │ Bytes 2-3 (opcional)        │
│ relay_id│ action  │ duration_ms (big-endian)    │
└─────────┴─────────┴─────────────────────────────┘

relay_id:
  1 = Relé 1
  2 = Relé 2

action:
  0 = OFF (desactivar inmediatamente)
  1 = ON (activar con duración configurada o especificada)
  2 = PULSE (igual que ON)

duration_ms (opcional):
  Si se omite: usa duración configurada del dispositivo
  Si se incluye: duración en milisegundos (big-endian, 2 bytes)
```

### Ejemplos de Bytes

```typescript
// Activar relé 1 con duración configurada (2 bytes)
const activateRelay1Default = new Uint8Array([0x01, 0x01]);

// Activar relé 2 con duración configurada
const activateRelay2Default = new Uint8Array([0x02, 0x01]);

// Activar relé 1 por 5000ms (4 bytes)
const activateRelay1_5sec = new Uint8Array([0x01, 0x01, 0x13, 0x88]); // 0x1388 = 5000

// Activar relé 2 por 3000ms (4 bytes)  
const activateRelay2_3sec = new Uint8Array([0x02, 0x01, 0x0B, 0xB8]); // 0x0BB8 = 3000

// Desactivar relé 1
const deactivateRelay1 = new Uint8Array([0x01, 0x00]);

// Desactivar relé 2
const deactivateRelay2 = new Uint8Array([0x02, 0x00]);
```

### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:RELAY_ON` | Relé activado |
| `OK:RELAY_OFF` | Relé desactivado |
| `ERROR:NOT_AUTHORIZED` | Sin autenticación o permiso |
| `ERROR:INVALID_RELAY` | relay_id no es 1 o 2 |

---

## 4. Cambio de Modo (FF03)

### 🟡 Problema: Valores Incompletos

### ✅ Corrección: Formato de Comando FF03

```
Formato WRITE:
┌─────────┐
│ Byte 0  │
│ mode    │
└─────────┘

mode:
  0 = NORMAL
  1 = TORNO (Turnstile)
```

### READ FF03

```
Devuelve 1 byte:
  0 = Modo normal
  1 = Modo torno
```

### Ejemplos

```typescript
// Cambiar a modo normal
const setModeNormal = new Uint8Array([0x00]);

// Cambiar a modo torno
const setModeTurnstile = new Uint8Array([0x01]);
```

### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:MODE_NORMAL` | Cambiado a modo normal |
| `OK:MODE_TURNSTILE` | Cambiado a modo torno |
| `ERROR:NOT_AUTHORIZED` | Sin autenticación o permiso |

---

## 5. Añadir Códigos (FF04)

### 🟡 Problema: Formato Binario No Documentado Correctamente

### ✅ Corrección: Formato de Comando FF04

```
Formato WRITE:
┌─────────┬──────────┬─────────┬─────────────────────┐
│ Byte 0  │ Byte 1   │ Byte 2  │ Bytes 3-18 (hasta)  │
│ type    │ keyboard │ relay   │ code (ASCII)        │
└─────────┴──────────┴─────────┴─────────────────────┘

type:
  0 = PIN
  1 = TAG

keyboard:
  0 = Ambos teclados
  1 = Solo teclado 1
  2 = Solo teclado 2

relay:
  1 = Relé 1
  2 = Relé 2

code:
  1-16 caracteres ASCII
```

### Ejemplos de Bytes

```typescript
// Añadir PIN "123456" para ambos teclados, relé 1
// type=0, keyboard=0, relay=1, code="123456"
const addPIN123456 = new Uint8Array([
  0x00,  // type = PIN
  0x00,  // keyboard = ambos
  0x01,  // relay = 1
  0x31, 0x32, 0x33, 0x34, 0x35, 0x36  // "123456" en ASCII
]);

// Añadir TAG "A1B2C3D4" para teclado 2, relé 2
// type=1, keyboard=2, relay=2, code="A1B2C3D4"
const addTAG = new Uint8Array([
  0x01,  // type = TAG
  0x02,  // keyboard = teclado 2
  0x02,  // relay = 2
  0x41, 0x31, 0x42, 0x32, 0x43, 0x33, 0x44, 0x34  // "A1B2C3D4" en ASCII
]);
```

### Función Helper

```typescript
function buildAddCodeCommand(
  type: 'PIN' | 'TAG',
  keyboard: 0 | 1 | 2,
  relay: 1 | 2,
  code: string
): Uint8Array {
  const typeCode = type === 'PIN' ? 0 : 1;
  const codeBytes = new TextEncoder().encode(code);
  
  return new Uint8Array([
    typeCode,
    keyboard,
    relay,
    ...codeBytes
  ]);
}
```

### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:CODE_ADDED:N` | Código añadido (N = total de códigos) |
| `ERROR:NOT_AUTHENTICATED` | Sin autenticación |
| `ERROR:NO_PERMISSION` | Sin permiso ADD_CODES |
| `ERROR:STORAGE_NOT_READY` | Error interno |
| `ERROR:INVALID_TYPE` | type no es 0 o 1 |
| `ERROR:INVALID_KEYBOARD` | keyboard no está en 0-2 |
| `ERROR:INVALID_RELAY` | relay no es 1 o 2 |
| `ERROR:INVALID_CODE_LENGTH` | código vacío o > 16 chars |
| `ERROR:CODE_EXISTS_OR_FULL` | duplicado o memoria llena |
| `ERROR:INVALID_LENGTH` | menos de 4 bytes |

---

## 6. Tiempo de Relé (FF06)

### 🔴 Problema: No Documentado

La documentación original dice "Tiempo de activación" pero no especifica el formato.

### ✅ Corrección: Formato de Comando FF06

```
Formato READ:
  Devuelve 4 bytes (uint32_t little-endian)
  Valor en milisegundos

Formato WRITE:
  4 bytes (uint32_t little-endian)
  Valor en milisegundos
```

### Ejemplos

```typescript
// Leer tiempo actual
const currentTime = await BLE.read(deviceId, 'FF06');
const timeMs = new DataView(currentTime.buffer).getUint32(0, true); // little-endian
console.log(`Tiempo actual: ${timeMs}ms = ${timeMs/1000}s`);

// Establecer tiempo a 5 segundos (5000ms)
const newTime = new Uint8Array(4);
new DataView(newTime.buffer).setUint32(0, 5000, true); // little-endian
await BLE.write(deviceId, 'FF06', newTime);
```

### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:RELAY_TIME_SET` | Tiempo configurado |
| `ERROR:NOT_AUTHORIZED` | Sin autenticación o permiso |

---

## 7. Estado del Dispositivo (FF07)

### ⚠️ Aclaración

FF07 devuelve JSON pero el formato exacto no estaba documentado.

### ✅ Formato de Respuesta FF07

```json
{
  "relay1": false,
  "relay2": false,
  "mode": "normal",
  "auth": true,
  "user": "SUPERADMIN",
  "eth_connected": true,
  "mqtt_connected": true
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| relay1 | boolean | Estado relé 1 |
| relay2 | boolean | Estado relé 2 |
| mode | string | "normal" o "torno" |
| auth | boolean | Si el cliente BLE está autenticado |
| user | string | Nombre del usuario autenticado |
| eth_connected | boolean | Conexión Ethernet |
| mqtt_connected | boolean | Conexión MQTT |

---

## 8. Información del Dispositivo (FF08)

### ✅ Formato Correcto

```json
{
  "type": "A2",
  "serial": "D8F7B4BF1388",
  "firmware": "v4.1.0-BLE",
  "has_superadmin": true,
  "users_count": 2
}
```

---

## 9. Información Completa (FF09)

### ⚠️ Requiere Autenticación

### ✅ Formato de Respuesta FF09

```json
{
  "serial": "D8F7B4BF1388",
  "firmware": "v4.1.0-BLE",
  "ip": "192.168.1.100",
  "dhcp": true,
  "mode": "normal",
  "relay_time": 5000,
  "local_codes_count": 15,
  "local_codes_max": 50,
  "users": [
    {"slot": 0, "name": "SUPERADMIN", "enabled": true},
    {"slot": 1, "name": "Juan", "enabled": true},
    {"slot": 2, "name": "", "enabled": false}
  ]
}
```

---

## 10. Lista de Códigos (FF0A)

### ⚠️ Soporta Paginación

### READ FF0A

Devuelve página 0 por defecto.

### WRITE FF0A (Solicitar Página)

```json
{"page": 1}
```

### Formato de Respuesta

```json
{
  "page": 0,
  "total_pages": 3,
  "total_codes": 25,
  "codes": [
    {"type": "PIN", "value": "123456", "keyboard": 0, "relay": 1},
    {"type": "TAG", "value": "A1B2C3D4", "keyboard": 1, "relay": 2}
  ]
}
```

---

## 11. Challenge (FF0B) - Solo v4.1

### ✅ Documentación Correcta

READ devuelve 16 bytes (nonce aleatorio).

---

## 12. Lista Completa de Errores

| Error | Características | Descripción |
|-------|-----------------|-------------|
| `ERROR:NOT_AUTHENTICATED` | FF04, FF09, FF0A | Sin autenticación |
| `ERROR:NOT_AUTHORIZED` | FF02, FF03, FF05, FF06 | Sin autenticación o permiso |
| `ERROR:NO_PERMISSION` | FF04 | Sin permiso específico |
| `ERROR:INVALID_KEY` | FF01 | Clave incorrecta (v4.0) |
| `ERROR:INVALID_RESPONSE` | FF01 | Challenge-Response fallido (v4.1) |
| `ERROR:INVALID_LENGTH` | FF01, FF04 | Longitud de datos incorrecta |
| `ERROR:INVALID_RELAY` | FF02, FF04 | Relé no es 1 o 2 |
| `ERROR:INVALID_TYPE` | FF04 | Tipo no es PIN o TAG |
| `ERROR:INVALID_KEYBOARD` | FF04 | Teclado no está en 0-2 |
| `ERROR:INVALID_CODE_LENGTH` | FF04 | Código vacío o muy largo |
| `ERROR:CODE_EXISTS_OR_FULL` | FF04 | Duplicado o memoria llena |
| `ERROR:STORAGE_NOT_READY` | FF04 | Error interno de almacenamiento |
| `ERROR:INVALID_SLOT` | FF01 (gestión usuarios) | Slot fuera de rango |
| `ERROR:INVALID_PARAMS` | FF01 (gestión usuarios) | Parámetros incorrectos |
| `ERROR:UNKNOWN_COMMAND` | FF01 (gestión usuarios) | Comando no reconocido |

---

## 13. Resumen de Correcciones Necesarias en la App

### 🔴 Alta Prioridad

1. **Parseo de respuestas FF01:** Actualizar para incluir todos los formatos de respuesta con token
2. **Formato FF02:** Implementar correctamente el formato binario de control de relés
3. **Formato FF06:** Implementar lectura/escritura de tiempo de relé en milisegundos

### 🟡 Media Prioridad

4. **Formato FF04:** Verificar el formato binario de añadir códigos
5. **Errores:** Manejar todos los códigos de error documentados
6. **FF07/FF09/FF0A:** Parsear correctamente los JSON de respuesta

### 🟢 Baja Prioridad

7. **Documentación:** Actualizar documentación interna con estos cambios

---

## 14. Código de Validación Sugerido

```typescript
// Validar respuesta antes de procesar
function validateBLEResponse(response: string): {
  isOK: boolean;
  isError: boolean;
  errorCode?: string;
} {
  if (response.startsWith('OK:')) {
    return { isOK: true, isError: false };
  }
  
  if (response.startsWith('ERROR:')) {
    const errorCode = response.substring(6);
    return { isOK: false, isError: true, errorCode };
  }
  
  // Respuesta inesperada
  return { isOK: false, isError: false };
}

// Extraer token de respuesta de autenticación
function extractToken(response: string): string | null {
  const tokenMatch = response.match(/TOKEN:([A-Fa-f0-9]{16})/);
  return tokenMatch ? tokenMatch[1] : null;
}
```

---

**Documento creado:** 5 Febrero 2026  
**Analista:** Sistema SWATID  
**Versión:** 1.0
