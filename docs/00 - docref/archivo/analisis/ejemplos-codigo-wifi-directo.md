# Ejemplos de Código: Implementación WiFi Directo - ESP32

## 🎯 **Implementación Práctica del Modo WiFi Directo**

Este documento proporciona ejemplos de código específicos para implementar el modo WiFi directo en el ESP32, basado en el análisis previo.

## 📋 **Estructura de Implementación**

### **1. Variables Globales y Configuración**

```cpp
// =================== CONFIGURACIÓN WIFI DUAL ===================
enum WiFiMode {
  WIFI_MODE_ETHERNET_ONLY,    // Solo Ethernet (actual)
  WIFI_MODE_AP_EMERGENCY,     // AP de emergencia (actual)
  WIFI_MODE_AP_DUAL,          // AP + Cliente WiFi (nuevo)
  WIFI_MODE_AP_CAPTIVE        // AP con Captive Portal (nuevo)
};

WiFiMode currentWiFiMode = WIFI_MODE_ETHERNET_ONLY;

// Configuración WiFi
struct WiFiConfig {
  char apSSID[32];
  char apPassword[32];
  char clientSSID[32];
  char clientPassword[32];
  bool apEnabled;
  bool clientEnabled;
  bool captivePortalEnabled;
  uint32_t validMarker;
};

WiFiConfig wifiConfig;

// Servidor DNS para Captive Portal
DNSServer dnsServer;

// Variables de estado
bool apActive = false;
bool clientConnected = false;
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 30000; // 30 segundos
```

### **2. Inicialización del Sistema WiFi**

```cpp
// =================== INICIALIZACIÓN WIFI DUAL ===================
void initializeWiFiSystem() {
  Serial.println("🔧 Inicializando sistema WiFi dual...");
  
  // Cargar configuración WiFi desde EEPROM
  loadWiFiConfig();
  
  // Determinar modo de operación
  determineWiFiMode();
  
  // Configurar según el modo determinado
  setupWiFiMode();
  
  Serial.printf("✅ Sistema WiFi inicializado en modo: %d\n", currentWiFiMode);
}

void determineWiFiMode() {
  // Prioridad 1: Ethernet (si está disponible)
  if (ethConnected) {
    currentWiFiMode = WIFI_MODE_ETHERNET_ONLY;
    return;
  }
  
  // Prioridad 2: Cliente WiFi configurado
  if (wifiConfig.clientEnabled && strlen(wifiConfig.clientSSID) > 0) {
    currentWiFiMode = WIFI_MODE_AP_DUAL;
    return;
  }
  
  // Prioridad 3: Modo AP con Captive Portal para configuración inicial
  if (wifiConfig.captivePortalEnabled) {
    currentWiFiMode = WIFI_MODE_AP_CAPTIVE;
    return;
  }
  
  // Fallback: Modo AP de emergencia
  currentWiFiMode = WIFI_MODE_AP_EMERGENCY;
}

void setupWiFiMode() {
  switch (currentWiFiMode) {
    case WIFI_MODE_ETHERNET_ONLY:
      setupEthernetOnly();
      break;
      
    case WIFI_MODE_AP_DUAL:
      setupAPDualMode();
      break;
      
    case WIFI_MODE_AP_CAPTIVE:
      setupAPCaptiveMode();
      break;
      
    case WIFI_MODE_AP_EMERGENCY:
      setupAPEmergencyMode();
      break;
  }
}
```

### **3. Modo AP Dual (Recomendado)**

