# Operaciones del Sistema - KC868A2

## Descripción
Documentación completa de las operaciones del sistema KC868A2, incluyendo procesos de inicialización, funcionamiento normal, manejo de errores y procedimientos de mantenimiento.

## Proceso de Inicialización

### 🚀 Secuencia de Arranque

#### 1. Inicialización del Hardware
```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Inicializar RS-485
    RS485_Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RX2, RS485_TX2);
    
    // Configurar pines de relés
    pinMode(RELE1_PIN, OUTPUT);
    pinMode(RELE2_PIN, OUTPUT);
    digitalWrite(RELE1_PIN, LOW);
    digitalWrite(RELE2_PIN, LOW);
    
    // Configurar teclados Wiegand
    pinMode(WIEGAND1_D0, INPUT_PULLUP);
    pinMode(WIEGAND1_D1, INPUT_PULLUP);
    pinMode(WIEGAND2_D0, INPUT_PULLUP);
    pinMode(WIEGAND2_D1, INPUT_PULLUP);
    
    // Configurar interrupciones
    attachInterrupt(digitalPinToInterrupt(WIEGAND1_D0), handleWiegand1D0, FALLING);
    attachInterrupt(digitalPinToInterrupt(WIEGAND1_D1), handleWiegand1D1, FALLING);
    attachInterrupt(digitalPinToInterrupt(WIEGAND2_D0), handleWiegand2D0, FALLING);
    attachInterrupt(digitalPinToInterrupt(WIEGAND2_D1), handleWiegand2D1, FALLING);
}
```

#### 2. Carga de Configuración
- **EEPROM**: Inicialización de 4KB
- **Configuración**: Carga desde EEPROM
- **Códigos**: Carga de códigos almacenados
- **Validación**: Verificación de integridad

#### 3. Configuración de Red
- **Ethernet**: Inicialización del PHY LAN8720
- **DHCP**: Solicitud de IP automática
- **IP Estática**: Configuración manual si está configurada
- **Hostname**: Configuración basada en nombre del dispositivo

#### 4. Conexión MQTT
- **Broker**: Conexión a 188.245.213.181:1883
- **Autenticación**: Usuario/contraseña
- **Suscripciones**: Tópicos de comando
- **Keepalive**: Configuración de 60 segundos

#### 5. Servidor Web
- **Puerto**: 80
- **Rutas**: Configuración de endpoints
- **Autenticación**: HTTP Basic Auth
- **mDNS**: Configuración opcional

### ⏱️ Tiempos de Inicialización
- **Hardware**: ~2 segundos
- **Red**: ~5-30 segundos (depende de DHCP)
- **MQTT**: ~1-5 segundos
- **Total**: ~10-40 segundos

## Proceso Principal (Loop)

### 🔄 Ciclo Principal de Operación

#### 1. Gestión de Conexión MQTT
```cpp
void loop() {
    // Reconexión MQTT si es necesario
    if (!mqttClient.connected() && ethConnected) {
        static unsigned long lastReconnectAttempt = 0;
        if (currentTime - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = currentTime;
            connectToMqtt();
        }
    }
    
    // Procesar cola MQTT
    if (mqttClient.connected()) {
        mqttClient.loop();
    }
}
```

#### 2. Procesamiento de Teclados
- **Wiegand 1**: Procesamiento de datos del teclado principal
- **Wiegand 2**: Procesamiento de datos del teclado secundario
- **RS485**: Procesamiento de teclado legacy
- **Timeout**: Limpieza de buffers tras 5 segundos

#### 3. Sistema de Seguridad
- **Verificación de bloqueo**: Comprobación de estado
- **Timeout de bloqueo**: Desbloqueo automático
- **Reset de intentos**: Limpieza tras 5 minutos

#### 4. Control de Relés
- **Timeout automático**: Desactivación por tiempo
- **Estado**: Monitoreo de activación
- **MQTT**: Publicación de cambios de estado

#### 5. Servidor Web
- **Clientes**: Manejo de conexiones HTTP
- **Requests**: Procesamiento de peticiones
- **Responses**: Envío de respuestas

