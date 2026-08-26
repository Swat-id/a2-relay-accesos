# Procesos de Seguridad - KC868A2

## Descripción
Documentación detallada de los procesos de seguridad implementados en el sistema KC868A2, incluyendo validación de códigos, gestión de bloqueos, monitoreo de intentos y procedimientos de emergencia.

## Sistema de Validación de Códigos

### 🔍 Proceso de Validación

#### Flujo de Validación Local Primero
```cpp
void validateCode(const String& code, const String& type, int keyboardId) {
    // 1. Verificar bloqueo de acceso local
    if (localAccessBlocked) {
        Serial.printf("🔒 [%s] Acceso BLOQUEADO - Código rechazado\n", keyboardName.c_str());
        publishFailedAccess(code, type, keyboardId, "BLOCKED");
        return;
    }
    
    // 2. Búsqueda local en EEPROM
    int relayToActivate = 1;
    bool localFound = isCodeStored(type.c_str(), code.c_str(), &relayToActivate);
    
    // 3. Si encontrado localmente y modo local primero
    if (storedCodes.localValidationFirst && localFound) {
        // Acceso local exitoso
        Serial.printf("✅ [%s] Código válido LOCAL - Relé %d\n", keyboardName.c_str(), relayToActivate);
        controlReleWithDuration(releDuration, relayToActivate);
        resetFailedAttempts();
        publishAccessEvent(code, type, keyboardId, true, "LOCAL");
        return;
    }
    
    // 4. Si no encontrado localmente o modo remoto primero
    if (!storedCodes.localValidationFirst || !localFound) {
        if (mqttClient.connected()) {
            // Enviar para validación remota
            sendRemoteValidation(code, type, keyboardId);
        } else if (!storedCodes.localValidationFirst && localFound) {
            // Fallback local cuando MQTT no está conectado
            controlReleWithDuration(releDuration, relayToActivate);
            resetFailedAttempts();
            publishAccessEvent(code, type, keyboardId, true, "LOCAL_FALLBACK");
        } else {
            // Código inválido - incrementar intentos fallidos
            handleInvalidCode(code, type, keyboardId);
        }
    }
}
```

#### Flujo de Validación Remoto Primero
1. **Envío inmediato a MQTT**: Validación remota
2. **Procesamiento de respuesta**: Remota
3. **Fallback local**: Si MQTT falla y código existe localmente
4. **Manejo de errores**: Si no existe localmente

### 📡 Comunicación de Validación Remota

#### Envío de Solicitud
```cpp
void sendRemoteValidation(const String& code, const String& type, int keyboardId) {
    DynamicJsonDocument doc(1024);
    doc["timestamp"] = getTimestamp();
    doc["message_id"] = messageId++;
    doc["device"] = fixedSerialNumber;
    doc["device_name"] = deviceName;
    doc["message_type"] = 0;
    
    JsonObject msgInfo = doc.createNestedObject("message_info");
    msgInfo["source"] = "AUTO";
    msgInfo["code_type"] = type;
    msgInfo["code_value"] = code;
    msgInfo["keyboard_id"] = keyboardId;
    msgInfo["keyboard_name"] = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
    msgInfo["keyboard_pins"] = (keyboardId == 1) ? 
        String(WIEGAND1_D0) + "/" + String(WIEGAND1_D1) : 
        String(WIEGAND2_D0) + "/" + String(WIEGAND2_D1);
    msgInfo["request_relay"] = 1;
    msgInfo["max_duration"] = releDuration;
    
    String message;
    serializeJson(doc, message);
    
    String topic = "swatidhome/command/" + fixedSerialNumber + "/access";
    
    // Intentar publicar varias veces si es necesario
    bool published = false;
    for (int retry = 0; retry < 3 && !published; retry++) {
        published = mqttClient.publish(topic.c_str(), message.c_str(), false);
        if (!published) {
            delay(100);
            mqttClient.loop();
        }
    }
    
    if (!published) {
        handleMqttPublishFailed(code, type, keyboardId, localFound);
    }
}
```

#### Procesamiento de Respuesta
```cpp
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    
    if (topicStr.endsWith("/granted")) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload, length);
        
        String type = doc["code_type"].as<String>();
        String value = doc["code_value"].as<String>();
        bool granted = doc["access_granted"].as<bool>();
        int duration = doc.containsKey("duration") ? doc["duration"].as<int>() : (int)(releDuration * 1000);
        int relay = doc.containsKey("relay_number") ? doc["relay_number"].as<int>() : 1;
        String reason = doc.containsKey("reason") ? doc["reason"].as<String>() : "";
        
        if (granted) {
            // Acceso concedido
            controlReleWithDuration(duration / 1000.0, relay);
            resetFailedAttempts();
            publishAccessEvent(value, type, lastKeyboardId, true, "REMOTE");
        } else {
            // Acceso denegado
            failedAttempts++;
            lastFailedAttempt = millis();
            publishFailedAccess(value, type, lastKeyboardId, "REMOTE_DENIED: " + reason);
            
            // Verificar si debe activarse el bloqueo
            if (failedAttempts >= maxFailedAttempts) {
                activateSecurityBlock();
            }
        }
    }
}
```

