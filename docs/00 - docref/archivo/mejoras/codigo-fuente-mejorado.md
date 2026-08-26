# Código Fuente Mejorado - Sistema de Códigos por Teclado

## Descripción

Este documento contiene el código fuente completo mejorado para implementar el sistema de códigos por teclado en el KC868A2.

## Estructuras de Datos Mejoradas

### 🔧 Estructura de Código
```cpp
// Estructura de código mejorada
struct CodeEntry {
  char type[5];        // "PIN" o "TAG"
  char value[17];      // Valor del código (máximo 16 caracteres + null)
  uint8_t keyboard_id; // ID del teclado (0=ambos, 1=teclado1, 2=teclado2)
  uint8_t relay;       // Relé a activar (1 o 2)
  uint8_t reserved;    // Reservado para futuras extensiones
};
```

### 📊 Estructura de Almacenamiento
```cpp
// Estructura de almacenamiento con versión
struct StoredCodes {
  uint32_t validMarker;           // 0xCAFEBABE
  uint32_t version;               // 1 = formato antiguo, 2 = formato nuevo
  bool localValidationFirst;
  uint16_t count;                 // Número de códigos almacenados
  CodeEntry codes[MAX_CODES];     // Array de códigos
};
```

## Funciones de Migración

### 🔄 Migración de Códigos
```cpp
void migrateCodesToNewFormat() {
  if (storedCodes.version == 1) {
    Serial.println("🔄 Migrando códigos al formato nuevo...");
    
    // Migrar códigos existentes (keyboard_id = 0 = ambos teclados)
    for (int i = 0; i < storedCodes.count; i++) {
      storedCodes.codes[i].keyboard_id = 0; // Ambos teclados
      storedCodes.codes[i].reserved = 0;
    }
    
    storedCodes.version = 2;
    saveStoredCodes();
    Serial.printf("✅ Migrados %d códigos al formato nuevo\n", storedCodes.count);
  }
}
```

### 💾 Backup y Restauración
```cpp
#define EEPROM_BACKUP_OFFSET 1024

void backupCodes() {
  Serial.println("💾 Creando backup de códigos...");
  
  // Crear backup en EEPROM adicional
  EEPROM.put(EEPROM_BACKUP_OFFSET, storedCodes);
  EEPROM.commit();
  
  Serial.println("✅ Backup creado");
}

void restoreCodes() {
  Serial.println("🔄 Restaurando códigos desde backup...");
  
  EEPROM.get(EEPROM_BACKUP_OFFSET, storedCodes);
  
  Serial.println("✅ Códigos restaurados");
}
```

## Funciones de Gestión de Códigos

### 🔍 Búsqueda Mejorada
```cpp
bool isCodeStored(const char* type, const char* value, int keyboardId, int* relay) {
  for (int i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0) {
      
      if (storedCodes.version == 1) {
        // Formato antiguo: válido en cualquier teclado
        if (relay != nullptr) *relay = storedCodes.codes[i].relay;
        return true;
      } else {
        // Formato nuevo: verificar teclado
        if (storedCodes.codes[i].keyboard_id == 0 || 
            storedCodes.codes[i].keyboard_id == keyboardId) {
          if (relay != nullptr) *relay = storedCodes.codes[i].relay;
          return true;
        }
      }
    }
  }
  return false;
}

// Sobrecarga para compatibilidad
bool isCodeStored(const char* type, const char* value, int* relay) {
  return isCodeStored(type, value, 0, relay); // Teclado 0 = ambos
}
```

