# 🚀 Plan de Implementación - Modo Torno

## 📋 **Resumen del Plan**

Implementación del modo torno en 5 fases, con testing continuo y validación de funcionalidad en cada etapa.

## 🎯 **Objetivos de Implementación**

### **Objetivos Principales**
- ✅ **Modo torno funcional** con configuración web
- ✅ **Gestión de solicitudes pendientes** con timeout
- ✅ **Comunicación MQTT** simplificada
- ✅ **Compatibilidad** con modo normal existente
- ✅ **Interfaz web** intuitiva y funcional

### **Objetivos Secundarios**
- ✅ **Logging detallado** de eventos
- ✅ **Validación robusta** de configuración
- ✅ **Migración automática** de configuraciones
- ✅ **Documentación completa** de usuario

## 📅 **Cronograma de Implementación**

### **Fase 1: Estructuras de Datos (Día 1-2)**
- [ ] **Día 1**: Definir estructuras de datos
- [ ] **Día 2**: Implementar persistencia EEPROM

### **Fase 2: Lógica de Validación (Día 3-4)**
- [ ] **Día 3**: Modificar funciones de validación
- [ ] **Día 4**: Implementar gestión de solicitudes pendientes

### **Fase 3: Interfaz Web (Día 5-6)**
- [ ] **Día 5**: Crear formularios de configuración
- [ ] **Día 6**: Implementar handlers y validación

### **Fase 4: Comunicación MQTT (Día 7-8)**
- [ ] **Día 7**: Modificar mensajes MQTT
- [ ] **Día 8**: Implementar procesamiento de respuestas

### **Fase 5: Testing y Validación (Día 9-10)**
- [ ] **Día 9**: Pruebas de funcionalidad
- [ ] **Día 10**: Documentación y validación final

## 🔧 **Fase 1: Estructuras de Datos**

### **1.1 Definir Estructuras**
```cpp
// Configuración del modo torno
struct TurnstileConfig {
  bool enabled;           // true = modo torno, false = modo normal
  uint8_t keyboard1_relay; // Relé asignado al teclado 1 (1 o 2)
  uint8_t keyboard2_relay; // Relé asignado al teclado 2 (1 o 2)
  uint8_t reserved[5];    // Reservado para futuras extensiones
};

// Gestión de solicitudes pendientes
struct PendingRequest {
  bool active;            // true si hay solicitud pendiente
  uint8_t keyboard_id;    // Teclado origen (1 o 2)
  char code[17];          // Código introducido
  char type[5];           // Tipo de código (PIN/TAG)
  unsigned long timestamp; // Timestamp de la solicitud
  uint8_t relay_to_open;  // Relé que se abrirá si se aprueba
};
```

### **1.2 Actualizar Configuración Principal**
```cpp
struct Config {
  // ... configuración existente ...
  TurnstileConfig turnstile; // Nueva configuración de torno
};
```

### **1.3 Implementar Persistencia EEPROM**
```cpp
// Nuevas funciones de EEPROM
void saveTurnstileConfig();
void loadTurnstileConfig();
void migrateToTurnstileMode();
```

## 🔄 **Fase 2: Lógica de Validación**

### **2.1 Modificar Función validateCode**
```cpp
void validateCode(String code, String type, int keyboardId) {
  if (config.turnstile.enabled) {
    // Modo torno activo
    handleTurnstileValidation(code, type, keyboardId);
  } else {
    // Modo normal - lógica existente
    handleNormalValidation(code, type, keyboardId);
  }
}
```

### **2.2 Implementar Gestión de Solicitudes Pendientes**
```cpp
// Nuevas funciones
void handleTurnstileValidation(String code, String type, int keyboardId);
void processMqttResponse(String response);
void clearPendingRequest();
void checkPendingRequestTimeout();
```

### **2.3 Añadir Timeout y Limpieza**
```cpp
// En loop()
void checkPendingRequestTimeout() {
  if (pendingRequest.active && 
      (millis() - pendingRequest.timestamp) > TURNSTILE_TIMEOUT) {
    // Timeout - abrir relé según teclado origen
    controlReleWithDuration(releDuration, pendingRequest.relay_to_open);
    clearPendingRequest();
  }
}
```

## 🌐 **Fase 3: Interfaz Web**

### **3.1 Crear Formulario de Configuración**
```cpp
// Nueva función handleTurnstileConfig
void handleTurnstileConfig() {
  // Formulario de configuración del modo torno
  // Validación de parámetros
  // Guardado de configuración
}
```

### **3.2 Añadir Información de Estado**
```cpp
// Modificar handleRoot para incluir estado del torno
void handleRoot() {
  // ... código existente ...
  // Añadir sección de estado del modo torno
}
```

### **3.3 Implementar Handlers**
```cpp
// Nuevos handlers
void handleTurnstileConfig();
void handleTurnstileStatus();
void handleTurnstileReset();
```

## 📡 **Fase 4: Comunicación MQTT**

### **4.1 Modificar Mensajes de Solicitud**
```cpp
void publishAccessRequest(String code, String type, int keyboardId) {
  if (config.turnstile.enabled) {
    // Enviar siempre como relé 1 para MQTT
    publishMqttMessage("access_request", code, type, keyboardId, 1, "turnstile");
  } else {
    // Modo normal - lógica existente
    publishMqttMessage("access_request", code, type, keyboardId, relay, "normal");
  }
}
```

