# Endpoints del Servidor Remoto para OTA

## 🌐 API del Servidor de Actualizaciones

### **Base URL**
```
https://updates.swatid.com/api
```

## 📡 Endpoints Principales

### **1. Verificación de Actualizaciones**

#### **POST /check**
Verifica si hay actualizaciones disponibles para un dispositivo específico.

**Request:**
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

**Response (Actualización Disponible):**
```json
{
  "available": true,
  "version": "v2.6.0",
  "download_url": "https://updates.swatid.com/firmware/KC868A2-v2.6.0.bin",
  "checksum": "sha256:abc123def456789...",
  "size": 1048576,
  "release_date": "2025-06-15",
  "changelog": [
    "Nuevas características OTA",
    "Mejoras de seguridad",
    "Corrección de bugs"
  ],
  "min_version": "v2.0.0",
  "max_version": "v3.0.0",
  "force_update": false
}
```

**Response (Sin Actualizaciones):**
```json
{
  "available": false,
  "message": "Firmware actualizado"
}
```

**Response (Error):**
```json
{
  "error": true,
  "code": "INVALID_DEVICE",
  "message": "Dispositivo no reconocido"
}
```

### **2. Descarga de Firmware**

#### **GET /firmware/{filename}**
Descarga el archivo de firmware especificado.

**Request:**
```http
GET /api/firmware/KC868A2-v2.6.0.bin
```

**Response:**
```http
HTTP/1.1 200 OK
Content-Type: application/octet-stream
Content-Length: 1048576
Content-Disposition: attachment; filename="KC868A2-v2.6.0.bin"
X-Checksum: sha256:abc123def456789...

[binary data]
```

### **3. Información de Versiones**

#### **GET /versions**
Obtiene información sobre todas las versiones disponibles.

**Request:**
```http
GET /api/versions
```

**Response:**
```json
{
  "device_models": {
    "KC868-A2": {
      "current_version": "v2.6.0",
      "versions": [
        {
          "version": "v2.6.0",
          "release_date": "2025-06-15",
          "size": 1048576,
          "changelog": [
            "Nuevas características OTA",
            "Mejoras de seguridad"
          ],
          "download_url": "https://updates.swatid.com/firmware/KC868A2-v2.6.0.bin",
          "checksum": "sha256:abc123def456789..."
        },
        {
          "version": "v2.5.0",
          "release_date": "2025-06-01",
          "size": 1024000,
          "changelog": [
            "Sistema OTA implementado",
            "Corrección de persistencia EEPROM"
          ],
          "download_url": "https://updates.swatid.com/firmware/KC868A2-v2.5.0.bin",
          "checksum": "sha256:def456abc789..."
        }
      ]
    }
  }
}
```

## 🔧 Endpoints de Administración

### **4. Configuración de Dispositivos**

#### **GET /devices**
Obtiene lista de dispositivos registrados.

**Request:**
```http
GET /api/devices
Authorization: Bearer {admin_token}
```

**Response:**
```json
{
  "devices": [
    {
      "mac": "AA:BB:CC:DD:EE:FF",
      "serial": "KC868A2-001",
      "model": "KC868-A2",
      "current_version": "v2.5.0",
      "last_check": "2025-06-15T10:30:00Z",
      "status": "active",
      "auto_update_enabled": true,
      "update_url": "https://updates.swatid.com/api/check"
    }
  ]
}
```

#### **POST /devices**
Registra un nuevo dispositivo.

**Request:**
```http
POST /api/devices
Authorization: Bearer {admin_token}
Content-Type: application/json

{
  "mac": "AA:BB:CC:DD:EE:FF",
  "serial": "KC868A2-001",
  "model": "KC868-A2",
  "version": "v2.5.0"
}
```

**Response:**
```json
{
  "success": true,
  "device_id": "dev_123456",
  "message": "Dispositivo registrado correctamente"
}
```

#### **PUT /devices/{device_id}**
Actualiza configuración de un dispositivo.

**Request:**
```http
PUT /api/devices/dev_123456
Authorization: Bearer {admin_token}
Content-Type: application/json

{
  "auto_update_enabled": true,
  "update_url": "https://updates.swatid.com/api/check",
  "check_interval": 24
}
```

**Response:**
```json
{
  "success": true,
  "message": "Configuración actualizada"
}
```

### **5. Gestión de Firmware**

#### **POST /firmware/upload**
Sube un nuevo archivo de firmware.

**Request:**
```http
POST /api/firmware/upload
Authorization: Bearer {admin_token}
Content-Type: multipart/form-data

file: KC868A2-v2.6.0.bin
version: v2.6.0
model: KC868-A2
changelog: ["Nuevas características", "Mejoras de seguridad"]
```

**Response:**
```json
{
  "success": true,
  "firmware_id": "fw_123456",
  "filename": "KC868A2-v2.6.0.bin",
  "size": 1048576,
  "checksum": "sha256:abc123def456789...",
  "download_url": "https://updates.swatid.com/firmware/KC868A2-v2.6.0.bin"
}
```

#### **GET /firmware**
Lista todos los archivos de firmware disponibles.

**Request:**
```http
GET /api/firmware
Authorization: Bearer {admin_token}
```

**Response:**
```json
{
  "firmware": [
    {
      "id": "fw_123456",
      "version": "v2.6.0",
      "model": "KC868-A2",
      "filename": "KC868A2-v2.6.0.bin",
      "size": 1048576,
      "checksum": "sha256:abc123def456789...",
      "upload_date": "2025-06-15T10:30:00Z",
      "download_url": "https://updates.swatid.com/firmware/KC868A2-v2.6.0.bin"
    }
  ]
}
```

