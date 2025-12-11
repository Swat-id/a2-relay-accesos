# Análisis: Problema de Timeout en Modo Torno con Validación Local Primero

## 🔍 Problema Identificado

### Descripción del Problema
Cuando el dispositivo está en modo torno con configuración "Primero Local" y se introduce un código que **NO existe localmente**, el sistema:

1. ✅ Envía correctamente la solicitud de acceso a MQTT para validación remota
2. ❌ Si no hay respuesta en el timeout (5 segundos), publica un evento `ACCESS_DENIED` 
3. ❌ **PERO** este mensaje va al tópico de eventos, no al tópico donde el backend puede responder

### Flujo Actual (Incorrecto)

```
1. Usuario introduce código "4444" en WIEGAND2
2. Sistema busca código localmente: NO ENCONTRADO
3. Sistema envía solicitud a: swatidhome/command/{SERIAL}/access
   - message_type: 0 (solicitud de validación)
4. Backend NO responde en 5 segundos
5. Sistema publica evento a: swatidhome/event/{DEVICE}/turnstile
   - event_type: ACCESS_DENIED
   - reason: TORNO_TIMEOUT_DENIED
6. ❌ Backend NO puede responder porque ya expiró la solicitud
```

### Problema de Arquitectura

El sistema actual tiene dos tipos de mensajes diferentes:

| Tipo | Propósito | Tópico | message_type |
|------|-----------|---------|--------------|
| **Solicitud** | Pedir validación | `swatidhome/command/{SERIAL}/access` | 0 |
| **Evento** | Notificar resultado | `swatidhome/event/{DEVICE}/turnstile` | 1 |

**El problema**: Cuando hay timeout, se publica un **evento**, pero el backend necesita **responder a una solicitud**.

## 📊 Análisis de Mensajes

### Mensaje de Solicitud Actual (Correcto)
```json
{
  "timestamp": "2025-10-13T09:04:46+01:00",
  "message_id": 39,
  "device": "SWATID_584614BBBC2C",
  "device_name": "SWATID_584614BBBC2C",
  "message_type": 0,  // ← Solicitud
  "mode": "turnstile",
  "message_info": {
    "source": "AUTO",
    "code_type": "PIN",
    "code_value": "4444",
    "keyboard_id": 2,
    "keyboard_name": "WIEGAND2",
    "keyboard_pins": "13/12",
    "request_relay": 1,      // ← Siempre 1 para MQTT
    "actual_relay": 2,       // ← Relé real que se abrirá
    "max_duration": 5
  },
  "turnstile_info": {
    "enabled": true,
    "keyboard1_relay": 1,
    "keyboard2_relay": 2,
    "timeout": 5000,
    "pending_request": true
  }
}
```

### Mensaje de Timeout Actual (Incorrecto)
```json
{
  "timestamp": "2025-10-13T09:04:51+01:00",
  "message_id": 40,
  "device": "SWATID_584614BBBC2C",
  "device_name": "SWATID_584614BBBC2C",
  "message_type": 1,  // ← Evento (no solicitud)
  "mode": "turnstile",
  "event_type": "ACCESS_DENIED",
  "event_info": {
    "code_type": "PIN",
    "code_value": "4444",
    "keyboard_id": 2,
    "keyboard_name": "WIEGAND2",
    "success": false,
    "reason": "TORNO_TIMEOUT_DENIED",
    "relay_opened": 0,
    "duration": 0
  },
  "turnstile_info": {
    "enabled": true,
    "keyboard1_relay": 1,
    "keyboard2_relay": 2,
    "timeout": 5000
  }
}
```

## 🎯 Soluciones Propuestas

### Opción 1: Mantener Solicitud Activa Después del Timeout (Recomendada)

**Concepto**: No cancelar la solicitud pendiente inmediatamente después del timeout. Permitir que el backend responda incluso después del timeout.

**Ventajas**:
- ✅ Permite respuestas tardías del backend
- ✅ Más flexible para redes lentas
- ✅ No requiere cambios en el backend

