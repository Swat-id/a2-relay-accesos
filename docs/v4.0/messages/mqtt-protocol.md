# Protocolo MQTT - SWATID-A2 v4.0

## Descripción General

El sistema SWATID-A2 implementa un protocolo MQTT completo para comunicación bidireccional con servidores remotos, permitiendo control remoto, monitoreo, gestión del sistema de control de acceso dual Wiegand y gestión de vinculaciones BLE.

---

## Configuración del Broker MQTT

### Parámetros de Conexión

```
┌─────────────────────────────────────────────────────────────────┐
│                    CONFIGURACIÓN BROKER MQTT                     │
├─────────────────────────────────────────────────────────────────┤
│  Servidor:     188.245.213.181                                  │
│  Puerto:       1883 (TCP sin TLS)                               │
│  Usuario:      swatidhome                                       │
│  Contraseña:   Swatid2025!                                      │
│  Protocolo:    MQTT 3.1.1                                       │
└─────────────────────────────────────────────────────────────────┘
```

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| **Host/IP** | `188.245.213.181` | Dirección IP del broker |
| **Puerto** | `1883` | Puerto TCP estándar MQTT |
| **Usuario** | `swatidhome` | Usuario de autenticación |
| **Contraseña** | `Swatid2025!` | Contraseña de autenticación |
| **Protocolo** | MQTT 3.1.1 | Versión del protocolo |
| **QoS** | 0 (At Most Once) | Nivel de calidad de servicio |
| **Keepalive** | 60 segundos | Intervalo de ping |
| **Buffer** | 2048 bytes | Tamaño máximo de mensaje |
| **Clean Session** | true | Sesión limpia al conectar |

### Configuración en el Firmware

```cpp
// Definido en src/main.ino
const char* mqtt_broker = "188.245.213.181";
const int mqtt_port = 1883;
const char* mqtt_username = "swatidhome";
const char* mqtt_password = "Swatid2025!";
```

### Reconexión Automática

| Parámetro | Valor |
|-----------|-------|
| Intervalo de reconexión | 5 segundos |
| Intentos máximos | Ilimitados |
| Backoff exponencial | No implementado |

---

## Conexión desde Clientes Externos

### Mosquitto (CLI)

```bash
# Suscribirse a todos los mensajes de un dispositivo
mosquitto_sub -h 188.245.213.181 -p 1883 \
  -u swatidhome -P "Swatid2025!" \
  -t "swatidhome/+/SWATID_XXXXXXXXXXXX/#" -v

# Enviar comando de activación de relé
mosquitto_pub -h 188.245.213.181 -p 1883 \
  -u swatidhome -P "Swatid2025!" \
  -t "swatidhome/command/SWATID_XXXXXXXXXXXX/relay" \
  -m '{"device":"SWATID_XXXXXXXXXXXX","message_type":0,"message_id":1,"message_info":{"relay_number":1,"duration":2.0}}'

# Solicitar información del dispositivo
mosquitto_pub -h 188.245.213.181 -p 1883 \
  -u swatidhome -P "Swatid2025!" \
  -t "swatidhome/command/SWATID_XXXXXXXXXXXX/info" \
  -m '{"device":"SWATID_XXXXXXXXXXXX","message_type":1,"message_id":2}'
```

### MQTT Explorer / MQTT.fx

```yaml
Configuración de conexión:
  Host: 188.245.213.181
  Port: 1883
  Username: swatidhome
  Password: Swatid2025!
  Client ID: MQTTExplorer_[random]
  Protocol: MQTT 3.1.1
  Keep Alive: 60
  Clean Session: true
  SSL/TLS: Disabled
```

### Python (paho-mqtt)

```python
import paho.mqtt.client as mqtt
import json

# Configuración del broker
BROKER_HOST = "188.245.213.181"
BROKER_PORT = 1883
BROKER_USER = "swatidhome"
BROKER_PASS = "Swatid2025!"
DEVICE_SERIAL = "SWATID_XXXXXXXXXXXX"

# Callbacks
def on_connect(client, userdata, flags, rc):
    print(f"Conectado con código: {rc}")
    # Suscribirse a respuestas del dispositivo
    client.subscribe(f"swatidhome/response/{DEVICE_SERIAL}/rx")
    client.subscribe(f"swatidhome/events/{DEVICE_SERIAL}/#")
    client.subscribe(f"swatidhome/{DEVICE_SERIAL}/info")

def on_message(client, userdata, msg):
    print(f"Topic: {msg.topic}")
    print(f"Mensaje: {msg.payload.decode()}")

# Crear cliente
client = mqtt.Client(client_id="PythonClient_001")
client.username_pw_set(BROKER_USER, BROKER_PASS)
client.on_connect = on_connect
client.on_message = on_message

# Conectar
client.connect(BROKER_HOST, BROKER_PORT, 60)

# Enviar comando (activar relé 1 por 3 segundos)
comando = {
    "device": DEVICE_SERIAL,
    "message_type": 0,
    "message_id": 12345,
    "message_info": {
        "relay_number": 1,
        "duration": 3.0
    }
}
client.publish(
    f"swatidhome/command/{DEVICE_SERIAL}/relay",
    json.dumps(comando)
)

# Loop infinito para recibir mensajes
client.loop_forever()
```