```cpp
// =================== MODO AP DUAL ===================
void setupAPDualMode() {
  Serial.println("🔧 Configurando modo AP dual...");
  
  // Configurar modo AP + Cliente
  WiFi.mode(WIFI_AP_STA);
  
  // Configurar AP
  setupAP();
  
  // Intentar conectar como cliente
  if (wifiConfig.clientEnabled) {
    connectWiFiClient();
  }
  
  // Configurar servidor web
  setupWebServer();
  
  Serial.println("✅ Modo AP dual configurado");
}

void setupAP() {
  if (!wifiConfig.apEnabled) {
    Serial.println("⚠️ AP deshabilitado en configuración");
    return;
  }
  
  // Generar SSID único basado en MAC
  String macSuffix = fixedSerialNumber.substring(fixedSerialNumber.length() - 6);
  String apSSID = "SWATID_CONFIG_" + macSuffix;
  
  // Usar configuración personalizada o valores por defecto
  const char* ssid = (strlen(wifiConfig.apSSID) > 0) ? wifiConfig.apSSID : apSSID.c_str();
  const char* password = (strlen(wifiConfig.apPassword) > 0) ? wifiConfig.apPassword : "12345678";
  
  // Configurar AP
  WiFi.softAP(ssid, password);
  
  // Configurar IP del AP
  IPAddress apIP(192, 168, 4, 1);
  IPAddress apGateway(192, 168, 4, 1);
  IPAddress apSubnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  
  apActive = true;
  
  Serial.printf("📶 AP configurado:\n");
  Serial.printf("   SSID: %s\n", ssid);
  Serial.printf("   Password: %s\n", password);
  Serial.printf("   IP: %s\n", apIP.toString().c_str());
}

void connectWiFiClient() {
  if (!wifiConfig.clientEnabled || strlen(wifiConfig.clientSSID) == 0) {
    Serial.println("⚠️ Cliente WiFi no configurado");
    return;
  }
  
  Serial.printf("🔗 Conectando a WiFi: %s\n", wifiConfig.clientSSID);
  
  WiFi.begin(wifiConfig.clientSSID, wifiConfig.clientPassword);
  
  // Esperar conexión con timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    clientConnected = true;
    Serial.printf("\n✅ WiFi conectado: %s\n", WiFi.localIP().toString().c_str());
  } else {
    clientConnected = false;
    Serial.println("\n❌ Fallo al conectar WiFi");
  }
}
```

### **4. Modo AP con Captive Portal**

```cpp
// =================== MODO AP CON CAPTIVE PORTAL ===================
void setupAPCaptiveMode() {
  Serial.println("🔧 Configurando modo AP con Captive Portal...");
  
  // Configurar modo AP exclusivo
  WiFi.mode(WIFI_AP);
  
  // Configurar AP
  setupAP();
  
  // Configurar Captive Portal
  setupCaptivePortal();
  
  // Configurar servidor web
  setupWebServer();
  
  Serial.println("✅ Modo AP con Captive Portal configurado");
}

void setupCaptivePortal() {
  if (!wifiConfig.captivePortalEnabled) {
    return;
  }
  
  // Configurar servidor DNS para redireccionar todas las peticiones
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(53, "*", apIP);
  
  Serial.println("🌐 Captive Portal activado");
  Serial.printf("   Todas las peticiones DNS redirigidas a: %s\n", apIP.toString().c_str());
}

void handleCaptivePortal() {
  // Redirigir todas las peticiones al portal de configuración
  if (dnsServer.processNextRequest()) {
    return;
  }
  
  // Si no es una petición DNS, redirigir a la página de configuración
  server.sendHeader("Location", "http://192.168.4.1/wifi-config", true);
  server.send(302, "text/plain", "");
}
```

### **5. Escaneo de Redes WiFi**

```cpp
// =================== ESCANEO DE REDES WIFI ===================
void scanWiFiNetworks() {
  Serial.println("🔍 Escaneando redes WiFi...");
  
  // Iniciar escaneo
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("❌ No se encontraron redes WiFi");
    return;
  }
  
  Serial.printf("📶 Se encontraron %d redes:\n", n);
  
  for (int i = 0; i < n; ++i) {
    Serial.printf("   %d: %s (%d dBm) %s\n", 
      i + 1, 
      WiFi.SSID(i).c_str(), 
      WiFi.RSSI(i),
      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Abierta" : "Protegida"
    );
  }
}

String getWiFiNetworksJSON() {
  String json = "[";
  
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"encryption\":" + String(WiFi.encryptionType(i)) + ",";
    json += "\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }
  
  json += "]";
  return json;
}
```

### **6. Página de Configuración WiFi**

