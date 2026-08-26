# Sistema de Actualización Remota (OTA) - KC868-A2

## 📋 Índice

1. [Formatos de Archivos](#formatos-de-archivos)
2. [Endpoints del Servidor](#endpoints-del-servidor)
3. [Configuración desde Web](#configuración-desde-web)
4. [Configuración Remota MQTT](#configuración-remota-mqtt)
5. [Ejemplos de Uso](#ejemplos-de-uso)
6. [Seguridad](#seguridad)

## 📁 Formatos de Archivos

### **Archivo de Firmware (.bin)**
- **Extensión**: `.bin`
- **Formato**: Archivo binario compilado para ESP32
- **Tamaño típico**: 1-4 MB
- **Nomenclatura**: `KC868A2-v{MAJOR}.{MINOR}.{PATCH}.bin`

### **Archivos de Verificación**
- **Checksum**: `KC868A2-v2.5.0.sha256`
- **Firma Digital**: `KC868A2-v2.5.0.sig` (opcional)
- **Metadatos**: `manifest.json`

### **Proceso de Compilación**
```bash
# Compilar en Arduino IDE
# Configuración: ESP32 Dev Module, 4MB Flash, Default partition scheme

# Copiar archivo compilado
cp build/esp32.esp32.esp32/KC868A2-Cursor_WIFI.ino.bin KC868A2-v2.5.0.bin

# Generar checksum
sha256sum KC868A2-v2.5.0.bin > KC868A2-v2.5.0.sha256
```

## 🌐 Endpoints del Servidor

### **Base URL**
```
https://updates.swatid.com/api
```

### **Endpoints Principales**

#### **1. Verificación de Actualizaciones**
```http
POST /api/check
Content-Type: application/json

{
  "mac": "AA:BB:CC:DD:EE:FF",
  "version": "v2.5.0",
  "model": "KC868-A2",
  "serial": "KC868A2-001"
}
```

#### **2. Descarga de Firmware**
```http
GET /api/firmware/KC868A2-v2.6.0.bin
```

#### **3. Gestión de Dispositivos**
```http
GET /api/devices
POST /api/devices
PUT /api/devices/{device_id}
```

#### **4. Gestión de Firmware**
```http
POST /api/firmware/upload
GET /api/firmware
DELETE /api/firmware/{firmware_id}
```

## 🔧 Configuración desde Web

### **Página de Configuración OTA**
Acceso: `http://{IP_DISPOSITIVO}/ota`

### **Funcionalidades**
- ✅ **Configuración de URL** del servidor de actualizaciones
- ✅ **Intervalo de verificación** (1-168 horas)
- ✅ **Habilitar/deshabilitar** actualización automática
- ✅ **Verificación manual** de actualizaciones
- ✅ **Subida de archivos** .bin para actualización manual
- ✅ **Progreso en tiempo real** de actualizaciones

### **Formulario de Configuración**
```html
<form action='/ota/config' method='post'>
  <input type='url' name='updateUrl' value='https://updates.swatid.com/api/check'>
  <input type='number' name='checkInterval' value='24' min='1' max='168'>
  <input type='checkbox' name='autoUpdateEnabled'>
  <button type='submit'>Guardar Configuración</button>
</form>
```

### **Subida de Firmware**
```html
<form id='uploadForm' enctype='multipart/form-data'>
  <input type='file' name='firmwareFile' accept='.bin' required>
  <button type='submit'>Subir y Actualizar</button>
</form>
```

## 📡 Configuración Remota MQTT

### **Comandos MQTT Disponibles**

#### **1. Configuración OTA**
```json
{
  "device": "KC868A2-001",
  "message_type": 2,
  "message_info": {
    "update_url": "https://updates.swatid.com/api/check",
    "check_interval": 24,
    "auto_update_enabled": true,
    "force_check": false
  }
}
```

#### **2. Verificación de Actualizaciones**
```json
{
  "device": "KC868A2-001",
  "message_type": 2,
  "message_info": "ota_check"
}
```

### **Respuestas MQTT**
```json
{
  "device": "KC868A2-001",
  "message_type": 1,
  "message_info": "OTA configuration updated"
}
```

## 🚀 Ejemplos de Uso

### **1. Configuración Inicial desde Web**

1. **Acceder a la página OTA**:
   ```
   http://192.168.1.100/ota
   ```

2. **Configurar servidor de actualizaciones**:
   - URL: `https://updates.swatid.com/api/check`
   - Intervalo: `24 horas`
   - Habilitar actualización automática

3. **Guardar configuración**

### **2. Verificación Manual**

1. **Desde la web**:
   - Hacer clic en "Verificar Actualizaciones"
   - Revisar logs del dispositivo

2. **Desde MQTT**:
   ```bash
   mosquitto_pub -h 188.245.213.181 -u swatidhome -P Swatid2025! \
     -t "swatidhome/KC868A2-001/command" \
     -m '{"device":"KC868A2-001","message_type":2,"message_info":"ota_check"}'
   ```

### **3. Actualización Manual**

1. **Compilar firmware**:
   ```bash
   # En Arduino IDE
   # Compilar para ESP32 Dev Module
   ```

2. **Subir archivo**:
   - Acceder a `/ota`
   - Seleccionar archivo `.bin`
   - Hacer clic en "Subir y Actualizar"

3. **Monitorear progreso**:
   - Barra de progreso en tiempo real
   - Logs en Serial Monitor

### **4. Configuración Remota**

```bash
# Configurar URL del servidor
mosquitto_pub -h 188.245.213.181 -u swatidhome -P Swatid2025! \
  -t "swatidhome/KC868A2-001/command" \
  -m '{
    "device":"KC868A2-001",
    "message_type":2,
    "message_info":{
      "update_url":"https://updates.swatid.com/api/check",
      "check_interval":12,
      "auto_update_enabled":true
    }
  }'
```

## 🔒 Seguridad

### **Validaciones Implementadas**

#### **1. Validación de Archivos**
- ✅ Verificación de extensión `.bin`
- ✅ Verificación de tamaño (1KB - 4MB)
- ✅ Verificación de magic bytes ESP32
- ✅ Verificación de checksum SHA256

#### **2. Autenticación**
- ✅ Autenticación requerida para configuración web
- ✅ Autenticación MQTT con usuario/contraseña
- ✅ Validación de tokens de acceso

#### **3. Protección contra Ataques**
- ✅ Validación estricta de magic bytes
- ✅ Verificación de checksum
- ✅ Límites de tamaño de archivo
- ✅ Sistema de rollback automático

### **Configuración de Seguridad**

#### **Contraseñas Protegidas**
- La contraseña del servidor de actualizaciones se puede configurar pero no se muestra en la interfaz
- Se almacena de forma segura en EEPROM
- Se puede cambiar remotamente vía MQTT

#### **Lista Blanca de MACs**
- El servidor puede mantener una lista blanca de MACs autorizados
- Solo dispositivos autorizados pueden recibir actualizaciones
- Configuración opcional en el servidor

## 📊 Monitoreo y Logs

### **Logs de Actualización**
```
[2025-06-15 14:30:25] 🔄 Verificando actualizaciones...
[2025-06-15 14:30:26] 📥 Actualización disponible: v2.5.0 -> v2.6.0
[2025-06-15 14:30:27] 📊 Descargando: 1.2MB / 1.2MB (100%)
[2025-06-15 14:30:28] ✅ Checksum validado: sha256:abc123...
[2025-06-15 14:30:29] 🔄 Iniciando actualización OTA...
[2025-06-15 14:30:35] ✅ Actualización completada
[2025-06-15 14:30:36] 🔄 Reiniciando dispositivo...
[2025-06-15 14:30:40] ✅ Dispositivo reiniciado - v2.6.0
```

### **Estado del Sistema**
```json
{
  "current_version": "v2.5.0",
  "build_date": "Jun 15 2025 14:30:25",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "serial_number": "KC868A2-001",
  "auto_update_enabled": true,
  "update_url": "https://updates.swatid.com/api/check",
  "check_interval": 24,
  "last_check": 1640995200,
  "free_heap": 234567,
  "flash_size": 4194304
}
```

## 📋 Checklist de Implementación

### **Funcionalidades Implementadas**
- [x] ✅ Sistema de versiones v2.5.0
- [x] ✅ Interfaz web de configuración OTA
- [x] ✅ Actualización manual desde web
- [x] ✅ Actualización automática por URL
- [x] ✅ Configuración remota vía MQTT
- [x] ✅ Sistema de rollback automático
- [x] ✅ Validación de archivos
- [x] ✅ Logging detallado

### **Pruebas Requeridas**
- [ ] ✅ Compilación exitosa del firmware
- [ ] ✅ Carga del firmware en dispositivo
- [ ] ✅ Acceso a página de configuración OTA
- [ ] ✅ Configuración de URL del servidor
- [ ] ✅ Verificación manual de actualizaciones
- [ ] ✅ Prueba de actualización manual
- [ ] ✅ Configuración remota vía MQTT
- [ ] ✅ Prueba de rollback automático

## 🎯 Próximos Pasos

### **1. Configuración del Servidor**
- [ ] Configurar servidor web para actualizaciones
- [ ] Implementar API de verificación
- [ ] Crear sistema de distribución de archivos
- [ ] Implementar autenticación del servidor

### **2. Pruebas de Integración**
- [ ] Probar actualización automática
- [ ] Verificar sistema de rollback
- [ ] Validar configuración remota
- [ ] Probar con múltiples dispositivos

### **3. Mejoras Futuras**
- [ ] Firma digital de archivos
- [ ] Compresión de archivos
- [ ] Actualización diferencial
- [ ] Dashboard de monitoreo

---

**Estado:** ✅ **IMPLEMENTACIÓN COMPLETADA**  
**Versión:** v2.5.0  
**Fecha:** $(date)  
**Próximo Paso:** Configuración del servidor de actualizaciones