### ➕ Añadir Código
```cpp
bool addCode(const char* type, const char* value, int keyboardId, int relay) {
  if (storedCodes.count >= MAX_CODES) {
    Serial.println("❌ Máximo de códigos alcanzado");
    return false;
  }
  
  // Validar parámetros
  if (keyboardId < 0 || keyboardId > 2) {
    Serial.println("❌ ID de teclado inválido (0-2)");
    return false;
  }
  
  if (relay < 1 || relay > 2) {
    Serial.println("❌ Número de relé inválido (1-2)");
    return false;
  }
  
  // Verificar duplicados exactos
  for (int i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0 &&
        storedCodes.codes[i].keyboard_id == keyboardId) {
      Serial.println("❌ Código duplicado exacto");
      return false;
    }
  }
  
  // Añadir nuevo código
  strncpy(storedCodes.codes[storedCodes.count].type, type, 4);
  storedCodes.codes[storedCodes.count].type[4] = '\0';
  
  strncpy(storedCodes.codes[storedCodes.count].value, value, 16);
  storedCodes.codes[storedCodes.count].value[16] = '\0';
  
  storedCodes.codes[storedCodes.count].keyboard_id = keyboardId;
  storedCodes.codes[storedCodes.count].relay = relay;
  storedCodes.codes[storedCodes.count].reserved = 0;
  
  storedCodes.count++;
  storedCodes.version = 2; // Asegurar versión nueva
  
  saveStoredCodes();
  
  String keyboardName = "Ambos";
  if (keyboardId == 1) keyboardName = "Teclado 1";
  else if (keyboardId == 2) keyboardName = "Teclado 2";
  
  Serial.printf("✅ Código añadido: %s %s -> %s -> Relé %d\n", 
                type, value, keyboardName.c_str(), relay);
  return true;
}

// Sobrecarga para compatibilidad (keyboardId = 0)
bool addCode(const char* type, const char* value, int relay) {
  return addCode(type, value, 0, relay);
}
```

### 🗑️ Eliminar Código
```cpp
bool deleteCode(const char* type, const char* value, int keyboardId) {
  for (uint16_t i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0 &&
        storedCodes.codes[i].keyboard_id == keyboardId) {
      
      // Mover último elemento a la posición eliminada
      storedCodes.codes[i] = storedCodes.codes[storedCodes.count - 1];
      storedCodes.count--;
      saveStoredCodes();
      
      String keyboardName = "Ambos";
      if (keyboardId == 1) keyboardName = "Teclado 1";
      else if (keyboardId == 2) keyboardName = "Teclado 2";
      
      Serial.printf("✅ Código eliminado: %s %s de %s\n", 
                    type, value, keyboardName.c_str());
      return true;
    }
  }
  
  Serial.println("❌ Código no encontrado");
  return false;
}

// Sobrecarga para compatibilidad
bool deleteCode(const char* type, const char* value) {
  return deleteCode(type, value, 0);
}
```

### 📋 Listar Códigos
```cpp
void listCodes() {
  Serial.println("📋 Códigos almacenados:");
  Serial.printf("Versión: %d, Total: %d/%d\n", storedCodes.version, storedCodes.count, MAX_CODES);
  
  if (storedCodes.count == 0) {
    Serial.println("  No hay códigos almacenados");
    return;
  }
  
  for (int i = 0; i < storedCodes.count; i++) {
    String keyboardName = "Ambos";
    if (storedCodes.codes[i].keyboard_id == 1) keyboardName = "Teclado 1";
    else if (storedCodes.codes[i].keyboard_id == 2) keyboardName = "Teclado 2";
    
    Serial.printf("  %d: %s %s -> %s -> Relé %d\n", 
                  i+1, 
                  storedCodes.codes[i].type, 
                  storedCodes.codes[i].value,
                  keyboardName.c_str(),
                  storedCodes.codes[i].relay);
  }
}
```

## Validación Mejorada

