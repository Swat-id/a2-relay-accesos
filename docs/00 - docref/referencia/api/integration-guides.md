# Guías de Integración - KC868A2

## Descripción
Guías completas de integración del sistema KC868A2 con diferentes plataformas y sistemas, incluyendo ejemplos prácticos y mejores prácticas.

## Integración con Home Assistant

### 🏠 Configuración Básica

#### 1. Configuración MQTT
```yaml
# configuration.yaml
mqtt:
  broker: 188.245.213.181
  port: 1883
  username: swatidhome
  password: Swatid2025!
  discovery: true
  discovery_prefix: homeassistant
```

#### 2. Sensores de Estado
```yaml
# sensors.yaml
- platform: mqtt
  name: "KC868A2 Access Events"
  state_topic: "swatidhome/events/+/access"
  value_template: "{{ value_json.success }}"
  json_attributes:
    - code_type
    - code_value
    - source
    - keyboard_name
    - timestamp

- platform: mqtt
  name: "KC868A2 Failed Access"
  state_topic: "swatidhome/events/+/failed_access"
  value_template: "{{ value_json.reason }}"
  json_attributes:
    - code_type
    - code_value
    - failed_attempts
    - max_attempts
    - timestamp

- platform: mqtt
  name: "KC868A2 Security Status"
  state_topic: "swatidhome/+/info"
  value_template: "{{ value_json.security.local_access_blocked }}"
  json_attributes:
    - failed_attempts
    - max_failed_attempts
    - block_duration_seconds
    - remaining_block_time
```

#### 3. Interruptores de Relés
```yaml
# switches.yaml
- platform: mqtt
  name: "KC868A2 Relay 1"
  command_topic: "swatidhome/command/SWATID_12345678/relay"
  state_topic: "swatidhome/SWATID_12345678/relay_enable"
  value_template: "{{ value_json.estado }}"
  payload_on: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 0, "message_info": {"relay_number": 1, "duration": 2.0}}'
  payload_off: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 0, "message_info": {"relay_number": 1, "duration": 0.0}}'
  state_on: "ON"
  state_off: "OFF"

- platform: mqtt
  name: "KC868A2 Relay 2"
  command_topic: "swatidhome/command/SWATID_12345678/relay"
  state_topic: "swatidhome/SWATID_12345678/relay_enable"
  value_template: "{{ value_json.estado }}"
  payload_on: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 0, "message_info": {"relay_number": 2, "duration": 2.0}}'
  payload_off: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 0, "message_info": {"relay_number": 2, "duration": 0.0}}'
  state_on: "ON"
  state_off: "OFF"
```

#### 4. Automatizaciones
```yaml
# automations.yaml
- alias: "KC868A2 Access Notification"
  trigger:
    platform: mqtt
    topic: "swatidhome/events/+/access"
  action:
    service: notify.mobile_app_phone
    data:
      title: "Acceso KC868A2"
      message: "{{ trigger.payload_json.code_type }}: {{ trigger.payload_json.code_value }} - {{ trigger.payload_json.source }}"

- alias: "KC868A2 Failed Access Alert"
  trigger:
    platform: mqtt
    topic: "swatidhome/events/+/failed_access"
  action:
    service: notify.mobile_app_phone
    data:
      title: "⚠️ Acceso Fallido KC868A2"
      message: "{{ trigger.payload_json.reason }} - {{ trigger.payload_json.code_type }}: {{ trigger.payload_json.code_value }}"

- alias: "KC868A2 Security Block Alert"
  trigger:
    platform: mqtt
    topic: "swatidhome/errors/+/rx"
  condition:
    condition: template
    value_template: "{{ trigger.payload_json.error_code == 6 }}"
  action:
    service: notify.mobile_app_phone
    data:
      title: "🚨 BLOQUEO DE SEGURIDAD KC868A2"
      message: "{{ trigger.payload_json.description }}"
```