```cpp
// =================== PÁGINA DE CONFIGURACIÓN WIFI ===================
void handleWiFiConfig() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Configuración WiFi - Controladora A2</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:800px;margin:0 auto;padding:20px;background:#f5f5f5;}";
  html += ".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
  html += "h1{color:#2c3e50;text-align:center;border-bottom:2px solid #3498db;padding-bottom:10px;}";
  html += ".section{margin:20px 0;padding:15px;border:1px solid #ddd;border-radius:5px;}";
  html += ".section h3{color:#34495e;margin-top:0;}";
  html += "input,select,button{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:5px;}";
  html += "button{background:#3498db;color:white;cursor:pointer;font-size:16px;}";
  html += "button:hover{background:#2980b9;}";
  html += ".status{padding:10px;margin:10px 0;border-radius:5px;}";
  html += ".status.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}";
  html += ".status.error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}";
  html += ".status.info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb;}";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h1>Configuración WiFi</h1>";
  
  // Estado actual
  html += "<div class='section'>";
  html += "<h3>Estado Actual</h3>";
  html += "<div class='status info'>";
  html += "<strong>Modo:</strong> " + getWiFiModeString() + "<br>";
  html += "<strong>AP:</strong> " + String(apActive ? "Activo" : "Inactivo") + "<br>";
  html += "<strong>Cliente:</strong> " + String(clientConnected ? "Conectado" : "Desconectado") + "<br>";
  if (clientConnected) {
    html += "<strong>IP Cliente:</strong> " + WiFi.localIP().toString() + "<br>";
  }
  if (apActive) {
    html += "<strong>IP AP:</strong> " + WiFi.softAPIP().toString() + "<br>";
  }
  html += "</div></div>";
  
  // Configuración AP
  html += "<div class='section'>";
  html += "<h3>Punto de Acceso (AP)</h3>";
  html += "<form action='/wifi-config/save' method='post'>";
  html += "<label>SSID del AP:</label>";
  html += "<input type='text' name='apSSID' value='" + String(wifiConfig.apSSID) + "' placeholder='SWATID_CONFIG_XXXXXX'>";
  html += "<label>Contraseña del AP:</label>";
  html += "<input type='password' name='apPassword' value='" + String(wifiConfig.apPassword) + "' placeholder='12345678'>";
  html += "<label><input type='checkbox' name='apEnabled' " + String(wifiConfig.apEnabled ? "checked" : "") + "> Habilitar AP</label>";
  html += "</div>";
  
  // Configuración Cliente
  html += "<div class='section'>";
  html += "<h3>Cliente WiFi</h3>";
  html += "<label>Red WiFi:</label>";
  html += "<select name='clientSSID' id='clientSSID'>";
  html += "<option value=''>Seleccionar red...</option>";
  html += "</select>";
  html += "<button type='button' onclick='scanNetworks()'>Escanear Redes</button>";
  html += "<label>Contraseña:</label>";
  html += "<input type='password' name='clientPassword' value='" + String(wifiConfig.clientPassword) + "'>";
  html += "<label><input type='checkbox' name='clientEnabled' " + String(wifiConfig.clientEnabled ? "checked" : "") + "> Conectar como cliente</label>";
  html += "</div>";
  
  // Captive Portal
  html += "<div class='section'>";
  html += "<h3>Captive Portal</h3>";
  html += "<label><input type='checkbox' name='captivePortalEnabled' " + String(wifiConfig.captivePortalEnabled ? "checked" : "") + "> Habilitar Captive Portal</label>";
  html += "<p><small>El Captive Portal redirige automáticamente a esta página cuando alguien se conecta al AP.</small></p>";
  html += "</div>";
  
  // Botones de acción
  html += "<div class='section'>";
  html += "<button type='submit'>Guardar Configuración</button>";
  html += "<button type='button' onclick='testConnection()'>Probar Conexión</button>";
  html += "<button type='button' onclick='resetWiFi()'>Reset WiFi</button>";
  html += "</div>";
  
  html += "</form></div>";
  
  // JavaScript
  html += "<script>";
  html += "function scanNetworks() {";
  html += "  fetch('/wifi-config/scan')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      const select = document.getElementById('clientSSID');";
  html += "      select.innerHTML = '<option value=\"\">Seleccionar red...</option>';";
  html += "      data.forEach(network => {";
  html += "        const option = document.createElement('option');";
  html += "        option.value = network.ssid;";
  html += "        option.textContent = network.ssid + ' (' + network.rssi + ' dBm)';";
  html += "        select.appendChild(option);";
  html += "      });";
  html += "    });";
  html += "}";
  html += "function testConnection() {";
  html += "  fetch('/wifi-config/test')";
  html += "    .then(response => response.text())";
  html += "    .then(data => alert(data));";
  html += "}";
  html += "function resetWiFi() {";
  html += "  if (confirm('¿Está seguro de resetear la configuración WiFi?')) {";
  html += "    fetch('/wifi-config/reset', {method: 'POST'})";
  html += "      .then(response => response.text())";
  html += "      .then(data => {";
  html += "        alert(data);";
  html += "        location.reload();";
  html += "      });";
  html += "  }";
  html += "}";
  html += "</script>";
  
  html += "<a href='/'><button>Volver al inicio</button></a>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}
```

