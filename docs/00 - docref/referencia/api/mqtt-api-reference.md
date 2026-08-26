# API MQTT - Referencia Completa KC868A2

## Descripción
Referencia completa de la API MQTT del sistema KC868A2, incluyendo todos los tópicos, mensajes, parámetros y ejemplos de uso.

## Configuración de Conexión

### 📡 Parámetros de Conexión
- **Broker**: 188.245.213.181
- **Puerto**: 1883
- **Protocolo**: TCP/IP
- **Usuario**: swatidhome
- **Contraseña**: Swatid2025!
- **QoS**: 0 (At Most Once)
- **Keepalive**: 60 segundos
- **Buffer**: 2048 bytes

### 🔐 Autenticación
```python
import paho.mqtt.client as mqtt

client = mqtt.Client()
client.username_pw_set("swatidhome", "Swatid2025!")
client.connect("188.245.213.181", 1883, 60)
```

## Tópicos de Comando (Envío al Dispositivo)

### ⚡ Activación de Relé
**Tópico**: `swatidhome/command/[SERIAL]/relay`

**Mensaje**:
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

**Ejemplo Python**:
```python
def activate_relay(serial, relay, duration):
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
```

### 📋 Solicitud de Información
**Tópico**: `swatidhome/command/[SERIAL]/info`

**Mensaje**:
```json
{
  "message_id": 125,
  "device": "SWATID_XXXXXXXX",
  "message_type": 1
}
```

**Respuesta**: Información completa del dispositivo (ver sección de respuestas)

**Ejemplo Python**:
```python
def get_device_info(serial):
    message = {
        "message_id": int(time.time()),
        "device": serial,
        "message_type": 1
    }
    
    topic = f"swatidhome/command/{serial}/info"
    client.publish(topic, json.dumps(message))
```

### 🔧 Comandos de Sistema
**Tópico**: `swatidhome/command/[SERIAL]/system`

**Mensaje**:
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

**Ejemplo Python**:
```python
def reboot_device(serial):
    message = {
        "message_id": int(time.time()),
        "device": serial,
        "message_type": 2,
        "message_info": "reboot"
    }
    
    topic = f"swatidhome/command/{serial}/system"
    client.publish(topic, json.dumps(message))
```

### 🔒 Comandos de Seguridad
**Tópico**: `swatidhome/command/[SERIAL]/security`

#### Bloquear Acceso Local
```json
{
  "message_id": 127,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "block_local_access"
  }
}
```

#### Desbloquear Acceso Local
```json
{
  "message_id": 128,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "unblock_local_access"
  }
}
```

#### Configurar Duración de Bloqueo
```json
{
  "message_id": 129,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "set_block_duration",
    "duration_seconds": 120
  }
}
```

#### Configurar Máximo de Intentos
```json
{
  "message_id": 130,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "set_max_failed_attempts",
    "max_attempts": 5
  }
}
```

**Ejemplo Python**:
```python
def block_local_access(serial):
    message = {
        "message_id": int(time.time()),
        "device": serial,
        "message_type": 3,
        "message_info": {
            "security_command": "block_local_access"
        }
    }
    
    topic = f"swatidhome/command/{serial}/security"
    client.publish(topic, json.dumps(message))
```

### 🔍 Validación de Acceso (Automático)
**Tópico**: `swatidhome/command/[SERIAL]/access`

**Mensaje** (enviado automáticamente por el dispositivo):
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "message_id": 131,
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

### ✅ Respuesta de Validación
**Tópico**: `swatidhome/command/[SERIAL]/granted`

**Mensaje** (respuesta del servidor):
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

**Ejemplo Python**:
```python
def grant_access(serial, code_type, code_value, duration=2000, relay=1):
    message = {
        "access_granted": True,
        "code_type": code_type,
        "code_value": code_value,
        "duration": duration,
        "relay_number": relay,
        "reason": "Valid code"
    }
    
    topic = f"swatidhome/command/{serial}/granted"
    client.publish(topic, json.dumps(message))
```

## Tópicos de Respuesta (Recepción del Dispositivo)

### 📊 Información del Dispositivo
**Tópico**: `swatidhome/[SERIAL]/info`

**Mensaje**:
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

### ⚡ Estado de Relé
**Tópico**: `swatidhome/[SERIAL]/relay_enable`

**Mensaje**:
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

### 🎯 Evento de Acceso
**Tópico**: `swatidhome/events/[SERIAL]/access`

**Mensaje**:
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