#### 5. Scripts de Control
```yaml
# scripts.yaml
kc868a2_block_access:
  alias: "Bloquear Acceso KC868A2"
  sequence:
    - service: mqtt.publish
      data:
        topic: "swatidhome/command/SWATID_12345678/security"
        payload: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 3, "message_info": {"security_command": "block_local_access"}}'

kc868a2_unblock_access:
  alias: "Desbloquear Acceso KC868A2"
  sequence:
    - service: mqtt.publish
      data:
        topic: "swatidhome/command/SWATID_12345678/security"
        payload: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 3, "message_info": {"security_command": "unblock_local_access"}}'

kc868a2_reboot:
  alias: "Reiniciar KC868A2"
  sequence:
    - service: mqtt.publish
      data:
        topic: "swatidhome/command/SWATID_12345678/system"
        payload: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 2, "message_info": "reboot"}'
```

## Integración con Node-RED

### 🔴 Flujo de Monitoreo

#### 1. Configuración MQTT
```json
{
  "id": "mqtt-broker",
  "type": "mqtt-broker",
  "name": "KC868A2 Broker",
  "broker": "188.245.213.181",
  "port": "1883",
  "clientid": "",
  "usetls": false,
  "protocolVersion": "4",
  "keepalive": "60",
  "cleansession": true,
  "birthTopic": "",
  "birthQos": "0",
  "birthPayload": "",
  "birthMsg": {},
  "closeTopic": "",
  "closeQos": "0",
  "closePayload": "",
  "closeMsg": {},
  "willTopic": "",
  "willQos": "0",
  "willPayload": "",
  "willMsg": {},
  "userProps": "",
  "sessionExpiry": ""
}
```

#### 2. Flujo de Monitoreo de Accesos
```json
[
  {
    "id": "mqtt-in-access",
    "type": "mqtt in",
    "z": "flow1",
    "name": "Access Events",
    "topic": "swatidhome/events/+/access",
    "qos": "0",
    "datatype": "json",
    "broker": "mqtt-broker",
    "x": 150,
    "y": 100,
    "wires": [["process-access"]]
  },
  {
    "id": "process-access",
    "type": "function",
    "z": "flow1",
    "name": "Process Access",
    "func": "const data = msg.payload;\nconst success = data.success ? '✅ ÉXITO' : '❌ FALLO';\nconst source = data.source || 'UNKNOWN';\nconst codeType = data.code_type || 'UNKNOWN';\nconst codeValue = data.code_value || 'UNKNOWN';\nconst keyboard = data.keyboard_name || 'UNKNOWN';\n\nmsg.payload = {\n    message: `${success} - ${source} - ${codeType}:${codeValue} - ${keyboard}`,\n    timestamp: data.timestamp,\n    success: data.success,\n    source: data.source,\n    code_type: data.code_type,\n    code_value: data.code_value,\n    keyboard_name: data.keyboard_name\n};\n\nreturn msg;",
    "outputs": 1,
    "x": 350,
    "y": 100,
    "wires": [["debug-access", "notify-access"]]
  },
  {
    "id": "notify-access",
    "type": "function",
    "z": "flow1",
    "name": "Notify Access",
    "func": "if (msg.payload.success) {\n    // Acceso exitoso - solo log\n    node.log(`Acceso exitoso: ${msg.payload.code_type}:${msg.payload.code_value}`);\n} else {\n    // Acceso fallido - notificación\n    msg.payload = {\n        title: 'Acceso Fallido KC868A2',\n        message: msg.payload.message\n    };\n    return msg;\n}\n\nreturn null;",
    "outputs": 1,
    "x": 550,
    "y": 100,
    "wires": [["telegram-notify"]]
  }
]
```