### 🔍 Validación por Teclado
```cpp
void validateCode(const String& code, const String& type, int keyboardId) {
  lastKeyboardId = keyboardId;
  String keyboardName = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
  
  Serial.printf("🔍 [%s] Validando: %s (%s)\n", keyboardName.c_str(), code.c_str(), type.c_str());
  
  // Verificar bloqueo de acceso local
  if (localAccessBlocked) {
    Serial.printf("🔒 [%s] Acceso BLOQUEADO - Código rechazado\n", keyboardName.c_str());
    publishFailedAccess(code, type, keyboardId, "BLOCKED");
    return;
  }
  
  int relayToActivate = 1;
  bool localFound = isCodeStored(type.c_str(), code.c_str(), keyboardId, &relayToActivate);
  
  if (storedCodes.localValidationFirst && localFound) {
    // Acceso local exitoso
    Serial.printf("✅ [%s] Código válido LOCAL - Relé %d\n", keyboardName.c_str(), relayToActivate);
    controlReleWithDuration(releDuration, relayToActivate);
    
    // Resetear intentos fallidos
    resetFailedAttempts();
    
    lastType = type;
    lastCode = code;
    lastTime = getTimeString();
    
    // Publicar evento de acceso local exitoso
    publishAccessEvent(code, type, keyboardId, true, "LOCAL");
    return;
  }
  
  if (!storedCodes.localValidationFirst || !localFound) {
    // Validación remota
    if (mqttClient.connected()) {
      Serial.printf("📡 [%s] Enviando validación remota...\n", keyboardName.c_str());
      sendRemoteValidation(code, type, keyboardId);
    } else if (!storedCodes.localValidationFirst && localFound) {
      // Fallback local cuando MQTT no está conectado
      Serial.printf("🔄 [%s] Fallback LOCAL (MQTT desconectado) - Relé %d\n", keyboardName.c_str(), relayToActivate);
      controlReleWithDuration(releDuration, relayToActivate);
      
      resetFailedAttempts();
      lastType = type;
      lastCode = code;
      lastTime = getTimeString();
      
      // Publicar evento de acceso local fallback exitoso
      publishAccessEvent(code, type, keyboardId, true, "LOCAL_FALLBACK");
      return;
    } else {
      // Código inválido - incrementar intentos fallidos
      failedAttempts++;
      lastFailedAttempt = millis();
      
      Serial.printf("❌ [%s] Código no válido - Intento %d/%d\n", keyboardName.c_str(), failedAttempts, maxFailedAttempts);
      
      // Publicar intento fallido
      publishFailedAccess(code, type, keyboardId, "INVALID_CODE");
      
      // Verificar si debe activarse el bloqueo
      if (failedAttempts >= maxFailedAttempts) {
        localAccessBlocked = true;
        blockStartTime = millis();
        
        Serial.printf("🔒 BLOQUEO ACTIVADO tras %d intentos fallidos por %lu segundos\n", 
                      maxFailedAttempts, blockDuration/1000);
        
        publishError(6, "Acceso local bloqueado tras " + String(maxFailedAttempts) + " intentos fallidos");
      }
    }
  }
}
```

## Interfaz Web Mejorada

