# Formatos de Archivos para Actualización OTA

## 📁 Formato del Archivo de Firmware

### **Archivo Principal (.bin)**

#### **Especificaciones Técnicas**
- **Extensión**: `.bin`
- **Formato**: Archivo binario compilado para ESP32
- **Tamaño típico**: 1-4 MB
- **Compresión**: No (archivo binario directo)
- **Codificación**: Binario puro

#### **Nomenclatura de Archivos**
```
KC868A2-v{MAJOR}.{MINOR}.{PATCH}.bin
```

**Ejemplos:**
- `KC868A2-v2.5.0.bin`
- `KC868A2-v2.6.0.bin`
- `KC868A2-v3.0.0.bin`

#### **Estructura del Archivo .bin**
```
┌─────────────────────────────────────────┐
│ Header ESP32 (Magic Bytes)              │
│ 0xE9 0x02 0x00 0x00                     │
├─────────────────────────────────────────┤
│ Segment Header 1                        │
│ - Load Address                          │
│ - Data Size                             │
│ - Data                                  │
├─────────────────────────────────────────┤
│ Segment Header 2                        │
│ - Load Address                          │
│ - Data Size                             │
│ - Data                                  │
├─────────────────────────────────────────┤
│ ... (más segmentos)                     │
├─────────────────────────────────────────┤
│ Checksum                                │
└─────────────────────────────────────────┘
```

### **Archivos de Verificación**

#### **1. Archivo de Checksum (.sha256)**
```
KC868A2-v2.5.0.sha256
```

**Contenido:**
```
abc123def456789...  KC868A2-v2.5.0.bin
```

**Formato:**
```
{CHECKSUM_SHA256}  {NOMBRE_ARCHIVO}
```

#### **2. Archivo de Firma Digital (.sig) - Opcional**
```
KC868A2-v2.5.0.sig
```

**Contenido:**
```
-----BEGIN PGP SIGNATURE-----
Version: GnuPG v1.4.11 (GNU/Linux)

iQEcBAABAgAGBQJX...
-----END PGP SIGNATURE-----
```

### **Archivo de Metadatos (manifest.json)**
```
manifest.json
```

**Contenido:**
```json
{
  "device_models": {
    "KC868-A2": {
      "current_version": "v2.5.0",
      "download_url": "https://updates.swatid.com/firmware/KC868A2-v2.5.0.bin",
      "checksum": "sha256:abc123def456789...",
      "size": 1048576,
      "release_date": "2025-06-15",
      "changelog": [
        "Sistema OTA implementado",
        "Corrección de persistencia EEPROM",
        "Mejoras de seguridad"
      ],
      "mac_whitelist": [
        "AA:BB:CC:DD:EE:FF",
        "11:22:33:44:55:66"
      ],
      "min_version": "v2.0.0",
      "max_version": "v3.0.0"
    }
  }
}
```

## 🔧 Proceso de Compilación

### **1. Compilación en Arduino IDE**

#### **Configuración de la Placa**
```
Board: "ESP32 Dev Module"
CPU Frequency: "240MHz (WiFi/BT)"
Flash Frequency: "80MHz"
Flash Mode: "QIO"
Flash Size: "4MB (32Mb)"
Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
Upload Speed: "115200"
```

#### **Configuración de Particiones**
```
# partitions.csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x100000,
```

### **2. Generación del Archivo .bin**

#### **Ubicación del Archivo Compilado**
```
{PROYECTO}/build/esp32.esp32.esp32/KC868A2-Cursor_WIFI.ino.bin
```

#### **Comando de Copia**
```bash
# Copiar archivo compilado
cp build/esp32.esp32.esp32/KC868A2-Cursor_WIFI.ino.bin KC868A2-v2.5.0.bin

# Generar checksum
sha256sum KC868A2-v2.5.0.bin > KC868A2-v2.5.0.sha256
```

### **3. Validación del Archivo**

#### **Verificación de Magic Bytes**
```bash
# Verificar magic bytes ESP32
hexdump -C KC868A2-v2.5.0.bin | head -1
# Debe mostrar: e9 02 00 00
```

#### **Verificación de Tamaño**
```bash
# Verificar tamaño del archivo
ls -la KC868A2-v2.5.0.bin
# Tamaño típico: 1-4 MB
```

## 📤 Actualización por Web

### **Proceso de Subida**

#### **1. Validación en el Cliente**
```javascript
function validateFirmwareFile(file) {
  // Verificar extensión
  if (!file.name.endsWith('.bin')) {
    throw new Error('Solo archivos .bin permitidos');
  }
  
  // Verificar tamaño (máximo 4MB)
  if (file.size > 4 * 1024 * 1024) {
    throw new Error('Archivo demasiado grande (máximo 4MB)');
  }
  
  // Verificar tamaño mínimo (1KB)
  if (file.size < 1024) {
    throw new Error('Archivo demasiado pequeño');
  }
  
  return true;
}
```

#### **2. Validación en el Servidor**
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

### **Interfaz Web de Subida**

#### **HTML Form**
```html
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
```

#### **JavaScript de Subida**
```javascript
document.getElementById('uploadForm').addEventListener('submit', function(e) {
  e.preventDefault();
  
  const fileInput = document.getElementById('firmwareFile');
  const file = fileInput.files[0];
  
  if (!file) {
    alert('Por favor, seleccione un archivo');
    return;
  }
  
  // Validar archivo
  try {
    validateFirmwareFile(file);
  } catch (error) {
    alert('Error: ' + error.message);
    return;
  }
  
  // Mostrar progreso
  document.getElementById('uploadProgress').style.display = 'block';
  
  // Subir archivo
  const formData = new FormData();
  formData.append('firmwareFile', file);
  
  fetch('/ota/upload', {
    method: 'POST',
    body: formData
  })
  .then(response => response.text())
  .then(data => {
    if (data.includes('completada')) {
      alert('Actualización completada. El dispositivo se reiniciará...');
      setTimeout(() => {
        window.location.href = '/';
      }, 5000);
    } else {
      alert('Error en la actualización: ' + data);
    }
  })
  .catch(error => {
    alert('Error subiendo archivo: ' + error);
  });
});
```

