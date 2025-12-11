# Análisis para Sistema de Actualización Remota (OTA)

## 🎯 Objetivo

Desarrollar un sistema completo de actualización remota (Over-The-Air) para el dispositivo KC868-A2 que permita:

1. **Actualización automática** desde URL configurable
2. **Actualización manual** desde interfaz web
3. **Sistema de versiones** integrado
4. **Validación de integridad** y rollback automático

## 📋 Requisitos Funcionales

### 1. **Sistema de Versiones**
- ✅ Variable de versión no editable en compilación
- ✅ Versión actual: **v2.5** (actualizar desde v1.7.0-COMPLETE-SECURITY)
- ✅ Mostrar versión en página principal
- ✅ Comparación de versiones para actualizaciones

### 2. **Actualización Automática por URL**
- ✅ Configurar URL de servidor de actualizaciones
- ✅ Consulta automática por MAC del dispositivo
- ✅ Descarga y validación de firmware
- ✅ Actualización automática si hay versión más nueva

### 3. **Actualización Manual desde Web**
- ✅ Interfaz web para subir archivo .bin
- ✅ Validación de archivo antes de actualización
- ✅ Progreso de actualización en tiempo real
- ✅ Confirmación de actualización exitosa

## 🏗️ Arquitectura del Sistema

### **Componentes Principales**

```
┌─────────────────────────────────────────────────────────────┐
│                    SISTEMA OTA                              │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │   Web Interface │  │  Auto Update    │  │  Version     │ │
│  │                 │  │                 │  │  Manager     │ │
│  │ • Upload .bin   │  │ • URL Check     │  │              │ │
│  │ • Progress      │  │ • Download      │  │ • Compare    │ │
│  │ • Status        │  │ • Validate      │  │ • Display    │ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │   OTA Manager   │  │  File Validator │  │  Rollback    │ │
│  │                 │  │                 │  │  System      │ │
│  │ • Flash Write   │  │ • Checksum      │  │              │ │
│  │ • Partition     │  │ • Size Check    │  │ • Backup     │ │
│  │ • Boot Switch   │  │ • Signature     │  │ • Recovery   │ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## 🔧 Implementación Técnica

### **1. Sistema de Versiones**

#### **Estructura de Versión**
```cpp
// =================== SISTEMA DE VERSIONES ===================
#define FIRMWARE_VERSION_MAJOR 2
#define FIRMWARE_VERSION_MINOR 5
#define FIRMWARE_VERSION_PATCH 0
#define FIRMWARE_VERSION_BUILD __DATE__ " " __TIME__

const char* firmwareVersion = "v2.5.0";
const char* firmwareBuild = FIRMWARE_VERSION_BUILD;
const char* firmwareFullVersion = "v2.5.0-" FIRMWARE_VERSION_BUILD;

// Información del dispositivo para actualizaciones
struct DeviceInfo {
  String macAddress;
  String currentVersion;
  String deviceModel;
  String serialNumber;
  size_t flashSize;
  size_t freeHeap;
};
```

#### **Comparación de Versiones**
```cpp
int compareVersions(const String& version1, const String& version2) {
  // Parsear versiones (v2.5.0 -> [2,5,0])
  int v1[3], v2[3];
  parseVersion(version1, v1);
  parseVersion(version2, v2);
  
  for (int i = 0; i < 3; i++) {
    if (v1[i] > v2[i]) return 1;   // version1 > version2
    if (v1[i] < v2[i]) return -1;  // version1 < version2
  }
  return 0; // version1 == version2
}
```

### **2. Actualización Automática por URL**

#### **Configuración de URL**
```cpp
// =================== CONFIGURACIÓN OTA ===================
struct OTAConfig {
  char updateUrl[256];           // URL del servidor de actualizaciones
  bool autoUpdateEnabled;        // Habilitar actualización automática
  int checkInterval;             // Intervalo de verificación (horas)
  unsigned long lastCheck;       // Última verificación
  bool forceUpdate;              // Forzar actualización
};