#### 3. Flujo de Control de Relés
```json
[
  {
    "id": "relay-control",
    "type": "function",
    "z": "flow1",
    "name": "Relay Control",
    "func": "const relay = msg.payload.relay || 1;\nconst duration = msg.payload.duration || 2.0;\n\nmsg.payload = {\n    message_id: Date.now(),\n    device: 'SWATID_12345678',\n    message_type: 0,\n    message_info: {\n        relay_number: relay,\n        duration: duration\n    }\n};\n\nmsg.topic = 'swatidhome/command/SWATID_12345678/relay';\n\nreturn msg;",
    "outputs": 1,
    "x": 350,
    "y": 200,
    "wires": [["mqtt-out"]]
  },
  {
    "id": "mqtt-out",
    "type": "mqtt out",
    "z": "flow1",
    "name": "MQTT Out",
    "topic": "",
    "qos": "0",
    "retain": "false",
    "broker": "mqtt-broker",
    "x": 550,
    "y": 200,
    "wires": []
  }
]
```

## Integración con OpenHAB

### 🏡 Configuración de Items

#### 1. Archivo de Items
```java
// items/kc868a2.items
Group KC868A2 "KC868A2 Controller" <lock>

// Sensores
String KC868A2_AccessEvent "Último Acceso" <lock> (KC868A2) { mqtt="<[broker:swatidhome/events/+/access:state:JSONPATH($.code_type + ':' + $.code_value)]" }
String KC868A2_FailedAccess "Último Acceso Fallido" <lock> (KC868A2) { mqtt="<[broker:swatidhome/events/+/failed_access:state:JSONPATH($.reason)]" }
Switch KC868A2_SecurityBlocked "Acceso Bloqueado" <lock> (KC868A2) { mqtt="<[broker:swatidhome/+/info:state:JSONPATH($.security.local_access_blocked)]" }
Number KC868A2_FailedAttempts "Intentos Fallidos" <lock> (KC868A2) { mqtt="<[broker:swatidhome/+/info:state:JSONPATH($.security.failed_attempts)]" }

// Relés
Switch KC868A2_Relay1 "Relé 1" <switch> (KC868A2) { mqtt=">[broker:swatidhome/command/SWATID_12345678/relay:command:ON:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":0,\"message_info\":{\"relay_number\":1,\"duration\":2.0}}],>[broker:swatidhome/command/SWATID_12345678/relay:command:OFF:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":0,\"message_info\":{\"relay_number\":1,\"duration\":0.0}}],<[broker:swatidhome/SWATID_12345678/relay_enable:state:JSONPATH($.estado)]" }
Switch KC868A2_Relay2 "Relé 2" <switch> (KC868A2) { mqtt=">[broker:swatidhome/command/SWATID_12345678/relay:command:ON:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":0,\"message_info\":{\"relay_number\":2,\"duration\":2.0}}],>[broker:swatidhome/command/SWATID_12345678/relay:command:OFF:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":0,\"message_info\":{\"relay_number\":2,\"duration\":0.0}}],<[broker:swatidhome/SWATID_12345678/relay_enable:state:JSONPATH($.estado)]" }

// Controles de seguridad
Switch KC868A2_BlockAccess "Bloquear Acceso" <lock> (KC868A2) { mqtt=">[broker:swatidhome/command/SWATID_12345678/security:command:ON:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":3,\"message_info\":{\"security_command\":\"block_local_access\"}}],>[broker:swatidhome/command/SWATID_12345678/security:command:OFF:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":3,\"message_info\":{\"security_command\":\"unblock_local_access\"}}]" }
Switch KC868A2_Reboot "Reiniciar Dispositivo" <switch> (KC868A2) { mqtt=">[broker:swatidhome/command/SWATID_12345678/system:command:ON:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":2,\"message_info\":\"reboot\"}]" }
```