**Implementación**:
```cpp
void checkPendingRequestTimeout() {
  if (!isTurnstileModeEnabled() || !pendingRequest.active) {
    return;
  }
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - pendingRequest.timestamp;
  
  if (elapsedTime > TURNSTILE_TIMEOUT) {
    String keyboardName = (pendingRequest.keyboard_id == 1) ? "WIEGAND1" : "WIEGAND2";
    
    Serial.printf("⏰ [TORNO] [%s] Timeout de solicitud (%lu ms)\n", 
                  keyboardName.c_str(), elapsedTime);
    
    // NO incrementar intentos fallidos aún
    // NO publicar evento de acceso denegado aún
    
    // Publicar evento de TIMEOUT (informativo)
    publishTurnstileTimeoutEvent(String(pendingRequest.code), 
                                 String(pendingRequest.type), 
                                 pendingRequest.keyboard_id);
    
    // MANTENER solicitud activa para respuesta tardía
    // El backend aún puede responder
    Serial.println("📡 [TORNO] Solicitud sigue activa - Backend puede responder");
  }
  
  // Timeout máximo (ej: 30 segundos)
  if (elapsedTime > TURNSTILE_MAX_TIMEOUT) {
    Serial.printf("❌ [TORNO] [%s] Timeout máximo alcanzado - Código DENEGADO\n", 
                  keyboardName.c_str());
    
    // Ahora sí, denegar acceso
    failedAttempts++;
    lastFailedAttempt = millis();
    
    publishTurnstileEvent(String(pendingRequest.code), 
                         String(pendingRequest.type), 
                         pendingRequest.keyboard_id, 
                         false, 
                         "TORNO_MAX_TIMEOUT_DENIED");
    
    clearPendingRequest();
  }
}
```

### Opción 2: Reenviar Solicitud Después del Timeout

**Concepto**: Si no hay respuesta en el timeout inicial, reenviar la solicitud automáticamente.

**Ventajas**:
- ✅ Maneja casos de pérdida de paquetes
- ✅ Aumenta confiabilidad

**Desventajas**:
- ❌ Más tráfico MQTT
- ❌ Puede causar duplicados

**Implementación**:
```cpp
void checkPendingRequestTimeout() {
  if (!isTurnstileModeEnabled() || !pendingRequest.active) {
    return;
  }
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - pendingRequest.timestamp;
  
  if (elapsedTime > TURNSTILE_TIMEOUT && pendingRequest.retry_count < MAX_RETRIES) {
    Serial.printf("🔄 [TORNO] Timeout - Reintentando solicitud (%d/%d)\n", 
                  pendingRequest.retry_count + 1, MAX_RETRIES);
    
    // Reenviar solicitud
    String message = createTurnstileMqttMessage(
      String(pendingRequest.code),
      String(pendingRequest.type),
      pendingRequest.keyboard_id,
      pendingRequest.relay_to_open
    );
    
    String topic = "swatidhome/command/" + fixedSerialNumber + "/access";
    mqttClient.publish(topic.c_str(), message.c_str(), false);
    
    // Actualizar timestamp y contador de reintentos
    pendingRequest.timestamp = millis();
    pendingRequest.retry_count++;
  }
  
  if (elapsedTime > TURNSTILE_TIMEOUT * (MAX_RETRIES + 1)) {
    // Todos los reintentos fallaron
    Serial.println("❌ [TORNO] Máximo de reintentos alcanzado - Código DENEGADO");
    
    failedAttempts++;
    lastFailedAttempt = millis();
    
    publishTurnstileEvent(String(pendingRequest.code), 
                         String(pendingRequest.type), 
                         pendingRequest.keyboard_id, 
                         false, 
                         "TORNO_MAX_RETRIES_EXCEEDED");
    
    clearPendingRequest();
  }
}
```

### Opción 3: Timeout Configurable por Backend

**Concepto**: Permitir que el backend configure el timeout a través de MQTT.

**Ventajas**:
- ✅ Máxima flexibilidad
- ✅ Adaptable a diferentes redes

**Implementación**:
```cpp
// En Config struct
struct {
  // ... otros campos
  unsigned long response_timeout;  // Timeout configurable
  unsigned long max_timeout;       // Timeout máximo
  uint8_t max_retries;            // Número de reintentos
} turnstile;

// Comando MQTT para configurar
{
  "message_type": 7,  // Configuración de timeout
  "timeout_config": {
    "response_timeout": 5000,   // 5 segundos
    "max_timeout": 30000,       // 30 segundos
    "max_retries": 2
  }
}
```

