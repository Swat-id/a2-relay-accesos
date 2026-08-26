# Tipos de Mensajes MQTT - KC868A2

## Descripción
Documentación detallada de todos los tipos de mensajes MQTT utilizados en el sistema KC868A2, incluyendo estructuras, parámetros, validaciones y ejemplos de uso.

## Clasificación de Mensajes

### 📤 Mensajes de Entrada (Comandos al Dispositivo)

#### Tipo 0: Activación de Relé
**Tópico**: `swatidhome/command/[SERIAL]/relay`

**Estructura**:
```json
{
  "message_id": 123,
  "device": "SWATID_XXXXXXXX",
  "message_type": 0,
  "message_info": {
    "relay_number": 1,
    "duration": 3.0
  }
}
```

**Parámetros**:
- `relay_number` (int): 1 o 2
- `duration` (float): 0.5-60.0 segundos

**Validaciones**:
- Número de relé válido (1-2)
- Duración dentro del rango permitido
- Formato numérico correcto

**Respuesta**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "response_id": 124,
  "message_id": 123,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "response_type": 0,
  "response_info": "relay 1 activated",
  "relay": 1
}
```

#### Tipo 1: Solicitud de Información
**Tópico**: `swatidhome/command/[SERIAL]/info`

**Estructura**:
```json
{
  "message_id": 125,
  "device": "SWATID_XXXXXXXX",
  "message_type": 1
}
```

**Parámetros**: Ninguno

**Respuesta**: Información completa del dispositivo (ver sección de respuestas)

#### Tipo 2: Comandos de Sistema
**Tópico**: `swatidhome/command/[SERIAL]/system`

**Estructura**:
```json
{
  "message_id": 126,
  "device": "SWATID_XXXXXXXX",
  "message_type": 2,
  "message_info": "reboot"
}
```

**Comandos disponibles**:
- `reboot`: Reinicio del sistema
- `reset`: Reset a valores por defecto
- `update`: Actualización de configuración

**Validaciones**:
- Comando válido
- Formato de string correcto

**Respuesta**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "response_id": 127,
  "message_id": 126,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "response_type": 0,
  "response_info": "reboot initiated"
}
```

#### Tipo 3: Comandos de Seguridad
**Tópico**: `swatidhome/command/[SERIAL]/security`

**Estructura base**:
```json
{
  "message_id": 128,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "block_local_access"
  }
}
```

**Comandos de seguridad**:

##### Bloquear Acceso Local
```json
{
  "message_id": 128,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "block_local_access"
  }
}
```

##### Desbloquear Acceso Local
```json
{
  "message_id": 129,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "unblock_local_access"
  }
}
```

##### Configurar Duración de Bloqueo
```json
{
  "message_id": 130,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "set_block_duration",
    "duration_seconds": 120
  }
}
```

**Parámetros**:
- `duration_seconds` (int): 30-3600 segundos

##### Configurar Máximo de Intentos
```json
{
  "message_id": 131,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "set_max_failed_attempts",
    "max_attempts": 5
  }
}
```

**Parámetros**:
- `max_attempts` (int): 1-10 intentos

### 📥 Mensajes de Salida (Respuestas del Dispositivo)

#### Información del Dispositivo
**Tópico**: `swatidhome/[SERIAL]/info`

**Estructura completa**:
```json
{
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "ip": "192.168.1.100",
  "mac": "AA:BB:CC:DD:EE:FF",
  "wifi_signal": -1,
  "firmware_version": "1.7.0-COMPLETE-SECURITY",
  "use_dhcp": true,
  "relay_duration": 2.0,
  "security": {
    "local_access_blocked": false,
    "failed_attempts": 0,
    "max_failed_attempts": 3,
    "block_duration_seconds": 60,
    "remaining_block_time": 0
  },
  "keyboards": {
    "wiegand1_pins": "33/14",
    "wiegand2_pins": "4/16",
    "dual_support": true,
    "total_keyboards": 2
  },
  "stored_codes": [
    {
      "type": "PIN",
      "value": "1234",
      "relay": 1
    },
    {
      "type": "TAG",
      "value": "12345678",
      "relay": 2
    }
  ]
}
```