## 🌐 Actualización Automática (OTAA)

### **Proceso de Descarga**

#### **1. Consulta al Servidor**
```cpp
void checkForUpdates() {
  HTTPClient http;
  http.begin(otaConfig.updateUrl);
  http.addHeader("Content-Type", "application/json");
  
  // Crear JSON con información del dispositivo
  DynamicJsonDocument deviceInfo(512);
  deviceInfo["mac"] = ETH.macAddress();
  deviceInfo["version"] = firmwareVersion;
  deviceInfo["model"] = "KC868-A2";
  deviceInfo["serial"] = fixedSerialNumber;
  
  String jsonString;
  serializeJson(deviceInfo, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    processUpdateResponse(response);
  }
}
```

#### **2. Procesamiento de Respuesta**
```cpp
void processUpdateResponse(const String& response) {
  DynamicJsonDocument updateInfo(1024);
  deserializeJson(updateInfo, response);
  
  if (updateInfo["available"].as<bool>()) {
    String newVersion = updateInfo["version"].as<String>();
    String downloadUrl = updateInfo["download_url"].as<String>();
    String checksum = updateInfo["checksum"].as<String>();
    
    Serial.printf("📥 Actualización disponible: %s -> %s\n", 
                  firmwareVersion, newVersion.c_str());
    
    if (compareVersions(newVersion, firmwareVersion) > 0) {
      downloadAndUpdate(downloadUrl, checksum);
    }
  }
}
```

#### **3. Descarga y Validación**
```cpp
bool downloadAndUpdate(const String& downloadUrl, const String& expectedChecksum) {
  HTTPClient http;
  http.begin(downloadUrl);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    int contentLength = http.getSize();
    
    // Verificar tamaño
    if (contentLength > 4 * 1024 * 1024) {
      Serial.println("❌ Archivo demasiado grande");
      return false;
    }
    
    // Iniciar actualización OTA
    if (Update.begin(contentLength)) {
      WiFiClient* stream = http.getStreamPtr();
      
      size_t written = 0;
      uint8_t buff[1024] = { 0 };
      SHA256 sha256;
      
      while (http.connected() && (written < contentLength)) {
        size_t size = stream->available();
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          
          // Actualizar checksum
          sha256.update(buff, c);
          
          // Escribir a OTA
          Update.write(buff, c);
          written += c;
        }
      }
      
      // Verificar checksum
      uint8_t hash[SHA256_SIZE];
      sha256.finalize(hash, SHA256_SIZE);
      
      String actualChecksum = "sha256:";
      for (int i = 0; i < SHA256_SIZE; i++) {
        actualChecksum += String(hash[i], HEX);
      }
      
      if (actualChecksum != expectedChecksum) {
        Serial.println("❌ Checksum no coincide");
        Update.abort();
        return false;
      }
      
      if (Update.end()) {
        Serial.println("✅ Actualización OTA completada");
        ESP.restart();
        return true;
      }
    }
  }
  
  return false;
}
```

## 🔒 Seguridad y Validación

### **Validaciones Implementadas**

#### **1. Validación de Archivo**
- ✅ Verificación de extensión .bin
- ✅ Verificación de tamaño (1KB - 4MB)
- ✅ Verificación de magic bytes ESP32
- ✅ Verificación de checksum SHA256

#### **2. Validación de Servidor**
- ✅ Verificación de URL del servidor
- ✅ Autenticación requerida
- ✅ Lista blanca de MACs (opcional)

#### **3. Validación de Versión**
- ✅ Comparación semántica de versiones
- ✅ Verificación de compatibilidad
- ✅ Prevención de downgrade (opcional)

### **Protección contra Ataques**

#### **1. Ataques de Archivos Maliciosos**
- Validación estricta de magic bytes
- Verificación de checksum
- Límites de tamaño de archivo

#### **2. Ataques de Servidor**
- Autenticación requerida
- Validación de URL
- Lista blanca de servidores

#### **3. Ataques de Rollback**
- Verificación de versión mínima
- Prevención de downgrade
- Sistema de rollback automático

## 📋 Checklist de Validación

### **Antes de Subir Archivo**
- [ ] ✅ Archivo compilado correctamente
- [ ] ✅ Extensión .bin
- [ ] ✅ Tamaño entre 1KB y 4MB
- [ ] ✅ Magic bytes ESP32 válidos
- [ ] ✅ Checksum SHA256 generado
- [ ] ✅ Metadatos actualizados

### **Durante la Actualización**
- [ ] ✅ Validación de archivo en cliente
- [ ] ✅ Validación de archivo en servidor
- [ ] ✅ Verificación de checksum
- [ ] ✅ Progreso de actualización
- [ ] ✅ Confirmación de instalación

### **Después de la Actualización**
- [ ] ✅ Verificación de nueva versión
- [ ] ✅ Funcionamiento correcto del sistema
- [ ] ✅ Logs de actualización
- [ ] ✅ Sistema de rollback disponible

---

**Estado:** ✅ **DOCUMENTACIÓN COMPLETADA**  
**Versión:** v2.5.0  
**Fecha:** $(date)  
**Próximo Paso:** Implementación de configuración OTA
