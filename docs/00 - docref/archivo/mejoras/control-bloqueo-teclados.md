# Control de Bloqueo de Teclados - Funcionalidad de Seguridad

## 🎯 **Objetivos Implementados**

### **Funcionalidad Principal**
- **Control independiente**: Bloqueo/desbloqueo de lectura de teclados independiente del bloqueo de acceso local
- **Interfaz web**: Botones de control en la página principal para gestión fácil
- **Control remoto**: Comandos MQTT para habilitar/deshabilitar lectura de teclados
- **Estado persistente**: Configuración guardada en EEPROM y restaurada al reiniciar

### **Casos de Uso**
1. **Mantenimiento**: Deshabilitar temporalmente la lectura de teclados durante mantenimiento
2. **Emergencias**: Bloqueo rápido de acceso en situaciones de emergencia
3. **Control remoto**: Gestión centralizada desde sistema de supervisión
4. **Seguridad**: Doble capa de protección (acceso local + lectura de teclados)

## 🔧 **Implementación Técnica**

### **1. Variables Globales Añadidas**

#### **Variable de Control**
```cpp
bool keyboardReadingEnabled = true;  // Control de lectura de teclados (por defecto habilitado)
```

#### **Estructura de Configuración**
```cpp
struct Config {
  // ... campos existentes ...
  bool keyboardReadingEnabled;  // Control de lectura de teclados
  // ... resto de campos ...
};
```

### **2. Lógica de Validación Actualizada**

#### **Función `validateCode()` - Verificación de Lectura**
```cpp
// Verificar si la lectura de teclados está habilitada
if (!keyboardReadingEnabled) {
  Serial.printf("🔒 [%s] Lectura de teclados DESHABILITADA - Código rechazado\n", keyboardName.c_str());
  publishFailedAccess(code, type, keyboardId, "KEYBOARD_READING_DISABLED");
  return;
}
```

#### **Flujo de Validación**
```
1. Verificar bloqueo de acceso local (existente)
2. Verificar lectura de teclados habilitada (NUEVO)
3. Verificar solicitud pendiente (existente)
4. Validar código (existente)
```

### **3. Comandos MQTT Implementados**

#### **Deshabilitar Lectura de Teclados**
```json
{
  "message_id": 12345,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_12345678",
  "message_type": 3,
  "message_info": {
    "security_command": "disable_keyboard_reading"
  }
}
```

#### **Habilitar Lectura de Teclados**
```json
{
  "message_id": 12346,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_12345678",
  "message_type": 3,
  "message_info": {
    "security_command": "enable_keyboard_reading"
  }
}
```

#### **Respuestas MQTT**
```json
// Deshabilitar
{
  "message_id": 12345,
  "response_type": 0,
  "response_info": "keyboard reading disabled"
}

// Habilitar
{
  "message_id": 12346,
  "response_type": 0,
  "response_info": "keyboard reading enabled"
}
```

### **4. Interfaz Web Implementada**

#### **Estado de Seguridad Actualizado**
```html
<tr><td>Lectura Teclados</td><td>🔓 Habilitada</td></tr>
```

#### **Botones de Control**
```html
<!-- Botones de control de lectura de teclados -->
if (keyboardReadingEnabled) {
  html += "<a href='/security/disable-keyboards'><button class='btn-warning'><i class='fas fa-keyboard icon'></i>Deshabilitar Lectura Teclados</button></a>";
} else {
  html += "<a href='/security/enable-keyboards'><button class='btn-success'><i class='fas fa-keyboard icon'></i>Habilitar Lectura Teclados</button></a>";
}
```

#### **Estilos CSS Añadidos**
```css
.security-controls {
  margin-top: 20px;
  padding: 15px;
  background: #f8f9fa;
  border-radius: 8px;
  border: 1px solid #dee2e6;
}

.btn-success {
  background-color: #28a745;
  color: white;
}

.btn-warning {
  background-color: #ffc107;
  color: #212529;
}
```

### **5. Rutas del Servidor Web**

#### **Nuevas Rutas Añadidas**
```cpp
server.on("/security/block-access", HTTP_GET, handleSecurityBlockAccess);
server.on("/security/unblock-access", HTTP_GET, handleSecurityUnblockAccess);
server.on("/security/disable-keyboards", HTTP_GET, handleSecurityDisableKeyboards);
server.on("/security/enable-keyboards", HTTP_GET, handleSecurityEnableKeyboards);
```

#### **Funciones Implementadas**
- **`handleSecurityBlockAccess()`**: Bloquea acceso local
- **`handleSecurityUnblockAccess()`**: Desbloquea acceso local
- **`handleSecurityDisableKeyboards()`**: Deshabilita lectura de teclados
- **`handleSecurityEnableKeyboards()`**: Habilita lectura de teclados