## Sistema de Bloqueo de Seguridad

### 🔒 Activación de Bloqueo

#### Condiciones de Activación
- **Intentos fallidos**: Alcanzar el máximo configurado
- **Comando remoto**: Bloqueo manual vía MQTT
- **Errores críticos**: Fallos del sistema

#### Proceso de Activación
```cpp
void activateSecurityBlock() {
    localAccessBlocked = true;
    blockStartTime = millis();
    
    Serial.printf("🔒 BLOQUEO ACTIVADO tras %d intentos fallidos por %lu segundos\n", 
                  maxFailedAttempts, blockDuration/1000);
    
    // Guardar estado en EEPROM
    saveConfiguration();
    
    // Notificar vía MQTT
    publishError(6, "Acceso local bloqueado tras " + String(maxFailedAttempts) + " intentos fallidos");
    
    // Logging detallado
    Serial.println("🔒 === BLOQUEO DE SEGURIDAD ACTIVADO ===");
    Serial.printf("   Intentos fallidos: %d/%d\n", failedAttempts, maxFailedAttempts);
    Serial.printf("   Duración: %lu segundos\n", blockDuration/1000);
    Serial.printf("   Tiempo de inicio: %lu ms\n", blockStartTime);
    Serial.println("🔒 === FIN BLOQUEO DE SEGURIDAD ===");
}
```

### 🔓 Desbloqueo de Seguridad

#### Desbloqueo Automático
```cpp
void checkAccessBlock() {
    // Verificar si el bloqueo temporal debe levantarse
    if (localAccessBlocked && blockStartTime > 0) {
        if (millis() - blockStartTime >= blockDuration) {
            localAccessBlocked = false;
            blockStartTime = 0;
            
            Serial.println("🔓 Bloqueo temporal levantado automáticamente");
            
            // Notificar vía MQTT
            publishError(5, "Bloqueo temporal levantado automáticamente tras " + String(blockDuration/1000) + " segundos");
            
            // Guardar estado
            saveConfiguration();
        }
    }
    
    // Resetear contador de intentos fallidos tras timeout
    if (failedAttempts > 0 && (millis() - lastFailedAttempt > failedAttemptTimeout)) {
        resetFailedAttempts();
    }
}
```

#### Desbloqueo Remoto
```cpp
void processCommand(const JsonDocument& doc) {
    if (doc["message_type"].as<int>() == 3) {
        String securityCommand = doc["message_info"]["security_command"].as<String>();
        
        if (securityCommand == "unblock_local_access") {
            localAccessBlocked = false;
            failedAttempts = 0;
            saveConfiguration();
            
            Serial.println("🔓 Acceso local DESBLOQUEADO remotamente");
            publishResponse(0, receivedMessageId, "local access unblocked");
        }
    }
}
```

## Gestión de Intentos Fallidos

### 📊 Monitoreo de Intentos

#### Contador de Intentos
```cpp
void handleInvalidCode(const String& code, const String& type, int keyboardId) {
    // Incrementar contador de intentos fallidos
    failedAttempts++;
    lastFailedAttempt = millis();
    
    Serial.printf("❌ [%s] Código no válido - Intento %d/%d\n", 
                  keyboardName.c_str(), failedAttempts, maxFailedAttempts);
    
    // Publicar intento fallido
    publishFailedAccess(code, type, keyboardId, "INVALID_CODE");
    
    // Verificar si debe activarse el bloqueo
    if (failedAttempts >= maxFailedAttempts) {
        activateSecurityBlock();
    }
}
```

#### Reset de Intentos
```cpp
void resetFailedAttempts() {
    failedAttempts = 0;
    Serial.println("🔄 Contador de intentos fallidos reseteado");
}

void checkAccessBlock() {
    // Resetear contador de intentos fallidos tras timeout
    if (failedAttempts > 0 && (millis() - lastFailedAttempt > failedAttemptTimeout)) {
        resetFailedAttempts();
    }
}
```

### ⏱️ Timeouts y Temporizadores

#### Timeout de Intentos
- **Duración**: 5 minutos (300,000 ms)
- **Condición**: Sin actividad de teclado
- **Acción**: Reset automático del contador
- **Logging**: Registro de reset

#### Timeout de Bloqueo
- **Duración**: Configurable (30-3600 segundos)
- **Condición**: Bloqueo activo
- **Acción**: Desbloqueo automático
- **Notificación**: MQTT y logs

## Configuración de Seguridad

### ⚙️ Parámetros Configurables

#### Máximo de Intentos Fallidos
```cpp
void processCommand(const JsonDocument& doc) {
    if (doc["message_info"]["security_command"].as<String>() == "set_max_failed_attempts") {
        if (doc["message_info"].containsKey("max_attempts")) {
            maxFailedAttempts = doc["message_info"]["max_attempts"].as<int>();
            saveConfiguration();
            
            Serial.printf("🚫 Máximo intentos fallidos actualizado: %d\n", maxFailedAttempts);
            publishResponse(0, receivedMessageId, "max failed attempts updated to " + String(maxFailedAttempts));
        }
    }
}
```