#### 2. Archivo de Reglas
```java
// rules/kc868a2.rules
rule "KC868A2 Access Event"
when
    Item KC868A2_AccessEvent changed
then
    val accessType = KC868A2_AccessEvent.state.toString.split(":")[0]
    val accessCode = KC868A2_AccessEvent.state.toString.split(":")[1]
    
    logInfo("KC868A2", "Acceso detectado: {} - {}", accessType, accessCode)
    
    // Enviar notificación
    sendNotification("admin@example.com", "Acceso KC868A2: " + accessType + " - " + accessCode)
end

rule "KC868A2 Failed Access Alert"
when
    Item KC868A2_FailedAccess changed
then
    val reason = KC868A2_FailedAccess.state.toString
    val attempts = KC868A2_FailedAttempts.state as Number
    
    logWarn("KC868A2", "Acceso fallido: {} - Intentos: {}", reason, attempts)
    
    // Enviar alerta
    sendNotification("admin@example.com", "⚠️ Acceso Fallido KC868A2: " + reason + " - Intentos: " + attempts)
end

rule "KC868A2 Security Block Alert"
when
    Item KC868A2_SecurityBlocked changed to ON
then
    logError("KC868A2", "BLOQUEO DE SEGURIDAD ACTIVADO")
    
    // Enviar alerta crítica
    sendNotification("admin@example.com", "🚨 BLOQUEO DE SEGURIDAD KC868A2 ACTIVADO")
end
```

## Integración con Sistemas de Videovigilancia

### 📹 Integración con ZoneMinder

#### 1. Script de Monitoreo
```bash
#!/bin/bash
# /usr/local/bin/kc868a2-monitor.sh

# Configuración
MQTT_BROKER="188.245.213.181"
MQTT_PORT="1883"
MQTT_USER="swatidhome"
MQTT_PASS="Swatid2025!"
DEVICE_SERIAL="SWATID_12345678"

# Función para procesar eventos de acceso
process_access_event() {
    local event_data="$1"
    local success=$(echo "$event_data" | jq -r '.success')
    local code_type=$(echo "$event_data" | jq -r '.code_type')
    local code_value=$(echo "$event_data" | jq -r '.code_value')
    local source=$(echo "$event_data" | jq -r '.source')
    local keyboard=$(echo "$event_data" | jq -r '.keyboard_name')
    
    if [ "$success" = "true" ]; then
        echo "Acceso exitoso: $code_type:$code_value - $source - $keyboard"
        
        # Activar grabación de video
        zmtrigger.pl -d 1 -m 1 -e "KC868A2 Access: $code_type:$code_value"
        
        # Enviar notificación
        echo "Acceso KC868A2: $code_type:$code_value" | mail -s "Acceso KC868A2" admin@example.com
    else
        echo "Acceso fallido: $code_type:$code_value - $source - $keyboard"
        
        # Activar grabación de video para acceso fallido
        zmtrigger.pl -d 1 -m 1 -e "KC868A2 Failed Access: $code_type:$code_value"
        
        # Enviar alerta
        echo "Acceso fallido KC868A2: $code_type:$code_value" | mail -s "⚠️ Acceso Fallido KC868A2" admin@example.com
    fi
}

# Suscribirse a eventos MQTT
mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USER" -P "$MQTT_PASS" \
    -t "swatidhome/events/$DEVICE_SERIAL/access" | while read -r line; do
    process_access_event "$line"
done
```

#### 2. Configuración de ZoneMinder
```ini
# /etc/zm/zm.conf
ZM_PATH_ZMS=/cgi-bin/nph-zms
ZM_PATH_CGI=/cgi-bin
ZM_WEB_USER=www-data
ZM_WEB_GROUP=www-data
ZM_RUN_USER=www-data
ZM_RUN_GROUP=www-data
```

### 📹 Integración con Shinobi