### **7. Gestión de Configuración**

```cpp
// =================== GESTIÓN DE CONFIGURACIÓN ===================
void loadWiFiConfig() {
  EEPROM.get(EEPROM_WIFI_CONFIG_OFFSET, wifiConfig);
  
  if (wifiConfig.validMarker != 0xCAFEWIFI) {
    // Configuración por defecto
    strncpy(wifiConfig.apSSID, "", sizeof(wifiConfig.apSSID));
    strncpy(wifiConfig.apPassword, "12345678", sizeof(wifiConfig.apPassword));
    strncpy(wifiConfig.clientSSID, "", sizeof(wifiConfig.clientSSID));
    strncpy(wifiConfig.clientPassword, "", sizeof(wifiConfig.clientPassword));
    wifiConfig.apEnabled = true;
    wifiConfig.clientEnabled = false;
    wifiConfig.captivePortalEnabled = true;
    wifiConfig.validMarker = 0xCAFEWIFI;
    
    saveWiFiConfig();
    Serial.println("🔧 Configuración WiFi inicializada con valores por defecto");
  } else {
    Serial.println("💾 Configuración WiFi cargada desde EEPROM");
  }
}

void saveWiFiConfig() {
  wifiConfig.validMarker = 0xCAFEWIFI;
  EEPROM.put(EEPROM_WIFI_CONFIG_OFFSET, wifiConfig);
  EEPROM.commit();
  Serial.println("💾 Configuración WiFi guardada");
}

void handleWiFiConfigSave() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  // Obtener parámetros del formulario
  if (server.hasArg("apSSID")) {
    strncpy(wifiConfig.apSSID, server.arg("apSSID").c_str(), sizeof(wifiConfig.apSSID) - 1);
    wifiConfig.apSSID[sizeof(wifiConfig.apSSID) - 1] = '\0';
  }
  
  if (server.hasArg("apPassword")) {
    strncpy(wifiConfig.apPassword, server.arg("apPassword").c_str(), sizeof(wifiConfig.apPassword) - 1);
    wifiConfig.apPassword[sizeof(wifiConfig.apPassword) - 1] = '\0';
  }
  
  if (server.hasArg("clientSSID")) {
    strncpy(wifiConfig.clientSSID, server.arg("clientSSID").c_str(), sizeof(wifiConfig.clientSSID) - 1);
    wifiConfig.clientSSID[sizeof(wifiConfig.clientSSID) - 1] = '\0';
  }
  
  if (server.hasArg("clientPassword")) {
    strncpy(wifiConfig.clientPassword, server.arg("clientPassword").c_str(), sizeof(wifiConfig.clientPassword) - 1);
    wifiConfig.clientPassword[sizeof(wifiConfig.clientPassword) - 1] = '\0';
  }
  
  wifiConfig.apEnabled = server.hasArg("apEnabled");
  wifiConfig.clientEnabled = server.hasArg("clientEnabled");
  wifiConfig.captivePortalEnabled = server.hasArg("captivePortalEnabled");
  
  // Guardar configuración
  saveWiFiConfig();
  
  // Reiniciar WiFi con nueva configuración
  restartWiFi();
  
  server.send(200, "text/html", 
    "<html><body><h1>Configuración Guardada</h1>"
    "<p>La configuración WiFi ha sido guardada y aplicada.</p>"
    "<p>El sistema se reiniciará en 5 segundos...</p>"
    "<script>setTimeout(function(){window.location.href='/';}, 5000);</script>"
    "</body></html>");
}

void restartWiFi() {
  Serial.println("🔄 Reiniciando sistema WiFi...");
  
  // Desconectar WiFi
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(1000);
  
  // Reinicializar
  initializeWiFiSystem();
}
```

