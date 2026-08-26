# Plan de Implementación - Sistema de Códigos por Teclado

## Descripción del Plan

### 🎯 Objetivo
Implementar de forma gradual y segura el sistema de códigos por teclado, manteniendo la compatibilidad con sistemas existentes y minimizando el riesgo de pérdida de datos.

### 📋 Estrategia
- **Migración gradual**: Implementar sin romper funcionalidad existente
- **Compatibilidad**: Mantener soporte para códigos existentes
- **Rollback**: Posibilidad de revertir cambios si es necesario
- **Testing**: Pruebas exhaustivas en cada fase

## Fases de Implementación

### 🚀 Fase 1: Preparación y Estructura (1-2 días)

#### 1.1 Modificar Estructuras de Datos
```cpp
// Nueva estructura de código
struct CodeEntry {
  char type[5];        // "PIN" o "TAG"
  char value[17];      // Valor del código
  uint8_t keyboard_id; // ID del teclado (1 o 2, 0 = ambos)
  uint8_t relay;       // Relé a activar (1 o 2)
  uint8_t reserved;    // Reservado para futuras extensiones
};

// Estructura de almacenamiento con versión
struct StoredCodes {
  uint32_t validMarker;           // 0xCAFEBABE
  uint32_t version;               // 1 = actual, 2 = nuevo
  bool localValidationFirst;
  uint16_t count;
  CodeEntry codes[MAX_CODES];
};
```

#### 1.2 Función de Migración
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