### 🌐 Página de Gestión de Códigos
```cpp
void handleCodes() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  String html = R"=====(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset='UTF-8'>
    <title>Gestión de Códigos - Sistema Dual</title>
    <style>
      body { font-family: Arial, sans-serif; margin: 30px; background-color: #f5f5f5; }
      .container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
      h1 { color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }
      h2 { color: #34495e; margin-top: 30px; }
      table { width: 100%; border-collapse: collapse; margin-top: 20px; }
      th, td { border: 1px solid #ddd; padding: 12px; text-align: center; }
      th { background-color: #3498db; color: white; font-weight: bold; }
      tr:nth-child(even) { background-color: #f2f2f2; }
      tr:hover { background-color: #e8f4f8; }
      form { background: #f8f9fa; padding: 25px; border-radius: 8px; margin-bottom: 30px; border: 1px solid #dee2e6; }
      label { display: block; margin-top: 15px; font-weight: bold; color: #495057; }
      input, select { width: 100%; padding: 10px; margin-top: 5px; border: 1px solid #ced4da; border-radius: 4px; font-size: 14px; }
      button { background-color: #28a745; color: white; padding: 12px 20px; border: none; border-radius: 5px; margin-top: 15px; cursor: pointer; font-size: 16px; }
      button:hover { background-color: #218838; }
      .delete-btn { background-color: #dc3545; padding: 6px 12px; font-size: 12px; }
      .delete-btn:hover { background-color: #c82333; }
      .stats { background: #e9ecef; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
      .version { color: #6c757d; font-size: 12px; }
    </style>
  </head>
  <body>
    <div class='container'>
      <h1>🔐 Gestión de Códigos - Sistema Dual</h1>
      
      <div class='stats'>
        <strong>Estadísticas:</strong> 
        Códigos almacenados: )=====";
  
  html += String(storedCodes.count) + "/" + String(MAX_CODES);
  html += " | Versión: " + String(storedCodes.version);
  html += " | Validación: " + String(storedCodes.localValidationFirst ? "Local Primero" : "Remoto Primero");
  
  html += R"=====(
      </div>
      
      <form action='/codes/add' method='post'>
        <h2>➕ Añadir nuevo código</h2>
        
        <label for='type'>Tipo:</label>
        <select name='type' required>
          <option value=''>Seleccionar tipo</option>
          <option value='PIN'>PIN (4-6 dígitos)</option>
          <option value='TAG'>TAG (tarjeta RFID/NFC)</option>
        </select>

        <label for='value'>Código:</label>
        <input type='text' name='value' maxlength='16' required placeholder='Ej: 1234 o ABCD1234'>

        <label for='keyboard_id'>Teclado autorizado:</label>
        <select name='keyboard_id' required>
          <option value=''>Seleccionar teclado</option>
          <option value='0'>Ambos teclados</option>
          <option value='1'>Teclado 1 (GPIO 33/14)</option>
          <option value='2'>Teclado 2 (GPIO 4/16)</option>
        </select>

        <label for='relay'>Relé a activar:</label>
        <select name='relay' required>
          <option value=''>Seleccionar relé</option>
          <option value='1'>Relé 1</option>
          <option value='2'>Relé 2</option>
        </select>

        <button type='submit'>➕ Añadir Código</button>
      </form>

      <h2>📋 Códigos Almacenados</h2>
      <table>
        <tr>
          <th>Tipo</th>
          <th>Valor</th>
          <th>Teclado</th>
          <th>Relé</th>
          <th>Acción</th>
        </tr>
  )=====";

  for (int i = 0; i < storedCodes.count; i++) {
    html += "<tr>";
    html += "<td><strong>" + String(storedCodes.codes[i].type) + "</strong></td>";
    html += "<td><code>" + String(storedCodes.codes[i].value) + "</code></td>";
    
    String keyboardName = "Ambos";
    String keyboardIcon = "🔑";
    if (storedCodes.codes[i].keyboard_id == 1) {
      keyboardName = "Teclado 1";
      keyboardIcon = "🔑";
    } else if (storedCodes.codes[i].keyboard_id == 2) {
      keyboardName = "Teclado 2";
      keyboardIcon = "🔑";
    }
    
    html += "<td>" + keyboardIcon + " " + keyboardName + "</td>";
    html += "<td>⚡ Relé " + String(storedCodes.codes[i].relay) + "</td>";
    html += "<td><a href='/codes/delete?type=" + String(storedCodes.codes[i].type);
    html += "&value=" + String(storedCodes.codes[i].value);
    html += "&keyboard=" + String(storedCodes.codes[i].keyboard_id) + "'";
    html += " class='delete-btn' onclick='return confirm(\"¿Eliminar este código?\")'>🗑️ Eliminar</a></td>";
    html += "</tr>";
  }

  if (storedCodes.count == 0) {
    html += "<tr><td colspan='5' style='text-align: center; color: #6c757d; padding: 20px;'>No hay códigos almacenados</td></tr>";
  }

  html += R"=====(
      </table>
      
      <div style='margin-top: 30px; text-align: center;'>
        <a href='/'><button style='background-color: #6c757d;'>🏠 Volver al inicio</button></a>
        <a href='/codes/mode'><button style='background-color: #17a2b8;'>⚙️ Cambiar modo validación</button></a>
      </div>
      
      <div class='version'>
        <p>Sistema KC868A2 - Firmware v1.8.0 | Formato de códigos v" + String(storedCodes.version) + "</p>
      </div>
    </div>
  </body></html>
  )=====";

  server.send(200, "text/html", html);
}
```

### ➕ Añadir Código
```cpp
void handleCodesAdd() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  if (server.hasArg("type") && server.hasArg("value") && 
      server.hasArg("keyboard_id") && server.hasArg("relay")) {
    
    String type = server.arg("type");
    String value = server.arg("value");
    int keyboardId = server.arg("keyboard_id").toInt();
    int relay = server.arg("relay").toInt();

    value.trim();
    type.trim();

    // Validaciones
    bool isValid = false;
    if (type == "PIN") {
      isValid = value.length() >= 4 && value.length() <= 6;
      for (int i = 0; i < value.length(); i++) {
        if (!isDigit(value.charAt(i))) {
          isValid = false;
          break;
        }
      }
    } else if (type == "TAG") {
      isValid = value.length() > 0 && value.length() <= 16;
    }

    if (isValid && relay >= 1 && relay <= 2 && keyboardId >= 0 && keyboardId <= 2) {
      bool added = addCode(type.c_str(), value.c_str(), keyboardId, relay);
      if (added) {
        Serial.printf("✅ Código añadido desde web: %s %s -> Teclado %d -> Relé %d\n", 
                      type.c_str(), value.c_str(), keyboardId, relay);
        server.sendHeader("Location", "/codes");
        server.send(303);
        return;
      } else {
        server.send(200, "text/html",
          "<html><body><h1>❌ Error al añadir código</h1>"
          "<p>El código ya existe o se alcanzó el máximo de códigos</p>"
          "<a href='/codes'>Volver</a></body></html>");
        return;
      }
    }
    
    server.send(200, "text/html",
      "<html><body><h1>❌ Error de validación</h1>"
      "<p>Verifique el formato del código y los parámetros</p>"
      "<a href='/codes'>Volver</a></body></html>");
  } else {
    server.send(200, "text/html",
      "<html><body><h1>❌ Parámetros faltantes</h1>"
      "<p>Todos los campos son obligatorios</p>"
      "<a href='/codes'>Volver</a></body></html>");
  }
}
```