### Node.js (mqtt)

```javascript
const mqtt = require('mqtt');

// Configuración
const BROKER_URL = 'mqtt://188.245.213.181:1883';
const OPTIONS = {
    username: 'swatidhome',
    password: 'Swatid2025!',
    clientId: 'NodeJS_Client_001',
    clean: true,
    keepalive: 60
};
const DEVICE_SERIAL = 'SWATID_XXXXXXXXXXXX';

// Conectar
const client = mqtt.connect(BROKER_URL, OPTIONS);

client.on('connect', () => {
    console.log('Conectado al broker MQTT');
    
    // Suscribirse
    client.subscribe(`swatidhome/response/${DEVICE_SERIAL}/rx`);
    client.subscribe(`swatidhome/events/${DEVICE_SERIAL}/#`);
    
    // Enviar comando
    const comando = {
        device: DEVICE_SERIAL,
        message_type: 1,  // Solicitar información
        message_id: Date.now()
    };
    
    client.publish(
        `swatidhome/command/${DEVICE_SERIAL}/info`,
        JSON.stringify(comando)
    );
});

client.on('message', (topic, message) => {
    console.log(`Topic: ${topic}`);
    console.log(`Mensaje: ${message.toString()}`);
});
```

### Home Assistant (YAML)

```yaml
# configuration.yaml
mqtt:
  broker: 188.245.213.181
  port: 1883
  username: swatidhome
  password: Swatid2025!

# Sensor para estado del dispositivo
sensor:
  - platform: mqtt
    name: "SWATID Estado"
    state_topic: "swatidhome/SWATID_XXXXXXXXXXXX/info"
    value_template: "{{ value_json.firmware_version }}"
    json_attributes_topic: "swatidhome/SWATID_XXXXXXXXXXXX/info"

# Switch para relé 1
switch:
  - platform: mqtt
    name: "SWATID Relé 1"
    command_topic: "swatidhome/command/SWATID_XXXXXXXXXXXX/relay"
    payload_on: '{"device":"SWATID_XXXXXXXXXXXX","message_type":0,"message_id":1,"message_info":{"relay_number":1,"duration":2.0}}'
    payload_off: '{"device":"SWATID_XXXXXXXXXXXX","message_type":0,"message_id":2,"message_info":{"relay_number":1,"duration":0.1}}'
```

---

## Arquitectura de Comunicación

```
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│   Backend/APP   │       │  Broker MQTT    │       │   KC868-A2      │
│                 │       │                 │       │   (ESP32)       │
└────────┬────────┘       └────────┬────────┘       └────────┬────────┘
         │                         │                         │
         │  1. Publicar comando    │                         │
         │────────────────────────>│                         │
         │                         │  2. Entregar comando    │
         │                         │────────────────────────>│
         │                         │                         │
         │                         │  3. Publicar respuesta  │
         │                         │<────────────────────────│
         │  4. Entregar respuesta  │                         │
         │<────────────────────────│                         │
         │                         │                         │
         │                         │  5. Publicar eventos    │
         │                         │<────────────────────────│
         │  6. Entregar eventos    │                         │
         │<────────────────────────│                         │
