# Propuesta de Mejora - Sistema de Códigos por Teclado

## Descripción de la Mejora

### 🎯 Objetivo
Implementar un sistema de códigos que permita **control granular por teclado**, donde cada código pueda estar asociado a un teclado específico y activar un relé determinado.

### 📋 Requisitos
1. **Tipo**: PIN o TAG
2. **Código**: Valor del código (4-16 caracteres)
3. **Teclado**: ID del teclado (1 o 2)
4. **Relé**: Relé a activar (1 o 2)
5. **Compatibilidad**: Mantener funcionalidad existente

## Nueva Estructura de Datos

### 🔧 Estructura Mejorada
```cpp
struct CodeEntry {
  char type[5];        // "PIN" o "TAG"
  char value[17];      // Valor del código
  uint8_t keyboard_id; // ID del teclado (1 o 2)
  uint8_t relay;       // Relé a activar (1 o 2)
  uint8_t reserved;    // Reservado para futuras extensiones
};
```

### 📊 Comparación de Tamaños
```cpp
// Estructura actual
struct CodeEntry_OLD {
  char type[5];      // 5 bytes
  char value[17];    // 17 bytes
  uint8_t relay;     // 1 byte
  // Total: 23 bytes
};

// Estructura nueva
struct CodeEntry_NEW {
  char type[5];        // 5 bytes
  char value[17];      // 17 bytes
  uint8_t keyboard_id; // 1 byte
  uint8_t relay;       // 1 byte
  uint8_t reserved;    // 1 byte
  // Total: 25 bytes (+2 bytes por código)
};
```

### 💾 Impacto en Memoria
- **Códigos actuales**: 500 × 23 = 11,500 bytes
- **Códigos nuevos**: 500 × 25 = 12,500 bytes
- **Incremento**: +1,000 bytes (+8.7%)
- **EEPROM disponible**: 4,096 bytes
- **Espacio usado**: 12,500 + 512 = 13,012 bytes
- **❌ PROBLEMA**: Excede capacidad de EEPROM

## Solución de Compatibilidad

### 🔄 Estrategia de Migración
```cpp
struct StoredCodes {
  uint32_t validMarker;           // 0xCAFEBABE
  uint32_t version;               // Versión del formato (1 = actual, 2 = nuevo)
  bool localValidationFirst;
  uint16_t count;
  CodeEntry codes[MAX_CODES];
};
```

### 📈 Versiones de Formato
- **Versión 1**: Formato actual (sin keyboard_id)
- **Versión 2**: Formato nuevo (con keyboard_id)
- **Migración**: Automática al detectar versión antigua

## Funciones Mejoradas

### 🔍 Búsqueda por Teclado
```cpp
bool isCodeStored(const char* type, const char* value, int keyboardId, int* relay) {
  for (int i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0) {
      
      // Verificar si el código es válido para este teclado
      if (storedCodes.version == 1) {
        // Versión antigua: código válido en cualquier teclado
        if (relay != nullptr) *relay = storedCodes.codes[i].relay;
        return true;
      } else {
        // Versión nueva: verificar teclado específico
        if (storedCodes.codes[i].keyboard_id == keyboardId) {
          if (relay != nullptr) *relay = storedCodes.codes[i].relay;
          return true;
        }
      }
    }
  }
  return false;
}
```

### ➕ Añadir Código con Teclado
```cpp
bool addCode(const char* type, const char* value, int keyboardId, int relay) {
  if (storedCodes.count >= MAX_CODES) return false;

  // Verificar duplicados (mismo tipo, valor y teclado)
  for (int i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0 &&
        storedCodes.codes[i].keyboard_id == keyboardId) {
      return false; // Duplicado
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
  storedCodes.version = 2; // Marcar como versión nueva
  
  saveStoredCodes();
  return true;
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
      return true;
    }
  }
  return false;
}
```

## Validación Mejorada

### 🔍 Validación por Teclado
```cpp
void validateCode(const String& code, const String& type, int keyboardId) {
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
    
    resetFailedAttempts();
    publishAccessEvent(code, type, keyboardId, true, "LOCAL");
    return;
  }
  
  // Continuar con validación remota si es necesario...
}
```

## Interfaz Web Mejorada

### 📋 Formulario de Códigos
```html
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
    <option value='1'>Teclado 1 (GPIO 33/14)</option>
    <option value='2'>Teclado 2 (GPIO 4/16)</option>
    <option value='0'>Ambos teclados</option>
  </select>

  <label for='relay'>Relé a activar:</label>
  <select name='relay'>
    <option value='1'>Relé 1</option>
    <option value='2'>Relé 2</option>
  </select>

  <button type='submit'>Añadir Código</button>
</form>
```

### 📊 Tabla de Códigos
```html
<table>
  <tr>
    <th>Tipo</th>
    <th>Valor</th>
    <th>Teclado</th>
    <th>Relé</th>
    <th>Acción</th>
  </tr>
  <tr>
    <td>PIN</td>
    <td>1234</td>
    <td>Teclado 1</td>
    <td>Relé 1</td>
    <td><a href='/codes/delete?type=PIN&value=1234&keyboard=1'>Eliminar</a></td>
  </tr>
</table>
```

## API MQTT Mejorada

### 📡 Información del Dispositivo
```json
{
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "stored_codes": [
    {
      "type": "PIN",
      "value": "1234",
      "keyboard_id": 1,
      "relay": 1
    },
    {
      "type": "TAG",
      "value": "12345678",
      "keyboard_id": 2,
      "relay": 2
    }
  ]
}
```

## Plan de Implementación

### 🚀 Fase 1: Estructura de Datos
1. Modificar `CodeEntry` para incluir `keyboard_id`
2. Añadir campo `version` a `StoredCodes`
3. Implementar funciones de migración

### 🔧 Fase 2: Funciones de Gestión
1. Actualizar `isCodeStored()` para validar por teclado
2. Modificar `addCode()` para incluir teclado
3. Actualizar `deleteCode()` para especificar teclado

### 🌐 Fase 3: Interfaz Web
1. Modificar formulario de añadir códigos
2. Actualizar tabla de códigos existentes
3. Implementar selección de teclado

### 📡 Fase 4: API MQTT
1. Actualizar mensajes de información
2. Modificar validación remota
3. Actualizar documentación

### 🧪 Fase 5: Testing
1. Pruebas de migración de datos
2. Validación de compatibilidad
3. Pruebas de funcionalidad

## Beneficios de la Mejora

### 🔒 Seguridad
- **Control granular**: Códigos específicos por teclado
- **Restricciones**: Posibilidad de limitar acceso por ubicación
- **Auditoría**: Trazabilidad completa de accesos

### ⚙️ Funcionalidad
- **Puertas independientes**: Control real de dos puertas
- **Permisos**: Sistema de permisos por teclado
- **Escalabilidad**: Base para sistemas más complejos

### 📊 Gestión
- **Administración**: Control preciso de accesos
- **Configuración**: Flexibilidad en la configuración
- **Monitoreo**: Información detallada de uso

---

**Fecha de propuesta**: Junio 2025  
**Prioridad**: ALTA  
**Beneficio**: ALTO  
**Esfuerzo**: MEDIO