#### 1.3 Función de Compatibilidad
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
```

### 🔧 Fase 2: Funciones de Gestión (2-3 días)

#### 2.1 Función de Añadir Código
```cpp
bool addCode(const char* type, const char* value, int keyboardId, int relay) {
  if (storedCodes.count >= MAX_CODES) return false;
  
  // Verificar duplicados
  for (int i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0 &&
        storedCodes.codes[i].keyboard_id == keyboardId) {
      return false; // Duplicado exacto
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
  return true;
}
```

#### 2.2 Función de Eliminar Código
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
      return true;
    }
  }
  return false;
}
```

#### 2.3 Función de Listar Códigos
```cpp
void listCodes() {
  Serial.println("📋 Códigos almacenados:");
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

### 🌐 Fase 3: Interfaz Web (3-4 días)

#### 3.1 Formulario de Añadir Código
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
        Serial.printf("✅ Código añadido: %s %s -> Teclado %d -> Relé %d\n", 
                      type.c_str(), value.c_str(), keyboardId, relay);
        server.sendHeader("Location", "/codes");
        server.send(303);
        return;
      }
    }
    
    server.send(200, "text/html",
      "<html><body><h1>❌ Error al añadir código</h1>"
      "<p>Verifique el formato y que no exista ya</p>"
      "<a href='/codes'>Volver</a></body></html>");
  }
}
```

#### 3.2 Página de Gestión de Códigos
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
      body { font-family: Arial, sans-serif; margin: 30px; }
      table { width: 100%; border-collapse: collapse; margin-top: 30px; }
      th, td { border: 1px solid #ccc; padding: 10px; text-align: center; }
      th { background-color: #f2f2f2; }
      form { background: #fff; padding: 20px; border-radius: 10px; margin-bottom: 30px; }
      label { display: block; margin-top: 15px; font-weight: bold; }
      input, select { width: 100%; padding: 8px; margin-top: 5px; }
      button { background-color: #2ecc71; color: white; padding: 10px 15px; border: none; border-radius: 5px; margin-top: 15px; cursor: pointer; }
    </style>
  </head>
  <body>
    <h1>Gestión de Códigos - Sistema Dual</h1>
    
    <form action='/codes/add' method='post'>
      <h2>Añadir nuevo código</h2>
      
      <label for='type'>Tipo:</label>
      <select name='type'>
        <option value='PIN'>PIN (4-6 dígitos)</option>
        <option value='TAG'>TAG (tarjeta RFID/NFC)</option>
      </select>

      <label for='value'>Código:</label>
      <input type='text' name='value' maxlength='16' required>

      <label for='keyboard_id'>Teclado autorizado:</label>
      <select name='keyboard_id'>
        <option value='0'>Ambos teclados</option>
        <option value='1'>Teclado 1 (GPIO 33/14)</option>
        <option value='2'>Teclado 2 (GPIO 4/16)</option>
      </select>

      <label for='relay'>Relé a activar:</label>
      <select name='relay'>
        <option value='1'>Relé 1</option>
        <option value='2'>Relé 2</option>
      </select>

      <button type='submit'>Añadir Código</button>
    </form>

    <h2>Códigos Almacenados ()=====";
  
  html += String(storedCodes.count) + "/" + String(MAX_CODES);
  html += R"=====()</h2>
    <table>
      <tr><th>Tipo</th><th>Valor</th><th>Teclado</th><th>Relé</th><th>Acción</th></tr>
  )=====";

  for (int i = 0; i < storedCodes.count; i++) {
    html += "<tr>";
    html += "<td>" + String(storedCodes.codes[i].type) + "</td>";
    html += "<td>" + String(storedCodes.codes[i].value) + "</td>";
    
    String keyboardName = "Ambos";
    if (storedCodes.codes[i].keyboard_id == 1) keyboardName = "Teclado 1";
    else if (storedCodes.codes[i].keyboard_id == 2) keyboardName = "Teclado 2";
    
    html += "<td>" + keyboardName + "</td>";
    html += "<td>" + String(storedCodes.codes[i].relay) + "</td>";
    html += "<td><a href='/codes/delete?type=" + String(storedCodes.codes[i].type);
    html += "&value=" + String(storedCodes.codes[i].value);
    html += "&keyboard=" + String(storedCodes.codes[i].keyboard_id) + "'>Eliminar</a></td>";
    html += "</tr>";
  }

  if (storedCodes.count == 0) {
    html += "<tr><td colspan='5'>No hay códigos almacenados</td></tr>";
  }

  html += R"=====(
    </table>
    <br>
    <a href='/'><button>Volver al inicio</button></a>
  </body></html>
  )=====";

  server.send(200, "text/html", html);
}
```

### 📡 Fase 4: API MQTT (2-3 días)

#### 4.1 Información del Dispositivo
```cpp
void publishDeviceInfo(unsigned long originalMessageId) {
  if (!mqttClient.connected()) return;

  DynamicJsonDocument infoDoc(2048);
  infoDoc["device"] = deviceName;
  infoDoc["serial"] = fixedSerialNumber;
  infoDoc["firmware_version"] = firmwareVersion;
  infoDoc["codes_version"] = storedCodes.version; // Nueva información

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
}
```

#### 4.2 Validación Remota Mejorada
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
    } else {
      failedAttempts++;
      lastFailedAttempt = millis();
      publishFailedAccess(value, type, keyboardId, "REMOTE_DENIED");
      
      if (failedAttempts >= maxFailedAttempts) {
        activateSecurityBlock();
      }
    }
  }
}
```

### 🧪 Fase 5: Testing y Validación (2-3 días)

#### 5.1 Pruebas de Migración
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

#### 5.2 Pruebas de Funcionalidad
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

## Cronograma de Implementación

### 📅 Timeline Detallado

| Fase | Duración | Días | Actividades |
|------|----------|------|-------------|
| **Fase 1** | 1-2 días | 1-2 | Estructuras de datos y migración |
| **Fase 2** | 2-3 días | 3-5 | Funciones de gestión |
| **Fase 3** | 3-4 días | 6-9 | Interfaz web |
| **Fase 4** | 2-3 días | 10-12 | API MQTT |
| **Fase 5** | 2-3 días | 13-15 | Testing y validación |

### 🎯 Hitos Importantes
- **Día 2**: Migración de datos funcionando
- **Día 5**: Funciones de gestión completas
- **Día 9**: Interfaz web operativa
- **Día 12**: API MQTT actualizada
- **Día 15**: Sistema completamente funcional

## Riesgos y Mitigaciones

### ⚠️ Riesgos Identificados

#### 1. Pérdida de Datos
- **Riesgo**: Corrupción de EEPROM durante migración
- **Mitigación**: Backup automático antes de migración
- **Rollback**: Función de restauración

#### 2. Incompatibilidad
- **Riesgo**: Códigos existentes no funcionen
- **Mitigación**: Modo de compatibilidad (keyboard_id = 0)
- **Testing**: Pruebas exhaustivas

#### 3. Rendimiento
- **Riesgo**: Búsqueda más lenta por validación adicional
- **Mitigación**: Optimización de algoritmos
- **Monitoreo**: Medición de tiempos

### 🛡️ Estrategias de Mitigación

#### Backup Automático
```cpp
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

#### Modo de Compatibilidad
```cpp
bool isCompatibilityMode() {
  return (storedCodes.version == 1);
}

void enableCompatibilityMode() {
  storedCodes.version = 1;
  saveStoredCodes();
  Serial.println("🔄 Modo de compatibilidad activado");
}
```

## Criterios de Éxito

### ✅ Definición de Éxito
1. **Migración exitosa**: Todos los códigos existentes funcionan
2. **Nueva funcionalidad**: Códigos específicos por teclado operativos
3. **Compatibilidad**: Sistema funciona con códigos antiguos y nuevos
4. **Rendimiento**: Tiempo de validación < 100ms
5. **Estabilidad**: Sin errores en 48 horas de funcionamiento

### 📊 Métricas de Validación
- **Tiempo de migración**: < 5 segundos
- **Tiempo de validación**: < 100ms
- **Uso de memoria**: < 13KB EEPROM
- **Tasa de error**: < 0.1%
- **Compatibilidad**: 100% con códigos existentes

---

**Fecha del plan**: Junio 2025  
**Duración total**: 15 días  
**Prioridad**: ALTA  
**Riesgo**: MEDIO