```

---

## Identificación del Dispositivo

| Campo | Formato | Ejemplo | Descripción |
|-------|---------|---------|-------------|
| **Client ID** | `ESP32Client-[HEX]` | ESP32Client-A1B2C3 | ID único de conexión MQTT |
| **Serial Fijo** | `SWATID_[MAC]` | SWATID_B4F7D88813BF | Identificador permanente |
| **Nombre** | Configurable | KC868A2_Entrada | Nombre amigable editable |

### Generación del Serial

El serial se genera a partir de la MAC del ESP32:

```cpp
String generateFixedSerial() {
    uint64_t chipid = ESP.getEfuseMac();
    char buf[32];
    snprintf(buf, sizeof(buf), "SWATID_%04X%08X", 
             (uint16_t)(chipid>>32), (uint32_t)chipid);
    return String(buf);
}
```

---

## Estructura de Tópicos

### Tópicos de Comando (Dispositivo ← Backend)

```
swatidhome/command/[SERIAL]/#
```

| Tópico | Descripción |
|--------|-------------|
| `swatidhome/command/[SERIAL]/relay` | Control de relés |
| `swatidhome/command/[SERIAL]/info` | Solicitud de información |
| `swatidhome/command/[SERIAL]/system` | Comandos de sistema |
| `swatidhome/command/[SERIAL]/security` | Comandos de seguridad |
| `swatidhome/command/[SERIAL]/access` | Validación de acceso |
| `swatidhome/command/[SERIAL]/granted` | Respuesta de validación |

### Tópicos de Publicación (Dispositivo → Backend)

| Tópico | Descripción |
|--------|-------------|
| `swatidhome/[SERIAL]/info` | Información del dispositivo |
| `swatidhome/[SERIAL]/relay_enable` | Estado de relés |
| `swatidhome/errors/[SERIAL]/rx` | Errores del sistema |
| `swatidhome/response/[SERIAL]/rx` | Respuestas a comandos |
| `swatidhome/events/[SERIAL]/access` | Eventos de acceso |
| `swatidhome/events/[SERIAL]/failed_access` | Accesos fallidos |
| `swatidhome/events/[SERIAL]/ble` | Eventos BLE |
| `swatidhome/keepalive/[SERIAL]` | Keepalive periódico |

---

## Tipos de Mensaje (message_type)

| Tipo | Descripción |
|------|-------------|
| 0 | Activación de relé |
| 1 | Solicitud de información |
| 2 | Comandos de sistema |
| 3 | Comandos de seguridad |
| 4 | Sincronización de tiempo |
| 5 | Gestión de códigos remotos |
| **6** | **Gestión de vinculaciones BLE** (v4.0) |

---

## Mensaje Tipo 0: Activación de Relé

### Solicitud

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 0,
  "message_id": 12345,
  "message_info": {
    "relay_number": 1,
    "duration": 5.0
  }
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| device | string | Serial del dispositivo |
| message_type | int | 0 |
| message_id | int | ID único del mensaje |
| relay_number | int | Relé a activar (1 o 2) |
| duration | float | Duración en segundos (mín. 0.5) |

### Respuesta

```json
{
  "timestamp": "2026-02-05 12:30:00",
  "response_id": 1,
  "message_id": 12345,
  "device": "KC868A2",
  "serial": "SWATID_XXXXXXXXXXXX",
  "response_type": 0,
  "response_info": "relay 1 activated",
  "relay": 1
}
```

---

## Mensaje Tipo 1: Solicitud de Información

### Solicitud

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 1,
  "message_id": 12346
}
```

### Respuesta

```json
{
  "device": "KC868A2",
  "serial": "SWATID_XXXXXXXXXXXX",
  "ip": "192.168.1.100",
  "mac": "88:13:BF:B4:F7:D8",
  "wifi_signal": -1,
  "firmware_version": "v4.0.0",
  "use_dhcp": true,
  "relay1_state": false,
  "relay2_state": false,
  "relay_duration": 2.0,
  "turnstile_mode": false,
  "local_access_blocked": false,
  "keyboard_reading_enabled": true,
  "free_heap": 150000,
  "uptime_seconds": 3600,
  "local_codes": {
    "count": 25,
    "max_capacity": 50,
    "available": 25
  },
  "remote_codes": {
    "count": 10,
    "max_capacity": 40,
    "available": 30
  },
  "digital_inputs": {
    "di1": {
      "enabled": true,
      "relay": 1,
      "inverse": false,
      "duration_ms": 2000
    },
    "di2": {
      "enabled": false,
      "relay": 2,
      "inverse": false,
      "duration_ms": 2000
    }
  }
}
```

---

## Mensaje Tipo 2: Comandos de Sistema

### Reiniciar Dispositivo

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 2,
  "message_id": 12347,
  "message_info": "reboot"
}
```

### Resetear a Valores por Defecto

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 2,
  "message_id": 12348,
  "message_info": "reset"
}
```

### Actualizar Configuración

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 2,
  "message_id": 12349,
  "message_info": "update",
  "config": {
    "device_name": "Puerta Principal",
    "relay_duration": 3.0,
    "use_dhcp": false,
    "ip": "192.168.1.50",
    "gateway": "192.168.1.1",
    "subnet": "255.255.255.0",
    "dns": "8.8.8.8"
  }
}
```

### Configurar OTA

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 2,
  "message_id": 12350,
  "message_info": "ota_config",
  "ota": {
    "update_url": "http://server.com/firmware/",
    "auto_update_enabled": true,
    "check_interval": 24
  }
}
```

### Verificar Actualizaciones OTA

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 2,
  "message_id": 12351,
  "message_info": "ota_check"
}
```

---

## Mensaje Tipo 3: Comandos de Seguridad

### Bloquear Acceso Local

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 3,
  "message_id": 12352,
  "message_info": {
    "security_command": "block_local_access"
  }
}
```

### Desbloquear Acceso Local

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 3,
  "message_id": 12353,
  "message_info": {
    "security_command": "unblock_local_access"
  }
}
```

### Deshabilitar Lectura de Teclados

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 3,
  "message_id": 12354,
  "message_info": {
    "security_command": "disable_keyboard_reading"
  }
}
```

