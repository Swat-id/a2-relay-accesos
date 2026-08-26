# Ejemplos de Integración MQTT - KC868A2

## Descripción
Ejemplos prácticos de integración con el sistema KC868A2 utilizando diferentes lenguajes de programación y plataformas, incluyendo casos de uso comunes y mejores prácticas.

## Integración con Python

### 🐍 Cliente Básico de Monitoreo

```python
import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime

class KC868A2Monitor:
    def __init__(self, broker="188.245.213.181", port=1883):
        self.broker = broker
        self.port = port
        self.client = mqtt.Client()
        self.client.username_pw_set("swatidhome", "Swatid2025!")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.devices = {}
        
    def on_connect(self, client, userdata, flags, rc):
        print(f"Conectado al broker MQTT con código: {rc}")
        # Suscribirse a todos los eventos
        client.subscribe("swatidhome/events/+/access")
        client.subscribe("swatidhome/events/+/failed_access")
        client.subscribe("swatidhome/errors/+/rx")
        client.subscribe("swatidhome/+/relay_enable")
        
    def on_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode())
            topic = msg.topic
            
            if "/access" in topic:
                self.handle_access_event(data)
            elif "/failed_access" in topic:
                self.handle_failed_access(data)
            elif "/errors/" in topic:
                self.handle_error(data)
            elif "/relay_enable" in topic:
                self.handle_relay_status(data)
                
        except json.JSONDecodeError as e:
            print(f"Error decodificando JSON: {e}")
            
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
        
    def handle_error(self, data):
        error_code = data.get("error_code", 0)
        description = data.get("description", "Sin descripción")
        
        print(f"🚨 ERROR {error_code}: {description}")
        
    def handle_relay_status(self, data):
        relay = data.get("rele", 0)
        estado = data.get("estado", "UNKNOWN")
        origen = data.get("origen", "UNKNOWN")
        
        print(f"⚡ Relé {relay}: {estado} (origen: {origen})")
        
    def start_monitoring(self):
        self.client.connect(self.broker, self.port, 60)
        self.client.loop_forever()

# Uso
if __name__ == "__main__":
    monitor = KC868A2Monitor()
    monitor.start_monitoring()
```

### 🎮 Control de Relés

```python
import paho.mqtt.client as mqtt
import json
import time

class KC868A2Controller:
    def __init__(self, device_serial, broker="188.245.213.181", port=1883):
        self.device_serial = device_serial
        self.broker = broker
        self.port = port
        self.client = mqtt.Client()
        self.client.username_pw_set("swatidhome", "Swatid2025!")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
    def on_connect(self, client, userdata, flags, rc):
        print(f"Conectado al broker MQTT")
        # Suscribirse a respuestas del dispositivo
        client.subscribe(f"swatidhome/response/{self.device_serial}/rx")
        
    def on_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode())
            print(f"Respuesta recibida: {data.get('response_info', 'Sin información')}")
        except json.JSONDecodeError as e:
            print(f"Error decodificando respuesta: {e}")
            
    def activate_relay(self, relay_number, duration=2.0):
        """Activar un relé específico"""
        message = {
            "message_id": int(time.time()),
            "device": self.device_serial,
            "message_type": 0,
            "message_info": {
                "relay_number": relay_number,
                "duration": duration
            }
        }
        
        topic = f"swatidhome/command/{self.device_serial}/relay"
        result = self.client.publish(topic, json.dumps(message))
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"Comando enviado: Relé {relay_number} por {duration}s")
        else:
            print(f"Error enviando comando: {result.rc}")
            
    def get_device_info(self):
        """Solicitar información del dispositivo"""
        message = {
            "message_id": int(time.time()),
            "device": self.device_serial,
            "message_type": 1
        }
        
        topic = f"swatidhome/command/{self.device_serial}/info"
        self.client.publish(topic, json.dumps(message))
        print("Solicitud de información enviada")
        
    def reboot_device(self):
        """Reiniciar el dispositivo"""
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
        """Bloquear acceso local"""
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
        """Desbloquear acceso local"""
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
    controller = KC868A2Controller("SWATID_12345678")
    controller.connect()
    
    # Ejemplos de uso
    controller.activate_relay(1, 3.0)  # Activar relé 1 por 3 segundos
    time.sleep(1)
    controller.get_device_info()        # Solicitar información
    time.sleep(1)
    controller.block_local_access()     # Bloquear acceso local
    
    time.sleep(5)
    controller.disconnect()
```

## Integración con Node.js

### 🟨 Servidor de Monitoreo Web

