# Integración MQTT - SWATID-A2 v4.1

> **Versión Firmware:** v4.1.0  
> **Última actualización:** Marzo 2026  
> **Broker:** MQTT 3.1.1 compatible  
> **Biblioteca:** PubSubClient 2.8

---

## Índice

1. [Arquitectura MQTT](#arquitectura-mqtt)
2. [Configuración de Conexión](#configuración-de-conexión)
3. [Estructura de Tópicos](#estructura-de-tópicos)
4. [Tipos de Mensajes](#tipos-de-mensajes)
5. [Validación de Acceso Remota](#validación-de-acceso-remota)
6. [Control de Relés](#control-de-relés)
7. [Gestión de Códigos](#gestión-de-códigos)
8. [Eventos y Notificaciones](#eventos-y-notificaciones)
9. [Robustez y Reconexión](#robustez-y-reconexión)
10. [Ejemplos Probados](#ejemplos-probados)

---

## Arquitectura MQTT

### Diagrama de Comunicación

```
┌─────────────────┐         MQTT          ┌─────────────────────┐
│   Backend/App   │◄──────────────────────►│   Broker MQTT       │
│   (Publisher/   │                        │  (devices.swat-id)  │
│   Subscriber)   │                        │                     │
└─────────────────┘                        └─────────────────────┘
                                                    ▲
                                                    │
                                                    ▼
                                           ┌─────────────────────┐
                                           │   SWATID-A2 (ESP32) │
                                           │   (Publisher/       │
                                           │   Subscriber)       │
                                           └─────────────────────┘
```

### Flujo de Comunicación

1. **Dispositivo → Broker**: Eventos, validaciones, errores
2. **Backend → Broker → Dispositivo**: Comandos, respuestas de validación
3. **Bidireccional**: Sincronización de códigos, configuración

---

## Configuración de Conexión

### Parámetros de Conexión

```cpp
const char* mqtt_broker = "devices.swat-id.com";
const int mqtt_port = 1883;
const char* mqtt_username = "user";
const char* mqtt_password = "password";

// Timeout de conexión (optimizado para no bloquear)
#define MQTT_CONNECT_TIMEOUT_SEC 5
```

### Inicialización Robusta

```cpp
void connectToMqtt() {
  // Verificar conexión Ethernet primero
  if (!ethConnected) {
    Serial.println("📡 MQTT: Sin conexión Ethernet, saltando");
    return;
  }
  
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(2048);  // Buffer grande para JSONs
  mqttClient.setSocketTimeout(MQTT_CONNECT_TIMEOUT_SEC);  // Timeout corto
  
  String clientId = "ESP32Client-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
    // Suscribirse a comandos
    String commandTopic = "swatidhome/command/" + fixedSerialNumber + "/#";
    mqttClient.subscribe(commandTopic.c_str());
  }
}
```

### Backoff Exponencial en Reconexión

```cpp
// En loop() - reconexión no bloqueante
static int reconnectBackoff = 5000;  // 5s inicial

if (!mqttClient.connected() && ethConnected) {
  if (currentTime - lastReconnectAttempt > reconnectBackoff) {
    lastReconnectAttempt = currentTime;
    connectToMqtt();
    
    // Backoff: 5s → 10s → 20s → 30s (max)
    if (!mqttClient.connected()) {
      reconnectBackoff = min(reconnectBackoff * 2, 30000);
    } else {
      reconnectBackoff = 5000;  // Reset en éxito
    }
  }
}
```

---

## Estructura de Tópicos

### Formato General

```
swatidhome/{tipo}/{serial}/{acción}
```

### Tópicos de Suscripción (Dispositivo escucha)

| Tópico | Descripción |
|--------|-------------|
| `swatidhome/command/{serial}/access` | Validación de acceso |
| `swatidhome/command/{serial}/granted` | Respuesta de acceso concedido |
| `swatidhome/command/{serial}/denied` | Respuesta de acceso denegado |
| `swatidhome/command/{serial}/relay` | Control de relés |
| `swatidhome/command/{serial}/config` | Configuración remota |
| `swatidhome/command/{serial}/codes` | Gestión de códigos |
| `swatidhome/command/{serial}/security` | Comandos de seguridad |

### Tópicos de Publicación (Dispositivo envía)

| Tópico | Descripción |
|--------|-------------|
| `swatidhome/command/{serial}/access` | Solicitud de validación |
| `swatidhome/events/{serial}/access` | Eventos de acceso |
| `swatidhome/events/{serial}/error` | Errores del sistema |
| `swatidhome/events/{serial}/codes` | Eventos de códigos |
| `swatidhome/events/{serial}/ble` | Eventos BLE |
| `swatidhome/events/{serial}/relay` | Eventos de relés |

---

## Tipos de Mensajes

### Estructura Base de Mensaje

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "message_id": 12345,
  "device": "SWATID_D8F7B4BF1388",
  "device_name": "Puerta Principal",
  "message_type": 0,
  "message_info": {
    // Contenido específico del mensaje
  }
}
```

### Tipos de Mensaje (message_type)

| Código | Tipo | Descripción |
|--------|------|-------------|
| 0 | Validación | Solicitud de validación de código |
| 1 | Relay Control | Control de relés |
| 2 | Error | Notificación de error |
| 3 | Info | Mensaje informativo |
| 4 | Config | Configuración |
| 5 | Security | Seguridad |
| 6 | Access Event | Evento de acceso |

---

## Validación de Acceso Remota

### Flujo de Validación

```
┌─────────────┐                  ┌─────────────┐                  ┌─────────────┐
│  Teclado    │                  │  SWATID-A2  │                  │   Backend   │
└──────┬──────┘                  └──────┬──────┘                  └──────┬──────┘
       │                                │                                │
       │ Código ingresado               │                                │
       ├───────────────────────────────►│                                │
       │                                │                                │
       │                                │ Publicar validación            │
       │                                ├───────────────────────────────►│
       │                                │                                │
       │                                │                   Verificar DB │
       │                                │                                │
       │                                │◄───────────────────────────────┤
       │                                │        granted/denied          │
       │                                │                                │
       │◄───────────────────────────────┤                                │
       │        Activar relé            │                                │
       │        (si granted)            │                                │
       │                                │                                │
```

### Solicitud de Validación (Dispositivo → Backend)

**Tópico:** `swatidhome/command/{serial}/access`

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "message_id": 12345,
  "device": "SWATID_D8F7B4BF1388",
  "device_name": "Puerta Principal",
  "message_type": 0,
  "message_info": {
    "source": "AUTO",
    "code_type": "PIN",
    "code_value": "123456",
    "keyboard_id": 1,
    "keyboard_name": "WIEGAND1",
    "keyboard_pins": "32/33",
    "request_relay": 1,
    "max_duration": 2.0
  }
}
```

### Respuesta de Acceso Concedido (Backend → Dispositivo)

**Tópico:** `swatidhome/command/{serial}/granted`

```json
{
  "timestamp": "2026-03-05T14:30:01Z",
  "message_id": 12345,
  "response": true,
  "relay": 1,
  "duration": 2.0,
  "user_name": "Juan García",
  "user_id": "usr_123456"
}
```

### Respuesta de Acceso Denegado (Backend → Dispositivo)

**Tópico:** `swatidhome/command/{serial}/denied`

```json
{
  "timestamp": "2026-03-05T14:30:01Z",
  "message_id": 12345,
  "response": false,
  "reason": "CODE_NOT_FOUND"
}
```

### Códigos de Denegación

| Código | Descripción |
|--------|-------------|
| `CODE_NOT_FOUND` | Código no existe en base de datos |
| `CODE_EXPIRED` | Código ha expirado |
| `CODE_DISABLED` | Código deshabilitado |
| `TIME_RESTRICTION` | Fuera de horario permitido |
| `DEVICE_DISABLED` | Dispositivo deshabilitado |

---

## Control de Relés

### Comando de Control (Backend → Dispositivo)

**Tópico:** `swatidhome/command/{serial}/relay`

**Activar relé:**
```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "message_id": 12346,
  "message_type": 1,
  "message_info": {
    "action": "activate",
    "relay": 1,
    "duration": 3.0
  }
}
```

**Desactivar relé:**
```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "message_id": 12347,
  "message_type": 1,
  "message_info": {
    "action": "deactivate",
    "relay": 1
  }
}
```

**Pulso:**
```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "message_id": 12348,
  "message_type": 1,
  "message_info": {
    "action": "pulse",
    "relay": 2,
    "duration": 5.0
  }
}
```

### Evento de Relé (Dispositivo → Backend)

**Tópico:** `swatidhome/events/{serial}/relay`

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "event": "RELAY_ACTIVATED",
  "relay": 1,
  "duration": 3.0,
  "source": "MQTT",
  "user": "remote_command"
}
```

---

## Gestión de Códigos

### Añadir Código Remoto

**Tópico:** `swatidhome/command/{serial}/codes`

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "message_id": 12349,
  "message_type": 4,
  "message_info": {
    "action": "add",
    "code_type": "PIN",
    "code_value": "789012",
    "keyboard_id": 1,
    "relay": 1,
    "user_name": "Empleado Nuevo",
    "expiry": "2026-12-31T23:59:59Z"
  }
}
```

### Eliminar Código

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "message_id": 12350,
  "message_type": 4,
  "message_info": {
    "action": "delete",
    "code_type": "PIN",
    "code_value": "789012"
  }
}
```

### Sincronizar Códigos (Bulk)

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "message_id": 12351,
  "message_type": 4,
  "message_info": {
    "action": "sync",
    "codes": [
      {"type": "PIN", "value": "111111", "keyboard": 1, "relay": 1},
      {"type": "PIN", "value": "222222", "keyboard": 2, "relay": 2},
      {"type": "TAG", "value": "12345678", "keyboard": 1, "relay": 1}
    ]
  }
}
```

### Evento de Código (Dispositivo → Backend)

**Tópico:** `swatidhome/events/{serial}/codes`

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "event": "CODE_ADDED",
  "source": "BLE",
  "user": "SUPERADMIN",
  "code_type": "PIN",
  "code_value": "990099",
  "keyboard": 1,
  "relay": 1,
  "total_codes": 4
}
```

---

## Eventos y Notificaciones

### Evento de Acceso Exitoso

**Tópico:** `swatidhome/events/{serial}/access`

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "event": "ACCESS_GRANTED",
  "code_type": "PIN",
  "code_value": "123456",
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1",
  "relay_activated": 1,
  "duration": 2.0,
  "validation_source": "LOCAL",
  "user_name": "Juan García"
}
```

### Evento de Acceso Fallido

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "event": "ACCESS_DENIED",
  "code_type": "PIN",
  "code_value": "999999",
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1",
  "reason": "INVALID_CODE",
  "failed_attempts": 2,
  "max_attempts": 3
}
```

### Evento de Error

**Tópico:** `swatidhome/events/{serial}/error`

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "error_code": 7,
  "description": "Error publicando validación remota para PIN 123456",
  "severity": "warning"
}
```

### Códigos de Error

| Código | Descripción |
|--------|-------------|
| 1 | Error de conexión Ethernet |
| 2 | Formato Wiegand desconocido |
| 3 | Dispositivo iniciado (info) |
| 4 | Configuración reseteada |
| 5 | Error de EEPROM |
| 6 | Acceso local bloqueado |
| 7 | Error de publicación MQTT |

### Evento BLE

**Tópico:** `swatidhome/events/{serial}/ble`

```json
{
  "timestamp": "2026-03-05T14:30:00Z",
  "event": "AUTH_TIMEOUT",
  "device": "SWATID_D8F7B4BF1388",
  "reason": "no_authentication_received",
  "timeout_ms": 30000
}
```

---

## Robustez y Reconexión

### Comportamiento sin MQTT

El sistema está diseñado para funcionar **sin dependencia de MQTT**:

1. **Códigos locales siempre funcionan**: `localValidationFirst = true` por defecto
2. **Fallback automático**: Si MQTT falla, usa validación local
3. **Reconexión en background**: No bloquea operaciones principales
4. **Backoff exponencial**: Evita saturar recursos

### Flujo de Validación con Fallback

```cpp
void validateCode(const String& code, const String& type, int keyboardId) {
  bool localFound = isCodeStored(type.c_str(), code.c_str(), keyboardId, &relay);
  
  // Prioridad 1: Validación local si está habilitada
  if (storedCodes->localValidationFirst && localFound) {
    controlReleWithDuration(releDuration, relay);
    return;
  }
  
  // Prioridad 2: Validación remota si hay MQTT
  if (mqttClient.connected()) {
    bool published = mqttClient.publish(topic, message);
    
    if (!published && localFound) {
      // Fallback local si falla publicación
      controlReleWithDuration(releDuration, relay);
    }
  } else if (localFound) {
    // Sin MQTT pero código local existe
    controlReleWithDuration(releDuration, relay);
  }
}
```

### Log de Modo Autónomo

```
🏠 MODO AUTÓNOMO: Sistema funcionando sin conectividad Ethernet
   Códigos locales disponibles: 5/50
   Modo validación: Local primero
   ✅ Sistema completamente operativo en modo autónomo
```

---

## Ejemplos Probados

### Ejemplo 1: Validación Remota Exitosa

**Log de Firmware:**
```
🔐 [TECLADO 1] === DATOS WIEGAND RECIBIDOS ===
🔐 [TECLADO 1] Bits: 4, Valor: 0x00000005
🔑 [TECLADO 1] Tecla detectada: 5
🔍 [WIEGAND1] Validando: 123456 (PIN)
📡 [WIEGAND1] Enviando para validación REMOTA
📤 Enviando validación remota:
   Tópico: swatidhome/command/SWATID_D8F7B4BF1388/access
   Tamaño mensaje: 312 bytes
✅ Mensaje publicado correctamente

📨 [MQTT] Mensaje recibido [swatidhome/command/SWATID_D8F7B4BF1388/granted]: 
{"response":true,"relay":1,"duration":2.0,"user_name":"Juan"}
✅ Validación remota CONCEDIDA - Activando relé 1
🔓 Relé 1 ACTIVADO (2.0s) - Validación remota
```

### Ejemplo 2: Fallback Local por MQTT Desconectado

**Log de Firmware:**
```
🔐 [TECLADO 1] === DATOS WIEGAND RECIBIDOS ===
🔑 [TECLADO 1] Tecla detectada: #
🔍 [WIEGAND1] Validando: 654321 (PIN)
🔄 [WIEGAND1] Fallback LOCAL (MQTT desconectado) - Relé 2
🔓 Relé 2 ACTIVADO (2.0s) - Acceso local
```

### Ejemplo 3: Código Añadido por BLE y Evento MQTT

**Log de Firmware:**
```
🔵 [BLE] FF04 - Recibido
🔵 [BLE] FF04 - Tipo: PIN, Código: 990099, Teclado: 1, Relé: 1
🔵 [BLE] FF04 - Código añadido a RAM (total: 4)
💾 [LOOP] Guardando código pendiente en EEPROM...
💾 [LOOP] Código '990099' guardado en EEPROM
📤 [MQTT] Publicando evento: swatidhome/events/SWATID_D8F7B4BF1388/codes
   {"event":"CODE_ADDED","source":"BLE","user":"SUPERADMIN","code_type":"PIN",...}
```

### Ejemplo 4: Reconexión MQTT con Backoff

**Log de Firmware:**
```
🌐 ETH Dirección IP: 192.168.5.86
🌐 MQTT conexión programada para loop()
📡 Conectando a MQTT (timeout: 5s)... ❌ Error rc=-2 (5012ms)
   -> MQTT_CONNECT_FAILED
🔄 Reconexión MQTT (backoff: 10s)...
📡 Conectando a MQTT (timeout: 5s)... ❌ Error rc=-2 (5008ms)
🔄 Reconexión MQTT (backoff: 20s)...
📡 Conectando a MQTT (timeout: 5s)... ✅ Conectado en 234ms
📡 Suscrito a: swatidhome/command/SWATID_D8F7B4BF1388/#
```

### Ejemplo 5: Control Remoto de Relé

**Mensaje recibido:**
```json
{
  "message_type": 1,
  "message_info": {
    "action": "pulse",
    "relay": 1,
    "duration": 3.0
  }
}
```

**Log de Firmware:**
```
📨 [MQTT] Mensaje recibido [swatidhome/command/SWATID_D8F7B4BF1388/relay]:
{"message_type":1,"message_info":{"action":"pulse","relay":1,"duration":3.0}}
🔓 Relé 1 ACTIVADO (3.0s) - Comando remoto MQTT
```

---

## Configuración Remota

### Cambiar Modo de Validación

**Tópico:** `swatidhome/command/{serial}/config`

```json
{
  "message_type": 4,
  "message_info": {
    "validation_mode": "local_first"
  }
}
```

### Cambiar Tiempo de Relé

```json
{
  "message_type": 4,
  "message_info": {
    "relay_duration": 5.0
  }
}
```

### Cambiar Configuración de Red

```json
{
  "message_type": 4,
  "message_info": {
    "use_dhcp": false,
    "static_ip": "192.168.5.100",
    "gateway": "192.168.5.1",
    "subnet": "255.255.255.0",
    "dns": "8.8.8.8"
  }
}
```

**Nota:** Cambios de red causan reinicio automático.

---

## Comandos de Seguridad

**Tópico:** `swatidhome/command/{serial}/security`

### Bloquear Acceso Local

```json
{
  "message_type": 5,
  "message_info": {
    "action": "block_local_access"
  }
}
```

### Desbloquear Acceso Local

```json
{
  "message_type": 5,
  "message_info": {
    "action": "unblock_local_access"
  }
}
```

### Deshabilitar Lectura de Teclados

```json
{
  "message_type": 5,
  "message_info": {
    "action": "disable_keyboard_reading"
  }
}
```

### Configurar Bloqueo por Intentos Fallidos

```json
{
  "message_type": 5,
  "message_info": {
    "action": "set_block_duration",
    "duration_seconds": 120
  }
}
```

```json
{
  "message_type": 5,
  "message_info": {
    "action": "set_max_attempts",
    "max_attempts": 5
  }
}
```

---

## Resumen de Tópicos

### Suscripciones del Dispositivo

```
swatidhome/command/SWATID_XXXXXXXXXXXX/#
```

### Publicaciones del Dispositivo

| Tópico | Uso |
|--------|-----|
| `swatidhome/command/{serial}/access` | Solicitud validación |
| `swatidhome/events/{serial}/access` | Evento acceso |
| `swatidhome/events/{serial}/error` | Errores |
| `swatidhome/events/{serial}/codes` | Eventos códigos |
| `swatidhome/events/{serial}/ble` | Eventos BLE |
| `swatidhome/events/{serial}/relay` | Eventos relé |

---

**Estado:** ✅ DOCUMENTACIÓN COMPLETA - FIRMWARE v4.1.0