### Habilitar Lectura de Teclados

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 3,
  "message_id": 12355,
  "message_info": {
    "security_command": "enable_keyboard_reading"
  }
}
```

### Configurar Duración de Bloqueo

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 3,
  "message_id": 12356,
  "message_info": {
    "security_command": "set_block_duration",
    "duration_seconds": 120
  }
}
```

### Configurar Máximo Intentos Fallidos

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 3,
  "message_id": 12357,
  "message_info": {
    "security_command": "set_max_failed_attempts",
    "max_attempts": 5
  }
}
```

---

## Mensaje Tipo 4: Sincronización de Tiempo

### Solicitud

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 4,
  "message_id": 12358,
  "message_info": {
    "time_string": "2026-02-05 14:30:00"
  }
}
```

### Respuesta

```json
{
  "timestamp": "2026-02-05 14:30:00",
  "response_id": 10,
  "message_id": 12358,
  "response_type": 0,
  "response_info": "time synchronized to 2026-02-05 14:30:00"
}
```

---

## Mensaje Tipo 5: Gestión de Códigos Remotos

### Añadir Código Remoto

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 5,
  "message_id": 12359,
  "message_info": {
    "action": "add_remote_code",
    "code_type": "PIN",
    "code_value": "1234",
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

| Campo | Tipo | Descripción |
|-------|------|-------------|
| code_type | string | "PIN" o "TAG" |
| code_value | string | Valor del código |
| keyboard_id | int | 0=ambos, 1=teclado1, 2=teclado2 |
| relay | int | Relé a activar (1 o 2) |
| time_slots | array | Franjas horarias (opcional) |
| days_of_week | int | Bitmask: 1=Lun, 2=Mar, 4=Mié, 8=Jue, 16=Vie, 32=Sáb, 64=Dom |

### Eliminar Código Remoto

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 5,
  "message_id": 12360,
  "message_info": {
    "action": "delete_remote_code",
    "code_type": "PIN",
    "code_value": "1234"
  }
}
```

### Eliminar Todos los Códigos Remotos

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 5,
  "message_id": 12361,
  "message_info": {
    "action": "clear_remote_codes"
  }
}
```

---

## Mensaje Tipo 6: Gestión de Vinculaciones BLE (v4.0)

### Limpiar Todas las Vinculaciones

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 6,
  "message_id": 12362,
  "message_info": {
    "action": "clear_all"
  }
}
```

**Respuesta:**
```json
{
  "response_type": 0,
  "response_info": "all BLE bindings cleared"
}
```

### Limpiar Superadmin

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 6,
  "message_id": 12363,
  "message_info": {
    "action": "clear_superadmin"
  }
}
```

**Respuesta:**
```json
{
  "response_type": 0,
  "response_info": "superadmin BLE binding cleared"
}
```

### Limpiar Usuario Específico

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 6,
  "message_id": 12364,
  "message_info": {
    "action": "clear_user",
    "slot": 1
  }
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| slot | int | Número de usuario (1-5) |

**Respuesta:**
```json
{
  "response_type": 0,
  "response_info": "BLE user 1 cleared"
}
```

### Obtener Estado BLE

```json
{
  "device": "SWATID_XXXXXXXXXXXX",
  "message_type": 6,
  "message_id": 12365,
  "message_info": {
    "action": "get_status"
  }
}
```

**Respuesta:**
```json
{
  "response_type": 0,
  "response_info": "{\"name\":\"SWATID_XXXX\",\"connected\":false,\"authenticated\":false,\"connected_user\":\"\",\"superadmin_registered\":true,\"active_users\":2}"
}
```

---

## Eventos Publicados

### Evento de Acceso

**Tópico:** `swatidhome/events/[SERIAL]/access`

```json
{
  "timestamp": "2026-02-05 14:35:00",
  "message_id": 100,
  "device": "KC868A2",
  "serial": "SWATID_XXXXXXXXXXXX",
  "event_type": "ACCESS_EVENT",
  "success": true,
  "source": "LOCAL",
  "code_type": "PIN",
  "code_value": "1234",
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1"
}
```

| source | Descripción |
|--------|-------------|
| LOCAL | Validación local |
| REMOTE | Validación remota |
| WEB | Apertura desde interfaz web |
| MQTT_RELAY | Apertura desde comando MQTT |

### Evento de Acceso Fallido

**Tópico:** `swatidhome/events/[SERIAL]/failed_access`

```json
{
  "timestamp": "2026-02-05 14:36:00",
  "message_id": 101,
  "device": "KC868A2",
  "serial": "SWATID_XXXXXXXXXXXX",
  "event_type": "FAILED_ACCESS",
  "code_type": "PIN",
  "code_value": "9999",
  "reason": "INVALID_CODE",
  "failed_attempts": 1,
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1"
}
```