#### **DELETE /firmware/{firmware_id}**
Elimina un archivo de firmware.

**Request:**
```http
DELETE /api/firmware/fw_123456
Authorization: Bearer {admin_token}
```

**Response:**
```json
{
  "success": true,
  "message": "Firmware eliminado correctamente"
}
```

### **6. Estadísticas y Logs**

#### **GET /stats**
Obtiene estadísticas del servidor.

**Request:**
```http
GET /api/stats
Authorization: Bearer {admin_token}
```

**Response:**
```json
{
  "total_devices": 150,
  "active_devices": 145,
  "total_updates": 1250,
  "successful_updates": 1200,
  "failed_updates": 50,
  "firmware_versions": {
    "v2.6.0": 80,
    "v2.5.0": 65
  },
  "last_24h_updates": 25
}
```

#### **GET /logs**
Obtiene logs de actualizaciones.

**Request:**
```http
GET /api/logs?device_id=dev_123456&limit=50
Authorization: Bearer {admin_token}
```

**Response:**
```json
{
  "logs": [
    {
      "timestamp": "2025-06-15T10:30:00Z",
      "device_id": "dev_123456",
      "mac": "AA:BB:CC:DD:EE:FF",
      "action": "update_check",
      "result": "success",
      "details": "No updates available"
    },
    {
      "timestamp": "2025-06-15T09:15:00Z",
      "device_id": "dev_123456",
      "mac": "AA:BB:CC:DD:EE:FF",
      "action": "firmware_download",
      "result": "success",
      "details": "Downloaded v2.6.0"
    }
  ]
}
```

## 🔐 Autenticación

### **Métodos de Autenticación**

#### **1. Bearer Token**
```http
Authorization: Bearer {admin_token}
```

#### **2. API Key**
```http
X-API-Key: {api_key}
```

#### **3. Basic Auth**
```http
Authorization: Basic {base64_encoded_credentials}
```

### **Generación de Tokens**

#### **POST /auth/login**
Autentica y obtiene token de acceso.

**Request:**
```http
POST /api/auth/login
Content-Type: application/json

{
  "username": "admin",
  "password": "password123"
}
```

**Response:**
```json
{
  "success": true,
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_in": 3600,
  "token_type": "Bearer"
}
```

#### **POST /auth/refresh**
Renueva token de acceso.

**Request:**
```http
POST /api/auth/refresh
Authorization: Bearer {refresh_token}
```

**Response:**
```json
{
  "success": true,
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_in": 3600
}
```

## 📊 Códigos de Error

### **Códigos HTTP**
- `200` - OK
- `201` - Created
- `400` - Bad Request
- `401` - Unauthorized
- `403` - Forbidden
- `404` - Not Found
- `409` - Conflict
- `422` - Unprocessable Entity
- `500` - Internal Server Error

### **Códigos de Error Personalizados**
```json
{
  "error": true,
  "code": "INVALID_DEVICE",
  "message": "Dispositivo no reconocido",
  "details": "MAC address not found in database"
}
```

**Códigos disponibles:**
- `INVALID_DEVICE` - Dispositivo no reconocido
- `INVALID_VERSION` - Versión no válida
- `UPDATE_NOT_AVAILABLE` - No hay actualizaciones disponibles
- `FILE_NOT_FOUND` - Archivo de firmware no encontrado
- `CHECKSUM_MISMATCH` - Checksum no coincide
- `SIZE_EXCEEDED` - Tamaño de archivo excedido
- `UNAUTHORIZED` - No autorizado
- `RATE_LIMITED` - Límite de velocidad excedido

## 🔄 Rate Limiting

### **Límites por Endpoint**
- `/check` - 10 requests/minuto por dispositivo
- `/firmware/*` - 5 requests/minuto por dispositivo
- `/devices/*` - 100 requests/minuto por admin
- `/firmware/upload` - 10 requests/minuto por admin

### **Headers de Rate Limiting**
```http
X-RateLimit-Limit: 10
X-RateLimit-Remaining: 7
X-RateLimit-Reset: 1640995200
```

## 📋 Ejemplos de Uso

### **Verificación de Actualizaciones**
```bash
curl -X POST https://updates.swatid.com/api/check \
  -H "Content-Type: application/json" \
  -d '{
    "mac": "AA:BB:CC:DD:EE:FF",
    "version": "v2.5.0",
    "model": "KC868-A2",
    "serial": "KC868A2-001"
  }'
```

### **Descarga de Firmware**
```bash
curl -O https://updates.swatid.com/api/firmware/KC868A2-v2.6.0.bin
```

### **Subida de Firmware (Admin)**
```bash
curl -X POST https://updates.swatid.com/api/firmware/upload \
  -H "Authorization: Bearer {admin_token}" \
  -F "file=@KC868A2-v2.6.0.bin" \
  -F "version=v2.6.0" \
  -F "model=KC868-A2" \
  -F "changelog=Nuevas características"
```

### **Configuración de Dispositivo**
```bash
curl -X PUT https://updates.swatid.com/api/devices/dev_123456 \
  -H "Authorization: Bearer {admin_token}" \
  -H "Content-Type: application/json" \
  -d '{
    "auto_update_enabled": true,
    "check_interval": 24
  }'
```

---

**Estado:** ✅ **DOCUMENTACIÓN COMPLETADA**  
**Versión:** v2.5.0  
**Fecha:** $(date)  
**Próximo Paso:** Implementación de configuración OTA en el dispositivo