### 🗑️ Eliminar Código
```cpp
void handleCodesDelete() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  if (server.hasArg("type") && server.hasArg("value") && server.hasArg("keyboard")) {
    String type = server.arg("type");
    String value = server.arg("value");
    int keyboardId = server.arg("keyboard").toInt();

    bool deleted = deleteCode(type.c_str(), value.c_str(), keyboardId);
    if (deleted) {
      Serial.printf("✅ Código eliminado desde web: %s %s del teclado %d\n", 
                    type.c_str(), value.c_str(), keyboardId);
    }
  }
  
  server.sendHeader("Location", "/codes");
  server.send(303);
}
```

## API MQTT Mejorada

### 📡 Información del Dispositivo
```cpp
void publishDeviceInfo(unsigned long originalMessageId) {
  if (!mqttClient.connected()) return;

  DynamicJsonDocument infoDoc(2048);
  infoDoc["device"] = deviceName;
  infoDoc["serial"] = fixedSerialNumber;
  infoDoc["firmware_version"] = firmwareVersion;
  infoDoc["codes_version"] = storedCodes.version;
  infoDoc["local_validation_first"] = storedCodes.localValidationFirst;
  infoDoc["total_codes"] = storedCodes.count;
  infoDoc["max_codes"] = MAX_CODES;

  JsonArray codesArray = infoDoc.createNestedArray("stored_codes");
  for (int i = 0; i < storedCodes.count; i++) {
    JsonObject code = codesArray.createNestedObject();
    code["type"] = storedCodes.codes[i].type;
    code["value"] = storedCodes.codes[i].value;
    code["keyboard_id"] = storedCodes.codes[i].keyboard_id;
    code["relay"] = storedCodes.codes[i].relay;
  }

  String message;
  serializeJson(infoDoc, message);
  String topic = "swatidhome/" + fixedSerialNumber + "/info";
  
  mqttClient.publish(topic.c_str(), message.c_str());
  Serial.printf("📡 Información del dispositivo publicada: %d códigos\n", storedCodes.count);
}
```