```javascript
const mqtt = require('mqtt');
const express = require('express');
const WebSocket = require('ws');
const app = express();
const server = require('http').createServer(app);
const wss = new WebSocket.Server({ server });

// Configuración MQTT
const mqttClient = mqtt.connect('mqtt://188.245.213.181:1883', {
  username: 'swatidhome',
  password: 'Swatid2025!'
});

// Almacenamiento de eventos
const events = [];
const devices = new Map();

// Conexión MQTT
mqttClient.on('connect', () => {
  console.log('Conectado al broker MQTT');
  
  // Suscribirse a todos los eventos
  mqttClient.subscribe('swatidhome/events/+/access');
  mqttClient.subscribe('swatidhome/events/+/failed_access');
  mqttClient.subscribe('swatidhome/errors/+/rx');
  mqttClient.subscribe('swatidhome/+/relay_enable');
  mqttClient.subscribe('swatidhome/+/info');
});

// Procesamiento de mensajes MQTT
mqttClient.on('message', (topic, message) => {
  try {
    const data = JSON.parse(message.toString());
    const event = {
      timestamp: new Date().toISOString(),
      topic: topic,
      data: data
    };
    
    // Almacenar evento
    events.push(event);
    if (events.length > 1000) {
      events.shift(); // Mantener solo los últimos 1000 eventos
    }
    
    // Extraer serial del tópico
    const serialMatch = topic.match(/swatidhome\/([^\/]+)/);
    if (serialMatch) {
      const serial = serialMatch[1];
      if (!devices.has(serial)) {
        devices.set(serial, {
          serial: serial,
          lastSeen: new Date(),
          status: 'online'
        });
      }
      devices.get(serial).lastSeen = new Date();
    }
    
    // Enviar a clientes WebSocket
    wss.clients.forEach(client => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(JSON.stringify(event));
      }
    });
    
  } catch (error) {
    console.error('Error procesando mensaje MQTT:', error);
  }
});

// WebSocket para tiempo real
wss.on('connection', (ws) => {
  console.log('Cliente WebSocket conectado');
  
  // Enviar eventos recientes
  const recentEvents = events.slice(-50);
  ws.send(JSON.stringify({ type: 'history', events: recentEvents }));
  
  ws.on('close', () => {
    console.log('Cliente WebSocket desconectado');
  });
});

// API REST
app.use(express.json());
app.use(express.static('public'));

// Obtener eventos
app.get('/api/events', (req, res) => {
  const limit = parseInt(req.query.limit) || 100;
  const eventsToSend = events.slice(-limit);
  res.json(eventsToSend);
});

// Obtener dispositivos
app.get('/api/devices', (req, res) => {
  const deviceList = Array.from(devices.values());
  res.json(deviceList);
});

// Activar relé
app.post('/api/relay/:serial/:relay', (req, res) => {
  const { serial, relay } = req.params;
  const { duration = 2.0 } = req.body;
  
  const message = {
    message_id: Date.now(),
    device: serial,
    message_type: 0,
    message_info: {
      relay_number: parseInt(relay),
      duration: parseFloat(duration)
    }
  };
  
  const topic = `swatidhome/command/${serial}/relay`;
  mqttClient.publish(topic, JSON.stringify(message));
  
  res.json({ success: true, message: 'Comando enviado' });
});

// Obtener información del dispositivo
app.get('/api/device/:serial/info', (req, res) => {
  const { serial } = req.params;
  
  const message = {
    message_id: Date.now(),
    device: serial,
    message_type: 1
  };
  
  const topic = `swatidhome/command/${serial}/info`;
  mqttClient.publish(topic, JSON.stringify(message));
  
  res.json({ success: true, message: 'Solicitud enviada' });
});

// Página web
app.get('/', (req, res) => {
  res.sendFile(__dirname + '/public/index.html');
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`Servidor ejecutándose en puerto ${PORT}`);
});
```

### 📱 Interfaz Web (HTML/JavaScript)