| reason | Descripción |
|--------|-------------|
| INVALID_CODE | Código no válido |
| BLOCKED | Acceso bloqueado |
| REMOTE_DENIED | Rechazado por servidor |

### Evento BLE - Desvinculación

**Tópico:** `swatidhome/events/[SERIAL]/ble`

Estos eventos se publican cuando se eliminan vinculaciones BLE, tanto desde la **web** como desde **MQTT**, permitiendo monitorización remota.

#### Evento: Todas las vinculaciones eliminadas

```json
{
  "event": "BINDINGS_CLEARED",
  "type": "ALL",
  "source": "WEB",
  "device": "SWATID_XXXXXXXXXXXX",
  "details": {
    "superadmin_cleared": true,
    "users_cleared": 5
  },
  "timestamp": "2026-02-05 14:37:00"
}
```

#### Evento: Superadmin eliminado

```json
{
  "event": "BINDINGS_CLEARED",
  "type": "SUPERADMIN",
  "source": "MQTT",
  "device": "SWATID_XXXXXXXXXXXX",
  "message_id": 12345,
  "details": {
    "action": "superadmin_removed",
    "awaiting_new_superadmin": true
  },
  "timestamp": "2026-02-05 14:37:00"
}
```

#### Evento: Usuario eliminado

```json
{
  "event": "BINDINGS_CLEARED",
  "type": "USER",
  "source": "WEB",
  "device": "SWATID_XXXXXXXXXXXX",
  "details": {
    "slot": 2,
    "user_name": "Tecnico",
    "previous_permissions": "0x9"
  },
  "timestamp": "2026-02-05 14:37:00"
}
```

#### Campos del Evento

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `event` | string | Tipo de evento: `BINDINGS_CLEARED` |
| `type` | string | `ALL`, `SUPERADMIN`, `USER` |
| `source` | string | Origen: `WEB` o `MQTT` |
| `device` | string | Serial del dispositivo |
| `message_id` | int | ID del mensaje MQTT (solo si source=MQTT) |
| `details` | object | Detalles específicos del evento |
| `timestamp` | string | Fecha/hora del evento |

#### Detalles por Tipo

| type | Campo details | Descripción |
|------|---------------|-------------|
| ALL | `superadmin_cleared` | Siempre `true` |
| ALL | `users_cleared` | Número de slots (5) |
| SUPERADMIN | `action` | `superadmin_removed` |
| SUPERADMIN | `awaiting_new_superadmin` | `true` (dispositivo espera nueva vinculación) |
| USER | `slot` | Número de slot eliminado (1-5) |
| USER | `user_name` | Nombre del usuario eliminado |
| USER | `previous_permissions` | Permisos que tenía (hex) |

#### Uso para Monitorización

```python
# Ejemplo: Suscribirse a eventos BLE
client.subscribe("swatidhome/events/+/ble")

def on_message(client, userdata, msg):
    event = json.loads(msg.payload)
    
    if event["event"] == "BINDINGS_CLEARED":
        device = event["device"]
        source = event["source"]
        event_type = event["type"]
        
        if event_type == "ALL":
            print(f"⚠️ {device}: TODAS las vinculaciones eliminadas desde {source}")
            # Notificar al administrador
            
        elif event_type == "SUPERADMIN":
            print(f"⚠️ {device}: Superadmin eliminado desde {source}")
            # Marcar dispositivo como pendiente de reconfiguración
            
        elif event_type == "USER":
            slot = event["details"]["slot"]
            name = event["details"]["user_name"]
            print(f"ℹ️ {device}: Usuario {name} (slot {slot}) eliminado desde {source}")
            # Actualizar base de datos de usuarios
```

### Estado de Relé

**Tópico:** `swatidhome/[SERIAL]/relay_enable`

```json
{
  "timestamp": "2026-02-05 14:38:00",
  "resultado": "OK",
  "rele": 1,
  "estado": "ON",
  "origen": "SYSTEM",
  "message_id": "102",
  "request_id": "50"
}
```

### Keepalive

**Tópico:** `swatidhome/keepalive/[SERIAL]`

```json
{
  "status": "online",
  "uptime": 3600000
}
```

---

## Códigos de Respuesta

| response_type | Descripción |
|---------------|-------------|
| 0 | Éxito |
| 1 | Error |

---

## Errores Comunes

| Código | Descripción |
|--------|-------------|
| 1 | Parámetros faltantes |
| 2 | Parámetros inválidos |
| 3 | Operación no permitida |
| 4 | Error interno |
| 8 | Memoria baja |

---

## Integración con BLE

El protocolo MQTT complementa al BLE permitiendo:

1. **Gestión remota de vinculaciones BLE** - Eliminar usuarios sin acceso físico
2. **Monitoreo de eventos BLE** - Recibir notificaciones de cambios
3. **Backup de configuración** - El backend puede mantener registro de usuarios

### Comparativa de Acciones

| Acción | BLE | MQTT | Web |
|--------|-----|------|-----|
| Activar relé | ✅ | ✅ | ✅ |
| Cambiar modo | ✅ | ❌ | ✅ |
| Añadir código | ✅ | ✅ | ✅ |
| Config red | ✅ | ✅ | ✅ |
| Limpiar vinculaciones BLE | ❌ | ✅ | ✅ |
| Gestionar usuarios BLE | ✅* | ❌ | ✅ |

*Solo superadmin vía BLE puede añadir usuarios

---

## Seguridad

### Recomendaciones

1. Usar conexión TLS si el broker lo soporta
2. Cambiar credenciales por defecto en producción
3. Monitorizar eventos de seguridad
4. Implementar rate limiting en el backend

### Limitaciones Actuales

- Sin encriptación TLS (puerto 1883)
- Credenciales fijas en firmware
- Sin autenticación por dispositivo individual

---

## Actualización Remota de Firmware (OTA)

El sistema SWATID-A2 soporta actualización de firmware de dos formas:

1. **Actualización Remota** - Comprobación de versión en servidor y descarga automática
2. **Actualización Manual** - Subida de archivo .bin desde la interfaz web

### Servidor de Firmware

```
┌─────────────────────────────────────────────────────────────────┐
│                 SERVIDOR DE FIRMWARE SWAT-ID                     │
├─────────────────────────────────────────────────────────────────┤
│  URL Base:       https://devices.swat-id.com                    │
│  Autenticación:  No requerida (endpoints públicos)              │
│  Protocolo:      HTTPS                                          │
│  Tipo Device:    SWATID-A2                                      │
└─────────────────────────────────────────────────────────────────┘
```

### Endpoints de Firmware

| Acción | Método | Endpoint |
|--------|--------|----------|
| Consultar versión | GET | `/api/v1/firmware/check` |
| Descargar .bin | GET | `/api/v1/firmware/download/{version_id}` |

---

### Endpoint 1: Consultar Versión Disponible (Check)

Comprueba si existe una versión de firmware más nueva que la instalada.

#### Request

| Campo | Valor |
|-------|-------|
| **Método** | GET |
| **URL** | `https://devices.swat-id.com/api/v1/firmware/check` |
| **Autenticación** | No requerida (público) |

#### Parámetros Query

| Parámetro | Tipo | Requerido | Descripción |
|-----------|------|-----------|-------------|
| `device_type` | string | **Sí** | Tipo de dispositivo: `SWATID-A2` |
| `current_version` | string | No | Versión actual del firmware instalado |
| `device_id` | string | No | UUID del dispositivo (para registro) |

#### Ejemplos de URL

```bash
# Mínimo (solo tipo de dispositivo)
GET https://devices.swat-id.com/api/v1/firmware/check?device_type=SWATID-A2

# Con versión actual (para saber si hay actualización)
GET https://devices.swat-id.com/api/v1/firmware/check?device_type=SWATID-A2&current_version=4.0.0

# Completo (con registro de dispositivo)
GET https://devices.swat-id.com/api/v1/firmware/check?device_type=SWATID-A2&device_id=<uuid>&current_version=4.0.0
```

#### Respuesta: No Hay Actualizaciones

Cuando no hay versión publicada o el dispositivo ya está actualizado:

```json
{
  "device_type": "SWATID-A2",
  "current_version": null,
  "newer": false,
  "download_url": ""
}
```

**Interpretación:**

| Campo | Valor | Significado |
|-------|-------|-------------|
| `current_version` | `null` | No existe versión publicada |
| `newer` | `false` | No hay actualización disponible |
| `download_url` | `""` | No hay URL de descarga |

**Regla de validación:**
```
IF current_version === null AND newer === false AND download_url === ""
THEN → No hay actualizaciones; NO intentar descargar
```

#### Respuesta: Hay Actualización Disponible

Cuando existe una versión más nueva:

```json
{
  "device_type": "SWATID-A2",
  "current_version": "4.1.0",
  "version_id": "550e8400-e29b-41d4-a716-446655440000",
  "newer": true,
  "download_url": "/api/v1/firmware/download/550e8400-e29b-41d4-a716-446655440000",
  "file_size": 1456789
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `device_type` | string | Tipo de dispositivo (eco del query) |
| `current_version` | string | Versión más reciente publicada |
| `version_id` | string | UUID de la versión (para descarga) |
| `newer` | boolean | `true` = hay actualización disponible |
| `download_url` | string | Ruta relativa para descargar el .bin |
| `file_size` | number | Tamaño del archivo en bytes |

**URL completa de descarga:**
```
https://devices.swat-id.com + download_url
```

**Ejemplo:**
```
https://devices.swat-id.com/api/v1/firmware/download/550e8400-e29b-41d4-a716-446655440000
```

#### Errores del Endpoint Check

| Código HTTP | Body | Causa |
|-------------|------|-------|
| 400 | `{"error": "device_type is required"}` | Falta el parámetro `device_type` |
| 403 | `{"error": "no access to this device"}` | Sin acceso al `device_id` especificado |
| 500 | `{"error": "..."}` | Error interno del servidor |

---

### Endpoint 2: Descargar Firmware (Download)

Descarga el archivo binario de una versión específica de firmware.

#### Request

| Campo | Valor |
|-------|-------|
| **Método** | GET |
| **URL** | `https://devices.swat-id.com/api/v1/firmware/download/{version_id}` |
| **Autenticación** | No requerida (público) |

#### Parámetros de Ruta

| Parámetro | Descripción |
|-----------|-------------|
| `version_id` | UUID de la versión (obtenido del endpoint check) |

#### Parámetros Query (Opcionales)

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `device_id` | string | UUID del dispositivo (para registrar descarga) |

#### Ejemplos de URL

```bash
# Sin registrar descarga
GET https://devices.swat-id.com/api/v1/firmware/download/550e8400-e29b-41d4-a716-446655440000

# Registrando descarga para dispositivo
GET https://devices.swat-id.com/api/v1/firmware/download/550e8400-e29b-41d4-a716-446655440000?device_id=<uuid>
```

#### Respuesta Exitosa (200)

| Header | Valor |
|--------|-------|
| `Content-Type` | `application/octet-stream` |
| `Content-Disposition` | `attachment; filename="firmware.bin"` |
| `Content-Length` | Tamaño en bytes |

**Body:** Contenido binario del archivo .bin

#### Errores del Endpoint Download

| Código HTTP | Body | Causa |
|-------------|------|-------|
| 400 | `{"error": "version_id is required"}` | Falta UUID en la ruta |
| 404 | `{"error": "firmware not found"}` | No existe esa versión |
| 404 | `{"error": "firmware file not found"}` | Versión existe pero .bin no está en disco |
| 500 | `{"error": "..."}` | Error interno |

---

### Flujo de Actualización desde Web

```
┌─────────────────┐      ┌─────────────────┐      ┌─────────────────┐
│   Usuario Web   │      │    KC868-A2     │      │ Servidor SWATID │
│   (Navegador)   │      │    (ESP32)      │      │   devices.      │
└────────┬────────┘      └────────┬────────┘      └────────┬────────┘
         │                        │                        │
         │  1. Clic "Comprobar    │                        │
         │     Actualización"     │                        │
         │───────────────────────>│                        │
         │                        │                        │
         │                        │  2. GET /check         │
         │                        │     ?device_type=      │
         │                        │      SWATID-A2         │
         │                        │     &current_version=  │
         │                        │      4.0.0             │
         │                        │───────────────────────>│
         │                        │                        │
         │                        │  3. JSON Response      │
         │                        │     newer=true         │
         │                        │<───────────────────────│
         │                        │                        │
         │  4. Mostrar: "Nueva    │                        │
         │     versión 4.1.0      │                        │
         │     disponible"        │                        │
         │<───────────────────────│                        │
         │                        │                        │
         │  5. Clic "Descargar    │                        │
         │     e Instalar"        │                        │
         │───────────────────────>│                        │
         │                        │                        │
         │                        │  6. GET /download/     │
         │                        │     {version_id}       │
         │                        │───────────────────────>│
         │                        │                        │
         │                        │  7. Binary .bin        │
         │                        │<───────────────────────│
         │                        │                        │
         │                        │  8. Aplicar OTA        │
         │                        │  9. Reiniciar          │
         │                        │                        │
         │  10. "Actualización    │                        │
         │       completada"      │                        │
         │<───────────────────────│                        │
```

### Opciones de Actualización en la Web

La página de configuración del dispositivo ofrece dos métodos:

#### Método 1: Actualización Remota (Recomendado)

| Elemento | Función |
|----------|---------|
| **Botón "Comprobar Actualización"** | Consulta el servidor para verificar si hay nueva versión |
| **Información mostrada** | Versión actual vs versión disponible, tamaño del archivo |
| **Botón "Descargar e Instalar"** | Descarga el .bin y aplica OTA (requiere aprobación del usuario) |

#### Método 2: Actualización Manual

| Elemento | Función |
|----------|---------|
| **Campo de selección de archivo** | Permite seleccionar un .bin local |
| **Botón "Subir Firmware"** | Sube el archivo al dispositivo e inicia OTA |

### Implementación en el Firmware