## 📊 **Información de Estado**

### **Función `publishDeviceInfo()` - Estado Actualizado**
```json
{
  "security": {
    "local_access_blocked": false,
    "keyboard_reading_enabled": true,  // NUEVO CAMPO
    "failed_attempts": 0,
    "max_failed_attempts": 3,
    "block_duration_seconds": 60
  }
}
```

### **Logs del Sistema**
```
🔒 Lectura de teclados deshabilitada remotamente
🔓 Lectura de teclados habilitada remotamente
🔒 Lectura de teclados deshabilitada desde web
🔓 Lectura de teclados habilitada desde web
```

## 🔄 **Gestión de Configuración**

### **Función `saveConfiguration()`**
```cpp
config.keyboardReadingEnabled = keyboardReadingEnabled;
```

### **Función `loadConfiguration()`**
```cpp
keyboardReadingEnabled = config.keyboardReadingEnabled;
```

### **Función `resetToDefault()`**
```cpp
keyboardReadingEnabled = true;  // Por defecto habilitado
```

## 🎨 **Interfaz de Usuario**

### **Página Principal - Sección de Seguridad**

#### **Estado Visual**
```
┌─────────────────────────────────────────────────────────┐
│                Estado de Seguridad                      │
├─────────────────────────────────────────────────────────┤
│ Parámetro          │ Valor                              │
├─────────────────────────────────────────────────────────┤
│ Acceso Local       │ 🔓 Permitido                       │
│ Lectura Teclados   │ 🔓 Habilitada                      │
│ Intentos fallidos  │ 0/3                                │
│ Duración bloqueo   │ 60 segundos                        │
└─────────────────────────────────────────────────────────┘
```

#### **Controles de Seguridad**
```
┌─────────────────────────────────────────────────────────┐
│                Controles de Seguridad                   │
├─────────────────────────────────────────────────────────┤
│ [🔒 Bloquear Acceso Local] [🔒 Deshabilitar Lectura]    │
│ [🔓 Desbloquear Acceso]   [🔓 Habilitar Lectura]       │
└─────────────────────────────────────────────────────────┘
```

### **Estados de los Botones**

#### **Cuando Lectura Habilitada**
- **Botón**: "🔒 Deshabilitar Lectura Teclados" (Amarillo)
- **Acción**: Deshabilita lectura de teclados
- **URL**: `/security/disable-keyboards`

#### **Cuando Lectura Deshabilitada**
- **Botón**: "🔓 Habilitar Lectura Teclados" (Verde)
- **Acción**: Habilita lectura de teclados
- **URL**: `/security/enable-keyboards`

## 📡 **Mensajería MQTT Completa**

### **Comandos de Seguridad Disponibles**

| Comando | Descripción | Respuesta |
|---------|-------------|-----------|
| `block_local_access` | Bloquea acceso local | `local access blocked` |
| `unblock_local_access` | Desbloquea acceso local | `local access unblocked` |
| `disable_keyboard_reading` | Deshabilita lectura teclados | `keyboard reading disabled` |
| `enable_keyboard_reading` | Habilita lectura teclados | `keyboard reading enabled` |
| `set_block_duration` | Establece duración bloqueo | `block duration updated` |

### **Estructura de Comando Completa**
```json
{
  "message_id": 12345,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_12345678",
  "message_type": 3,
  "message_info": {
    "security_command": "disable_keyboard_reading"
  }
}
```

### **Topic de Comando**
```
swatidhome/command/SWATID_12345678/security
```

### **Topic de Respuesta**
```
swatidhome/response/SWATID_12345678/rx
```

## 🧪 **Casos de Prueba**

### **1. Pruebas de Interfaz Web**

#### **Habilitar/Deshabilitar desde Web**
1. Acceder a la página principal
2. Verificar estado actual de "Lectura Teclados"
3. Hacer clic en botón correspondiente
4. Verificar cambio de estado
5. Probar lectura de teclado (debe rechazar si está deshabilitado)

#### **Verificación Visual**
- **Estado habilitado**: "🔓 Habilitada" + botón amarillo "Deshabilitar"
- **Estado deshabilitado**: "🔒 Deshabilitada" + botón verde "Habilitar"

### **2. Pruebas de MQTT**

#### **Comando de Deshabilitación**
```bash
mosquitto_pub -h broker -t "swatidhome/command/SWATID_12345678/security" \
  -m '{"message_id":12345,"device":"SWATID_PUERTA","serial":"SWATID_12345678","message_type":3,"message_info":{"security_command":"disable_keyboard_reading"}}'
```