### **4.2 Implementar Procesamiento de Respuestas**
```cpp
void processMqttResponse(String response) {
  if (config.turnstile.enabled && pendingRequest.active) {
    if (response == "APPROVED") {
      // Abrir relé según teclado origen
      controlReleWithDuration(releDuration, pendingRequest.relay_to_open);
    } else {
      // Acceso denegado
      Serial.printf("❌ Torno: Acceso denegado (teclado %d)\n", 
                   pendingRequest.keyboard_id);
    }
    clearPendingRequest();
  }
}
```

### **4.3 Añadir Logging Específico**
```cpp
void logTurnstileEvent(String event, String details) {
  Serial.printf("🔄 [TORNO] %s: %s\n", event.c_str(), details.c_str());
  // Opcional: enviar a MQTT para logging remoto
}
```

## 🧪 **Fase 5: Testing y Validación**

### **5.1 Pruebas de Funcionalidad**
```cpp
// Casos de prueba
void testTurnstileMode() {
  // 1. Activar modo torno
  // 2. Configurar teclado 1 → relé 1, teclado 2 → relé 2
  // 3. Probar código en teclado 1
  // 4. Verificar apertura de relé 1
  // 5. Probar código en teclado 2
  // 6. Verificar apertura de relé 2
  // 7. Probar timeout
  // 8. Probar denegación
}
```

### **5.2 Validación de Seguridad**
```cpp
// Validaciones de seguridad
void validateTurnstileSecurity() {
  // 1. Verificar que teclado 1 y 2 usan relés diferentes
  // 2. Verificar timeout de solicitudes
  // 3. Verificar limpieza de solicitudes pendientes
  // 4. Verificar logging de eventos
}
```

### **5.3 Documentación de Usuario**
```cpp
// Crear documentación
void createUserDocumentation() {
  // 1. Manual de configuración
  // 2. Casos de uso
  // 3. Troubleshooting
  // 4. FAQ
}
```

## 📊 **Métricas de Éxito**

### **Métricas Técnicas**
- ✅ **Compilación exitosa** sin errores
- ✅ **Uso de memoria** < 80% RAM, < 90% Flash
- ✅ **Tiempo de respuesta** < 100ms para validación local
- ✅ **Timeout** configurable (30s por defecto)

### **Métricas de Funcionalidad**
- ✅ **Modo torno** funcional al 100%
- ✅ **Compatibilidad** con modo normal
- ✅ **Interfaz web** intuitiva y funcional
- ✅ **Comunicación MQTT** estable

### **Métricas de Usuario**
- ✅ **Configuración** en < 2 minutos
- ✅ **Documentación** clara y completa
- ✅ **Troubleshooting** efectivo
- ✅ **Soporte** técnico disponible

## ⚠️ **Riesgos y Mitigaciones**

### **Riesgo 1: Pérdida de Compatibilidad**
- **Mitigación**: Mantener modo normal intacto
- **Testing**: Pruebas exhaustivas de compatibilidad

### **Riesgo 2: Problemas de Memoria**
- **Mitigación**: Optimización de estructuras de datos
- **Testing**: Monitoreo continuo de uso de memoria

### **Riesgo 3: Timeout de Solicitudes**
- **Mitigación**: Timeout configurable y logging
- **Testing**: Pruebas de timeout en diferentes escenarios

### **Riesgo 4: Configuración Incorrecta**
- **Mitigación**: Validación robusta de parámetros
- **Testing**: Pruebas de configuración inválida

## 🔄 **Plan de Rollback**

### **Escenario 1: Problemas Críticos**
```cpp
// Función de rollback
void rollbackToNormalMode() {
  config.turnstile.enabled = false;
  clearPendingRequest();
  saveTurnstileConfig();
  Serial.println("🔄 Rollback a modo normal completado");
}
```

### **Escenario 2: Problemas de Configuración**
```cpp
// Reset de configuración
void resetTurnstileConfig() {
  config.turnstile.enabled = false;
  config.turnstile.keyboard1_relay = 1;
  config.turnstile.keyboard2_relay = 2;
  clearPendingRequest();
  saveTurnstileConfig();
}
```

## 📋 **Checklist de Implementación**

### **Fase 1: Estructuras de Datos**
- [ ] Definir `TurnstileConfig`
- [ ] Definir `PendingRequest`
- [ ] Actualizar `Config` principal
- [ ] Implementar persistencia EEPROM
- [ ] Testing de estructuras

### **Fase 2: Lógica de Validación**
- [ ] Modificar `validateCode()`
- [ ] Implementar gestión de solicitudes pendientes
- [ ] Añadir timeout y limpieza
- [ ] Testing de validación

### **Fase 3: Interfaz Web**
- [ ] Crear formulario de configuración
- [ ] Añadir información de estado
- [ ] Implementar handlers
- [ ] Testing de interfaz

### **Fase 4: Comunicación MQTT**
- [ ] Modificar mensajes de solicitud
- [ ] Implementar procesamiento de respuestas
- [ ] Añadir logging específico
- [ ] Testing de MQTT

### **Fase 5: Testing y Validación**
- [ ] Pruebas de funcionalidad
- [ ] Validación de seguridad
- [ ] Documentación de usuario
- [ ] Validación final

---

**📅 Fecha de Plan**: $(date)
**🔧 Estado**: Plan completado
**📋 Próximo Paso**: Iniciar Fase 1 - Estructuras de Datos