```cpp
// Constantes para servidor de firmware
const char* FIRMWARE_SERVER = "https://devices.swat-id.com";
const char* FIRMWARE_CHECK_ENDPOINT = "/api/v1/firmware/check";
const char* DEVICE_TYPE = "SWATID-A2";

// Función para comprobar actualización
void checkFirmwareUpdate() {
    HTTPClient http;
    String url = String(FIRMWARE_SERVER) + FIRMWARE_CHECK_ENDPOINT;
    url += "?device_type=" + String(DEVICE_TYPE);
    url += "&current_version=" + String(FIRMWARE_VERSION);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        // Parsear JSON y verificar campo "newer"
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        
        bool newer = doc["newer"];
        if (newer) {
            String downloadUrl = doc["download_url"].as<String>();
            int fileSize = doc["file_size"];
            // Notificar al usuario que hay actualización
        }
    }
    http.end();
}

// Función para descargar e instalar
void downloadAndInstallFirmware(String versionId) {
    String url = String(FIRMWARE_SERVER) + "/api/v1/firmware/download/" + versionId;
    
    // Usar Update.h de ESP32 para OTA
    httpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = httpUpdate.update(url);
    
    // Manejar resultado
}
```

---

### Configuración CORS

Para que la actualización funcione desde la página web servida por el dispositivo (ej. `http://192.168.1.100`), el servidor debe permitir CORS.

#### Configuración en el Servidor Backend

Variable de entorno en el servidor SWAT-ID:

```bash
# Un origen específico
CORS_ORIGINS=http://192.168.1.100

# Múltiples orígenes
CORS_ORIGINS=http://192.168.1.100,http://192.168.4.1,http://10.0.0.1

# Cualquier origen (solo si es aceptable)
CORS_ORIGINS=*
```

Reiniciar el servicio:
```bash
sudo systemctl restart swatid-device-manager
```

#### Comandos de Validación CORS

**1. Verificar headers CORS en endpoint check:**

```bash
curl -s -D - -o /dev/null \
  -H "Origin: http://192.168.1.100" \
  "https://devices.swat-id.com/api/v1/firmware/check?device_type=SWATID-A2"
```

Buscar en respuesta:
```
Access-Control-Allow-Origin: http://192.168.1.100
```

**2. Verificar preflight OPTIONS:**

```bash
curl -s -D - -o /dev/null -X OPTIONS \
  -H "Origin: http://192.168.1.100" \
  -H "Access-Control-Request-Method: GET" \
  "https://devices.swat-id.com/api/v1/firmware/check?device_type=SWATID-A2"
```

Esperado: Estado **204** con headers `Access-Control-Allow-Origin` y `Access-Control-Allow-Methods`.

**3. Verificar CORS en endpoint download:**

```bash
curl -s -D - -o /dev/null \
  -H "Origin: http://192.168.1.100" \
  "https://devices.swat-id.com/api/v1/firmware/download/<VERSION_ID>"
```

**4. Ver solo headers CORS:**

```bash
curl -s -D - -o /dev/null -H "Origin: http://192.168.1.100" \
  "https://devices.swat-id.com/api/v1/firmware/check?device_type=SWATID-A2" \
  | grep -i "Access-Control"
```

#### Si CORS No Está Configurado

Si el navegador bloquea la petición por CORS, el usuario puede:

1. **Descargar el .bin manualmente** desde otro navegador o equipo
2. **Usar la opción "Subir fichero .bin"** para actualizar manualmente

---

### Resumen de Integración OTA

| Acción | Endpoint | Método | URL |
|--------|----------|--------|-----|
| Consultar versión | check | GET | `https://devices.swat-id.com/api/v1/firmware/check?device_type=SWATID-A2` |
| Descargar .bin | download | GET | `https://devices.swat-id.com/api/v1/firmware/download/{version_id}` |

**Lógica de decisión:**

```
┌────────────────────────────────────────────────────────────────┐
│                    FLUJO DE ACTUALIZACIÓN                       │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. Llamar /check con device_type=SWATID-A2                    │
│                     &current_version=X.X.X                      │
│                           │                                     │
│                           ▼                                     │
│              ┌────────────────────────┐                         │
│              │  newer === true ?      │                         │
│              └───────────┬────────────┘                         │
│                   │              │                              │
│                  YES            NO                              │
│                   │              │                              │
│                   ▼              ▼                              │
│          Hay actualización   Ya actualizado                    │
│          Mostrar botón       Mostrar "Al día"                  │
│          "Instalar v4.1.0"                                      │
│                   │                                             │
│                   ▼                                             │
│           Usuario aprueba                                       │
│                   │                                             │
│                   ▼                                             │
│          Llamar /download/{version_id}                         │
│                   │                                             │
│                   ▼                                             │
│          Recibir .bin y aplicar OTA                            │
│                   │                                             │
│                   ▼                                             │
│          Reiniciar dispositivo                                  │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

---

*Documentación generada para SWATID-A2 v4.0*
*Última actualización: Febrero 2026*