#### 1. Plugin de Monitoreo
```javascript
// plugins/kc868a2-monitor.js
const mqtt = require('mqtt');

class KC868A2Monitor {
    constructor(config) {
        this.config = config;
        this.client = mqtt.connect(`mqtt://${config.broker}:${config.port}`, {
            username: config.username,
            password: config.password
        });
        
        this.client.on('connect', () => {
            console.log('Conectado a MQTT para KC868A2');
            this.client.subscribe(`swatidhome/events/${config.deviceSerial}/access`);
            this.client.subscribe(`swatidhome/events/${config.deviceSerial}/failed_access`);
        });
        
        this.client.on('message', (topic, message) => {
            this.handleMessage(topic, message);
        });
    }
    
    handleMessage(topic, message) {
        try {
            const data = JSON.parse(message.toString());
            
            if (topic.includes('access')) {
                this.handleAccessEvent(data);
            } else if (topic.includes('failed_access')) {
                this.handleFailedAccess(data);
            }
        } catch (error) {
            console.error('Error procesando mensaje MQTT:', error);
        }
    }
    
    handleAccessEvent(data) {
        const success = data.success ? 'ÉXITO' : 'FALLO';
        const codeType = data.code_type || 'UNKNOWN';
        const codeValue = data.code_value || 'UNKNOWN';
        const source = data.source || 'UNKNOWN';
        
        console.log(`Acceso KC868A2: ${success} - ${codeType}:${codeValue} - ${source}`);
        
        // Activar grabación de video
        this.triggerRecording(data);
        
        // Enviar notificación
        this.sendNotification(data);
    }
    
    handleFailedAccess(data) {
        const reason = data.reason || 'UNKNOWN';
        const codeType = data.code_type || 'UNKNOWN';
        const codeValue = data.code_value || 'UNKNOWN';
        
        console.log(`Acceso fallido KC868A2: ${reason} - ${codeType}:${codeValue}`);
        
        // Activar grabación de video para acceso fallido
        this.triggerRecording(data, true);
        
        // Enviar alerta
        this.sendAlert(data);
    }
    
    triggerRecording(data, isFailed = false) {
        // Implementar lógica de grabación de video
        const event = isFailed ? 'Failed Access' : 'Access';
        const message = `KC868A2 ${event}: ${data.code_type}:${data.code_value}`;
        
        // Activar grabación en Shinobi
        // Implementar según API de Shinobi
    }
    
    sendNotification(data) {
        // Implementar envío de notificaciones
        // Email, SMS, push notifications, etc.
    }
    
    sendAlert(data) {
        // Implementar envío de alertas
        // Para accesos fallidos
    }
}

module.exports = KC868A2Monitor;
```

## Integración con Sistemas de Alarma

### 🚨 Integración con OpenHAB

#### 1. Configuración de Alarma
```java
// items/alarm.items
Group Alarm "Sistema de Alarma" <alarm>

// Estados de alarma
String Alarm_State "Estado de Alarma" <alarm> (Alarm) { mqtt="<[broker:swatidhome/+/info:state:JSONPATH($.security.local_access_blocked)]" }
Number Alarm_FailedAttempts "Intentos Fallidos" <alarm> (Alarm) { mqtt="<[broker:swatidhome/+/info:state:JSONPATH($.security.failed_attempts)]" }

// Controles de alarma
Switch Alarm_Arm "Armar Alarma" <alarm> (Alarm) { mqtt=">[broker:swatidhome/command/SWATID_12345678/security:command:ON:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":3,\"message_info\":{\"security_command\":\"block_local_access\"}}],>[broker:swatidhome/command/SWATID_12345678/security:command:OFF:{\"message_id\":1,\"device\":\"SWATID_12345678\",\"message_type\":3,\"message_info\":{\"security_command\":\"unblock_local_access\"}}]" }
```

#### 2. Reglas de Alarma
```java
// rules/alarm.rules
rule "Alarm Failed Access"
when
    Item Alarm_FailedAttempts changed
then
    val attempts = Alarm_FailedAttempts.state as Number
    val maxAttempts = 3
    
    if (attempts >= maxAttempts) {
        logError("Alarm", "Máximo de intentos fallidos alcanzado")
        
        // Activar alarma
        Alarm_Arm.sendCommand(ON)
        
        // Enviar alerta
        sendNotification("admin@example.com", "🚨 ALARMA ACTIVADA - Máximo de intentos fallidos")
        
        // Activar sirena
        Siren.sendCommand(ON)
    }
