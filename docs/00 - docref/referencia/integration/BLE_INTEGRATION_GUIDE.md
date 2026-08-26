# Guía Completa de Integración BLE - SWATID-A2

> **Versión:** 4.1.0  
> **Última actualización:** 5 Febrero 2026  
> **Audiencia:** Desarrolladores de App Móvil

---

## Índice

1. [Resumen de Arquitectura](#1-resumen-de-arquitectura)
2. [Características BLE](#2-características-ble)
3. [Flujo de Vinculación Superadmin](#3-flujo-de-vinculación-superadmin)
4. [Autenticación Challenge-Response](#4-autenticación-challenge-response)
5. [Autenticación Legada (v4.0)](#5-autenticación-legada-v40)
6. [Mensajería Securizada con Token](#6-mensajería-securizada-con-token)
7. [Gestión de Usuarios](#7-gestión-de-usuarios)
8. [Desvinculación](#8-desvinculación)
9. [Códigos de Error](#9-códigos-de-error)
10. [Implementación Completa (TypeScript)](#10-implementación-completa-typescript)
11. [Mejoras Requeridas en la App](#11-mejoras-requeridas-en-la-app)

---

## 1. Resumen de Arquitectura

### Modelo de Seguridad

```
┌─────────────────────────────────────────────────────────────────────┐
│                         DISPOSITIVO SWATID-A2                        │
├─────────────────────────────────────────────────────────────────────┤
│  SUPERADMIN (Slot 0)          │  USUARIOS (Slots 1-5)               │
│  - Clave 64 bytes             │  - Clave 64 bytes cada uno          │
│  - Permisos completos (0xFF)  │  - Permisos configurables           │
│  - Puede gestionar usuarios   │  - No pueden modificar otros        │
├─────────────────────────────────────────────────────────────────────┤
│  DEVICE MASTER KEY (32 bytes) - Para derivación HKDF                │
└─────────────────────────────────────────────────────────────────────┘
```

### Flujo General

```
┌──────────┐      BLE       ┌─────────────┐       MQTT        ┌──────────┐
│   APP    │ ◄────────────► │  DISPOSITIVO │ ◄───────────────► │ BACKEND  │
└──────────┘                └─────────────┘                    └──────────┘
     │                            │                                  │
     │  1. Escaneo BLE            │                                  │
     │  2. Conexión               │                                  │
     │  3. Challenge-Response     │                                  │
     │  4. Operaciones con Token  │                                  │
     │                            │                                  │
     │                            │  Eventos BLE publicados          │
     │                            │───────────────────────────────►  │
     │                            │                                  │
     │                            │  Comandos de gestión             │
     │                            │ ◄───────────────────────────────│
```

---

## 2. Características BLE

### Servicio Principal

**UUID:** `0000FF00-0000-1000-8000-00805F9B34FB`

### Tabla de Características

| UUID | Nombre | Propiedades | Requiere Auth | Descripción |
|------|--------|-------------|---------------|-------------|
| FF01 | Auth | Write, Notify | No | Autenticación |
| FF02 | Relay | Write, Notify | Sí | Control de relés |
| FF03 | Mode | Write, Notify | Sí | Cambio de modo |
| FF04 | AddCode | Write, Notify | Sí + Permiso | Añadir códigos |
| FF05 | Network | Read, Write, Notify | Sí | Config red |
| FF06 | Unbind | Write, Notify | Sí + Admin | Desvincular |
| FF07 | Codes | Read, Notify | Sí | Listar códigos |
| FF08 | DevInfo | Read | No | Info dispositivo (pública) |
| FF09 | FullInfo | Read, Notify | Sí | Info completa |
| FF0A | Config | Read, Notify | Sí | Configuración completa |
| **FF0B** | **Challenge** | **Read** | No | **Challenge para auth v4.1** |

### Permisos de Usuario

```typescript
enum BLEPermissions {
  RELAY_CONTROL = 0x01,    // Control de relés
  MODE_CHANGE = 0x02,      // Cambio de modo
  ADD_CODES = 0x04,        // Añadir códigos
  NETWORK_CONFIG = 0x08,   // Configuración de red
  ADMIN = 0xFF             // Todos los permisos (Superadmin)
}
```

---

## 3. Flujo de Vinculación Superadmin

### Prerequisitos

- Dispositivo sin Superadmin configurado
- App con generador de claves de 64 bytes

### Diagrama de Flujo

```
┌────────────────┐                              ┌─────────────────┐
│      APP       │                              │   DISPOSITIVO    │
└───────┬────────┘                              └────────┬────────┘
        │                                                │
        │  1. READ FF08 (Info pública)                  │
        │ ─────────────────────────────────────────────►│
        │                                                │
        │  {"type":"A2","serial":"...",                 │
        │   "has_superadmin":false}                      │
        │ ◄─────────────────────────────────────────────│
        │                                                │
        │  2. Generar clave (64 bytes random)           │
        │  [Guardar en almacenamiento seguro]           │
        │                                                │
        │  3. WRITE FF01 (64 bytes = clave)             │
        │ ─────────────────────────────────────────────►│
        │                                                │
        │  Dispositivo detecta: sin superadmin          │
        │  → Registra clave como SUPERADMIN             │
        │  → Genera token de sesión                      │
        │                                                │
        │  "OK:SUPERADMIN_BOUND:TOKEN:XXXXXXXX"         │
        │ ◄─────────────────────────────────────────────│
        │                                                │
        │  4. Guardar token para operaciones            │
        │                                                │
```

### Implementación App

```typescript
interface BindingResult {
  success: boolean;
  userType: 'SUPERADMIN' | 'USER' | null;
  token: Uint8Array | null;
  key: Uint8Array | null;
}

async function bindAsSuperadmin(deviceId: string): Promise<BindingResult> {
  // 1. Verificar que no hay superadmin
  const deviceInfo = await readDeviceInfo(deviceId);
  
  if (deviceInfo.has_superadmin) {
    return { success: false, userType: null, token: null, key: null };
  }
  
  // 2. Generar clave aleatoria de 64 bytes
  const key = crypto.getRandomValues(new Uint8Array(64));
  
  // 3. Configurar notificaciones y enviar clave
  await BLE.startNotifications(deviceId, 'FF01');
  await BLE.write(deviceId, 'FF01', key);
  
  // 4. Esperar respuesta
  const response = await waitForNotification(deviceId, 'FF01', 5000);
  const responseStr = new TextDecoder().decode(response);
  
  if (responseStr.startsWith('OK:SUPERADMIN_BOUND:TOKEN:')) {
    const tokenHex = responseStr.substring(26);
    const token = hexToBytes(tokenHex);
    
    // 5. Guardar clave de forma segura
    await SecureStorage.set(`key_${deviceId}`, bytesToHex(key));
    
    return {
      success: true,
      userType: 'SUPERADMIN',
      token: token,
      key: key
    };
  }
  
  return { success: false, userType: null, token: null, key: null };
}
```

### Respuestas Posibles

| Respuesta | Significado |
|-----------|-------------|
| `OK:SUPERADMIN_BOUND:TOKEN:XXXXXXXX` | Vinculado como Superadmin |
| `ERROR:SUPERADMIN_EXISTS` | Ya hay un Superadmin |
| `ERROR:INVALID_KEY_LENGTH` | Clave no tiene 64 bytes |

---

## 4. Autenticación Challenge-Response (v4.1)

### ¿Por qué Challenge-Response?

**Problema v4.0:** La clave de 64 bytes viaja en claro por BLE, vulnerable a sniffing.

**Solución v4.1:** Solo viaja un hash SHA256, imposible de revertir.

### Diagrama de Flujo

```
┌────────────────┐                              ┌─────────────────┐
│      APP       │                              │   DISPOSITIVO    │
└───────┬────────┘                              └────────┬────────┘
        │                                                │
        │  1. READ FF0B (Solicitar Challenge)           │
        │ ─────────────────────────────────────────────►│
        │                                                │
        │  Dispositivo genera: nonce = random(16 bytes) │
        │  Guarda nonce + timestamp (30s timeout)       │
        │                                                │
        │  nonce (16 bytes)                             │
        │ ◄─────────────────────────────────────────────│
        │                                                │
        │  2. Calcular response:                        │
        │     response = SHA256(key + nonce)            │
        │     (key = clave guardada de 64 bytes)        │
        │                                                │
        │  3. WRITE FF01 (response = 32 bytes)          │
        │ ─────────────────────────────────────────────►│
        │                                                │
        │  Dispositivo verifica:                        │
        │  for cada clave_guardada:                     │
        │    expected = SHA256(clave + nonce)           │
        │    if response == expected → MATCH            │
        │                                                │
        │  Si match: genera token = random(8 bytes)     │
        │                                                │
        │  "OK:TOKEN:XXXXXXXX" (16 hex chars)           │
        │ ◄─────────────────────────────────────────────│
        │                                                │
        │  4. Guardar token para operaciones            │
        │                                                │
```

### Implementación App

```typescript
class SecureBLEAuthenticator {
  private sessionToken: Uint8Array | null = null;
  private deviceId: string;
  
  constructor(deviceId: string) {
    this.deviceId = deviceId;
  }
  
  /**
   * Autenticación Challenge-Response (v4.1)
   */
  async authenticateSecure(userKey: Uint8Array): Promise<AuthResult> {
    // 1. Obtener challenge (nonce de 16 bytes)
    const nonce = await BLE.read(this.deviceId, 'FF0B');
    console.log('[AUTH] Challenge recibido:', bytesToHex(nonce));
    
    if (nonce.length !== 16) {
      throw new Error('Challenge inválido');
    }
    
    // 2. Calcular response = SHA256(key || nonce)
    const dataToHash = new Uint8Array([...userKey, ...nonce]);
    const hashBuffer = await crypto.subtle.digest('SHA-256', dataToHash);
    const response = new Uint8Array(hashBuffer);
    console.log('[AUTH] Response calculado:', bytesToHex(response));
    
    // 3. Configurar notificaciones antes de escribir
    await BLE.startNotifications(this.deviceId, 'FF01');
    
    // 4. Enviar response (32 bytes)
    await BLE.write(this.deviceId, 'FF01', response);
    
    // 5. Esperar resultado
    const result = await this.waitForAuthResponse(5000);
    
    return result;
  }
  
  private async waitForAuthResponse(timeout: number): Promise<AuthResult> {
    return new Promise((resolve, reject) => {
      const timeoutId = setTimeout(() => {
        reject(new Error('Auth timeout'));
      }, timeout);
      
      BLE.onNotification(this.deviceId, 'FF01', (data) => {
        clearTimeout(timeoutId);
        const response = new TextDecoder().decode(data);
        
        if (response.startsWith('OK:TOKEN:')) {
          // Extraer token (8 bytes en hex = 16 caracteres)
          const tokenHex = response.substring(9, 25);
          this.sessionToken = hexToBytes(tokenHex);
          
          // Extraer tipo de usuario y permisos si están
          const parts = response.split(':');
          const userType = parts[3] || 'USER';
          const permissions = parts[4] ? parseInt(parts[4], 16) : 0xFF;
          
          resolve({
            success: true,
            token: this.sessionToken,
            userType: userType,
            permissions: permissions
          });
        } else {
          resolve({
            success: false,
            error: response
          });
        }
      });
    });
  }
  
  /**
   * Obtener token actual de sesión
   */
  getSessionToken(): Uint8Array | null {
    return this.sessionToken;
  }
  
  /**
   * Verificar si la sesión es válida
   */
  isAuthenticated(): boolean {
    return this.sessionToken !== null;
  }
}

// Interfaces
interface AuthResult {
  success: boolean;
  token?: Uint8Array;
  userType?: string;
  permissions?: number;
  error?: string;
}
```

### Timeouts Importantes

| Timeout | Valor | Acción al Expirar |
|---------|-------|-------------------|
| Challenge | 30s | Nonce invalidado, debe solicitar nuevo |
| Sesión | 5 min | Token invalidado, requiere re-auth |
| Conexión | N/A | Token invalidado al desconectar |

---

## 5. Autenticación Legada (v4.0)

Para compatibilidad con APPs antiguas, el método de clave directa sigue funcionando:

```typescript
/**
 * Autenticación legada (envía clave directamente)
 * ⚠️ MENOS SEGURO - Solo usar si Challenge-Response falla
 */
async function authenticateLegacy(
  deviceId: string, 
  userKey: Uint8Array
): Promise<AuthResult> {
  // Verificar longitud de clave
  if (userKey.length !== 64) {
    throw new Error('Key must be 64 bytes');
  }
  
  await BLE.startNotifications(deviceId, 'FF01');
  await BLE.write(deviceId, 'FF01', userKey);
  
  const response = await waitForNotification(deviceId, 'FF01', 5000);
  const responseStr = new TextDecoder().decode(response);
  
  // Respuesta: "OK:AUTHENTICATED:TOKEN:XXXXXXXX"
  if (responseStr.startsWith('OK:AUTHENTICATED:TOKEN:')) {
    const tokenHex = responseStr.substring(23, 39);
    return {
      success: true,
      token: hexToBytes(tokenHex)
    };
  }
  
  return { success: false, error: responseStr };
}
```

### Detección Automática por Longitud

| Longitud | Método |
|----------|--------|
| 32 bytes | Challenge-Response (v4.1) |
| 64 bytes | Clave directa (v4.0 legado) |

---

## 6. Mensajería Securizada con Token

### Concepto

Tras autenticación exitosa, todas las operaciones sensibles deben incluir el token de sesión.

### Estructura de Mensaje con Token

```
┌─────────────────────────────────────────────────────┐
│  Byte 0-7: Token de sesión (8 bytes)                │
│  Byte 8+:  Payload del comando                      │
└─────────────────────────────────────────────────────┘
```

### FF02 - Control de Relés

```typescript
async function activateRelay(
  deviceId: string, 
  token: Uint8Array, 
  relay: number
): Promise<boolean> {
  if (token.length !== 8) {
    throw new Error('Invalid token');
  }
  if (relay < 1 || relay > 2) {
    throw new Error('Relay must be 1 or 2');
  }
  
  // Construir comando: token (8) + relay (1)
  const command = new Uint8Array([...token, relay]);
  
  await BLE.startNotifications(deviceId, 'FF02');
  await BLE.write(deviceId, 'FF02', command);
  
  const response = await waitForNotification(deviceId, 'FF02', 3000);
  const responseStr = new TextDecoder().decode(response);
  
  return responseStr.startsWith('OK');
}
```

### FF03 - Cambio de Modo

```typescript
async function setMode(
  deviceId: string,
  token: Uint8Array,
  mode: 'normal' | 'inverse' | 'turnstile'
): Promise<boolean> {
  const modeMap = {
    'normal': 0,
    'inverse': 1,
    'turnstile': 2
  };
  
  const command = new Uint8Array([...token, modeMap[mode]]);
  
  await BLE.startNotifications(deviceId, 'FF03');
  await BLE.write(deviceId, 'FF03', command);
  
  const response = await waitForNotification(deviceId, 'FF03', 3000);
  return new TextDecoder().decode(response).startsWith('OK');
}
```

### FF04 - Añadir Código (PIN/TAG)

```typescript
interface CodeParams {
  type: 'PIN' | 'TAG';
  value: string;
  keyboard: 0 | 1 | 2;  // 0=ambos, 1=teclado1, 2=teclado2
  relay: 1 | 2;
}

async function addCode(
  deviceId: string,
  token: Uint8Array,
  params: CodeParams
): Promise<AddCodeResult> {
  // Validaciones
  if (params.value.length < 1 || params.value.length > 16) {
    throw new Error('Code must be 1-16 characters');
  }
  
  // Formato: token(8) + type(1) + keyboard(1) + relay(1) + code(n)
  const typeCode = params.type === 'PIN' ? 0 : 1;
  const codeBytes = new TextEncoder().encode(params.value);
  
  const command = new Uint8Array([
    ...token,
    typeCode,
    params.keyboard,
    params.relay,
    ...codeBytes
  ]);
  
  await BLE.startNotifications(deviceId, 'FF04');
  await BLE.write(deviceId, 'FF04', command);
  
  const response = await waitForNotification(deviceId, 'FF04', 5000);
  const responseStr = new TextDecoder().decode(response);
  
  if (responseStr.startsWith('OK:CODE_ADDED:')) {
    const totalCodes = parseInt(responseStr.split(':')[2]);
    return { success: true, totalCodes };
  }
  
  return { success: false, error: responseStr };
}
```

### FF05 - Configuración de Red

```typescript
interface NetworkConfig {
  dhcp: boolean;
  ip?: string;
  gateway?: string;
  subnet?: string;
  dns?: string;
}

async function setNetworkConfig(
  deviceId: string,
  token: Uint8Array,
  config: NetworkConfig
): Promise<boolean> {
  // Formato JSON con token
  const configJson = JSON.stringify({
    token: bytesToHex(token),
    dhcp: config.dhcp,
    ip: config.ip,
    gateway: config.gateway,
    subnet: config.subnet,
    dns: config.dns
  });
  
  const command = new TextEncoder().encode(configJson);
  
  await BLE.startNotifications(deviceId, 'FF05');
  await BLE.write(deviceId, 'FF05', command);
  
  const response = await waitForNotification(deviceId, 'FF05', 5000);
  return new TextDecoder().decode(response).startsWith('OK');
}
```

---

## 7. Gestión de Usuarios

### 7.1 Añadir Usuario (Solo Superadmin)

El Superadmin puede añadir hasta 5 usuarios adicionales (slots 1-5).

```typescript
interface UserParams {
  slot: 1 | 2 | 3 | 4 | 5;
  key: Uint8Array;  // 64 bytes
  name: string;
  permissions: number;
}

async function addUser(
  deviceId: string,
  superadminToken: Uint8Array,
  params: UserParams
): Promise<AddUserResult> {
  // Validaciones
  if (params.key.length !== 64) {
    throw new Error('User key must be 64 bytes');
  }
  if (params.name.length > 31) {
    throw new Error('Name max 31 characters');
  }
  
  // Formato JSON
  const userJson = JSON.stringify({
    token: bytesToHex(superadminToken),
    action: 'add_user',
    slot: params.slot,
    key: bytesToHex(params.key),
    name: params.name,
    permissions: params.permissions
  });
  
  await BLE.startNotifications(deviceId, 'FF06');
  await BLE.write(deviceId, 'FF06', new TextEncoder().encode(userJson));
  
  const response = await waitForNotification(deviceId, 'FF06', 5000);
  const responseStr = new TextDecoder().decode(response);
  
  if (responseStr.startsWith('OK:USER_ADDED')) {
    return { success: true, slot: params.slot };
  }
  
  return { success: false, error: responseStr };
}
```

### 7.2 Añadir Usuario con Clave Derivada (Recomendado)

Para mayor seguridad, usar derivación HKDF:

```typescript
async function addUserDerived(
  deviceId: string,
  superadminToken: Uint8Array,
  deviceMasterKey: Uint8Array,
  serial: string,
  params: {
    slot: 1 | 2 | 3 | 4 | 5;
    userId: string;
    name: string;
    permissions: number;
  }
): Promise<{ success: boolean; userKey?: Uint8Array; error?: string }> {
  
  // 1. Derivar clave usando HKDF
  const userKey = await deriveUserKey(deviceMasterKey, serial, params.userId);
  
  // 2. Enviar comando con clave derivada
  const userJson = JSON.stringify({
    token: bytesToHex(superadminToken),
    action: 'add_user',
    slot: params.slot,
    key: bytesToHex(userKey),
    name: params.name,
    permissions: params.permissions,
    user_id: params.userId  // Incluir para referencia
  });
  
  await BLE.startNotifications(deviceId, 'FF06');
  await BLE.write(deviceId, 'FF06', new TextEncoder().encode(userJson));
  
  const response = await waitForNotification(deviceId, 'FF06', 5000);
  const responseStr = new TextDecoder().decode(response);
  
  if (responseStr.startsWith('OK:USER_ADDED')) {
    return { success: true, userKey: userKey };
  }
  
  return { success: false, error: responseStr };
}

// Función de derivación HKDF
async function deriveUserKey(
  masterKey: Uint8Array,
  serial: string,
  userId: string
): Promise<Uint8Array> {
  const encoder = new TextEncoder();
  const salt = encoder.encode(serial);
  const info = encoder.encode(`user_key:${userId}`);
  
  const keyMaterial = await crypto.subtle.importKey(
    'raw', masterKey, { name: 'HKDF' }, false, ['deriveBits']
  );
  
  const derivedBits = await crypto.subtle.deriveBits(
    { name: 'HKDF', hash: 'SHA-256', salt, info },
    keyMaterial,
    64 * 8
  );
  
  return new Uint8Array(derivedBits);
}
```

### 7.3 Listar Usuarios

```typescript
interface UserInfo {
  slot: number;
  enabled: boolean;
  name: string;
  permissions: number;
  user_id?: string;
}

async function listUsers(
  deviceId: string,
  token: Uint8Array
): Promise<UserInfo[]> {
  const command = JSON.stringify({
    token: bytesToHex(token),
    action: 'list_users'
  });
  
  await BLE.startNotifications(deviceId, 'FF06');
  await BLE.write(deviceId, 'FF06', new TextEncoder().encode(command));
  
  const response = await waitForNotification(deviceId, 'FF06', 5000);
  const responseStr = new TextDecoder().decode(response);
  
  if (responseStr.startsWith('OK:')) {
    const jsonStart = responseStr.indexOf('[');
    const usersJson = responseStr.substring(jsonStart);
    return JSON.parse(usersJson);
  }
  
  throw new Error(responseStr);
}
```

---

## 8. Desvinculación

### 8.1 Desvincular Usuario (Solo Superadmin)

```typescript
async function removeUser(
  deviceId: string,
  superadminToken: Uint8Array,
  slot: 1 | 2 | 3 | 4 | 5
): Promise<boolean> {
  const command = JSON.stringify({
    token: bytesToHex(superadminToken),
    action: 'remove_user',
    slot: slot
  });
  
  await BLE.startNotifications(deviceId, 'FF06');
  await BLE.write(deviceId, 'FF06', new TextEncoder().encode(command));
  
  const response = await waitForNotification(deviceId, 'FF06', 5000);
  const responseStr = new TextDecoder().decode(response);
  
  // Se publica evento MQTT: USER_UNBOUND
  return responseStr.startsWith('OK:USER_REMOVED');
}
```

### 8.2 Desvincular Superadmin (Auto-desvinculación)

⚠️ **PELIGRO:** Esta operación elimina TODOS los usuarios y el superadmin.

```typescript
async function unbindSuperadmin(
  deviceId: string,
  superadminToken: Uint8Array
): Promise<boolean> {
  // Confirmación doble en la App antes de llamar
  const command = JSON.stringify({
    token: bytesToHex(superadminToken),
    action: 'unbind_superadmin',
    confirm: 'YES_UNBIND_ALL'  // Confirmación requerida
  });
  
  await BLE.startNotifications(deviceId, 'FF06');
  await BLE.write(deviceId, 'FF06', new TextEncoder().encode(command));
  
  const response = await waitForNotification(deviceId, 'FF06', 10000);
  const responseStr = new TextDecoder().decode(response);
  
  // Se publican eventos MQTT: SUPERADMIN_UNBOUND + USER_UNBOUND para cada usuario
  if (responseStr.startsWith('OK:SUPERADMIN_UNBOUND')) {
    // Limpiar almacenamiento local
    await SecureStorage.remove(`key_${deviceId}`);
    return true;
  }
  
  return false;
}
```

### 8.3 Eventos MQTT de Desvinculación

Al desvincular, el dispositivo publica eventos al broker MQTT:

**Desvincular Usuario:**
```json
{
  "event": "USER_UNBOUND",
  "source": "BLE",
  "device": "SWATID_D8F7B4BF1388",
  "slot": 2,
  "name": "Juan García",
  "unbound_by": "SUPERADMIN",
  "timestamp": "2026-02-05T12:30:00Z"
}
```

**Desvincular Superadmin:**
```json
{
  "event": "SUPERADMIN_UNBOUND",
  "source": "BLE",
  "device": "SWATID_D8F7B4BF1388",
  "users_removed": 3,
  "timestamp": "2026-02-05T12:30:00Z"
}
```

---

## 9. Códigos de Error

### Errores de Autenticación (FF01)

| Código | Descripción | Acción Recomendada |
|--------|-------------|-------------------|
| `ERROR:NOT_AUTHENTICATED` | Sesión no válida | Re-autenticar |
| `ERROR:INVALID_RESPONSE` | Challenge-response falló | Verificar clave |
| `ERROR:INVALID_KEY_LENGTH` | Clave no es 64 bytes | Verificar formato |
| `ERROR:SUPERADMIN_EXISTS` | Ya hay superadmin | Usar vinculación de usuario |
| `ERROR:CHALLENGE_EXPIRED` | Nonce expirado (>30s) | Solicitar nuevo challenge |
| `ERROR:NO_CHALLENGE` | No se solicitó challenge | Leer FF0B primero |
| `ERROR:USER_NOT_REGISTERED` | Usuario no vinculado | Contactar superadmin |

### Errores de Operación

| Código | Característica | Descripción |
|--------|----------------|-------------|
| `ERROR:NO_PERMISSION` | FF02-FF06 | Sin permisos suficientes |
| `ERROR:INVALID_TOKEN` | Cualquiera | Token de sesión inválido |
| `ERROR:SESSION_EXPIRED` | Cualquiera | Sesión expirada (5 min) |
| `ERROR:INVALID_SLOT` | FF06 | Slot fuera de rango (1-5) |
| `ERROR:SLOT_OCCUPIED` | FF06 | Slot ya tiene usuario |
| `ERROR:SLOT_EMPTY` | FF06 | Slot vacío al intentar eliminar |
| `ERROR:CODE_EXISTS_OR_FULL` | FF04 | Código duplicado o memoria llena |

---

## 10. Implementación Completa (TypeScript)

```typescript
// ═══════════════════════════════════════════════════════════════════
// SWATID-A2 BLE Client - Implementación Completa
// ═══════════════════════════════════════════════════════════════════

import { BleClient } from '@capacitor-community/bluetooth-le';

const SERVICE_UUID = '0000FF00-0000-1000-8000-00805F9B34FB';
const CHAR_AUTH = '0000FF01-0000-1000-8000-00805F9B34FB';
const CHAR_RELAY = '0000FF02-0000-1000-8000-00805F9B34FB';
const CHAR_MODE = '0000FF03-0000-1000-8000-00805F9B34FB';
const CHAR_ADDCODE = '0000FF04-0000-1000-8000-00805F9B34FB';
const CHAR_NETWORK = '0000FF05-0000-1000-8000-00805F9B34FB';
const CHAR_USERS = '0000FF06-0000-1000-8000-00805F9B34FB';
const CHAR_CODES = '0000FF07-0000-1000-8000-00805F9B34FB';
const CHAR_DEVINFO = '0000FF08-0000-1000-8000-00805F9B34FB';
const CHAR_FULLINFO = '0000FF09-0000-1000-8000-00805F9B34FB';
const CHAR_CONFIG = '0000FF0A-0000-1000-8000-00805F9B34FB';
const CHAR_CHALLENGE = '0000FF0B-0000-1000-8000-00805F9B34FB';

export class SWATIDA2Client {
  private deviceId: string;
  private sessionToken: Uint8Array | null = null;
  private userType: string = '';
  private permissions: number = 0;
  
  constructor(deviceId: string) {
    this.deviceId = deviceId;
  }
  
  // ═══════════════════════════════════════════════════════════════
  // CONEXIÓN Y DESCONEXIÓN
  // ═══════════════════════════════════════════════════════════════
  
  async connect(): Promise<void> {
    await BleClient.connect(this.deviceId);
  }
  
  async disconnect(): Promise<void> {
    this.sessionToken = null;
    await BleClient.disconnect(this.deviceId);
  }
  
  // ═══════════════════════════════════════════════════════════════
  // INFORMACIÓN DEL DISPOSITIVO (SIN AUTENTICACIÓN)
  // ═══════════════════════════════════════════════════════════════
  
  async getDeviceInfo(): Promise<DeviceInfo> {
    const data = await BleClient.read(this.deviceId, SERVICE_UUID, CHAR_DEVINFO);
    const json = new TextDecoder().decode(data);
    return JSON.parse(json);
  }
  
  // ═══════════════════════════════════════════════════════════════
  // AUTENTICACIÓN
  // ═══════════════════════════════════════════════════════════════
  
  /**
   * Autenticación segura v4.1 (Challenge-Response)
   */
  async authenticateSecure(userKey: Uint8Array): Promise<AuthResult> {
    // 1. Obtener challenge
    const nonce = await BleClient.read(this.deviceId, SERVICE_UUID, CHAR_CHALLENGE);
    const nonceArray = new Uint8Array(nonce);
    
    if (nonceArray.length !== 16) {
      return { success: false, error: 'Invalid challenge' };
    }
    
    // 2. Calcular SHA256(key + nonce)
    const dataToHash = new Uint8Array([...userKey, ...nonceArray]);
    const hashBuffer = await crypto.subtle.digest('SHA-256', dataToHash);
    const response = new Uint8Array(hashBuffer);
    
    // 3. Configurar notificaciones
    let resolveAuth: (result: AuthResult) => void;
    const authPromise = new Promise<AuthResult>(resolve => { resolveAuth = resolve; });
    
    await BleClient.startNotifications(
      this.deviceId, SERVICE_UUID, CHAR_AUTH,
      (value) => {
        const responseStr = new TextDecoder().decode(value);
        resolveAuth(this.parseAuthResponse(responseStr));
      }
    );
    
    // 4. Enviar response
    await BleClient.write(this.deviceId, SERVICE_UUID, CHAR_AUTH, response);
    
    // 5. Esperar resultado con timeout
    const timeoutPromise = new Promise<AuthResult>(resolve => 
      setTimeout(() => resolve({ success: false, error: 'Timeout' }), 5000)
    );
    
    const result = await Promise.race([authPromise, timeoutPromise]);
    
    await BleClient.stopNotifications(this.deviceId, SERVICE_UUID, CHAR_AUTH);
    
    return result;
  }
  
  /**
   * Vinculación como Superadmin (dispositivo sin superadmin)
   */
  async bindAsSuperadmin(): Promise<BindResult> {
    // Verificar que no hay superadmin
    const info = await this.getDeviceInfo();
    if (info.has_superadmin) {
      return { success: false, error: 'Superadmin already exists' };
    }
    
    // Generar clave aleatoria
    const key = crypto.getRandomValues(new Uint8Array(64));
    
    // Configurar notificaciones
    let resolveAuth: (result: BindResult) => void;
    const authPromise = new Promise<BindResult>(resolve => { resolveAuth = resolve; });
    
    await BleClient.startNotifications(
      this.deviceId, SERVICE_UUID, CHAR_AUTH,
      (value) => {
        const responseStr = new TextDecoder().decode(value);
        if (responseStr.startsWith('OK:SUPERADMIN_BOUND:TOKEN:')) {
          const tokenHex = responseStr.substring(26, 42);
          this.sessionToken = hexToBytes(tokenHex);
          this.userType = 'SUPERADMIN';
          this.permissions = 0xFF;
          resolveAuth({ success: true, key, token: this.sessionToken });
        } else {
          resolveAuth({ success: false, error: responseStr });
        }
      }
    );
    
    // Enviar clave
    await BleClient.write(this.deviceId, SERVICE_UUID, CHAR_AUTH, key);
    
    const result = await Promise.race([
      authPromise,
      new Promise<BindResult>(r => setTimeout(() => r({ success: false, error: 'Timeout' }), 5000))
    ]);
    
    await BleClient.stopNotifications(this.deviceId, SERVICE_UUID, CHAR_AUTH);
    
    return result;
  }
  
  private parseAuthResponse(response: string): AuthResult {
    if (response.startsWith('OK:TOKEN:')) {
      const tokenHex = response.substring(9, 25);
      this.sessionToken = hexToBytes(tokenHex);
      this.userType = 'USER';
      this.permissions = 0x0F;
      return { success: true, token: this.sessionToken, userType: this.userType };
    }
    if (response.startsWith('OK:AUTHENTICATED:TOKEN:')) {
      const tokenHex = response.substring(23, 39);
      this.sessionToken = hexToBytes(tokenHex);
      return { success: true, token: this.sessionToken };
    }
    return { success: false, error: response };
  }
  
  // ═══════════════════════════════════════════════════════════════
  // OPERACIONES CON TOKEN
  // ═══════════════════════════════════════════════════════════════
  
  private requireAuth(): Uint8Array {
    if (!this.sessionToken) {
      throw new Error('Not authenticated');
    }
    return this.sessionToken;
  }
  
  async activateRelay(relay: 1 | 2): Promise<OperationResult> {
    const token = this.requireAuth();
    const command = new Uint8Array([...token, relay]);
    return this.writeWithResponse(CHAR_RELAY, command);
  }
  
  async setMode(mode: 'normal' | 'inverse' | 'turnstile'): Promise<OperationResult> {
    const token = this.requireAuth();
    const modeMap = { 'normal': 0, 'inverse': 1, 'turnstile': 2 };
    const command = new Uint8Array([...token, modeMap[mode]]);
    return this.writeWithResponse(CHAR_MODE, command);
  }
  
  async addCode(params: CodeParams): Promise<OperationResult> {
    const token = this.requireAuth();
    const typeCode = params.type === 'PIN' ? 0 : 1;
    const codeBytes = new TextEncoder().encode(params.value);
    const command = new Uint8Array([
      ...token, typeCode, params.keyboard, params.relay, ...codeBytes
    ]);
    return this.writeWithResponse(CHAR_ADDCODE, command);
  }
  
  // ═══════════════════════════════════════════════════════════════
  // GESTIÓN DE USUARIOS (SOLO SUPERADMIN)
  // ═══════════════════════════════════════════════════════════════
  
  async addUser(params: AddUserParams): Promise<OperationResult> {
    const token = this.requireAuth();
    const command = JSON.stringify({
      token: bytesToHex(token),
      action: 'add_user',
      slot: params.slot,
      key: bytesToHex(params.key),
      name: params.name,
      permissions: params.permissions
    });
    return this.writeWithResponse(CHAR_USERS, new TextEncoder().encode(command));
  }
  
  async removeUser(slot: 1 | 2 | 3 | 4 | 5): Promise<OperationResult> {
    const token = this.requireAuth();
    const command = JSON.stringify({
      token: bytesToHex(token),
      action: 'remove_user',
      slot: slot
    });
    return this.writeWithResponse(CHAR_USERS, new TextEncoder().encode(command));
  }
  
  async unbindSuperadmin(): Promise<OperationResult> {
    const token = this.requireAuth();
    const command = JSON.stringify({
      token: bytesToHex(token),
      action: 'unbind_superadmin',
      confirm: 'YES_UNBIND_ALL'
    });
    const result = await this.writeWithResponse(CHAR_USERS, new TextEncoder().encode(command));
    if (result.success) {
      this.sessionToken = null;
    }
    return result;
  }
  
  // ═══════════════════════════════════════════════════════════════
  // HELPERS
  // ═══════════════════════════════════════════════════════════════
  
  private async writeWithResponse(
    charUuid: string, 
    data: Uint8Array,
    timeout: number = 5000
  ): Promise<OperationResult> {
    let resolveOp: (result: OperationResult) => void;
    const opPromise = new Promise<OperationResult>(resolve => { resolveOp = resolve; });
    
    await BleClient.startNotifications(
      this.deviceId, SERVICE_UUID, charUuid,
      (value) => {
        const response = new TextDecoder().decode(value);
        resolveOp({
          success: response.startsWith('OK'),
          response: response
        });
      }
    );
    
    await BleClient.write(this.deviceId, SERVICE_UUID, charUuid, data);
    
    const result = await Promise.race([
      opPromise,
      new Promise<OperationResult>(r => 
        setTimeout(() => r({ success: false, response: 'Timeout' }), timeout)
      )
    ]);
    
    await BleClient.stopNotifications(this.deviceId, SERVICE_UUID, charUuid);
    return result;
  }
}

// ═══════════════════════════════════════════════════════════════════
// TIPOS E INTERFACES
// ═══════════════════════════════════════════════════════════════════

interface DeviceInfo {
  type: string;
  serial: string;
  firmware: string;
  has_superadmin: boolean;
  users_count: number;
}

interface AuthResult {
  success: boolean;
  token?: Uint8Array;
  userType?: string;
  error?: string;
}

interface BindResult {
  success: boolean;
  key?: Uint8Array;
  token?: Uint8Array;
  error?: string;
}

interface OperationResult {
  success: boolean;
  response: string;
}

interface CodeParams {
  type: 'PIN' | 'TAG';
  value: string;
  keyboard: 0 | 1 | 2;
  relay: 1 | 2;
}

interface AddUserParams {
  slot: 1 | 2 | 3 | 4 | 5;
  key: Uint8Array;
  name: string;
  permissions: number;
}

// ═══════════════════════════════════════════════════════════════════
// UTILIDADES
// ═══════════════════════════════════════════════════════════════════

function bytesToHex(bytes: Uint8Array): string {
  return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('');
}

function hexToBytes(hex: string): Uint8Array {
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < bytes.length; i++) {
    bytes[i] = parseInt(hex.substr(i * 2, 2), 16);
  }
  return bytes;
}

export { SWATIDA2Client, bytesToHex, hexToBytes };
```

---

## 11. Mejoras Requeridas en la App

### 11.1 Cambios Obligatorios (v4.1)

| Prioridad | Mejora | Descripción |
|-----------|--------|-------------|
| **ALTA** | Challenge-Response | Implementar autenticación FF0B + SHA256 |
| **ALTA** | Token en operaciones | Incluir 8 bytes de token en todos los comandos |
| **ALTA** | Manejo de sesión | Detectar expiración y re-autenticar |
| **MEDIA** | Derivación HKDF | Para añadir usuarios de forma segura |
| **MEDIA** | Eventos de desvinculación | Limpiar almacenamiento local |

### 11.2 Flujo de Conexión Recomendado

```typescript
async function connectToDevice(deviceId: string, storedKey: Uint8Array | null) {
  const client = new SWATIDA2Client(deviceId);
  await client.connect();
  
  // 1. Obtener info del dispositivo
  const info = await client.getDeviceInfo();
  
  // 2. Decidir flujo según estado
  if (!info.has_superadmin) {
    // Dispositivo nuevo - ofrecer vinculación
    return { client, needsBinding: true };
  }
  
  if (!storedKey) {
    // Usuario no tiene clave - no puede acceder
    return { client, error: 'No key stored' };
  }
  
  // 3. Autenticar con Challenge-Response
  const authResult = await client.authenticateSecure(storedKey);
  
  if (!authResult.success) {
    // Fallback a legado si falla
    const legacyResult = await client.authenticateLegacy(storedKey);
    if (!legacyResult.success) {
      return { client, error: 'Authentication failed' };
    }
  }
  
  return { client, authenticated: true };
}
```

### 11.3 Almacenamiento Seguro Recomendado

```typescript
// iOS: Usar Keychain
// Android: Usar Encrypted SharedPreferences o Keystore
// Capacitor: @capacitor/preferences con encrypt

import { Preferences } from '@capacitor/preferences';

async function saveDeviceKey(deviceId: string, key: Uint8Array): Promise<void> {
  await Preferences.set({
    key: `swatid_key_${deviceId}`,
    value: bytesToHex(key)
  });
}

async function getDeviceKey(deviceId: string): Promise<Uint8Array | null> {
  const { value } = await Preferences.get({ key: `swatid_key_${deviceId}` });
  return value ? hexToBytes(value) : null;
}

async function removeDeviceKey(deviceId: string): Promise<void> {
  await Preferences.remove({ key: `swatid_key_${deviceId}` });
}
```

---

## Apéndice A: Diagrama de Estados de Sesión

```
                    ┌─────────────────┐
                    │  DESCONECTADO   │
                    └────────┬────────┘
                             │ connect()
                             ▼
                    ┌─────────────────┐
                    │   CONECTADO     │
                    │  (Sin Auth)     │
                    └────────┬────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
              ▼                             ▼
     ┌────────────────┐           ┌────────────────┐
     │ READ FF08      │           │ READ FF0B      │
     │ (Device Info)  │           │ (Challenge)    │
     └────────────────┘           └───────┬────────┘
                                          │
                                          ▼
                                 ┌────────────────┐
                                 │ WRITE FF01     │
                                 │ (Response)     │
                                 └───────┬────────┘
                                         │
                      ┌──────────────────┼──────────────────┐
                      │                  │                  │
                      ▼                  ▼                  ▼
             ┌────────────────┐ ┌────────────────┐ ┌────────────────┐
             │ SUPERADMIN     │ │ AUTHENTICATED  │ │ ERROR          │
             │ BOUND          │ │ (Con Token)    │ │                │
             └───────┬────────┘ └───────┬────────┘ └────────────────┘
                     │                  │
                     ▼                  ▼
            ┌─────────────────────────────────────┐
            │         SESIÓN ACTIVA               │
            │  (Operaciones con token válidas)    │
            └──────────────────┬──────────────────┘
                               │
              ┌────────────────┼────────────────┐
              │                │                │
              ▼                ▼                ▼
     ┌────────────────┐ ┌────────────────┐ ┌────────────────┐
     │ Timeout 5min   │ │ Disconnect     │ │ unbind()       │
     └───────┬────────┘ └───────┬────────┘ └───────┬────────┘
             │                  │                  │
             └──────────────────┴──────────────────┘
                               │
                               ▼
                    ┌─────────────────┐
                    │  SESIÓN         │
                    │  INVALIDADA     │
                    └─────────────────┘
```

---

**Documento creado:** 5 Febrero 2026  
**Autor:** Equipo SWATID  
**Versión:** 1.0
