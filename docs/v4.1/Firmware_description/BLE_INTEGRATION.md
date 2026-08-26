# Integración BLE - SWATID-A2 v4.1

> **Versión Firmware:** v4.1.0-BLE  
> **Última actualización:** Marzo 2026  
> **Protocolo BLE:** NimBLE 1.4.x  
> **Arquitectura:** Challenge-Response con HKDF

---

## Índice

1. [Arquitectura General](#arquitectura-general)
2. [Servicio y Características BLE](#servicio-y-características-ble)
3. [Sistema de Autenticación](#sistema-de-autenticación)
4. [Gestión de Usuarios BLE](#gestión-de-usuarios-ble)
5. [Características y Operaciones](#características-y-operaciones)
6. [Protocolo de Transmisión con EOT](#protocolo-de-transmisión-con-eot)
7. [Ejemplos Probados y Validados](#ejemplos-probados-y-validados)
8. [Troubleshooting](#troubleshooting)

---

## Arquitectura General

### Diagrama de Conexión

```
┌─────────────────┐          BLE          ┌─────────────────────┐
│    App Móvil    │◄────────────────────►│   SWATID-A2 (ESP32) │
│   (Cliente)     │                        │     (Servidor)      │
└─────────────────┘                        └─────────────────────┘
        │                                           │
        │ 1. Scan & Connect                         │
        │ 2. Read FF08 (Device Info)                │
        │ 3. Read FF0B (Challenge) ────────────────►│
        │◄────────────────────────── Challenge (16B)│
        │ 4. SHA256(key + challenge) ──────────────►│
        │◄────────────────────────── Token (8B)     │
        │ 5. Operaciones autenticadas               │
        └───────────────────────────────────────────┘
```

### Flujo de Autenticación v4.1

```
App                                          Firmware
 │                                              │
 ├──── Read FF08 (Device Info) ────────────────►│
 │◄─── JSON con superadmin_registered ─────────┤
 │                                              │
 ├──── Read FF0B (Challenge) ──────────────────►│
 │◄─── 16 bytes (challenge aleatorio) ─────────┤
 │                                              │
 │ [Calcular: SHA256(stored_key + challenge)]   │
 │                                              │
 ├──── Write FF01 (32 bytes response) ─────────►│
 │                                              │
 │     [Firmware verifica respuesta]            │
 │                                              │
 │◄─── NOTIFY: "OK:TOKEN:XXXXXXXXXXXXXXXX" ────┤
 │                                              │
 └──────── Sesión autenticada (5 min) ──────────┘
```

---

## Servicio y Características BLE

### UUID del Servicio

```
SERVICE_UUID = "0000FF00-0000-1000-8000-00805F9B34FB"
```

### Tabla de Características

| UUID | Nombre | Propiedades | Autenticación | Descripción |
|------|--------|-------------|---------------|-------------|
| FF01 | Auth | W, N | No | Autenticación Challenge-Response |
| FF02 | Relay | W, WNR, N | Sí | Control de relés |
| FF03 | Mode | R, W, WNR, N | Sí | Modo operación (normal/torno) |
| FF04 | AddCode | W, WNR, N | Sí | Añadir códigos PIN/TAG |
| FF05 | Network | R, W, WNR, N | Sí | Configuración Ethernet |
| FF06 | RelayTime | R, W, WNR | Sí | Tiempo de activación relé |
| FF07 | Status | R, N | Sí | Estado del dispositivo |
| FF08 | DevInfo | R, N | No | Información pública del dispositivo |
| FF09 | FullInfo | R, N | Sí | Configuración completa |
| FF0A | Codes | R, W, WNR, N | Sí | Lista de códigos locales |
| FF0B | Challenge | R | No | Challenge para autenticación v4.1 |

**Propiedades:**
- `R` = READ
- `W` = WRITE (con ACK)
- `WNR` = WRITE_NO_RESPONSE (sin ACK)
- `N` = NOTIFY

---

## Sistema de Autenticación

### Arquitectura de Seguridad v4.1

El sistema v4.1 implementa autenticación **Challenge-Response** que evita transmitir la clave por BLE:

1. **Clave Maestra del Dispositivo**: Almacenada en EEPROM, nunca transmitida
2. **Challenge**: 16 bytes aleatorios generados por el dispositivo
3. **Response**: SHA256(clave + challenge) calculado por la App
4. **Token de Sesión**: 8 bytes válidos por 5 minutos

### Registro de SUPERADMIN

El primer usuario que se registra se convierte en SUPERADMIN con permisos completos.

**Flujo de Registro:**

```typescript
// 1. Verificar que no hay superadmin registrado
const deviceInfo = await readFF08();
if (deviceInfo.superadmin_registered) {
  throw new Error('Superadmin ya registrado');
}

// 2. Generar clave de 64 bytes
const key = crypto.getRandomValues(new Uint8Array(64));

// 3. Enviar a FF01 para registro
await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF01, key.buffer);

// 4. Esperar confirmación
// NOTIFY: "OK:SUPERADMIN_REGISTERED" o "OK:TOKEN:XXXXXXXX"

// 5. Guardar clave localmente (nunca se transmite de nuevo)
await Preferences.set({ key: `ble_key_${serial}`, value: toHex(key) });
```

### Autenticación de Usuario Existente

```typescript
// 1. Leer challenge desde FF0B
const challengeData = await BleClient.read(deviceId, SERVICE_UUID, CHAR_FF0B);
const challenge = new Uint8Array(challengeData.buffer); // 16 bytes

// 2. Recuperar clave almacenada
const keyHex = await Preferences.get({ key: `ble_key_${serial}` });
const key = fromHex(keyHex.value); // 64 bytes

// 3. Calcular respuesta: SHA256(key + challenge)
const combined = concat(key, challenge); // 80 bytes
const response = await crypto.subtle.digest('SHA-256', combined); // 32 bytes

// 4. Enviar respuesta a FF01
await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF01, response);

// 5. Esperar NOTIFY con token
// "OK:TOKEN:E7F3CD2E3CBBCA4F"
```

### Sistema de Permisos

```cpp
// Permisos definidos en firmware
#define BLE_PERM_RELAY_CONTROL    0x01  // Control de relés
#define BLE_PERM_NETWORK_CONFIG   0x02  // Configuración de red
#define BLE_PERM_CODE_MANAGE      0x04  // Gestión de códigos
#define BLE_PERM_USER_MANAGE      0x08  // Gestión de usuarios
#define BLE_PERM_SYSTEM_CONFIG    0x10  // Configuración del sistema
#define BLE_PERM_ALL              0xFF  // Todos los permisos (SUPERADMIN)
```

---

## Gestión de Usuarios BLE

### Capacidad de Usuarios

- **Máximo**: 5 usuarios BLE (configurable con `BLE_MAX_USERS`)
- **Slot 0**: Reservado para SUPERADMIN
- **Slots 1-4**: Usuarios adicionales

### Añadir Usuario (Solo SUPERADMIN)

```typescript
// Formato: JSON con nombre y clave
const userData = {
  action: "add_user",
  name: "Usuario1",
  key: "64_bytes_hex_string...",
  permissions: 0x07  // RELAY + NETWORK + CODE_MANAGE
};

await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF01, 
  new TextEncoder().encode(JSON.stringify(userData)));

// Respuesta: "OK:USER_ADDED:1" (slot 1)
```

### Eliminar Usuario

```typescript
const deleteData = {
  action: "delete_user",
  slot: 1
};

await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF01,
  new TextEncoder().encode(JSON.stringify(deleteData)));

// Respuesta: "OK:USER_DELETED:1"
```

---

## Características y Operaciones

### FF02 - Control de Relés

**Formato binario (4 bytes):**

| Byte | Descripción | Valores |
|------|-------------|---------|
| 0 | Número de relé | 1 o 2 |
| 1 | Acción | 0=OFF, 1=ON, 2=PULSE |
| 2-3 | Duración (ms) | Little-endian, solo para PULSE |

**Ejemplo: Pulso de 2 segundos en relé 1**

```typescript
const bytes = new Uint8Array([
  0x01,       // Relé 1
  0x02,       // Acción: PULSE
  0xD0, 0x07  // 2000ms (little-endian)
]);

await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF02, bytes.buffer);
// Respuesta NOTIFY: "OK:RELAY_PULSE:1:2000"
```

**Log del firmware:**
```
🔵 [BLE] FF02 - Recibido: relé=1, acción=2, duración=2000ms
🔵 [BLE] Pulso en relé 1 (2000ms) por SUPERADMIN
```

### FF03 - Modo de Operación

**Lectura:**
```typescript
const data = await BleClient.read(deviceId, SERVICE_UUID, CHAR_FF03);
// Respuesta: "normal" o "torno"
```

**Escritura:**
```typescript
// 0x00 = normal, 0x01 = torno
await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF03, new Uint8Array([0x00]));
// Respuesta NOTIFY: "OK:MODE_CHANGED:normal"
```

### FF04 - Añadir Códigos PIN/TAG

**Formato binario:**

| Byte | Descripción |
|------|-------------|
| 0 | Tipo: 0=PIN, 1=TAG |
| 1 | Teclado origen (1 o 2) |
| 2 | Relé a activar (1 o 2) |
| 3+ | Código (string terminado en null) |

**Ejemplo: Añadir PIN "123456" para teclado 1, relé 1**

```typescript
function buildAddCodeBytes(type: 'pin'|'tag', code: string, keyboard: number, relay: number): Uint8Array {
  const codeBytes = new TextEncoder().encode(code);
  const bytes = new Uint8Array(3 + codeBytes.length + 1);
  
  bytes[0] = type === 'pin' ? 0x00 : 0x01;
  bytes[1] = keyboard;
  bytes[2] = relay;
  bytes.set(codeBytes, 3);
  bytes[3 + codeBytes.length] = 0x00; // Null terminator
  
  return bytes;
}

const bytes = buildAddCodeBytes('pin', '123456', 1, 1);
// Resultado: [0x00, 0x01, 0x01, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x00]

await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF04, bytes.buffer);
// Respuesta NOTIFY: "OK:CODE_ADDED:4" (4 = total de códigos)
```

**Respuestas posibles:**

| Respuesta | Descripción |
|-----------|-------------|
| `OK:CODE_ADDED:N` | Código añadido, N = total |
| `ERROR:NOT_AUTHENTICATED` | No autenticado |
| `ERROR:CODE_EXISTS` | Código duplicado |
| `ERROR:STORAGE_FULL` | Memoria llena (50 códigos max) |
| `ERROR:INVALID_TYPE` | Tipo no válido |
| `ERROR:INVALID_KEYBOARD` | Teclado no válido |
| `ERROR:INVALID_RELAY` | Relé no válido |

### FF05 - Configuración de Red

**Lectura (JSON con EOT):**

```typescript
// Suscribirse a NOTIFY
await BleClient.startNotifications(deviceId, SERVICE_UUID, CHAR_FF05, onNotify);

// Trigger con READ
await BleClient.read(deviceId, SERVICE_UUID, CHAR_FF05);

// Respuesta (termina con 0x04):
// {"dhcp":true,"eth":true,"ip":"192.168.5.86","gw":"192.168.5.1","mask":"255.255.255.0","mac":"88:13:BF:B4:F7:DB"}\x04
```

**Escritura (17 bytes binarios):**

| Offset | Tamaño | Campo |
|--------|--------|-------|
| 0 | 1 | DHCP (0=fijo, 1=DHCP) |
| 1-4 | 4 | IP Address |
| 5-8 | 4 | Gateway |
| 9-12 | 4 | Subnet Mask |
| 13-16 | 4 | DNS |

```typescript
function buildNetworkBytes(config: {dhcp: boolean, ip: string, gateway: string, subnet: string, dns: string}): Uint8Array {
  const bytes = new Uint8Array(17);
  bytes[0] = config.dhcp ? 0x01 : 0x00;
  
  if (!config.dhcp) {
    config.ip.split('.').forEach((p, i) => bytes[1 + i] = Number(p));
    config.gateway.split('.').forEach((p, i) => bytes[5 + i] = Number(p));
    config.subnet.split('.').forEach((p, i) => bytes[9 + i] = Number(p));
    config.dns.split('.').forEach((p, i) => bytes[13 + i] = Number(p));
  }
  return bytes;
}

// IP fija 192.168.5.88
const bytes = buildNetworkBytes({
  dhcp: false,
  ip: '192.168.5.88',
  gateway: '192.168.5.1',
  subnet: '255.255.255.0',
  dns: '192.168.5.1'
});
// Hex: 00c0a80558c0a80501ffffff00c0a80501

await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF05, bytes.buffer);
// Respuesta: "OK:NETWORK_CONFIGURED:RESTARTING"
// El dispositivo se reiniciará en 2 segundos
```

### FF06 - Tiempo de Relé

**Lectura:**
```typescript
const data = await BleClient.read(deviceId, SERVICE_UUID, CHAR_FF06);
const view = new DataView(data.buffer);
const milliseconds = view.getUint32(0, true); // Little-endian
console.log('Tiempo:', milliseconds, 'ms');
```

**Escritura:**
```typescript
const bytes = new Uint8Array(4);
new DataView(bytes.buffer).setUint32(0, 3000, true); // 3000ms
await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF06, bytes.buffer);
// Respuesta: "OK:RELAY_TIME:3000"
```

### FF07 - Estado del Dispositivo

```typescript
// Respuesta JSON:
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

### FF08 - Información del Dispositivo (Pública)

```typescript
// No requiere autenticación
const data = await BleClient.read(deviceId, SERVICE_UUID, CHAR_FF08);
// Respuesta JSON:
{
  "device_type": "SWATID-A2",
  "serial": "SWATID_D8F7B4BF1388",
  "firmware_version": "v4.1.0",
  "firmware_variant": "BLE",
  "protocol_version": 1,
  "capabilities": {
    "relays": 2,
    "wiegand_inputs": 2,
    "digital_inputs": 2,
    "ble_users": 5
  },
  "superadmin_registered": true,
  "active_users": 0
}
```

### FF09 - Configuración Completa

**JSON compacto (con EOT):**
```json
{
  "type": "SWATID-A2",
  "sn": "SWATID_D8F7B4BF1388",
  "name": "UWATID_D8F7B4BF1388",
  "fw": "v4.1.0",
  "net": {
    "ip": "192.168.5.86",
    "gw": "192.168.5.1",
    "mask": "255.255.255.0",
    "dhcp": true,
    "eth": true
  },
  "st": {
    "r1": false,
    "r2": false,
    "dur": 2,
    "mode": 0,
    "mqtt": true
  },
  "cd": {
    "loc": 3,
    "max": 50,
    "rem": 0
  },
  "up": 1234
}
```

### FF0A - Lista de Códigos

**Solicitud de página:**
```typescript
await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF0A, 
  new TextEncoder().encode('{"page":0}'));
```

**Respuesta JSON compacto (con EOT):**
```json
{
  "n": 3,
  "m": 50,
  "p": 0,
  "ps": 3,
  "tp": 1,
  "more": false,
  "c": [
    {"i": 0, "t": "PIN", "v": "123456", "k": 1, "r": 1},
    {"i": 1, "t": "PIN", "v": "654321", "k": 2, "r": 2},
    {"i": 2, "t": "TAG", "v": "12345678", "k": 1, "r": 1}
  ]
}
```

**Mapeo de campos:**
- `n` = count total
- `m` = max códigos
- `p` = página actual
- `ps` = tamaño de página
- `tp` = total páginas
- `c` = array de códigos
- `i` = id, `t` = type, `v` = value, `k` = keyboard, `r` = relay

---

## Protocolo de Transmisión con EOT

### Problema Resuelto

El MTU de BLE limita las transmisiones a ~240 bytes. Para JSONs más grandes, se implementó:

1. **Chunking**: Datos divididos en fragmentos de 240 bytes
2. **Marcador EOT**: Byte `0x04` indica fin de transmisión

### Detección de Fin de Transmisión

```typescript
const EOT = 0x04;
let accumulated = new Uint8Array();

function onNotify(data: DataView) {
  const chunk = new Uint8Array(data.buffer);
  accumulated = concat(accumulated, chunk);
  
  // Verificar si termina con EOT
  if (accumulated[accumulated.length - 1] === EOT) {
    // Quitar EOT y parsear
    const jsonBytes = accumulated.slice(0, -1);
    const json = new TextDecoder().decode(jsonBytes);
    const parsed = JSON.parse(json);
    
    // Limpiar para siguiente transmisión
    accumulated = new Uint8Array();
    
    return parsed;
  }
}
```

### Características que usan EOT

| Característica | Usa EOT | Notas |
|----------------|---------|-------|
| FF05 (Network) | ✅ | JSON de configuración |
| FF09 (FullInfo) | ✅ | Puede requerir chunking |
| FF0A (Codes) | ✅ | Paginado, 3 códigos por página |

---

## Ejemplos Probados y Validados

### Ejemplo 1: Flujo Completo de Autenticación

**Log de App (probado):**
```
[BLE-A2] ===== AUTHENTICATION START =====
[BLE-A2] Reading FF08 (Device Info - public)...
[BLE-A2] FF08 parsed: {device_type: 'SWATID-A2', serial: 'SWATID_D8F7B4BF1388', ...}
[BLE-A2] Device superadmin_registered: true
[BLE-A2] Using stored key
[BLE-A2] Key length: 64 bytes
[BLE-A2] Detecting firmware version - trying to read FF0B (Challenge)...
[BLE-A2] Firmware v4.1 detected - Challenge received: c6c338e2b3e1d00099c0fc4f7fd342cd
[BLE] Firmware version detected: v4.1
[BLE-A2] Computing SHA256(key + challenge)...
[BLE-A2] Sending 32-byte response to FF01...
[BLE-A2] *** Auth NOTIFY received: OK:TOKEN:13BA189BFB5E1462
[BLE] Session token set, expires in 5 min
[BLE-A2] ===== AUTHENTICATION COMPLETE =====
```

### Ejemplo 2: Control de Relé con Pulso

**Log de App:**
```
[A2] Sending relay 1 pulse command, duration: 2000ms
[BLE-A2] FF02 controlRelay: {relay: 1, action: 'pulse', actionByte: 2, durationMs: 2000, bytes: [1,2,208,7]}
[BLE-A2] FF02 controlRelay - Write successful, waiting for NOTIFY...
[BLE-A2] FF02 controlRelay - NOTIFY Response: OK:RELAY_PULSE:1:2000
```

**Log de Firmware:**
```
🔵 [BLE] FF02 - Recibido: relé=1, acción=2, duración=2000ms
🔵 [BLE] Pulso en relé 1 (2000ms) por SUPERADMIN
```

### Ejemplo 3: Añadir Código PIN

**Log de App:**
```
[BLE-A2] FF04 addCode: {type: 'pin', typeByte: 0, code: '990099', keyboard: 1, relay: 1, bytes: [0,1,1,57,57,48,48,57,57,0]}
[BLE-A2] FF04 addCode - Subscribing to NOTIFY...
[BLE-A2] FF04 addCode - Sending write...
[BLE-A2] FF04 addCode - NOTIFY Response: OK:CODE_ADDED:3
```

**Log de Firmware:**
```
🔵 [BLE] FF04 - Recibido
🔵 [BLE] FF04 - Auth: 1, Permisos: 0xFF
🔵 [BLE] FF04 - Tipo: PIN, Código: 990099, Teclado: 1, Relé: 1
🔵 [BLE] FF04 - Código añadido a RAM (total: 3)
💾 [LOOP] Guardando código pendiente en EEPROM...
💾 [LOOP] Código '990099' guardado en EEPROM
```

### Ejemplo 4: Configuración de Red con Reinicio

**Log de App:**
```
[A2] handleSaveEthernet called: {ethDhcp: false, ethIp: '192.168.5.106', ethMask: '255.255.255.0', ethGw: '192.168.5.1'}
[BLE-A2] FF05 setNetwork: {dhcp: false, ip: '192.168.5.106', bytes: [0,192,168,5,106,192,168,5,1,255,255,255,0,192,168,5,1]}
[BLE-A2] FF05 setNetwork - Subscribing to NOTIFY...
[BLE-A2] FF05 setNetwork NOTIFY chunk: 32 bytes
[BLE-A2] FF05 setNetwork response: OK:NETWORK_CONFIGURED:RESTARTING
[BLE-A2] FF05 setNetwork - SUCCESS, device will restart
```

**Log de Firmware:**
```
🔵 [BLE] FF05 - onWrite llamado
🔵 [BLE] FF05 - Auth: 1, Permisos: 0xFF, NETWORK_CONFIG: 0x02
🔵 [BLE] FF05 - Recibidos 17 bytes
🔵 [BLE] FF05 - Byte DHCP: 0x00 -> IP Fija
🔵 [BLE] FF05 - Nueva config: DHCP=false, IP=192.168.5.106
🔵 [BLE] FF05 - GW=192.168.5.1, Mask=255.255.255.0, DNS=192.168.5.1
🔵 [BLE] FF05 - Configuración guardada en EEPROM
🔵 [BLE] FF05 - Reinicio programado en 2 segundos para aplicar configuración de red
🔄 [LOOP] Ejecutando reinicio programado para aplicar configuración de red...
```

### Ejemplo 5: Lectura de Códigos con EOT

**Log de App:**
```
[BLE-A2] FF0A Fetching codes page: 0
[BLE-A2] FF0A - Subscribing to notifications...
[BLE-A2] FF0A - Writing page request: {"page":0}
[BLE-A2] FF0A NOTIFY chunk received: 183 bytes
[BLE-A2] FF0A Total accumulated: 183 bytes
[BLE-A2] FF0A EOT byte detected! Transmission complete.
[BLE-A2] FF0A RAW JSON: {"n":3,"m":50,"p":0,"ps":3,"tp":1,"more":false,"c":[{"i":0,"t":"PIN","v":"123456","k":1,"r":1},{"i":1,"t":"PIN","v":"654321","k":2,"r":2},{"i":2,"t":"PIN","v":"344654","k":1,"r":1}]}
[BLE-A2] FF0A PARSED OK - count: 3 codes: 3 has_more: false
```

---

## Troubleshooting

### Error 201 (GATT_WRITE_NOT_PERMITTED)

**Causa:** La App usa `writeWithoutResponse` pero la característica no tenía `WRITE_NR`.

**Solución aplicada:** Todas las características de escritura ahora tienen:
```cpp
NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
```

**Acción en App:** Limpiar caché BLE en Android:
1. Settings → Bluetooth → Olvidar dispositivo
2. Reconectar

### Timeout en FF04 (AddCode)

**Causa:** EEPROM.commit() bloqueaba el callback BLE.

**Solución aplicada:** Guardado diferido al loop():
```cpp
// En callback: solo añadir a RAM y responder
pendingCode.pending = true;
pCharacteristic->notify();

// En loop(): guardar en EEPROM
if (pendingCode.pending) {
  saveStoredCodes();
  pendingCode.pending = false;
}
```

### JSON Truncado en FF09/FF0A

**Causa:** MTU limitado a ~252 bytes.

**Solución aplicada:**
1. JSON compacto con nombres cortos
2. Paginación (3 códigos por página)
3. Marcador EOT (0x04) para detectar fin

### Configuración de Red No Persiste

**Causa:** `saveConfiguration()` usaba variables globales, no `config.*`.

**Solución aplicada:** Actualizar ambas:
```cpp
// Variables globales (para saveConfiguration)
useDhcp = newDhcp;
staticIP = IPAddress(value[1], value[2], value[3], value[4]);
staticGateway = IPAddress(...);

// config.* (para consistencia)
config.useDhcp = newDhcp;
memcpy(config.ip, ...);
```

---

## Resumen de UUIDs

```typescript
const BLE_SERVICE = '0000FF00-0000-1000-8000-00805F9B34FB';

const CHAR_AUTH      = '0000FF01-0000-1000-8000-00805F9B34FB';
const CHAR_RELAY     = '0000FF02-0000-1000-8000-00805F9B34FB';
const CHAR_MODE      = '0000FF03-0000-1000-8000-00805F9B34FB';
const CHAR_ADDCODE   = '0000FF04-0000-1000-8000-00805F9B34FB';
const CHAR_NETWORK   = '0000FF05-0000-1000-8000-00805F9B34FB';
const CHAR_RELAYTIME = '0000FF06-0000-1000-8000-00805F9B34FB';
const CHAR_STATUS    = '0000FF07-0000-1000-8000-00805F9B34FB';
const CHAR_DEVINFO   = '0000FF08-0000-1000-8000-00805F9B34FB';
const CHAR_FULLINFO  = '0000FF09-0000-1000-8000-00805F9B34FB';
const CHAR_CODES     = '0000FF0A-0000-1000-8000-00805F9B34FB';
const CHAR_CHALLENGE = '0000FF0B-0000-1000-8000-00805F9B34FB';
```

---

**Estado:** ✅ DOCUMENTACIÓN COMPLETA - FIRMWARE v4.1.0