**Campos de seguridad**:
- `local_access_blocked` (bool): Estado del bloqueo
- `failed_attempts` (int): Intentos fallidos actuales
- `max_failed_attempts` (int): Máximo configurado
- `block_duration_seconds` (int): Duración del bloqueo
- `remaining_block_time` (int): Tiempo restante si está bloqueado

**Campos de teclados**:
- `wiegand1_pins` (string): Pines GPIO del teclado 1
- `wiegand2_pins` (string): Pines GPIO del teclado 2
- `dual_support` (bool): Soporte dual activo
- `total_keyboards` (int): Número total de teclados

#### Estado de Relé
**Tópico**: `swatidhome/[SERIAL]/relay_enable`

**Estructura**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "resultado": "OK",
  "rele": 1,
  "estado": "ON",
  "origen": "SYSTEM",
  "message_id": 132,
  "request_id": 133
}
```

**Estados**:
- `ON`: Relé activado
- `OFF`: Relé desactivado

**Orígenes**:
- `SYSTEM`: Activación del sistema
- `TIMEOUT`: Desactivación por tiempo
- `WEB`: Activación desde web
- `MQTT`: Activación remota

#### Evento de Acceso
**Tópico**: `swatidhome/events/[SERIAL]/access`

**Estructura**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "message_id": 134,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "event_type": "ACCESS_EVENT",
  "success": true,
  "source": "LOCAL",
  "code_type": "PIN",
  "code_value": "1234",
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1"
}
```

**Fuentes**:
- `LOCAL`: Validación local
- `REMOTE`: Validación remota
- `WEB`: Activación desde web
- `MQTT_RELAY`: Activación remota de relé
- `LOCAL_EMERGENCY`: Fallback local de emergencia
- `LOCAL_FALLBACK`: Fallback local por MQTT desconectado

#### Intento de Acceso Fallido
**Tópico**: `swatidhome/events/[SERIAL]/failed_access`

**Estructura**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "message_id": 135,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "event_type": "FAILED_ACCESS",
  "code_type": "PIN",
  "code_value": "9999",
  "reason": "INVALID_CODE",
  "failed_attempts": 2,
  "max_attempts": 3,
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1"
}
```

**Razones**:
- `INVALID_CODE`: Código no válido
- `BLOCKED`: Acceso bloqueado
- `REMOTE_DENIED`: Denegado remotamente
- `MQTT_PUBLISH_FAILED`: Error de comunicación

#### Error del Sistema
**Tópico**: `swatidhome/errors/[SERIAL]/rx`

**Estructura**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "message_id": 136,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "error_code": 6,
  "description": "Acceso bloqueado tras 3 intentos fallidos"
}
```

**Códigos de error**:
- `1`: Error de conexión de red
- `2`: Error de configuración
- `3`: Mensaje de inicio/estado
- `4`: Cambio de configuración
- `5`: Sistema de seguridad
- `6`: Bloqueo de acceso activado
- `7`: Error de validación remota
- `8`: Memoria baja
- `9`: MQTT desconectado prolongado

#### Respuesta a Comando
**Tópico**: `swatidhome/response/[SERIAL]/rx`

**Estructura**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "response_id": 137,
  "message_id": 123,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "response_type": 0,
  "response_info": "relay 1 activated",
  "relay": 1
}
```

**Tipos de respuesta**:
- `0`: Respuesta a comando de relé
- `1`: Respuesta a solicitud de información

#### Keepalive
**Tópico**: `swatidhome/keepalive/[SERIAL]`

**Estructura**:
```json
{
  "status": "online",
  "uptime": 3600000
}
```

**Campos**:
- `status` (string): "online"
- `uptime` (int): Tiempo de funcionamiento en milisegundos

### 🔄 Mensajes de Validación

#### Solicitud de Validación (Automático)
**Tópico**: `swatidhome/command/[SERIAL]/access`

**Estructura**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "message_id": 138,
  "device": "SWATID_XXXXXXXX",
  "device_name": "DeviceName",
  "message_type": 0,
  "message_info": {
    "source": "AUTO",
    "code_type": "PIN",
    "code_value": "1234",
    "keyboard_id": 1,
    "keyboard_name": "WIEGAND1",
    "keyboard_pins": "33/14",
    "request_relay": 1,
    "max_duration": 2.0
  }
}
```