### **8. Monitoreo y Mantenimiento**

```cpp
// =================== MONITOREO WIFI ===================
void monitorWiFiConnection() {
  if (millis() - lastWiFiCheck < wifiCheckInterval) {
    return;
  }
  
  lastWiFiCheck = millis();
  
  // Verificar conexión del cliente
  if (wifiConfig.clientEnabled && WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Cliente WiFi desconectado, reintentando...");
    connectWiFiClient();
  }
  
  // Procesar Captive Portal
  if (currentWiFiMode == WIFI_MODE_AP_CAPTIVE && dnsServer) {
    dnsServer.processNextRequest();
  }
}

String getWiFiModeString() {
  switch (currentWiFiMode) {
    case WIFI_MODE_ETHERNET_ONLY: return "Solo Ethernet";
    case WIFI_MODE_AP_EMERGENCY: return "AP de Emergencia";
    case WIFI_MODE_AP_DUAL: return "AP Dual";
    case WIFI_MODE_AP_CAPTIVE: return "AP con Captive Portal";
    default: return "Desconocido";
  }
}

void printWiFiStatus() {
  Serial.println("📶 === ESTADO WIFI ===");
  Serial.printf("   Modo: %s\n", getWiFiModeString().c_str());
  Serial.printf("   AP Activo: %s\n", apActive ? "Sí" : "No");
  Serial.printf("   Cliente Conectado: %s\n", clientConnected ? "Sí" : "No");
  
  if (apActive) {
    Serial.printf("   IP AP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("   SSID AP: %s\n", WiFi.softAPSSID().c_str());
  }
  
  if (clientConnected) {
    Serial.printf("   IP Cliente: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   SSID Cliente: %s\n", WiFi.SSID().c_str());
    Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());
  }
}
```

### **9. Integración en el Loop Principal**

```cpp
// =================== INTEGRACIÓN EN LOOP ===================
void loop() {
  // ... código existente ...
  
  // Monitorear conexión WiFi
  monitorWiFiConnection();
  
  // Procesar servidor web
  server.handleClient();
  
  // ... resto del código existente ...
}
```

### **10. Configuración de EEPROM**

```cpp
// =================== OFFSETS EEPROM ===================
#define EEPROM_WIFI_CONFIG_OFFSET 1024  // Offset para configuración WiFi

// Asegurar que no hay solapamiento con otras configuraciones
#define EEPROM_CODES_OFFSET 512
#define EEPROM_REMOTE_CODES_OFFSET 2048
```

## 📝 **Instrucciones de Implementación**

### **1. Modificaciones Requeridas**
1. Añadir las variables globales al inicio del archivo
2. Implementar las funciones de gestión WiFi
3. Añadir las rutas del servidor web
4. Modificar la función `setup()` para incluir `initializeWiFiSystem()`
5. Añadir `monitorWiFiConnection()` al loop principal

### **2. Nuevas Rutas del Servidor Web**
```cpp
// Añadir en setupWebServer()
server.on("/wifi-config", handleWiFiConfig);
server.on("/wifi-config/save", HTTP_POST, handleWiFiConfigSave);
server.on("/wifi-config/scan", HTTP_GET, handleWiFiScan);
server.on("/wifi-config/test", HTTP_GET, handleWiFiTest);
server.on("/wifi-config/reset", HTTP_POST, handleWiFiReset);
```

### **3. Configuración de Compilación**
- Asegurar que las librerías WiFi y DNSServer están incluidas
- Verificar que hay suficiente espacio en EEPROM
- Configurar el ESP32 para 240MHz para mejor rendimiento WiFi

Esta implementación proporciona una solución completa para el modo WiFi directo, permitiendo configuración sin cables y manteniendo la funcionalidad completa del sistema.