```html
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>KC868A2 Monitor</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        .event { margin: 10px 0; padding: 10px; border: 1px solid #ddd; border-radius: 5px; }
        .success { background-color: #d4edda; border-color: #c3e6cb; }
        .error { background-color: #f8d7da; border-color: #f5c6cb; }
        .info { background-color: #d1ecf1; border-color: #bee5eb; }
        .controls { margin: 20px 0; }
        .controls button { margin: 5px; padding: 10px 15px; }
        .devices { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .device { border: 1px solid #ddd; padding: 15px; border-radius: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>KC868A2 Monitor</h1>
        
        <div class="controls">
            <h2>Controles</h2>
            <div>
                <label>Serial: <input type="text" id="deviceSerial" value="SWATID_12345678"></label>
                <button onclick="getDeviceInfo()">Obtener Info</button>
                <button onclick="activateRelay(1)">Relé 1</button>
                <button onclick="activateRelay(2)">Relé 2</button>
            </div>
        </div>
        
        <div class="devices" id="devices"></div>
        
        <div>
            <h2>Eventos en Tiempo Real</h2>
            <div id="events"></div>
        </div>
    </div>

    <script>
        const ws = new WebSocket('ws://localhost:3000');
        const eventsDiv = document.getElementById('events');
        const devicesDiv = document.getElementById('devices');
        
        ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            
            if (data.type === 'history') {
                data.events.forEach(addEvent);
            } else {
                addEvent(data);
            }
        };
        
        function addEvent(event) {
            const eventDiv = document.createElement('div');
            eventDiv.className = 'event';
            
            let className = 'info';
            if (event.topic.includes('access') && event.data.success) {
                className = 'success';
            } else if (event.topic.includes('failed_access') || event.topic.includes('errors')) {
                className = 'error';
            }
            
            eventDiv.className += ' ' + className;
            
            const time = new Date(event.timestamp).toLocaleTimeString();
            const topic = event.topic.split('/').pop();
            
            eventDiv.innerHTML = `
                <strong>${time}</strong> - ${topic}<br>
                ${JSON.stringify(event.data, null, 2)}
            `;
            
            eventsDiv.insertBefore(eventDiv, eventsDiv.firstChild);
            
            // Mantener solo los últimos 50 eventos
            while (eventsDiv.children.length > 50) {
                eventsDiv.removeChild(eventsDiv.lastChild);
            }
        }
        
        function getDeviceInfo() {
            const serial = document.getElementById('deviceSerial').value;
            fetch(`/api/device/${serial}/info`, { method: 'GET' })
                .then(response => response.json())
                .then(data => console.log('Info solicitada:', data));
        }
        
        function activateRelay(relay) {
            const serial = document.getElementById('deviceSerial').value;
            fetch(`/api/relay/${serial}/${relay}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ duration: 2.0 })
            })
            .then(response => response.json())
            .then(data => console.log('Relé activado:', data));
        }
        
        // Cargar dispositivos
        fetch('/api/devices')
            .then(response => response.json())
            .then(devices => {
                devicesDiv.innerHTML = devices.map(device => `
                    <div class="device">
                        <h3>${device.serial}</h3>
                        <p>Última vez visto: ${new Date(device.lastSeen).toLocaleString()}</p>
                        <p>Estado: ${device.status}</p>
                    </div>
                `).join('');
            });
    </script>
</body>
</html>
```

## Integración con C++ (ESP32)

### 🔧 Cliente MQTT para ESP32

```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