**Campos**:
- `source` (string): "AUTO"
- `code_type` (string): "PIN" o "TAG"
- `code_value` (string): Valor del código
- `keyboard_id` (int): 1 o 2
- `keyboard_name` (string): "WIEGAND1" o "WIEGAND2"
- `keyboard_pins` (string): Pines GPIO
- `request_relay` (int): Relé solicitado
- `max_duration` (float): Duración máxima

#### Respuesta de Validación
**Tópico**: `swatidhome/command/[SERIAL]/granted`

**Estructura**:
```json
{
  "access_granted": true,
  "code_type": "PIN",
  "code_value": "1234",
  "duration": 2000,
  "relay_number": 1,
  "reason": "Valid code"
}
```

**Campos**:
- `access_granted` (bool): Acceso concedido
- `code_type` (string): Tipo de código
- `code_value` (string): Valor del código
- `duration` (int): Duración en milisegundos
- `relay_number` (int): Relé a activar
- `reason` (string): Razón de la decisión

## Validaciones y Restricciones

### 📝 Validaciones de Entrada
- **Formato JSON**: Estructura válida
- **Campos requeridos**: Presencia obligatoria
- **Tipos de datos**: Coincidencia con especificación
- **Rangos**: Valores dentro de límites
- **Formato**: Validación de patrones

### 🚫 Restricciones del Sistema
- **Tamaño de mensaje**: Máximo 2048 bytes
- **Frecuencia**: Límites de envío
- **Concurrencia**: Un mensaje por vez
- **Timeout**: Tiempo de respuesta

### 🔒 Seguridad
- **Autenticación**: Usuario/contraseña
- **Autorización**: Por tópico
- **Validación**: Estructura de mensajes
- **Sanitización**: Datos de entrada

## Ejemplos de Uso

### 🐍 Python - Envío de Comando
```python
import paho.mqtt.client as mqtt
import json

def send_relay_command(serial, relay, duration):
    client = mqtt.Client()
    client.username_pw_set("swatidhome", "Swatid2025!")
    client.connect("188.245.213.181", 1883, 60)
    
    message = {
        "message_id": int(time.time()),
        "device": serial,
        "message_type": 0,
        "message_info": {
            "relay_number": relay,
            "duration": duration
        }
    }
    
    topic = f"swatidhome/command/{serial}/relay"
    client.publish(topic, json.dumps(message))
    client.disconnect()
```

### 🟨 Node.js - Monitoreo de Eventos
```javascript
const mqtt = require('mqtt');
const client = mqtt.connect('mqtt://188.245.213.181:1883', {
  username: 'swatidhome',
  password: 'Swatid2025!'
});

client.on('connect', () => {
  client.subscribe('swatidhome/events/+/access');
  client.subscribe('swatidhome/events/+/failed_access');
});

client.on('message', (topic, message) => {
  const data = JSON.parse(message.toString());
  
  if (topic.includes('access')) {
    console.log(`Acceso: ${data.success} - ${data.source}`);
  } else if (topic.includes('failed_access')) {
    console.log(`Acceso fallido: ${data.reason}`);
  }
});
```

### 🔧 C++ - Procesamiento de Respuestas
```cpp
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, payload, length);
  
  if (strstr(topic, "/relay_enable")) {
    int relay = doc["rele"];
    String estado = doc["estado"];
    String origen = doc["origen"];
    
    Serial.printf("Relé %d: %s (origen: %s)\n", 
                  relay, estado.c_str(), origen.c_str());
  }
}
```

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