#### Duración de Bloqueo
```cpp
void processCommand(const JsonDocument& doc) {
    if (doc["message_info"]["security_command"].as<String>() == "set_block_duration") {
        if (doc["message_info"].containsKey("duration_seconds")) {
            blockDuration = doc["message_info"]["duration_seconds"].as<unsigned long>() * 1000;
            saveConfiguration();
            
            Serial.printf("⏰ Duración de bloqueo actualizada: %lu segundos\n", blockDuration/1000);
            publishResponse(0, receivedMessageId, "block duration updated to " + String(blockDuration/1000) + " seconds");
        }
    }
}
```

### 🔧 Configuración por Defecto
- **Máximo intentos**: 3
- **Duración bloqueo**: 60 segundos
- **Timeout intentos**: 5 minutos
- **Modo validación**: Local primero

## Eventos de Seguridad

### 📤 Publicación de Eventos

#### Evento de Acceso Exitoso
```cpp
void publishAccessEvent(const String& code, const String& type, int keyboardId, bool success, const String& source) {
    DynamicJsonDocument doc(512);
    doc["timestamp"] = getTimestamp();
    doc["message_id"] = messageId++;
    doc["device"] = deviceName;
    doc["serial"] = fixedSerialNumber;
    doc["event_type"] = "ACCESS_EVENT";
    doc["success"] = success;
    doc["source"] = source;
    doc["code_type"] = type;
    doc["code_value"] = code;
    
    if (keyboardId > 0) {
        doc["keyboard_id"] = keyboardId;
        doc["keyboard_name"] = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
    }
    
    String message;
    serializeJson(doc, message);
    String topic = "swatidhome/events/" + fixedSerialNumber + "/access";
    
    mqttClient.publish(topic.c_str(), message.c_str());
}
```

#### Evento de Acceso Fallido
```cpp
void publishFailedAccess(const String& code, const String& type, int keyboardId, const String& reason) {
    DynamicJsonDocument doc(512);
    doc["timestamp"] = getTimestamp();
    doc["message_id"] = messageId++;
    doc["device"] = deviceName;
    doc["serial"] = fixedSerialNumber;
    doc["event_type"] = "FAILED_ACCESS";
    doc["code_type"] = type;
    doc["code_value"] = code;
    doc["reason"] = reason;
    doc["failed_attempts"] = failedAttempts;
    doc["max_attempts"] = maxFailedAttempts;
    
    if (keyboardId > 0) {
        doc["keyboard_id"] = keyboardId;
        doc["keyboard_name"] = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
    }
    
    String message;
    serializeJson(doc, message);
    String topic = "swatidhome/events/" + fixedSerialNumber + "/failed_access";
    
    mqttClient.publish(topic.c_str(), message.c_str());
}
```

### 🚨 Códigos de Error de Seguridad

| Código | Descripción | Acción Requerida |
|--------|-------------|------------------|
| 5 | Sistema de seguridad | Informativo |
| 6 | Bloqueo de acceso activado | Desbloquear si necesario |
| 7 | Error de validación remota | Verificar MQTT |

## Procedimientos de Emergencia

### 🚨 Bloqueo de Emergencia

#### Comando MQTT
```json
{
  "message_id": 123,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "block_local_access"
  }
}
```

#### Comando Web
- **URL**: `/security/block`
- **Método**: POST
- **Autenticación**: Requerida
- **Efecto**: Inmediato

### 🔓 Desbloqueo de Emergencia

#### Comando MQTT
```json
{
  "message_id": 124,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "unblock_local_access"
  }
}
```

#### Comando Web
- **URL**: `/security/unblock`
- **Método**: POST
- **Autenticación**: Requerida
- **Efecto**: Inmediato

### 🔄 Reset de Seguridad

#### Reset Completo
- **Comando**: `reset` vía MQTT o web
- **Efecto**: Restauración a valores por defecto
- **Parámetros restaurados**:
  - Máximo intentos: 3
  - Duración bloqueo: 60 segundos
  - Acceso local: Desbloqueado
  - Intentos fallidos: 0

## Monitoreo de Seguridad

### 📊 Estado de Seguridad en Tiempo Real

#### Información Disponible
- **Estado de bloqueo**: Activo/Inactivo
- **Intentos fallidos**: Contador actual
- **Tiempo restante**: Si está bloqueado
- **Último intento**: Timestamp
- **Configuración**: Parámetros actuales

#### Actualización
- **Frecuencia**: Tiempo real
- **MQTT**: Eventos inmediatos
- **Web**: Actualización automática
- **Logs**: Registro continuo

### 🔍 Auditoría de Seguridad

#### Eventos Registrados
- **Accesos exitosos**: Todos los accesos
- **Intentos fallidos**: Con razón y contador
- **Bloqueos**: Activación y desbloqueo
- **Configuración**: Cambios de parámetros
- **Errores**: Fallos del sistema

#### Información de Auditoría
- **Timestamp**: ISO 8601 con timezone
- **Origen**: Teclado, web, MQTT
- **Usuario**: Identificación del origen
- **Acción**: Descripción detallada
- **Resultado**: Éxito o fallo
- **Contexto**: Información adicional

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