#### 6. Monitoreo del Sistema
- **Estado**: Cada 30 segundos
- **Keepalive**: Cada minuto
- **Memoria**: Verificación de uso
- **Conectividad**: Estado de red y MQTT

### ⚡ Frecuencias de Operación
- **Loop principal**: ~1ms
- **Procesamiento Wiegand**: Inmediato (interrupciones)
- **MQTT**: Continuo
- **Web**: Continuo
- **Monitoreo**: 30 segundos
- **Keepalive**: 60 segundos

## Procesamiento de Teclados

### 🔐 Sistema Dual Wiegand

#### Teclado 1 (Principal)
- **Pines**: GPIO 33/14
- **Interrupciones**: FALLING edge
- **Procesamiento**: Inmediato
- **Formato**: 4, 8, 26, 34 bits

#### Teclado 2 (Secundario)
- **Pines**: GPIO 4/16
- **Interrupciones**: FALLING edge
- **Procesamiento**: Inmediato
- **Formato**: 4, 8, 26, 34 bits

#### Procesamiento de Datos
```cpp
void processWiegand1Data() {
    unsigned long currentMillis = millis();
    
    if (wiegand1Bits > 0 && (currentMillis - wiegand1BitTime > 25) && !wiegand1Complete) {
        wiegand1Complete = true;
        
        if (wiegand1Bits == 4) {
            // Tecla individual
            uint8_t key = wiegand1Data & 0x0F;
            processKey(key, 1);
        } else if (wiegand1Bits == 26 || wiegand1Bits == 34) {
            // Tarjeta RFID/NFC
            String tagCode = String(wiegand1Data & 0x00FFFFFF);
            validateCode(tagCode, "TAG", 1);
        }
        
        // Reiniciar para siguiente lectura
        wiegand1Data = 0;
        wiegand1Bits = 0;
        wiegand1Complete = false;
    }
}
```

### ⌨️ Procesamiento de Teclas

#### Teclas Numéricas (0-9)
- **Acción**: Añadir al PIN actual
- **Validación**: Longitud máxima 6 dígitos
- **Timeout**: 5 segundos sin actividad

#### Tecla Asterisco (*)
- **Acción**: Borrar PIN actual
- **Efecto**: Limpiar buffer
- **Logging**: Registro de acción

#### Tecla Hash (#)
- **Acción**: Confirmar PIN
- **Validación**: Longitud 4-6 dígitos
- **Procesamiento**: Envío a validación

### 🏷️ Procesamiento de Tags
- **Formato**: 26 o 34 bits
- **Procesamiento**: Inmediato
- **Validación**: Envío directo a validación
- **Logging**: Registro completo

## Sistema de Validación

### 🔍 Flujo de Validación

#### Modo Local Primero
1. **Búsqueda local**: Verificar en EEPROM
2. **Si encontrado**: Activar relé localmente
3. **Si no encontrado**: Enviar a MQTT
4. **Procesar respuesta**: Remota o fallback local

#### Modo Remoto Primero
1. **Envío a MQTT**: Validación remota
2. **Procesar respuesta**: Remota
3. **Si falla**: Búsqueda local
4. **Fallback**: Activación local si existe

### 📡 Comunicación MQTT
```cpp
void validateCode(const String& code, const String& type, int keyboardId) {
    // Verificar bloqueo
    if (localAccessBlocked) {
        publishFailedAccess(code, type, keyboardId, "BLOCKED");
        return;
    }
    
    // Búsqueda local
    int relayToActivate = 1;
    bool localFound = isCodeStored(type.c_str(), code.c_str(), &relayToActivate);
    
    if (storedCodes.localValidationFirst && localFound) {
        // Acceso local exitoso
        controlReleWithDuration(releDuration, relayToActivate);
        resetFailedAttempts();
        publishAccessEvent(code, type, keyboardId, true, "LOCAL");
        return;
    }
    
    // Validación remota
    if (mqttClient.connected()) {
        // Enviar a MQTT
        DynamicJsonDocument doc(1024);
        doc["timestamp"] = getTimestamp();
        doc["message_id"] = messageId++;
        doc["device"] = fixedSerialNumber;
        doc["message_type"] = 0;
        
        JsonObject msgInfo = doc.createNestedObject("message_info");
        msgInfo["source"] = "AUTO";
        msgInfo["code_type"] = type;
        msgInfo["code_value"] = code;
        msgInfo["keyboard_id"] = keyboardId;
        
        String message;
        serializeJson(doc, message);
        
        String topic = "swatidhome/command/" + fixedSerialNumber + "/access";
        mqttClient.publish(topic.c_str(), message.c_str());
    }
}
```