OTAConfig otaConfig;
```

#### **Proceso de Actualización Automática**
```cpp
void checkForUpdates() {
  if (!otaConfig.autoUpdateEnabled) return;
  
  // Verificar si es hora de comprobar actualizaciones
  if (millis() - otaConfig.lastCheck < otaConfig.checkInterval * 3600000) {
    return;
  }
  
  Serial.println("🔄 Verificando actualizaciones automáticas...");
  
  // Crear JSON con información del dispositivo
  DynamicJsonDocument deviceInfo(512);
  deviceInfo["mac"] = ETH.macAddress();
  deviceInfo["version"] = firmwareVersion;
  deviceInfo["model"] = "KC868-A2";
  deviceInfo["serial"] = fixedSerialNumber;
  
  // Enviar consulta al servidor
  String response = httpPost(otaConfig.updateUrl, deviceInfo);
  
  if (response.length() > 0) {
    DynamicJsonDocument updateInfo(1024);
    deserializeJson(updateInfo, response);
    
    if (updateInfo["available"].as<bool>()) {
      String newVersion = updateInfo["version"].as<String>();
      String downloadUrl = updateInfo["download_url"].as<String>();
      
      Serial.printf("📥 Actualización disponible: %s -> %s\n", 
                    firmwareVersion, newVersion.c_str());
      
      if (downloadAndUpdate(downloadUrl)) {
        Serial.println("✅ Actualización automática completada");
      } else {
        Serial.println("❌ Error en actualización automática");
      }
    } else {
      Serial.println("✅ Firmware actualizado");
    }
  }
  
  otaConfig.lastCheck = millis();
}
```

### **3. Actualización Manual desde Web**

#### **Interfaz Web**
```html
<!-- Página de actualización OTA -->
<div class="ota-section">
  <h2><i class="fas fa-download"></i> Actualización de Firmware</h2>
  
  <div class="current-version">
    <h3>Versión Actual</h3>
    <p><strong>Firmware:</strong> <span id="currentVersion">v2.5.0</span></p>
    <p><strong>Compilación:</strong> <span id="buildDate">Jun 15 2025 14:30:25</span></p>
  </div>
  
  <div class="update-methods">
    <!-- Actualización automática -->
    <div class="auto-update">
      <h3>Actualización Automática</h3>
      <form action="/ota/config" method="post">
        <div class="form-group">
          <label for="updateUrl">URL del Servidor:</label>
          <input type="url" id="updateUrl" name="updateUrl" 
                 placeholder="https://updates.swatid.com/api/check">
        </div>
        <div class="form-group">
          <label for="checkInterval">Intervalo de Verificación (horas):</label>
          <input type="number" id="checkInterval" name="checkInterval" 
                 min="1" max="168" value="24">
        </div>
        <div class="form-group">
          <label>
            <input type="checkbox" id="autoUpdateEnabled" name="autoUpdateEnabled">
            Habilitar actualización automática
          </label>
        </div>
        <button type="submit">Guardar Configuración</button>
      </form>
      <button onclick="checkForUpdates()">Verificar Actualizaciones Ahora</button>
    </div>
    
    <!-- Actualización manual -->
    <div class="manual-update">
      <h3>Actualización Manual</h3>
      <form id="uploadForm" enctype="multipart/form-data">
        <div class="form-group">
          <label for="firmwareFile">Archivo de Firmware (.bin):</label>
          <input type="file" id="firmwareFile" name="firmwareFile" 
                 accept=".bin" required>
        </div>
        <button type="submit">Subir y Actualizar</button>
      </form>
      
      <div id="uploadProgress" class="progress-container" style="display:none;">
        <div class="progress-bar">
          <div id="progressBar" class="progress-fill"></div>
        </div>
        <p id="progressText">Preparando actualización...</p>
      </div>
    </div>
  </div>
</div>
```

#### **Manejo de Subida de Archivos**
```cpp
void handleOTAUpload() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("📥 Iniciando actualización OTA: %s\n", upload.filename.c_str());
    
    // Validar archivo
    if (!upload.filename.endsWith(".bin")) {
      server.send(400, "text/plain", "Error: Solo archivos .bin permitidos");
      return;
    }
    
    // Iniciar OTA
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      server.send(500, "text/plain", "Error: No se pudo iniciar OTA");
      return;
    }
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Escribir datos
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      server.send(500, "text/plain", "Error: Fallo al escribir datos");
      return;
    }
    
    // Calcular progreso
    int progress = (upload.totalSize * 100) / upload.currentSize;
    Serial.printf("📊 Progreso OTA: %d%%\n", progress);
    
  } else if (upload.status == UPLOAD_FILE_END) {
    // Finalizar actualización
    if (Update.end(true)) {
      Serial.println("✅ Actualización OTA completada");
      server.send(200, "text/plain", "Actualización completada. Reiniciando...");
      
      // Reiniciar después de 2 segundos
      delay(2000);
      ESP.restart();
    } else {
      Serial.println("❌ Error al finalizar OTA");
      server.send(500, "text/plain", "Error al finalizar actualización");
    }
  }
}
```

### **4. Validación y Seguridad**

#### **Validación de Archivos**
```cpp
bool validateFirmwareFile(const uint8_t* data, size_t size) {
  // Verificar tamaño mínimo
  if (size < 1024) {
    Serial.println("❌ Archivo demasiado pequeño");
    return false;
  }
  
  // Verificar tamaño máximo (4MB para ESP32)
  if (size > 4 * 1024 * 1024) {
    Serial.println("❌ Archivo demasiado grande");
    return false;
  }
  
  // Verificar magic bytes del ESP32
  if (data[0] != 0xE9 || data[1] != 0x02) {
    Serial.println("❌ Archivo no válido para ESP32");
    return false;
  }
  
  // Verificar checksum básico
  uint32_t checksum = 0;
  for (size_t i = 0; i < size; i++) {
    checksum += data[i];
  }
  
  Serial.printf("📊 Checksum del archivo: 0x%08X\n", checksum);
  
  return true;
}
```

#### **Sistema de Rollback**
```cpp
void setupRollback() {
  // Configurar particiones para rollback
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next_update = esp_ota_get_next_update_partition(NULL);
  
  Serial.printf("📊 Partición actual: %s\n", running->label);
  Serial.printf("📊 Partición de actualización: %s\n", next_update->label);
  
  // Verificar integridad del firmware actual
  if (!verifyCurrentFirmware()) {
    Serial.println("⚠️ Firmware actual corrupto, iniciando rollback...");
    performRollback();
  }
}