#### **Comando de Habilitación**
```bash
mosquitto_pub -h broker -t "swatidhome/command/SWATID_12345678/security" \
  -m '{"message_id":12346,"device":"SWATID_PUERTA","serial":"SWATID_12345678","message_type":3,"message_info":{"security_command":"enable_keyboard_reading"}}'
```

### **3. Pruebas de Funcionalidad**

#### **Lectura de Teclado Deshabilitada**
1. Deshabilitar lectura de teclados
2. Intentar leer código en teclado
3. Verificar que se rechaza con mensaje "KEYBOARD_READING_DISABLED"
4. Verificar log: "🔒 Lectura de teclados DESHABILITADA"

#### **Lectura de Teclado Habilitada**
1. Habilitar lectura de teclados
2. Intentar leer código válido
3. Verificar que se procesa normalmente
4. Verificar log: "🔍 Validando: [código]"

### **4. Pruebas de Persistencia**

#### **Reinicio del Sistema**
1. Deshabilitar lectura de teclados
2. Reiniciar el ESP32
3. Verificar que el estado se mantiene deshabilitado
4. Verificar en interfaz web que muestra "🔒 Deshabilitada"

## ⚠️ **Consideraciones Importantes**

### **1. Diferencias entre Bloqueos**

| Tipo | Variable | Efecto | Duración |
|------|----------|--------|----------|
| **Acceso Local** | `localAccessBlocked` | Bloquea validación de códigos | Temporal (60s) |
| **Lectura Teclados** | `keyboardReadingEnabled` | Bloquea lectura de teclados | Permanente hasta cambio |

### **2. Comportamiento del Sistema**

#### **Lectura Deshabilitada**
- **Teclados**: No procesan códigos (PINs/TAGs)
- **Validación**: No se ejecuta (se rechaza antes)
- **MQTT**: No se envían solicitudes de validación
- **Logs**: Mensaje claro de rechazo

#### **Acceso Local Bloqueado**
- **Teclados**: Siguen leyendo códigos
- **Validación**: Se ejecuta pero rechaza todos
- **MQTT**: Se envían solicitudes pero se rechazan
- **Logs**: Mensaje de bloqueo temporal

### **3. Seguridad**

#### **Autenticación Requerida**
- **Interfaz web**: Requiere login de administrador
- **MQTT**: Requiere serial correcto del dispositivo
- **Configuración**: Se guarda en EEPROM protegida

#### **Logs de Auditoría**
- **Todas las acciones**: Registradas en Serial
- **Eventos MQTT**: Publicados en topic de eventos
- **Cambios de estado**: Timestamp y fuente registrados

## 🚀 **Beneficios de la Implementación**

### **1. Control Granular**
- **Doble capa**: Acceso local + lectura de teclados
- **Independiente**: Cada control funciona por separado
- **Flexible**: Permite diferentes niveles de restricción

### **2. Gestión Centralizada**
- **MQTT**: Control remoto desde sistema central
- **Interfaz web**: Control local desde dispositivo
- **Estado sincronizado**: Información actualizada en tiempo real

### **3. Seguridad Mejorada**
- **Bloqueo rápido**: Deshabilitación inmediata de teclados
- **Persistencia**: Estado mantenido tras reinicio
- **Auditoría**: Logs completos de todas las acciones

### **4. Usabilidad**
- **Interfaz intuitiva**: Botones claros y estados visibles
- **Feedback inmediato**: Confirmación de acciones
- **Navegación fácil**: Enlaces de retorno a página principal

## 📝 **Conclusión**

La implementación del control de bloqueo de teclados proporciona:

### **✅ Funcionalidades Completadas**:
1. **Control independiente**: Lectura de teclados separada del acceso local
2. **Interfaz web**: Botones de control en página principal
3. **Mensajería MQTT**: Comandos remotos para habilitar/deshabilitar
4. **Estado persistente**: Configuración guardada en EEPROM
5. **Logs completos**: Auditoría de todas las acciones

### **✅ Beneficios Obtenidos**:
1. **Seguridad mejorada**: Doble capa de protección
2. **Control granular**: Diferentes niveles de restricción
3. **Gestión centralizada**: Control remoto y local
4. **Usabilidad**: Interfaz intuitiva y clara

### **✅ Casos de Uso Cubiertos**:
1. **Mantenimiento**: Deshabilitación temporal de teclados
2. **Emergencias**: Bloqueo rápido de acceso
3. **Control remoto**: Gestión desde sistema central
4. **Seguridad**: Protección adicional del sistema

El sistema ahora ofrece un control completo y flexible de la funcionalidad de lectura de teclados, tanto desde la interfaz web local como desde comandos MQTT remotos, proporcionando una capa adicional de seguridad y control operacional.