## Sistema de Seguridad

### 🔒 Gestión de Bloqueos

#### Activación de Bloqueo
- **Condición**: Intentos fallidos >= máximo
- **Duración**: Configurable (30-3600 segundos)
- **Efecto**: Bloqueo de acceso local
- **Notificación**: MQTT y logs

#### Desbloqueo Automático
- **Condición**: Tiempo transcurrido >= duración
- **Efecto**: Restauración de acceso
- **Notificación**: MQTT y logs
- **Reset**: Contador de intentos

#### Desbloqueo Remoto
- **Comando**: MQTT
- **Efecto**: Inmediato
- **Persistencia**: Guardado en EEPROM
- **Notificación**: Confirmación

### 📊 Monitoreo de Intentos
- **Contador**: Intentos fallidos consecutivos
- **Reset**: Tras 5 minutos sin actividad
- **Límite**: Configurable (1-10 intentos)
- **Logging**: Todos los intentos

## Control de Relés

### ⚡ Activación de Relés

#### Activación Local
- **Duración**: Configurable por código
- **Relé**: Especificado por código
- **Temporización**: Automática
- **Estado**: Monitoreado

#### Activación Remota
- **Comando**: MQTT
- **Duración**: Especificada en comando
- **Relé**: Especificado en comando
- **Confirmación**: Respuesta MQTT

#### Activación Web
- **Interfaz**: Página web
- **Duración**: Configuración por defecto
- **Relé**: Seleccionado por usuario
- **Logging**: Registro de acción

### ⏱️ Temporización
```cpp
void controlReleWithDuration(float duration, int relay) {
    if (relay < 1 || relay > 2) return;
    
    int relayPin = (relay == 1) ? RELE1_PIN : RELE2_PIN;
    
    digitalWrite(relayPin, HIGH);
    releStartTime[relay] = millis();
    releDurations[relay] = duration * 1000;
    releActive[relay] = true;
    
    // Publicar estado
    publishRelayStatus(relay, "ON", "SYSTEM");
}

void checkRelayTimeout() {
    for (int relay = 1; relay <= 2; relay++) {
        if (releActive[relay] && millis() - releStartTime[relay] >= releDurations[relay]) {
            int relayPin = (relay == 1) ? RELE1_PIN : RELE2_PIN;
            digitalWrite(relayPin, LOW);
            releActive[relay] = false;
            
            publishRelayStatus(relay, "OFF", "TIMEOUT");
        }
    }
}
```

## Gestión de Memoria

### 💾 Almacenamiento EEPROM

#### Configuración del Sistema
- **Offset**: 0
- **Tamaño**: 512 bytes
- **Contenido**: Configuración general
- **Validación**: Marcador 0xABCD1234

#### Códigos de Acceso
- **Offset**: 512
- **Tamaño**: 3584 bytes
- **Capacidad**: 500 códigos
- **Validación**: Marcador 0xCAFEBABE

### 🔄 Gestión de Memoria RAM
- **Heap libre**: Monitoreado
- **Umbral**: 10KB
- **Alerta**: MQTT y logs
- **Acción**: Reinicio si es crítico

## Manejo de Errores

### ❌ Tipos de Errores

#### Errores de Red
- **Ethernet**: Desconexión
- **MQTT**: Pérdida de conexión
- **DNS**: Resolución fallida
- **Acción**: Reconexión automática

#### Errores de Hardware
- **Teclados**: Sin respuesta
- **Relés**: Fallo de activación
- **EEPROM**: Corrupción
- **Acción**: Logging y notificación

#### Errores de Software
- **JSON**: Parsing fallido
- **Memoria**: Insuficiente
- **Configuración**: Inválida
- **Acción**: Recuperación automática

### 🔧 Procedimientos de Recuperación

