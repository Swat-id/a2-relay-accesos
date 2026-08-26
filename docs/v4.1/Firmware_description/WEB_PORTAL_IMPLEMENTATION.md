# Implementación del Portal Web - SWATID-A2 v4.1

> **Versión Firmware:** v4.1.0  
> **Última actualización:** Marzo 2026  
> **Framework:** ESP32 WebServer  
> **Puerto:** 80 (HTTP)

---

## Índice

1. [Arquitectura del Servidor Web](#arquitectura-del-servidor-web)
2. [Sistema de Autenticación](#sistema-de-autenticación)
3. [Estructura de Páginas HTML](#estructura-de-páginas-html)
4. [Gestión de Códigos (Tabla)](#gestión-de-códigos-tabla)
5. [Gestión de Usuarios BLE (Tabla)](#gestión-de-usuarios-ble-tabla)
6. [Formularios y Validación](#formularios-y-validación)
7. [APIs JSON](#apis-json)
8. [Paginación y Búsqueda](#paginación-y-búsqueda)
9. [Patrones Replicables](#patrones-replicables)
10. [Recomendaciones de Implementación](#recomendaciones-de-implementación)

---

## Arquitectura del Servidor Web

### Inicialización

```cpp
#include <WebServer.h>

WebServer server(80);  // Puerto HTTP estándar

void setup() {
  // Configurar rutas
  setupWebServer();
  
  // Iniciar servidor
  server.begin();
  Serial.println("🌐 Servidor web iniciado en puerto 80");
}

void loop() {
  // Procesar peticiones HTTP (NO BLOQUEANTE)
  server.handleClient();
  
  // ... otras tareas
}
```

### Registro de Rutas

```cpp
void setupWebServer() {
  // =================== RUTAS PRINCIPALES ===================
  server.on("/", handleRoot);                        // Página principal
  server.on("/save", HTTP_POST, handleSave);         // Guardar configuración
  server.on("/rele", handleRele);                    // Control de relés
  server.on("/reboot", handleReboot);                // Reiniciar dispositivo
  server.on("/reset", handleReset);                  // Restaurar defaults
  server.on("/changepass", HTTP_POST, handleChangePass);  // Cambiar contraseña

  // =================== GESTIÓN DE CÓDIGOS ===================
  server.on("/codes", handleCodes);                  // Página de códigos
  server.on("/codes/add", HTTP_POST, handleCodesAdd);  // Añadir código
  server.on("/codes/delete", HTTP_GET, handleCodesDelete);  // Eliminar código
  server.on("/codes/bulk-import", HTTP_POST, handleBulkImport, handleBulkImport);
  server.on("/codes/template", HTTP_GET, handleCSVTemplate);
  
  // =================== LECTURA DE TAGS ===================
  server.on("/codes/start-tag-reading", HTTP_POST, handleStartTagReading);
  server.on("/codes/stop-tag-reading", HTTP_POST, handleStopTagReading);
  server.on("/codes/read-tags-status", HTTP_GET, handleReadTagsStatus);
  server.on("/codes/export-read-tags", HTTP_GET, handleExportReadTags);
  server.on("/codes/load-read-tags", HTTP_POST, handleLoadReadTags);
  
  // =================== CÓDIGOS REMOTOS ===================
  server.on("/remote-codes", handleRemoteCodes);
  server.on("/remote-codes/add", HTTP_POST, handleRemoteCodesAdd);
  server.on("/remote-codes/delete", HTTP_GET, handleRemoteCodesDelete);
  server.on("/remote-codes/delete-all", HTTP_GET, handleRemoteCodesDeleteAll);
  
  // =================== EXPORTACIÓN ===================
  server.on("/export/codes", HTTP_GET, handleExportCodes);
  server.on("/export/remote-codes", HTTP_GET, handleExportRemoteCodes);
  server.on("/import/codes", HTTP_POST, handleImportCodes);
  
  // =================== MODO TORNO ===================
  server.on("/turnstile/config", HTTP_POST, handleTurnstileConfig);
  server.on("/turnstile/reset", HTTP_GET, handleTurnstileReset);
  
  // =================== SEGURIDAD ===================
  server.on("/security/block-access", HTTP_GET, handleSecurityBlockAccess);
  server.on("/security/unblock-access", HTTP_GET, handleSecurityUnblockAccess);
  server.on("/security/disable-keyboards", HTTP_GET, handleSecurityDisableKeyboards);
  server.on("/security/enable-keyboards", HTTP_GET, handleSecurityEnableKeyboards);
  
  // =================== ENTRADAS DIGITALES ===================
  server.on("/digital_inputs", HTTP_GET, handleDigitalInputs);
  server.on("/api/digital_inputs_status", HTTP_GET, handleDigitalInputsStatus);
  server.on("/api/digital_inputs_config", HTTP_GET, handleDigitalInputsConfig);
  server.on("/save_digital_input", HTTP_POST, handleSaveDigitalInput);
  
  // =================== SINCRONIZACIÓN DE HORA ===================
  server.on("/time/sync", HTTP_GET, handleTimeSync);
  server.on("/time/update", HTTP_POST, handleTimeUpdate);
  
  // =================== OTA ===================
  server.on("/ota", HTTP_GET, handleOTAPage);
  server.on("/ota/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "OK");
  }, handleOTAUpload);
  server.on("/ota/config", HTTP_POST, handleOTAConfig);
  server.on("/ota/check", HTTP_GET, handleOTACheck);
  server.on("/ota/status", HTTP_GET, handleOTAStatus);
  
  // =================== GESTIÓN BLE ===================
  #ifdef ENABLE_BLE
  server.on("/ble", HTTP_GET, handleBLEPage);
  server.on("/ble/clear-all", HTTP_GET, handleBLEClearAll);
  server.on("/ble/clear-superadmin", HTTP_GET, handleBLEClearSuperadmin);
  server.on("/ble/clear-user", HTTP_GET, handleBLEClearUser);
  server.on("/api/ble/status", HTTP_GET, handleBLEStatus);
  #endif
  
  Serial.println("🌐 Rutas web configuradas");
}
```

---

## Sistema de Autenticación

### Credenciales

```cpp
// Usuario fijo (no modificable)
const char* admin_user = "admin";

// Contraseña modificable (almacenada en EEPROM)
char admin_password[32] = "admin";  // Valor por defecto
```

### Patrón de Protección de Endpoints

Cada handler debe verificar autenticación como primera acción:

```cpp
void handleProtectedEndpoint() {
  // 1. VERIFICAR AUTENTICACIÓN (obligatorio)
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();  // Solicitar login
  }
  
  // 2. Procesar la petición solo si está autenticado
  // ... lógica del endpoint
  
  // 3. Enviar respuesta
  server.send(200, "text/html", html);
}
```

### Cambio de Contraseña

```cpp
void handleChangePass() {
  // Verificar autenticación actual
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // Verificar que se envió el parámetro
  if (server.hasArg("newPassword")) {
    String newPass = server.arg("newPassword");
    
    // Validar longitud (mínimo 4, máximo 31 caracteres)
    if (newPass.length() >= 4 && newPass.length() < 32) {
      // Copiar a variable global
      newPass.toCharArray(admin_password, sizeof(admin_password));
      
      // Persistir en EEPROM
      saveConfiguration();
      
      // Responder con éxito
      server.send(200, "text/html", 
        "<html><body>"
        "<h1>✅ Contraseña actualizada</h1>"
        "<p>La nueva contraseña ha sido guardada.</p>"
        "<a href='/'>Volver al inicio</a>"
        "</body></html>");
      return;
    }
  }
  
  // Error de validación
  server.send(400, "text/html", 
    "<html><body>"
    "<h1>❌ Error</h1>"
    "<p>La contraseña debe tener entre 4 y 31 caracteres.</p>"
    "<a href='/'>Volver</a>"
    "</body></html>");
}
```

### Persistencia de Contraseña

```cpp
// En DeviceConfig (EEPROM)
struct DeviceConfig {
  // ... otros campos
  char webPassword[32];
  // ...
};

void saveConfiguration() {
  // Copiar contraseña actual a config
  strncpy(config.webPassword, admin_password, sizeof(config.webPassword));
  config.configValid = 0xABCD1234;
  
  EEPROM.put(EEPROM_CONFIG_OFFSET, config);
  EEPROM.commit();
}

void loadConfiguration() {
  EEPROM.get(EEPROM_CONFIG_OFFSET, config);
  
  if (config.configValid == 0xABCD1234) {
    // Restaurar contraseña guardada
    strncpy(admin_password, config.webPassword, sizeof(admin_password));
  } else {
    // Primera ejecución - usar default
    strncpy(admin_password, "admin", sizeof(admin_password));
  }
}
```

---

## Estructura de Páginas HTML

### Patrón de Generación HTML

```cpp
void handleExamplePage() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // ===== CABECERA HTML =====
  String html = "<!DOCTYPE html><html lang='es'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Página - SWATID</title>";
  
  // Font Awesome para iconos
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css'>";
  
  // ===== ESTILOS CSS =====
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:0;padding:0;background:#f4f4f4;}";
  html += "header{background:#35424a;color:#fff;padding:20px 0;text-align:center;}";
  html += "main{padding:20px;}";
  html += ".container{max-width:1000px;margin:auto;background:#fff;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);}";
  
  // Estilos de tablas
  html += "table{width:100%;border-collapse:collapse;margin-top:20px;}";
  html += "th,td{border:1px solid #ddd;padding:10px;text-align:center;}";
  html += "th{background-color:#f2f2f2;}";
  
  // Estilos de botones
  html += "button{padding:10px 15px;border:none;border-radius:4px;cursor:pointer;font-size:14px;margin:5px;}";
  html += ".btn-primary{background:#007bff;color:white;}";
  html += ".btn-success{background:#28a745;color:white;}";
  html += ".btn-danger{background:#dc3545;color:white;}";
  html += ".btn-warning{background:#ffc107;color:#333;}";
  html += "button:hover{opacity:0.8;}";
  
  // Estilos de formularios
  html += ".form-group{margin-bottom:15px;}";
  html += "label{display:block;margin-bottom:5px;font-weight:bold;}";
  html += "input,select{width:100%;padding:10px;border:1px solid #ccc;border-radius:4px;}";
  
  // Estilos de alertas
  html += ".alert{padding:15px;margin:15px 0;border-radius:5px;}";
  html += ".alert-info{background:#e7f3ff;border-left:4px solid #007bff;}";
  html += ".alert-success{background:#e8f5e8;border-left:4px solid #28a745;}";
  html += ".alert-warning{background:#fff3cd;border-left:4px solid #ffc107;}";
  html += ".alert-danger{background:#f8d7da;border-left:4px solid #dc3545;}";
  
  html += "</style>";
  html += "</head>";
  
  // ===== CABECERA =====
  html += "<body><header>";
  html += "<h1><i class='fas fa-cogs'></i> Título de la Página</h1>";
  html += "<p>Device: " + String(deviceName) + " | Serial: " + fixedSerialNumber + "</p>";
  html += "</header>";
  
  // ===== CONTENIDO PRINCIPAL =====
  html += "<main><div class='container'>";
  
  // ... contenido dinámico aquí ...
  
  html += "</div></main>";
  
  // ===== PIE DE PÁGINA =====
  html += "<footer style='background:#2c3e50;color:white;padding:20px;text-align:center;margin-top:30px;'>";
  html += "<h3>Smart World And Things SLU</h3>";
  html += "<p>Web: www.swat-id.com | Tel: 633 44 84 27</p>";
  html += "</footer>";
  
  html += "</body></html>";
  
  // ===== ENVIAR RESPUESTA =====
  server.send(200, "text/html", html);
}
```

### Uso de Raw String Literals

Para HTML extenso, usar raw string literals:

```cpp
void handlePageWithRawString() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <title>Página Ejemplo</title>
  <style>
    body { font-family: Arial, sans-serif; }
    .container { max-width: 800px; margin: auto; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>Contenido Estático</h1>
    <p>Este contenido es fijo.</p>
    <p>Variable dinámica: )=====";
  
  // Insertar contenido dinámico
  html += String(variableDinamica);
  
  // Continuar con más HTML estático
  html += R"=====(</p>
  </div>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}
```

---

## Gestión de Códigos (Tabla)

### Estructura de Datos

```cpp
#define MAX_CODES 50  // Máximo de códigos

struct CodeEntry {
  char type[4];        // "PIN" o "TAG"
  char value[17];      // Valor del código (máx 16 caracteres)
  uint8_t keyboard_id; // 0=Ambos, 1=Teclado1, 2=Teclado2
  uint8_t relay;       // 1 o 2
  uint8_t reserved[2]; // Para futuras extensiones
};

struct StoredCodes {
  uint32_t validMarker;       // 0xCAFEBABE si es válido
  uint32_t version;           // Versión de estructura
  bool localValidationFirst;  // Modo de validación
  uint16_t count;             // Número de códigos
  CodeEntry codes[MAX_CODES]; // Array de códigos
};

StoredCodes* storedCodes;
```

### Renderizado de Tabla de Códigos

```cpp
void renderCodesTable(String& html, const String& searchTerm, int page) {
  const int ITEMS_PER_PAGE = 20;
  int startIndex = (page - 1) * ITEMS_PER_PAGE;
  int endIndex = startIndex + ITEMS_PER_PAGE;
  
  // Cabecera de tabla
  html += "<table>";
  html += "<tr>";
  html += "<th>Tipo</th>";
  html += "<th>Valor</th>";
  html += "<th>Teclado</th>";
  html += "<th>Relé</th>";
  html += "<th>Acciones</th>";
  html += "</tr>";
  
  int filteredCount = 0;
  int displayedCount = 0;
  
  // Iterar códigos
  for (int i = 0; i < storedCodes->count; i++) {
    // Aplicar filtro de búsqueda
    bool matchesFilter = true;
    if (searchTerm.length() > 0) {
      String codeType = String(storedCodes->codes[i].type);
      String codeValue = String(storedCodes->codes[i].value);
      
      codeType.toLowerCase();
      codeValue.toLowerCase();
      String searchLower = searchTerm;
      searchLower.toLowerCase();
      
      matchesFilter = (codeType.indexOf(searchLower) >= 0 || 
                       codeValue.indexOf(searchLower) >= 0);
    }
    
    if (matchesFilter) {
      filteredCount++;
      
      // Aplicar paginación
      if (filteredCount > startIndex && filteredCount <= endIndex) {
        html += "<tr>";
        
        // Columna Tipo
        html += "<td>" + String(storedCodes->codes[i].type) + "</td>";
        
        // Columna Valor
        html += "<td><code>" + String(storedCodes->codes[i].value) + "</code></td>";
        
        // Columna Teclado
        String keyboardName;
        switch (storedCodes->codes[i].keyboard_id) {
          case 0: keyboardName = "🔑 Ambos"; break;
          case 1: keyboardName = "🔑 Teclado 1"; break;
          case 2: keyboardName = "🔑 Teclado 2"; break;
          default: keyboardName = "❓ Desconocido";
        }
        html += "<td>" + keyboardName + "</td>";
        
        // Columna Relé
        html += "<td>⚡ Relé " + String(storedCodes->codes[i].relay) + "</td>";
        
        // Columna Acciones
        html += "<td>";
        html += "<a href='/codes/delete?type=" + String(storedCodes->codes[i].type);
        html += "&value=" + String(storedCodes->codes[i].value);
        html += "&keyboard=" + String(storedCodes->codes[i].keyboard_id);
        html += "' onclick='return confirm(\"¿Eliminar este código?\");'>";
        html += "<button class='btn-danger'><i class='fas fa-trash'></i></button>";
        html += "</a>";
        html += "</td>";
        
        html += "</tr>";
        displayedCount++;
      }
    }
  }
  
  // Mensaje si no hay resultados
  if (displayedCount == 0) {
    html += "<tr><td colspan='5' style='text-align:center;color:#999;'>";
    if (searchTerm.length() > 0) {
      html += "No se encontraron códigos que coincidan con '" + searchTerm + "'";
    } else {
      html += "No hay códigos almacenados";
    }
    html += "</td></tr>";
  }
  
  html += "</table>";
  
  // Renderizar paginación
  renderPagination(html, page, filteredCount, ITEMS_PER_PAGE, "/codes", searchTerm);
}
```

### Formulario de Añadir Código

```cpp
void renderAddCodeForm(String& html) {
  html += "<form action='/codes/add' method='post'>";
  html += "<h3><i class='fas fa-plus-circle'></i> Añadir nuevo código</h3>";
  
  // Campo Tipo
  html += "<div class='form-group'>";
  html += "<label for='type'>Tipo:</label>";
  html += "<select name='type' id='type' required>";
  html += "<option value='PIN'>PIN (4-6 dígitos)</option>";
  html += "<option value='TAG'>TAG (tarjeta RFID/NFC)</option>";
  html += "</select>";
  html += "</div>";
  
  // Campo Valor
  html += "<div class='form-group'>";
  html += "<label for='value'>Código:</label>";
  html += "<input type='text' name='value' id='value' maxlength='16' ";
  html += "placeholder='Ej: 1234 o código de tarjeta' required>";
  html += "</div>";
  
  // Campo Teclado
  html += "<div class='form-group'>";
  html += "<label for='keyboard_id'>Teclado autorizado:</label>";
  html += "<select name='keyboard_id' id='keyboard_id'>";
  html += "<option value='0'>Ambos teclados</option>";
  html += "<option value='1'>Teclado 1 (GPIO 33/14)</option>";
  html += "<option value='2'>Teclado 2 (GPIO 4/16)</option>";
  html += "</select>";
  html += "</div>";
  
  // Campo Relé
  html += "<div class='form-group'>";
  html += "<label for='relay'>Relé a activar:</label>";
  html += "<select name='relay' id='relay'>";
  html += "<option value='1'>Relé 1</option>";
  html += "<option value='2'>Relé 2</option>";
  html += "</select>";
  html += "</div>";
  
  html += "<button type='submit' class='btn-success'>";
  html += "<i class='fas fa-plus'></i> Añadir Código</button>";
  html += "</form>";
}
```

### Handler de Añadir Código

```cpp
void handleCodesAdd() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  // Verificar parámetros requeridos
  if (server.hasArg("type") && server.hasArg("value") && 
      server.hasArg("relay") && server.hasArg("keyboard_id")) {
    
    String type = server.arg("type");
    String value = server.arg("value");
    int relay = server.arg("relay").toInt();
    int keyboardId = server.arg("keyboard_id").toInt();
    
    // Limpiar espacios
    value.trim();
    
    // Validaciones
    if (value.length() == 0) {
      server.send(400, "text/html", errorPage("El código no puede estar vacío"));
      return;
    }
    
    if (storedCodes->count >= MAX_CODES) {
      server.send(400, "text/html", errorPage("Memoria llena (máximo " + String(MAX_CODES) + " códigos)"));
      return;
    }
    
    if (relay < 1 || relay > 2) {
      server.send(400, "text/html", errorPage("Relé debe ser 1 o 2"));
      return;
    }
    
    if (keyboardId < 0 || keyboardId > 2) {
      server.send(400, "text/html", errorPage("Teclado inválido"));
      return;
    }
    
    // Verificar duplicados
    for (int i = 0; i < storedCodes->count; i++) {
      if (strcmp(storedCodes->codes[i].value, value.c_str()) == 0 &&
          strcmp(storedCodes->codes[i].type, type.c_str()) == 0) {
        server.send(400, "text/html", errorPage("El código ya existe"));
        return;
      }
    }
    
    // Añadir código
    int idx = storedCodes->count;
    strncpy(storedCodes->codes[idx].type, type.c_str(), 3);
    storedCodes->codes[idx].type[3] = '\0';
    strncpy(storedCodes->codes[idx].value, value.c_str(), 16);
    storedCodes->codes[idx].value[16] = '\0';
    storedCodes->codes[idx].keyboard_id = keyboardId;
    storedCodes->codes[idx].relay = relay;
    
    storedCodes->count++;
    
    // Guardar en EEPROM
    saveStoredCodes();
    
    Serial.printf("✅ Código añadido: %s %s -> Relé %d (Teclado %d)\n",
                  type.c_str(), value.c_str(), relay, keyboardId);
    
    // Redirigir a lista de códigos
    server.sendHeader("Location", "/codes");
    server.send(303);
    
  } else {
    server.send(400, "text/html", errorPage("Faltan parámetros requeridos"));
  }
}

// Función auxiliar para páginas de error
String errorPage(const String& message) {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<title>Error</title></head><body style='font-family:Arial;padding:20px;'>";
  html += "<h1 style='color:#dc3545;'>❌ Error</h1>";
  html += "<p>" + message + "</p>";
  html += "<a href='/codes'><button>Volver a códigos</button></a>";
  html += "</body></html>";
  return html;
}
```

### Handler de Eliminar Código

```cpp
void handleCodesDelete() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  if (server.hasArg("type") && server.hasArg("value")) {
    String type = server.arg("type");
    String value = server.arg("value");
    
    // Buscar y eliminar código
    bool found = false;
    for (int i = 0; i < storedCodes->count; i++) {
      if (strcmp(storedCodes->codes[i].type, type.c_str()) == 0 &&
          strcmp(storedCodes->codes[i].value, value.c_str()) == 0) {
        
        // Mover códigos para llenar el hueco
        for (int j = i; j < storedCodes->count - 1; j++) {
          storedCodes->codes[j] = storedCodes->codes[j + 1];
        }
        
        storedCodes->count--;
        found = true;
        
        // Guardar cambios
        saveStoredCodes();
        
        Serial.printf("🗑️ Código eliminado: %s %s\n", type.c_str(), value.c_str());
        break;
      }
    }
    
    // Redirigir
    server.sendHeader("Location", "/codes");
    server.send(303);
    
  } else {
    server.send(400, "text/html", errorPage("Faltan parámetros"));
  }
}
```

---

## Gestión de Usuarios BLE (Tabla)

### Estructura de Datos

```cpp
#define BLE_MAX_USERS 5

// Permisos BLE
#define BLE_PERM_RELAY_CONTROL    0x01
#define BLE_PERM_MODE_CHANGE      0x02
#define BLE_PERM_ADD_CODES        0x04
#define BLE_PERM_NETWORK_CONFIG   0x08
#define BLE_PERM_USER_MANAGE      0x10
#define BLE_PERM_ALL              0xFF

struct BLEAuthConfig {
  uint32_t validMarker;                    // 0xB1E4C0DE
  bool superadmin_registered;
  uint8_t superadmin_key[64];
  bool user_enabled[BLE_MAX_USERS];
  char user_names[BLE_MAX_USERS][32];
  uint8_t user_keys[BLE_MAX_USERS][64];
  uint8_t user_permissions[BLE_MAX_USERS];
  uint32_t checksum;
};

BLEAuthConfig bleAuthConfig;
```

### Renderizado de Tabla de Usuarios BLE

```cpp
void renderBLEUsersTable(String& html) {
  html += "<h2><i class='fas fa-users'></i> Usuarios Vinculados</h2>";
  html += "<table>";
  
  // Cabecera
  html += "<tr>";
  html += "<th>#</th>";
  html += "<th>Nombre</th>";
  html += "<th>Estado</th>";
  html += "<th>Permisos</th>";
  html += "<th>Acciones</th>";
  html += "</tr>";
  
  // Filas de usuarios
  for (int i = 0; i < BLE_MAX_USERS; i++) {
    html += "<tr style='";
    html += bleAuthConfig.user_enabled[i] ? "background:#f0fff0;" : "opacity:0.6;";
    html += "'>";
    
    // Número de slot
    html += "<td>" + String(i + 1) + "</td>";
    
    // Nombre
    String userName = (bleAuthConfig.user_names[i][0] != '\0') ? 
                      String(bleAuthConfig.user_names[i]) : "-";
    html += "<td>" + userName + "</td>";
    
    // Estado
    html += "<td>";
    if (bleAuthConfig.user_enabled[i]) {
      html += "<span style='background:#28a745;color:white;padding:3px 8px;border-radius:3px;'>Activo</span>";
    } else {
      html += "<span style='background:#dc3545;color:white;padding:3px 8px;border-radius:3px;'>Inactivo</span>";
    }
    html += "</td>";
    
    // Permisos
    html += "<td style='font-size:12px;'>";
    if (bleAuthConfig.user_enabled[i]) {
      uint8_t perm = bleAuthConfig.user_permissions[i];
      if (perm & BLE_PERM_RELAY_CONTROL) html += "🔌 Relés ";
      if (perm & BLE_PERM_MODE_CHANGE)   html += "⚙️ Modo ";
      if (perm & BLE_PERM_ADD_CODES)     html += "🔑 Códigos ";
      if (perm & BLE_PERM_NETWORK_CONFIG) html += "🌐 Red ";
      if (perm & BLE_PERM_USER_MANAGE)   html += "👤 Usuarios ";
      
      // Preview de clave
      char keyPreview[16];
      sprintf(keyPreview, "[%02X%02X..]", 
              bleAuthConfig.user_keys[i][0], 
              bleAuthConfig.user_keys[i][1]);
      html += "<code style='font-size:10px;'>" + String(keyPreview) + "</code>";
    } else {
      html += "-";
    }
    html += "</td>";
    
    // Acciones
    html += "<td>";
    if (bleAuthConfig.user_enabled[i]) {
      html += "<a href='/ble/clear-user?slot=" + String(i + 1) + "' ";
      html += "onclick='return confirm(\"¿Eliminar usuario " + String(i + 1) + "?\");'>";
      html += "<button class='btn-danger'><i class='fas fa-user-minus'></i></button>";
      html += "</a>";
    }
    html += "</td>";
    
    html += "</tr>";
  }
  
  html += "</table>";
}
```

### Sección de Superadmin

```cpp
void renderSuperadminSection(String& html) {
  html += "<h2><i class='fas fa-user-shield'></i> Superadmin</h2>";
  html += "<div style='border:2px solid #dc3545;border-radius:8px;padding:15px;background:#fff5f5;'>";
  
  html += "<h3><i class='fas fa-crown' style='color:gold;'></i> Superadministrador</h3>";
  
  // Estado
  html += "<p><strong>Estado:</strong> ";
  if (bleAuthConfig.superadmin_registered) {
    html += "<span style='background:#28a745;color:white;padding:3px 8px;border-radius:3px;'>Registrado</span>";
  } else {
    html += "<span style='background:#ffc107;color:#333;padding:3px 8px;border-radius:3px;'>No registrado</span>";
  }
  html += "</p>";
  
  // Preview de clave si existe
  if (bleAuthConfig.superadmin_registered) {
    char keyPreview[32];
    sprintf(keyPreview, "%02X%02X%02X%02X...", 
            bleAuthConfig.superadmin_key[0], bleAuthConfig.superadmin_key[1],
            bleAuthConfig.superadmin_key[2], bleAuthConfig.superadmin_key[3]);
    html += "<p><strong>Key preview:</strong> <code>" + String(keyPreview) + "</code></p>";
  }
  
  html += "<p style='font-size:12px;'><i class='fas fa-key'></i> Permisos: Todos (0xFF)</p>";
  html += "<p><small>El superadmin se registra cuando el primer dispositivo envía una clave de 64 bytes.</small></p>";
  
  // Botón eliminar
  html += "<button class='btn-danger' onclick='confirmClearSuperadmin()'>";
  html += "<i class='fas fa-trash'></i> Eliminar Superadmin</button>";
  
  html += "</div>";
}
```

### Handler de Eliminar Usuario BLE

```cpp
void handleBLEClearUser() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  if (server.hasArg("slot")) {
    int slot = server.arg("slot").toInt();
    
    // Validar slot (1-5)
    if (slot < 1 || slot > BLE_MAX_USERS) {
      server.send(400, "text/html", errorPage("Slot inválido"));
      return;
    }
    
    int idx = slot - 1;  // Convertir a índice 0-based
    
    // Verificar que hay usuario
    if (!bleAuthConfig.user_enabled[idx]) {
      server.send(400, "text/html", errorPage("No hay usuario en ese slot"));
      return;
    }
    
    // Limpiar usuario
    bleAuthConfig.user_enabled[idx] = false;
    memset(bleAuthConfig.user_names[idx], 0, 32);
    memset(bleAuthConfig.user_keys[idx], 0, 64);
    bleAuthConfig.user_permissions[idx] = 0;
    
    // Recalcular checksum y guardar
    bleAuthConfig.checksum = calculateBLEChecksum(bleAuthConfig);
    saveBLEAuthConfig();
    
    Serial.printf("🔵 [BLE] Usuario %d eliminado desde web\n", slot);
    
    // Redirigir
    server.sendHeader("Location", "/ble");
    server.send(303);
    
  } else {
    server.send(400, "text/html", errorPage("Falta parámetro slot"));
  }
}
```

---

## Formularios y Validación

### Patrón de Formulario con Validación

```cpp
void renderConfigForm(String& html) {
  html += "<form action='/save' method='post' id='configForm'>";
  html += "<h2><i class='fas fa-cogs'></i> Configuración</h2>";
  
  // Campo texto
  html += "<div class='form-group'>";
  html += "<label for='deviceName'>Nombre del dispositivo:</label>";
  html += "<input type='text' id='deviceName' name='deviceName' ";
  html += "value='" + String(deviceName) + "' ";
  html += "minlength='3' maxlength='32' required>";
  html += "</div>";
  
  // Campo numérico con paso decimal
  html += "<div class='form-group'>";
  html += "<label for='releDuration'>Duración del relé (segundos):</label>";
  html += "<input type='number' id='releDuration' name='releDuration' ";
  html += "value='" + String(releDuration) + "' ";
  html += "min='0.5' max='60' step='0.5' required>";
  html += "</div>";
  
  // Select con valor dinámico
  html += "<div class='form-group'>";
  html += "<label for='useDhcp'>Usar DHCP:</label>";
  html += "<select id='useDhcp' name='useDhcp' onchange='toggleIpFields()'>";
  html += useDhcp ? 
    "<option value='1' selected>Sí</option><option value='0'>No</option>" :
    "<option value='1'>Sí</option><option value='0' selected>No</option>";
  html += "</select>";
  html += "</div>";
  
  // Campos condicionales
  html += "<div id='staticIpFields' style='display:" + String(useDhcp ? "none" : "block") + "'>";
  
  html += "<div class='form-group'>";
  html += "<label for='staticIp'>IP Estática:</label>";
  html += "<input type='text' id='staticIp' name='staticIp' ";
  html += "value='" + staticIP.toString() + "' ";
  html += "pattern='\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}'>";
  html += "</div>";
  
  // Más campos IP...
  
  html += "</div>";  // Fin campos condicionales
  
  html += "<button type='submit' class='btn-success'>";
  html += "<i class='fas fa-save'></i> Guardar configuración</button>";
  html += "</form>";
  
  // JavaScript para campos condicionales
  html += "<script>";
  html += "function toggleIpFields() {";
  html += "  var dhcp = document.getElementById('useDhcp').value;";
  html += "  document.getElementById('staticIpFields').style.display = ";
  html += "    (dhcp == '1') ? 'none' : 'block';";
  html += "}";
  html += "</script>";
}
```

### Handler de Guardar Configuración

```cpp
void handleSave() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // Procesar cada campo si existe
  if (server.hasArg("deviceName")) {
    String tempName = server.arg("deviceName");
    strncpy(deviceName, tempName.c_str(), sizeof(deviceName) - 1);
    deviceName[sizeof(deviceName) - 1] = '\0';  // Asegurar null terminator
  }
  
  if (server.hasArg("releDuration")) {
    releDuration = server.arg("releDuration").toFloat();
    // Validar rango
    if (releDuration < 0.5) releDuration = 0.5;
    if (releDuration > 60) releDuration = 60;
  }
  
  if (server.hasArg("useDhcp")) {
    useDhcp = (server.arg("useDhcp") == "1");
  }
  
  // Procesar IP estática solo si DHCP está deshabilitado
  if (!useDhcp) {
    if (server.hasArg("staticIp")) {
      staticIP.fromString(server.arg("staticIp"));
    }
    if (server.hasArg("staticGateway")) {
      staticGateway.fromString(server.arg("staticGateway"));
    }
    if (server.hasArg("staticSubnet")) {
      staticSubnet.fromString(server.arg("staticSubnet"));
    }
    if (server.hasArg("staticDns")) {
      staticDns.fromString(server.arg("staticDns"));
    }
  }
  
  // Guardar en EEPROM
  saveConfiguration();
  
  Serial.println("⚙️ Configuración guardada desde web");
  
  // Redirigir a página principal
  server.sendHeader("Location", "/");
  server.send(303);
}
```

---

## APIs JSON

### Patrón de API JSON

```cpp
void handleAPIEndpoint() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // Crear documento JSON
  DynamicJsonDocument doc(1024);
  
  doc["status"] = "ok";
  doc["timestamp"] = millis();
  
  // Añadir datos
  JsonObject data = doc.createNestedObject("data");
  data["relay1"] = digitalRead(RELAY1_PIN);
  data["relay2"] = digitalRead(RELAY2_PIN);
  data["codes_count"] = storedCodes->count;
  data["mqtt_connected"] = mqttClient.connected();
  
  // Array de ejemplo
  JsonArray items = doc.createNestedArray("items");
  items.add("item1");
  items.add("item2");
  
  // Serializar y enviar
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}
```

### API de Estado BLE

```cpp
void handleBLEStatus() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  DynamicJsonDocument doc(2048);
  
  doc["ble_enabled"] = true;
  doc["device_name"] = fixedSerialNumber;
  doc["connected"] = bleDeviceConnected;
  doc["authenticated"] = bleAuthenticated;
  doc["superadmin_registered"] = bleAuthConfig.superadmin_registered;
  
  // Contar usuarios activos
  int activeUsers = 0;
  for (int i = 0; i < BLE_MAX_USERS; i++) {
    if (bleAuthConfig.user_enabled[i]) activeUsers++;
  }
  doc["active_users"] = activeUsers;
  doc["max_users"] = BLE_MAX_USERS;
  
  // Lista de usuarios
  JsonArray users = doc.createNestedArray("users");
  for (int i = 0; i < BLE_MAX_USERS; i++) {
    JsonObject user = users.createNestedObject();
    user["slot"] = i + 1;
    user["name"] = bleAuthConfig.user_names[i];
    user["enabled"] = bleAuthConfig.user_enabled[i];
    user["permissions"] = bleAuthConfig.user_permissions[i];
  }
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}
```

### API de Estado de Entradas Digitales

```cpp
void handleDigitalInputsStatus() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  DynamicJsonDocument doc(512);
  
  // Leer estado actual de entradas
  bool input1 = digitalRead(DIGITAL_INPUT_1);
  bool input2 = digitalRead(DIGITAL_INPUT_2);
  
  doc["input1"]["state"] = input1;
  doc["input1"]["gpio"] = DIGITAL_INPUT_1;
  doc["input1"]["last_change"] = lastInput1Change;
  
  doc["input2"]["state"] = input2;
  doc["input2"]["gpio"] = DIGITAL_INPUT_2;
  doc["input2"]["last_change"] = lastInput2Change;
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}
```

---

## Paginación y Búsqueda

### Función de Renderizado de Paginación

```cpp
void renderPagination(String& html, int currentPage, int totalItems, 
                      int itemsPerPage, const String& baseUrl, 
                      const String& searchTerm) {
  
  int totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;
  
  if (totalPages <= 1) {
    // Sin paginación necesaria
    html += "<div style='margin:20px 0;text-align:center;'>";
    html += "<p>Mostrando " + String(totalItems) + " elementos</p>";
    html += "</div>";
    return;
  }
  
  html += "<div style='margin:20px 0;text-align:center;'>";
  
  // Info de paginación
  int startItem = (currentPage - 1) * itemsPerPage + 1;
  int endItem = min(currentPage * itemsPerPage, totalItems);
  html += "<p>Mostrando " + String(startItem) + "-" + String(endItem);
  html += " de " + String(totalItems) + " (Página " + String(currentPage);
  html += " de " + String(totalPages) + ")</p>";
  
  // Botón anterior
  if (currentPage > 1) {
    html += "<a href='" + baseUrl + "?page=" + String(currentPage - 1);
    if (searchTerm.length() > 0) {
      html += "&search=" + searchTerm;
    }
    html += "' style='margin:0 5px;padding:8px 12px;background:#007bff;color:white;";
    html += "text-decoration:none;border-radius:4px;'>";
    html += "<i class='fas fa-chevron-left'></i> Anterior</a>";
  }
  
  // Números de página
  int startPage = max(1, currentPage - 2);
  int endPage = min(totalPages, currentPage + 2);
  
  for (int p = startPage; p <= endPage; p++) {
    if (p == currentPage) {
      // Página actual (sin enlace)
      html += "<span style='margin:0 5px;padding:8px 12px;background:#6c757d;";
      html += "color:white;border-radius:4px;'>" + String(p) + "</span>";
    } else {
      // Enlace a otra página
      html += "<a href='" + baseUrl + "?page=" + String(p);
      if (searchTerm.length() > 0) {
        html += "&search=" + searchTerm;
      }
      html += "' style='margin:0 5px;padding:8px 12px;background:#007bff;color:white;";
      html += "text-decoration:none;border-radius:4px;'>" + String(p) + "</a>";
    }
  }
  
  // Botón siguiente
  if (currentPage < totalPages) {
    html += "<a href='" + baseUrl + "?page=" + String(currentPage + 1);
    if (searchTerm.length() > 0) {
      html += "&search=" + searchTerm;
    }
    html += "' style='margin:0 5px;padding:8px 12px;background:#007bff;color:white;";
    html += "text-decoration:none;border-radius:4px;'>";
    html += "Siguiente <i class='fas fa-chevron-right'></i></a>";
  }
  
  html += "</div>";
}
```

### Formulario de Búsqueda

```cpp
void renderSearchForm(String& html, const String& currentSearch, const String& baseUrl) {
  html += "<div style='background:#f8f9fa;padding:15px;border-radius:8px;margin:20px 0;'>";
  html += "<h3><i class='fas fa-search'></i> Buscar</h3>";
  
  html += "<form method='GET' action='" + baseUrl + "' ";
  html += "style='display:flex;gap:10px;align-items:center;flex-wrap:wrap;'>";
  
  html += "<input type='text' name='search' ";
  html += "placeholder='Buscar por código o tipo...' ";
  html += "value='" + currentSearch + "' ";
  html += "style='flex:1;min-width:200px;padding:8px;border:1px solid #ddd;border-radius:4px;'>";
  
  html += "<button type='submit' class='btn-primary'>";
  html += "<i class='fas fa-search'></i> Buscar</button>";
  
  html += "<a href='" + baseUrl + "' class='btn-secondary' style='text-decoration:none;'>";
  html += "<i class='fas fa-times'></i> Limpiar</a>";
  
  html += "</form></div>";
}
```

---

## Patrones Replicables

### 1. Handler Básico con Autenticación

```cpp
void handleMiEndpoint() {
  // 1. Autenticación obligatoria
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // 2. Lógica del endpoint
  // ...
  
  // 3. Respuesta
  server.send(200, "text/html", html);
}
```

### 2. Handler POST con Parámetros

```cpp
void handleMiEndpointPost() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // Verificar parámetros requeridos
  if (!server.hasArg("parametro_requerido")) {
    server.send(400, "text/html", errorPage("Falta parámetro"));
    return;
  }
  
  // Obtener y validar parámetros
  String valor = server.arg("parametro_requerido");
  valor.trim();
  
  if (valor.length() == 0) {
    server.send(400, "text/html", errorPage("Valor vacío"));
    return;
  }
  
  // Procesar
  // ...
  
  // Redirigir
  server.sendHeader("Location", "/destino");
  server.send(303);
}
```

### 3. Handler de Acción con Confirmación

```cpp
void handleAccionPeligrosa() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // Verificar confirmación
  if (!server.hasArg("confirm") || server.arg("confirm") != "yes") {
    // Mostrar página de confirmación
    String html = "<!DOCTYPE html><html><body>";
    html += "<h1>⚠️ Confirmar Acción</h1>";
    html += "<p>¿Está seguro de realizar esta acción?</p>";
    html += "<a href='/accion?confirm=yes'><button class='btn-danger'>Confirmar</button></a>";
    html += "<a href='/'><button class='btn-secondary'>Cancelar</button></a>";
    html += "</body></html>";
    server.send(200, "text/html", html);
    return;
  }
  
  // Ejecutar acción
  // ...
  
  // Redirigir
  server.sendHeader("Location", "/");
  server.send(303);
}
```

### 4. Handler de Exportación CSV

```cpp
void handleExportCSV() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // Generar CSV
  String csv = "Columna1,Columna2,Columna3\n";
  
  for (int i = 0; i < dataCount; i++) {
    csv += String(data[i].campo1) + ",";
    csv += String(data[i].campo2) + ",";
    csv += String(data[i].campo3) + "\n";
  }
  
  // Configurar headers para descarga
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", 
    "attachment; filename=export_" + String(millis()) + ".csv");
  
  server.send(200, "text/csv", csv);
}
```

### 5. Handler de Upload de Archivo

```cpp
// Registrar con dos handlers
server.on("/upload", HTTP_POST, 
  []() { server.send(200, "text/plain", "OK"); },  // Respuesta final
  handleFileUpload  // Procesar chunks
);

String uploadContent = "";

void handleFileUpload() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("📁 Iniciando upload: %s\n", upload.filename.c_str());
    uploadContent = "";
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Acumular contenido
    for (size_t i = 0; i < upload.currentSize; i++) {
      uploadContent += (char)upload.buf[i];
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("📁 Upload completo: %d bytes\n", upload.totalSize);
    
    // Procesar contenido
    processUploadedFile(uploadContent);
    
    uploadContent = "";  // Limpiar
  }
}
```

---

## Recomendaciones de Implementación

### 1. Seguridad

```cpp
// ✅ SIEMPRE verificar autenticación primero
void handleAnyEndpoint() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  // ...
}

// ✅ Validar todos los parámetros de entrada
if (server.hasArg("value")) {
  String value = server.arg("value");
  value.trim();
  if (value.length() == 0 || value.length() > MAX_LENGTH) {
    // Rechazar
  }
}

// ✅ Escapar contenido dinámico en HTML
html += "<p>" + htmlEscape(userInput) + "</p>";

String htmlEscape(const String& input) {
  String output = input;
  output.replace("&", "&amp;");
  output.replace("<", "&lt;");
  output.replace(">", "&gt;");
  output.replace("\"", "&quot;");
  return output;
}
```

### 2. Rendimiento

```cpp
// ✅ Usar raw string literals para HTML estático grande
String html = R"=====(
<!DOCTYPE html>
<html>
...gran cantidad de HTML estático...
</html>
)=====";

// ✅ Limitar tamaño de respuestas
const int MAX_ITEMS_PER_PAGE = 20;

// ✅ Usar paginación para listas largas
int page = server.arg("page").toInt();
if (page < 1) page = 1;

// ✅ No bloquear en handleClient()
void loop() {
  server.handleClient();  // Rápido, no bloqueante
  // ... otras tareas
}
```

### 3. Usabilidad

```cpp
// ✅ Usar redirecciones después de POST
server.sendHeader("Location", "/destino");
server.send(303);

// ✅ Mostrar mensajes de éxito/error claros
html += "<div class='alert alert-success'>✅ Operación completada</div>";

// ✅ Confirmar acciones destructivas
html += "onclick='return confirm(\"¿Está seguro?\");'";

// ✅ Mantener estado en formularios
html += "value='" + String(valorActual) + "'";
```

### 4. Mantenibilidad

```cpp
// ✅ Separar renderizado en funciones
void handlePage() {
  String html = "";
  renderHeader(html);
  renderContent(html);
  renderFooter(html);
  server.send(200, "text/html", html);
}

// ✅ Centralizar páginas de error
String errorPage(const String& message) {
  return "<!DOCTYPE html>..." + message + "...";
}

// ✅ Usar constantes
#define MAX_CODES 50
#define ITEMS_PER_PAGE 20
```

### 5. Debugging

```cpp
// ✅ Log de peticiones
void handleEndpoint() {
  Serial.printf("🌐 [WEB] %s %s\n", 
    server.method() == HTTP_GET ? "GET" : "POST",
    server.uri().c_str());
  // ...
}

// ✅ Log de errores
if (!success) {
  Serial.printf("❌ [WEB] Error en %s: %s\n", 
    server.uri().c_str(), 
    errorMessage.c_str());
}
```

---

## Resumen de Estructura de Tablas

### Tabla de Códigos

| Campo | Tipo | Tamaño | Descripción |
|-------|------|--------|-------------|
| type | char[] | 4 | "PIN" o "TAG" |
| value | char[] | 17 | Código (max 16 chars) |
| keyboard_id | uint8_t | 1 | 0=Ambos, 1=T1, 2=T2 |
| relay | uint8_t | 1 | 1 o 2 |
| reserved | uint8_t[] | 2 | Futuras extensiones |

### Tabla de Usuarios BLE

| Campo | Tipo | Tamaño | Descripción |
|-------|------|--------|-------------|
| user_enabled | bool | 1 | Usuario activo |
| user_names | char[] | 32 | Nombre del usuario |
| user_keys | uint8_t[] | 64 | Clave de autenticación |
| user_permissions | uint8_t | 1 | Permisos (flags) |

---

**Estado:** ✅ DOCUMENTACIÓN COMPLETA - FIRMWARE v4.1.0