### ❌ Intento de Acceso Fallido
**Tópico**: `swatidhome/events/[SERIAL]/failed_access`

**Mensaje**:
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

### 🚨 Error del Sistema
**Tópico**: `swatidhome/errors/[SERIAL]/rx`

**Mensaje**:
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

### 📤 Respuesta a Comando
**Tópico**: `swatidhome/response/[SERIAL]/rx`

**Mensaje**:
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

### 💓 Keepalive
**Tópico**: `swatidhome/keepalive/[SERIAL]`

**Mensaje**:
```json
{
  "status": "online",
  "uptime": 3600000
}
```

## Suscripciones Recomendadas

### 📡 Para Monitoreo Completo
```python
def setup_subscriptions(client, serial):
    # Eventos de acceso
    client.subscribe(f"swatidhome/events/{serial}/access")
    client.subscribe(f"swatidhome/events/{serial}/failed_access")
    
    # Estado del sistema
    client.subscribe(f"swatidhome/{serial}/relay_enable")
    client.subscribe(f"swatidhome/{serial}/info")
    
    # Errores
    client.subscribe(f"swatidhome/errors/{serial}/rx")
    
    # Respuestas
    client.subscribe(f"swatidhome/response/{serial}/rx")
    
    # Keepalive
    client.subscribe(f"swatidhome/keepalive/{serial}")
```

### 🔍 Para Monitoreo Global
```python
def setup_global_subscriptions(client):
    # Todos los eventos de acceso
    client.subscribe("swatidhome/events/+/access")
    client.subscribe("swatidhome/events/+/failed_access")
    
    # Todos los errores
    client.subscribe("swatidhome/errors/+/rx")
    
    # Todos los keepalives
    client.subscribe("swatidhome/keepalive/+/#")
```

## Ejemplos de Uso Completos