#### Reconexión Automática
- **MQTT**: Cada 5 segundos
- **Ethernet**: Reinicio del PHY
- **Web**: Reinicio del servidor
- **Logging**: Estado de recuperación

#### Reset de Emergencia
- **Condición**: Errores críticos
- **Acción**: Reinicio completo
- **Configuración**: Mantenida
- **Logging**: Evento registrado

## Monitoreo y Logging

### 📊 Monitoreo del Sistema
```cpp
void printSystemStatus(unsigned long currentTime) {
    Serial.println("\n=============== SISTEMA DUAL WIEGAND SEGURO ===============");
    Serial.printf("🕐 Uptime: %lu ms (%.1f horas)\n", currentTime, (float)currentTime/3600000.0);
    Serial.printf("🆔 Serial: %s | Dispositivo: %s\n", fixedSerialNumber.c_str(), deviceName.c_str());
    
    // Estado de conectividad
    Serial.printf("🌐 Conectividad:\n");
    Serial.printf("   Ethernet: %s", ethConnected ? "✅ CONECTADO" : "❌ DESCONECTADO");
    if (ethConnected) {
        Serial.printf(" (%s)", ip.toString().c_str());
    }
    Serial.println();
    
    Serial.printf("   MQTT: %s", mqttClient.connected() ? "✅ CONECTADO" : "❌ DESCONECTADO");
    if (mqttClient.connected()) {
        Serial.printf(" (Buffer: %d bytes)", mqttClient.getBufferSize());
    }
    Serial.println();
    
    // Estado de seguridad
    Serial.printf("🔒 Seguridad:\n");
    Serial.printf("   Acceso local: %s\n", localAccessBlocked ? "🔒 BLOQUEADO" : "🔓 PERMITIDO");
    Serial.printf("   Intentos fallidos: %d/%d\n", failedAttempts, maxFailedAttempts);
    
    // Estado de teclados
    Serial.printf("🔐 Teclados Wiegand:\n");
    Serial.printf("   📍 TECLADO 1 (GPIO%d/%d): PIN='%s' (%d chars)\n", 
                  WIEGAND1_D0, WIEGAND1_D1, currentPin1.c_str(), currentPin1.length());
    Serial.printf("   📍 TECLADO 2 (GPIO%d/%d): PIN='%s' (%d chars)\n", 
                  WIEGAND2_D0, WIEGAND2_D1, currentPin2.c_str(), currentPin2.length());
    
    // Estado de relés
    Serial.printf("⚡ Relés:\n");
    for (int i = 1; i <= 2; i++) {
        Serial.printf("   R%d: %s", i, releActive[i] ? "🟢 ON" : "🔴 OFF");
        if (releActive[i]) {
            unsigned long elapsed = millis() - releStartTime[i];
            unsigned long remaining = (releDurations[i] > elapsed) ? 
                (unsigned long)(releDurations[i] - elapsed) : 0UL;
            Serial.printf(" (quedan %lu ms)", remaining);
        }
        Serial.println();
    }
    
    // Almacenamiento
    Serial.printf("💾 Almacenamiento:\n");
    Serial.printf("   Códigos: %d/%d (%.1f%% usado)\n", 
                  storedCodes.count, MAX_CODES, 
                  (float)storedCodes.count / MAX_CODES * 100);
    Serial.printf("   Modo validación: %s\n",
                  storedCodes.localValidationFirst ? "🏠 Local primero" : "🌐 Remoto primero");
    
    // Información de sistema
    Serial.printf("💻 Sistema:\n");
    Serial.printf("   Memoria libre: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("   Memoria mínima: %d bytes\n", ESP.getMinFreeHeap());
    Serial.printf("   CPU: %.1f MHz\n", (float)ESP.getCpuFreqMHz());
    Serial.printf("   Temperatura: %.1f°C\n", temperatureRead());
    
    Serial.println("===========================================================\n");
}
```

### 📝 Logging de Eventos
- **Accesos**: Todos los intentos
- **Errores**: Códigos y descripciones
- **Configuración**: Cambios realizados
- **Sistema**: Estado y operaciones

### 🔔 Notificaciones MQTT
- **Eventos**: Tiempo real
- **Errores**: Inmediatos
- **Estado**: Periódicos
- **Keepalive**: Cada minuto

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
