# Protocolo BLE - SWATID-A2 v4.0

## Índice

1. [Descripción General](#descripción-general)
2. [Configuración BLE](#configuración-ble)
3. [Sistema de Autenticación](#sistema-de-autenticación)
4. [Características BLE](#características-ble)
   - [FF01 - Autenticación](#1-autenticación-ff01)
   - [FF02 - Control de Relés](#2-control-de-relés-ff02)
   - [FF03 - Control de Modo](#3-control-de-modo-ff03)
   - [FF04 - Añadir Códigos](#4-añadir-códigos-ff04)
   - [FF05 - Configuración de Red](#5-configuración-de-red-ff05)
   - [FF06 - Tiempo de Relé](#6-tiempo-de-relé-ff06)
   - [FF07 - Estado del Dispositivo](#7-estado-del-dispositivo-ff07)
   - [FF08 - Info Dispositivo (Provisión)](#8-información-del-dispositivo-ff08---provisión-automática)
   - [FF09 - Info Completa](#9-información-completa-del-dispositivo-ff09)
   - [FF0A - Lista de Códigos](#10-lista-de-códigos-locales-ff0a)
5. [Gestión de Vinculaciones](#gestión-de-vinculaciones)
6. [Almacenamiento EEPROM](#almacenamiento-eeprom)
7. [Ejemplos de Implementación](#ejemplos-de-implementación-app)
8. [Códigos de Error](#códigos-de-error)
9. [Seguridad](#seguridad)

---

## Descripción General

El servidor BLE permite configurar el dispositivo KC868-A2 desde una aplicación móvil sin necesidad de conexión a Internet. Utiliza la librería NimBLE para ESP32 y proporciona un sistema de autenticación basado en claves de 64 bytes.

### Resumen de Características

| Categoría | Característica | UUID | Descripción |
|-----------|----------------|------|-------------|
| **Públicas** | Info Dispositivo | FF08 | Identificación automática (sin auth) |
| | Estado | FF07 | Estado básico de relés y modo |
| **Autenticación** | Login | FF01 | Autenticación con clave 64 bytes |
| **Control** | Relés | FF02 | Activar/desactivar relés 1 y 2 |
| | Modo | FF03 | Cambiar entre normal y torno |
| | Códigos | FF04 | Añadir TAG/PIN locales |
| **Configuración** | Red | FF05 | IP fija o DHCP |
| | Tiempo Relé | FF06 | Duración de activación |
| **Información** | Info Completa | FF09 | IP, red, estado, códigos (auth) |
| | Lista Códigos | FF0A | Todos los códigos locales (auth) |

## Configuración BLE

### Parámetros de Conexión

| Parámetro | Valor |
|-----------|-------|
| **Nombre del dispositivo** | `SWATID_XXXXXXXXXXXX` (Serial completo) |
| **Potencia TX** | ESP_PWR_LVL_P9 (Máxima) |
| **Alcance típico** | ~10 metros |
| **MTU** | 512 bytes |

### UUID del Servicio

```
Service UUID: 0000FF00-0000-1000-8000-00805F9B34FB
```

### UUIDs de Características

| UUID Corto | UUID Completo | Nombre | Propiedades | Auth |
|------------|---------------|--------|-------------|------|
| `FF01` | `0000FF01-0000-1000-8000-00805F9B34FB` | Autenticación | Write, Notify | No |
| `FF02` | `0000FF02-0000-1000-8000-00805F9B34FB` | Control Relés | Write, Notify | Sí |
| `FF03` | `0000FF03-0000-1000-8000-00805F9B34FB` | Control Modo | Read, Write, Notify | Sí |
| `FF04` | `0000FF04-0000-1000-8000-00805F9B34FB` | Añadir Códigos | Write, Notify | Sí |
| `FF05` | `0000FF05-0000-1000-8000-00805F9B34FB` | Config Red | Read, Write, Notify | Sí |
| `FF06` | `0000FF06-0000-1000-8000-00805F9B34FB` | Tiempo Relé | Read, Write, Notify | Sí |
| `FF07` | `0000FF07-0000-1000-8000-00805F9B34FB` | Estado Dispositivo | Read, Notify | No |
| `FF08` | `0000FF08-0000-1000-8000-00805F9B34FB` | **Info Dispositivo** | Read | **No** |
| `FF09` | `0000FF09-0000-1000-8000-00805F9B34FB` | **Info Completa** | Read, **Notify** | **Sí** |
| `FF0A` | `0000FF0A-0000-1000-8000-00805F9B34FB` | **Lista Códigos** | Read, Write, Notify | **Sí** |

**Nota:** Las características FF08, FF09 y FF0A son nuevas en v4.0 para mejorar la provisión automática y el acceso a información detallada del dispositivo.

---

## Sistema de Autenticación

### Jerarquía de Usuarios

| Tipo | Cantidad | Permisos | Descripción |
|------|----------|----------|-------------|
| **Superadmin** | 1 | 0xFF (Todos) | Primer dispositivo vinculado |
| **Usuario** | 5 máx. | Configurables | Añadidos por superadmin |

### Proceso de Vinculación con Provisión Automática

```
┌─────────────────────────────────────────────────────────────────┐
│           FLUJO COMPLETO DE VINCULACIÓN (CON FF08)              │
└─────────────────────────────────────────────────────────────────┘

PASO 0: IDENTIFICACIÓN AUTOMÁTICA DEL DISPOSITIVO
   ┌─────────────┐                    ┌─────────────┐
   │  APP Móvil  │                    │   ESP32     │
   └──────┬──────┘                    └──────┬──────┘
          │                                  │
          │  Escanear BLE (SWATID_*)         │
          │─────────────────────────────────>│
          │                                  │
          │  Conectar                        │
          │─────────────────────────────────>│
          │                                  │
          │  Leer FF08 (Info Dispositivo)    │
          │─────────────────────────────────>│
          │                                  │
          │  JSON: device_type, version,     │
          │        superadmin_registered     │
          │<─────────────────────────────────│
          │                                  │
          │  APP verifica:                   │
          │  - device_type == "SWATID-A2"    │
          │  - protocol_version compatible   │
          │  - Muestra info al usuario       │
          │                                  │

1. PRIMER USO (superadmin_registered == false):
   ┌─────────────┐                    ┌─────────────┐
   │  APP Móvil  │                    │   ESP32     │
   └──────┬──────┘                    └──────┬──────┘
          │                                  │
          │  APP muestra: "Dispositivo       │
          │  SWATID-A2 v4.0.0 sin vincular.  │
          │  ¿Registrarse como superadmin?"  │
          │                                  │
          │  Usuario confirma                │
          │                                  │
          │  Generar clave 64 bytes          │
          │  (SecureRandom/CSPRNG)           │
          │                                  │
          │  Escribir en FF01 (Auth)         │
          │─────────────────────────────────>│
          │                                  │
          │       Registrar como SUPERADMIN  │
          │<─────────────────────────────────│
          │  "OK:SUPERADMIN_REGISTERED"      │
          │                                  │
          │  APP guarda clave localmente     │
          │  (KeyStore/Keychain)             │
          │                                  │

2. USO NORMAL (superadmin_registered == true):
   ┌─────────────┐                    ┌─────────────┐
   │  APP Móvil  │                    │   ESP32     │
   └──────┬──────┘                    └──────┬──────┘
          │                                  │
          │  APP muestra: "SWATID-A2 v4.0.0" │
          │  "Vinculado. Autenticando..."    │
          │                                  │
          │  Recuperar clave guardada        │
          │                                  │
          │  Escribir clave en FF01          │
          │─────────────────────────────────>│
          │                                  │
          │       Verificar clave            │
          │<─────────────────────────────────│
          │  "OK:AUTHENTICATED"              │
          │  (permisos: 0xFF si superadmin)  │
          │                                  │
          │  Enviar comandos a FF02-FF06     │
          │─────────────────────────────────>│
          │                                  │

3. AÑADIR USUARIO (Solo superadmin autenticado):
   ┌─────────────┐                    ┌─────────────┐
   │  APP Móvil  │                    │   ESP32     │
   └──────┬──────┘                    └──────┬──────┘
          │                                  │
          │  (Superadmin autenticado)        │
          │                                  │
          │  Generar clave para usuario      │
          │  (64 bytes, compartir con el     │
          │   nuevo usuario por QR/NFC)      │
          │                                  │
          │  ADD_USER:<slot>:<key_64>        │
          │  Escribir en FF01                │
          │─────────────────────────────────>│
          │                                  │
          │       Guardar en slot 1-5        │
          │<─────────────────────────────────│
          │  "OK:USER_ADDED"                 │
          │                                  │
```

### Formato de Clave

```
Clave de autenticación: 64 bytes (512 bits)
- Puede ser generada por la APP
- Debe ser única por usuario
- Se almacena en EEPROM del ESP32
```

### Permisos

| Bit | Valor | Permiso | Descripción |
|-----|-------|---------|-------------|
| 0 | 0x01 | `BLE_PERM_RELAY_CONTROL` | Control de relés |
| 1 | 0x02 | `BLE_PERM_MODE_CHANGE` | Cambio de modo Normal/Torno |
| 2 | 0x04 | `BLE_PERM_ADD_CODES` | Añadir códigos de acceso |
| 3 | 0x08 | `BLE_PERM_NETWORK_CONFIG` | Configuración de red |
| 7 | 0xFF | `BLE_PERM_ADMIN` | Todos los permisos |

---

## Gestión de los 5 Usuarios Vinculados

### Estructura de Slots de Usuario

El dispositivo reserva **5 espacios (slots)** para usuarios adicionales además del superadmin:

```
┌─────────────────────────────────────────────────────────────────┐
│                    SLOTS DE USUARIOS BLE                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ SUPERADMIN (Slot 0)                                         ││
│  │ ─────────────────────────────────────────────────────────── ││
│  │ Clave: 64 bytes (único, primer vinculado)                   ││
│  │ Permisos: 0xFF (todos)                                      ││
│  │ Puede: Añadir/eliminar usuarios, configurar todo            ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ USUARIO 1 (Slot 1)          │ USUARIO 2 (Slot 2)            ││
│  │ Clave: 64 bytes             │ Clave: 64 bytes               ││
│  │ Permisos: Configurables     │ Permisos: Configurables       ││
│  │ Nombre: hasta 16 chars      │ Nombre: hasta 16 chars        ││
│  └─────────────────────────────┴────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ USUARIO 3 (Slot 3)          │ USUARIO 4 (Slot 4)            ││
│  │ Clave: 64 bytes             │ Clave: 64 bytes               ││
│  │ Permisos: Configurables     │ Permisos: Configurables       ││
│  │ Nombre: hasta 16 chars      │ Nombre: hasta 16 chars        ││
│  └─────────────────────────────┴────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ USUARIO 5 (Slot 5)                                          ││
│  │ Clave: 64 bytes                                             ││
│  │ Permisos: Configurables                                     ││
│  │ Nombre: hasta 16 chars                                      ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Datos por Usuario

| Campo | Tamaño | Descripción |
|-------|--------|-------------|
| `key` | 64 bytes | Clave única de autenticación |
| `enabled` | 1 byte | Estado: 0=deshabilitado, 1=habilitado |
| `permissions` | 1 byte | Bitmask de permisos (ver tabla anterior) |
| `name` | 16 bytes | Nombre identificativo (UTF-8, null-terminated) |

### Ejemplos de Permisos por Rol

| Rol | Permisos | Valor | Capacidades |
|-----|----------|-------|-------------|
| **Administrador** | Todos | `0xFF` | Control total del dispositivo |
| **Operador** | Relés + Modo | `0x03` | Abrir puertas, cambiar modo |
| **Recepción** | Solo Relés | `0x01` | Solo abrir puertas |
| **Técnico** | Red + Relés | `0x09` | Configurar red, probar relés |
| **Seguridad** | Relés + Códigos | `0x05` | Abrir puertas, añadir accesos |

### Comandos de Gestión de Usuarios (FF01)

#### Añadir Usuario con Permisos

**Formato extendido:**
```
ADD_USER:<slot>:<permissions>:<name>:<key_64_bytes>
```

| Campo | Descripción |
|-------|-------------|
| slot | Número de slot (1-5) |
| permissions | Byte de permisos en hexadecimal |
| name | Nombre del usuario (hasta 16 caracteres) |
| key | Clave de 64 bytes |

**Ejemplo - Añadir operador en slot 2:**
```
ADD_USER:2:03:Recepcion:[64_bytes_key]
```

#### Modificar Permisos de Usuario

**Formato:**
```
SET_PERMS:<slot>:<permissions>
```

**Ejemplo - Dar todos los permisos al usuario 1:**
```
SET_PERMS:1:FF
```

#### Modificar Nombre de Usuario

**Formato:**
```
SET_NAME:<slot>:<name>
```

**Ejemplo:**
```
SET_NAME:3:Guardia_Noche
```

#### Listar Usuarios (Lectura FF01)

Al leer la característica FF01 después de autenticarse como superadmin:

```json
{
  "users": [
    {
      "slot": 1,
      "enabled": true,
      "name": "Recepcion",
      "permissions": "0x03"
    },
    {
      "slot": 2,
      "enabled": true,
      "name": "Tecnico",
      "permissions": "0x09"
    },
    {
      "slot": 3,
      "enabled": false,
      "name": "",
      "permissions": "0x00"
    },
    {
      "slot": 4,
      "enabled": false,
      "name": "",
      "permissions": "0x00"
    },
    {
      "slot": 5,
      "enabled": false,
      "name": "",
      "permissions": "0x00"
    }
  ],
  "available_slots": 3
}
```

### Identificación de Acciones por Usuario

Cuando un usuario autenticado ejecuta una acción, el sistema registra:

| Campo | Valor |
|-------|-------|
| `user_type` | `SUPERADMIN` o `USER` |
| `user_slot` | 0 (superadmin) o 1-5 (usuario) |
| `user_name` | Nombre configurado |
| `action` | Acción realizada |
| `timestamp` | Fecha/hora |

**Ejemplo de evento MQTT:**
```json
{
  "event": "BLE_ACTION",
  "user_type": "USER",
  "user_slot": 2,
  "user_name": "Tecnico",
  "action": "RELAY_ACTIVATED",
  "relay": 1,
  "timestamp": "2026-02-05 14:30:00"
}
```

---

## Características BLE (Endpoints)

### 1. Autenticación (FF01)

**UUID:** `0000FF01-0000-1000-8000-00805F9B34FB`

**Propiedades:** Write, Notify

#### Autenticación con Clave

| Campo | Tipo | Tamaño | Descripción |
|-------|------|--------|-------------|
| key | bytes | 64 | Clave de autenticación |

**Enviar:**
```
[64 bytes de clave]
```

**Respuestas:**
| Respuesta | Descripción |
|-----------|-------------|
| `OK:SUPERADMIN_REGISTERED` | Registrado como superadmin (primer uso) |
| `OK:AUTHENTICATED` | Autenticación exitosa |
| `ERROR:INVALID_KEY` | Clave incorrecta |
| `ERROR:INVALID_LENGTH` | Longitud incorrecta |

#### Añadir Usuario (Solo Superadmin)

**Formato:**
```
ADD_USER:<slot>:<key_64_bytes>
```

| Campo | Descripción |
|-------|-------------|
| slot | Número de slot (1-5) |
| key | Clave de 64 bytes del nuevo usuario |

**Respuesta:**
```
OK:USER_ADDED
```

#### Eliminar Usuario (Solo Superadmin)

**Formato:**
```
DEL_USER:<slot>
```

**Respuesta:**
```
OK:USER_DELETED
```

---

### 2. Control de Relés (FF02)

**UUID:** `0000FF02-0000-1000-8000-00805F9B34FB`

**Propiedades:** Write, Notify

**Permiso requerido:** `BLE_PERM_RELAY_CONTROL` (0x01)

#### Formato del Mensaje

| Byte | Campo | Descripción |
|------|-------|-------------|
| 0 | relay_id | ID del relé (1 o 2) |
| 1 | action | Acción a realizar |
| 2-3 | duration_ms | Duración en ms (opcional, big-endian) |

#### Acciones

| Valor | Acción | Descripción |
|-------|--------|-------------|
| 0 | OFF | Desactivar relé inmediatamente |
| 1 | ON | Activar relé por tiempo configurado (o especificado) |
| 2 | PULSE | Igual que ON (mantiene compatibilidad) |

#### Comportamiento de la Duración

El comando BLE funciona de manera **idéntica a la web y MQTT**:

| Caso | Comportamiento |
|------|----------------|
| Solo 2 bytes (relay + action) | Usa la **duración configurada** del dispositivo |
| 4 bytes (con duration_ms) | Usa la duración especificada en el comando |

**Duración configurada:** Se puede consultar/modificar via característica FF06 (Tiempo de Relé) o desde la web.

#### Ejemplos

**Activar relé 1 con duración configurada (ej: 2 segundos):**
```hex
01 01
```
→ Activa el relé 1 por la duración configurada en el dispositivo

**Activar relé 2 con duración configurada:**
```hex
02 01
```

**Desactivar relé 1 inmediatamente:**
```hex
01 00
```

**Activar relé 1 por 3 segundos exactos:**
```hex
01 01 0B B8  (0x0BB8 = 3000ms)
```

**Activar relé 2 por 5 segundos exactos:**
```hex
02 02 13 88  (0x1388 = 5000ms)
```

#### Eventos MQTT Generados

Al activar un relé vía BLE, se publica un evento MQTT:

**Tópico:** `swatidhome/events/[SERIAL]/ble`

```json
{
  "event": "RELAY_ACTIVATED",
  "relay": 1,
  "duration": 2.0,
  "source": "BLE",
  "user": "SUPERADMIN",
  "timestamp": "2026-02-05 14:30:00"
}
```

Al desactivar:
```json
{
  "event": "RELAY_DEACTIVATED",
  "relay": 1,
  "source": "BLE",
  "user": "SUPERADMIN",
  "timestamp": "2026-02-05 14:30:00"
}
```

#### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:RELAY_ON` | Relé activado (action=1) |
| `OK:RELAY_OFF` | Relé desactivado (action=0) |
| `OK:RELAY_PULSE` | Relé activado (action=2) |
| `ERROR:NOT_AUTHORIZED` | Sin permisos |
| `ERROR:INVALID_RELAY` | Relé inválido (debe ser 1 o 2) |

#### Ejemplo de Implementación (APP)

**Android (Kotlin) - Abrir puerta con duración configurada:**
```kotlin
// Comando mínimo: usa duración del dispositivo
val data = byteArrayOf(0x01, 0x01)  // Relé 1, ON
relayCharacteristic.value = data
gatt.writeCharacteristic(relayCharacteristic)
```

**Android (Kotlin) - Abrir puerta con duración específica:**
```kotlin
// Comando con duración: 2500ms
val relay = 1
val action = 1
val durationMs = 2500
val data = byteArrayOf(
    relay.toByte(),
    action.toByte(),
    (durationMs shr 8).toByte(),  // High byte
    (durationMs and 0xFF).toByte() // Low byte
)
relayCharacteristic.value = data
gatt.writeCharacteristic(relayCharacteristic)
```

**iOS (Swift) - Abrir puerta:**
```swift
// Con duración configurada
let data = Data([0x01, 0x01])
peripheral.writeValue(data, for: relayChar, type: .withResponse)

// Con duración específica (3000ms)
let durationMs: UInt16 = 3000
let data = Data([0x01, 0x01, UInt8(durationMs >> 8), UInt8(durationMs & 0xFF)])
peripheral.writeValue(data, for: relayChar, type: .withResponse)
```

---

### 3. Control de Modo (FF03)

**UUID:** `0000FF03-0000-1000-8000-00805F9B34FB`

**Propiedades:** Read, Write, Notify

**Permiso requerido:** `BLE_PERM_MODE_CHANGE` (0x02)

#### Lectura

Devuelve 1 byte:
| Valor | Modo |
|-------|------|
| 0 | Normal |
| 1 | Torno |

#### Escritura

| Byte | Valor | Descripción |
|------|-------|-------------|
| 0 | 0 | Cambiar a modo Normal |
| 0 | 1 | Cambiar a modo Torno |

#### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:MODE_NORMAL` | Cambiado a modo normal |
| `OK:MODE_TURNSTILE` | Cambiado a modo torno |
| `ERROR:NOT_AUTHORIZED` | Sin permisos |

---

### 4. Añadir Códigos (FF04)

**UUID:** `0000FF04-0000-1000-8000-00805F9B34FB`

**Propiedades:** Write, Notify

**Permiso requerido:** `BLE_PERM_ADD_CODES` (0x04)

#### Formato del Mensaje

| Byte | Campo | Descripción |
|------|-------|-------------|
| 0 | type | Tipo de código |
| 1 | keyboard | Teclado asignado |
| 2 | relay | Relé a activar |
| 3+ | code | Código (hasta 16 caracteres) |

#### Tipos de Código

| Valor | Tipo |
|-------|------|
| 0 | PIN |
| 1 | TAG |

#### Teclados

| Valor | Descripción |
|-------|-------------|
| 0 | Ambos teclados |
| 1 | Solo teclado 1 |
| 2 | Solo teclado 2 |

#### Ejemplo

**Añadir PIN "1234" para teclado 1, relé 1:**
```hex
00 01 01 31 32 33 34  (0x31='1', 0x32='2', etc.)
```

**Añadir TAG "ABC123" para ambos teclados, relé 2:**
```hex
01 00 02 41 42 43 31 32 33
```

#### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:CODE_ADDED` | Código añadido correctamente |
| `ERROR:CODE_EXISTS` | El código ya existe |
| `ERROR:INVALID_PARAMS` | Parámetros inválidos |
| `ERROR:NOT_AUTHORIZED` | Sin permisos |

---

### 5. Configuración de Red (FF05)

**UUID:** `0000FF05-0000-1000-8000-00805F9B34FB`

**Propiedades:** Read, Write, Notify

**Permiso requerido:** `BLE_PERM_NETWORK_CONFIG` (0x08)

#### Formato (17 bytes)

| Bytes | Campo | Descripción |
|-------|-------|-------------|
| 0 | dhcp | 1=DHCP, 0=IP Fija |
| 1-4 | ip | Dirección IP |
| 5-8 | gateway | Puerta de enlace |
| 9-12 | subnet | Máscara de subred |
| 13-16 | dns | Servidor DNS |

#### Lectura

Devuelve la configuración actual (17 bytes).

#### Escritura

**Ejemplo - Configurar IP fija 192.168.1.100:**
```hex
00                    // DHCP = No
C0 A8 01 64          // IP = 192.168.1.100
C0 A8 01 01          // Gateway = 192.168.1.1
FF FF FF 00          // Subnet = 255.255.255.0
08 08 08 08          // DNS = 8.8.8.8
```

**Ejemplo - Activar DHCP:**
```hex
01                    // DHCP = Sí
00 00 00 00          // IP (ignorado)
00 00 00 00          // Gateway (ignorado)
00 00 00 00          // Subnet (ignorado)
00 00 00 00          // DNS (ignorado)
```

#### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:NETWORK_CONFIGURED` | Configuración guardada |
| `ERROR:NOT_AUTHORIZED` | Sin permisos |

---

### 6. Tiempo de Relé (FF06)

**UUID:** `0000FF06-0000-1000-8000-00805F9B34FB`

**Propiedades:** Read, Write, Notify

**Permiso requerido:** `BLE_PERM_RELAY_CONTROL` (0x01)

#### Formato

4 bytes (uint32_t little-endian) - Duración en milisegundos

#### Lectura

Devuelve el tiempo actual de apertura de relé.

#### Escritura

**Ejemplo - Configurar 5 segundos:**
```hex
88 13 00 00  (0x1388 = 5000ms)
```

#### Respuestas

| Respuesta | Descripción |
|-----------|-------------|
| `OK:RELAY_TIME_SET` | Tiempo configurado |
| `ERROR:NOT_AUTHORIZED` | Sin permisos |

---

### 7. Estado del Dispositivo (FF07)

**UUID:** `0000FF07-0000-1000-8000-00805F9B34FB`

**Propiedades:** Read, Notify

**Permiso requerido:** Ninguno (solo lectura)

**Importante:** Esta característica ahora tiene callback `onRead` que genera JSON dinámico y envía `notify()` automáticamente.

#### Formato de Respuesta (JSON)

```json
{
  "relay1": false,
  "relay2": false,
  "mode": "normal",
  "auth": true,
  "user": "SUPERADMIN",
  "eth_connected": true,
  "mqtt_connected": false
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| relay1 | bool | Estado relé 1 (true/false) |
| relay2 | bool | Estado relé 2 (true/false) |
| mode | string | "normal" o "torno" |
| auth | bool | Autenticado vía BLE (true/false) |
| user | string | Usuario conectado (vacío si no autenticado) |
| eth_connected | bool | Conexión Ethernet activa |
| mqtt_connected | bool | Conexión MQTT activa |

#### Uso recomendado

```
1. Suscribirse a NOTIFY de FF07
2. Hacer READ de FF07 (esto dispara onRead que envía notify)
3. Recibir JSON actualizado vía NOTIFY
```

---

### 8. Información del Dispositivo (FF08) - Provisión Automática

**UUID:** `0000FF08-0000-1000-8000-00805F9B34FB`

**Propiedades:** Read

**Permiso requerido:** Ninguno (lectura pública sin autenticación)

Esta característica permite a la APP móvil **identificar automáticamente** el tipo de dispositivo, versión de firmware y capacidades **antes de autenticarse**. Es fundamental para:

- Seleccionar automáticamente el protocolo correcto
- Mostrar información del dispositivo al usuario
- Verificar compatibilidad de la APP con el firmware
- Evitar errores de conexión a dispositivos incompatibles

#### Formato de Respuesta (JSON)

```json
{
  "device_type": "SWATID-A2",
  "serial": "SWATID_B4F7D88813BF",
  "firmware_version": "4.0.0",
  "firmware_variant": "BLE",
  "protocol_version": 1,
  "capabilities": {
    "relays": 2,
    "wiegand_inputs": 2,
    "digital_inputs": 2,
    "ble_users": 5
  },
  "superadmin_registered": true
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `device_type` | string | Tipo de dispositivo: `SWATID-A2`, `SWATID-B16`, etc. |
| `serial` | string | Número de serie único del dispositivo |
| `firmware_version` | string | Versión del firmware (ej: `4.0.0`) |
| `firmware_variant` | string | Variante: `BLE`, `EEPROM`, `STANDARD` |
| `protocol_version` | int | Versión del protocolo BLE (para compatibilidad) |
| `capabilities` | object | Capacidades hardware del dispositivo |
| `capabilities.relays` | int | Número de relés disponibles |
| `capabilities.wiegand_inputs` | int | Número de entradas Wiegand |
| `capabilities.digital_inputs` | int | Número de entradas digitales |
| `capabilities.ble_users` | int | Máximo de usuarios BLE (5) |
| `superadmin_registered` | bool | Indica si ya hay superadmin vinculado |

#### Uso en la APP

```
┌─────────────────────────────────────────────────────────────────┐
│             FLUJO DE CONEXIÓN CON PROVISIÓN AUTOMÁTICA          │
└─────────────────────────────────────────────────────────────────┘

   ┌─────────────┐                    ┌─────────────┐
   │  APP Móvil  │                    │   ESP32     │
   └──────┬──────┘                    └──────┬──────┘
          │                                  │
          │  1. Escanear dispositivos BLE    │
          │  (Buscar nombre SWATID_*)        │
          │─────────────────────────────────>│
          │                                  │
          │  2. Conectar al dispositivo      │
          │─────────────────────────────────>│
          │                                  │
          │  3. Leer característica FF08     │
          │     (Info Dispositivo)           │
          │─────────────────────────────────>│
          │                                  │
          │  4. Recibir JSON con:            │
          │     - device_type                │
          │     - firmware_version           │
          │     - capabilities               │
          │<─────────────────────────────────│
          │                                  │
          │  5. APP verifica compatibilidad  │
          │     y muestra info al usuario    │
          │                                  │
          │  6. Si compatible, proceder      │
          │     con autenticación (FF01)     │
          │─────────────────────────────────>│
          │                                  │
```

#### Ejemplo de Implementación en APP

**Android (Kotlin):**
```kotlin
// Al conectar, primero leer información del dispositivo
fun onServicesDiscovered(gatt: BluetoothGatt) {
    val infoChar = gatt.getService(SERVICE_UUID)
        ?.getCharacteristic(CHAR_DEVICE_INFO_UUID)
    
    if (infoChar != null) {
        gatt.readCharacteristic(infoChar)
    }
}

// Procesar respuesta
fun onCharacteristicRead(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
    if (characteristic.uuid == CHAR_DEVICE_INFO_UUID) {
        val json = String(characteristic.value)
        val deviceInfo = JSONObject(json)
        
        val deviceType = deviceInfo.getString("device_type")
        val firmwareVersion = deviceInfo.getString("firmware_version")
        val protocolVersion = deviceInfo.getInt("protocol_version")
        val superadminRegistered = deviceInfo.getBoolean("superadmin_registered")
        
        // Verificar compatibilidad
        if (deviceType == "SWATID-A2" && protocolVersion >= 1) {
            // Dispositivo compatible, mostrar en UI
            showDeviceInfo(deviceInfo)
            
            // Si no hay superadmin, mostrar opción de registrarse
            if (!superadminRegistered) {
                showRegisterAsSuperadminOption()
            }
        } else {
            showIncompatibleDeviceError()
        }
    }
}
```

**iOS (Swift):**
```swift
// Leer información del dispositivo
func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
    if let infoChar = service.characteristics?.first(where: { $0.uuid == CHAR_DEVICE_INFO_UUID }) {
        peripheral.readValue(for: infoChar)
    }
}

// Procesar respuesta
func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
    guard characteristic.uuid == CHAR_DEVICE_INFO_UUID,
          let data = characteristic.value,
          let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
    
    let deviceType = json["device_type"] as? String ?? ""
    let firmwareVersion = json["firmware_version"] as? String ?? ""
    let protocolVersion = json["protocol_version"] as? Int ?? 0
    let superadminRegistered = json["superadmin_registered"] as? Bool ?? false
    
    // Verificar y proceder
    if deviceType == "SWATID-A2", protocolVersion >= 1 {
        updateUI(with: json)
        if !superadminRegistered {
            showRegisterPrompt()
        }
    }
}
```

---

### 9. Información Completa del Dispositivo (FF09)

**UUID:** `0000FF09-0000-1000-8000-00805F9B34FB`

**Propiedades:** Read

**Permiso requerido:** Autenticación obligatoria

> **NOTA:** Esta característica usa ArduinoJson para garantizar JSON válido. Todos los campos de red se incluyen siempre (tanto en DHCP como IP fija).

#### Formato de Respuesta (JSON)

```json
{
  "device_type": "SWATID-A2",
  "serial": "SWATID_8813BFB4F7DB",
  "name": "Controladora Principal",
  "firmware": "v4.0.0",
  "network": {
    "ip": "192.168.5.86",
    "gateway": "192.168.5.1",
    "subnet": "255.255.255.0",
    "dns": "192.168.5.1",
    "mac": "88:13:BF:B4:F7:DB",
    "dhcp": true,
    "connected": true
  },
  "status": {
    "relay1": false,
    "relay2": false,
    "relay_duration": 3.0,
    "mode": "normal",
    "local_access_blocked": false,
    "keyboard_reading": true,
    "mqtt_connected": true
  },
  "codes": {
    "local_count": 15,
    "local_max": 100,
    "remote_count": 25,
    "validation_mode": "local_first"
  },
  "uptime_seconds": 86400
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `device_type` | string | Tipo de dispositivo ("SWATID-A2") |
| `serial` | string | Número de serie único |
| `name` | string | Nombre configurado del dispositivo |
| `firmware` | string | Versión del firmware instalado |
| `network.ip` | string | **IP actual asignada** (real) |
| `network.gateway` | string | **Gateway actual** (real) |
| `network.subnet` | string | **Máscara actual** (real) |
| `network.dns` | string | **DNS actual** (real) |
| `network.mac` | string | Dirección MAC Ethernet |
| `network.dhcp` | bool | `true` = DHCP, `false` = IP fija |
| `network.connected` | bool | Estado de conexión Ethernet |
| `status.relay1` | bool | Estado del relé 1 |
| `status.relay2` | bool | Estado del relé 2 |
| `status.relay_duration` | float | Duración de activación en segundos |
| `status.mode` | string | `"normal"` o `"torno"` |
| `status.local_access_blocked` | bool | Si el acceso local está bloqueado |
| `status.keyboard_reading` | bool | Si los teclados están activos |
| `status.mqtt_connected` | bool | Estado de conexión MQTT |
| `codes.local_count` | int | Número de códigos locales almacenados |
| `codes.local_max` | int | Capacidad máxima (100) |
| `codes.remote_count` | int | Número de códigos remotos almacenados |
| `codes.validation_mode` | string | `"local_first"` o `"remote_first"` |
| `uptime_seconds` | int | Segundos desde el último reinicio |

#### Códigos de Error

| Respuesta | Significado |
|-----------|-------------|
| `{"error":"NOT_AUTHORIZED"}` | Usuario no autenticado |
| `{"error":"JSON_OVERFLOW"}` | Error interno al generar JSON |

#### Ejemplo de Uso

**Android (Kotlin):**
```kotlin
fun readFullDeviceInfo(gatt: BluetoothGatt) {
    if (!isAuthenticated) {
        Log.e(TAG, "No autenticado - no se puede leer FF09")
        return
    }
    
    val fullInfoChar = gatt.getService(SERVICE_UUID)
        ?.getCharacteristic(CHAR_FULLINFO_UUID)
    
    if (fullInfoChar != null) {
        gatt.readCharacteristic(fullInfoChar)
    }
}

fun onCharacteristicRead(characteristic: BluetoothGattCharacteristic) {
    if (characteristic.uuid == CHAR_FULLINFO_UUID) {
        val json = JSONObject(String(characteristic.value))
        
        // Información de red
        val network = json.getJSONObject("network")
        val ipAddress = network.getString("ip")
        val isConnected = network.getBoolean("connected")
        
        // Estado operacional
        val status = json.getJSONObject("status")
        val relay1Active = status.getBoolean("relay1")
        val mqttConnected = status.getBoolean("mqtt_connected")
        
        // Información de códigos
        val codes = json.getJSONObject("codes")
        val localCount = codes.getInt("local_count")
        val localMax = codes.getInt("local_max")
        
        updateDetailedUI(json)
    }
}
```

---

### 10. Lista de Códigos Locales (FF0A)

**UUID:** `0000FF0A-0000-1000-8000-00805F9B34FB`

**Propiedades:** Read, Write, Notify

**Permiso requerido:** Autenticación obligatoria

> **NOTA:** Esta característica usa ArduinoJson para garantizar JSON válido. La paginación usa **15 códigos por página** para respetar el MTU BLE.

#### Lectura - Obtener Códigos (Primera Página)

Al leer la característica, se obtienen los primeros 15 códigos:

```json
{
  "count": 45,
  "max": 100,
  "validation_mode": "local_first",
  "page": 0,
  "page_size": 15,
  "total_pages": 3,
  "has_more": true,
  "codes": [
    {
      "id": 0,
      "type": "TAG",
      "value": "12345678",
      "keyboard": 0,
      "relay": 1
    },
    {
      "id": 1,
      "type": "PIN",
      "value": "1234",
      "keyboard": 1,
      "relay": 1
    }
  ]
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `count` | int | **Total** de códigos almacenados |
| `max` | int | Capacidad máxima (100) |
| `validation_mode` | string | Modo de validación: `"local_first"` o `"remote_first"` |
| `page` | int | Número de página actual (0-indexed) |
| `page_size` | int | Códigos por página (15) |
| `total_pages` | int | Total de páginas disponibles |
| `has_more` | bool | `true` si hay más páginas |
| `codes` | array | Array de códigos de esta página |
| `codes[].id` | int | Índice del código (0-99) |
| `codes[].type` | string | Tipo: `"TAG"` (tarjeta) o `"PIN"` (código numérico) |
| `codes[].value` | string | Valor del código |
| `codes[].keyboard` | int | Teclado asignado: `0`=ambos, `1`=teclado 1, `2`=teclado 2 |
| `codes[].relay` | int | Relé a activar: `1` o `2` |

#### Códigos de Error

| Respuesta | Significado |
|-----------|-------------|
| `{"error":"NOT_AUTHORIZED"}` | Usuario no autenticado |
| `{"error":"EMPTY_REQUEST"}` | Solicitud vacía |
| `{"error":"INVALID_JSON"}` | JSON de solicitud mal formado |
| `{"error":"MISSING_PAGE"}` | Falta el campo `page` |
| `{"error":"INVALID_PAGE"}` | Página fuera de rango |
| `{"error":"JSON_OVERFLOW"}` | Error interno al generar JSON |

#### Escritura - Solicitar Página Específica

Para obtener más códigos, escribir el número de página (cada página tiene 15 códigos):

**Formato de solicitud:**
```json
{"page": 1}
```

**Respuesta (página 1 = códigos 15-29):**
```json
{
  "count": 45,
  "page": 1,
  "page_size": 15,
  "total_pages": 3,
  "has_more": true,
  "codes": [
    {
      "id": 40,
      "type": "PIN",
      "value": "5678",
      "keyboard": 0,
      "relay": 1
    },
    {
      "id": 41,
      "type": "TAG",
      "value": "99887766",
      "keyboard": 1,
      "relay": 2
    }
    // ... códigos 40-44 (5 códigos restantes)
  ]
}
```

#### Ejemplo de Implementación

**Android (Kotlin):**
```kotlin
// Leer primera página de códigos
fun readLocalCodes(gatt: BluetoothGatt) {
    val codesChar = gatt.getService(SERVICE_UUID)
        ?.getCharacteristic(CHAR_CODES_UUID)
    
    codesChar?.let { gatt.readCharacteristic(it) }
}

// Solicitar página específica
fun requestCodesPage(gatt: BluetoothGatt, page: Int) {
    val codesChar = gatt.getService(SERVICE_UUID)
        ?.getCharacteristic(CHAR_CODES_UUID)
    
    codesChar?.let {
        val request = """{"page": $page}"""
        it.value = request.toByteArray()
        gatt.writeCharacteristic(it)
    }
}

// Procesar respuesta
fun onCharacteristicRead(characteristic: BluetoothGattCharacteristic) {
    if (characteristic.uuid == CHAR_CODES_UUID) {
        val json = JSONObject(String(characteristic.value))
        
        val totalCount = json.getInt("count")
        val codesArray = json.getJSONArray("codes")
        
        val codesList = mutableListOf<LocalCode>()
        for (i in 0 until codesArray.length()) {
            val codeObj = codesArray.getJSONObject(i)
            codesList.add(LocalCode(
                id = codeObj.getInt("id"),
                type = codeObj.getString("type"),
                value = codeObj.getString("value"),
                keyboard = codeObj.getInt("keyboard"),
                relay = codeObj.getInt("relay")
            ))
        }
        
        // Verificar si hay más páginas
        if (json.optBoolean("more", false)) {
            val nextPage = (json.optInt("page", 0)) + 1
            requestCodesPage(gatt, nextPage)
        }
        
        updateCodesUI(codesList, totalCount)
    }
}
```

**iOS (Swift):**
```swift
// Solicitar página específica
func requestCodesPage(_ page: Int) {
    guard let codesChar = peripheral.services?
        .first(where: { $0.uuid == SERVICE_UUID })?
        .characteristics?
        .first(where: { $0.uuid == CHAR_CODES_UUID }) else { return }
    
    let request = "{\"page\": \(page)}".data(using: .utf8)!
    peripheral.writeValue(request, for: codesChar, type: .withResponse)
}

// Procesar notificación
func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
    guard characteristic.uuid == CHAR_CODES_UUID,
          let data = characteristic.value,
          let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
          let codesArray = json["codes"] as? [[String: Any]] else { return }
    
    var codes: [LocalCode] = []
    for codeDict in codesArray {
        codes.append(LocalCode(
            id: codeDict["id"] as? Int ?? 0,
            type: codeDict["type"] as? String ?? "",
            value: codeDict["value"] as? String ?? "",
            keyboard: codeDict["keyboard"] as? Int ?? 0,
            relay: codeDict["relay"] as? Int ?? 1
        ))
    }
    
    let hasMore = json["more"] as? Bool ?? false
    if hasMore {
        let currentPage = json["page"] as? Int ?? 0
        requestCodesPage(currentPage + 1)
    }
    
    delegate?.didReceiveCodes(codes, totalCount: json["count"] as? Int ?? 0)
}
```

---

## Gestión de Vinculaciones

### Página Web `/ble`

La interfaz web permite gestionar las vinculaciones BLE:

- Ver estado del servidor BLE
- Ver superadmin registrado
- Ver usuarios vinculados con permisos
- Eliminar superadmin
- Eliminar usuarios individuales
- Eliminar todas las vinculaciones

### API REST

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/ble` | GET | Página de gestión BLE |
| `/ble/clear-all` | GET | Eliminar todas las vinculaciones |
| `/ble/clear-superadmin` | GET | Eliminar superadmin |
| `/ble/clear-user?slot=N` | GET | Eliminar usuario N (1-5) |
| `/api/ble/status` | GET | Estado BLE en JSON |

### Comandos MQTT

Ver [mqtt-protocol.md](mqtt-protocol.md) para comandos de gestión BLE vía MQTT (message_type: 6).

---

## Almacenamiento EEPROM

### Estructura BLEAuthConfig

```c
struct BLEAuthConfig {
  uint32_t validMarker;                    // 0xB1E4C0DE
  uint8_t superadmin_key[64];              // Clave superadmin
  uint8_t user_keys[5][64];                // Claves usuarios
  uint8_t user_enabled[5];                 // Estados usuarios
  uint8_t user_permissions[5];             // Permisos usuarios
  char user_names[5][16];                  // Nombres usuarios
  uint8_t superadmin_registered;           // Superadmin activo
  uint32_t checksum;                       // Integridad
};
```

**Offset EEPROM:** 3400
**Tamaño:** ~450 bytes

---

## Ejemplos de Implementación (APP)

### Android (Kotlin) - Autenticación

```kotlin
// Generar clave de 64 bytes
val key = ByteArray(64)
SecureRandom().nextBytes(key)

// Escribir en característica de autenticación
val authCharacteristic = gatt.getService(SERVICE_UUID)
    .getCharacteristic(CHAR_AUTH_UUID)
authCharacteristic.value = key
gatt.writeCharacteristic(authCharacteristic)
```

### iOS (Swift) - Activar Relé

```swift
// Activar relé 1 con pulso de 2 segundos
let data = Data([0x01, 0x02, 0x07, 0xD0]) // relay=1, action=pulse, 2000ms
peripheral.writeValue(data, for: relayCharacteristic, type: .withResponse)
```

---

## Códigos de Error

| Error | Causa | Solución |
|-------|-------|----------|
| `ERROR:NOT_AUTHORIZED` | No autenticado o sin permisos | Autenticarse primero |
| `ERROR:INVALID_KEY` | Clave incorrecta | Verificar clave |
| `ERROR:INVALID_LENGTH` | Longitud de datos incorrecta | Revisar formato |
| `ERROR:INVALID_RELAY` | Relé fuera de rango | Usar 1 o 2 |
| `ERROR:INVALID_PARAMS` | Parámetros incorrectos | Revisar formato |
| `ERROR:CODE_EXISTS` | Código duplicado | Usar código diferente |

---

## Seguridad

### Recomendaciones

1. **Usar claves aleatorias** de 64 bytes generadas con CSPRNG
2. **No compartir claves** entre dispositivos
3. **Eliminar vinculaciones** si se pierde un dispositivo móvil
4. **Usar permisos mínimos** para usuarios que no sean admin

### Limitaciones

- No hay encriptación adicional sobre BLE
- Las claves se almacenan en EEPROM (no encriptadas)
- El nombre BLE es público y revela el serial

---

## Historial de Cambios

| Versión | Fecha | Cambios |
|---------|-------|---------|
| v4.0.0 | Feb 2026 | Versión inicial con BLE |
| v4.0.1 | Feb 2026 | Añadidas características FF09 (Info Completa) y FF0A (Lista Códigos) |
| v4.0.2 | 27-Feb-2026 | FF09 ahora tiene NOTIFY habilitado. Corregido setValue con longitud explícita. |
| v4.0.3 | 27-Feb-2026 | FF07 ahora tiene callback onRead con JSON completo y notify(). Añadidos campos eth_connected y mqtt_connected. |

---

*Documentación generada para SWATID-A2 v4.0*
*Última actualización: 5 Febrero 2026*