## 📋 Recomendación

**Implementar Opción 1: Mantener Solicitud Activa**

### Razones:
1. **Simplicidad**: Cambio mínimo en el código actual
2. **Compatibilidad**: No requiere cambios en el backend
3. **Flexibilidad**: Permite respuestas tardías sin complejidad adicional
4. **Mejor UX**: El usuario puede recibir acceso incluso si la red es lenta

### Cambios Necesarios:

1. **Añadir timeout máximo** además del timeout de notificación
2. **Publicar evento informativo** de timeout (no de denegación)
3. **Mantener solicitud activa** hasta timeout máximo
4. **Denegar acceso** solo después del timeout máximo

### Constantes a Definir:
```cpp
#define TURNSTILE_TIMEOUT 5000        // 5 segundos - timeout de notificación
#define TURNSTILE_MAX_TIMEOUT 30000   // 30 segundos - timeout máximo
```

### Flujo Mejorado:
```
1. Usuario introduce código "4444" en WIEGAND2
2. Sistema busca código localmente: NO ENCONTRADO
3. Sistema envía solicitud a: swatidhome/command/{SERIAL}/access
4. Espera 5 segundos (TURNSTILE_TIMEOUT)
5. Si no hay respuesta: Publica evento TIMEOUT_WAITING
6. ✅ Solicitud SIGUE ACTIVA
7. Backend puede responder hasta 30 segundos (TURNSTILE_MAX_TIMEOUT)
8. Si responde APPROVED: Abre relé
9. Si responde DENIED: Deniega acceso
10. Si llega a 30 segundos sin respuesta: Deniega acceso definitivamente
```

## 🔧 Implementación Detallada

### Estructura de Datos Actualizada:
```cpp
struct {
  bool active;
  int keyboard_id;
  char code[32];
  char type[16];
  unsigned long timestamp;
  uint8_t relay_to_open;
  bool timeout_notified;    // ← Nuevo: si se notificó el timeout
  uint8_t retry_count;      // ← Nuevo: contador de reintentos (opcional)
} pendingRequest;
```

### Función de Timeout Mejorada:
Ver implementación en Opción 1.

### Nueva Función de Evento de Timeout:
```cpp
void publishTurnstileTimeoutEvent(const String& code, const String& type, int keyboardId) {
  if (!mqttClient.connected()) {
    return;
  }
  
  String keyboardName = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
  
  DynamicJsonDocument doc(1024);
  doc["timestamp"] = getTimestamp();
  doc["message_id"] = messageId++;
  doc["device"] = fixedSerialNumber;
  doc["device_name"] = deviceName;
  doc["message_type"] = 1;  // Evento
  doc["mode"] = "turnstile";
  doc["event_type"] = "TIMEOUT_WAITING";  // ← Nuevo tipo de evento
  
  JsonObject eventInfo = doc.createNestedObject("event_info");
  eventInfo["code_type"] = type;
  eventInfo["code_value"] = code;
  eventInfo["keyboard_id"] = keyboardId;
  eventInfo["keyboard_name"] = keyboardName;
  eventInfo["status"] = "WAITING_RESPONSE";
  eventInfo["elapsed_time"] = TURNSTILE_TIMEOUT;
  eventInfo["max_wait_time"] = TURNSTILE_MAX_TIMEOUT;
  
  String message;
  serializeJson(doc, message);
  
  String topic = "swatidhome/event/" + fixedSerialNumber + "/turnstile";
  mqttClient.publish(topic.c_str(), message.c_str());
  
  Serial.printf("📡 [TORNO] Evento de timeout publicado - Esperando respuesta\n");
}
```

## 📝 Resumen

El problema actual es que el sistema está tratando el timeout como un **rechazo definitivo**, cuando debería ser una **notificación informativa** que permita al backend responder incluso después del timeout inicial.

La solución recomendada es mantener la solicitud activa por un período extendido, permitiendo respuestas tardías mientras se notifica al backend del retraso.