class KC868A2Client {
private:
    WiFiClient espClient;
    PubSubClient mqttClient;
    String deviceSerial;
    String deviceName;
    
public:
    KC868A2Client(String serial, String name) : 
        mqttClient(espClient), deviceSerial(serial), deviceName(name) {
        mqttClient.setServer("188.245.213.181", 1883);
        mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->onMqttMessage(topic, payload, length);
        });
    }
    
    void onMqttMessage(char* topic, byte* payload, unsigned int length) {
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload, length);
        
        String topicStr = String(topic);
        
        if (topicStr.endsWith("/access")) {
            handleAccessEvent(doc);
        } else if (topicStr.endsWith("/failed_access")) {
            handleFailedAccess(doc);
        } else if (topicStr.endsWith("/relay_enable")) {
            handleRelayStatus(doc);
        } else if (topicStr.endsWith("/info")) {
            handleDeviceInfo(doc);
        }
    }
    
    void handleAccessEvent(JsonDocument& doc) {
        bool success = doc["success"];
        String source = doc["source"];
        String codeType = doc["code_type"];
        String codeValue = doc["code_value"];
        
        Serial.printf("Acceso: %s - %s - %s:%s\n", 
                     success ? "ÉXITO" : "FALLO", 
                     source.c_str(), 
                     codeType.c_str(), 
                     codeValue.c_str());
    }
    
    void handleFailedAccess(JsonDocument& doc) {
        String reason = doc["reason"];
        String codeType = doc["code_type"];
        String codeValue = doc["code_value"];
        int attempts = doc["failed_attempts"];
        int maxAttempts = doc["max_attempts"];
        
        Serial.printf("Acceso fallido: %s - %s:%s - %d/%d\n", 
                     reason.c_str(), 
                     codeType.c_str(), 
                     codeValue.c_str(), 
                     attempts, 
                     maxAttempts);
    }
    
    void handleRelayStatus(JsonDocument& doc) {
        int relay = doc["rele"];
        String estado = doc["estado"];
        String origen = doc["origen"];
        
        Serial.printf("Relé %d: %s (origen: %s)\n", 
                     relay, 
                     estado.c_str(), 
                     origen.c_str());
    }
    
    void handleDeviceInfo(JsonDocument& doc) {
        String device = doc["device"];
        String serial = doc["serial"];
        String ip = doc["ip"];
        String firmware = doc["firmware_version"];
        
        Serial.printf("Dispositivo: %s (%s) - IP: %s - FW: %s\n", 
                     device.c_str(), 
                     serial.c_str(), 
                     ip.c_str(), 
                     firmware.c_str());
    }
    
    bool connect() {
        if (!mqttClient.connected()) {
            String clientId = "ESP32Client-" + String(random(0xffff), HEX);
            
            if (mqttClient.connect(clientId.c_str(), "swatidhome", "Swatid2025!")) {
                Serial.println("Conectado al broker MQTT");
                
                // Suscribirse a eventos
                String commandTopic = "swatidhome/command/" + deviceSerial + "/#";
                mqttClient.subscribe(commandTopic.c_str());
                
                // Suscribirse a eventos globales
                mqttClient.subscribe("swatidhome/events/+/access");
                mqttClient.subscribe("swatidhome/events/+/failed_access");
                mqttClient.subscribe("swatidhome/+/relay_enable");
                
                return true;
            } else {
                Serial.printf("Error conectando a MQTT: %d\n", mqttClient.state());
                return false;
            }
        }
        return true;
    }
    
    void activateRelay(int relay, float duration) {
        DynamicJsonDocument doc(1024);
        doc["message_id"] = millis();
        doc["device"] = deviceSerial;
        doc["message_type"] = 0;
        
        JsonObject msgInfo = doc.createNestedObject("message_info");
        msgInfo["relay_number"] = relay;
        msgInfo["duration"] = duration;
        
        String message;
        serializeJson(doc, message);
        
        String topic = "swatidhome/command/" + deviceSerial + "/relay";
        mqttClient.publish(topic.c_str(), message.c_str());
        
        Serial.printf("Comando enviado: Relé %d por %.1fs\n", relay, duration);
    }
    
    void getDeviceInfo() {
        DynamicJsonDocument doc(512);
        doc["message_id"] = millis();
        doc["device"] = deviceSerial;
        doc["message_type"] = 1;
        
        String message;
        serializeJson(doc, message);
        
        String topic = "swatidhome/command/" + deviceSerial + "/info";
        mqttClient.publish(topic.c_str(), message.c_str());
        
        Serial.println("Solicitud de información enviada");
    }
    
    void loop() {
        if (!mqttClient.connected()) {
            connect();
        }
        mqttClient.loop();
    }
};

// Uso
KC868A2Client client("SWATID_12345678", "MiDispositivo");

void setup() {
    Serial.begin(115200);
    WiFi.begin("SSID", "PASSWORD");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Conectando a WiFi...");
    }
    
    Serial.println("WiFi conectado");
    client.connect();
}

void loop() {
    client.loop();
    
    // Ejemplo: activar relé cada 30 segundos
    static unsigned long lastActivation = 0;
    if (millis() - lastActivation > 30000) {
        client.activateRelay(1, 2.0);
        lastActivation = millis();
    }
}
```

## Integración con Home Assistant

### 🏠 Configuración de Home Assistant

```yaml
# configuration.yaml
mqtt:
  broker: 188.245.213.181
  port: 1883
  username: swatidhome
  password: Swatid2025!

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

- platform: mqtt
  name: "KC868A2 Failed Access"
  state_topic: "swatidhome/events/+/failed_access"
  value_template: "{{ value_json.reason }}"
  json_attributes:
    - code_type
    - code_value
    - failed_attempts
    - max_attempts

# switches.yaml
- platform: mqtt
  name: "KC868A2 Relay 1"
  command_topic: "swatidhome/command/SWATID_12345678/relay"
  state_topic: "swatidhome/SWATID_12345678/relay_enable"
  value_template: "{{ value_json.estado }}"
  payload_on: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 0, "message_info": {"relay_number": 1, "duration": 2.0}}'
  payload_off: '{"message_id": 1, "device": "SWATID_12345678", "message_type": 0, "message_info": {"relay_number": 1, "duration": 0.0}}'

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