end

rule "Alarm Access Blocked"
when
    Item Alarm_State changed to ON
then
    logError("Alarm", "Acceso bloqueado - Alarma activada")
    
    // Enviar alerta crítica
    sendNotification("admin@example.com", "🚨 ALARMA ACTIVADA - Acceso bloqueado")
    
    // Activar sirena
    Siren.sendCommand(ON)
    
    // Activar grabación de video
    VideoRecording.sendCommand(ON)
end
```

## Integración con Sistemas de Gestión

### 📊 Integración con Grafana

#### 1. Configuración de Datasource
```json
{
  "name": "KC868A2 MQTT",
  "type": "mqtt",
  "url": "ws://188.245.213.181:9001",
  "access": "proxy",
  "basicAuth": true,
  "basicAuthUser": "swatidhome",
  "basicAuthPassword": "Swatid2025!",
  "jsonData": {
    "clientId": "grafana-kc868a2"
  }
}
```

#### 2. Dashboard de Monitoreo
```json
{
  "dashboard": {
    "title": "KC868A2 Monitoring",
    "panels": [
      {
        "title": "Access Events",
        "type": "stat",
        "targets": [
          {
            "query": "swatidhome/events/+/access",
            "field": "success"
          }
        ]
      },
      {
        "title": "Failed Access",
        "type": "stat",
        "targets": [
          {
            "query": "swatidhome/events/+/failed_access",
            "field": "failed_attempts"
          }
        ]
      },
      {
        "title": "Security Status",
        "type": "stat",
        "targets": [
          {
            "query": "swatidhome/+/info",
            "field": "security.local_access_blocked"
          }
        ]
      }
    ]
  }
}
```

### 📈 Integración con InfluxDB

#### 1. Configuración de Telegraf
```toml
# /etc/telegraf/telegraf.conf
[[inputs.mqtt_consumer]]
  servers = ["tcp://188.245.213.181:1883"]
  username = "swatidhome"
  password = "Swatid2025!"
  topics = [
    "swatidhome/events/+/access",
    "swatidhome/events/+/failed_access",
    "swatidhome/+/info"
  ]
  data_format = "json"
  json_string_fields = ["code_type", "code_value", "source", "reason"]
  json_int_fields = ["failed_attempts", "max_attempts", "keyboard_id"]
  json_bool_fields = ["success", "local_access_blocked"]
```

#### 2. Consultas de InfluxDB
```sql
-- Accesos por hora
SELECT COUNT(*) FROM "mqtt_consumer" 
WHERE "topic" =~ /access/ AND "success" = true 
GROUP BY time(1h)

-- Intentos fallidos por día
SELECT COUNT(*) FROM "mqtt_consumer" 
WHERE "topic" =~ /failed_access/ 
GROUP BY time(1d)

-- Estado de seguridad
SELECT "local_access_blocked" FROM "mqtt_consumer" 
WHERE "topic" =~ /info/ 
ORDER BY time DESC LIMIT 1
```

## Mejores Prácticas de Integración

### 🔒 Seguridad
- **Autenticación**: Usar credenciales seguras
- **Encriptación**: Considerar TLS para MQTT
- **Validación**: Verificar todos los datos recibidos
- **Logging**: Registrar todas las integraciones

### ⚡ Rendimiento
- **Conexiones**: Reutilizar conexiones MQTT
- **Buffering**: Implementar buffers apropiados
- **Rate Limiting**: Controlar frecuencia de mensajes
- **Monitoring**: Supervisar rendimiento

### 🛠️ Mantenimiento
- **Testing**: Pruebas regulares de integración
- **Backup**: Respaldo de configuraciones
- **Updates**: Mantener sistemas actualizados
- **Documentation**: Documentar todas las integraciones

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