bool verifyCurrentFirmware() {
  // Verificar que el firmware actual arranca correctamente
  // Implementar verificaciones básicas de integridad
  return true;
}

void performRollback() {
  Serial.println("🔄 Ejecutando rollback...");
  
  const esp_partition_t* last_known_good = esp_ota_get_last_invalid_partition();
  if (last_known_good != NULL) {
    esp_ota_set_boot_partition(last_known_good);
    Serial.println("✅ Rollback completado, reiniciando...");
    ESP.restart();
  } else {
    Serial.println("❌ No hay firmware válido para rollback");
  }
}
```

## 📁 Estructura de Archivos

### **Archivos a Enviar/Descargar**
```
firmware/
├── KC868A2-v2.5.0.bin          # Firmware principal
├── KC868A2-v2.5.0.sha256       # Checksum SHA256
├── KC868A2-v2.5.0.sig          # Firma digital (opcional)
└── manifest.json               # Información de versiones
```

### **Manifest del Servidor**
```json
{
  "device_models": {
    "KC868-A2": {
      "current_version": "v2.5.0",
      "download_url": "https://updates.swatid.com/firmware/KC868A2-v2.5.0.bin",
      "checksum": "sha256:abc123...",
      "size": 1048576,
      "release_date": "2025-06-15",
      "changelog": [
        "Corrección de persistencia EEPROM",
        "Sistema OTA integrado",
        "Mejoras de seguridad"
      ],
      "mac_whitelist": [
        "AA:BB:CC:DD:EE:FF",
        "11:22:33:44:55:66"
      ]
    }
  }
}
```

## 🔄 Flujo de Actualización

### **Actualización Automática**
```
1. Dispositivo inicia
2. Verifica configuración OTA
3. Si está habilitada y es hora de verificar:
   a. Envía información del dispositivo al servidor
   b. Servidor responde con información de actualización
   c. Si hay nueva versión:
      - Descarga archivo .bin
      - Valida checksum
      - Inicia actualización OTA
      - Reinicia dispositivo
4. Continúa funcionamiento normal
```

### **Actualización Manual**
```
1. Usuario accede a interfaz web
2. Selecciona archivo .bin
3. Sistema valida archivo
4. Inicia proceso OTA
5. Muestra progreso en tiempo real
6. Completa actualización
7. Reinicia dispositivo
8. Verifica nueva versión
```

## 🛡️ Consideraciones de Seguridad

### **Validación de Archivos**
- ✅ Verificación de checksum SHA256
- ✅ Validación de tamaño de archivo
- ✅ Verificación de magic bytes ESP32
- ✅ Firma digital (opcional)

### **Control de Acceso**
- ✅ Autenticación requerida para actualizaciones
- ✅ Lista blanca de MACs (opcional)
- ✅ Validación de URL del servidor

### **Recuperación**
- ✅ Sistema de rollback automático
- ✅ Verificación de integridad al arrancar
- ✅ Particiones de respaldo

## 📊 Métricas y Monitoreo

### **Logs de Actualización**
```
[2025-06-15 14:30:25] 🔄 Verificando actualizaciones...
[2025-06-15 14:30:26] 📥 Actualización disponible: v2.4.0 -> v2.5.0
[2025-06-15 14:30:27] 📊 Descargando: 1.2MB / 1.2MB (100%)
[2025-06-15 14:30:28] ✅ Checksum validado: sha256:abc123...
[2025-06-15 14:30:29] 🔄 Iniciando actualización OTA...
[2025-06-15 14:30:35] ✅ Actualización completada
[2025-06-15 14:30:36] 🔄 Reiniciando dispositivo...
[2025-06-15 14:30:40] ✅ Dispositivo reiniciado - v2.5.0
```

## 🚀 Plan de Implementación

### **Fase 1: Sistema de Versiones**
- [ ] Actualizar versión a v2.5.0
- [ ] Implementar estructura de versiones
- [ ] Mostrar versión en interfaz web

### **Fase 2: Actualización Manual**
- [ ] Crear interfaz web de actualización
- [ ] Implementar subida de archivos
- [ ] Validación de archivos
- [ ] Proceso OTA básico

### **Fase 3: Actualización Automática**
- [ ] Configuración de URL
- [ ] Consulta automática al servidor
- [ ] Descarga automática
- [ ] Actualización automática

### **Fase 4: Seguridad y Recuperación**
- [ ] Sistema de rollback
- [ ] Validación de integridad
- [ ] Firma digital
- [ ] Control de acceso

---

**Estado:** 📋 **ANÁLISIS COMPLETADO**  
**Versión Objetivo:** v2.5.0  
**Fecha:** $(date)  
**Próximo Paso:** Implementación Fase 1
