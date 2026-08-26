# Guía Completa de Integración MQTT - SWATID-A2

> **Versión:** 4.1.0  
> **Última actualización:** 5 Febrero 2026  
> **Audiencia:** Desarrolladores Backend y Sistemas

---

## Índice

1. [Arquitectura MQTT](#1-arquitectura-mqtt)
2. [Tópicos y Estructura](#2-tópicos-y-estructura)
3. [Formato de Mensajes](#3-formato-de-mensajes)
4. [Comandos de Control](#4-comandos-de-control)
5. [Gestión de Códigos](#5-gestión-de-códigos)
6. [Gestión Segura de Usuarios BLE](#6-gestión-segura-de-usuarios-ble)
7. [Eventos y Notificaciones](#7-eventos-y-notificaciones)
8. [Seguridad y Mejores Prácticas](#8-seguridad-y-mejores-prácticas)
9. [Implementación Backend (Python)](#9-implementación-backend-python)
10. [Implementación Backend (Node.js)](#10-implementación-backend-nodejs)
11. [Mejoras Requeridas](#11-mejoras-requeridas)

---

## 1. Arquitectura MQTT

### Diagrama General

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              BROKER MQTT                                     │
│                     (Mosquitto, EMQX, HiveMQ, AWS IoT)                       │
└─────────────────┬─────────────────────────────┬─────────────────────────────┘
                  │                             │
         Subscribe│                             │Publish
                  ▼                             │
┌─────────────────────────┐         ┌───────────────────────────────────────┐
│    DISPOSITIVO A2       │         │              BACKEND                   │
│   SWATID_D8F7B4BF1388   │         │         (Servidor/Cloud)               │
├─────────────────────────┤         ├───────────────────────────────────────┤
│                         │         │                                        │
│ Subscribe:              │         │ Subscribe:                             │
│ swatidhome/cmd/{serial} │         │ swatidhome/response/{serial}          │
│                         │         │ swatidhome/status/{serial}            │
│ Publish:                │         │ swatidhome/events/{serial}/#          │
│ swatidhome/response/... │         │                                        │
│ swatidhome/status/...   │         │ Publish:                               │
│ swatidhome/events/...   │         │ swatidhome/cmd/{serial}               │
│                         │         │                                        │
└─────────────────────────┘         └───────────────────────────────────────┘
```

### Configuración de Conexión

```yaml
# Configuración típica
broker: mqtt.tudominio.com
port: 1883          # Sin TLS
port_tls: 8883      # Con TLS (recomendado)
username: device_user
password: secure_password
client_id: SWATID_{serial}
keep_alive: 60
clean_session: true
```

---

## 2. Tópicos y Estructura

### Tópicos del Dispositivo

| Tópico | Dirección | Descripción |
|--------|-----------|-------------|
| `swatidhome/cmd/{serial}` | Backend → Device | Comandos al dispositivo |
| `swatidhome/response/{serial}` | Device → Backend | Respuestas a comandos |
| `swatidhome/status/{serial}` | Device → Backend | Estado periódico |
| `swatidhome/events/{serial}/access` | Device → Backend | Eventos de acceso |
| `swatidhome/events/{serial}/relay` | Device → Backend | Eventos de relés |
| `swatidhome/events/{serial}/ble` | Device → Backend | Eventos BLE |
| `swatidhome/events/{serial}/users` | Device → Backend | Eventos de usuarios |
| `swatidhome/events/{serial}/codes` | Device → Backend | Eventos de códigos |

### Ejemplo de Serial

```
Serial: D8F7B4BF1388 (derivado de MAC)
Nombre BLE: SWATID_D8F7B4BF1388
Tópico base: swatidhome/cmd/D8F7B4BF1388
```

---

## 3. Formato de Mensajes

### Estructura de Comando

```json
{
  "message_type": <número>,
  "message_id": <id_único>,
  "message_info": {
    // Parámetros según message_type
  }
}
```

### Estructura de Respuesta

```json
{
  "status": <0=ok, 1=error>,
  "message_id": <id_original>,
  "response": "<descripción>",
  "data": { /* datos adicionales */ }
}
```

### Tipos de Mensaje (message_type)

| Tipo | Nombre | Descripción |
|------|--------|-------------|
| 1 | Relay Control | Control de relés |
| 2 | Mode Control | Cambio de modo |
| 3 | Get Info | Obtener información |
| 4 | BLE Management | Gestión BLE/Usuarios |
| 5 | Code Management | Gestión de códigos |
| 6 | Network Config | Configuración de red |
| 7 | Security | Seguridad |
| 8 | OTA | Actualizaciones |

---

## 4. Comandos de Control

### 4.1 Control de Relés (message_type: 1)

**Activar Relé:**
```json
{
  "message_type": 1,
  "message_id": 1001,
  "message_info": {
    "action": "activate",
    "relay": 1,
    "duration": 5000
  }
}
```

**Respuesta:**
```json
{
  "status": 0,
  "message_id": 1001,
  "response": "relay 1 activated for 5000ms"
}
```

**Desactivar Relé:**
```json
{
  "message_type": 1,
  "message_id": 1002,
  "message_info": {
    "action": "deactivate",
    "relay": 1
  }
}
```

**Toggle Relé:**
```json
{
  "message_type": 1,
  "message_id": 1003,
  "message_info": {
    "action": "toggle",
    "relay": 2
  }
}
```

### 4.2 Control de Modo (message_type: 2)

**Cambiar Modo:**
```json
{
  "message_type": 2,
  "message_id": 2001,
  "message_info": {
    "action": "set_mode",
    "mode": "turnstile"
  }
}
```

Modos válidos: `normal`, `inverse`, `turnstile`

### 4.3 Obtener Información (message_type: 3)

**Info Completa:**
```json
{
  "message_type": 3,
  "message_id": 3001,
  "message_info": {
    "action": "get_full_info"
  }
}
```

**Respuesta:**
```json
{
  "status": 0,
  "message_id": 3001,
  "response": "info",
  "data": {
    "device": {
      "type": "KC868-A2",
      "serial": "D8F7B4BF1388",
      "firmware": "v4.1.0-BLE",
      "uptime_ms": 3600000
    },
    "network": {
      "ip": "192.168.1.100",
      "mac": "88:13:BF:B4:F7:D8",
      "dhcp": true,
      "connected": true
    },
    "relays": {
      "relay1": false,
      "relay2": false
    },
    "mode": "normal",
    "ble": {
      "enabled": true,
      "connected": false,
      "has_superadmin": true,
      "users_count": 2
    },
    "codes": {
      "local_count": 15,
      "local_max": 50,
      "remote_count": 5,
      "remote_max": 30
    }
  }
}
```

---

## 5. Gestión de Códigos

### 5.1 Códigos Locales (message_type: 5)

#### Añadir Código Local

```json
{
  "message_type": 5,
  "message_id": 5001,
  "message_info": {
    "action": "add_local_code",
    "code_type": "PIN",
    "code_value": "123456",
    "keyboard_id": 0,
    "relay": 1
  }
}
```

| Campo | Tipo | Valores | Descripción |
|-------|------|---------|-------------|
| code_type | string | "PIN", "TAG" | Tipo de código |
| code_value | string | 1-16 chars | Valor del código |
| keyboard_id | int | 0, 1, 2 | 0=ambos, 1=teclado1, 2=teclado2 |
| relay | int | 1, 2 | Relé a activar |

**Respuesta Exitosa:**
```json
{
  "status": 0,
  "message_id": 5001,
  "response": "local code added: {\"code_type\":\"PIN\",\"code_value\":\"123456\",\"keyboard_id\":0,\"relay\":1,\"total_codes\":16}"
}
```

#### Eliminar Código Local

```json
{
  "message_type": 5,
  "message_id": 5002,
  "message_info": {
    "action": "remove_local_code",
    "code_type": "PIN",
    "code_value": "123456",
    "keyboard_id": 0
  }
}
```

#### Listar Códigos Locales

```json
{
  "message_type": 5,
  "message_id": 5003,
  "message_info": {
    "action": "list_local_codes"
  }
}
```

**Respuesta:**
```json
{
  "status": 0,
  "message_id": 5003,
  "response": "codes list",
  "data": {
    "count": 3,
    "max": 50,
    "validation_mode": "local_first",
    "codes": [
      {"id": 0, "type": "PIN", "value": "123456", "keyboard": 0, "relay": 1},
      {"id": 1, "type": "PIN", "value": "654321", "keyboard": 1, "relay": 2},
      {"id": 2, "type": "TAG", "value": "A1B2C3D4", "keyboard": 0, "relay": 1}
    ]
  }
}
```

#### Limpiar Todos los Códigos

```json
{
  "message_type": 5,
  "message_id": 5004,
  "message_info": {
    "action": "clear_local_codes"
  }
}
```

### 5.2 Códigos Remotos (con Franjas Horarias)

#### Añadir Código Remoto

```json
{
  "message_type": 5,
  "message_id": 5010,
  "message_info": {
    "action": "add_remote_code",
    "code_type": "PIN",
    "code_value": "789012",
    "keyboard_id": 0,
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

**days_of_week (bitmask):**
```
Bit 0 (1):  Lunes
Bit 1 (2):  Martes
Bit 2 (4):  Miércoles
Bit 3 (8):  Jueves
Bit 4 (16): Viernes
Bit 5 (32): Sábado
Bit 6 (64): Domingo

Ejemplo: 31 = 11111 = Lunes a Viernes
         127 = 1111111 = Todos los días
```

---

## 6. Gestión Segura de Usuarios BLE

### 6.1 El Problema de Seguridad (v4.0)

En v4.0, añadir usuarios enviaba la clave en claro:

```json
// ❌ INSEGURO - Clave visible en el mensaje
{
  "message_type": 4,
  "message_info": {
    "action": "add_user",
    "slot": 1,
    "key": "A1B2C3D4E5F6...128 caracteres hex..."
  }
}
```

**Riesgos:**
- Clave interceptable si MQTT no usa TLS
- Visible en logs del broker
- Almacenable en históricos

### 6.2 Solución v4.1: Derivación HKDF

La clave del usuario NO viaja en el mensaje. Se deriva usando:

```
user_key = HKDF-SHA256(
    IKM  = device_master_key,  // 32 bytes, compartido
    salt = serial_number,       // Único por dispositivo
    info = "user_key:" + user_id,
    len  = 64 bytes
)
```

#### Comando add_user_derived

```json
{
  "message_type": 4,
  "message_id": 4001,
  "message_info": {
    "action": "add_user_derived",
    "slot": 1,
    "user_id": "user_unique_123",
    "name": "Juan García",
    "permissions": 15
  }
}
```

**Respuesta:**
```json
{
  "status": 0,
  "message_id": 4001,
  "response": "user added (derived): {\"slot\":1,\"user_id\":\"user_unique_123\",\"name\":\"Juan García\",\"permissions\":15,\"enabled\":true,\"method\":\"hkdf_derived\",\"salt\":\"SWATID_D8F7B4BF1388\",\"info_prefix\":\"user_key:\"}",
  "data": {
    "slot": 1,
    "user_id": "user_unique_123",
    "name": "Juan García",
    "permissions": 15,
    "method": "hkdf_derived"
  }
}
```

### 6.3 Flujo Completo de Derivación

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         FLUJO DE DERIVACIÓN HKDF                             │
└─────────────────────────────────────────────────────────────────────────────┘

1. REGISTRO INICIAL DEL DISPOSITIVO (Una sola vez)
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

   ┌──────────────┐                              ┌───────────────┐
   │   BACKEND    │                              │  DISPOSITIVO  │
   └──────┬───────┘                              └───────┬───────┘
          │                                              │
          │  Durante instalación física o primer boot   │
          │  el técnico registra el dispositivo         │
          │                                              │
          │  Dispositivo genera:                        │
          │  device_master_key = random(32 bytes)       │
          │◄─────────────────────────────────────────────│
          │  (Transferido de forma segura, ej: QR)      │
          │                                              │
          │  Backend almacena:                          │
          │  {                                          │
          │    serial: "D8F7B4BF1388",                  │
          │    master_key: "a1b2c3d4..."                │
          │  }                                          │
          │                                              │

2. AÑADIR USUARIO (Cada vez que se necesite)
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

   ┌──────────────┐          ┌──────────┐          ┌───────────────┐
   │   BACKEND    │          │  BROKER  │          │  DISPOSITIVO  │
   └──────┬───────┘          └────┬─────┘          └───────┬───────┘
          │                       │                        │
          │ a) Calcular clave:    │                        │
          │    user_key = HKDF(   │                        │
          │      master_key,      │                        │
          │      serial,          │                        │
          │      "user_key:user123")                       │
          │                       │                        │
          │ b) Guardar user_key   │                        │
          │    para dar a la App  │                        │
          │                       │                        │
          │ c) Publicar (SIN clave):                       │
          │    {action:"add_user_derived",                 │
          │     slot:1,                                    │
          │     user_id:"user123",      ──────────────────►│
          │     name:"Juan"}            │                  │
          │                             │                  │
          │                             │  d) Dispositivo  │
          │                             │  calcula MISMA   │
          │                             │  clave usando:   │
          │                             │  - Su master_key │
          │                             │  - Su serial     │
          │                             │  - user_id recibido
          │                             │                  │
          │                             │  e) Guarda en    │
          │                             │  EEPROM slot 1   │
          │                             │                  │
          │◄───────────────────────────────────────────────│
          │         OK: user added (derived)               │
          │                                                │

3. LA APP SE AUTENTICA
   ━━━━━━━━━━━━━━━━━━━━

   ┌───────────────┐
   │     APP       │
   └───────┬───────┘
           │
           │  La App tiene la user_key
           │  (dada por el Backend)
           │
           │  Usa BLE Challenge-Response
           │  o clave directa (legado)
           │
           ▼
   ┌───────────────┐
   │  DISPOSITIVO  │
   │  verifica con │
   │  clave derivada│
   └───────────────┘
```

### 6.4 Implementación HKDF en Backend

#### Python

```python
import hashlib
import hmac
from typing import Optional

def hkdf_extract(salt: bytes, ikm: bytes) -> bytes:
    """HKDF-Extract: PRK = HMAC-SHA256(salt, IKM)"""
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

def derive_user_key(
    device_master_key: bytes,  # 32 bytes
    serial: str,
    user_id: str
) -> bytes:
    """
    Deriva la clave de usuario BLE usando HKDF-SHA256.
    
    Returns:
        bytes: Clave de 64 bytes para autenticación BLE
    """
    salt = serial.encode('utf-8')
    info = f"user_key:{user_id}".encode('utf-8')
    
    prk = hkdf_extract(salt, device_master_key)
    user_key = hkdf_expand(prk, info, 64)
    
    return user_key


# ═══════════════════════════════════════════════════════════════════
# EJEMPLO DE USO COMPLETO
# ═══════════════════════════════════════════════════════════════════

import json
import paho.mqtt.client as mqtt

class DeviceManager:
    def __init__(self, broker: str, username: str, password: str):
        self.client = mqtt.Client()
        self.client.username_pw_set(username, password)
        self.client.on_message = self._on_message
        self.client.connect(broker, 1883)
        self.client.loop_start()
        
        # Base de datos de dispositivos (ejemplo simplificado)
        self.devices = {}
        self.pending_responses = {}
    
    def register_device(self, serial: str, master_key: bytes):
        """Registra un dispositivo con su clave maestra."""
        self.devices[serial] = {
            'master_key': master_key,
            'users': {}
        }
        # Suscribirse a respuestas y eventos
        self.client.subscribe(f"swatidhome/response/{serial}")
        self.client.subscribe(f"swatidhome/events/{serial}/#")
    
    def add_user_secure(
        self,
        serial: str,
        slot: int,
        user_id: str,
        name: str,
        permissions: int
    ) -> Optional[bytes]:
        """
        Añade un usuario de forma segura usando derivación HKDF.
        
        Returns:
            bytes: La clave derivada para dar a la App, o None si falla
        """
        if serial not in self.devices:
            raise ValueError(f"Dispositivo {serial} no registrado")
        
        device = self.devices[serial]
        
        # 1. Derivar la clave del usuario
        user_key = derive_user_key(
            device['master_key'],
            serial,
            user_id
        )
        
        # 2. Guardar en nuestra base de datos
        device['users'][user_id] = {
            'slot': slot,
            'name': name,
            'key': user_key,
            'permissions': permissions
        }
        
        # 3. Enviar comando MQTT (SIN la clave)
        message_id = f"add_user_{serial}_{slot}"
        command = {
            "message_type": 4,
            "message_id": message_id,
            "message_info": {
                "action": "add_user_derived",
                "slot": slot,
                "user_id": user_id,
                "name": name,
                "permissions": permissions
            }
        }
        
        topic = f"swatidhome/cmd/{serial}"
        self.client.publish(topic, json.dumps(command))
        
        print(f"✅ Usuario {name} añadido al slot {slot}")
        print(f"   User ID: {user_id}")
        print(f"   Clave derivada: {user_key.hex()[:32]}...")
        
        return user_key
    
    def get_user_key_for_app(self, serial: str, user_id: str) -> Optional[bytes]:
        """
        Obtiene la clave de un usuario para entregarla a la App.
        
        En producción, esto se entregaría de forma segura
        (HTTPS, notificación push encriptada, etc.)
        """
        if serial in self.devices and user_id in self.devices[serial]['users']:
            return self.devices[serial]['users'][user_id]['key']
        return None
    
    def _on_message(self, client, userdata, msg):
        """Procesa mensajes recibidos del broker."""
        topic = msg.topic
        payload = json.loads(msg.payload.decode())
        
        if '/response/' in topic:
            print(f"📩 Respuesta: {payload}")
        elif '/events/' in topic:
            print(f"📢 Evento: {payload}")


# Ejemplo de uso
if __name__ == "__main__":
    manager = DeviceManager("mqtt.example.com", "user", "pass")
    
    # Registro inicial del dispositivo (con clave maestra compartida)
    device_master_key = bytes.fromhex(
        "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4"
        "e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2"
    )
    manager.register_device("D8F7B4BF1388", device_master_key)
    
    # Añadir usuario de forma segura
    user_key = manager.add_user_secure(
        serial="D8F7B4BF1388",
        slot=1,
        user_id="user_juan_001",
        name="Juan García",
        permissions=0x0F
    )
    
    # La clave user_key se entrega a la App de Juan
    # (vía HTTPS, notificación, etc.)
```

#### Node.js / TypeScript

```typescript
import * as crypto from 'crypto';
import * as mqtt from 'mqtt';

// ═══════════════════════════════════════════════════════════════════
// FUNCIONES HKDF
// ═══════════════════════════════════════════════════════════════════

function hkdfExtract(salt: Buffer, ikm: Buffer): Buffer {
  return crypto.createHmac('sha256', salt).update(ikm).digest();
}

function hkdfExpand(prk: Buffer, info: Buffer, length: number): Buffer {
  let t = Buffer.alloc(0);
  let okm = Buffer.alloc(0);
  let counter = 1;
  
  while (okm.length < length) {
    const data = Buffer.concat([t, info, Buffer.from([counter])]);
    t = crypto.createHmac('sha256', prk).update(data).digest();
    okm = Buffer.concat([okm, t]);
    counter++;
  }
  
  return okm.slice(0, length);
}

function deriveUserKey(
  deviceMasterKey: Buffer,  // 32 bytes
  serial: string,
  userId: string
): Buffer {
  const salt = Buffer.from(serial, 'utf-8');
  const info = Buffer.from(`user_key:${userId}`, 'utf-8');
  
  const prk = hkdfExtract(salt, deviceMasterKey);
  const userKey = hkdfExpand(prk, info, 64);
  
  return userKey;
}

// ═══════════════════════════════════════════════════════════════════
// CLASE DE GESTIÓN DE DISPOSITIVOS
// ═══════════════════════════════════════════════════════════════════

interface DeviceInfo {
  masterKey: Buffer;
  users: Map<string, UserInfo>;
}

interface UserInfo {
  slot: number;
  name: string;
  key: Buffer;
  permissions: number;
}

class DeviceManager {
  private client: mqtt.MqttClient;
  private devices: Map<string, DeviceInfo> = new Map();
  
  constructor(brokerUrl: string, username: string, password: string) {
    this.client = mqtt.connect(brokerUrl, {
      username,
      password,
      clientId: `backend_${Date.now()}`
    });
    
    this.client.on('connect', () => {
      console.log('✅ Conectado al broker MQTT');
    });
    
    this.client.on('message', this.handleMessage.bind(this));
  }
  
  registerDevice(serial: string, masterKey: Buffer): void {
    this.devices.set(serial, {
      masterKey,
      users: new Map()
    });
    
    this.client.subscribe(`swatidhome/response/${serial}`);
    this.client.subscribe(`swatidhome/events/${serial}/#`);
    
    console.log(`📱 Dispositivo ${serial} registrado`);
  }
  
  async addUserSecure(
    serial: string,
    slot: number,
    userId: string,
    name: string,
    permissions: number
  ): Promise<Buffer | null> {
    const device = this.devices.get(serial);
    if (!device) {
      throw new Error(`Dispositivo ${serial} no registrado`);
    }
    
    // 1. Derivar clave
    const userKey = deriveUserKey(device.masterKey, serial, userId);
    
    // 2. Guardar localmente
    device.users.set(userId, {
      slot,
      name,
      key: userKey,
      permissions
    });
    
    // 3. Enviar comando MQTT
    const command = {
      message_type: 4,
      message_id: `add_${serial}_${slot}_${Date.now()}`,
      message_info: {
        action: 'add_user_derived',
        slot,
        user_id: userId,
        name,
        permissions
      }
    };
    
    const topic = `swatidhome/cmd/${serial}`;
    this.client.publish(topic, JSON.stringify(command));
    
    console.log(`✅ Usuario ${name} añadido al slot ${slot}`);
    console.log(`   User ID: ${userId}`);
    console.log(`   Clave: ${userKey.toString('hex').substring(0, 32)}...`);
    
    return userKey;
  }
  
  getUserKeyForApp(serial: string, userId: string): Buffer | null {
    const device = this.devices.get(serial);
    if (!device) return null;
    
    const user = device.users.get(userId);
    return user?.key || null;
  }
  
  private handleMessage(topic: string, message: Buffer): void {
    try {
      const payload = JSON.parse(message.toString());
      
      if (topic.includes('/response/')) {
        console.log(`📩 Respuesta:`, payload);
      } else if (topic.includes('/events/')) {
        console.log(`📢 Evento:`, payload);
      }
    } catch (e) {
      console.error('Error procesando mensaje:', e);
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
// EJEMPLO DE USO
// ═══════════════════════════════════════════════════════════════════

const manager = new DeviceManager(
  'mqtt://mqtt.example.com',
  'user',
  'password'
);

// Registrar dispositivo con clave maestra
const masterKey = Buffer.from(
  'a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2',
  'hex'
);
manager.registerDevice('D8F7B4BF1388', masterKey);

// Añadir usuario
manager.addUserSecure(
  'D8F7B4BF1388',
  1,
  'user_juan_001',
  'Juan García',
  0x0F
).then(userKey => {
  console.log('Clave para la App:', userKey?.toString('hex'));
});
```

### 6.5 Eliminar Usuario

```json
{
  "message_type": 4,
  "message_id": 4002,
  "message_info": {
    "action": "remove_user",
    "slot": 1
  }
}
```

**Evento publicado:**
```json
{
  "event": "USER_UNBOUND",
  "source": "MQTT",
  "device": "D8F7B4BF1388",
  "slot": 1,
  "name": "Juan García",
  "timestamp": "2026-02-05T15:30:00Z"
}
```

### 6.6 Listar Usuarios

```json
{
  "message_type": 4,
  "message_id": 4003,
  "message_info": {
    "action": "list_users"
  }
}
```

**Respuesta:**
```json
{
  "status": 0,
  "message_id": 4003,
  "response": "users list",
  "data": {
    "superadmin": {
      "enabled": true,
      "name": "Admin Principal"
    },
    "users": [
      {"slot": 1, "enabled": true, "name": "Juan García", "permissions": 15},
      {"slot": 2, "enabled": true, "name": "María López", "permissions": 7},
      {"slot": 3, "enabled": false, "name": "", "permissions": 0},
      {"slot": 4, "enabled": false, "name": "", "permissions": 0},
      {"slot": 5, "enabled": false, "name": "", "permissions": 0}
    ]
  }
}
```

---

## 7. Eventos y Notificaciones

### 7.1 Eventos de Acceso

**Tópico:** `swatidhome/events/{serial}/access`

```json
{
  "event": "ACCESS_GRANTED",
  "code_type": "PIN",
  "code_value": "123456",
  "keyboard": 1,
  "relay": 1,
  "validation": "local",
  "timestamp": "2026-02-05T10:30:00Z"
}
```

```json
{
  "event": "ACCESS_DENIED",
  "code_type": "PIN",
  "code_value": "******",
  "keyboard": 1,
  "reason": "invalid_code",
  "timestamp": "2026-02-05T10:30:05Z"
}
```

### 7.2 Eventos de Relés

**Tópico:** `swatidhome/events/{serial}/relay`

```json
{
  "event": "RELAY_ACTIVATED",
  "relay": 1,
  "source": "MQTT",
  "duration_ms": 5000,
  "timestamp": "2026-02-05T10:31:00Z"
}
```

### 7.3 Eventos BLE

**Tópico:** `swatidhome/events/{serial}/ble`

```json
{
  "event": "BLE_CONNECTED",
  "device_address": "AA:BB:CC:DD:EE:FF",
  "timestamp": "2026-02-05T10:32:00Z"
}
```

```json
{
  "event": "AUTH_SUCCESS",
  "method": "challenge_response",
  "user": "SUPERADMIN",
  "timestamp": "2026-02-05T10:32:05Z"
}
```

### 7.4 Eventos de Usuarios

**Tópico:** `swatidhome/events/{serial}/users`

```json
{
  "event": "USER_ADDED_DERIVED",
  "source": "MQTT",
  "slot": 1,
  "user_id": "user_juan_001",
  "name": "Juan García",
  "permissions": 15,
  "method": "hkdf",
  "timestamp": "2026-02-05T10:33:00Z"
}
```

```json
{
  "event": "SUPERADMIN_UNBOUND",
  "source": "BLE",
  "users_removed": 3,
  "timestamp": "2026-02-05T10:34:00Z"
}
```

### 7.5 Eventos de Códigos

**Tópico:** `swatidhome/events/{serial}/codes`

```json
{
  "event": "CODE_ADDED",
  "source": "BLE",
  "user": "SUPERADMIN",
  "code_type": "PIN",
  "code_value": "789012",
  "keyboard": 0,
  "relay": 1,
  "total_codes": 16,
  "timestamp": "2026-02-05T10:35:00Z"
}
```

---

## 8. Seguridad y Mejores Prácticas

### 8.1 Configuración TLS

```yaml
# Mosquitto config
listener 8883
certfile /etc/mosquitto/certs/server.crt
cafile /etc/mosquitto/certs/ca.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
tls_version tlsv1.2
```

**Conexión desde dispositivo:**
```cpp
// En el firmware
espClient.setCACert(ca_cert);
espClient.setCertificate(client_cert);
espClient.setPrivateKey(client_key);
```

### 8.2 Control de Acceso ACL

```
# ACL para Mosquitto
# Dispositivos solo pueden publicar en sus tópicos
pattern write swatidhome/response/%c
pattern write swatidhome/status/%c
pattern write swatidhome/events/%c/#

# Dispositivos solo leen comandos dirigidos a ellos
pattern read swatidhome/cmd/%c

# Backend puede leer/escribir todo
user backend_admin
topic readwrite swatidhome/#
```

### 8.3 Validación de Mensajes

```python
import json
from jsonschema import validate, ValidationError

COMMAND_SCHEMA = {
    "type": "object",
    "required": ["message_type", "message_id", "message_info"],
    "properties": {
        "message_type": {"type": "integer", "minimum": 1, "maximum": 10},
        "message_id": {"type": ["string", "integer"]},
        "message_info": {"type": "object"}
    }
}

def validate_command(payload: str) -> bool:
    try:
        data = json.loads(payload)
        validate(instance=data, schema=COMMAND_SCHEMA)
        return True
    except (json.JSONDecodeError, ValidationError):
        return False
```

### 8.4 Rate Limiting

```python
from collections import defaultdict
from time import time

class RateLimiter:
    def __init__(self, max_requests: int = 10, window_seconds: int = 60):
        self.max_requests = max_requests
        self.window = window_seconds
        self.requests = defaultdict(list)
    
    def is_allowed(self, device_serial: str) -> bool:
        now = time()
        # Limpiar requests antiguos
        self.requests[device_serial] = [
            t for t in self.requests[device_serial]
            if now - t < self.window
        ]
        
        if len(self.requests[device_serial]) >= self.max_requests:
            return False
        
        self.requests[device_serial].append(now)
        return True
```

### 8.5 Tabla de Comparación de Seguridad

| Aspecto | v4.0 | v4.1 | Mejora |
|---------|------|------|--------|
| Claves en MQTT | En claro | Derivadas (HKDF) | ✅ |
| Auth BLE | Clave directa | Challenge-Response | ✅ |
| Sesiones | Sin token | Token 8 bytes | ✅ |
| Timeout sesión | No | 5 minutos | ✅ |
| Eventos seguridad | Básicos | Completos | ✅ |
| Logging | Sin info sensible | Sin info sensible | = |

---

## 9. Implementación Backend (Python)

### Cliente MQTT Completo

```python
#!/usr/bin/env python3
"""
SWATID-A2 Backend Client
Implementación completa de gestión de dispositivos via MQTT
"""

import json
import hashlib
import hmac
import logging
from typing import Dict, Optional, Callable, Any
from dataclasses import dataclass, field
from datetime import datetime
import paho.mqtt.client as mqtt

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@dataclass
class User:
    slot: int
    user_id: str
    name: str
    key: bytes
    permissions: int
    enabled: bool = True


@dataclass
class Device:
    serial: str
    master_key: bytes
    users: Dict[str, User] = field(default_factory=dict)
    last_seen: Optional[datetime] = None
    info: Dict[str, Any] = field(default_factory=dict)


class SWATIDA2Backend:
    """Backend para gestión de dispositivos SWATID-A2."""
    
    def __init__(
        self,
        broker: str,
        port: int = 1883,
        username: Optional[str] = None,
        password: Optional[str] = None,
        use_tls: bool = False
    ):
        self.client = mqtt.Client(client_id=f"swatid_backend_{id(self)}")
        
        if username and password:
            self.client.username_pw_set(username, password)
        
        if use_tls:
            self.client.tls_set()
        
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        
        self.broker = broker
        self.port = port
        self.devices: Dict[str, Device] = {}
        self.event_handlers: Dict[str, Callable] = {}
        self.pending_responses: Dict[str, Callable] = {}
        self._message_counter = 0
    
    # ═══════════════════════════════════════════════════════════════
    # CONEXIÓN
    # ═══════════════════════════════════════════════════════════════
    
    def connect(self) -> None:
        """Conecta al broker MQTT."""
        self.client.connect(self.broker, self.port, keepalive=60)
        self.client.loop_start()
    
    def disconnect(self) -> None:
        """Desconecta del broker MQTT."""
        self.client.loop_stop()
        self.client.disconnect()
    
    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logger.info("✅ Conectado al broker MQTT")
            # Re-suscribirse a dispositivos registrados
            for serial in self.devices:
                self._subscribe_device(serial)
        else:
            logger.error(f"❌ Error de conexión: {rc}")
    
    def _on_disconnect(self, client, userdata, rc):
        logger.warning(f"⚠️ Desconectado del broker: {rc}")
    
    def _on_message(self, client, userdata, msg):
        try:
            topic = msg.topic
            payload = json.loads(msg.payload.decode())
            
            # Extraer serial del tópico
            parts = topic.split('/')
            if len(parts) >= 3:
                serial = parts[2]
                
                if serial in self.devices:
                    self.devices[serial].last_seen = datetime.now()
            
            # Respuestas
            if '/response/' in topic:
                self._handle_response(payload)
            
            # Eventos
            elif '/events/' in topic:
                self._handle_event(topic, payload)
            
            # Estado
            elif '/status/' in topic:
                self._handle_status(topic, payload)
                
        except json.JSONDecodeError:
            logger.error(f"Error decodificando mensaje: {msg.payload}")
        except Exception as e:
            logger.error(f"Error procesando mensaje: {e}")
    
    def _subscribe_device(self, serial: str):
        """Suscribe a los tópicos de un dispositivo."""
        self.client.subscribe(f"swatidhome/response/{serial}")
        self.client.subscribe(f"swatidhome/status/{serial}")
        self.client.subscribe(f"swatidhome/events/{serial}/#")
    
    # ═══════════════════════════════════════════════════════════════
    # REGISTRO DE DISPOSITIVOS
    # ═══════════════════════════════════════════════════════════════
    
    def register_device(self, serial: str, master_key: bytes) -> Device:
        """
        Registra un dispositivo con su clave maestra.
        
        Args:
            serial: Número de serie del dispositivo
            master_key: Clave maestra de 32 bytes
        
        Returns:
            Device: Objeto dispositivo creado
        """
        if len(master_key) != 32:
            raise ValueError("master_key debe ser de 32 bytes")
        
        device = Device(serial=serial, master_key=master_key)
        self.devices[serial] = device
        self._subscribe_device(serial)
        
        logger.info(f"📱 Dispositivo {serial} registrado")
        return device
    
    def get_device(self, serial: str) -> Optional[Device]:
        """Obtiene un dispositivo por su serial."""
        return self.devices.get(serial)
    
    # ═══════════════════════════════════════════════════════════════
    # HKDF
    # ═══════════════════════════════════════════════════════════════
    
    @staticmethod
    def _hkdf_extract(salt: bytes, ikm: bytes) -> bytes:
        return hmac.new(salt, ikm, hashlib.sha256).digest()
    
    @staticmethod
    def _hkdf_expand(prk: bytes, info: bytes, length: int) -> bytes:
        t = b""
        okm = b""
        counter = 1
        while len(okm) < length:
            t = hmac.new(prk, t + info + bytes([counter]), hashlib.sha256).digest()
            okm += t
            counter += 1
        return okm[:length]
    
    def derive_user_key(
        self,
        device_master_key: bytes,
        serial: str,
        user_id: str
    ) -> bytes:
        """Deriva la clave de un usuario usando HKDF-SHA256."""
        salt = serial.encode('utf-8')
        info = f"user_key:{user_id}".encode('utf-8')
        prk = self._hkdf_extract(salt, device_master_key)
        return self._hkdf_expand(prk, info, 64)
    
    # ═══════════════════════════════════════════════════════════════
    # COMANDOS
    # ═══════════════════════════════════════════════════════════════
    
    def _next_message_id(self) -> str:
        self._message_counter += 1
        return f"msg_{self._message_counter}_{int(datetime.now().timestamp())}"
    
    def _send_command(
        self,
        serial: str,
        message_type: int,
        message_info: Dict,
        callback: Optional[Callable] = None
    ) -> str:
        """Envía un comando a un dispositivo."""
        if serial not in self.devices:
            raise ValueError(f"Dispositivo {serial} no registrado")
        
        message_id = self._next_message_id()
        command = {
            "message_type": message_type,
            "message_id": message_id,
            "message_info": message_info
        }
        
        if callback:
            self.pending_responses[message_id] = callback
        
        topic = f"swatidhome/cmd/{serial}"
        self.client.publish(topic, json.dumps(command))
        
        logger.debug(f"📤 Comando enviado: {message_type} -> {serial}")
        return message_id
    
    # ═══════════════════════════════════════════════════════════════
    # GESTIÓN DE USUARIOS
    # ═══════════════════════════════════════════════════════════════
    
    def add_user_secure(
        self,
        serial: str,
        slot: int,
        user_id: str,
        name: str,
        permissions: int,
        callback: Optional[Callable] = None
    ) -> bytes:
        """
        Añade un usuario de forma segura usando derivación HKDF.
        
        Args:
            serial: Serial del dispositivo
            slot: Slot del usuario (1-5)
            user_id: ID único del usuario
            name: Nombre del usuario
            permissions: Permisos (0x01-0xFF)
            callback: Callback para la respuesta
        
        Returns:
            bytes: Clave derivada para entregar a la App
        """
        device = self.devices.get(serial)
        if not device:
            raise ValueError(f"Dispositivo {serial} no registrado")
        
        if not 1 <= slot <= 5:
            raise ValueError("slot debe estar entre 1 y 5")
        
        # 1. Derivar clave
        user_key = self.derive_user_key(device.master_key, serial, user_id)
        
        # 2. Guardar localmente
        user = User(
            slot=slot,
            user_id=user_id,
            name=name,
            key=user_key,
            permissions=permissions
        )
        device.users[user_id] = user
        
        # 3. Enviar comando
        self._send_command(
            serial,
            message_type=4,
            message_info={
                "action": "add_user_derived",
                "slot": slot,
                "user_id": user_id,
                "name": name,
                "permissions": permissions
            },
            callback=callback
        )
        
        logger.info(f"👤 Usuario {name} añadido al slot {slot} de {serial}")
        return user_key
    
    def remove_user(
        self,
        serial: str,
        slot: int,
        callback: Optional[Callable] = None
    ) -> str:
        """Elimina un usuario de un dispositivo."""
        return self._send_command(
            serial,
            message_type=4,
            message_info={
                "action": "remove_user",
                "slot": slot
            },
            callback=callback
        )
    
    def list_users(
        self,
        serial: str,
        callback: Optional[Callable] = None
    ) -> str:
        """Lista los usuarios de un dispositivo."""
        return self._send_command(
            serial,
            message_type=4,
            message_info={"action": "list_users"},
            callback=callback
        )
    
    def get_user_key_for_app(self, serial: str, user_id: str) -> Optional[bytes]:
        """
        Obtiene la clave de un usuario para entregarla a la App.
        """
        device = self.devices.get(serial)
        if device and user_id in device.users:
            return device.users[user_id].key
        return None
    
    # ═══════════════════════════════════════════════════════════════
    # CONTROL DE RELÉS
    # ═══════════════════════════════════════════════════════════════
    
    def activate_relay(
        self,
        serial: str,
        relay: int,
        duration_ms: int = 5000,
        callback: Optional[Callable] = None
    ) -> str:
        """Activa un relé."""
        return self._send_command(
            serial,
            message_type=1,
            message_info={
                "action": "activate",
                "relay": relay,
                "duration": duration_ms
            },
            callback=callback
        )
    
    def deactivate_relay(
        self,
        serial: str,
        relay: int,
        callback: Optional[Callable] = None
    ) -> str:
        """Desactiva un relé."""
        return self._send_command(
            serial,
            message_type=1,
            message_info={
                "action": "deactivate",
                "relay": relay
            },
            callback=callback
        )
    
    # ═══════════════════════════════════════════════════════════════
    # GESTIÓN DE CÓDIGOS
    # ═══════════════════════════════════════════════════════════════
    
    def add_local_code(
        self,
        serial: str,
        code_type: str,
        code_value: str,
        keyboard_id: int = 0,
        relay: int = 1,
        callback: Optional[Callable] = None
    ) -> str:
        """Añade un código local."""
        return self._send_command(
            serial,
            message_type=5,
            message_info={
                "action": "add_local_code",
                "code_type": code_type,
                "code_value": code_value,
                "keyboard_id": keyboard_id,
                "relay": relay
            },
            callback=callback
        )
    
    def list_local_codes(
        self,
        serial: str,
        callback: Optional[Callable] = None
    ) -> str:
        """Lista los códigos locales."""
        return self._send_command(
            serial,
            message_type=5,
            message_info={"action": "list_local_codes"},
            callback=callback
        )
    
    # ═══════════════════════════════════════════════════════════════
    # INFORMACIÓN
    # ═══════════════════════════════════════════════════════════════
    
    def get_device_info(
        self,
        serial: str,
        callback: Optional[Callable] = None
    ) -> str:
        """Obtiene información completa del dispositivo."""
        return self._send_command(
            serial,
            message_type=3,
            message_info={"action": "get_full_info"},
            callback=callback
        )
    
    # ═══════════════════════════════════════════════════════════════
    # HANDLERS
    # ═══════════════════════════════════════════════════════════════
    
    def _handle_response(self, payload: Dict):
        """Procesa respuestas de dispositivos."""
        message_id = payload.get('message_id')
        
        logger.info(f"📩 Respuesta [{message_id}]: status={payload.get('status')}")
        
        if message_id and message_id in self.pending_responses:
            callback = self.pending_responses.pop(message_id)
            callback(payload)
    
    def _handle_event(self, topic: str, payload: Dict):
        """Procesa eventos de dispositivos."""
        event_type = payload.get('event', 'UNKNOWN')
        logger.info(f"📢 Evento [{event_type}]: {payload}")
        
        # Extraer tipo de evento del tópico
        parts = topic.split('/')
        if len(parts) >= 4:
            event_category = parts[3]  # access, relay, ble, users, codes
            
            handler_key = f"on_{event_category}_{event_type.lower()}"
            if handler_key in self.event_handlers:
                self.event_handlers[handler_key](payload)
    
    def _handle_status(self, topic: str, payload: Dict):
        """Procesa estados periódicos."""
        parts = topic.split('/')
        if len(parts) >= 3:
            serial = parts[2]
            if serial in self.devices:
                self.devices[serial].info.update(payload)
    
    def on_event(self, event_name: str, handler: Callable):
        """
        Registra un handler para un tipo de evento.
        
        Ejemplo:
            backend.on_event('access_granted', my_handler)
        """
        self.event_handlers[f"on_{event_name}"] = handler


# ═══════════════════════════════════════════════════════════════════
# EJEMPLO DE USO
# ═══════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    # Crear backend
    backend = SWATIDA2Backend(
        broker="mqtt.example.com",
        port=1883,
        username="backend_user",
        password="secure_password"
    )
    
    # Registrar handlers de eventos
    def on_access_granted(event):
        print(f"🚪 Acceso concedido: {event}")
    
    def on_user_added(event):
        print(f"👤 Usuario añadido: {event}")
    
    backend.on_event("access_access_granted", on_access_granted)
    backend.on_event("users_user_added_derived", on_user_added)
    
    # Conectar
    backend.connect()
    
    # Registrar dispositivo
    master_key = bytes.fromhex(
        "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4"
        "e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2"
    )
    backend.register_device("D8F7B4BF1388", master_key)
    
    # Añadir usuario seguro
    user_key = backend.add_user_secure(
        serial="D8F7B4BF1388",
        slot=1,
        user_id="user_001",
        name="Juan García",
        permissions=0x0F
    )
    
    print(f"Clave para App: {user_key.hex()}")
    
    # Mantener corriendo
    import time
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        backend.disconnect()
```

---

## 10. Implementación Backend (Node.js)

Ver sección 6.4 para la implementación TypeScript/Node.js completa de HKDF.

---

## 11. Mejoras Requeridas

### 11.1 Cambios Obligatorios en Backend (v4.1)

| Prioridad | Mejora | Descripción |
|-----------|--------|-------------|
| **ALTA** | HKDF para usuarios | Usar `add_user_derived` en lugar de `add_user` |
| **ALTA** | Almacén de claves maestras | Base de datos segura para device_master_key |
| **ALTA** | TLS para MQTT | Configurar conexión encriptada |
| **MEDIA** | Escuchar eventos usuarios | Procesar USER_ADDED_DERIVED, USER_UNBOUND |
| **MEDIA** | Escuchar eventos acceso | Logging de ACCESS_GRANTED/DENIED |
| **BAJA** | Rate limiting | Evitar flood de comandos |

### 11.2 Arquitectura Recomendada

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            BACKEND SWATID                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐      │
│  │   API REST/WS    │    │  MQTT Handler    │    │   Key Vault      │      │
│  │   (App móvil)    │◄───│  (Dispositivos)  │───►│  (Master Keys)   │      │
│  └────────┬─────────┘    └────────┬─────────┘    └──────────────────┘      │
│           │                       │                                          │
│           ▼                       ▼                                          │
│  ┌──────────────────────────────────────────────────────────────────┐       │
│  │                      BASE DE DATOS                                │       │
│  │  - Dispositivos (serial, master_key_encrypted)                    │       │
│  │  - Usuarios (user_id, device_serial, slot, name, permissions)     │       │
│  │  - Eventos (timestamp, device, type, data)                        │       │
│  │  - Códigos (device_serial, type, value, keyboard, relay)          │       │
│  └──────────────────────────────────────────────────────────────────┘       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 11.3 Checklist de Implementación

```markdown
## Backend - Checklist v4.1

### Seguridad
- [ ] Implementar HKDF para derivación de claves
- [ ] Almacenar master_keys encriptadas en DB
- [ ] Configurar MQTT con TLS
- [ ] Implementar ACL en broker MQTT
- [ ] Rate limiting por dispositivo

### Gestión de Usuarios
- [ ] Endpoint API: POST /devices/{serial}/users (usa add_user_derived)
- [ ] Endpoint API: DELETE /devices/{serial}/users/{slot}
- [ ] Endpoint API: GET /devices/{serial}/users
- [ ] Endpoint API: GET /users/{user_id}/key (para entregar a App)

### Eventos
- [ ] Procesar USER_ADDED_DERIVED → Actualizar DB
- [ ] Procesar USER_UNBOUND → Actualizar DB
- [ ] Procesar ACCESS_GRANTED → Log de accesos
- [ ] Procesar AUTH_SUCCESS/FAILED → Alertas de seguridad

### Monitorización
- [ ] Dashboard de dispositivos online
- [ ] Alertas de dispositivos offline
- [ ] Logs de accesos y eventos
- [ ] Métricas de uso
```

---

## Apéndice A: Referencia Rápida de Comandos

| Acción | message_type | action | Parámetros |
|--------|--------------|--------|------------|
| Activar relé | 1 | activate | relay, duration |
| Desactivar relé | 1 | deactivate | relay |
| Toggle relé | 1 | toggle | relay |
| Cambiar modo | 2 | set_mode | mode |
| Info completa | 3 | get_full_info | - |
| Añadir usuario seguro | 4 | add_user_derived | slot, user_id, name, permissions |
| Eliminar usuario | 4 | remove_user | slot |
| Listar usuarios | 4 | list_users | - |
| Añadir código local | 5 | add_local_code | code_type, code_value, keyboard_id, relay |
| Eliminar código local | 5 | remove_local_code | code_type, code_value |
| Listar códigos | 5 | list_local_codes | - |
| Añadir código remoto | 5 | add_remote_code | + time_slots |

---

**Documento creado:** 5 Febrero 2026  
**Autor:** Equipo SWATID  
**Versión:** 1.0