### 🔄 Validación Remota Mejorada
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
    int keyboardId = doc.containsKey("keyboard_id") ? doc["keyboard_id"].as<int>() : lastKeyboardId;
    
    if (granted) {
      controlReleWithDuration(duration / 1000.0, relay);
      resetFailedAttempts();
      publishAccessEvent(value, type, keyboardId, true, "REMOTE");
      
      Serial.printf("✅ Acceso remoto concedido: %s %s en teclado %d -> Relé %d\n", 
                    type.c_str(), value.c_str(), keyboardId, relay);
    } else {
      failedAttempts++;
      lastFailedAttempt = millis();
      publishFailedAccess(value, type, keyboardId, "REMOTE_DENIED");
      
      Serial.printf("❌ Acceso remoto denegado: %s %s en teclado %d\n", 
                    type.c_str(), value.c_str(), keyboardId);
      
      if (failedAttempts >= maxFailedAttempts) {
        activateSecurityBlock();
      }
    }
  }
}
```

## Funciones de Testing

### 🧪 Pruebas de Migración
```cpp
void testMigration() {
  Serial.println("🧪 Iniciando pruebas de migración...");
  
  // Crear códigos de prueba en formato antiguo
  storedCodes.version = 1;
  storedCodes.count = 0;
  
  addCode("PIN", "1234", 1, 1);
  addCode("TAG", "ABCD1234", 2, 2);
  
  Serial.printf("Códigos antes de migración: %d\n", storedCodes.count);
  
  // Migrar
  migrateCodesToNewFormat();
  
  Serial.printf("Códigos después de migración: %d\n", storedCodes.count);
  Serial.printf("Versión: %d\n", storedCodes.version);
  
  // Verificar que los códigos funcionan en ambos teclados
  int relay;
  bool found1 = isCodeStored("PIN", "1234", 1, &relay);
  bool found2 = isCodeStored("PIN", "1234", 2, &relay);
  
  Serial.printf("PIN 1234 en teclado 1: %s (relé %d)\n", found1 ? "SÍ" : "NO", relay);
  Serial.printf("PIN 1234 en teclado 2: %s (relé %d)\n", found2 ? "SÍ" : "NO", relay);
}
```

### 🧪 Pruebas de Funcionalidad
```cpp
void testNewFunctionality() {
  Serial.println("🧪 Iniciando pruebas de funcionalidad nueva...");
  
  // Limpiar códigos existentes
  storedCodes.count = 0;
  storedCodes.version = 2;
  
  // Añadir códigos específicos por teclado
  addCode("PIN", "1111", 1, 1); // Solo teclado 1 -> Relé 1
  addCode("PIN", "2222", 2, 2); // Solo teclado 2 -> Relé 2
  addCode("TAG", "BOTH123", 0, 1); // Ambos teclados -> Relé 1
  
  // Probar validaciones
  int relay;
  
  // PIN 1111 solo debe funcionar en teclado 1
  bool found1 = isCodeStored("PIN", "1111", 1, &relay);
  bool found2 = isCodeStored("PIN", "1111", 2, &relay);
  
  Serial.printf("PIN 1111 en teclado 1: %s (relé %d)\n", found1 ? "SÍ" : "NO", relay);
  Serial.printf("PIN 1111 en teclado 2: %s (relé %d)\n", found2 ? "SÍ" : "NO", relay);
  
  // PIN 2222 solo debe funcionar en teclado 2
  found1 = isCodeStored("PIN", "2222", 1, &relay);
  found2 = isCodeStored("PIN", "2222", 2, &relay);
  
  Serial.printf("PIN 2222 en teclado 1: %s (relé %d)\n", found1 ? "SÍ" : "NO", relay);
  Serial.printf("PIN 2222 en teclado 2: %s (relé %d)\n", found2 ? "SÍ" : "NO", relay);
  
  // TAG BOTH123 debe funcionar en ambos teclados
  found1 = isCodeStored("TAG", "BOTH123", 1, &relay);
  found2 = isCodeStored("TAG", "BOTH123", 2, &relay);
  
  Serial.printf("TAG BOTH123 en teclado 1: %s (relé %d)\n", found1 ? "SÍ" : "NO", relay);
  Serial.printf("TAG BOTH123 en teclado 2: %s (relé %d)\n", found2 ? "SÍ" : "NO", relay);
}
```

## Inicialización del Sistema

### 🚀 Setup Mejorado
```cpp
void setup() {
  Serial.begin(115200);
  Serial.println("🚀 Iniciando KC868A2 - Sistema Dual Mejorado");
  
  // Inicializar EEPROM
  EEPROM.begin(4096);
  
  // Cargar configuración
  loadConfiguration();
  
  // Cargar códigos almacenados
  loadStoredCodes();
  
  // Migrar códigos si es necesario
  migrateCodesToNewFormat();
  
  // Mostrar información del sistema
  Serial.printf("📊 Sistema cargado:\n");
  Serial.printf("  - Códigos: %d/%d\n", storedCodes.count, MAX_CODES);
  Serial.printf("  - Versión: %d\n", storedCodes.version);
  Serial.printf("  - Validación: %s\n", storedCodes.localValidationFirst ? "Local Primero" : "Remoto Primero");
  
  // Listar códigos almacenados
  listCodes();
  
  // Continuar con inicialización normal...
  setupEthernet();
  setupMqtt();
  setupWebServer();
  setupWiegand();
  
  Serial.println("✅ Sistema inicializado correctamente");
}
```

---

**Fecha del código**: Junio 2025  
**Versión**: 1.8.0  
**Compatibilidad**: Retrocompatible con v1.7.0  
**Nuevas características**: Códigos por teclado, migración automática, interfaz mejorada