### 🐍 Cliente Python Completo
```python
import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime

class KC868A2Client:
    def __init__(self, device_serial, broker="188.245.213.181", port=1883):
        self.device_serial = device_serial
        self.broker = broker
        self.port = port
        self.client = mqtt.Client()
        self.client.username_pw_set("swatidhome", "Swatid2025!")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.connected = False
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print(f"Conectado al broker MQTT")
            self.setup_subscriptions()
        else:
            print(f"Error conectando: {rc}")
            
    def on_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode())
            topic = msg.topic
            
            if "/access" in topic:
                self.handle_access_event(data)
            elif "/failed_access" in topic:
                self.handle_failed_access(data)
            elif "/relay_enable" in topic:
                self.handle_relay_status(data)
            elif "/info" in topic:
                self.handle_device_info(data)
            elif "/errors/" in topic:
                self.handle_error(data)
            elif "/response/" in topic:
                self.handle_response(data)
            elif "/keepalive" in topic:
                self.handle_keepalive(data)
                
        except json.JSONDecodeError as e:
            print(f"Error decodificando JSON: {e}")
            
    def setup_subscriptions(self):
        # Suscribirse a todos los eventos del dispositivo
        self.client.subscribe(f"swatidhome/events/{self.device_serial}/access")
        self.client.subscribe(f"swatidhome/events/{self.device_serial}/failed_access")
        self.client.subscribe(f"swatidhome/{self.device_serial}/relay_enable")
        self.client.subscribe(f"swatidhome/{self.device_serial}/info")
        self.client.subscribe(f"swatidhome/errors/{self.device_serial}/rx")
        self.client.subscribe(f"swatidhome/response/{self.device_serial}/rx")
        self.client.subscribe(f"swatidhome/keepalive/{self.device_serial}")
        
    def handle_access_event(self, data):
        success = "✅ ÉXITO" if data.get("success") else "❌ FALLO"
        source = data.get("source", "UNKNOWN")
        code_type = data.get("code_type", "UNKNOWN")
        code_value = data.get("code_value", "UNKNOWN")
        keyboard = data.get("keyboard_name", "UNKNOWN")
        
        print(f"{success} - {source} - {code_type}:{code_value} - {keyboard}")
        
    def handle_failed_access(self, data):
        reason = data.get("reason", "UNKNOWN")
        code_type = data.get("code_type", "UNKNOWN")
        code_value = data.get("code_value", "UNKNOWN")
        attempts = data.get("failed_attempts", 0)
        max_attempts = data.get("max_attempts", 0)
        
        print(f"❌ ACCESO FALLIDO - {reason} - {code_type}:{code_value} - {attempts}/{max_attempts}")
        
    def handle_relay_status(self, data):
        relay = data.get("rele", 0)
        estado = data.get("estado", "UNKNOWN")
        origen = data.get("origen", "UNKNOWN")
        
        print(f"⚡ Relé {relay}: {estado} (origen: {origen})")
        
    def handle_device_info(self, data):
        device = data.get("device", "UNKNOWN")
        serial = data.get("serial", "UNKNOWN")
        ip = data.get("ip", "UNKNOWN")
        firmware = data.get("firmware_version", "UNKNOWN")
        
        print(f"📊 Dispositivo: {device} ({serial}) - IP: {ip} - FW: {firmware}")
        
    def handle_error(self, data):
        error_code = data.get("error_code", 0)
        description = data.get("description", "Sin descripción")
        
        print(f"🚨 ERROR {error_code}: {description}")
        
    def handle_response(self, data):
        response_info = data.get("response_info", "Sin información")
        message_id = data.get("message_id", 0)
        
        print(f"📤 Respuesta: {response_info} (ID: {message_id})")
        
    def handle_keepalive(self, data):
        status = data.get("status", "UNKNOWN")
        uptime = data.get("uptime", 0)
        
        print(f"💓 Keepalive: {status} - Uptime: {uptime}ms")
        
    def activate_relay(self, relay, duration=2.0):
        message = {
            "message_id": int(time.time()),
            "device": self.device_serial,
            "message_type": 0,
            "message_info": {
                "relay_number": relay,
                "duration": duration
            }
        }
        
        topic = f"swatidhome/command/{self.device_serial}/relay"
        result = self.client.publish(topic, json.dumps(message))
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"Comando enviado: Relé {relay} por {duration}s")
        else:
            print(f"Error enviando comando: {result.rc}")
            
    def get_device_info(self):
        message = {
            "message_id": int(time.time()),
            "device": self.device_serial,
            "message_type": 1
        }
        
        topic = f"swatidhome/command/{self.device_serial}/info"
        self.client.publish(topic, json.dumps(message))
        print("Solicitud de información enviada")
        
    def reboot_device(self):
        message = {
            "message_id": int(time.time()),
            "device": self.device_serial,
            "message_type": 2,
            "message_info": "reboot"
        }
        
        topic = f"swatidhome/command/{self.device_serial}/system"
        self.client.publish(topic, json.dumps(message))
        print("Comando de reinicio enviado")
        
    def block_local_access(self):
        message = {
            "message_id": int(time.time()),
            "device": self.device_serial,
            "message_type": 3,
            "message_info": {
                "security_command": "block_local_access"
            }
        }
        
        topic = f"swatidhome/command/{self.device_serial}/security"
        self.client.publish(topic, json.dumps(message))
        print("Comando de bloqueo enviado")
        
    def unblock_local_access(self):
        message = {
            "message_id": int(time.time()),
            "device": self.device_serial,
            "message_type": 3,
            "message_info": {
                "security_command": "unblock_local_access"
            }
        }
        
        topic = f"swatidhome/command/{self.device_serial}/security"
        self.client.publish(topic, json.dumps(message))
        print("Comando de desbloqueo enviado")
        
    def connect(self):
        self.client.connect(self.broker, self.port, 60)
        self.client.loop_start()
        
    def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()

# Uso
if __name__ == "__main__":
    client = KC868A2Client("SWATID_12345678")
    client.connect()
    
    # Esperar conexión
    time.sleep(2)
    
    # Ejemplos de uso
    client.get_device_info()
    time.sleep(1)
    
    client.activate_relay(1, 3.0)
    time.sleep(1)
    
    client.block_local_access()
    time.sleep(5)
    
    client.unblock_local_access()
    time.sleep(5)
    
    client.disconnect()
```

## Mejores Prácticas

### 🔒 Seguridad
- **Autenticación**: Siempre usar credenciales
- **Validación**: Verificar estructura de mensajes
- **Sanitización**: Limpiar datos de entrada
- **Logging**: Registrar eventos importantes

### ⚡ Rendimiento
- **Reconexión**: Implementar reconexión automática
- **Buffer**: Usar buffers apropiados
- **QoS**: Seleccionar nivel adecuado
- **Compresión**: Optimizar tamaño de mensajes

### 🛠️ Mantenimiento
- **Monitoreo**: Supervisar estado de conexión
- **Logs**: Mantener logs detallados
- **Backup**: Respaldo de configuraciones
- **Testing**: Pruebas regulares

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
