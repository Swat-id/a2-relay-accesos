/*
 * Controladora A2 - SWATID
 * Version: v2.5.0
 * Date: June 2025
 * 
 * Características completas:
 * - Soporte dual de teclados Wiegand (GPIO 33/14 y GPIO 4/16)
 * - Sistema de seguridad avanzado con bloqueo por intentos fallidos
 * - Control remoto de seguridad vía MQTT
 * - Serial number fijo para MQTT (no modificable)
 * - Nombre de dispositivo editable por usuario
 * - Notificaciones MQTT completas de todos los eventos
 * - Interfaz web con estado de seguridad
 * - Conectividad Ethernet con PHY LAN8720
 * - Control de relés duales con temporización individual
 * - Almacenamiento de hasta 500 códigos PIN/TAG
 * 
 * Hardware: KinCony KC868-A2 ESP32 board
 * Compatible con: Arduino IDE con librerías ESP32
 * 
 * Librerías requeridas:
 * - ETH (incluida en ESP32)
 * - WiFi (incluida en ESP32)
 * - WebServer (incluida en ESP32)
 * - PubSubClient
 * - ArduinoJson
 * - EEPROM (incluida en ESP32)
 * - time (incluida en ESP32)
 * - DNSServer (incluida en ESP32)
 * - ESPmDNS (incluida en ESP32)
 */

#include <ETH.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>

// =================== MÓDULOS PROPIOS (Fase 0.75 - estructura) ===================
#include "target_features.h"   // Feature flags por target (A2 / A2v3)
#include "hw_config.h"         // Pines y constantes de placa
#include "eeprom_layout.h"     // Estructuras persistentes + mapa EEPROM/NVS
#include "net_manager.h"       // Estado agregado de conectividad (ETH > WiFi)
#include "wifi_manager.h"      // WiFi de gestión (AP de emergencia; Fase 1: AP/STA)
#include "gsm_modem.h"         // Módem 4G (stub; Fase 3 en A2v3)
#include "remote_codes.h"      // Caché de códigos remotos (NVS)
#include "local_codes.h"       // Códigos de acceso locales (EEPROM)
#include "digital_inputs.h"    // Entradas digitales DI1/DI2
#include "web_common.h"        // CSS compartido + helpers del portal web
#include "rtc_time.h"          // RTC DS3231 (A2v3): hora propia mantenida
#include "status_display.h"    // Pantalla OLED de estado (A2v3)
#if A2_BOARD_A2V3
#include <SPI.h>               // Bus del W5500
#endif

// =================== BLE (Solo si está habilitado) ===================
#ifdef ENABLE_BLE
#include <NimBLEDevice.h>
#include "mbedtls/sha256.h"     // v4.1: Para Challenge-Response seguro
#include "mbedtls/md.h"         // v4.1: Para HMAC-SHA256 (usado en HKDF manual)
#include "esp_random.h"         // v4.1: Para generación de nonces y tokens
#define BLE_ENABLED true
#else
#define BLE_ENABLED false
#endif

// =================== MACROS PARA OPTIMIZACIÓN ===================
#define DEBUG_LEVEL 0  // 0=Sin debug, 1=Minimal, 2=Normal, 3=Verbose
#if DEBUG_LEVEL == 0
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...)
#elif DEBUG_LEVEL == 1
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#endif
 
 // =================== CONFIGURACIÓN MQTT ===================
 const char* mqtt_broker = "188.245.213.181";
 const int mqtt_port = 1883;
 const char* mqtt_username = "swatidhome";
 const char* mqtt_password = "Swatid2025!";
 const char* admin_user = "admin";
 char admin_password[32] = "admin";
 
 // =================== VARIABLES DE CONTROL DE RELÉS ===================
 unsigned long releStartTime[3] = {0, 0, 0};
 float releDurations[3] = {0, 0, 0};
 bool releActive[3] = {false, false, false};
 
// =================== INFORMACIÓN DEL FIRMWARE ===================
#define FIRMWARE_VERSION_MAJOR 5
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 0
const char* firmwareVersion = "v5.0.0";
#if defined(ENABLE_BLE)
const char* firmwareFullVersion = "v5.0.0-BLE";
#elif A2_BOARD_A2V3
const char* firmwareFullVersion = "v5.0.0-S3";
#elif A2_FEATURE_GSM_MODEM
const char* firmwareFullVersion = "v5.0.0-4G";
#else
const char* firmwareFullVersion = "v5.0.0";
#endif
#define FIRMWARE_VERSION_BUILD __DATE__ " " __TIME__
const char* firmwareBuild = FIRMWARE_VERSION_BUILD;
 
 // =================== IDENTIFICACIÓN DEL DISPOSITIVO ===================
 String fixedSerialNumber;  // SERIAL FIJO PARA MQTT - NO MODIFICABLE
 char deviceName[32] = "";    // NOMBRE EDITABLE POR USUARIO
 bool useDhcp = true;
 IPAddress staticIP(0, 0, 0, 0);
 IPAddress staticGateway(0, 0, 0, 0);
 IPAddress staticSubnet(255, 255, 255, 0);
 IPAddress staticDns(8, 8, 8, 8);
 int lastKeyboardId = 1;
 
// =================== SISTEMA DE SEGURIDAD ===================
bool localAccessBlocked = false;              // Bloqueo de acceso local
bool keyboardReadingEnabled = true;           // Control de lectura de teclados (por defecto habilitado)
unsigned long blockStartTime = 0;             // Tiempo de inicio del bloqueo
unsigned long blockDuration = 60000;          // Duración del bloqueo (60s por defecto)
int failedAttempts = 0;                       // Intentos fallidos consecutivos
int maxFailedAttempts = 3;                    // Máximo intentos antes del bloqueo
unsigned long lastFailedAttempt = 0;          // Último intento fallido
const unsigned long failedAttemptTimeout = 300000;  // Reset contador tras 5 minutos
 
// Estructuras persistentes (Config, StoredCodes, códigos remotos, DI, BLE):
// ver eeprom_layout.h — única fuente del mapa EEPROM/NVS
Config config;

#define TURNSTILE_TIMEOUT 5000   // 5 segundos timeout para respuestas MQTT

// =================== ESTRUCTURAS DEL MODO TORNO ===================
struct PendingRequest {
  bool active;            // true si hay solicitud pendiente
  uint8_t keyboard_id;    // Teclado origen (1 o 2)
  char code[17];          // Código introducido
  char type[5];           // Tipo de código (PIN/TAG)
  unsigned long timestamp; // Timestamp de la solicitud
  uint8_t relay_to_open;  // Relé que se abrirá si se aprueba
};

// (storedCodes vive en local_codes.cpp; storedRemoteCodes en remote_codes.cpp)
 TurnstileConfig turnstileConfig;
 PendingRequest pendingRequest;

// =================== ESTRUCTURAS PARA OTA ===================
struct OTAConfig {
  char updateUrl[256];           // URL del servidor de actualizaciones
  bool autoUpdateEnabled;        // Habilitar actualización automática
  int checkInterval;             // Intervalo de verificación (horas)
  unsigned long lastCheck;       // Última verificación
  bool forceUpdate;              // Forzar actualización
  uint32_t validMarker;          // Marcador de validación
};

struct DeviceInfo {
  String macAddress;
  String currentVersion;
  String deviceModel;
  String serialNumber;
  size_t flashSize;
  size_t freeHeap;
};

OTAConfig otaConfig;
DeviceInfo deviceInfo;
 
// =================== CONTADORES DE MENSAJES ===================
unsigned long messageId = 0;
unsigned long responseId = 0;

// =================== GESTIÓN DE TIEMPO ===================
unsigned long systemStartTime = 0;
unsigned long lastTimeSync = 0;
bool timeSynced = false;
char currentTimeString[32] = "";
 
 // =================== HARDWARE ===================
 // Pines y constantes de placa: ver hw_config.h
 float releDuration = 2.0;
 HardwareSerial RS485_Serial(RS485_UART_NUM);  // UART2 en A2 clásico, UART1 en A2v3

// (ver digital_inputs.h)

// =================== CONFIGURACIÓN BLE (v4.0) ===================
// Estructuras persistentes BLE (DeviceKeyConfig, BLEAuthConfig) y sus
// offsets/tamaños: ver eeprom_layout.h
#ifdef ENABLE_BLE
DeviceKeyConfig deviceKeyConfig;

// Permisos BLE
#define BLE_PERM_RELAY_CONTROL  0x01  // Control de relés
#define BLE_PERM_MODE_CHANGE    0x02  // Cambio de modo
#define BLE_PERM_ADD_CODES      0x04  // Añadir códigos
#define BLE_PERM_NETWORK_CONFIG 0x08  // Configuración de red
#define BLE_PERM_ADMIN          0xFF  // Todos los permisos

BLEAuthConfig bleAuthConfig;
bool bleAuthenticated = false;
uint8_t bleCurrentPermissions = 0;
String bleConnectedUser = "";
unsigned long bleConnectionTime = 0;        // Momento de conexión BLE
#define BLE_AUTH_TIMEOUT_MS 30000           // 30 segundos para autenticarse

// =================== SEGURIDAD BLE v4.1 - Challenge-Response ===================
// Sistema de autenticación seguro que evita transmitir la clave en claro
uint8_t bleSessionNonce[16];                // Challenge generado por el dispositivo (16 bytes)
uint8_t bleSessionToken[8];                 // Token de sesión para operaciones (8 bytes)
bool bleSessionValid = false;               // Si hay sesión activa con token válido
unsigned long bleSessionTime = 0;           // Tiempo de última actividad de sesión
bool bleChallengeReady = false;             // Si hay un challenge pendiente de respuesta
#define BLE_SESSION_TIMEOUT_MS 300000       // 5 minutos de inactividad máxima
#define BLE_CHALLENGE_TIMEOUT_MS 30000      // 30 segundos para responder al challenge

// UUIDs para servicios BLE
#define SERVICE_UUID        "0000FF00-0000-1000-8000-00805F9B34FB"
#define CHAR_AUTH_UUID      "0000FF01-0000-1000-8000-00805F9B34FB"
#define CHAR_RELAY_UUID     "0000FF02-0000-1000-8000-00805F9B34FB"
#define CHAR_MODE_UUID      "0000FF03-0000-1000-8000-00805F9B34FB"
#define CHAR_ADDCODE_UUID   "0000FF04-0000-1000-8000-00805F9B34FB"
#define CHAR_NETWORK_UUID   "0000FF05-0000-1000-8000-00805F9B34FB"
#define CHAR_RELAYTIME_UUID "0000FF06-0000-1000-8000-00805F9B34FB"
#define CHAR_STATUS_UUID    "0000FF07-0000-1000-8000-00805F9B34FB"
#define CHAR_DEVINFO_UUID   "0000FF08-0000-1000-8000-00805F9B34FB"
#define CHAR_FULLINFO_UUID  "0000FF09-0000-1000-8000-00805F9B34FB"
#define CHAR_CODES_UUID     "0000FF0A-0000-1000-8000-00805F9B34FB"
#define CHAR_CHALLENGE_UUID "0000FF0B-0000-1000-8000-00805F9B34FB"  // Challenge para auth seguro

// Tipo de dispositivo para provisión automática BLE
#define DEVICE_TYPE         "SWATID-A2"
#define PROTOCOL_VERSION    1

// Punteros BLE
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pAuthChar = nullptr;
NimBLECharacteristic* pRelayChar = nullptr;
NimBLECharacteristic* pModeChar = nullptr;
NimBLECharacteristic* pAddCodeChar = nullptr;
NimBLECharacteristic* pNetworkChar = nullptr;
NimBLECharacteristic* pRelayTimeChar = nullptr;
NimBLECharacteristic* pStatusChar = nullptr;
NimBLECharacteristic* pDevInfoChar = nullptr;
NimBLECharacteristic* pFullInfoChar = nullptr;
NimBLECharacteristic* pCodesChar = nullptr;
NimBLECharacteristic* pChallengeChar = nullptr;  // v4.1: Challenge para auth seguro
bool bleDeviceConnected = false;

// Forward declarations de funciones BLE (evita que PlatformIO genere prototipos fuera del #ifdef)
uint32_t calculateBLEChecksum(const BLEAuthConfig& cfg);
uint32_t calculateDeviceKeyChecksum(const DeviceKeyConfig& cfg);
void loadBLEAuthConfig();
void saveBLEAuthConfig();
void loadDeviceKeyConfig();           // v4.1: Clave maestra para HKDF
bool deriveUserKey(const char* userId, uint8_t* outputKey);  // v4.1: HKDF
void sendBLEChunked(NimBLECharacteristic* pChar, const String& data);
void sendBLEWithEOT(NimBLECharacteristic* pChar, const String& data);
void initBLE();
void updateBLEStatus();
void updateFF09Value();
void updateFF0AValue(int page = 0);
void handleBLEPage();
void handleBLEClearAll();
void handleBLEClearSuperadmin();
void handleBLEClearUser();
void handleBLEStatus();

#endif // ENABLE_BLE

// Función para calcular checksum de la configuración DI
// (ver digital_inputs.h)

// Variables globales para entradas digitales
// (ver digital_inputs.h)

// Todas las configuraciones usan EEPROM para almacenamiento persistente
 
// =================== SERVIDOR WEB Y CONECTIVIDAD ===================
WebServer server(80);
IPAddress ip;
bool ethConnected = false;   // Estado ETH legacy (net_manager es la fuente agregada)

// Declaraciones de funciones del servidor web
void handleRoot();
void handleSave();
void handleRele();
void handleReboot();
void handleReset();
void handleChangePass();
void handleCodes();
void handleCodesAdd();
void handleCodesDelete();
void handleNotFound();

// Declaraciones de funciones del modo torno
void loadTurnstileConfig();
void saveTurnstileConfig();
void initializeTurnstileMode();
void clearPendingRequest();
bool isTurnstileModeEnabled();
uint8_t getRelayForKeyboard(int keyboardId);
void handleTurnstileValidation(const String& code, const String& type, int keyboardId);
void processMqttResponse(const JsonDocument& doc);

// Declaraciones de funciones de entradas digitales
void loadDigitalInputConfig();
void saveDigitalInputConfig();
void processDigitalInput(int inputNumber, DigitalInputState &state, uint8_t enabled, uint8_t relay, uint32_t duration_ms, uint8_t inverse);
void publishDigitalInputEvent(int inputNumber, int relay, uint32_t duration_ms);
void handleDigitalInputs();
void handleDigitalInputsStatus();
void handleDigitalInputsConfig();
void handleSaveDigitalInput();
void checkPendingRequestTimeout();
void handleTurnstileConfig();
void handleTurnstileReset();
void handleExportCodes();
void handleExportRemoteCodes();
void handleImportCodes();
void handleFileUpload();
void processCSVImport(String csvContent);
void processCSVImportWithResponse(String csvContent);
void handleBulkImport();
void handleCSVTemplate();
void handleStartTagReading();
void handleStopTagReading();
void handleReadTagsStatus();
void handleExportReadTags();
void handleLoadReadTags();
void handleTagReading(const String& code, int keyboardId);
void handleTimeSync();
void handleTimeUpdate();
void handleSecurityBlockAccess();
void handleSecurityUnblockAccess();
void handleSecurityDisableKeyboards();
void handleSecurityEnableKeyboards();
void publishTurnstileEvent(const String& code, const String& type, int keyboardId, bool success, const String& reason);
String createTurnstileMqttMessage(const String& code, const String& type, int keyboardId, uint8_t relayToOpen);
void logTurnstileCommunication(const String& action, const String& topic, const String& message, bool success);
void processRemoteValidationResponse(const JsonDocument& doc);

// Declaraciones de funciones para códigos remotos
// (gestión de códigos remotos: ver remote_codes.h)
void handleRemoteCodes();
void handleRemoteCodesAdd();
void handleRemoteCodesDelete();
void handleRemoteCodesDeleteAll();
 
 // Cliente MQTT
 WiFiClient espClient;
 PubSubClient mqttClient(espClient);
 
 // =================== VARIABLES PARA LECTURA DE TAGS ===================
bool tagReadingActive = false;
struct TagReadingConfig {
  bool enabled;
  int keyboard1_relay1;  // -1 = deshabilitado, 1-2 = relé
  int keyboard1_relay2;  // -1 = deshabilitado, 1-2 = relé
  int keyboard2_relay1;  // -1 = deshabilitado, 1-2 = relé
  int keyboard2_relay2;  // -1 = deshabilitado, 1-2 = relé
  bool saveToMemory;     // true = guardar en EEPROM, false = solo exportar
} tagReadingConfig;

struct ReadTag {
  String code;
  unsigned long timestamp;
  bool saved;
};
std::vector<ReadTag> readTags;

// =================== VARIABLES WIEGAND - TECLADO 1 ===================
 volatile unsigned long wiegand1Data = 0;
 volatile unsigned long wiegand1Bits = 0;
 volatile unsigned long wiegand1BitTime = 0;
 volatile bool wiegand1Complete = false;
 
 // =================== VARIABLES WIEGAND - TECLADO 2 ===================
 volatile unsigned long wiegand2Data = 0;
 volatile unsigned long wiegand2Bits = 0;
 volatile unsigned long wiegand2BitTime = 0;
 volatile bool wiegand2Complete = false;
 
 // =================== VARIABLES DE PIN ===================
char currentPin1[17] = "";
unsigned long lastKeyPressTime1 = 0;
char currentPin2[17] = "";
unsigned long lastKeyPressTime2 = 0;
 
 const int maxPinLength = 6;
 const unsigned long keyPressTimeout = 5000;
 
 // =================== ÚLTIMO ACCESO ===================
char lastType[5] = "";
char lastCode[17] = "";
char lastTime[32] = "";
 
 // =================== CONFIGURACIÓN NTP ===================
 const char* ntpServer = "pool.ntp.org";
 const long gmtOffset_sec = TIME_GMT_OFFSET_SEC;
 const int daylightOffset_sec = TIME_DST_OFFSET_SEC;
 
 // =================== DECLARACIONES DE FUNCIONES ===================
 void saveConfiguration();
 void loadConfiguration();
 void connectToMqtt();
 String generateFixedSerial();
 void publishError(int errorCode, String description);
 void publishDeviceInfo(unsigned long originalMessageId = 0);
 void publishAccessEvent(const String& code, const String& type, int keyboardId, bool success, const String& source);
 void publishFailedAccess(const String& code, const String& type, int keyboardId, const String& reason);
 void controlRele();
 void controlReleWithDuration(float duration, int relay = 1);
 void processKey(uint8_t key, int keyboardId);
 // (AP de emergencia: ver wifi_manager.h)
 void validateCode(const String& code, const String& type, int keyboardId);
 void setupWebServer();
 void publishResponse(int responseType, unsigned long originalMessageId, String responseInfo, int relay = 0);
 String getTimeString();
 void handleCodes();
 void handleCodesAdd();
 void handleCodesDelete();
void loadStoredCodes();
void saveStoredCodes();
// Funciones de diagnóstico eliminadas para reducir tamaño
// void diagnoseEEPROM();
// void verifyMemoryLayout();
// void testPersistence();
// void verifyEEPROMIntegrity();
bool addCode(const char* type, const char* value, int keyboardId, int relay);
bool addCode(const char* type, const char* value, int relay);

// =================== FUNCIONES OTA ===================
void loadOTAConfig();
void saveOTAConfig();
void initializeDeviceInfo();
int compareVersions(const String& version1, const String& version2);
void checkForUpdates();
bool downloadAndUpdate(const String& downloadUrl);
void handleOTAPage();
void handleOTAUpload();
void handleOTAConfig();
void handleOTACheck();
void handleOTAStatus();
void setupRollback();
bool verifyCurrentFirmware();
void performRollback();
 bool deleteCode(const char* type, const char* value);
 bool isCodeStored(const char* type, const char* value, int* relay = nullptr);
bool isCodeStored(const char* type, const char* value, int keyboardId, int* relay);
 void setupWebServer();
 void checkRelayTimeout();
 void processWiegand1Data();
 void processWiegand2Data();
 void processRS485Keypad();
 void checkAccessBlock();
 void resetFailedAttempts();
 void resetToDefault();
void updateConfiguration(const JsonDocument& doc);
void updateOTAConfiguration(const JsonDocument& doc);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void processCommand(const JsonDocument& doc);
 
// Flag para conexión MQTT pendiente (no bloquear en callback)
bool mqttConnectionPending = false;

// =================== CALLBACK EVENTOS ETHERNET ===================
// IMPORTANTE: No hacer operaciones bloqueantes aquí
void WiFiEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("🌐 ETH Iniciado");
      ETH.setHostname(deviceName);
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("🌐 ETH Conectado");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ip = ETH.localIP();
      Serial.print("🌐 ETH Dirección IP: ");
      Serial.println(ip);
      ethConnected = true;
      netOnEthGotIp();
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      // NO llamar a connectToMqtt() aquí - puede bloquear
      // En su lugar, marcar para conectar en el loop()
      mqttConnectionPending = true;
      Serial.println("🌐 MQTT conexión programada para loop()");
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("🌐 ETH Desconectado");
      ethConnected = false;
      netOnEthDown();
      mqttConnectionPending = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("🌐 ETH Detenido");
      ethConnected = false;
      netOnEthDown();
      mqttConnectionPending = false;
       break;
     default:
       break;
   }
 }
 
 // =================== GENERACIÓN DE SERIAL FIJO ===================
 String generateFixedSerial() {
   uint64_t chipid = ESP.getEfuseMac();
   char buf[32];
   snprintf(buf, sizeof(buf), "SWATID_%04X%08X", (uint16_t)(chipid>>32), (uint32_t)chipid);
   return String(buf);
 }
 
 // =================== FUNCIONES DE TIEMPO ===================
 String getTimestamp() {
   struct tm timeinfo;
   char timeStringBuff[30];
   
   if(!getLocalTime(&timeinfo)) {
     Serial.println("⚠️ Error obteniendo la hora");
     return String("2023-01-01T00:00:00+01:00");
   }
   
   strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S+01:00", &timeinfo);
   return String(timeStringBuff);
 }
 
String getTimeString() {
  // Si tenemos hora sincronizada, usar esa
  if (timeSynced && strlen(currentTimeString) > 0) {
    // Extraer solo la parte de tiempo (HH:MM:SS) de la cadena completa
    char* spaceIndex = strchr(currentTimeString, ' ');
    if (spaceIndex != NULL) {
      return String(spaceIndex + 1);
    }
    return String(currentTimeString);
  }
  
  // Fallback: usar tiempo del sistema
  struct tm timeinfo;
  char timeStringBuff[9];
  
  if(!getLocalTime(&timeinfo)) {
    unsigned long now = millis() / 1000;
    int seconds = now % 60;
    int minutes = (now / 60) % 60;
    int hours = (now / 3600) % 24;
    
    sprintf(timeStringBuff, "%02d:%02d:%02d", hours, minutes, seconds);
  } else {
    strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
  }
  
  return String(timeStringBuff);
}
 
// =================== CONECTIVIDAD MQTT ROBUSTA ===================
// (timeout MQTT_CONNECT_TIMEOUT_SEC definido en hw_config.h)

void connectToMqtt() {
  // Verificar conectividad agregada (ETH hoy; ETH o WiFi STA desde Fase 1)
  if (!netHasConnectivity()) {
    Serial.println("📡 MQTT: Sin conectividad de red, saltando");
    return;
  }
  
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(2048);
  mqttClient.setSocketTimeout(MQTT_CONNECT_TIMEOUT_SEC);  // Timeout corto
  
  Serial.printf("📡 Conectando a MQTT (timeout: %ds)...", MQTT_CONNECT_TIMEOUT_SEC);
  
  String clientId = "ESP32Client-";
  clientId += String(random(0xffff), HEX);
  
  unsigned long startTime = millis();
  bool connected = mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password);
  unsigned long elapsed = millis() - startTime;
  
  if (connected) {
    Serial.printf(" ✅ Conectado en %lums\n", elapsed);
    
    // Suscribirse usando SERIAL FIJO
    String commandTopic = "swatidhome/command/" + fixedSerialNumber + "/#";
    bool subscribed = mqttClient.subscribe(commandTopic.c_str());
    
    if (subscribed) {
      Serial.printf("📡 Suscrito a: %s\n", commandTopic.c_str());
    } else {
      Serial.printf("❌ Error suscripción: %s\n", commandTopic.c_str());
    }
    
    // Pequeña pausa (no bloqueante significativamente)
    delay(100);
    
    publishError(3, "Dispositivo iniciado - Dual Wiegand " + String(firmwareVersion));
    
  } else {
    int rc = mqttClient.state();
    Serial.printf(" ❌ Error rc=%d (%lums)\n", rc, elapsed);
    
    // Log del código de error
    switch(rc) {
      case -4: Serial.println("   -> MQTT_CONNECTION_TIMEOUT"); break;
      case -3: Serial.println("   -> MQTT_CONNECTION_LOST"); break;
      case -2: Serial.println("   -> MQTT_CONNECT_FAILED"); break;
      case -1: Serial.println("   -> MQTT_DISCONNECTED"); break;
      case 1:  Serial.println("   -> MQTT_CONNECT_BAD_PROTOCOL"); break;
      case 2:  Serial.println("   -> MQTT_CONNECT_BAD_CLIENT_ID"); break;
      case 3:  Serial.println("   -> MQTT_CONNECT_UNAVAILABLE"); break;
      case 4:  Serial.println("   -> MQTT_CONNECT_BAD_CREDENTIALS"); break;
      case 5:  Serial.println("   -> MQTT_CONNECT_UNAUTHORIZED"); break;
      default: Serial.printf("   -> Código desconocido: %d\n", rc); break;
    }
  }
}
 
 // =================== CALLBACK MQTT ===================
 void mqttCallback(char* topic, byte* payload, unsigned int length) {
   Serial.print("📨 [MQTT] Mensaje recibido [");
   Serial.print(topic);
   Serial.print("]: ");
 
   String message;
   for (unsigned int i = 0; i < length; i++) {
     message += (char)payload[i];
   }
   Serial.println(message);

   // 2048 = mismo tamaño que el buffer de PubSubClient; con 1024 un sync de
   // códigos remotos con franjas horarias podía descartarse en silencio
   DynamicJsonDocument doc(2048);
   DeserializationError error = deserializeJson(doc, message);
   if (error) {
     Serial.print("❌ deserializeJson() falló: ");
     Serial.println(error.c_str());
     return;
   }
 
   String topicStr = String(topic);
 
   if (topicStr.endsWith("/granted")) {
     Serial.println("📋 [MQTT] Validación remota recibida (granted)");
 
     // Soporte para ambos formatos: "access_granted" (antiguo) y "response" (nuevo)
     bool hasOldFormat = doc.containsKey("access_granted");
     bool hasNewFormat = doc.containsKey("response");
     
     if (!hasOldFormat && !hasNewFormat) {
       Serial.println("❌ Mensaje 'granted' inválido: falta campo 'access_granted' o 'response'");
       publishError(7, "Mensaje de validación remota inválido recibido");
       return;
     }

     // Si usa el formato nuevo con "response", delegar a processRemoteValidationResponse
     if (hasNewFormat) {
       Serial.println("🔄 [MQTT] Usando formato nuevo (response)");
       processRemoteValidationResponse(doc);
       return;
     }

     // Procesar formato antiguo con "access_granted"
     Serial.println("🔄 [MQTT] Usando formato antiguo (access_granted)");
     
     if (!doc.containsKey("code_type") || !doc.containsKey("code_value")) {
       Serial.println("❌ Mensaje 'granted' inválido: falta code_type o code_value");
       publishError(7, "Mensaje de validación remota inválido recibido");
       return;
     }
 
     String type = doc["code_type"].as<String>();
     String value = doc["code_value"].as<String>();
     bool granted = doc["access_granted"].as<bool>();
     int duration = doc.containsKey("duration") ? doc["duration"].as<int>() : (int)(releDuration * 1000);
     int relay = doc.containsKey("relay_number") ? doc["relay_number"].as<int>() : 1;
     String reason = doc.containsKey("reason") ? doc["reason"].as<String>() : "";
 
     Serial.printf("📋 Validación remota para código %s (%s): %s\n", 
                   value.c_str(), type.c_str(), granted ? "CONCEDIDO" : "DENEGADO");
 
     if (granted) {
       Serial.printf("✅ Acceso CONCEDIDO remotamente. Activando relé %d por %d ms\n", relay, duration);
       controlReleWithDuration(duration / 1000.0, relay);
       
       // Resetear intentos fallidos en acceso exitoso
       resetFailedAttempts();
       
       // Publicar evento de acceso remoto exitoso
       publishAccessEvent(value, type, lastKeyboardId, true, "REMOTE");
 
      strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
      lastType[sizeof(lastType) - 1] = '\0';
      strncpy(lastCode, value.c_str(), sizeof(lastCode) - 1);
      lastCode[sizeof(lastCode) - 1] = '\0';
      strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
      lastTime[sizeof(lastTime) - 1] = '\0';
     } else {
       Serial.printf("❌ Acceso DENEGADO para código %s. Razón: %s\n", value.c_str(), reason.c_str());
       
       // Incrementar contador de intentos fallidos también para accesos remotos denegados
       failedAttempts++;
       lastFailedAttempt = millis();
       
       // Publicar evento de acceso remoto fallido
       publishFailedAccess(value, type, lastKeyboardId, "REMOTE_DENIED: " + reason);
       
       // Verificar si debe activarse el bloqueo
       if (failedAttempts >= maxFailedAttempts) {
         localAccessBlocked = true;
         blockStartTime = millis();
         
         Serial.printf("🔒 BLOQUEO ACTIVADO tras %d intentos fallidos (incluyendo remotos)\n", maxFailedAttempts);
         publishError(6, "Acceso bloqueado tras " + String(maxFailedAttempts) + " intentos fallidos");
       }
     }
 
     return;
   }
 
   processCommand(doc);
 }
 
 
 // =================== PROCESAMIENTO DE COMANDOS MQTT ===================
 void processCommand(const JsonDocument& doc) {
   unsigned long receivedMessageId = doc["message_id"].as<unsigned long>();
   String receivedDevice = doc["device"].as<String>();
   int messageType = doc["message_type"].as<int>();
   
   // Verificar usando SERIAL FIJO o nombre de dispositivo
   if (receivedDevice != fixedSerialNumber && receivedDevice != deviceName) {
     Serial.println("⚠️ Mensaje no es para este dispositivo");
     return;
   }
   
   // Verificar si es una respuesta para el modo torno
   if (isTurnstileModeEnabled() && pendingRequest.active && doc.containsKey("response")) {
     processMqttResponse(doc);
     return;
   }
   
   // Verificar si es una respuesta de validación remota (modo normal)
   if (!isTurnstileModeEnabled() && doc.containsKey("response")) {
     processRemoteValidationResponse(doc);
     return;
   }
   
   switch (messageType) {
     case 0:  // Activación del relé
       if (doc.containsKey("message_info")) {
         int relayNumber = doc["message_info"]["relay_number"].as<int>();
         float duration = doc["message_info"]["duration"].as<float>();
         if ((relayNumber == 1 || relayNumber == 2) && duration >= 0.5) {
           controlReleWithDuration(duration, relayNumber);
           publishResponse(0, receivedMessageId, "relay " + String(relayNumber) + " activated", relayNumber);
           
           // Publicar evento de apertura remota
           publishAccessEvent("", "REMOTE_COMMAND", 0, true, "MQTT_RELAY");
         }
       }
       break;
       
     case 1:  // Solicitud de información
       publishDeviceInfo(receivedMessageId);
       break;
       
     case 2:  // Comandos de sistema
       if (doc.containsKey("message_info")) {
         String command = doc["message_info"].as<String>();
         
         if (command == "reboot") {
           publishResponse(0, receivedMessageId, "reboot initiated");
           delay(1000);
           ESP.restart();
         } else if (command == "reset") {
           resetToDefault();
           publishResponse(0, receivedMessageId, "reset to default done");
        } else if (command == "update") {
          updateConfiguration(doc);
          publishResponse(0, receivedMessageId, "configuration updated");
        } else if (command == "ota_config") {
          updateOTAConfiguration(doc);
          publishResponse(0, receivedMessageId, "OTA configuration updated");
        } else if (command == "ota_check") {
          checkForUpdates();
          publishResponse(0, receivedMessageId, "OTA check completed");
        }
       }
       break;
       
     case 3:  // Comandos de seguridad
       if (doc.containsKey("message_info")) {
         String securityCommand = doc["message_info"]["security_command"].as<String>();
         
         if (securityCommand == "block_local_access") {
           localAccessBlocked = true;
           blockStartTime = millis();
           saveConfiguration();
           
           Serial.println("🔒 Acceso local BLOQUEADO remotamente");
           publishResponse(0, receivedMessageId, "local access blocked");
           
        } else if (securityCommand == "unblock_local_access") {
          localAccessBlocked = false;
          failedAttempts = 0;
          saveConfiguration();
          
          Serial.println("🔓 Acceso local DESBLOQUEADO remotamente");
          publishResponse(0, receivedMessageId, "local access unblocked");
          
        } else if (securityCommand == "disable_keyboard_reading") {
          keyboardReadingEnabled = false;
          saveConfiguration();
          
          Serial.println("🔒 Lectura de teclados deshabilitada remotamente");
          publishResponse(0, receivedMessageId, "keyboard reading disabled");
          
        } else if (securityCommand == "enable_keyboard_reading") {
          keyboardReadingEnabled = true;
          saveConfiguration();
          
          Serial.println("🔓 Lectura de teclados habilitada remotamente");
          publishResponse(0, receivedMessageId, "keyboard reading enabled");
          
        } else if (securityCommand == "set_block_duration") {
           if (doc["message_info"].containsKey("duration_seconds")) {
             blockDuration = doc["message_info"]["duration_seconds"].as<unsigned long>() * 1000;
             saveConfiguration();
             
             Serial.printf("⏰ Duración de bloqueo actualizada: %lu segundos\n", blockDuration/1000);
             publishResponse(0, receivedMessageId, "block duration updated to " + String(blockDuration/1000) + " seconds");
           }
           
         } else if (securityCommand == "set_max_failed_attempts") {
           if (doc["message_info"].containsKey("max_attempts")) {
             maxFailedAttempts = doc["message_info"]["max_attempts"].as<int>();
             saveConfiguration();
             
             Serial.printf("🚫 Máximo intentos fallidos actualizado: %d\n", maxFailedAttempts);
             publishResponse(0, receivedMessageId, "max failed attempts updated to " + String(maxFailedAttempts));
           }
         }
       }
       break;
       
     case 4:  // Sincronización de tiempo
       if (doc.containsKey("message_info") && doc["message_info"].containsKey("time_string")) {
         String timeString = doc["message_info"]["time_string"].as<String>();
         
         // Actualizar variables de tiempo
         strncpy(currentTimeString, timeString.c_str(), sizeof(currentTimeString) - 1);
        currentTimeString[sizeof(currentTimeString) - 1] = '\0';
         lastTimeSync = millis();
         timeSynced = true;

         // A2v3: fijar también el reloj del sistema y el DS3231 (no-op sin RTC)
         rtcSetFromLocalString(timeString);
         
         Serial.printf("🕐 Hora sincronizada remotamente: %s\n", timeString.c_str());
         publishResponse(0, receivedMessageId, "time synchronized to " + timeString);
         
         // Publicar evento de sincronización de tiempo
         if (mqttClient.connected()) {
           String topic = "swatidhome/" + fixedSerialNumber + "/events";
           String message = "{";
           message += "\"timestamp\":\"" + timeString + "\",";
           message += "\"message_id\":" + String(++messageId) + ",";
           message += "\"device\":\"SWATID_PUERTA\",";
           message += "\"serial\":\"" + fixedSerialNumber + "\",";
           message += "\"event_type\":\"TIME_SYNC\",";
           message += "\"time_string\":\"" + timeString + "\",";
           message +=
             "\"source\":\"MQTT\""
             "}";
           
           mqttClient.publish(topic.c_str(), message.c_str());
           Serial.printf("📡 Evento de sincronización de tiempo publicado\n");
         }
      }
      break;
      
    case 5:  // Gestión de códigos remotos
      if (doc.containsKey("message_info")) {
        String action = doc["message_info"]["action"].as<String>();
        
        if (action == "add_remote_code") {
          // Extraer parámetros del código remoto
          if (!doc["message_info"].containsKey("code_type") || 
              !doc["message_info"].containsKey("code_value") ||
              !doc["message_info"].containsKey("keyboard_id") ||
              !doc["message_info"].containsKey("relay")) {
            Serial.println("❌ Faltan parámetros para add_remote_code");
            publishResponse(1, receivedMessageId, "missing parameters for add_remote_code");
            break;
          }
          
          String codeType = doc["message_info"]["code_type"].as<String>();
          String codeValue = doc["message_info"]["code_value"].as<String>();
          uint8_t keyboardId = doc["message_info"]["keyboard_id"].as<uint8_t>();
          uint8_t relay = doc["message_info"]["relay"].as<uint8_t>();
          
          // Procesar franjas horarias si existen
          TimeSlot timeSlots[4];
          uint8_t timeSlotsCount = 0;
          
          if (doc["message_info"].containsKey("time_slots")) {
            // Iterar sobre el array directamente
            for (int i = 0; i < 4; i++) {
              if (doc["message_info"]["time_slots"][i].isNull()) break;
              
              if (doc["message_info"]["time_slots"][i].containsKey("start_hour") && 
                  doc["message_info"]["time_slots"][i].containsKey("start_minute") &&
                  doc["message_info"]["time_slots"][i].containsKey("end_hour") && 
                  doc["message_info"]["time_slots"][i].containsKey("end_minute") &&
                  doc["message_info"]["time_slots"][i].containsKey("days_of_week")) {
                
                timeSlots[timeSlotsCount].start_hour = doc["message_info"]["time_slots"][i]["start_hour"].as<uint8_t>();
                timeSlots[timeSlotsCount].start_minute = doc["message_info"]["time_slots"][i]["start_minute"].as<uint8_t>();
                timeSlots[timeSlotsCount].end_hour = doc["message_info"]["time_slots"][i]["end_hour"].as<uint8_t>();
                timeSlots[timeSlotsCount].end_minute = doc["message_info"]["time_slots"][i]["end_minute"].as<uint8_t>();
                timeSlots[timeSlotsCount].days_of_week = doc["message_info"]["time_slots"][i]["days_of_week"].as<uint8_t>();
                timeSlotsCount++;
              }
            }
          }
          
          // Añadir el código remoto
          bool added = addRemoteCode(codeType.c_str(), codeValue.c_str(), keyboardId, relay, timeSlots, timeSlotsCount);
          
          if (added) {
            Serial.printf("✅ [MQTT] Código remoto añadido: %s %s\n", codeType.c_str(), codeValue.c_str());
            publishResponse(0, receivedMessageId, "remote code added: " + codeType + " " + codeValue);
          } else {
            Serial.printf("❌ [MQTT] Error añadiendo código remoto: %s %s\n", codeType.c_str(), codeValue.c_str());
            publishResponse(1, receivedMessageId, "failed to add remote code (may already exist or memory full)");
          }
          
        } else if (action == "remove_remote_code") {
          // Eliminar código remoto específico
          if (!doc["message_info"].containsKey("code_type") || 
              !doc["message_info"].containsKey("code_value")) {
            Serial.println("❌ Faltan parámetros para remove_remote_code");
            publishResponse(1, receivedMessageId, "missing parameters for remove_remote_code");
            break;
          }
          
          String codeType = doc["message_info"]["code_type"].as<String>();
          String codeValue = doc["message_info"]["code_value"].as<String>();
          
          bool deleted = deleteRemoteCode(codeType.c_str(), codeValue.c_str());
          
          if (deleted) {
            Serial.printf("✅ [MQTT] Código remoto eliminado: %s %s\n", codeType.c_str(), codeValue.c_str());
            publishResponse(0, receivedMessageId, "remote code removed: " + codeType + " " + codeValue);
          } else {
            Serial.printf("❌ [MQTT] Código remoto no encontrado: %s %s\n", codeType.c_str(), codeValue.c_str());
            publishResponse(1, receivedMessageId, "remote code not found");
          }
          
        } else if (action == "clear_remote_codes") {
          // Eliminar todos los códigos remotos
          deleteAllRemoteCodes();
          Serial.println("✅ [MQTT] Todos los códigos remotos eliminados");
          publishResponse(0, receivedMessageId, "all remote codes cleared");
          
        // ===== GESTIÓN DE CÓDIGOS LOCALES =====
        } else if (action == "add_local_code") {
          // Añadir código local
          if (!doc["message_info"].containsKey("code_type") || 
              !doc["message_info"].containsKey("code_value") ||
              !doc["message_info"].containsKey("relay")) {
            Serial.println("❌ Faltan parámetros para add_local_code");
            publishResponse(1, receivedMessageId, "missing parameters (code_type, code_value, relay required)");
            break;
          }
          
          String codeType = doc["message_info"]["code_type"].as<String>();
          String codeValue = doc["message_info"]["code_value"].as<String>();
          int keyboardId = doc["message_info"].containsKey("keyboard_id") ? 
                          doc["message_info"]["keyboard_id"].as<int>() : 0;
          int relay = doc["message_info"]["relay"].as<int>();
          
          // Validaciones
          bool isValidType = (codeType == "PIN" || codeType == "TAG");
          bool isValidCode = codeValue.length() > 0 && codeValue.length() <= 16;
          bool isValidKeyboard = (keyboardId >= 0 && keyboardId <= 2);
          bool isValidRelay = (relay >= 1 && relay <= 2);
          
          if (!isValidType) {
            publishResponse(1, receivedMessageId, "invalid code_type (must be PIN or TAG)");
          } else if (!isValidCode) {
            publishResponse(1, receivedMessageId, "invalid code_value (1-16 characters)");
          } else if (!isValidKeyboard) {
            publishResponse(1, receivedMessageId, "invalid keyboard_id (0=both, 1, 2)");
          } else if (!isValidRelay) {
            publishResponse(1, receivedMessageId, "invalid relay (1 or 2)");
          } else {
            bool added = addCode(codeType.c_str(), codeValue.c_str(), keyboardId, relay);
            if (added) {
              Serial.printf("✅ [MQTT] Código local añadido: %s %s -> Teclado %d, Relé %d\n", 
                           codeType.c_str(), codeValue.c_str(), keyboardId, relay);
              
              DynamicJsonDocument respDoc(256);
              respDoc["code_type"] = codeType;
              respDoc["code_value"] = codeValue;
              respDoc["keyboard_id"] = keyboardId;
              respDoc["relay"] = relay;
              respDoc["total_codes"] = storedCodes->count;
              String respStr;
              serializeJson(respDoc, respStr);
              publishResponse(0, receivedMessageId, "local code added: " + respStr);
            } else {
              publishResponse(1, receivedMessageId, "failed to add local code (may exist or memory full)");
            }
          }
          
        } else if (action == "remove_local_code") {
          // Eliminar código local específico
          if (!doc["message_info"].containsKey("code_type") || 
              !doc["message_info"].containsKey("code_value")) {
            Serial.println("❌ Faltan parámetros para remove_local_code");
            publishResponse(1, receivedMessageId, "missing parameters (code_type, code_value required)");
            break;
          }
          
          String codeType = doc["message_info"]["code_type"].as<String>();
          String codeValue = doc["message_info"]["code_value"].as<String>();
          int keyboardId = doc["message_info"].containsKey("keyboard_id") ? 
                          doc["message_info"]["keyboard_id"].as<int>() : -1;
          
          // Buscar y eliminar el código
          bool found = false;
          for (int i = 0; i < storedCodes->count; i++) {
            if (strcmp(storedCodes->codes[i].type, codeType.c_str()) == 0 &&
                strcmp(storedCodes->codes[i].value, codeValue.c_str()) == 0 &&
                (keyboardId == -1 || storedCodes->codes[i].keyboard_id == keyboardId)) {
              // Mover el último código a esta posición
              if (i < storedCodes->count - 1) {
                storedCodes->codes[i] = storedCodes->codes[storedCodes->count - 1];
              }
              storedCodes->count--;
              saveStoredCodes();
              found = true;
              Serial.printf("✅ [MQTT] Código local eliminado: %s %s\n", codeType.c_str(), codeValue.c_str());
              publishResponse(0, receivedMessageId, "local code removed: " + codeType + " " + codeValue);
              break;
            }
          }
          
          if (!found) {
            publishResponse(1, receivedMessageId, "local code not found");
          }
          
        } else if (action == "clear_local_codes") {
          // Eliminar todos los códigos locales
          storedCodes->count = 0;
          saveStoredCodes();
          Serial.println("✅ [MQTT] Todos los códigos locales eliminados");
          publishResponse(0, receivedMessageId, "all local codes cleared");
          
        } else if (action == "list_local_codes") {
          // Listar códigos locales
          Serial.println("📋 [MQTT] Listando códigos locales...");
          
          DynamicJsonDocument listDoc(2048);
          listDoc["count"] = storedCodes->count;
          listDoc["max"] = MAX_CODES;
          listDoc["validation_mode"] = storedCodes->localValidationFirst ? "local_first" : "remote_only";
          
          JsonArray codesArray = listDoc.createNestedArray("codes");
          for (int i = 0; i < storedCodes->count && i < 30; i++) {  // Limitar a 30 para no exceder buffer
            JsonObject code = codesArray.createNestedObject();
            code["id"] = i;
            code["type"] = storedCodes->codes[i].type;
            code["value"] = storedCodes->codes[i].value;
            code["keyboard"] = storedCodes->codes[i].keyboard_id;
            code["relay"] = storedCodes->codes[i].relay;
          }
          
          if (storedCodes->count > 30) {
            listDoc["truncated"] = true;
            listDoc["showing"] = 30;
          }
          
          String listStr;
          serializeJson(listDoc, listStr);
          publishResponse(0, receivedMessageId, listStr);
          
        } else {
          Serial.printf("❌ Acción desconocida para message_type 5: %s\n", action.c_str());
          publishResponse(1, receivedMessageId, "unknown action: " + action);
        }
      }
      break;
      
#ifdef ENABLE_BLE
    case 6:  // Gestión de vinculaciones BLE
      if (doc.containsKey("message_info")) {
        String bleAction = doc["message_info"]["action"].as<String>();
        
        if (bleAction == "clear_all") {
          // Limpiar todas las vinculaciones
          Serial.println("🔵 [MQTT] Comando: Limpiar todas las vinculaciones BLE");
          memset(&bleAuthConfig, 0, sizeof(BLEAuthConfig));
          bleAuthConfig.validMarker = BLE_AUTH_CONFIG_MARKER;
          bleAuthConfig.superadmin_registered = 0;
          for (int i = 0; i < BLE_MAX_USERS; i++) {
            bleAuthConfig.user_enabled[i] = 0;
            bleAuthConfig.user_permissions[i] = 0;
          }
          saveBLEAuthConfig();
          bleAuthenticated = false;
          bleCurrentPermissions = 0;
          bleConnectedUser = "";
          publishResponse(0, receivedMessageId, "all BLE bindings cleared");
          
          // Publicar evento de desvinculación para monitorización
          {
            String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
            String eventMsg = String("{") +
              "\"event\":\"BINDINGS_CLEARED\"," +
              "\"type\":\"ALL\"," +
              "\"source\":\"MQTT\"," +
              "\"device\":\"" + fixedSerialNumber + "\"," +
              "\"message_id\":" + String(receivedMessageId) + "," +
              "\"details\":{" +
                "\"superadmin_cleared\":true," +
                "\"users_cleared\":5" +
              "}," +
              "\"timestamp\":\"" + getTimestamp() + "\"" +
            "}";
            mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
          }
          
        } else if (bleAction == "clear_superadmin") {
          // Limpiar solo superadmin
          Serial.println("🔵 [MQTT] Comando: Limpiar superadmin BLE");
          memset(bleAuthConfig.superadmin_key, 0, BLE_KEY_SIZE);
          bleAuthConfig.superadmin_registered = 0;
          saveBLEAuthConfig();
          bleAuthenticated = false;
          bleCurrentPermissions = 0;
          bleConnectedUser = "";
          publishResponse(0, receivedMessageId, "superadmin BLE binding cleared");
          
          // Publicar evento de desvinculación para monitorización
          {
            String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
            String eventMsg = String("{") +
              "\"event\":\"BINDINGS_CLEARED\"," +
              "\"type\":\"SUPERADMIN\"," +
              "\"source\":\"MQTT\"," +
              "\"device\":\"" + fixedSerialNumber + "\"," +
              "\"message_id\":" + String(receivedMessageId) + "," +
              "\"details\":{" +
                "\"action\":\"superadmin_removed\"," +
                "\"awaiting_new_superadmin\":true" +
              "}," +
              "\"timestamp\":\"" + getTimestamp() + "\"" +
            "}";
            mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
          }
          
        } else if (bleAction == "clear_user") {
          // Limpiar usuario específico
          if (doc["message_info"].containsKey("slot")) {
            int slot = doc["message_info"]["slot"].as<int>();
            if (slot >= 1 && slot <= BLE_MAX_USERS) {
              Serial.printf("🔵 [MQTT] Comando: Limpiar usuario BLE %d\n", slot);
              int idx = slot - 1;
              
              // Guardar nombre del usuario antes de eliminarlo
              String userName = String(bleAuthConfig.user_names[idx]);
              if (userName.length() == 0) userName = "Usuario" + String(slot);
              uint8_t oldPermissions = bleAuthConfig.user_permissions[idx];
              
              memset(bleAuthConfig.user_keys[idx], 0, BLE_KEY_SIZE);
              bleAuthConfig.user_enabled[idx] = 0;
              bleAuthConfig.user_permissions[idx] = 0;
              memset(bleAuthConfig.user_names[idx], 0, 16);
              saveBLEAuthConfig();
              publishResponse(0, receivedMessageId, "BLE user " + String(slot) + " cleared");
              
              // Publicar evento de desvinculación para monitorización
              {
                String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
                String eventMsg = String("{") +
                  "\"event\":\"BINDINGS_CLEARED\"," +
                  "\"type\":\"USER\"," +
                  "\"source\":\"MQTT\"," +
                  "\"device\":\"" + fixedSerialNumber + "\"," +
                  "\"message_id\":" + String(receivedMessageId) + "," +
                  "\"details\":{" +
                    "\"slot\":" + String(slot) + "," +
                    "\"user_name\":\"" + userName + "\"," +
                    "\"previous_permissions\":\"0x" + String(oldPermissions, HEX) + "\"" +
                  "}," +
                  "\"timestamp\":\"" + getTimestamp() + "\"" +
                "}";
                mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
              }
            } else {
              publishResponse(1, receivedMessageId, "invalid slot (1-5)");
            }
          } else {
            publishResponse(1, receivedMessageId, "missing slot parameter");
          }
          
        } else if (bleAction == "add_user") {
          // Añadir usuario BLE (solo desde servidor)
          // Formato: { "action": "add_user", "slot": 1-5, "key": "hex64bytes", "name": "nombre", "permissions": 3 }
          if (doc["message_info"].containsKey("slot") && doc["message_info"].containsKey("key")) {
            int slot = doc["message_info"]["slot"].as<int>();
            String keyHex = doc["message_info"]["key"].as<String>();
            String userName = doc["message_info"].containsKey("name") ? 
                              doc["message_info"]["name"].as<String>() : 
                              String("Usuario") + String(slot);
            uint8_t permissions = doc["message_info"].containsKey("permissions") ? 
                                  doc["message_info"]["permissions"].as<int>() : 
                                  (BLE_PERM_RELAY_CONTROL | BLE_PERM_MODE_CHANGE);
            
            if (slot >= 1 && slot <= BLE_MAX_USERS && keyHex.length() == BLE_KEY_SIZE * 2) {
              Serial.printf("🔵 [MQTT] Añadiendo usuario BLE en slot %d: %s\n", slot, userName.c_str());
              
              // Convertir hex a bytes
              int idx = slot - 1;
              for (int i = 0; i < BLE_KEY_SIZE; i++) {
                String byteStr = keyHex.substring(i * 2, i * 2 + 2);
                bleAuthConfig.user_keys[idx][i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
              }
              bleAuthConfig.user_enabled[idx] = 1;
              bleAuthConfig.user_permissions[idx] = permissions;
              strncpy(bleAuthConfig.user_names[idx], userName.c_str(), 15);
              bleAuthConfig.user_names[idx][15] = '\0';
              saveBLEAuthConfig();
              
              // Respuesta
              DynamicJsonDocument respDoc(256);
              respDoc["slot"] = slot;
              respDoc["name"] = userName;
              respDoc["permissions"] = permissions;
              respDoc["enabled"] = true;
              String respStr;
              serializeJson(respDoc, respStr);
              publishResponse(0, receivedMessageId, "user added: " + respStr);
              
              // Evento
              String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
              String eventMsg = String("{") +
                "\"event\":\"USER_ADDED\"," +
                "\"source\":\"MQTT\"," +
                "\"device\":\"" + fixedSerialNumber + "\"," +
                "\"slot\":" + String(slot) + "," +
                "\"name\":\"" + userName + "\"," +
                "\"permissions\":" + String(permissions) + "," +
                "\"timestamp\":\"" + getTimestamp() + "\"" +
              "}";
              mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
            } else {
              publishResponse(1, receivedMessageId, "invalid slot (1-5) or key length (must be 128 hex chars)");
            }
          } else {
            publishResponse(1, receivedMessageId, "missing slot or key parameter");
          }
        
        // =================== v4.1: AÑADIR USUARIO CON DERIVACIÓN SEGURA ===================  
        } else if (bleAction == "add_user_derived") {
          // Añadir usuario BLE con derivación de clave (SEGURO - no transmite clave)
          // La clave se deriva usando HKDF: key = HKDF(device_master_key, serial, "user_key:" + user_id)
          // Formato: { "action": "add_user_derived", "slot": 1-5, "user_id": "unique_id", "name": "nombre", "permissions": 3 }
          
          if (doc["message_info"].containsKey("slot") && doc["message_info"].containsKey("user_id")) {
            int slot = doc["message_info"]["slot"].as<int>();
            String userId = doc["message_info"]["user_id"].as<String>();
            String userName = doc["message_info"].containsKey("name") ? 
                              doc["message_info"]["name"].as<String>() : 
                              String("Usuario") + String(slot);
            uint8_t permissions = doc["message_info"].containsKey("permissions") ? 
                                  doc["message_info"]["permissions"].as<int>() : 
                                  (BLE_PERM_RELAY_CONTROL | BLE_PERM_MODE_CHANGE);
            
            if (slot >= 1 && slot <= BLE_MAX_USERS && userId.length() > 0 && userId.length() <= 64) {
              Serial.printf("🔑 [MQTT] v4.1: Añadiendo usuario con derivación HKDF en slot %d\n", slot);
              Serial.printf("🔑 [MQTT] user_id: %s, name: %s\n", userId.c_str(), userName.c_str());
              
              // Derivar clave usando HKDF
              int idx = slot - 1;
              if (deriveUserKey(userId.c_str(), bleAuthConfig.user_keys[idx])) {
                bleAuthConfig.user_enabled[idx] = 1;
                bleAuthConfig.user_permissions[idx] = permissions;
                strncpy(bleAuthConfig.user_names[idx], userName.c_str(), 15);
                bleAuthConfig.user_names[idx][15] = '\0';
                saveBLEAuthConfig();
                
                // Respuesta (incluye info para que la APP calcule la misma clave)
                DynamicJsonDocument respDoc(512);
                respDoc["slot"] = slot;
                respDoc["user_id"] = userId;
                respDoc["name"] = userName;
                respDoc["permissions"] = permissions;
                respDoc["enabled"] = true;
                respDoc["method"] = "hkdf_derived";
                respDoc["salt"] = fixedSerialNumber;
                respDoc["info_prefix"] = "user_key:";
                String respStr;
                serializeJson(respDoc, respStr);
                publishResponse(0, receivedMessageId, "user added (derived): " + respStr);
                
                Serial.println("🔑 [MQTT] ✓ Usuario añadido con clave derivada HKDF");
                
                // Evento
                String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
                String eventMsg = String("{") +
                  "\"event\":\"USER_ADDED_DERIVED\"," +
                  "\"source\":\"MQTT\"," +
                  "\"device\":\"" + fixedSerialNumber + "\"," +
                  "\"slot\":" + String(slot) + "," +
                  "\"user_id\":\"" + userId + "\"," +
                  "\"name\":\"" + userName + "\"," +
                  "\"permissions\":" + String(permissions) + "," +
                  "\"method\":\"hkdf\"," +
                  "\"timestamp\":\"" + getTimestamp() + "\"" +
                "}";
                mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
              } else {
                publishResponse(1, receivedMessageId, "HKDF derivation failed");
              }
            } else {
              publishResponse(1, receivedMessageId, "invalid slot (1-5) or user_id (1-64 chars)");
            }
          } else {
            publishResponse(1, receivedMessageId, "missing slot or user_id parameter");
          }
          
        } else if (bleAction == "set_superadmin") {
          // Establecer superadmin desde servidor (CUIDADO: sobrescribe el existente)
          // Formato: { "action": "set_superadmin", "key": "hex64bytes" }
          if (doc["message_info"].containsKey("key")) {
            String keyHex = doc["message_info"]["key"].as<String>();
            
            if (keyHex.length() == BLE_KEY_SIZE * 2) {
              Serial.println("🔵 [MQTT] Estableciendo superadmin BLE desde servidor");
              
              // Convertir hex a bytes
              for (int i = 0; i < BLE_KEY_SIZE; i++) {
                String byteStr = keyHex.substring(i * 2, i * 2 + 2);
                bleAuthConfig.superadmin_key[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
              }
              bleAuthConfig.superadmin_registered = 1;
              saveBLEAuthConfig();
              
              publishResponse(0, receivedMessageId, "superadmin key set from server");
              
              // Evento
              String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
              String eventMsg = String("{") +
                "\"event\":\"SUPERADMIN_SET\"," +
                "\"source\":\"MQTT\"," +
                "\"device\":\"" + fixedSerialNumber + "\"," +
                "\"timestamp\":\"" + getTimestamp() + "\"" +
              "}";
              mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
            } else {
              publishResponse(1, receivedMessageId, "invalid key length (must be 128 hex chars for 64 bytes)");
            }
          } else {
            publishResponse(1, receivedMessageId, "missing key parameter");
          }
          
        } else if (bleAction == "get_status") {
          // Obtener estado BLE
          Serial.println("🔵 [MQTT] Comando: Obtener estado BLE");
          DynamicJsonDocument statusDoc(1024);
          statusDoc["name"] = fixedSerialNumber;
          statusDoc["connected"] = bleDeviceConnected;
          statusDoc["authenticated"] = bleAuthenticated;
          statusDoc["connected_user"] = bleConnectedUser;
          statusDoc["superadmin_registered"] = (bool)bleAuthConfig.superadmin_registered;
          
          // Lista detallada de usuarios
          JsonArray usersArray = statusDoc.createNestedArray("users");
          int activeUsers = 0;
          for (int i = 0; i < BLE_MAX_USERS; i++) {
            if (bleAuthConfig.user_enabled[i]) {
              activeUsers++;
              JsonObject user = usersArray.createNestedObject();
              user["slot"] = i + 1;
              user["name"] = bleAuthConfig.user_names[i];
              user["permissions"] = bleAuthConfig.user_permissions[i];
              user["enabled"] = true;
            }
          }
          statusDoc["active_users"] = activeUsers;
          statusDoc["max_users"] = BLE_MAX_USERS;
          
          String statusStr;
          serializeJson(statusDoc, statusStr);
          publishResponse(0, receivedMessageId, statusStr);
          
        } else if (bleAction == "list_users") {
          // Listar usuarios BLE detalladamente
          Serial.println("🔵 [MQTT] Comando: Listar usuarios BLE");
          DynamicJsonDocument usersDoc(1024);
          
          usersDoc["superadmin_registered"] = (bool)bleAuthConfig.superadmin_registered;
          if (bleAuthConfig.superadmin_registered) {
            // Mostrar primeros 8 bytes del hash de la clave (no la clave completa por seguridad)
            char keyPreview[17];
            sprintf(keyPreview, "%02X%02X%02X%02X%02X%02X%02X%02X",
                    bleAuthConfig.superadmin_key[0], bleAuthConfig.superadmin_key[1],
                    bleAuthConfig.superadmin_key[2], bleAuthConfig.superadmin_key[3],
                    bleAuthConfig.superadmin_key[4], bleAuthConfig.superadmin_key[5],
                    bleAuthConfig.superadmin_key[6], bleAuthConfig.superadmin_key[7]);
            usersDoc["superadmin_key_preview"] = String(keyPreview) + "...";
          }
          
          JsonArray usersArray = usersDoc.createNestedArray("users");
          for (int i = 0; i < BLE_MAX_USERS; i++) {
            JsonObject user = usersArray.createNestedObject();
            user["slot"] = i + 1;
            user["enabled"] = (bool)bleAuthConfig.user_enabled[i];
            user["name"] = bleAuthConfig.user_names[i];
            user["permissions"] = bleAuthConfig.user_permissions[i];
            if (bleAuthConfig.user_enabled[i]) {
              char keyPreview[17];
              sprintf(keyPreview, "%02X%02X%02X%02X%02X%02X%02X%02X",
                      bleAuthConfig.user_keys[i][0], bleAuthConfig.user_keys[i][1],
                      bleAuthConfig.user_keys[i][2], bleAuthConfig.user_keys[i][3],
                      bleAuthConfig.user_keys[i][4], bleAuthConfig.user_keys[i][5],
                      bleAuthConfig.user_keys[i][6], bleAuthConfig.user_keys[i][7]);
              user["key_preview"] = String(keyPreview) + "...";
            }
          }
          
          String usersStr;
          serializeJson(usersDoc, usersStr);
          publishResponse(0, receivedMessageId, usersStr);
          
        } else {
          Serial.printf("❌ Acción BLE desconocida: %s\n", bleAction.c_str());
          publishResponse(1, receivedMessageId, "unknown BLE action: " + bleAction);
        }
      }
      break;
#endif // ENABLE_BLE

    case 7:  // Gestión de red (WiFi AP/STA y GSM)
      if (doc.containsKey("message_info")) {
        String netAction = doc["message_info"]["action"].as<String>();
        JsonVariantConst info = doc["message_info"];

        if (netAction == "get_wifi") {
          publishResponse(0, receivedMessageId, wifiStatusJson());

        } else if (netAction == "set_wifi") {
          String result = "wifi updated:";
          if (info.containsKey("ap_mode")) {
            uint32_t to = info.containsKey("ap_timeout_s")
                              ? info["ap_timeout_s"].as<uint32_t>() : 0;
            if (wifiSetApMode(info["ap_mode"].as<uint8_t>(), to ? to : WIFI_AP_WINDOW_S_DEFAULT))
              result += " ap_mode";
            else result += " ap_mode_INVALID";
          }
          if (info.containsKey("ap_pass")) {
            if (wifiSetApPass(info["ap_pass"].as<String>())) result += " ap_pass";
            else result += " ap_pass_INVALID(min8)";
          }
          if (info.containsKey("sta_ssid")) {
            if (wifiStaConnectTo(info["sta_ssid"].as<String>(),
                                 info["sta_pass"].as<String>()))
              result += " sta";
            else result += " sta_INVALID";
          }
          if (info.containsKey("sta_enabled")) {
            wifiStaSetEnabled(info["sta_enabled"].as<bool>());
            result += " sta_enabled";
          }
          if (info.containsKey("forget_sta") && info["forget_sta"].as<bool>()) {
            wifiStaForget();
            result += " sta_forgotten";
          }
          publishResponse(0, receivedMessageId, result);

        } else if (netAction == "ap_start") {
          wifiApStartNow(info["seconds"].as<uint32_t>());
          publishResponse(0, receivedMessageId, "ap started");

        } else if (netAction == "ap_stop") {
          wifiApStopNow();
          publishResponse(0, receivedMessageId, "ap stopped");

        } else if (netAction == "wifi_scan") {
          wifiScanStart();
          publishResponse(0, receivedMessageId, "scan started, request get_wifi_scan in a few seconds");

        } else if (netAction == "get_wifi_scan") {
#if A2_FEATURE_WIFI_MGMT
          publishResponse(0, receivedMessageId, wifiScanJson());
#else
          publishResponse(1, receivedMessageId, "wifi not available");
#endif

        } else if (netAction == "get_gsm") {
          publishResponse(0, receivedMessageId, gsmStatusJson());

        } else if (netAction == "set_gsm") {
          String result = "gsm updated:";
          if (info.containsKey("enabled")) {
            gsmSetEnabled(info["enabled"].as<bool>());
            result += " enabled";
          }
          if (info.containsKey("pin")) {
            // PIN de solo escritura; un intento por valor (política PUK)
            if (gsmSetPin(info["pin"].as<String>())) result += " pin";
            else result += " pin_INVALID";
          }
          if (info.containsKey("pin_disable")) {
            // Desbloquear y desactivar el PIN de la SIM permanentemente
            if (gsmDisableSimPin(info["pin_disable"].as<String>())) result += " pin_disable";
            else result += " pin_disable_INVALID";
          }
          if (info.containsKey("apn")) {
            gsmSetApn(info["apn_mode"] | 0, info["apn"].as<String>(),
                      info["apn_user"].as<String>(), info["apn_pass"].as<String>());
            result += " apn";
          }
          publishResponse(0, receivedMessageId, result);

        } else if (netAction == "gsm_rescan") {
          gsmModemRescan();
          publishResponse(0, receivedMessageId, "gsm rescan started");

        } else if (netAction == "get_rtc") {
          publishResponse(0, receivedMessageId, rtcStatusJson());

        } else {
          publishResponse(1, receivedMessageId, "unknown network action: " + netAction);
        }
      }
      break;
  }
}

// =================== CONTROL DE RELÉS CORREGIDO ===================
 void controlReleWithDuration(float duration, int relay) {
   if (relay < 1 || relay > 2) return;
 
   int relayPin = (relay == 1) ? RELE1_PIN : RELE2_PIN;
 
   Serial.printf("⚡ Activando relé %d por %.1f s\n", relay, duration);
   digitalWrite(relayPin, HIGH);
   releStartTime[relay] = millis();
   releDurations[relay] = duration * 1000;
   releActive[relay] = true;
 
   // Publicar estado de relé ON
   if (mqttClient.connected()) {
     DynamicJsonDocument doc(256);
     doc["timestamp"] = getTimestamp();
     doc["resultado"] = "OK";
     doc["rele"] = relay;
     doc["estado"] = "ON";
     doc["origen"] = "SYSTEM";
     doc["message_id"] = String(messageId++);
     doc["request_id"] = String(responseId++);
     
     String output; 
     serializeJson(doc, output);
     String topic = "swatidhome/" + fixedSerialNumber + "/relay_enable";
     
     bool published = mqttClient.publish(topic.c_str(), output.c_str());
     if (!published) {
       Serial.printf("❌ Error publicando activación relé %d\n", relay);
     }
   }
 }
 
 void checkRelayTimeout() {
   for (int relay = 1; relay <= 2; relay++) {
     if (releActive[relay] && millis() - releStartTime[relay] >= releDurations[relay]) {
       int relayPin = (relay == 1) ? RELE1_PIN : RELE2_PIN;
       digitalWrite(relayPin, LOW);
       releActive[relay] = false;
 
       Serial.printf("⏹️ Relé %d apagado automáticamente\n", relay);
 
       // Publicar estado de relé OFF
       if (mqttClient.connected()) {
         DynamicJsonDocument doc(256);
         doc["timestamp"] = getTimestamp();
         doc["resultado"] = "OK";
         doc["rele"] = relay;
         doc["estado"] = "OFF";
         doc["origen"] = "TIMEOUT";
         doc["message_id"] = String(messageId++);
         doc["request_id"] = String(responseId++);
 
         String output;
         serializeJson(doc, output);
         String topic = "swatidhome/" + fixedSerialNumber + "/relay_enable";
         
         bool published = mqttClient.publish(topic.c_str(), output.c_str());
         if (!published) {
           Serial.printf("❌ Error publicando desactivación relé %d\n", relay);
         }
       }
     }
   }
 }
 
 // =================== SISTEMA DE SEGURIDAD ===================
 void checkAccessBlock() {
   // Verificar si el bloqueo temporal debe levantarse
   if (localAccessBlocked && blockStartTime > 0) {
     if (millis() - blockStartTime >= blockDuration) {
       localAccessBlocked = false;
       blockStartTime = 0;
       Serial.println("🔓 Bloqueo temporal levantado automáticamente");
       
       // Notificar vía MQTT
       publishError(5, "Bloqueo temporal levantado automáticamente tras " + String(blockDuration/1000) + " segundos");
     }
   }
   
   // Resetear contador de intentos fallidos tras timeout
   if (failedAttempts > 0 && (millis() - lastFailedAttempt > failedAttemptTimeout)) {
     resetFailedAttempts();
   }
 }
 
 void resetFailedAttempts() {
   failedAttempts = 0;
   DEBUG_PRINTLN("🔄 Contador de intentos fallidos reseteado");
 }
 
 // =================== PUBLICACIÓN DE EVENTOS MQTT MEJORADA ===================
 void publishAccessEvent(const String& code, const String& type, int keyboardId, bool success, const String& source) {
   if (!mqttClient.connected()) {
     Serial.println("⚠️ MQTT desconectado, no se puede publicar evento de acceso");
     return;
   }
 
   DynamicJsonDocument doc(512);
   doc["timestamp"] = getTimestamp();
   doc["message_id"] = messageId++;
   doc["device"] = deviceName;
   doc["serial"] = fixedSerialNumber;
   doc["event_type"] = "ACCESS_EVENT";
   doc["success"] = success;
   doc["source"] = source;  // LOCAL, REMOTE, WEB, MQTT_RELAY
   doc["code_type"] = type;
   doc["code_value"] = code;
   
   if (keyboardId > 0) {
     doc["keyboard_id"] = keyboardId;
     doc["keyboard_name"] = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
   }
   
   String message;
   serializeJson(doc, message);
   String topic = "swatidhome/events/" + fixedSerialNumber + "/access";
   
   bool published = mqttClient.publish(topic.c_str(), message.c_str());
   if (published) {
     Serial.printf("📤 Evento de acceso publicado: %s - %s\n", success ? "ÉXITO" : "FALLO", source.c_str());
   } else {
     Serial.printf("❌ Error publicando evento de acceso: %s\n", topic.c_str());
   }
 }
 
 void publishFailedAccess(const String& code, const String& type, int keyboardId, const String& reason) {
   if (!mqttClient.connected()) {
     Serial.println("⚠️ MQTT desconectado, no se puede publicar intento fallido");
     return;
   }
 
   DynamicJsonDocument doc(512);
   doc["timestamp"] = getTimestamp();
   doc["message_id"] = messageId++;
   doc["device"] = deviceName;
   doc["serial"] = fixedSerialNumber;
   doc["event_type"] = "FAILED_ACCESS";
   doc["code_type"] = type;
   doc["code_value"] = code;
   doc["reason"] = reason;  // INVALID_CODE, BLOCKED, REMOTE_DENIED
   doc["failed_attempts"] = failedAttempts;
   doc["max_attempts"] = maxFailedAttempts;
   
   if (keyboardId > 0) {
     doc["keyboard_id"] = keyboardId;
     doc["keyboard_name"] = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
   }
   
   String message;
   serializeJson(doc, message);
   String topic = "swatidhome/events/" + fixedSerialNumber + "/failed_access";
   
   bool published = mqttClient.publish(topic.c_str(), message.c_str());
   if (published) {
     Serial.printf("📤 Intento fallido publicado: %s - Intentos: %d/%d\n", reason.c_str(), failedAttempts, maxFailedAttempts);
   } else {
     Serial.printf("❌ Error publicando intento fallido: %s\n", topic.c_str());
   }
 }
 
 void publishError(int errorCode, String description) {
   if (!mqttClient.connected()) {
     Serial.printf("⚠️ MQTT desconectado, error local: %d - %s\n", errorCode, description.c_str());
     return;
   }
 
   DynamicJsonDocument doc(512);
   doc["timestamp"] = getTimestamp();
   doc["message_id"] = messageId++;
   doc["device"] = deviceName;
   doc["serial"] = fixedSerialNumber;
   doc["error_code"] = errorCode;
   doc["description"] = description;
   
   String message;
   serializeJson(doc, message);
   
   String topic = "swatidhome/errors/" + fixedSerialNumber + "/rx";
   bool published = mqttClient.publish(topic.c_str(), message.c_str());
   
   if (published) {
     Serial.printf("📤 Error publicado: %d - %s\n", errorCode, description.c_str());
   } else {
     Serial.printf("❌ Error publicando error: %d - %s\n", errorCode, description.c_str());
   }
 }
 
 void publishResponse(int responseType, unsigned long originalMessageId, String responseInfo, int relay) {
   if (!mqttClient.connected()) {
     Serial.println("⚠️ MQTT desconectado, no se puede publicar respuesta");
     return;
   }
 
   DynamicJsonDocument doc(1024);
   doc["timestamp"] = getTimestamp();
   doc["response_id"] = responseId++;
   doc["message_id"] = originalMessageId;
   doc["device"] = deviceName;
   doc["serial"] = fixedSerialNumber;
   doc["response_type"] = responseType;
   doc["response_info"] = responseInfo;
   if (relay > 0) doc["relay"] = relay;
 
   String message;
   serializeJson(doc, message);
   String topic = "swatidhome/response/" + fixedSerialNumber + "/rx";
   
   bool published = mqttClient.publish(topic.c_str(), message.c_str());
   if (published) {
     Serial.printf("📤 Respuesta publicada: tipo %d\n", responseType);
   } else {
     Serial.printf("❌ Error publicando respuesta: %s\n", topic.c_str());
   }
 }
 
 void publishDeviceInfo(unsigned long originalMessageId) {
   if (!mqttClient.connected()) {
     Serial.println("⚠️ MQTT desconectado, no se puede publicar información del dispositivo");
     return;
   }
 
   DynamicJsonDocument infoDoc(2048);
 
   infoDoc["device"] = deviceName;
   infoDoc["serial"] = fixedSerialNumber;
   infoDoc["ip"] = ip.toString();
   infoDoc["mac"] = ETH.macAddress();
   infoDoc["wifi_signal"] = -1;
   infoDoc["firmware_version"] = firmwareVersion;
   infoDoc["use_dhcp"] = useDhcp;
   infoDoc["relay_duration"] = releDuration;
 
  // Información de seguridad
  JsonObject security = infoDoc.createNestedObject("security");
  security["local_access_blocked"] = localAccessBlocked;
  security["keyboard_reading_enabled"] = keyboardReadingEnabled;
  security["failed_attempts"] = failedAttempts;
  security["max_failed_attempts"] = maxFailedAttempts;
  security["block_duration_seconds"] = blockDuration / 1000;
   if (localAccessBlocked && blockStartTime > 0) {
     // Usar operador ternario en lugar de max()
     unsigned long remaining = (blockDuration > (millis() - blockStartTime)) ? 
       (blockDuration - (millis() - blockStartTime)) / 1000 : 0;
     security["remaining_block_time"] = remaining;
   }
 
   // Información de teclados duales
   JsonObject keyboards = infoDoc.createNestedObject("keyboards");
   keyboards["wiegand1_pins"] = String(WIEGAND1_D0) + "/" + String(WIEGAND1_D1);
   keyboards["wiegand2_pins"] = String(WIEGAND2_D0) + "/" + String(WIEGAND2_D1);
   keyboards["dual_support"] = true;
   keyboards["total_keyboards"] = 2;
 
   if (!useDhcp) {
     infoDoc["static_ip"] = ip.toString();
     infoDoc["static_gateway"] = ETH.gatewayIP().toString();
     infoDoc["static_subnet"] = ETH.subnetMask().toString();
     infoDoc["static_dns"] = ETH.dnsIP().toString();
   }
 
  // Códigos locales
  JsonArray codesArray = infoDoc.createNestedArray("stored_codes");
 if (storedCodes != nullptr) {
   for (int i = 0; i < storedCodes->count; i++) {
     JsonObject code = codesArray.createNestedObject();
     code["type"] = storedCodes->codes[i].type;
     code["value"] = storedCodes->codes[i].value;
     code["keyboard_id"] = storedCodes->codes[i].keyboard_id;
     code["relay"] = storedCodes->codes[i].relay;
   }
 }
 
 // Añadir información de capacidad de códigos locales
 JsonObject localCodesInfo = infoDoc.createNestedObject("local_codes_info");
 localCodesInfo["count"] = storedCodes != nullptr ? storedCodes->count : 0;
 localCodesInfo["max_capacity"] = MAX_CODES;
 localCodesInfo["available"] = storedCodes != nullptr ? (MAX_CODES - storedCodes->count) : MAX_CODES;
 
 // Códigos remotos
 JsonArray remoteCodesArray = infoDoc.createNestedArray("stored_remote_codes");
 if (storedRemoteCodes != nullptr) {
   for (int i = 0; i < storedRemoteCodes->count; i++) {
     JsonObject remoteCode = remoteCodesArray.createNestedObject();
     remoteCode["type"] = storedRemoteCodes->codes[i].type;
     remoteCode["value"] = storedRemoteCodes->codes[i].value;
     remoteCode["keyboard_id"] = storedRemoteCodes->codes[i].keyboard_id;
     remoteCode["relay"] = storedRemoteCodes->codes[i].relay;
     
     // Añadir franjas horarias si existen
     if (storedRemoteCodes->codes[i].time_slots_count > 0) {
       JsonArray timeSlotsArray = remoteCode.createNestedArray("time_slots");
       for (int j = 0; j < storedRemoteCodes->codes[i].time_slots_count; j++) {
         JsonObject slot = timeSlotsArray.createNestedObject();
         slot["start_hour"] = storedRemoteCodes->codes[i].time_slots[j].start_hour;
         slot["start_minute"] = storedRemoteCodes->codes[i].time_slots[j].start_minute;
         slot["end_hour"] = storedRemoteCodes->codes[i].time_slots[j].end_hour;
         slot["end_minute"] = storedRemoteCodes->codes[i].time_slots[j].end_minute;
         slot["days_of_week"] = storedRemoteCodes->codes[i].time_slots[j].days_of_week;
         slot["days_string"] = getDaysOfWeekString(storedRemoteCodes->codes[i].time_slots[j].days_of_week);
       }
     }
   }
 }
 
 // Añadir información de capacidad de códigos remotos
 JsonObject remoteCodesInfo = infoDoc.createNestedObject("remote_codes_info");
 remoteCodesInfo["count"] = storedRemoteCodes != nullptr ? storedRemoteCodes->count : 0;
 remoteCodesInfo["max_capacity"] = MAX_REMOTE_CODES;
 remoteCodesInfo["available"] = storedRemoteCodes != nullptr ? (MAX_REMOTE_CODES - storedRemoteCodes->count) : MAX_REMOTE_CODES;

  String message;
  serializeJson(infoDoc, message);
   String topic = "swatidhome/" + fixedSerialNumber + "/info";
   
   bool published = mqttClient.publish(topic.c_str(), message.c_str());
   if (published) {
     Serial.println("📡 Información del dispositivo dual publicada");
   } else {
     Serial.println("❌ Error publicando información del dispositivo");
   }
 
   if (originalMessageId > 0) {
     publishResponse(1, originalMessageId, message);
   }
 }
 
 // =================== GESTIÓN DE INTERRUPCIONES WIEGAND ===================
 void IRAM_ATTR handleWiegand1D0() {
   wiegand1BitTime = millis();
   if (wiegand1Bits < 32) {
     wiegand1Data = wiegand1Data << 1;
     wiegand1Bits++;
   }
 }
 
 void IRAM_ATTR handleWiegand1D1() {
   wiegand1BitTime = millis();
   if (wiegand1Bits < 32) {
     wiegand1Data = (wiegand1Data << 1) | 1;
     wiegand1Bits++;
   }
 }
 
 void IRAM_ATTR handleWiegand2D0() {
   wiegand2BitTime = millis();
   if (wiegand2Bits < 32) {
     wiegand2Data = wiegand2Data << 1;
     wiegand2Bits++;
   }
 }
 
 void IRAM_ATTR handleWiegand2D1() {
   wiegand2BitTime = millis();
   if (wiegand2Bits < 32) {
     wiegand2Data = (wiegand2Data << 1) | 1;
     wiegand2Bits++;
   }
 }
 
 // =================== PROCESAMIENTO DE DATOS WIEGAND - COMPLETAMENTE REESCRITO ===================
 void processWiegand1Data() {
  unsigned long currentMillis = millis();
  
  if (wiegand1Bits > 0 && (currentMillis - wiegand1BitTime > 25) && !wiegand1Complete) {
    wiegand1Complete = true;
    
    Serial.printf("\n🔐 [TECLADO 1] === DATOS WIEGAND RECIBIDOS ===\n");
    Serial.printf("🔐 [TECLADO 1] Bits: %lu, Valor: 0x%08lX\n", wiegand1Bits, wiegand1Data);
    
    if (wiegand1Bits == 4) {
      // Tecla individual del teclado (4 bits)
      uint8_t key = wiegand1Data & 0x0F;
      Serial.printf("🔑 [TECLADO 1] Tecla detectada: %d\n", key);
      processKey(key, 1);
      
    } else if (wiegand1Bits == 26 || wiegand1Bits == 34) {
      // Tarjeta RFID/NFC (26 o 34 bits)
      String tagCode = String(wiegand1Data & 0x00FFFFFF);
      Serial.printf("🏷️ [TECLADO 1] TAG detectado: %s (%lu bits)\n", tagCode.c_str(), wiegand1Bits);
      
      // VALIDAR EL TAG INMEDIATAMENTE
      validateCode(tagCode, "TAG", 1);
      
    } else if (wiegand1Bits == 8) {
      // Código de 8 bits (posible formato especial)
      String specialCode = String(wiegand1Data & 0xFF);
      Serial.printf("🔸 [TECLADO 1] Código especial 8-bit: %s\n", specialCode.c_str());
      validateCode(specialCode, "TAG", 1);
      
    } else {
      // Formato desconocido
      Serial.printf("❓ [TECLADO 1] Código Wiegand desconocido: %lu bits, valor: 0x%08lX\n", wiegand1Bits, wiegand1Data);
      publishError(2, "Formato Wiegand desconocido en teclado 1: " + String(wiegand1Bits) + " bits");
    }
    
    // Reiniciar para la siguiente lectura
    wiegand1Data = 0;
    wiegand1Bits = 0;
    wiegand1Complete = false;
    Serial.printf("🔐 [TECLADO 1] === PROCESAMIENTO COMPLETADO ===\n\n");
  }
 }
 
 void processWiegand2Data() {
   unsigned long currentMillis = millis();
   
   if (wiegand2Bits > 0 && (currentMillis - wiegand2BitTime > 25) && !wiegand2Complete) {
     wiegand2Complete = true;
     
     Serial.printf("\n🔐 [TECLADO 2] === DATOS WIEGAND RECIBIDOS ===\n");
     Serial.printf("🔐 [TECLADO 2] Bits: %lu, Valor: 0x%08lX\n", wiegand2Bits, wiegand2Data);
     
     if (wiegand2Bits == 4) {
       // Tecla individual del teclado (4 bits)
       uint8_t key = wiegand2Data & 0x0F;
       Serial.printf("🔑 [TECLADO 2] Tecla detectada: %d\n", key);
       processKey(key, 2);
       
     } else if (wiegand2Bits == 26 || wiegand2Bits == 34) {
       // Tarjeta RFID/NFC (26 o 34 bits)
       String tagCode = String(wiegand2Data & 0x00FFFFFF);
       Serial.printf("🏷️ [TECLADO 2] TAG detectado: %s (%lu bits)\n", tagCode.c_str(), wiegand2Bits);
       
       // VALIDAR EL TAG INMEDIATAMENTE
       validateCode(tagCode, "TAG", 2);
       
     } else if (wiegand2Bits == 8) {
       // Código de 8 bits (posible formato especial)
       String specialCode = String(wiegand2Data & 0xFF);
       Serial.printf("🔸 [TECLADO 2] Código especial 8-bit: %s\n", specialCode.c_str());
       validateCode(specialCode, "TAG", 2);
       
     } else {
       // Formato desconocido
       Serial.printf("❓ [TECLADO 2] Código Wiegand desconocido: %lu bits, valor: 0x%08lX\n", wiegand2Bits, wiegand2Data);
       publishError(2, "Formato Wiegand desconocido en teclado 2: " + String(wiegand2Bits) + " bits");
     }
     
     // Reiniciar para la siguiente lectura
     wiegand2Data = 0;
     wiegand2Bits = 0;
     wiegand2Complete = false;
     Serial.printf("🔐 [TECLADO 2] === PROCESAMIENTO COMPLETADO ===\n\n");
   }
 }
 
 // =================== PROCESAMIENTO DE TECLAS - MEJORADO ===================
void processKey(uint8_t key, int keyboardId) {
  char* currentPin = (keyboardId == 1) ? currentPin1 : currentPin2;
  unsigned long* lastKeyPressTime = (keyboardId == 1) ? &lastKeyPressTime1 : &lastKeyPressTime2;
  
  *lastKeyPressTime = millis();
  lastKeyboardId = keyboardId;
  
  char keyChar;
  if (key < 10) {
    keyChar = '0' + key;
  } else if (key == 10) {
    keyChar = '*';
  } else if (key == 11) {
    keyChar = '#';
  } else {
    Serial.printf("❌ [TECLADO %d] Tecla inválida: %d\n", keyboardId, key);
    return;
  }

  Serial.printf("⌨️ [TECLADO %d] Tecla presionada: '%c'\n", keyboardId, keyChar);

 if (keyChar == '#') {
   // Tecla # = confirmar PIN
   int pinLength = strlen(currentPin);
   Serial.printf("🔍 [TECLADO %d] Validando PIN: '%s' (longitud: %d, maxPinLength: %d)\n", 
                 keyboardId, currentPin, pinLength, maxPinLength);
   
   if (pinLength >= 4 && pinLength <= maxPinLength) {
     Serial.printf("✅ [TECLADO %d] PIN completo ingresado: %s (longitud: %d)\n", 
                   keyboardId, currentPin, pinLength);
     
     // VALIDAR EL PIN COMPLETO
     validateCode(String(currentPin), "PIN", keyboardId);
   } else {
     Serial.printf("❌ [TECLADO %d] PIN inválido - Longitud incorrecta: %d (debe ser 4-6 dígitos)\n", 
                   keyboardId, pinLength);
     publishError(2, "PIN con longitud incorrecta ingresado en teclado " + String(keyboardId));
   }
    currentPin[0] = '\0';  // Limpiar PIN siempre después de #
    
  } else if (keyChar == '*') {
    // Tecla * = borrar PIN
    currentPin[0] = '\0';
    Serial.printf("🔄 [TECLADO %d] PIN borrado con tecla *\n", keyboardId);
    
  } else {
    // Dígito numérico 0-9
    int currentLength = strlen(currentPin);
    if (currentLength < maxPinLength) {
      currentPin[currentLength] = keyChar;
      currentPin[currentLength + 1] = '\0';
      Serial.printf("📝 [TECLADO %d] PIN parcial: %s (longitud: %d/%d)\n", 
                    keyboardId, currentPin, currentLength + 1, maxPinLength);
    } else {
      Serial.printf("⚠️ [TECLADO %d] PIN demasiado largo, ignorando dígito '%c'\n", keyboardId, keyChar);
    }
  }
}
 
 // =================== VALIDACIÓN DE CÓDIGOS CORREGIDA ===================
 void validateCode(const String& code, const String& type, int keyboardId) {
   lastKeyboardId = keyboardId;
   String keyboardName = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
   
   Serial.printf("🔍 [%s] Validando: %s (%s)\n", keyboardName.c_str(), code.c_str(), type.c_str());
   
  // Verificar bloqueo de acceso local
  if (localAccessBlocked) {
    Serial.printf("🔒 [%s] Acceso BLOQUEADO - Código rechazado\n", keyboardName.c_str());
    publishFailedAccess(code, type, keyboardId, "BLOCKED");
    return;
  }
  
  // Verificar si la lectura de teclados está habilitada
  if (!keyboardReadingEnabled) {
    Serial.printf("🔒 [%s] Lectura de teclados DESHABILITADA - Código rechazado\n", keyboardName.c_str());
    publishFailedAccess(code, type, keyboardId, "KEYBOARD_READING_DISABLED");
    return;
  }
   
   // Verificar si hay una solicitud pendiente
   if (pendingRequest.active) {
     Serial.printf("⚠️ [%s] Solicitud pendiente activa - Código rechazado\n", keyboardName.c_str());
     publishFailedAccess(code, type, keyboardId, "PENDING_REQUEST_ACTIVE");
     return;
   }
   
  // Verificar modo torno
  if (isTurnstileModeEnabled()) {
    handleTurnstileValidation(code, type, keyboardId);
    return;
  }
  
  // NUEVO: Verificar si estamos en modo lectura de tags
  if (tagReadingActive && type == "TAG") {
    handleTagReading(code, keyboardId);
    return; // No procesar validación normal
  }
  
 int relayToActivate = 1;
  bool localFound = isCodeStored(type.c_str(), code.c_str(), keyboardId, &relayToActivate);
  
  // Verificar también en códigos remotos
  uint8_t remoteRelay = 1;
  bool remoteFound = isRemoteCodeStored(type.c_str(), code.c_str(), keyboardId, &remoteRelay);
  
  if (remoteFound) {
    relayToActivate = remoteRelay;
    localFound = true; // Tratar como código local válido
    Serial.printf("✅ [%s] Código válido REMOTO - Relé %d\n", keyboardName.c_str(), relayToActivate);
  }

  if (storedCodes != nullptr && storedCodes->localValidationFirst && localFound) {
    // Acceso local exitoso (incluyendo códigos remotos)
    Serial.printf("✅ [%s] Código válido LOCAL - Relé %d\n", keyboardName.c_str(), relayToActivate);
    controlReleWithDuration(releDuration, relayToActivate);
    
    // Resetear intentos fallidos
    resetFailedAttempts();
    
    strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
    lastType[sizeof(lastType) - 1] = '\0';
    strncpy(lastCode, code.c_str(), sizeof(lastCode) - 1);
    lastCode[sizeof(lastCode) - 1] = '\0';
    strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
    lastTime[sizeof(lastTime) - 1] = '\0';

    // Publicar evento de acceso local exitoso
    publishAccessEvent(code, type, keyboardId, true, remoteFound ? "REMOTE_LOCAL" : "LOCAL");
    return;
  }
 
   if (storedCodes == nullptr || !storedCodes->localValidationFirst || !localFound) {
     if (mqttClient.connected()) {
       Serial.printf("📡 [%s] Enviando para validación REMOTA\n", keyboardName.c_str());
       
       // Crear mensaje JSON más compacto y garantizar entrega
       DynamicJsonDocument doc(1024);
       doc["timestamp"] = getTimestamp();
       doc["message_id"] = messageId++;
       doc["device"] = fixedSerialNumber;  // Usar serial fijo como device
       doc["device_name"] = deviceName;    // Nombre editable como campo adicional
       doc["message_type"] = 0;
       
       // Información del mensaje de validación
       JsonObject msgInfo = doc.createNestedObject("message_info");
       msgInfo["source"] = "AUTO";
       msgInfo["code_type"] = type;
       msgInfo["code_value"] = code;
       msgInfo["keyboard_id"] = keyboardId;
       msgInfo["keyboard_name"] = keyboardName;
       msgInfo["keyboard_pins"] = (keyboardId == 1) ? 
         String(WIEGAND1_D0) + "/" + String(WIEGAND1_D1) : 
         String(WIEGAND2_D0) + "/" + String(WIEGAND2_D1);
       msgInfo["request_relay"] = keyboardId;  // Relé según teclado origen
       msgInfo["max_duration"] = releDuration;  // Duración máxima permitida
 
       String message;
       serializeJson(doc, message);
       
       // TOPIC CORREGIDO: usar el formato correcto
       String topic = "swatidhome/command/" + fixedSerialNumber + "/access";
       
       Serial.printf("📤 Enviando validación remota:\n");
       Serial.printf("   Tópico: %s\n", topic.c_str());
       Serial.printf("   Tamaño mensaje: %d bytes\n", message.length());
       Serial.printf("   Mensaje: %s\n", message.c_str());
       
      // Intentar publicar UNA vez (sin delays bloqueantes)
      bool published = mqttClient.publish(topic.c_str(), message.c_str(), false);
      
      if (published) {
        Serial.println("✅ Mensaje publicado correctamente");
      } else {
        Serial.println("❌ ERROR: No se pudo publicar mensaje");
        
        // Fallback LOCAL inmediato si el código existe
        if (localFound) {
          Serial.printf("🔄 [%s] Fallback LOCAL de emergencia - Relé %d\n", keyboardName.c_str(), relayToActivate);
          controlReleWithDuration(releDuration, relayToActivate);
          resetFailedAttempts();
          strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
          lastType[sizeof(lastType) - 1] = '\0';
          strncpy(lastCode, code.c_str(), sizeof(lastCode) - 1);
          lastCode[sizeof(lastCode) - 1] = '\0';
          strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
          lastTime[sizeof(lastTime) - 1] = '\0';
          publishAccessEvent(code, type, keyboardId, true, "LOCAL_EMERGENCY");
        } else {
          // Código no existe localmente ni remotamente
          failedAttempts++;
          lastFailedAttempt = millis();
          publishFailedAccess(code, type, keyboardId, "MQTT_PUBLISH_FAILED");
        }
      }
       
      } else if (storedCodes != nullptr && !storedCodes->localValidationFirst && localFound) {
       // Fallback local cuando MQTT no está conectado
       Serial.printf("🔄 [%s] Fallback LOCAL (MQTT desconectado) - Relé %d\n", keyboardName.c_str(), relayToActivate);
       controlReleWithDuration(releDuration, relayToActivate);
       
       resetFailedAttempts();
       strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
       lastType[sizeof(lastType) - 1] = '\0';
       strncpy(lastCode, code.c_str(), sizeof(lastCode) - 1);
       lastCode[sizeof(lastCode) - 1] = '\0';
       strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
       lastTime[sizeof(lastTime) - 1] = '\0';
       
       // Publicar evento de acceso local fallback exitoso
       publishAccessEvent(code, type, keyboardId, true, "LOCAL_FALLBACK");
       return;
       
     } else {
       // Código inválido - incrementar intentos fallidos
       failedAttempts++;
       lastFailedAttempt = millis();
       
       Serial.printf("❌ [%s] Código no válido - Intento %d/%d\n", keyboardName.c_str(), failedAttempts, maxFailedAttempts);
       
       // Publicar intento fallido
       publishFailedAccess(code, type, keyboardId, "INVALID_CODE");
       
       // Verificar si debe activarse el bloqueo
       if (failedAttempts >= maxFailedAttempts) {
         localAccessBlocked = true;
         blockStartTime = millis();
         
         Serial.printf("🔒 BLOQUEO ACTIVADO tras %d intentos fallidos por %lu segundos\n", 
                       maxFailedAttempts, blockDuration/1000);
         
         publishError(6, "Acceso local bloqueado tras " + String(maxFailedAttempts) + " intentos fallidos");
       }
     }
   }
 }
 
// Función de estado del sistema eliminada para reducir tamaño
 
 // =================== PROCESAMIENTO RS485 - MEJORADO ===================
 void processRS485Keypad() {
   static String buf2;
   static unsigned long lastActivity = 0;
   
   while (RS485_Serial.available()) {
     char c = RS485_Serial.read();
     lastActivity = millis();
     
     if (c == '\r' || c == '\n') {
       if (buf2.length() >= 4) {
         Serial.printf("🔐 [RS485] Código recibido: %s (longitud: %d)\n", buf2.c_str(), buf2.length());
         
         // Validar código RS485 como PIN en teclado 2
         validateCode(buf2, "PIN", 2);
       } else {
         Serial.printf("🔐 [RS485] Código demasiado corto: %s (mínimo 4 dígitos)\n", buf2.c_str());
       }
       buf2 = "";
     } else if (isPrintable(c) && buf2.length() < 16) {
       buf2 += c;
       Serial.printf("🔐 [RS485] Carácter recibido: '%c' (buffer: %s)\n", c, buf2.c_str());
     }
   }
   
   // Timeout para limpiar buffer RS485
   if (buf2.length() > 0 && (millis() - lastActivity > 5000)) {
     Serial.printf("🔐 [RS485] Timeout - Limpiando buffer: %s\n", buf2.c_str());
     buf2 = "";
   }
 }
 
 // =================== GESTIÓN DE CONFIGURACIÓN ===================
 void resetToDefault() {
  strncpy(deviceName, "SWATID_DEFAULT", sizeof(deviceName) - 1);
  deviceName[sizeof(deviceName) - 1] = '\0';
  useDhcp = true;
  releDuration = 2.0;
  localAccessBlocked = false;
  keyboardReadingEnabled = true;
  failedAttempts = 0;
  blockDuration = 60000;
  maxFailedAttempts = 3;
   saveConfiguration();
   publishError(4, "Configuración reseteada a valores por defecto");
 }
 
 void updateConfiguration(const JsonDocument& doc) {
   if (doc.containsKey("message_info")) {
     if (doc["message_info"].containsKey("device_name")) {
       String tempName = doc["message_info"]["device_name"].as<String>();
      strncpy(deviceName, tempName.c_str(), sizeof(deviceName) - 1);
      deviceName[sizeof(deviceName) - 1] = '\0';
     }
     if (doc["message_info"].containsKey("relay_duration")) {
       releDuration = doc["message_info"]["relay_duration"].as<float>();
     }
     if (doc["message_info"].containsKey("use_dhcp")) {
       useDhcp = doc["message_info"]["use_dhcp"].as<bool>();
     }
     if (!useDhcp) {
       if (doc["message_info"].containsKey("static_ip")) {
         staticIP.fromString(doc["message_info"]["static_ip"].as<String>());
       }
       if (doc["message_info"].containsKey("static_gateway")) {
         staticGateway.fromString(doc["message_info"]["static_gateway"].as<String>());
       }
       if (doc["message_info"].containsKey("static_subnet")) {
         staticSubnet.fromString(doc["message_info"]["static_subnet"].as<String>());
       }
       if (doc["message_info"].containsKey("static_dns")) {
         staticDns.fromString(doc["message_info"]["static_dns"].as<String>());
       }
     }
     saveConfiguration();
     if (doc["message_info"].containsKey("use_dhcp") || doc["message_info"].containsKey("static_ip")) {
       ESP.restart();
     }
  }
}

void updateOTAConfiguration(const JsonDocument& doc) {
  if (doc.containsKey("message_info")) {
    bool configChanged = false;
    
    // Actualizar URL del servidor
    if (doc["message_info"].containsKey("update_url")) {
      String newUrl = doc["message_info"]["update_url"].as<String>();
      if (newUrl.length() > 0 && newUrl != String(otaConfig.updateUrl)) {
        strncpy(otaConfig.updateUrl, newUrl.c_str(), sizeof(otaConfig.updateUrl) - 1);
        otaConfig.updateUrl[sizeof(otaConfig.updateUrl) - 1] = '\0';
        configChanged = true;
        Serial.printf("🔧 URL de actualizaciones cambiada remotamente a: %s\n", otaConfig.updateUrl);
      }
    }
    
    // Actualizar intervalo de verificación
    if (doc["message_info"].containsKey("check_interval")) {
      int newInterval = doc["message_info"]["check_interval"].as<int>();
      if (newInterval >= 1 && newInterval <= 168 && newInterval != otaConfig.checkInterval) {
        otaConfig.checkInterval = newInterval;
        configChanged = true;
        Serial.printf("🔧 Intervalo de verificación cambiado remotamente a: %d horas\n", otaConfig.checkInterval);
      }
    }
    
    // Actualizar estado de actualización automática
    if (doc["message_info"].containsKey("auto_update_enabled")) {
      bool newAutoUpdate = doc["message_info"]["auto_update_enabled"].as<bool>();
      if (newAutoUpdate != otaConfig.autoUpdateEnabled) {
        otaConfig.autoUpdateEnabled = newAutoUpdate;
        configChanged = true;
        Serial.printf("🔧 Actualización automática %s remotamente\n", 
                      otaConfig.autoUpdateEnabled ? "habilitada" : "deshabilitada");
      }
    }
    
    // Forzar verificación de actualizaciones
    if (doc["message_info"].containsKey("force_check")) {
      bool forceCheck = doc["message_info"]["force_check"].as<bool>();
      if (forceCheck) {
        DEBUG_PRINTLN("🔄 Verificación forzada de actualizaciones solicitada remotamente");
        checkForUpdates();
      }
    }
    
    if (configChanged) {
      saveOTAConfig();
      Serial.println("✅ Configuración OTA actualizada remotamente");
    }
  }
}

 void saveConfiguration() {
   strncpy(config.deviceName, deviceName, sizeof(config.deviceName) - 1);
  config.deviceName[sizeof(config.deviceName) - 1] = '\0';
   fixedSerialNumber.toCharArray(config.fixedSerial, sizeof(config.fixedSerial));
   config.useDhcp = useDhcp;
 
   for (int i = 0; i < 4; i++) {
     config.ip[i] = staticIP[i];
     config.gateway[i] = staticGateway[i];
     config.subnet[i] = staticSubnet[i];
     config.dns[i] = staticDns[i];
   }
 
  config.releDuration = releDuration;
  config.localAccessBlocked = localAccessBlocked;
  config.keyboardReadingEnabled = keyboardReadingEnabled;
  config.blockDuration = blockDuration;
  config.maxFailedAttempts = maxFailedAttempts;
   strncpy(config.webPassword, admin_password, sizeof(config.webPassword));
   config.configValid = 0xABCD1234;
 
   EEPROM.put(0, config);
   EEPROM.commit();
   Serial.println("💾 Configuración guardada");
 }
 
 void loadConfiguration() {
   EEPROM.get(0, config);
 
   if (config.configValid == 0xABCD1234) {
     strncpy(deviceName, config.deviceName, sizeof(deviceName) - 1);
    deviceName[sizeof(deviceName) - 1] = '\0';
     fixedSerialNumber = String(config.fixedSerial);
     useDhcp = config.useDhcp;
 
     for (int i = 0; i < 4; i++) {
       staticIP[i] = config.ip[i];
       staticGateway[i] = config.gateway[i];
       staticSubnet[i] = config.subnet[i];
       staticDns[i] = config.dns[i];
     }
 
    releDuration = config.releDuration;
    localAccessBlocked = config.localAccessBlocked;
    keyboardReadingEnabled = config.keyboardReadingEnabled;
    blockDuration = config.blockDuration;
    maxFailedAttempts = config.maxFailedAttempts;
     strncpy(admin_password, config.webPassword, sizeof(admin_password));
     Serial.println("💾 Configuración cargada desde EEPROM");
  } else {
    // Primera ejecución - generar serial fijo
    fixedSerialNumber = generateFixedSerial();
   strncpy(deviceName, fixedSerialNumber.c_str(), sizeof(deviceName) - 1);
   deviceName[sizeof(deviceName) - 1] = '\0'; // Nombre inicial igual al serial
   useDhcp = true;
   releDuration = 2.0;
   localAccessBlocked = false;
   keyboardReadingEnabled = true;
   failedAttempts = 0;
   blockDuration = 60000;
   maxFailedAttempts = 3;
    strncpy(admin_password, "admin", sizeof(admin_password));
    
    // Modo torno DESHABILITADO por defecto
    config.turnstile.enabled = false;
    config.turnstile.keyboard1_relay = 1;
    config.turnstile.keyboard2_relay = 2;
    memset(config.turnstile.reserved, 0, sizeof(config.turnstile.reserved));
    
    saveConfiguration();
    Serial.println("🔧 Configuración por defecto aplicada (Modo Normal)");
  }
   
  Serial.printf("🆔 Serial fijo: %s\n", fixedSerialNumber.c_str());
  Serial.printf("🏷️ Nombre dispositivo: %s\n", deviceName);
}

// =================== GESTIÓN DEL MODO TORNO ===================
void loadTurnstileConfig() {
  // La configuración del torno se carga junto con la configuración principal
  // Verificar si es la primera vez que se usa el modo torno
  if (!config.turnstile.enabled) {
    // Configuración por defecto del modo torno
    config.turnstile.enabled = false;
    config.turnstile.keyboard1_relay = 1;
    config.turnstile.keyboard2_relay = 2;
    memset(config.turnstile.reserved, 0, sizeof(config.turnstile.reserved));
    
    DEBUG_PRINTLN("🔄 Configuración del modo torno inicializada por defecto");
  } else {
    DEBUG_PRINTLN("🔄 Configuración del modo torno cargada desde EEPROM");
    Serial.printf("   • Modo torno: %s\n", config.turnstile.enabled ? "ACTIVO" : "INACTIVO");
    Serial.printf("   • Teclado 1 → Relé %d\n", config.turnstile.keyboard1_relay);
    Serial.printf("   • Teclado 2 → Relé %d\n", config.turnstile.keyboard2_relay);
  }
}

void saveTurnstileConfig() {
  // La configuración del torno se guarda junto con la configuración principal
  saveConfiguration();
  Serial.println("💾 Configuración del modo torno guardada en EEPROM");
}

void initializeTurnstileMode() {
  // Inicializar modo torno por defecto
  config.turnstile.enabled = false;
  config.turnstile.keyboard1_relay = 1;
  config.turnstile.keyboard2_relay = 2;
  memset(config.turnstile.reserved, 0, sizeof(config.turnstile.reserved));
  
  // Limpiar solicitud pendiente
  clearPendingRequest();
  
  saveTurnstileConfig();
  Serial.println("🔄 Modo torno inicializado con configuración por defecto");
}

void clearPendingRequest() {
  pendingRequest.active = false;
  pendingRequest.keyboard_id = 0;
  memset(pendingRequest.code, 0, sizeof(pendingRequest.code));
  memset(pendingRequest.type, 0, sizeof(pendingRequest.type));
  pendingRequest.timestamp = 0;
  pendingRequest.relay_to_open = 0;
  
  // Limpiar también los PINs en curso para evitar interferencias
  if (strlen(currentPin1) > 0) {
    Serial.printf("🧹 [TECLADO 1] Limpiando PIN residual: '%s'\n", currentPin1);
    currentPin1[0] = '\0';
  }
  if (strlen(currentPin2) > 0) {
    Serial.printf("🧹 [TECLADO 2] Limpiando PIN residual: '%s'\n", currentPin2);
    currentPin2[0] = '\0';
  }
  
  Serial.println("🧹 Solicitud pendiente limpiada");
}

bool isTurnstileModeEnabled() {
  return config.turnstile.enabled;
}

uint8_t getRelayForKeyboard(int keyboardId) {
  if (keyboardId == 1) {
    return config.turnstile.keyboard1_relay;
  } else if (keyboardId == 2) {
    return config.turnstile.keyboard2_relay;
  }
  return 1; // Por defecto relé 1
}

void handleTurnstileValidation(const String& code, const String& type, int keyboardId) {
  String keyboardName = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
  
  Serial.printf("🔄 [TORNO] [%s] Procesando código: %s (%s)\n", keyboardName.c_str(), code.c_str(), type.c_str());
  
  // Determinar el relé que se abrirá según el teclado origen
  uint8_t relayToOpen = getRelayForKeyboard(keyboardId);
  
  // En modo torno, buscar el código en TODOS los keypads (1 y 2)
  int localRelay = 1;
  bool localFound = false;
  int foundKeyboardId = 0;
  
  // Buscar en keypad 1
  if (isCodeStored(type.c_str(), code.c_str(), 1, &localRelay)) {
    localFound = true;
    foundKeyboardId = 1;
    Serial.printf("✅ [TORNO] [%s] Código encontrado en KEYPAD 1 - Relé original: %d\n", keyboardName.c_str(), localRelay);
  }
  // Si no se encuentra en keypad 1, buscar en keypad 2
  else if (isCodeStored(type.c_str(), code.c_str(), 2, &localRelay)) {
    localFound = true;
    foundKeyboardId = 2;
    Serial.printf("✅ [TORNO] [%s] Código encontrado en KEYPAD 2 - Relé original: %d\n", keyboardName.c_str(), localRelay);
  }
  
  // Verificar también en códigos remotos (buscar en todos los keypads)
  uint8_t remoteRelay = 1;
  bool remoteFound = false;
  int foundRemoteKeyboardId = 0;
  
  // Buscar en keypad 1
  if (isRemoteCodeStored(type.c_str(), code.c_str(), 1, &remoteRelay)) {
    remoteFound = true;
    foundRemoteKeyboardId = 1;
    Serial.printf("✅ [TORNO] [%s] Código REMOTO encontrado en KEYPAD 1 - Relé original: %d\n", keyboardName.c_str(), remoteRelay);
  }
  // Si no se encuentra en keypad 1, buscar en keypad 2
  else if (isRemoteCodeStored(type.c_str(), code.c_str(), 2, &remoteRelay)) {
    remoteFound = true;
    foundRemoteKeyboardId = 2;
    Serial.printf("✅ [TORNO] [%s] Código REMOTO encontrado en KEYPAD 2 - Relé original: %d\n", keyboardName.c_str(), remoteRelay);
  }
  
  if (remoteFound) {
    localRelay = remoteRelay;
    localFound = true; // Tratar como código local válido
    foundKeyboardId = foundRemoteKeyboardId;
    Serial.printf("✅ [TORNO] [%s] Código válido REMOTO - Relé original: %d\n", keyboardName.c_str(), remoteRelay);
  }
  
  // MODO TORNO: Respetar configuración de validación (Primero Local vs Primero Remoto)
  if (storedCodes != nullptr && storedCodes->localValidationFirst) {
    // MODO: Primero Local
    if (localFound) {
      // Acceso local exitoso en modo torno (incluyendo códigos remotos)
      Serial.printf("✅ [TORNO] [%s] Código válido LOCAL - Código originalmente para KEYPAD %d, Relé %d\n", 
                    keyboardName.c_str(), foundKeyboardId, localRelay);
      Serial.printf("🔄 [TORNO] [%s] Aplicando configuración torno: KEYPAD %d → Relé %d\n", 
                    keyboardName.c_str(), keyboardId, relayToOpen);
      controlReleWithDuration(releDuration, relayToOpen);
      
      // Resetear intentos fallidos
      resetFailedAttempts();
      
      // Actualizar información de último acceso
      strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
      lastType[sizeof(lastType) - 1] = '\0';
      strncpy(lastCode, code.c_str(), sizeof(lastCode) - 1);
      lastCode[sizeof(lastCode) - 1] = '\0';
      strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
      lastTime[sizeof(lastTime) - 1] = '\0';
      
      // Publicar evento de acceso local exitoso
      publishTurnstileEvent(code, type, keyboardId, true, remoteFound ? "TORNO_REMOTE_LOCAL" : "TORNO_LOCAL");
      return;
    }
    // Si no se encuentra localmente, continuar a validación remota
    Serial.printf("📡 [TORNO] [%s] Código NO encontrado localmente - Enviando a MQTT\n", keyboardName.c_str());
  } else {
    // MODO: Primero Remoto
    Serial.printf("📡 [TORNO] [%s] Modo 'Primero Remoto' - Enviando a MQTT\n", keyboardName.c_str());
  }
  
  // Validación remota (cuando no se encuentra localmente o se prioriza remoto)
  if (mqttClient.connected()) {
    Serial.printf("📡 [TORNO] [%s] Enviando para validación REMOTA\n", keyboardName.c_str());
    
    // Guardar solicitud pendiente
    pendingRequest.active = true;
    pendingRequest.keyboard_id = keyboardId;
    strncpy(pendingRequest.code, code.c_str(), sizeof(pendingRequest.code) - 1);
    pendingRequest.code[sizeof(pendingRequest.code) - 1] = '\0';
    strncpy(pendingRequest.type, type.c_str(), sizeof(pendingRequest.type) - 1);
    pendingRequest.type[sizeof(pendingRequest.type) - 1] = '\0';
    pendingRequest.timestamp = millis();
    pendingRequest.relay_to_open = relayToOpen;
    
    // Crear mensaje JSON optimizado para modo torno
    String message = createTurnstileMqttMessage(code, type, keyboardId, relayToOpen);
    
    // Publicar mensaje
    String topic = "swatidhome/command/" + fixedSerialNumber + "/access";
    
    Serial.printf("📤 [TORNO] Enviando validación remota:\n");
    Serial.printf("   Relé MQTT: 1, Relé real: %d\n", relayToOpen);
    
    bool published = mqttClient.publish(topic.c_str(), message.c_str(), false);
    
    // Log de comunicación
    logTurnstileCommunication("Validación remota enviada", topic, message, published);
    
    if (!published) {
      clearPendingRequest();
      // Si MQTT falla al publicar, usar fallback local si está disponible
      if (localFound) {
        Serial.printf("🔄 [TORNO] [%s] Fallback LOCAL (MQTT falló) - Código originalmente para KEYPAD %d\n", 
                      keyboardName.c_str(), foundKeyboardId);
        Serial.printf("🔄 [TORNO] [%s] Aplicando configuración torno: KEYPAD %d → Relé %d\n", 
                      keyboardName.c_str(), keyboardId, relayToOpen);
        controlReleWithDuration(releDuration, relayToOpen);
        
        resetFailedAttempts();
        strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
        lastType[sizeof(lastType) - 1] = '\0';
        strncpy(lastCode, code.c_str(), sizeof(lastCode) - 1);
        lastCode[sizeof(lastCode) - 1] = '\0';
        strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
        lastTime[sizeof(lastTime) - 1] = '\0';
        
        publishTurnstileEvent(code, type, keyboardId, true, "TORNO_LOCAL_FALLBACK_MQTT_FAILED");
      } else {
        publishTurnstileEvent(code, type, keyboardId, false, "MQTT_PUBLISH_FAILED");
      }
    } else {
      Serial.printf("✅ [TORNO] Mensaje publicado - Esperando respuesta (timeout: %ds)\n", TURNSTILE_TIMEOUT/1000);
    }
    
  } else {
    // MQTT no conectado - fallback local si existe el código
    if (localFound) {
      Serial.printf("🔄 [TORNO] [%s] Fallback LOCAL (MQTT desconectado) - Código originalmente para KEYPAD %d\n", 
                    keyboardName.c_str(), foundKeyboardId);
      Serial.printf("🔄 [TORNO] [%s] Aplicando configuración torno: KEYPAD %d → Relé %d\n", 
                    keyboardName.c_str(), keyboardId, relayToOpen);
      controlReleWithDuration(releDuration, relayToOpen);
      
      resetFailedAttempts();
      strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
      lastType[sizeof(lastType) - 1] = '\0';
      strncpy(lastCode, code.c_str(), sizeof(lastCode) - 1);
      lastCode[sizeof(lastCode) - 1] = '\0';
      strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
      lastTime[sizeof(lastTime) - 1] = '\0';
      
      publishTurnstileEvent(code, type, keyboardId, true, "TORNO_LOCAL_FALLBACK");
    } else {
      // Código inválido - no se encontró ni local ni remoto
      failedAttempts++;
      lastFailedAttempt = millis();
      publishTurnstileEvent(code, type, keyboardId, false, "INVALID_CODE");
      
      Serial.printf("❌ [TORNO] [%s] Código inválido - Intentos fallidos: %d\n", keyboardName.c_str(), failedAttempts);
    }
  }
}

void processMqttResponse(const JsonDocument& doc) {
  if (!isTurnstileModeEnabled() || !pendingRequest.active) {
    return; // No hay solicitud pendiente o modo torno no activo
  }
  
  unsigned long receivedMessageId = doc["message_id"].as<unsigned long>();
  String receivedDevice = doc["device"].as<String>();
  
  // Verificar que el mensaje es para este dispositivo
  if (receivedDevice != fixedSerialNumber && receivedDevice != deviceName) {
    return;
  }
  
  // Verificar que el message_id coincide con la solicitud pendiente
  if (receivedMessageId != messageId - 1) {
    Serial.printf("⚠️ [TORNO] Message ID no coincide: esperado %d, recibido %d\n", messageId - 1, receivedMessageId);
    return;
  }
  
  String keyboardName = (pendingRequest.keyboard_id == 1) ? "WIEGAND1" : "WIEGAND2";
  
  if (doc.containsKey("response")) {
    String response = doc["response"].as<String>();
    
    if (response == "APPROVED") {
      // Acceso aprobado - abrir relé según teclado origen
      Serial.printf("✅ [TORNO] [%s] Acceso APROBADO - Abriendo Relé %d\n", keyboardName.c_str(), pendingRequest.relay_to_open);
      controlReleWithDuration(releDuration, pendingRequest.relay_to_open);
      
      // Resetear intentos fallidos
      resetFailedAttempts();
      
      // Actualizar información de último acceso
      strncpy(lastType, pendingRequest.type, sizeof(lastType) - 1);
      lastType[sizeof(lastType) - 1] = '\0';
      strncpy(lastCode, pendingRequest.code, sizeof(lastCode) - 1);
      lastCode[sizeof(lastCode) - 1] = '\0';
      strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
      lastTime[sizeof(lastTime) - 1] = '\0';
      
      // Publicar evento de acceso remoto exitoso
      publishTurnstileEvent(String(pendingRequest.code), String(pendingRequest.type), pendingRequest.keyboard_id, true, "TORNO_REMOTE");
      
    } else if (response == "DENIED") {
      // Acceso denegado
      String reason = doc.containsKey("reason") ? doc["reason"].as<String>() : "Acceso denegado";
      Serial.printf("❌ [TORNO] [%s] Acceso DENEGADO: %s\n", keyboardName.c_str(), reason.c_str());
      
      // Incrementar intentos fallidos
      failedAttempts++;
      lastFailedAttempt = millis();
      
      // Publicar evento de acceso remoto fallido
      publishTurnstileEvent(String(pendingRequest.code), String(pendingRequest.type), pendingRequest.keyboard_id, false, "TORNO_REMOTE_DENIED: " + reason);
      
    } else {
      Serial.printf("⚠️ [TORNO] [%s] Respuesta desconocida: %s\n", keyboardName.c_str(), response.c_str());
    }
  } else {
    Serial.printf("⚠️ [TORNO] [%s] Mensaje sin campo 'response'\n", keyboardName.c_str());
  }
  
  // Limpiar solicitud pendiente
  clearPendingRequest();
}

void checkPendingRequestTimeout() {
  if (!isTurnstileModeEnabled() || !pendingRequest.active) {
    return; // No hay solicitud pendiente o modo torno no activo
  }
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - pendingRequest.timestamp;
  
  if (elapsedTime > TURNSTILE_TIMEOUT) {
    String keyboardName = (pendingRequest.keyboard_id == 1) ? "WIEGAND1" : "WIEGAND2";
    
    Serial.printf("⏰ [TORNO] [%s] Timeout de solicitud (%lu ms) - Código DENEGADO por timeout\n", 
                  keyboardName.c_str(), elapsedTime);
    
    // CORRECCIÓN: NO abrir relé en caso de timeout - código inválido
    // Incrementar intentos fallidos
    failedAttempts++;
    lastFailedAttempt = millis();
    
    // Publicar evento de acceso fallido por timeout
    publishTurnstileEvent(String(pendingRequest.code), String(pendingRequest.type), pendingRequest.keyboard_id, false, "TORNO_TIMEOUT_DENIED");
    
    // Limpiar solicitud pendiente
    clearPendingRequest();
  }
}

// =================== FUNCIONES ESPECÍFICAS DEL MODO TORNO ===================
String createTurnstileMqttMessage(const String& code, const String& type, int keyboardId, uint8_t relayToOpen) {
  String keyboardName = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
  
  // Crear mensaje JSON optimizado para modo torno
  DynamicJsonDocument doc(1024);
  doc["timestamp"] = getTimestamp();
  doc["message_id"] = messageId++;
  doc["device"] = fixedSerialNumber;
  doc["device_name"] = deviceName;
  doc["message_type"] = 0;
  doc["mode"] = "turnstile";  // Indicar modo torno
  
  // Información del mensaje de validación optimizada
  JsonObject msgInfo = doc.createNestedObject("message_info");
  msgInfo["source"] = "AUTO";
  msgInfo["code_type"] = type;
  msgInfo["code_value"] = code;
  msgInfo["keyboard_id"] = keyboardId;
  msgInfo["keyboard_name"] = keyboardName;
  msgInfo["keyboard_pins"] = (keyboardId == 1) ? 
    String(WIEGAND1_D0) + "/" + String(WIEGAND1_D1) : 
    String(WIEGAND2_D0) + "/" + String(WIEGAND2_D1);
  msgInfo["request_relay"] = 1;  // Siempre relé 1 para MQTT en modo torno
  msgInfo["actual_relay"] = relayToOpen;  // Relé real que se abrirá
  msgInfo["max_duration"] = releDuration;
  
  // Campos específicos del modo torno
  JsonObject turnstileInfo = doc.createNestedObject("turnstile_info");
  turnstileInfo["enabled"] = true;
  turnstileInfo["keyboard1_relay"] = config.turnstile.keyboard1_relay;
  turnstileInfo["keyboard2_relay"] = config.turnstile.keyboard2_relay;
  turnstileInfo["timeout"] = TURNSTILE_TIMEOUT;
  turnstileInfo["pending_request"] = pendingRequest.active;
  
  String message;
  serializeJson(doc, message);
  
  return message;
}

void publishTurnstileEvent(const String& code, const String& type, int keyboardId, bool success, const String& reason) {
  if (!mqttClient.connected()) {
    Serial.println("⚠️ [TORNO] MQTT no conectado - No se puede publicar evento");
    return;
  }
  
  String keyboardName = (keyboardId == 1) ? "WIEGAND1" : "WIEGAND2";
  String eventType = success ? "ACCESS_GRANTED" : "ACCESS_DENIED";
  
  // Crear mensaje de evento optimizado
  DynamicJsonDocument doc(1024);
  doc["timestamp"] = getTimestamp();
  doc["message_id"] = messageId++;
  doc["device"] = fixedSerialNumber;
  doc["device_name"] = deviceName;
  doc["message_type"] = 1;  // Tipo evento
  doc["mode"] = "turnstile";
  doc["event_type"] = eventType;
  
  // Información del evento
  JsonObject eventInfo = doc.createNestedObject("event_info");
  eventInfo["code_type"] = type;
  eventInfo["code_value"] = code;
  eventInfo["keyboard_id"] = keyboardId;
  eventInfo["keyboard_name"] = keyboardName;
  eventInfo["success"] = success;
  eventInfo["reason"] = reason;
  eventInfo["relay_opened"] = success ? getRelayForKeyboard(keyboardId) : 0;
  eventInfo["duration"] = success ? releDuration : 0;
  
  // Información del modo torno
  JsonObject turnstileInfo = doc.createNestedObject("turnstile_info");
  turnstileInfo["enabled"] = true;
  turnstileInfo["keyboard1_relay"] = config.turnstile.keyboard1_relay;
  turnstileInfo["keyboard2_relay"] = config.turnstile.keyboard2_relay;
  turnstileInfo["timeout"] = TURNSTILE_TIMEOUT;
  
  String message;
  serializeJson(doc, message);
  
  // Publicar evento
  String topic = "swatidhome/event/" + fixedSerialNumber + "/turnstile";
  
  Serial.printf("📤 [TORNO] Publicando evento %s:\n", eventType.c_str());
  
  bool published = mqttClient.publish(topic.c_str(), message.c_str(), false);
  
  // Log de comunicación
  logTurnstileCommunication("Evento " + eventType + " publicado", topic, message, published);
  
  if (!published) {
    Serial.println("❌ [TORNO] ERROR: No se pudo publicar evento");
  } else {
    Serial.printf("✅ [TORNO] Evento %s publicado correctamente\n", eventType.c_str());
  }
}

void logTurnstileCommunication(const String& action, const String& topic, const String& message, bool success) {
  String status = success ? "✅" : "❌";
  String timestamp = getTimeString();
  
  Serial.printf("%s [TORNO] [%s] %s:\n", status.c_str(), timestamp.c_str(), action.c_str());
  Serial.printf("   Tópico: %s\n", topic.c_str());
  Serial.printf("   Tamaño: %d bytes\n", message.length());
  Serial.printf("   Estado: %s\n", success ? "EXITOSO" : "FALLIDO");
  
  if (!success) {
    Serial.printf("   Mensaje: %s\n", message.c_str());
  }
  
  // Log detallado solo en modo debug
  #ifdef DEBUG_TURNSTILE
  Serial.printf("   Mensaje completo: %s\n", message.c_str());
  #endif
}

void processRemoteValidationResponse(const JsonDocument& doc) {
  if (doc.containsKey("response")) {
    String response = doc["response"].as<String>();
    unsigned long receivedMessageId = doc["message_id"].as<unsigned long>();
    
    Serial.printf("📨 [MQTT] Respuesta de validación remota recibida: %s (Message ID: %lu)\n", response.c_str(), receivedMessageId);
    
    if (response == "APPROVED") {
      // Obtener información del mensaje de respuesta
      String code = doc.containsKey("code_value") ? doc["code_value"].as<String>() : "";
      String type = doc.containsKey("code_type") ? doc["code_type"].as<String>() : "PIN";
      int relayToOpen = doc.containsKey("relay_number") ? doc["relay_number"].as<int>() : 1;
      float duration = doc.containsKey("duration") ? doc["duration"].as<float>() : releDuration;
      String reason = doc.containsKey("reason") ? doc["reason"].as<String>() : "Valid code";
      
      // Validar que el relé es válido
      if (relayToOpen < 1 || relayToOpen > 2) {
        Serial.printf("⚠️ [MQTT] Relé inválido en respuesta: %d, usando relé 1\n", relayToOpen);
        relayToOpen = 1;
      }
      
      Serial.printf("✅ [MQTT] Acceso APROBADO para código %s (%s)\n", code.c_str(), type.c_str());
      Serial.printf("   • Relé a abrir: %d\n", relayToOpen);
      Serial.printf("   • Duración: %.1f segundos\n", duration);
      Serial.printf("   • Razón: %s\n", reason.c_str());
      
      // Abrir el relé con la duración especificada
      controlReleWithDuration(duration, relayToOpen);
      
      // Resetear intentos fallidos
      resetFailedAttempts();
      
      // Actualizar información de último acceso
      if (code.length() > 0) {
        strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
        lastType[sizeof(lastType) - 1] = '\0';
        strncpy(lastCode, code.c_str(), sizeof(lastCode) - 1);
        lastCode[sizeof(lastCode) - 1] = '\0';
        strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
        lastTime[sizeof(lastTime) - 1] = '\0';
        lastKeyboardId = relayToOpen; // Usar el relé como indicador del teclado
        
        // Publicar evento de acceso remoto exitoso
        publishAccessEvent(code, type, relayToOpen, true, "REMOTE_APPROVED");
      }
      
    } else if (response == "DENIED") {
      // Obtener información del mensaje de respuesta
      String code = doc.containsKey("code_value") ? doc["code_value"].as<String>() : "";
      String type = doc.containsKey("code_type") ? doc["code_type"].as<String>() : "PIN";
      String reason = doc.containsKey("reason") ? doc["reason"].as<String>() : "Acceso denegado";
      
      Serial.printf("❌ [MQTT] Acceso DENEGADO para código %s (%s)\n", code.c_str(), type.c_str());
      Serial.printf("   • Razón: %s\n", reason.c_str());
      
      // Incrementar intentos fallidos
      failedAttempts++;
      lastFailedAttempt = millis();
      
      // Publicar evento de acceso remoto fallido
      if (code.length() > 0) {
        publishFailedAccess(code, type, lastKeyboardId, "REMOTE_DENIED: " + reason);
      }
    }
  } else {
    Serial.println("⚠️ [MQTT] Respuesta de validación no contiene campo 'response'");
  }
}

// =================== FUNCIONES DE ENTRADAS DIGITALES ===================

// Cargar configuración de entradas digitales desde EEPROM
// (ver digital_inputs.h)

// =================== HANDLERS DE CONFIGURACIÓN DEL MODO TORNO ===================
void handleTurnstileConfig() {
  if (!webAuth()) return;
  
  if (server.hasArg("turnstile_mode") && server.hasArg("keyboard1_relay") && server.hasArg("keyboard2_relay")) {
    String turnstile_mode = server.arg("turnstile_mode");
    bool enabled = (turnstile_mode == "turnstile");
    int keyboard1_relay = server.arg("keyboard1_relay").toInt();
    int keyboard2_relay = server.arg("keyboard2_relay").toInt();
    
    // Validar parámetros
    if (keyboard1_relay < 1 || keyboard1_relay > 2 || keyboard2_relay < 1 || keyboard2_relay > 2) {
      server.send(400, "text/html", 
        "<html><body><h1>❌ Error: Parámetros inválidos</h1>"
        "<p>Los relés deben ser 1 o 2.</p>"
        "<a href='/'>Volver al inicio</a></body></html>");
      return;
    }
    
    // Solo validar mismo relé si el modo torno está activo
    if (enabled && keyboard1_relay == keyboard2_relay) {
      server.send(400, "text/html", 
        "<html><body><h1>❌ Error: Configuración inválida</h1>"
        "<p>En modo torno, los teclados no pueden controlar el mismo relé.</p>"
        "<a href='/'>Volver al inicio</a></body></html>");
      return;
    }
    
    // Actualizar configuración
    bool wasEnabled = config.turnstile.enabled;
    config.turnstile.enabled = enabled;
    config.turnstile.keyboard1_relay = keyboard1_relay;
    config.turnstile.keyboard2_relay = keyboard2_relay;
    
    // Limpiar solicitud pendiente si se desactiva el modo torno
    if (wasEnabled && !enabled) {
      clearPendingRequest();
      Serial.println("🔄 Modo torno desactivado - Solicitud pendiente limpiada");
      
      // También limpiar PINs en curso para evitar interferencias en modo normal
      if (strlen(currentPin1) > 0) {
        Serial.printf("🧹 [TECLADO 1] Limpiando PIN al desactivar torno: '%s'\n", currentPin1);
        currentPin1[0] = '\0';
      }
      if (strlen(currentPin2) > 0) {
        Serial.printf("🧹 [TECLADO 2] Limpiando PIN al desactivar torno: '%s'\n", currentPin2);
        currentPin2[0] = '\0';
      }
    }
    
    // Guardar configuración
    saveTurnstileConfig();
    
    Serial.printf("🔄 Configuración del modo torno actualizada:\n");
    Serial.printf("   • Modo torno: %s\n", enabled ? "ACTIVO" : "INACTIVO");
    Serial.printf("   • Teclado 1 → Relé %d\n", keyboard1_relay);
    Serial.printf("   • Teclado 2 → Relé %d\n", keyboard2_relay);
    
    // Redirigir al inicio
    server.sendHeader("Location", "/");
    server.send(303);
    
  } else {
    server.send(400, "text/html", 
      "<html><body><h1>❌ Error: Faltan parámetros</h1>"
      "<p>Se requieren todos los parámetros de configuración.</p>"
      "<a href='/'>Volver al inicio</a></body></html>");
  }
}

void handleTurnstileReset() {
  if (!webAuth()) return;
  
  if (isTurnstileModeEnabled() && pendingRequest.active) {
    String keyboardName = (pendingRequest.keyboard_id == 1) ? "Teclado 1" : "Teclado 2";
    
    Serial.printf("🔄 Solicitud pendiente cancelada manualmente:\n");
    Serial.printf("   • Código: %s (%s)\n", pendingRequest.code, pendingRequest.type);
    Serial.printf("   • Teclado: %s\n", keyboardName.c_str());
    Serial.printf("   • Relé: %d\n", pendingRequest.relay_to_open);
    
    // Limpiar solicitud pendiente
    clearPendingRequest();
    
    // Redirigir al inicio
    server.sendHeader("Location", "/");
    server.send(303);
    
  } else {
    server.send(400, "text/html", 
      "<html><body><h1>❌ Error: No hay solicitud pendiente</h1>"
      "<p>No hay ninguna solicitud pendiente para cancelar.</p>"
      "<a href='/'>Volver al inicio</a></body></html>");
  }
}

void handleExportCodes() {
  if (!webAuth()) return;
  
  Serial.println("📊 Exportando códigos locales a CSV");
  
  // Crear CSV con headers
  String csv = "Tipo,Codigo,Teclado,Rele,Fecha_Creacion\n";
  
  // Añadir códigos locales
  if (storedCodes == nullptr) return;
  
  for (int i = 0; i < storedCodes->count; i++) {
    CodeEntry& entry = storedCodes->codes[i];
    csv += String(entry.type) + ",";
    csv += String(entry.value) + ",";
    csv += String(entry.keyboard_id) + ",";
    csv += String(entry.relay) + ",";
    csv += String(millis()) + "\n";  // Usar timestamp actual
  }
  
  // Configurar headers para descarga
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=codigos_locales_" + String(millis()) + ".csv");
  server.send(200, "text/csv", csv);
  
  Serial.printf("✅ Exportados %d códigos locales a CSV\n", storedCodes->count);
}

void handleExportRemoteCodes() {
  if (!webAuth()) return;
  
  Serial.println("📊 Exportando códigos remotos a CSV");
  
  // Crear CSV con headers
  String csv = "Tipo,Codigo,Teclado,Rele,Franjas_Horarias,Fecha_Creacion\n";
  
  // Añadir códigos remotos
  if (storedRemoteCodes == nullptr) return;
  
  for (int i = 0; i < storedRemoteCodes->count; i++) {
    RemoteCodeEntry& entry = storedRemoteCodes->codes[i];
    csv += String(entry.type) + ",";
    csv += String(entry.value) + ",";
    csv += String(entry.keyboard_id) + ",";
    csv += String(entry.relay) + ",";
    
    // Añadir información de franjas horarias
    String timeSlots = "";
    for (int j = 0; j < entry.time_slots_count; j++) {
      if (j > 0) timeSlots += "; ";
      TimeSlot& slot = entry.time_slots[j];
      timeSlots += String(slot.start_hour) + ":" + 
                   (slot.start_minute < 10 ? "0" : "") + String(slot.start_minute) + "-" +
                   String(slot.end_hour) + ":" + 
                   (slot.end_minute < 10 ? "0" : "") + String(slot.end_minute);
    }
    csv += "\"" + timeSlots + "\",";
    csv += "Remoto\n";
  }
  
  // Configurar headers para descarga
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=codigos_remotos_" + String(millis()) + ".csv");
  server.send(200, "text/csv", csv);
  
  Serial.printf("✅ Exportados %d códigos remotos a CSV\n", storedRemoteCodes->count);
}

void handleImportCodes() {
  if (!webAuth()) return;
  
  Serial.println("📥 Procesando importación de códigos desde CSV");
  
  // Verificar si se envió un archivo
  if (!server.hasArg("csvFile") || server.arg("csvFile").length() == 0) {
    server.send(400, "text/html", 
      "<html><head><meta charset='UTF-8'></head><body><h1>❌ Error: No se envió archivo CSV</h1>"
      "<p>Por favor, seleccione un archivo CSV válido.</p>"
      "<a href='/codes'>Volver a códigos</a></body></html>");
    return;
  }
  
  String csvContent = server.arg("csvFile");
  Serial.printf("📊 Contenido CSV recibido: %d caracteres\n", csvContent.length());
  
  // Procesar CSV
  int importedCount = 0;
  int errorCount = 0;
  String errors = "";
  
  // Dividir en líneas
  int lineStart = 0;
  int lineEnd = csvContent.indexOf('\n');
  bool isFirstLine = true;
  
  while (lineEnd >= 0) {
    String line = csvContent.substring(lineStart, lineEnd);
    line.trim();
    
    // Saltar línea de headers
    if (isFirstLine) {
      isFirstLine = false;
      lineStart = lineEnd + 1;
      lineEnd = csvContent.indexOf('\n', lineStart);
      continue;
    }
    
    // Procesar línea de datos
    if (line.length() > 0) {
      // Dividir por comas
      int fieldStart = 0;
      int fieldEnd = line.indexOf(',');
      String fields[5];
      int fieldIndex = 0;
      
      while (fieldEnd >= 0 && fieldIndex < 5) {
        fields[fieldIndex] = line.substring(fieldStart, fieldEnd);
        fields[fieldIndex].trim();
        fieldStart = fieldEnd + 1;
        fieldEnd = line.indexOf(',', fieldStart);
        fieldIndex++;
      }
      
      // Último campo
      if (fieldIndex < 5) {
        fields[fieldIndex] = line.substring(fieldStart);
        fields[fieldIndex].trim();
        fieldIndex++;
      }
      
      // Validar campos mínimos
      if (fieldIndex >= 4) {
        String type = fields[0];
        String code = fields[1];
        int keyboardId = fields[2].toInt();
        int relay = fields[3].toInt();
        
        // Validar tipo
        if (type != "PIN" && type != "TAG") {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Tipo inválido '" + type + "'\n";
          errorCount++;
        }
        // Validar código
        else if (code.length() == 0) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Código vacío\n";
          errorCount++;
        }
        // Validar teclado (solo 1 y 2 son válidos)
        else if (keyboardId < 1 || keyboardId > 2) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Teclado inválido '" + String(keyboardId) + "' (debe ser 1 o 2)\n";
          errorCount++;
        }
        // Validar relé
        else if (relay < 1 || relay > 2) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Relé inválido '" + String(relay) + "'\n";
          errorCount++;
        }
        // Intentar añadir código
        else {
          if (addCode(type.c_str(), code.c_str(), keyboardId, relay)) {
            importedCount++;
            Serial.printf("✅ Importado: %s %s (Teclado %d, Relé %d)\n", 
                         type.c_str(), code.c_str(), keyboardId, relay);
          } else {
            errors += "Línea " + String(importedCount + errorCount + 1) + ": Error al añadir código '" + code + "'\n";
            errorCount++;
          }
        }
      } else {
        errors += "Línea " + String(importedCount + errorCount + 1) + ": Formato inválido (faltan campos)\n";
        errorCount++;
      }
    }
    
    lineStart = lineEnd + 1;
    lineEnd = csvContent.indexOf('\n', lineStart);
  }
  
  // Generar respuesta
  String response = "<html><head><meta charset='UTF-8'></head><body><h1>📥 Resultado de Importación</h1>";
  response += "<p><strong>Códigos importados:</strong> " + String(importedCount) + "</p>";
  response += "<p><strong>Errores:</strong> " + String(errorCount) + "</p>";
  
  if (errorCount > 0) {
    response += "<h3>❌ Errores encontrados:</h3>";
    response += "<pre>" + errors + "</pre>";
  }
  
  response += "<a href='/codes'><button>Volver a códigos</button></a></body></html>";
  
  server.send(200, "text/html", response);
  
  Serial.printf("📥 Importación completada: %d códigos importados, %d errores\n", importedCount, errorCount);
}

// =================== IMPORTACIÓN MASIVA DE CÓDIGOS ===================
String csvUploadContent = "";

void handleFileUpload() {
  if (!webAuth()) return;
  
  // Verificar si hay un archivo en el upload
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("📁 Iniciando upload: %s\n", upload.filename.c_str());
    csvUploadContent = "";
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Añadir datos al contenido
    for (int i = 0; i < upload.currentSize; i++) {
      csvUploadContent += (char)upload.buf[i];
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("📁 Upload completado: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
    Serial.printf("📁 Contenido final: %d caracteres\n", csvUploadContent.length());
    Serial.printf("📁 Primeros 100 chars: %s\n", csvUploadContent.substring(0, min(100, (int)csvUploadContent.length())).c_str());
    // Procesar el CSV y enviar respuesta
    processCSVImport(csvUploadContent);
  } else {
    // Si no hay archivo, verificar si se envió por otros medios
    if (!server.hasArg("csvFile") || server.arg("csvFile").length() == 0) {
      server.send(400, "text/html", 
        "<html><head><meta charset='UTF-8'></head><body><h1>❌ Error: No se envió archivo CSV</h1>"
        "<p>Por favor, seleccione un archivo CSV válido.</p>"
        "<a href='/codes'>Volver a códigos</a></body></html>");
    }
  }
}

void processCSVImport(String csvContent) {
  Serial.printf("📊 Procesando CSV: %d caracteres\n", csvContent.length());
  Serial.printf("📊 Contenido CSV (primeros 200 chars): %s\n", csvContent.substring(0, min(200, (int)csvContent.length())).c_str());
  
  // Procesar CSV
  int importedCount = 0;
  int errorCount = 0;
  String errors = "";
  
  // Dividir en líneas
  int lineStart = 0;
  int lineEnd = csvContent.indexOf('\n');
  bool isFirstLine = true;
  
  while (lineEnd >= 0) {
    String line = csvContent.substring(lineStart, lineEnd);
    line.trim();
    
    // Saltar línea de headers
    if (isFirstLine) {
      isFirstLine = false;
      lineStart = lineEnd + 1;
      lineEnd = csvContent.indexOf('\n', lineStart);
      continue;
    }
    
    // Procesar línea de datos
    if (line.length() > 0) {
      // Dividir por comas
      int fieldStart = 0;
      int fieldEnd = line.indexOf(',');
      String fields[5];
      int fieldIndex = 0;
      
      while (fieldEnd >= 0 && fieldIndex < 5) {
        fields[fieldIndex] = line.substring(fieldStart, fieldEnd);
        fields[fieldIndex].trim();
        fieldStart = fieldEnd + 1;
        fieldEnd = line.indexOf(',', fieldStart);
        fieldIndex++;
      }
      
      // Último campo
      if (fieldIndex < 5) {
        fields[fieldIndex] = line.substring(fieldStart);
        fields[fieldIndex].trim();
        fieldIndex++;
      }
      
      // Validar campos mínimos (todos obligatorios)
      if (fieldIndex >= 4) {
        String type = fields[0];
        String code = fields[1];
        int keyboardId = fields[2].toInt();
        int relay = fields[3].toInt();
        
        // Validar tipo
        if (type != "PIN" && type != "TAG") {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Tipo inválido '" + type + "'\n";
          errorCount++;
        }
        // Validar código según tipo
        else if (type == "PIN" && (code.length() < 4 || code.length() > 6)) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": PIN debe tener 4-6 dígitos\n";
          errorCount++;
        }
        else if (type == "TAG" && (code.length() < 1 || code.length() > 16)) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": TAG debe tener 1-16 caracteres\n";
          errorCount++;
        }
        // Validar teclado (solo 1 y 2 son válidos)
        else if (keyboardId < 1 || keyboardId > 2) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Teclado inválido '" + String(keyboardId) + "' (debe ser 1 o 2)\n";
          errorCount++;
        }
        // Validar relé
        else if (relay < 1 || relay > 2) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Relé inválido '" + String(relay) + "'\n";
          errorCount++;
        }
        // Intentar añadir código
        else {
          if (addCode(type.c_str(), code.c_str(), keyboardId, relay)) {
            importedCount++;
            Serial.printf("✅ Importado: %s %s (Teclado %d, Relé %d)\n", 
                         type.c_str(), code.c_str(), keyboardId, relay);
          } else {
            errors += "Línea " + String(importedCount + errorCount + 1) + ": Error al añadir código '" + code + "'\n";
            errorCount++;
          }
        }
      } else {
        errors += "Línea " + String(importedCount + errorCount + 1) + ": Formato inválido (faltan campos)\n";
        errorCount++;
      }
    }
    
    lineStart = lineEnd + 1;
    lineEnd = csvContent.indexOf('\n', lineStart);
  }
  
  // Procesar última línea si no termina con \n
  if (lineStart < csvContent.length()) {
    String line = csvContent.substring(lineStart);
    line.trim();
    
    // Procesar línea de datos (saltar si es header)
    if (line.length() > 0 && !isFirstLine) {
      // Dividir por comas
      int fieldStart = 0;
      int fieldEnd = line.indexOf(',');
      String fields[5];
      int fieldIndex = 0;
      
      while (fieldEnd >= 0 && fieldIndex < 5) {
        fields[fieldIndex] = line.substring(fieldStart, fieldEnd);
        fields[fieldIndex].trim();
        fieldStart = fieldEnd + 1;
        fieldEnd = line.indexOf(',', fieldStart);
        fieldIndex++;
      }
      
      // Último campo
      if (fieldIndex < 5) {
        fields[fieldIndex] = line.substring(fieldStart);
        fields[fieldIndex].trim();
        fieldIndex++;
      }
      
      // Validar campos mínimos (todos obligatorios)
      if (fieldIndex >= 4) {
        String type = fields[0];
        String code = fields[1];
        int keyboardId = fields[2].toInt();
        int relay = fields[3].toInt();
        
        // Validar tipo
        if (type != "PIN" && type != "TAG") {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Tipo inválido '" + type + "'\n";
          errorCount++;
        }
        // Validar código según tipo
        else if (type == "PIN" && (code.length() < 4 || code.length() > 6)) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": PIN debe tener 4-6 dígitos\n";
          errorCount++;
        }
        else if (type == "TAG" && (code.length() < 1 || code.length() > 16)) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": TAG debe tener 1-16 caracteres\n";
          errorCount++;
        }
        // Validar teclado (solo 1 y 2 son válidos)
        else if (keyboardId < 1 || keyboardId > 2) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Teclado inválido '" + String(keyboardId) + "' (debe ser 1 o 2)\n";
          errorCount++;
        }
        // Validar relé
        else if (relay < 1 || relay > 2) {
          errors += "Línea " + String(importedCount + errorCount + 1) + ": Relé inválido '" + String(relay) + "'\n";
          errorCount++;
        }
        // Intentar añadir código
        else {
          if (addCode(type.c_str(), code.c_str(), keyboardId, relay)) {
            importedCount++;
            Serial.printf("✅ Importado: %s %s (Teclado %d, Relé %d)\n", 
                         type.c_str(), code.c_str(), keyboardId, relay);
          } else {
            errors += "Línea " + String(importedCount + errorCount + 1) + ": Error al añadir código '" + code + "'\n";
            errorCount++;
          }
        }
      } else {
        errors += "Línea " + String(importedCount + errorCount + 1) + ": Formato inválido (faltan campos)\n";
        errorCount++;
      }
    }
  }
  
  // Generar respuesta
  String response = "<html><head><meta charset='UTF-8'></head><body><h1>📥 Resultado de Importación Masiva</h1>";
  response += "<p><strong>Códigos importados:</strong> " + String(importedCount) + "</p>";
  response += "<p><strong>Errores:</strong> " + String(errorCount) + "</p>";
  
  if (errorCount > 0) {
    response += "<h3>❌ Errores encontrados:</h3>";
    response += "<pre>" + errors + "</pre>";
  }
  
  response += "<a href='/codes'><button>Volver a códigos</button></a></body></html>";
  
  server.send(200, "text/html", response);
  
  Serial.printf("📥 Importación masiva completada: %d códigos importados, %d errores\n", importedCount, errorCount);
}

void handleBulkImport() {
  if (!webAuth()) return;
  
  Serial.println("📥 Procesando importación masiva de códigos desde CSV");
  
  // Verificar si hay un archivo en el upload
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("📁 Iniciando upload: %s\n", upload.filename.c_str());
    csvUploadContent = "";
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Añadir datos al contenido
    for (int i = 0; i < upload.currentSize; i++) {
      csvUploadContent += (char)upload.buf[i];
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("📁 Upload completado: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
    Serial.printf("📁 Contenido final: %d caracteres\n", csvUploadContent.length());
    Serial.printf("📁 Primeros 100 chars: %s\n", csvUploadContent.substring(0, min(100, (int)csvUploadContent.length())).c_str());
    
    // Procesar el CSV y enviar respuesta
    processCSVImportWithResponse(csvUploadContent);
  }
}

void processCSVImportWithResponse(String csvContent) {
  
  // Procesar CSV directamente con logging detallado
  Serial.println("📊 === INICIANDO PROCESAMIENTO CSV ===");
  
  int importedCount = 0;
  int errorCount = 0;
  int tagCount = 0;
  int pinCount = 0;
  String errors = "";
  
  // Dividir en líneas
  int lineStart = 0;
  int lineEnd = csvContent.indexOf('\n');
  bool isFirstLine = true;
  int lineNumber = 0;
  
  while (lineEnd >= 0) {
    lineNumber++;
    String line = csvContent.substring(lineStart, lineEnd);
    line.trim();
    
    Serial.printf("📊 Línea %d: '%s'\n", lineNumber, line.c_str());
    
    // Saltar línea de headers
    if (isFirstLine) {
      Serial.println("📊 Saltando línea de headers");
      isFirstLine = false;
      lineStart = lineEnd + 1;
      lineEnd = csvContent.indexOf('\n', lineStart);
      continue;
    }
    
    // Procesar línea de datos
    if (line.length() > 0) {
      Serial.printf("📊 Procesando datos línea %d: '%s'\n", lineNumber, line.c_str());
      
      // Dividir por comas
      int fieldStart = 0;
      int fieldEnd = line.indexOf(',');
      String fields[5];
      int fieldIndex = 0;
      
      while (fieldEnd >= 0 && fieldIndex < 5) {
        fields[fieldIndex] = line.substring(fieldStart, fieldEnd);
        fields[fieldIndex].trim();
        Serial.printf("📊 Campo %d: '%s'\n", fieldIndex, fields[fieldIndex].c_str());
        fieldStart = fieldEnd + 1;
        fieldEnd = line.indexOf(',', fieldStart);
        fieldIndex++;
      }
      
      // Último campo
      if (fieldIndex < 5) {
        fields[fieldIndex] = line.substring(fieldStart);
        fields[fieldIndex].trim();
        Serial.printf("📊 Campo %d: '%s'\n", fieldIndex, fields[fieldIndex].c_str());
        fieldIndex++;
      }
      
      Serial.printf("📊 Total campos encontrados: %d\n", fieldIndex);
      
      // Validar campos mínimos
      if (fieldIndex >= 4) {
        String type = fields[0];
        String code = fields[1];
        int keyboardId = fields[2].toInt();
        int relay = fields[3].toInt();
        
        Serial.printf("📊 Datos: Tipo='%s', Código='%s', Teclado=%d, Relé=%d\n", 
                     type.c_str(), code.c_str(), keyboardId, relay);
        
        // Validaciones básicas
        bool isValidType = false;
        bool isValidCode = false;
        
        if (type == "TAG" && code.length() >= 1 && code.length() <= 16) {
          isValidType = true;
          isValidCode = true;
          Serial.printf("📊 Validación TAG: OK (longitud: %d)\n", code.length());
        } else if (type == "PIN" && code.length() >= 4 && code.length() <= 6) {
          isValidType = true;
          isValidCode = true;
          Serial.printf("📊 Validación PIN: OK (longitud: %d)\n", code.length());
        } else {
          Serial.printf("📊 Validación falló: Tipo='%s', Longitud=%d\n", type.c_str(), code.length());
        }
        
        if (isValidType && isValidCode && 
            keyboardId >= 1 && keyboardId <= 2 && relay >= 1 && relay <= 2) {
          
          Serial.printf("📊 Validación OK, intentando añadir código...\n");
          
          if (addCode(type.c_str(), code.c_str(), keyboardId, relay)) {
            importedCount++;
            if (type == "TAG") {
              tagCount++;
            } else if (type == "PIN") {
              pinCount++;
            }
            Serial.printf("✅ IMPORTADO: %s %s (Teclado %d, Relé %d)\n", 
                         type.c_str(), code.c_str(), keyboardId, relay);
          } else {
            errorCount++;
            Serial.printf("❌ ERROR al añadir código: %s %s\n", type.c_str(), code.c_str());
            errors += "Línea " + String(lineNumber) + ": Error al añadir código '" + code + "'\n";
          }
        } else {
          errorCount++;
          Serial.printf("❌ VALIDACIÓN FALLÓ: Tipo='%s', Código='%s' (len=%d), Teclado=%d, Relé=%d\n", 
                       type.c_str(), code.c_str(), code.length(), keyboardId, relay);
          errors += "Línea " + String(lineNumber) + ": Validación falló\n";
        }
      } else {
        errorCount++;
        Serial.printf("❌ CAMPOS INSUFICIENTES: %d campos encontrados\n", fieldIndex);
        errors += "Línea " + String(lineNumber) + ": Campos insuficientes\n";
      }
    }
    
    lineStart = lineEnd + 1;
    lineEnd = csvContent.indexOf('\n', lineStart);
  }
  
  Serial.printf("📊 === PROCESAMIENTO COMPLETADO ===\n");
  Serial.printf("📊 Total códigos importados: %d\n", importedCount);
  Serial.printf("📊 TAGs importados: %d\n", tagCount);
  Serial.printf("📊 PINs importados: %d\n", pinCount);
  Serial.printf("📊 Errores: %d\n", errorCount);
  
  // Generar respuesta detallada
  String response = webPageBegin("Importación CSV - Controladora A2");
  response +=
    "<div class='container'>"
    "<h1>📥 Resultado de Importación CSV</h1>";
  
  if (importedCount > 0) {
    response +=
      "<div class='summary'>"
      "<h2 class='success'>✅ Importación Exitosa</h2>";
    response += "<p><strong>Total de códigos importados:</strong> <span class='success'>" + String(importedCount) + "</span></p>";
    response += "<p><strong>TAGs importados:</strong> <span class='info'>" + String(tagCount) + "</span></p>";
    response += "<p><strong>PINs importados:</strong> <span class='info'>" + String(pinCount) + "</span></p>";
    response += "</div>";
  }
  
  if (errorCount > 0) {
    response += "<div class='summary'>";
    response += "<h3 class='error'>❌ Errores encontrados: " + String(errorCount) + "</h3>";
    response += "<pre style='background:#f8f9fa;padding:10px;border-radius:5px;overflow-x:auto;'>" + errors + "</pre>";
    response += "</div>";
  }
  
  if (importedCount == 0 && errorCount == 0) {
    response +=
      "<div class='summary'>"
      "<h3 class='error'>❌ No se procesaron códigos</h3>"
      "<p>El archivo CSV no contenía datos válidos para importar.</p>"
      "</div>";
  }
  
  response +=
    "<div style='text-align:center;margin-top:20px;'>"
    "<a href='/codes'><button>📋 Volver a Códigos</button></a>"
    "<a href='/codes/template'><button>📄 Descargar Plantilla</button></a>"
    "</div>"
    "</div></body></html>";
  
  server.send(200, "text/html", response);
}

void handleCSVTemplate() {
  if (!webAuth()) return;
  
  Serial.println("📄 Generando plantilla CSV");
  
  // Crear plantilla CSV con ejemplos
  String csvTemplate = "Tipo,Codigo,Teclado,Rele,Fecha_Creacion\n";
  csvTemplate += "TAG,1234567890,1,1," + String(millis()) + "\n";
  csvTemplate += "TAG,0987654321,1,2," + String(millis() + 1000) + "\n";
  csvTemplate += "TAG,1122334455,2,1," + String(millis() + 2000) + "\n";
  csvTemplate += "TAG,5566778899,2,2," + String(millis() + 3000) + "\n";
  csvTemplate += "PIN,1234,1,1," + String(millis() + 4000) + "\n";
  csvTemplate += "PIN,5678,2,2," + String(millis() + 5000) + "\n";
  
  // Configurar headers para descarga
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=plantilla_codigos.csv");
  server.send(200, "text/csv", csvTemplate);
  
  Serial.println("📄 Plantilla CSV enviada");
}

// =================== LECTURA DE TAGS EN TIEMPO REAL ===================
void handleStartTagReading() {
  if (!webAuth()) return;
  
  Serial.println("📖 Iniciando modo de lectura de tags");
  
  // Parsear configuración JSON
  String body = server.arg("plain");
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"JSON inválido\"}");
    return;
  }
  
  // Configurar modo de lectura
  tagReadingConfig.keyboard1_relay1 = doc["k1r1"].as<int>();
  tagReadingConfig.keyboard1_relay2 = doc["k1r2"].as<int>();
  tagReadingConfig.keyboard2_relay1 = doc["k2r1"].as<int>();
  tagReadingConfig.keyboard2_relay2 = doc["k2r2"].as<int>();
  tagReadingConfig.saveToMemory = doc["saveToMemory"].as<bool>();
  
  // Validar configuración
  bool hasConfig = (tagReadingConfig.keyboard1_relay1 > 0 || tagReadingConfig.keyboard1_relay2 > 0 ||
                   tagReadingConfig.keyboard2_relay1 > 0 || tagReadingConfig.keyboard2_relay2 > 0);
  
  if (!hasConfig) {
    server.send(400, "application/json", "{\"error\":\"Debe configurar al menos un acceso\"}");
    return;
  }
  
  // Limpiar lista anterior
  readTags.clear();
  
  // Activar modo de lectura
  tagReadingActive = true;
  tagReadingConfig.enabled = true;
  
  Serial.printf("📖 Configuración de lectura:\n");
  Serial.printf("   K1->R1: %d, K1->R2: %d\n", tagReadingConfig.keyboard1_relay1, tagReadingConfig.keyboard1_relay2);
  Serial.printf("   K2->R1: %d, K2->R2: %d\n", tagReadingConfig.keyboard2_relay1, tagReadingConfig.keyboard2_relay2);
  Serial.printf("   Guardar en memoria: %s\n", tagReadingConfig.saveToMemory ? "Sí" : "No");
  
  server.send(200, "application/json", "{\"status\":\"started\",\"message\":\"Modo de lectura activado\"}");
}

void handleStopTagReading() {
  if (!webAuth()) return;
  
  Serial.println("📖 Deteniendo modo de lectura de tags");
  
  // Desactivar modo de lectura
  tagReadingActive = false;
  tagReadingConfig.enabled = false;
  
  Serial.printf("📖 Lectura finalizada. Tags leídos: %d\n", readTags.size());
  
  server.send(200, "application/json", "{\"status\":\"stopped\",\"count\":" + String(readTags.size()) + "}");
}

void handleReadTagsStatus() {
  if (!webAuth()) return;
  
  // Crear JSON con estado actual
  DynamicJsonDocument doc(2048);
  doc["active"] = tagReadingActive;
  doc["count"] = readTags.size();
  doc["saveToMemory"] = tagReadingConfig.saveToMemory;
  
  JsonArray tags = doc.createNestedArray("tags");
  for (int i = 0; i < readTags.size(); i++) {
    JsonObject tag = tags.createNestedObject();
    tag["code"] = readTags[i].code;
    tag["timestamp"] = readTags[i].timestamp;
    tag["saved"] = readTags[i].saved;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleExportReadTags() {
  if (!webAuth()) return;
  
  Serial.println("📊 Exportando tags leídos a CSV");
  
  // Crear CSV con headers (formato compatible con importación)
  String csv = "Tipo,Codigo,Teclado,Rele,Fecha_Creacion\n";
  
  // Añadir tags leídos
  for (int i = 0; i < readTags.size(); i++) {
    ReadTag& tag = readTags[i];
    
    // Determinar configuración aplicada (usar la primera configuración activa)
    int keyboardId = 0;
    int relayId = 1;
    
    if (tagReadingConfig.keyboard1_relay1 > 0) {
      keyboardId = 1;
      relayId = tagReadingConfig.keyboard1_relay1;
    } else if (tagReadingConfig.keyboard1_relay2 > 0) {
      keyboardId = 1;
      relayId = tagReadingConfig.keyboard1_relay2;
    } else if (tagReadingConfig.keyboard2_relay1 > 0) {
      keyboardId = 2;
      relayId = tagReadingConfig.keyboard2_relay1;
    } else if (tagReadingConfig.keyboard2_relay2 > 0) {
      keyboardId = 2;
      relayId = tagReadingConfig.keyboard2_relay2;
    }
    
    // Generar fecha de creación (timestamp en formato legible)
    String fechaCreacion = String(tag.timestamp);
    
    csv += "TAG,"; // Tipo siempre TAG para lectura de tags
    csv += tag.code + ",";
    csv += String(keyboardId) + ",";
    csv += String(relayId) + ",";
    csv += fechaCreacion + "\n";
  }
  
  // Configurar headers para descarga
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=tags_leidos_" + String(millis()) + ".csv");
  server.send(200, "text/csv", csv);
  
  Serial.printf("✅ Exportados %d tags leídos a CSV\n", readTags.size());
}

void handleLoadReadTags() {
  if (!webAuth()) return;
  
  Serial.println("💾 Cargando tags leídos a memoria");
  
  int loadedCount = 0;
  int errorCount = 0;
  String errors = "";
  
  // Procesar cada tag leído
  for (int i = 0; i < readTags.size(); i++) {
    ReadTag& tag = readTags[i];
    
    // Aplicar configuración según teclado (usar la primera configuración activa)
    bool tagLoaded = false;
    
    // Teclado 1 - Relé 1
    if (tagReadingConfig.keyboard1_relay1 > 0) {
      if (addCode("TAG", tag.code.c_str(), 1, tagReadingConfig.keyboard1_relay1)) {
        loadedCount++;
        tagLoaded = true;
        Serial.printf("✅ Tag cargado: %s (Teclado 1, Relé %d)\n", tag.code.c_str(), tagReadingConfig.keyboard1_relay1);
      } else {
        errorCount++;
        errors += "Error al cargar " + tag.code + " (K1->R" + String(tagReadingConfig.keyboard1_relay1) + ")\n";
      }
    }
    
    // Teclado 1 - Relé 2
    if (tagReadingConfig.keyboard1_relay2 > 0) {
      if (addCode("TAG", tag.code.c_str(), 1, tagReadingConfig.keyboard1_relay2)) {
        loadedCount++;
        tagLoaded = true;
        Serial.printf("✅ Tag cargado: %s (Teclado 1, Relé %d)\n", tag.code.c_str(), tagReadingConfig.keyboard1_relay2);
      } else {
        errorCount++;
        errors += "Error al cargar " + tag.code + " (K1->R" + String(tagReadingConfig.keyboard1_relay2) + ")\n";
      }
    }
    
    // Teclado 2 - Relé 1
    if (tagReadingConfig.keyboard2_relay1 > 0) {
      if (addCode("TAG", tag.code.c_str(), 2, tagReadingConfig.keyboard2_relay1)) {
        loadedCount++;
        tagLoaded = true;
        Serial.printf("✅ Tag cargado: %s (Teclado 2, Relé %d)\n", tag.code.c_str(), tagReadingConfig.keyboard2_relay1);
      } else {
        errorCount++;
        errors += "Error al cargar " + tag.code + " (K2->R" + String(tagReadingConfig.keyboard2_relay1) + ")\n";
      }
    }
    
    // Teclado 2 - Relé 2
    if (tagReadingConfig.keyboard2_relay2 > 0) {
      if (addCode("TAG", tag.code.c_str(), 2, tagReadingConfig.keyboard2_relay2)) {
        loadedCount++;
        tagLoaded = true;
        Serial.printf("✅ Tag cargado: %s (Teclado 2, Relé %d)\n", tag.code.c_str(), tagReadingConfig.keyboard2_relay2);
      } else {
        errorCount++;
        errors += "Error al cargar " + tag.code + " (K2->R" + String(tagReadingConfig.keyboard2_relay2) + ")\n";
      }
    }
    
    // Marcar tag como guardado si se cargó al menos una configuración
    if (tagLoaded) {
      tag.saved = true;
    }
  }
  
  // Generar respuesta JSON
  DynamicJsonDocument doc(1024);
  doc["success"] = (errorCount == 0);
  doc["loaded"] = loadedCount;
  doc["errors"] = errorCount;
  doc["total"] = readTags.size();
  
  if (errorCount > 0) {
    doc["error_details"] = errors;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.printf("💾 Carga completada: %d configuraciones cargadas, %d errores\n", loadedCount, errorCount);
}

void handleTagReading(const String& code, int keyboardId) {
  // Verificar si el tag ya fue leído
  for (int i = 0; i < readTags.size(); i++) {
    if (readTags[i].code == code) {
      Serial.printf("🔄 Tag ya leído: %s\n", code.c_str());
      return;
    }
  }
  
  // Añadir a lista de tags leídos
  ReadTag newTag;
  newTag.code = code;
  newTag.timestamp = millis();
  newTag.saved = false;
  readTags.push_back(newTag);
  
  // Guardar en memoria si está configurado
  if (tagReadingConfig.saveToMemory) {
    // Aplicar configuración según teclado
    if (keyboardId == 1 || keyboardId == 0) {
      if (tagReadingConfig.keyboard1_relay1 > 0) {
        addCode("TAG", code.c_str(), 1, tagReadingConfig.keyboard1_relay1);
      }
      if (tagReadingConfig.keyboard1_relay2 > 0) {
        addCode("TAG", code.c_str(), 1, tagReadingConfig.keyboard1_relay2);
      }
    }
    if (keyboardId == 2 || keyboardId == 0) {
      if (tagReadingConfig.keyboard2_relay1 > 0) {
        addCode("TAG", code.c_str(), 2, tagReadingConfig.keyboard2_relay1);
      }
      if (tagReadingConfig.keyboard2_relay2 > 0) {
        addCode("TAG", code.c_str(), 2, tagReadingConfig.keyboard2_relay2);
      }
    }
    newTag.saved = true;
  }
  
  Serial.printf("📖 Tag leído: %s (Teclado %d)\n", code.c_str(), keyboardId);
}

void handleTimeSync() {
  if (!webAuth()) return;
  
  Serial.println("🕐 Accediendo a sincronización de hora");
  
  String html = webPageBegin("Sincronización de Tiempo - SWATID-A2");
  
  html +=
    "<div class='container'>"
    "<h1>🕐 Sincronización de Tiempo</h1>";
  
  // Mostrar hora actual del dispositivo
  html +=
    "<div class='time-display'>"
    "<h3>📱 Hora del Dispositivo</h3>";
  html += "<p class='device-time'>" + String(currentTimeString) + "</p>";
  html += "<p><strong>Estado:</strong> " + String(timeSynced ? "✅ Sincronizado" : "❌ No sincronizado") + "</p>";
  if (timeSynced) {
    html += "<p><strong>Última sincronización:</strong> " + String((millis() - lastTimeSync) / 1000) + " segundos</p>";
  }
  html += "</div>";
  
  // Mostrar hora del navegador
  html +=
    "<div class='time-display'>"
    "<h3>🌐 Hora del Navegador</h3>"
    "<p class='browser-time' id='browserTime'>Cargando...</p>"
    "<p><strong>Zona horaria:</strong> <span id='timezone'></span></p>"
    "</div>";
  
  // Formulario oculto para sincronización
  html +=
    "<div id='syncForm' class='hidden'>"
    "<form action='/time/update' method='post' id='timeForm'>"
    "<div class='form-group'>"
    "<label for='currentTime'>Hora a sincronizar:</label>"
    "<input type='text' id='currentTime' name='currentTime' readonly>"
    "</div>"
    "</form>"
    "</div>";
  
  // Botones de acción
  html +=
    "<div class='button-group'>"
    "<button class='sync' onclick='syncTime()'>🔄 Sincronizar con Navegador</button>"
    "<button class='save' onclick='saveTime()' id='saveBtn' disabled>💾 Guardar Cambios</button>"
    "<button class='back' onclick='window.location.href=\"/\"'>🏠 Volver al Inicio</button>"
    "</div>";
  
  html += "</div>";
  
  // JavaScript mejorado
  html +=
    "<script>"
    "let browserTimeString = '';"
    "let isTimeSynced = false;";
  
  html +=
    "function updateBrowserTime() {"
    "  const now = new Date();"
    "  const year = now.getFullYear();"
    "  const month = String(now.getMonth() + 1).padStart(2, '0');"
    "  const day = String(now.getDate()).padStart(2, '0');"
    "  const hours = String(now.getHours()).padStart(2, '0');"
    "  const minutes = String(now.getMinutes()).padStart(2, '0');"
    "  const seconds = String(now.getSeconds()).padStart(2, '0');"
    "  browserTimeString = year + '-' + month + '-' + day + ' ' + hours + ':' + minutes + ':' + seconds;"
    "  document.getElementById('browserTime').textContent = browserTimeString;"
    "  document.getElementById('timezone').textContent = Intl.DateTimeFormat().resolvedOptions().timeZone;"
    "}";
  
  html +=
    "function syncTime() {"
    "  updateBrowserTime();"
    "  document.getElementById('currentTime').value = browserTimeString;"
    "  document.getElementById('saveBtn').disabled = false;"
    "  isTimeSynced = true;"
    "  alert('✅ Hora sincronizada con el navegador: ' + browserTimeString);"
    "}";
  
  html +=
    "function saveTime() {"
    "  if (!isTimeSynced) {"
    "    alert('❌ Primero debe sincronizar la hora con el navegador');"
    "    return;"
    "  }"
    "  if (confirm('¿Está seguro de que desea guardar la hora ' + browserTimeString + ' en el dispositivo?')) {"
    "    document.getElementById('timeForm').submit();"
    "  }"
    "}";
  
  html +=
    "// Actualizar hora del navegador cada segundo"
    "setInterval(updateBrowserTime, 1000);"
    "updateBrowserTime(); // Inicializar"
    "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleTimeUpdate() {
  if (!webAuth()) return;
  
  if (!server.hasArg("currentTime") || server.arg("currentTime").length() == 0) {
    server.send(400, "text/html", 
      "<html><body><h1>❌ Error: Hora no especificada</h1>"
      "<p>Por favor, especifique la hora actual.</p>"
      "<a href='/time/sync'>Volver</a></body></html>");
    return;
  }
  
  String timeString = server.arg("currentTime");
  Serial.printf("🕐 Actualizando hora del sistema: %s\n", timeString.c_str());
  
  // Actualizar variables de tiempo
  strncpy(currentTimeString, timeString.c_str(), sizeof(currentTimeString) - 1);
  currentTimeString[sizeof(currentTimeString) - 1] = '\0';
  lastTimeSync = millis();
  timeSynced = true;
  
  // Publicar evento de sincronización de tiempo
  if (mqttClient.connected()) {
    String topic = "swatidhome/" + fixedSerialNumber + "/events";
    String message = "{";
    message += "\"timestamp\":\"" + timeString + "\",";
    message += "\"message_id\":" + String(++messageId) + ",";
    message += "\"device\":\"SWATID_PUERTA\",";
    message += "\"serial\":\"" + fixedSerialNumber + "\",";
    message += "\"event_type\":\"TIME_SYNC\",";
    message += "\"time_string\":\"" + timeString + "\",";
    message +=
      "\"source\":\"WEB\""
      "}";
    
    mqttClient.publish(topic.c_str(), message.c_str());
    Serial.printf("📡 Evento de sincronización de tiempo publicado\n");
  }
  
  // Respuesta de confirmación mejorada
  String html = webPageBegin("Hora Actualizada - SWATID-A2");
  
  html +=
    "<div class='container'>"
    "<div class='success-icon'>✅</div>"
    "<h1>Hora Actualizada Exitosamente</h1>";
  
  html +=
    "<div class='info-box'>"
    "<h3>📱 Información de Sincronización</h3>"
    "<p><strong>Nueva hora del dispositivo:</strong></p>";
  html += "<p class='time-display'>" + timeString + "</p>";
  html +=
    "<p><strong>Estado:</strong> ✅ Sincronizado</p>"
    "<p><strong>Última sincronización:</strong> Ahora</p>"
    "<p><strong>Fuente:</strong> Navegador Web</p>"
    "</div>";
  
  html +=
    "<div class='info-box'>"
    "<h3>🔄 Próximos Pasos</h3>"
    "<p>La hora ha sido actualizada correctamente en el dispositivo.</p>"
    "<p>Puede verificar la sincronización en la página principal o volver a sincronizar si es necesario.</p>"
    "</div>";
  
  html +=
    "<div>"
    "<button class='sync' onclick='window.location.href=\"/time/sync\"'>🔄 Sincronizar Otra Vez</button>"
    "<button class='home' onclick='window.location.href=\"/\"'>🏠 Volver al Inicio</button>"
    "</div>";
  
  html +=
    "</div>"
    "</body></html>";
  
  server.send(200, "text/html", html);
  
  Serial.printf("✅ Hora del sistema actualizada: %s\n", timeString.c_str());
}

// =================== MANEJADORES DE SEGURIDAD ===================
void handleSecurityBlockAccess() {
  if (!webAuth()) return;
  
  localAccessBlocked = true;
  blockStartTime = millis();
  saveConfiguration();
  
  Serial.println("🔒 Acceso local bloqueado desde web");
  publishError(4, "Acceso local bloqueado desde web");
  
  server.send(200, "text/html", 
    webPageBegin("Acceso Bloqueado") +
    "<h1>Acceso Local Bloqueado</h1>"
    "<p>El acceso local ha sido bloqueado correctamente.</p>"
    "<p><strong>Estado:</strong> Bloqueado temporalmente</p>"
    "<p><strong>Duración:</strong> " + String(blockDuration/1000) + " segundos</p>"
    "<div style='text-align:center;margin-top:30px;'>"
    "<a href='/'><button>Volver al inicio</button></a></div></body></html>");
}

void handleSecurityUnblockAccess() {
  if (!webAuth()) return;
  
  localAccessBlocked = false;
  failedAttempts = 0;
  blockStartTime = 0;
  saveConfiguration();
  
  Serial.println("🔓 Acceso local desbloqueado desde web");
  publishError(4, "Acceso local desbloqueado desde web");
  
  server.send(200, "text/html", 
    webPageBegin("Acceso Desbloqueado") +
    "<h1>Acceso Local Desbloqueado</h1>"
    "<p>El acceso local ha sido desbloqueado correctamente.</p>"
    "<p><strong>Estado:</strong> Acceso permitido</p>"
    "<p><strong>Intentos fallidos:</strong> Reseteados a 0</p>"
    "<div style='text-align:center;margin-top:30px;'>"
    "<a href='/'><button>Volver al inicio</button></a></div></body></html>");
}

void handleSecurityDisableKeyboards() {
  if (!webAuth()) return;
  
  keyboardReadingEnabled = false;
  saveConfiguration();
  
  Serial.println("🔒 Lectura de teclados deshabilitada desde web");
  publishError(4, "Lectura de teclados deshabilitada desde web");
  
  server.send(200, "text/html", 
    webPageBegin("Teclados Deshabilitados") +
    "<h1>Lectura de Teclados Deshabilitada</h1>"
    "<p>La lectura de teclados ha sido deshabilitada correctamente.</p>"
    "<p><strong>Estado:</strong> Teclados bloqueados</p>"
    "<p><strong>Efecto:</strong> Los códigos no serán procesados</p>"
    "<div style='text-align:center;margin-top:30px;'>"
    "<a href='/'><button>Volver al inicio</button></a></div></body></html>");
}

void handleSecurityEnableKeyboards() {
  if (!webAuth()) return;
  
  keyboardReadingEnabled = true;
  saveConfiguration();
  
  Serial.println("🔓 Lectura de teclados habilitada desde web");
  publishError(4, "Lectura de teclados habilitada desde web");
  
  server.send(200, "text/html", 
    webPageBegin("Teclados Habilitados") +
    "<h1>Lectura de Teclados Habilitada</h1>"
    "<p>La lectura de teclados ha sido habilitada correctamente.</p>"
    "<p><strong>Estado:</strong> Teclados activos</p>"
    "<p><strong>Efecto:</strong> Los códigos serán procesados normalmente</p>"
    "<div style='text-align:center;margin-top:30px;'>"
    "<a href='/'><button>Volver al inicio</button></a></div></body></html>");
}

// =================== HANDLERS DE ENTRADAS DIGITALES ===================

// Handler para la página principal de entradas digitales
void handleDigitalInputs() {
  if (!webAuth()) return;
  
  String html = webPageBegin("Entradas Digitales - SWAT ID");
  html += R"rawliteral(
  <h1>⚡ Entradas Digitales</h1>
  
  <div class="status-panel">
    <h3>📊 Estado en Tiempo Real</h3>
    <p>
      <span class="status-indicator status-low" id="di1-status"></span>
      <strong>DI1 (GPIO36):</strong> <span id="di1-value">LOW</span>
      <span id="di1-waiting" style="color: #ff9800; margin-left: 10px;"></span>
    </p>
    <p>
      <span class="status-indicator status-low" id="di2-status"></span>
      <strong>DI2 (GPIO39):</strong> <span id="di2-value">LOW</span>
      <span id="di2-waiting" style="color: #ff9800; margin-left: 10px;"></span>
    </p>
  </div>
  
  <div class="input-config" id="di1-config">
    <h3>🔌 Entrada Digital 1 (DI1 - GPIO36)</h3>
    <form action="/save_digital_input" method="POST">
      <input type="hidden" name="input" value="1">
      
      <div class="form-group">
        <label>
          <input type="checkbox" name="di1_enabled" id="di1_enabled" value="1">
          <span class="checkbox-label">Habilitada</span>
        </label>
      </div>
      
      <div class="form-group">
        <label>Relé a activar:</label>
        <select name="di1_relay" id="di1_relay">
          <option value="1">Relé 1</option>
          <option value="2">Relé 2</option>
        </select>
      </div>
      
      <div class="form-group">
        <label>Duración (segundos):</label>
        <input type="number" name="di1_duration" id="di1_duration" 
               min="0.5" max="60" step="0.5" value="2.0">
        <small style="color: #666;">Solo aplica en modo Normal</small>
      </div>
      
      <div class="form-group">
        <label>Modo de Funcionamiento:</label>
        <select name="di1_inverse" id="di1_inverse">
          <option value="0">🔵 Normal - HIGH activa relé (temporizado)</option>
          <option value="1">🔴 Inverso - Relé siempre activo, HIGH lo desactiva</option>
        </select>
        <small style="color: #666; display: block; margin-top: 5px;">
          Normal: Pulso activa el relé por el tiempo configurado<br>
          Inverso: Relé siempre activo, pulso lo desactiva (seguridad)
        </small>
      </div>
      
      <button type="submit">💾 Guardar Configuración DI1</button>
    </form>
  </div>
  
  <div class="input-config" id="di2-config">
    <h3>🔌 Entrada Digital 2 (DI2 - GPIO39)</h3>
    <form action="/save_digital_input" method="POST">
      <input type="hidden" name="input" value="2">
      
      <div class="form-group">
        <label>
          <input type="checkbox" name="di2_enabled" id="di2_enabled" value="1">
          <span class="checkbox-label">Habilitada</span>
        </label>
      </div>
      
      <div class="form-group">
        <label>Relé a activar:</label>
        <select name="di2_relay" id="di2_relay">
          <option value="1">Relé 1</option>
          <option value="2">Relé 2</option>
        </select>
      </div>
      
      <div class="form-group">
        <label>Duración (segundos):</label>
        <input type="number" name="di2_duration" id="di2_duration" 
               min="0.5" max="60" step="0.5" value="2.0">
        <small style="color: #666;">Solo aplica en modo Normal</small>
      </div>
      
      <div class="form-group">
        <label>Modo de Funcionamiento:</label>
        <select name="di2_inverse" id="di2_inverse">
          <option value="0">🔵 Normal - HIGH activa relé (temporizado)</option>
          <option value="1">🔴 Inverso - Relé siempre activo, HIGH lo desactiva</option>
        </select>
        <small style="color: #666; display: block; margin-top: 5px;">
          Normal: Pulso activa el relé por el tiempo configurado<br>
          Inverso: Relé siempre activo, pulso lo desactiva (seguridad)
        </small>
      </div>
      
      <button type="submit">💾 Guardar Configuración DI2</button>
    </form>
  </div>
  
  <div class="info-panel">
    <h3>ℹ️ Información</h3>
    <ul>
      <li><strong>DI1</strong> y <strong>DI2</strong> son entradas digitales de 3.3V</li>
      <li>Sin pulsar (pulsador abierto): entrada en <strong>LOW (0V)</strong></li>
      <li>Pulsado (pulsador cerrado): entrada en <strong>HIGH (3.3V)</strong></li>
      <li><strong>Modo Normal (🔵):</strong> HIGH activa el relé por el tiempo configurado</li>
      <li><strong>Modo Inverso (🔴):</strong> Relé siempre activo, HIGH lo desactiva (sistema de seguridad)</li>
      <li>No interfiere con teclados Wiegand ni otras funcionalidades</li>
      <li>⚠️ <strong>Importante:</strong> Solo usar 3.3V, NO conectar 5V</li>
      <li>Se recomienda usar resistencia pull-up de 10kΩ a 3.3V</li>
    </ul>
    
    <h4>🔵 Modo Normal - Casos de Uso:</h4>
    <ul>
      <li>Botón de apertura temporal (portero automático)</li>
      <li>Sensor de presencia (activar luz X segundos)</li>
      <li>Detector de movimiento (alarma temporal)</li>
    </ul>
    
    <h4>🔴 Modo Inverso - Casos de Uso:</h4>
    <ul>
      <li>Botón de emergencia (relé normalmente cerrado)</li>
      <li>Sistema fail-safe (relé activo, pulso desactiva)</li>
      <li>Control de seguridad (relé enclavado, pulso libera)</li>
    </ul>
  </div>
  
  <div style="text-align: center; margin: 30px 0;">
    <a href="/"><button class="btn-back">🏠 Volver al Inicio</button></a>
  </div>
  
  <script>
    // Auto-refresh del estado cada 500ms
    setInterval(function() {
      fetch('/api/digital_inputs_status')
        .then(response => response.json())
        .then(data => {
          // Actualizar DI1
          document.getElementById('di1-value').textContent = data.di1_state ? 'HIGH' : 'LOW';
          document.getElementById('di1-status').className = 
            'status-indicator ' + (data.di1_state ? 'status-high' : 'status-low');
          document.getElementById('di1-waiting').textContent = 
            data.di1_waiting ? '(esperando pulso LOW)' : '';
          
          // Actualizar DI2
          document.getElementById('di2-value').textContent = data.di2_state ? 'HIGH' : 'LOW';
          document.getElementById('di2-status').className = 
            'status-indicator ' + (data.di2_state ? 'status-high' : 'status-low');
          document.getElementById('di2-waiting').textContent = 
            data.di2_waiting ? '(esperando pulso LOW)' : '';
          
          // Actualizar clases de configuración
          document.getElementById('di1-config').className = 
            'input-config' + (data.di1_enabled ? ' enabled' : '');
          document.getElementById('di2-config').className = 
            'input-config' + (data.di2_enabled ? ' enabled' : '');
        })
        .catch(error => console.error('Error:', error));
    }, 500);
    
    // Cargar configuración actual
    fetch('/api/digital_inputs_config')
      .then(response => response.json())
      .then(data => {
        document.getElementById('di1_enabled').checked = data.di1_enabled;
        document.getElementById('di1_relay').value = data.di1_relay;
        document.getElementById('di1_duration').value = data.di1_duration;
        document.getElementById('di1_inverse').value = data.di1_inverse ? '1' : '0';
        
        document.getElementById('di2_enabled').checked = data.di2_enabled;
        document.getElementById('di2_relay').value = data.di2_relay;
        document.getElementById('di2_duration').value = data.di2_duration;
        document.getElementById('di2_inverse').value = data.di2_inverse ? '1' : '0';
      })
      .catch(error => console.error('Error:', error));
  </script>
</body>
</html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

// API: Obtener estado actual de las entradas digitales
void handleDigitalInputsStatus() {
  if (!webAuth()) return;
  
  DynamicJsonDocument doc(256);
  doc["di1_state"] = di1State.currentState;
  doc["di2_state"] = di2State.currentState;
  doc["di1_enabled"] = digitalInputConfig.di1_enabled;
  doc["di2_enabled"] = digitalInputConfig.di2_enabled;
  doc["di1_waiting"] = di1State.waitingForLow;
  doc["di2_waiting"] = di2State.waitingForLow;
  
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

// API: Obtener configuración actual
void handleDigitalInputsConfig() {
  if (!webAuth()) return;
  
  DynamicJsonDocument doc(256);
  doc["di1_enabled"] = digitalInputConfig.di1_enabled;
  doc["di1_relay"] = digitalInputConfig.di1_relay;
  doc["di1_duration"] = digitalInputConfig.di1_duration_ms / 1000.0f;  // Convertir a segundos
  doc["di1_inverse"] = digitalInputConfig.di1_inverse;
  doc["di2_enabled"] = digitalInputConfig.di2_enabled;
  doc["di2_relay"] = digitalInputConfig.di2_relay;
  doc["di2_duration"] = digitalInputConfig.di2_duration_ms / 1000.0f;  // Convertir a segundos
  doc["di2_inverse"] = digitalInputConfig.di2_inverse;
  
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

// Handler para guardar configuración de entrada digital
void handleSaveDigitalInput() {
  if (!webAuth()) return;
  
  int inputNumber = server.arg("input").toInt();
  Serial.printf("📝 [DI] Guardando configuración para entrada %d\n", inputNumber);
  
  if (inputNumber == 1) {
    digitalInputConfig.di1_enabled = server.hasArg("di1_enabled") ? 1 : 0;
    digitalInputConfig.di1_relay = (uint8_t)server.arg("di1_relay").toInt();
    float di1_sec = server.arg("di1_duration").toFloat();
    digitalInputConfig.di1_inverse = (server.arg("di1_inverse").toInt() == 1) ? 1 : 0;
    
    // Validar valores
    if (digitalInputConfig.di1_relay < 1 || digitalInputConfig.di1_relay > 2) {
      digitalInputConfig.di1_relay = 1;
    }
    if (di1_sec < 0.5 || di1_sec > 60) {
      di1_sec = 2.0f;
    }
    digitalInputConfig.di1_duration_ms = (uint32_t)(di1_sec * 1000);
    
    Serial.printf("⚙️ [DI1] Configuración actualizada: %s, Relé %d, %dms (%.1fs), Modo %s\n",
                  digitalInputConfig.di1_enabled ? "HABILITADA" : "DESHABILITADA",
                  digitalInputConfig.di1_relay,
                  digitalInputConfig.di1_duration_ms, di1_sec,
                  digitalInputConfig.di1_inverse ? "INVERSO" : "NORMAL");
    
  } else if (inputNumber == 2) {
    digitalInputConfig.di2_enabled = server.hasArg("di2_enabled") ? 1 : 0;
    digitalInputConfig.di2_relay = (uint8_t)server.arg("di2_relay").toInt();
    float di2_sec = server.arg("di2_duration").toFloat();
    digitalInputConfig.di2_inverse = (server.arg("di2_inverse").toInt() == 1) ? 1 : 0;
    
    // Validar valores
    if (digitalInputConfig.di2_relay < 1 || digitalInputConfig.di2_relay > 2) {
      digitalInputConfig.di2_relay = 2;
    }
    if (di2_sec < 0.5 || di2_sec > 60) {
      di2_sec = 2.0f;
    }
    digitalInputConfig.di2_duration_ms = (uint32_t)(di2_sec * 1000);
    
    Serial.printf("⚙️ [DI2] Configuración actualizada: %s, Relé %d, %dms (%.1fs), Modo %s\n",
                  digitalInputConfig.di2_enabled ? "HABILITADA" : "DESHABILITADA",
                  digitalInputConfig.di2_relay,
                  digitalInputConfig.di2_duration_ms, di2_sec,
                  digitalInputConfig.di2_inverse ? "INVERSO" : "NORMAL");
  }
  
  saveDigitalInputConfig();
  
  // Redirigir de vuelta a la página de entradas digitales
  server.sendHeader("Location", "/digital_inputs");
  server.send(303);
}

// =================== GESTIÓN DE CÓDIGOS ALMACENADOS ===================
// (ver local_codes.h)

// Función de diagnóstico eliminada para reducir tamaño del firmware

// Función de verificación de memoria eliminada para reducir tamaño

// Función de prueba de persistencia eliminada para reducir tamaño

// Función de verificación de integridad eliminada para reducir tamaño

// =================== IMPLEMENTACIÓN DE FUNCIONES OTA ===================

void loadOTAConfig() {
  DEBUG_PRINTLN("🔄 Cargando configuración OTA...");
  
  // Inicializar configuración por defecto
  strcpy(otaConfig.updateUrl, "https://updates.swatid.com/api/check");
  otaConfig.autoUpdateEnabled = false;
  otaConfig.checkInterval = 24; // 24 horas
  otaConfig.lastCheck = 0;
  otaConfig.forceUpdate = false;
  otaConfig.validMarker = 0x1234ABCD;
  
  DEBUG_PRINTLN("✅ Configuración OTA inicializada");
}

void saveOTAConfig() {
  DEBUG_PRINTLN("💾 Guardando configuración OTA...");
  // En una implementación completa, esto guardaría en EEPROM
  DEBUG_PRINTLN("✅ Configuración OTA guardada");
}

void initializeDeviceInfo() {
  DEBUG_PRINTLN("🔄 Inicializando información del dispositivo...");
  
  deviceInfo.macAddress = ETH.macAddress();
  deviceInfo.currentVersion = firmwareVersion;
  deviceInfo.deviceModel = "KC868-A2";
  deviceInfo.serialNumber = fixedSerialNumber;
  deviceInfo.flashSize = ESP.getFlashChipSize();
  deviceInfo.freeHeap = ESP.getFreeHeap();
  
  DEBUG_PRINTF("📊 Dispositivo: %s\n", deviceInfo.deviceModel.c_str());
  DEBUG_PRINTF("📊 MAC: %s\n", deviceInfo.macAddress.c_str());
  DEBUG_PRINTF("📊 Versión: %s\n", deviceInfo.currentVersion.c_str());
  DEBUG_PRINTF("📊 Serial: %s\n", deviceInfo.serialNumber.c_str());
  DEBUG_PRINTF("📊 Flash: %d bytes\n", deviceInfo.flashSize);
  DEBUG_PRINTF("📊 Heap libre: %d bytes\n", deviceInfo.freeHeap);
}

int compareVersions(const String& version1, const String& version2) {
  // Parsear versiones (v2.5.0 -> [2,5,0])
  int v1[3] = {0, 0, 0};
  int v2[3] = {0, 0, 0};
  
  // Parsear version1
  String v1Str = version1;
  v1Str.replace("v", "");
  int dot1 = v1Str.indexOf('.');
  int dot2 = v1Str.indexOf('.', dot1 + 1);
  
  if (dot1 > 0 && dot2 > dot1) {
    v1[0] = v1Str.substring(0, dot1).toInt();
    v1[1] = v1Str.substring(dot1 + 1, dot2).toInt();
    v1[2] = v1Str.substring(dot2 + 1).toInt();
  }
  
  // Parsear version2
  String v2Str = version2;
  v2Str.replace("v", "");
  dot1 = v2Str.indexOf('.');
  dot2 = v2Str.indexOf('.', dot1 + 1);
  
  if (dot1 > 0 && dot2 > dot1) {
    v2[0] = v2Str.substring(0, dot1).toInt();
    v2[1] = v2Str.substring(dot1 + 1, dot2).toInt();
    v2[2] = v2Str.substring(dot2 + 1).toInt();
  }
  
  // Comparar
  for (int i = 0; i < 3; i++) {
    if (v1[i] > v2[i]) return 1;   // version1 > version2
    if (v1[i] < v2[i]) return -1;  // version1 < version2
  }
  return 0; // version1 == version2
}

void checkForUpdates() {
  if (!otaConfig.autoUpdateEnabled) return;
  
  Serial.println("🔄 Verificando actualizaciones automáticas...");
  
  HTTPClient http;
  http.begin(otaConfig.updateUrl);
  http.addHeader("Content-Type", "application/json");
  
  // Crear JSON con información del dispositivo
  DynamicJsonDocument deviceJson(512);
  deviceJson["mac"] = ETH.macAddress();
  deviceJson["version"] = firmwareVersion;
  deviceJson["model"] = "KC868-A2";
  deviceJson["serial"] = fixedSerialNumber;
  
  String jsonString;
  serializeJson(deviceJson, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    DynamicJsonDocument updateInfo(1024);
    deserializeJson(updateInfo, response);
    
    if (updateInfo["available"].as<bool>()) {
      String newVersion = updateInfo["version"].as<String>();
      String downloadUrl = updateInfo["download_url"].as<String>();
      
    Serial.printf("📥 Actualización disponible: %s -> %s\n", 
                  firmwareVersion, newVersion.c_str());
    
    if (compareVersions(newVersion, firmwareVersion) > 0) {
        Serial.println("🚀 Iniciando descarga de actualización...");
        if (downloadAndUpdate(downloadUrl)) {
          Serial.println("✅ Actualización automática completada");
        } else {
          Serial.println("❌ Error en actualización automática");
        }
      }
    } else {
      Serial.println("✅ Firmware actualizado");
    }
  } else {
    Serial.printf("❌ Error verificando actualizaciones: %d\n", httpResponseCode);
  }
  
  http.end();
  otaConfig.lastCheck = millis();
}

bool downloadAndUpdate(const String& downloadUrl) {
  Serial.printf("📥 Descargando desde: %s\n", downloadUrl.c_str());
  
  HTTPClient http;
  http.begin(downloadUrl);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    int contentLength = http.getSize();
    Serial.printf("📊 Tamaño del archivo: %d bytes\n", contentLength);
    
    if (contentLength > 0) {
      // Iniciar actualización OTA
      if (Update.begin(contentLength)) {
        WiFiClient* stream = http.getStreamPtr();
        
        size_t written = 0;
        uint8_t buff[1024] = { 0 };
        
        while (http.connected() && (written < contentLength)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            Update.write(buff, c);
            written += c;
            
            // Mostrar progreso
            int progress = (written * 100) / contentLength;
            if (progress % 10 == 0) {
              Serial.printf("📊 Progreso: %d%%\n", progress);
            }
          }
        }
        
        if (Update.end()) {
          Serial.println("✅ Actualización OTA completada");
          http.end();
          
          // Reiniciar después de 2 segundos
          delay(2000);
          ESP.restart();
          return true;
        } else {
          Serial.println("❌ Error al finalizar OTA");
        }
      } else {
        Serial.println("❌ Error al iniciar OTA");
      }
    }
  } else {
    Serial.printf("❌ Error descargando archivo: %d\n", httpResponseCode);
  }
  
  http.end();
  return false;
}

void handleOTAUpload() {
  if (!webAuth()) return;
  
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
      Serial.printf("❌ Error al iniciar OTA: %s\n", Update.errorString());
      server.send(500, "text/plain", "Error: No se pudo iniciar OTA - " + String(Update.errorString()));
      return;
    }
    
    Serial.println("✅ OTA iniciado correctamente");
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Escribir datos
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Serial.printf("❌ Error escribiendo datos: %s\n", Update.errorString());
      server.send(500, "text/plain", "Error: Fallo al escribir datos - " + String(Update.errorString()));
      return;
    }
    
    // Calcular y mostrar progreso
    if (upload.totalSize > 0) {
      int progress = (upload.currentSize * 100) / upload.totalSize;
      if (progress % 10 == 0) {
        Serial.printf("📊 Progreso OTA: %d%% (%d/%d bytes)\n", progress, upload.currentSize, upload.totalSize);
      }
    }
    
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.println("📥 Archivo recibido completamente, finalizando OTA...");
    
    // Finalizar actualización
    if (Update.end(true)) {
      Serial.println("✅ Actualización OTA completada exitosamente");
      server.send(200, "text/plain", "Actualización completada. Reiniciando en 3 segundos...");
      
      // Publicar evento de actualización exitosa
      if (mqttClient.connected()) {
        String topic = "swatidhome/" + fixedSerialNumber + "/events";
        String message = "{";
        message += "\"timestamp\":\"" + getTimestamp() + "\",";
        message += "\"message_id\":" + String(++messageId) + ",";
        message += "\"device\":\"SWATID_PUERTA\",";
        message += "\"serial\":\"" + fixedSerialNumber + "\",";
        message += "\"event_type\":\"OTA_UPDATE_SUCCESS\",";
        message += "\"new_version\":\"" + String(firmwareVersion) + "\",";
        message +=
          "\"source\":\"WEB_UPLOAD\""
          "}";
        
        mqttClient.publish(topic.c_str(), message.c_str());
        Serial.println("📡 Evento de actualización OTA publicado");
      }
      
      // Reiniciar después de 3 segundos
      delay(3000);
      ESP.restart();
    } else {
      Serial.printf("❌ Error al finalizar OTA: %s\n", Update.errorString());
      server.send(500, "text/plain", "Error al finalizar actualización: " + String(Update.errorString()));
    }
    
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.println("❌ Actualización OTA abortada");
    Update.abort();
    server.send(500, "text/plain", "Actualización abortada");
  }
}

void handleOTAConfig() {
  if (!webAuth()) return;
  
  bool configChanged = false;
  
  // Actualizar URL del servidor
  if (server.hasArg("updateUrl")) {
    String newUrl = server.arg("updateUrl");
    if (newUrl.length() > 0 && newUrl != String(otaConfig.updateUrl)) {
      strncpy(otaConfig.updateUrl, newUrl.c_str(), sizeof(otaConfig.updateUrl) - 1);
      otaConfig.updateUrl[sizeof(otaConfig.updateUrl) - 1] = '\0';
      configChanged = true;
      Serial.printf("🔧 URL de actualizaciones cambiada a: %s\n", otaConfig.updateUrl);
    }
  }
  
  // Actualizar intervalo de verificación
  if (server.hasArg("checkInterval")) {
    int newInterval = server.arg("checkInterval").toInt();
    if (newInterval >= 1 && newInterval <= 168 && newInterval != otaConfig.checkInterval) {
      otaConfig.checkInterval = newInterval;
      configChanged = true;
      Serial.printf("🔧 Intervalo de verificación cambiado a: %d horas\n", otaConfig.checkInterval);
    }
  }
  
  // Actualizar estado de actualización automática
  bool newAutoUpdate = server.hasArg("autoUpdateEnabled");
  if (newAutoUpdate != otaConfig.autoUpdateEnabled) {
    otaConfig.autoUpdateEnabled = newAutoUpdate;
    configChanged = true;
    Serial.printf("🔧 Actualización automática %s\n", 
                  otaConfig.autoUpdateEnabled ? "habilitada" : "deshabilitada");
  }
  
  if (configChanged) {
    saveOTAConfig();
    Serial.println("✅ Configuración OTA actualizada");
  }
  
  // Redirigir a la página OTA
  server.sendHeader("Location", "/ota");
  server.send(302, "text/plain", "");
}

void handleOTACheck() {
  if (!webAuth()) return;
  
  checkForUpdates();
  
  server.send(200, "application/json", "{\"status\":\"check_completed\"}");
}

void setupRollback() {
  Serial.println("🔄 Configurando sistema de rollback...");
  
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next_update = esp_ota_get_next_update_partition(NULL);
  
  Serial.printf("📊 Partición actual: %s\n", running->label);
  Serial.printf("📊 Partición de actualización: %s\n", next_update->label);
  
  // Verificar integridad del firmware actual
  if (!verifyCurrentFirmware()) {
    Serial.println("⚠️ Firmware actual corrupto, iniciando rollback...");
    performRollback();
  }
  
  Serial.println("✅ Sistema de rollback configurado");
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

// =================== PÁGINA DE CONFIGURACIÓN OTA ===================

void handleOTAPage() {
  if (!webAuth()) return;
  
  String html = webPageBegin("Configuración OTA - SWATID-A2");
  
  html +=
    "<div class='container'>"
    "<h1>🔄 Configuración de Actualización OTA</h1>";
  
  // Información actual del dispositivo
  html +=
    "<div class='status-info'>"
    "<h2>📱 Información del Dispositivo</h2>";
  html += "<p><strong>Versión Actual:</strong> " + String(firmwareFullVersion) + " (" HW_BOARD_NAME ")</p>";
  html += "<p><strong>Fecha de Compilación:</strong> " + String(firmwareBuild) + "</p>";
  html += "<p><strong>MAC Address:</strong> " + ETH.macAddress() + "</p>";
  html += "<p><strong>Serial Number:</strong> " + fixedSerialNumber + "</p>";
  html +=
    "<p><strong>Modelo:</strong> SWATID-A2</p>"
    "</div>";
  
  // Actualización manual
  html +=
    "<h2>📤 Actualización Manual</h2>"
    "<p>Subir archivo de firmware (.bin) para actualización manual:</p>";
  
  html +=
    "<form id='uploadForm' enctype='multipart/form-data'>"
    "<div class='form-group'>"
    "<label for='firmwareFile'>Archivo de Firmware (.bin):</label>"
    "<input type='file' id='firmwareFile' name='firmwareFile' accept='.bin' required>"
    "</div>"
    "<button type='submit' class='btn-success'>Subir y Actualizar</button>"
    "</form>";
  
  // Progreso de actualización
  html +=
    "<div id='uploadProgress' class='progress-container' style='display:none;'>"
    "<div class='progress-bar'>"
    "<div id='progressBar' class='progress-fill' style='width:0%;'></div>"
    "</div>"
    "<p id='progressText'>Preparando actualización...</p>"
    "</div>";
  
  // Información de seguridad
  html +=
    "<h2>⚠️ Información de Seguridad</h2>"
    "<div class='status-info'>"
    "<p><strong>Importante:</strong></p>"
    "<ul>"
    "<li>Solo suba archivos .bin compilados para ESP32</li>"
    "<li>El archivo debe ser de un firmware válido para SWATID-A2</li>"
    "<li>La actualización reiniciará el dispositivo automáticamente</li>"
    "<li>Mantenga una copia de seguridad del firmware actual</li>"
    "</ul>"
    "</div>";
  
  html +=
    "<div style='margin-top:30px;text-align:center;'>"
    "<a href='/'><button>🏠 Volver al Inicio</button></a>"
    "</div>";
  
  html += "</div>";
  
  // JavaScript
  html +=
    "<script>"
    "document.getElementById('uploadForm').addEventListener('submit', function(e) {"
    "  e.preventDefault();"
    "  const fileInput = document.getElementById('firmwareFile');"
    "  const file = fileInput.files[0];"
    "  if (!file) {"
    "    alert('Por favor, seleccione un archivo');"
    "    return;"
    "  }"
    "  if (!file.name.endsWith('.bin')) {"
    "    alert('Solo archivos .bin permitidos');"
    "    return;"
    "  }"
    "  if (file.size > 4 * 1024 * 1024) {"
    "    alert('Archivo demasiado grande (máximo 4MB)');"
    "    return;"
    "  }"
    "  if (!confirm('¿Está seguro de que desea actualizar el firmware? Esta acción reiniciará el dispositivo.')) {"
    "    return;"
    "  }"
    "  document.getElementById('uploadProgress').style.display = 'block';"
    "  document.getElementById('progressText').textContent = 'Subiendo archivo...';"
    "  const formData = new FormData();"
    "  formData.append('firmwareFile', file);"
    "  const xhr = new XMLHttpRequest();"
    "  xhr.upload.addEventListener('progress', function(e) {"
    "    if (e.lengthComputable) {"
    "      const percentComplete = (e.loaded / e.total) * 100;"
    "      document.getElementById('progressBar').style.width = percentComplete + '%';"
    "      document.getElementById('progressText').textContent = 'Subiendo: ' + Math.round(percentComplete) + '%';"
    "    }"
    "  });"
    "  xhr.addEventListener('load', function() {"
    "    if (xhr.status === 200) {"
    "      document.getElementById('progressText').textContent = 'Actualización completada. Reiniciando...';"
    "      document.getElementById('progressBar').style.width = '100%';"
    "      setTimeout(() => {"
    "        window.location.href = '/';"
    "      }, 5000);"
    "    } else {"
    "      document.getElementById('uploadProgress').style.display = 'none';"
    "      alert('Error en la actualización (HTTP ' + xhr.status + '): ' + xhr.responseText);"
    "    }"
    "  });"
    "  xhr.addEventListener('error', function() {"
    "    document.getElementById('uploadProgress').style.display = 'none';"
    "    alert('Error de conexión. Verifique que el dispositivo esté conectado.');"
    "  });"
    "  xhr.addEventListener('timeout', function() {"
    "    document.getElementById('uploadProgress').style.display = 'none';"
    "    alert('Tiempo de espera agotado. El archivo puede ser demasiado grande.');"
    "  });"
    "  xhr.open('POST', '/ota/upload');"
    "  xhr.timeout = 120000;"
    "  xhr.send(formData);"
    "});"
    "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleOTAStatus() {
  if (!webAuth()) return;
  
  DynamicJsonDocument status(512);
  status["current_version"] = firmwareVersion;
  status["build_date"] = firmwareBuild;
  status["mac_address"] = ETH.macAddress();
  status["serial_number"] = fixedSerialNumber;
  status["auto_update_enabled"] = otaConfig.autoUpdateEnabled;
  status["update_url"] = otaConfig.updateUrl;
  status["check_interval"] = otaConfig.checkInterval;
  status["last_check"] = otaConfig.lastCheck;
  status["free_heap"] = ESP.getFreeHeap();
  status["flash_size"] = ESP.getFlashChipSize();
  
  String response;
  serializeJson(status, response);
  
  server.send(200, "application/json", response);
}

// =================== FUNCIONES BLE (v4.0) ===================
#ifdef ENABLE_BLE

// Página principal de configuración BLE
void handleBLEPage() {
  if (!webAuth()) return;
  
  String html = webPageBegin("Configuración BLE - SWATID");
  html += R"=====(
  <header>
    <h1>Configuración BLE</h1>
    <p>Gestión de vinculaciones Bluetooth</p>
  </header>
  <nav>
    <a href='/'>Inicio</a>
    <a href='/codes'>Códigos</a>
    <a href='/digital_inputs'>Entradas</a>
    <a href='/ota'>OTA</a>
  </nav>
  <main>
    <div class='container'>
      <h2>Estado BLE</h2>
      <div id='ble-status' class='ble-status'>
        <p><strong>Nombre del dispositivo:</strong> <span id='ble-name'>)=====";
  html += fixedSerialNumber;
  html += R"=====(</span></p>
        <p><strong>Estado:</strong> <span id='ble-state'>Activo</span></p>
        <p><strong>Conexión actual:</strong> <span id='ble-connected'>)=====";
  html += bleDeviceConnected ? "Conectado" : "Sin conexión";
  html += R"=====(</span></p>
      </div>
      
      <h2>Superadmin</h2>
      <div class='user-card superadmin'>
        <h3>Superadministrador</h3>
        <p><strong>Estado:</strong> )=====";
  html += bleAuthConfig.superadmin_registered ? 
    "<span class='badge badge-success'>Registrado</span>" : 
    "<span class='badge badge-warning'>No registrado</span>";
  html += "</p>";
  
  // Mostrar preview de la clave si hay superadmin
  if (bleAuthConfig.superadmin_registered) {
    char keyPreview[32];
    sprintf(keyPreview, "%02X%02X%02X%02X...", 
            bleAuthConfig.superadmin_key[0], bleAuthConfig.superadmin_key[1],
            bleAuthConfig.superadmin_key[2], bleAuthConfig.superadmin_key[3]);
    html += "<p><strong>Key preview:</strong> <code>" + String(keyPreview) + "</code></p>";
  }
  
  html += R"=====(
        <p class='permissions'>Permisos: Todos (0xFF)</p>
        <p><small>El superadmin se registra automáticamente cuando el primer dispositivo se vincula con una clave de 64 bytes.</small></p>
        <button class='btn btn-danger' onclick='confirmClearSuperadmin()'>Eliminar Superadmin</button>
      </div>
      
      <h2>Usuarios Vinculados</h2>
      <table>
        <tr>
          <th>#</th>
          <th>Nombre</th>
          <th>Estado</th>
          <th>Permisos</th>
          <th>Acciones</th>
        </tr>)=====";
  
  for (int i = 0; i < BLE_MAX_USERS; i++) {
    html += "<tr class='user-card ";
    html += bleAuthConfig.user_enabled[i] ? "active" : "inactive";
    html += "'><td>" + String(i + 1) + "</td>";
    
    // Nombre del usuario
    String userName = (bleAuthConfig.user_names[i][0] != '\0') ? String(bleAuthConfig.user_names[i]) : "-";
    html += "<td>" + userName + "</td>";
    
    // Estado
    html += "<td>";
    html += bleAuthConfig.user_enabled[i] ? 
      "<span class='badge badge-success'>Activo</span>" : 
      "<span class='badge badge-danger'>Inactivo</span>";
    html += "</td>";
    
    // Permisos
    html += "<td class='permissions'>";
    if (bleAuthConfig.user_enabled[i]) {
      uint8_t perm = bleAuthConfig.user_permissions[i];
      if (perm & BLE_PERM_RELAY_CONTROL) html += "Relés ";
      if (perm & BLE_PERM_MODE_CHANGE) html += "Modo ";
      if (perm & BLE_PERM_ADD_CODES) html += "Códigos ";
      if (perm & BLE_PERM_NETWORK_CONFIG) html += "Red ";
      // Mostrar preview de clave
      char keyPreview[16];
      sprintf(keyPreview, " [%02X%02X..]", 
              bleAuthConfig.user_keys[i][0], bleAuthConfig.user_keys[i][1]);
      html += "<small><code>" + String(keyPreview) + "</code></small>";
    } else {
      html += "-";
    }
    html += "</td>";
    
    // Acciones
    html += "<td>";
    if (bleAuthConfig.user_enabled[i]) {
      html += "<a class='btn btn-danger' href='/ble/clear-user?slot=" + String(i + 1) + "' onclick='return confirm(\"¿Eliminar usuario " + String(i + 1) + "?\")'></a>";
    }
    html += "</td></tr>";
  }
  
  html += R"=====(
      </table>
      
      <h2>Zona de Peligro</h2>
      <div class='ble-warning'>
        <p><strong>Atención:</strong> Las siguientes acciones son irreversibles.</p>
        <button class='btn btn-danger' onclick='confirmClearAll()'>Eliminar TODAS las vinculaciones</button>
        <p><small>Esto eliminará el superadmin y todos los usuarios. El próximo dispositivo en conectarse se convertirá en superadmin.</small></p>
      </div>
      
      <div id='confirm-superadmin' class='confirm-box'>
        <p><strong>¿Está seguro de eliminar el Superadmin?</strong></p>
        <p>El próximo dispositivo en enviar una clave se convertirá en el nuevo superadmin.</p>
        <a class='btn btn-danger' href='/ble/clear-superadmin'>Confirmar</a>
        <button class='btn btn-secondary' onclick='hideConfirm("superadmin")'>Cancelar</button>
      </div>
      
      <div id='confirm-all' class='confirm-box'>
        <p><strong>¿Está seguro de eliminar TODAS las vinculaciones?</strong></p>
        <p>Se eliminarán el superadmin y los 5 usuarios.</p>
        <a class='btn btn-danger' href='/ble/clear-all'>Confirmar</a>
        <button class='btn btn-secondary' onclick='hideConfirm("all")'>Cancelar</button>
      </div>
      
      <h2>Información de Debug</h2>
      <div class='ble-status' style='font-family:monospace;font-size:12px;'>)=====";
  
  // Debug info
  html += "<p><strong>Estructura BLEAuthConfig:</strong></p>";
  html += "<p>validMarker: 0x" + String(bleAuthConfig.validMarker, HEX) + " (esperado: 0xB1E4C0DE)</p>";
  html += "<p>superadmin_registered: " + String(bleAuthConfig.superadmin_registered) + "</p>";
  html += "<p>checksum: 0x" + String(bleAuthConfig.checksum, HEX) + "</p>";
  
  // Verificar checksum
  uint32_t calcChecksum = calculateBLEChecksum(bleAuthConfig);
  html += "<p>checksum calculado: 0x" + String(calcChecksum, HEX);
  html += (bleAuthConfig.checksum == calcChecksum) ? " ✓" : " ✗ ERROR";
  html += "</p>";
  
  // Contar usuarios activos
  int activeUsers = 0;
  for (int i = 0; i < BLE_MAX_USERS; i++) {
    if (bleAuthConfig.user_enabled[i]) activeUsers++;
  }
  html += "<p>Usuarios activos: " + String(activeUsers) + " de " + String(BLE_MAX_USERS) + "</p>";
  html += "<p>Tamaño estructura: " + String(sizeof(BLEAuthConfig)) + " bytes</p>";
  html += "<p>EEPROM offset: " + String(EEPROM_BLE_AUTH_OFFSET) + "</p>";
  
  html += R"=====(
      </div>
    </div>
  </main>
  <script>
    function confirmClearSuperadmin() {
      document.getElementById('confirm-superadmin').style.display = 'block';
      document.getElementById('confirm-all').style.display = 'none';
    }
    function confirmClearAll() {
      document.getElementById('confirm-all').style.display = 'block';
      document.getElementById('confirm-superadmin').style.display = 'none';
    }
    function hideConfirm(type) {
      document.getElementById('confirm-' + type).style.display = 'none';
    }
  </script>
</body></html>
)=====";
  
  server.send(200, "text/html", html);
}

// Limpiar todas las vinculaciones BLE
void handleBLEClearAll() {
  if (!webAuth()) return;
  
  Serial.println("🔵 [BLE] Limpiando TODAS las vinculaciones...");
  
  // Reiniciar configuración
  memset(&bleAuthConfig, 0, sizeof(BLEAuthConfig));
  bleAuthConfig.validMarker = BLE_AUTH_CONFIG_MARKER;
  bleAuthConfig.superadmin_registered = 0;
  for (int i = 0; i < BLE_MAX_USERS; i++) {
    bleAuthConfig.user_enabled[i] = 0;
    bleAuthConfig.user_permissions[i] = 0;
  }
  saveBLEAuthConfig();
  
  // Desconectar si hay conexión activa
  bleAuthenticated = false;
  bleCurrentPermissions = 0;
  bleConnectedUser = "";
  
  Serial.println("🔵 [BLE] Todas las vinculaciones eliminadas");
  
  // Publicar evento MQTT para monitorización remota
  if (mqttClient.connected()) {
    String topic = "swatidhome/events/" + fixedSerialNumber + "/ble";
    String message = String("{") +
      "\"event\":\"BINDINGS_CLEARED\"," +
      "\"type\":\"ALL\"," +
      "\"source\":\"WEB\"," +
      "\"device\":\"" + fixedSerialNumber + "\"," +
      "\"details\":{" +
        "\"superadmin_cleared\":true," +
        "\"users_cleared\":5" +
      "}," +
      "\"timestamp\":\"" + getTimestamp() + "\"" +
    "}";
    mqttClient.publish(topic.c_str(), message.c_str());
    Serial.println("📤 [MQTT] Evento de desvinculación total publicado");
  }
  
  server.sendHeader("Location", "/ble");
  server.send(303);
}

// Limpiar superadmin
void handleBLEClearSuperadmin() {
  if (!webAuth()) return;
  
  Serial.println("🔵 [BLE] Eliminando superadmin...");
  
  memset(bleAuthConfig.superadmin_key, 0, BLE_KEY_SIZE);
  bleAuthConfig.superadmin_registered = 0;
  saveBLEAuthConfig();
  
  bleAuthenticated = false;
  bleCurrentPermissions = 0;
  bleConnectedUser = "";
  
  Serial.println("🔵 [BLE] Superadmin eliminado");
  
  // Publicar evento MQTT para monitorización remota
  if (mqttClient.connected()) {
    String topic = "swatidhome/events/" + fixedSerialNumber + "/ble";
    String message = String("{") +
      "\"event\":\"BINDINGS_CLEARED\"," +
      "\"type\":\"SUPERADMIN\"," +
      "\"source\":\"WEB\"," +
      "\"device\":\"" + fixedSerialNumber + "\"," +
      "\"details\":{" +
        "\"action\":\"superadmin_removed\"," +
        "\"awaiting_new_superadmin\":true" +
      "}," +
      "\"timestamp\":\"" + getTimestamp() + "\"" +
    "}";
    mqttClient.publish(topic.c_str(), message.c_str());
    Serial.println("📤 [MQTT] Evento de desvinculación superadmin publicado");
  }
  
  server.sendHeader("Location", "/ble");
  server.send(303);
}

// Limpiar un usuario específico
void handleBLEClearUser() {
  if (!webAuth()) return;
  
  if (!server.hasArg("slot")) {
    server.send(400, "text/plain", "Falta parámetro slot");
    return;
  }
  
  int slot = server.arg("slot").toInt();
  if (slot < 1 || slot > BLE_MAX_USERS) {
    server.send(400, "text/plain", "Slot inválido (1-5)");
    return;
  }
  
  Serial.printf("🔵 [BLE] Eliminando usuario %d...\n", slot);
  
  int idx = slot - 1;
  
  // Guardar nombre del usuario antes de eliminarlo (para el evento)
  String userName = String(bleAuthConfig.user_names[idx]);
  if (userName.length() == 0) userName = "Usuario" + String(slot);
  uint8_t oldPermissions = bleAuthConfig.user_permissions[idx];
  
  // Eliminar usuario
  memset(bleAuthConfig.user_keys[idx], 0, BLE_KEY_SIZE);
  bleAuthConfig.user_enabled[idx] = 0;
  bleAuthConfig.user_permissions[idx] = 0;
  memset(bleAuthConfig.user_names[idx], 0, 16);
  saveBLEAuthConfig();
  
  Serial.printf("🔵 [BLE] Usuario %d eliminado\n", slot);
  
  // Publicar evento MQTT para monitorización remota
  if (mqttClient.connected()) {
    String topic = "swatidhome/events/" + fixedSerialNumber + "/ble";
    String message = String("{") +
      "\"event\":\"BINDINGS_CLEARED\"," +
      "\"type\":\"USER\"," +
      "\"source\":\"WEB\"," +
      "\"device\":\"" + fixedSerialNumber + "\"," +
      "\"details\":{" +
        "\"slot\":" + String(slot) + "," +
        "\"user_name\":\"" + userName + "\"," +
        "\"previous_permissions\":\"0x" + String(oldPermissions, HEX) + "\"" +
      "}," +
      "\"timestamp\":\"" + getTimestamp() + "\"" +
    "}";
    mqttClient.publish(topic.c_str(), message.c_str());
    Serial.printf("📤 [MQTT] Evento de desvinculación usuario %d publicado\n", slot);
  }
  
  server.sendHeader("Location", "/ble");
  server.send(303);
}

// API para obtener estado BLE
void handleBLEStatus() {
  if (!webAuth()) return;
  
  DynamicJsonDocument doc(1024);
  doc["name"] = fixedSerialNumber;
  doc["connected"] = bleDeviceConnected;
  doc["authenticated"] = bleAuthenticated;
  doc["connected_user"] = bleConnectedUser;
  doc["superadmin_registered"] = (bool)bleAuthConfig.superadmin_registered;
  
  JsonArray users = doc.createNestedArray("users");
  for (int i = 0; i < BLE_MAX_USERS; i++) {
    JsonObject user = users.createNestedObject();
    user["slot"] = i + 1;
    user["enabled"] = (bool)bleAuthConfig.user_enabled[i];
    user["name"] = bleAuthConfig.user_names[i];
    user["permissions"] = bleAuthConfig.user_permissions[i];
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

#endif // ENABLE_BLE

 bool deleteCode(const char* type, const char* value, int keyboardId = -1) {
   for (uint16_t i = 0; i < storedCodes->count; i++) {
     if (strcmp(storedCodes->codes[i].value, value) == 0 &&
         strcmp(storedCodes->codes[i].type, type) == 0) {
       
       // Si no se especifica keyboardId, eliminar cualquier coincidencia
       // Si se especifica, solo eliminar si coincide
       if (keyboardId == -1 || storedCodes->codes[i].keyboard_id == keyboardId) {
         storedCodes->codes[i] = storedCodes->codes[storedCodes->count - 1];
         storedCodes->count--;
         saveStoredCodes();
         return true;
       }
     }
   }
   return false;
 }
 
// (ver local_codes.h)
 
 // =================== SERVIDOR WEB ===================
 void setupWebServer() {
   server.on("/", handleRoot);
   server.on("/save", HTTP_POST, handleSave);
   server.on("/rele", handleRele);
   server.on("/reboot", handleReboot);
   server.on("/reset", handleReset);
   server.on("/changepass", HTTP_POST, handleChangePass);
  server.on("/codes", handleCodes);
  server.on("/codes/add", HTTP_POST, handleCodesAdd);
  server.on("/codes/delete", HTTP_GET, handleCodesDelete);
  server.on("/codes/bulk-import", HTTP_POST, handleBulkImport, handleBulkImport);
  server.on("/codes/template", HTTP_GET, handleCSVTemplate);
  server.on("/codes/start-tag-reading", HTTP_POST, handleStartTagReading);
  server.on("/codes/stop-tag-reading", HTTP_POST, handleStopTagReading);
  server.on("/codes/read-tags-status", HTTP_GET, handleReadTagsStatus);
  server.on("/codes/export-read-tags", HTTP_GET, handleExportReadTags);
  server.on("/codes/load-read-tags", HTTP_POST, handleLoadReadTags);
  server.on("/remote-codes", handleRemoteCodes);
  server.on("/remote-codes/add", HTTP_POST, handleRemoteCodesAdd);
  server.on("/remote-codes/delete", HTTP_GET, handleRemoteCodesDelete);
  server.on("/remote-codes/delete-all", HTTP_GET, handleRemoteCodesDeleteAll);
  server.on("/turnstile/config", HTTP_POST, handleTurnstileConfig);
  server.on("/turnstile/reset", HTTP_GET, handleTurnstileReset);
  server.on("/export/codes", HTTP_GET, handleExportCodes);
  server.on("/export/remote-codes", HTTP_GET, handleExportRemoteCodes);
  server.on("/import/codes", HTTP_POST, handleImportCodes);
  server.on("/time/sync", HTTP_GET, handleTimeSync);
  server.on("/time/update", HTTP_POST, handleTimeUpdate);
  server.on("/security/block-access", HTTP_GET, handleSecurityBlockAccess);
  server.on("/security/unblock-access", HTTP_GET, handleSecurityUnblockAccess);
  
  // =================== RUTAS ENTRADAS DIGITALES ===================
  server.on("/digital_inputs", HTTP_GET, handleDigitalInputs);
  server.on("/api/digital_inputs_status", HTTP_GET, handleDigitalInputsStatus);
  server.on("/api/digital_inputs_config", HTTP_GET, handleDigitalInputsConfig);
  server.on("/save_digital_input", HTTP_POST, handleSaveDigitalInput);
  
  // =================== RUTAS OTA ===================
  server.on("/ota", HTTP_GET, handleOTAPage);
  server.on("/ota/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "OK");
  }, handleOTAUpload);
  server.on("/ota/config", HTTP_POST, handleOTAConfig);
  server.on("/ota/check", HTTP_GET, handleOTACheck);
  server.on("/ota/status", HTTP_GET, handleOTAStatus);
  server.on("/security/disable-keyboards", HTTP_GET, handleSecurityDisableKeyboards);
  server.on("/security/enable-keyboards", HTTP_GET, handleSecurityEnableKeyboards);
  
#ifdef ENABLE_BLE
  // =================== RUTAS BLE ===================
  server.on("/ble", HTTP_GET, handleBLEPage);
  server.on("/ble/clear-all", HTTP_GET, handleBLEClearAll);
  server.on("/ble/clear-superadmin", HTTP_GET, handleBLEClearSuperadmin);
  server.on("/ble/clear-user", HTTP_GET, handleBLEClearUser);
  server.on("/api/ble/status", HTTP_GET, handleBLEStatus);
#endif

  // =================== RUTAS DE RED (wifi_manager / gsm_modem) ===================
  wifiRegisterWebRoutes();   // /wifi, /api/wifi/*
  gsmRegisterWebRoutes();    // /api/gsm/* (solo targets con GSM)

  server.onNotFound(handleNotFound);

   server.begin();
   Serial.println("🌐 Servidor web iniciado");
   
   if (ethConnected) {
     Serial.printf("🌐 Acceso web: http://%s\n", ip.toString().c_str());
   }
 }
 
 // =================== MANEJADORES WEB ===================
 // =================== MANEJADORES WEB ===================
 void handleRoot() {
   if (!webAuth()) return;
 
   String html = webPageBegin("Controladora A2 - SWATID");
 
   html += "<header><h1>Controladora A2 - SWATID</h1>";
   html += "<p>Device: <strong>" + String(deviceName) + "</strong> | Serial: <strong>" + fixedSerialNumber + "</strong></p>";
   html += "<p>Hardware: <strong>" HW_BOARD_NAME "</strong> | Firmware: <strong>" + String(firmwareFullVersion) + "</strong> | Build: " + firmwareBuild + "</p>";
   html += "<p>IP: " + ip.toString() + "</p></header>";
 
   html += "<main><div class='container'>";

  // Panel de estado de red en vivo (valida la prioridad ETH > WiFi > 4G)
  html += R"=====(
<h2>📶 Estado de Red</h2>
<table>
 <tr><th>Interfaz</th><th>Estado</th><th>Detalle</th></tr>
 <tr><td>Ethernet</td><td id='nEth'>…</td><td id='nEthD'></td></tr>
 <tr><td>WiFi STA</td><td id='nSta'>…</td><td id='nStaD'></td></tr>
 <tr><td>AP</td><td id='nAp'>…</td><td id='nApD'></td></tr>
 <tr><td>4G</td><td id='nGsm'>…</td><td id='nGsmD'></td></tr>
 <tr><td><strong>MQTT</strong></td><td id='nMq'>…</td><td id='nMqD'></td></tr>
</table>
<script>
var netUpd=function(){
 fetch('/api/wifi/status').then(r=>r.json()).then(s=>{
  document.getElementById('nEth').textContent=s.eth_up?'✅ conectado':'❌ sin enlace';
  document.getElementById('nEthD').textContent=s.eth_ip||'';
  document.getElementById('nSta').textContent=s.sta.enabled?(s.sta.connected?'✅ conectada':'⏳ conectando'):'—';
  document.getElementById('nStaD').textContent=s.sta.ssid?(s.sta.ssid+(s.sta.connected?' · '+s.sta.ip:'')):'sin configurar';
  document.getElementById('nAp').textContent=s.ap.active?'✅ activo':'—';
  document.getElementById('nApD').textContent=s.ap.active?(s.ap.clients+' clientes · '+s.ap.ip):'';
  document.getElementById('nMq').textContent=s.mqtt_connected?'✅ conectado':'❌ sin conexión';
  document.getElementById('nMqD').innerHTML='vía <strong>'+s.mqtt_iface+'</strong>';
 }).catch(()=>{});
 fetch('/api/gsm/status').then(r=>r.json()).then(g=>{
  var t=g.state==='attached'?(g.data_up?'✅ datos OK':'✅ registrado'):g.state;
  document.getElementById('nGsm').textContent=t;
  document.getElementById('nGsmD').textContent=(g.operator||'')+
   (g.csq>=0&&g.csq<=31?' · CSQ '+g.csq:'')+(g.data_ip?' · '+g.data_ip:'');
 }).catch(()=>{document.getElementById('nGsm').textContent='—';});
};
netUpd();setInterval(netUpd,3000);
</script>
)=====";

   // Estado de seguridad
   html += "<div class='security-status" + String(localAccessBlocked ? " security-blocked" : "") + "'>";
   html +=
     "<h2>Estado de Seguridad</h2>"
     "<table>"
     "<tr><th>Parámetro</th><th>Valor</th></tr>";
  html += "<tr><td>Acceso Local</td><td>" + String(localAccessBlocked ? "🔒 BLOQUEADO" : "🔓 Permitido") + "</td></tr>";
  html += "<tr><td>Lectura Teclados</td><td>" + String(keyboardReadingEnabled ? "🔓 Habilitada" : "🔒 Deshabilitada") + "</td></tr>";
  html += "<tr><td>Intentos fallidos</td><td>" + String(failedAttempts) + "/" + String(maxFailedAttempts) + "</td></tr>";
  html += "<tr><td>Duración bloqueo</td><td>" + String(blockDuration/1000) + " segundos</td></tr>";
   if (localAccessBlocked && blockStartTime > 0) {
     unsigned long remaining = (blockDuration > (millis() - blockStartTime)) ? 
       (blockDuration - (millis() - blockStartTime)) / 1000 : 0;
     html += "<tr><td>Tiempo restante</td><td>" + String(remaining) + " segundos</td></tr>";
  }
  html += "</table>";
  
  // Botones de control de seguridad
  html +=
    "<div class='security-controls'>"
    "<h3>Controles de Seguridad</h3>";
  
  // Botones de bloqueo de acceso local
  if (localAccessBlocked) {
    html += "<a href='/security/unblock-access'><button class='btn-success'>Desbloquear Acceso Local</button></a>";
  } else {
    html += "<a href='/security/block-access'><button class='btn-warning'>Bloquear Acceso Local</button></a>";
  }
  
  // Botones de control de lectura de teclados
  if (keyboardReadingEnabled) {
    html += "<a href='/security/disable-keyboards'><button class='btn-warning'>Deshabilitar Lectura Teclados</button></a>";
  } else {
    html += "<a href='/security/enable-keyboards'><button class='btn-success'>Habilitar Lectura Teclados</button></a>";
  }
  
  html +=
    "</div>"
    "</div>";
 
   // Estado de teclados duales
   html +=
     "<div class='dual-status'>"
     "<h2>Estado de Teclados Wiegand Duales</h2>"
     "<table>"
     "<tr><th>Teclado</th><th>Pines GPIO</th><th>Estado</th><th>Función</th></tr>";
   html += "<tr><td>Teclado 1</td><td>" + String(WIEGAND1_D0) + "/" + String(WIEGAND1_D1) + "</td><td style='color:green'>✅ Activo</td><td>Principal</td></tr>";
   html += "<tr><td>Teclado 2</td><td>" + String(WIEGAND2_D0) + "/" + String(WIEGAND2_D1) + "</td><td style='color:green'>✅ Activo</td><td>Secundario</td></tr>";
   html +=
     "</table>"
     "<p><strong>💡 Sistema Dual Operativo:</strong> Ambos teclados funcionan simultáneamente</p>"
     "</div>";
 
   // Formulario de configuración
   html +=
     "<h2>Configuración del Dispositivo</h2>"
     "<form action='/save' method='post'>";
 
   html += "<div class='form-group'><label for='deviceName'>Nombre del dispositivo (editable):</label>";
    html += "<input type='text' id='deviceName' name='deviceName' value='" + String(deviceName) + "'></div>";
 
   html += "<div class='form-group'><label for='fixedSerial'>Número de serie MQTT (fijo):</label>";
   html += "<input type='text' id='fixedSerial' name='fixedSerial' value='" + fixedSerialNumber + "' class='readonly' readonly></div>";
 
   html += "<div class='form-group'><label for='releDuration'>Duración del relé (segundos):</label>";
   html += "<input type='number' id='releDuration' name='releDuration' min='0.5' step='0.5' value='" + String(releDuration) + "'></div>";
 
  html += "<div class='form-group'><label for='useDhcp'>Usar DHCP:</label><select id='useDhcp' name='useDhcp' onchange='toggleIpFields()'>";
  html += useDhcp ? "<option value='1' selected>Sí</option><option value='0'>No</option>" : "<option value='1'>Sí</option><option value='0' selected>No</option>";
  html += "</select></div>";

  // Con DHCP: mostrar la IP y puerta de enlace realmente asignadas (solo lectura)
  {
    String curGw = ethConnected ? ETH.gatewayIP().toString()
                                : (wifiStaConnected() ? WiFi.gatewayIP().toString() : String("--"));
    html += "<div id='dhcpInfo' style='display:" + String(useDhcp ? "block" : "none") + "'>";
    html += "<div class='form-group'><label>IP asignada (DHCP):</label><input type='text' value='" + ip.toString() + "' class='readonly' readonly></div>";
    html += "<div class='form-group'><label>Puerta de enlace:</label><input type='text' value='" + curGw + "' class='readonly' readonly></div>";
    html += "</div>";
  }

  html += "<div class='form-group'><label for='validationMode'>Modo de Validación:</label><select id='validationMode' name='validationMode'>";
  html += storedCodes->localValidationFirst ? 
    "<option value='local' selected>🏠 Primero Local</option><option value='remote'>🌐 Primero Remoto</option>" :
    "<option value='local'>🏠 Primero Local</option><option value='remote' selected>🌐 Primero Remoto</option>";
  html += "</select></div>";

  // Campos de IP estática
   html += "<div id='staticIpFields' style='display:" + String(useDhcp ? "none" : "block") + "'>";
   html += "<div class='form-group'><label for='staticIp'>IP Estática:</label><input type='text' id='staticIp' name='staticIp' value='" + staticIP.toString() + "'></div>";
   html += "<div class='form-group'><label for='staticGateway'>Puerta de enlace:</label><input type='text' id='staticGateway' name='staticGateway' value='" + staticGateway.toString() + "'></div>";
   html += "<div class='form-group'><label for='staticSubnet'>Máscara de subred:</label><input type='text' id='staticSubnet' name='staticSubnet' value='" + staticSubnet.toString() + "'></div>";
   html += "<div class='form-group'><label for='staticDns'>DNS:</label><input type='text' id='staticDns' name='staticDns' value='" + staticDns.toString() + "'></div>";
   html += "</div>";
 
  html +=
    "<button type='submit'>Guardar configuración</button>"
    "</form>";
  
  // Sección de sincronización de tiempo
  html +=
    "<div class='time-sync-section'>"
    "<h3>Sincronización de Tiempo</h3>";
   html += "<p><strong>Hora actual:</strong> " + String(currentTimeString) + "</p>";
  html += "<p><strong>Estado:</strong> " + String(timeSynced ? "✅ Sincronizado" : "❌ No sincronizado") + "</p>";
  html +=
    "<a href='/time/sync'><button class='btn-info'>Sincronizar Hora</button></a>"
    "</div>";
 
   // Cambio de contraseña
   html +=
     "<h2>Cambiar contraseña</h2>"
     "<form action='/changepass' method='post'>"
     "<div class='form-group'><label for='newPassword'>Nueva contraseña:</label>"
     "<input type='password' id='newPassword' name='newPassword'></div>"
     "<button type='submit'>Cambiar contraseña</button>"
     "</form>";

   // Configuración del Modo Torno
   html +=
     "<h2>Configuración del Modo Torno</h2>"
     "<div class='dual-status'>"
     "<p><strong>🔄 Modo Torno:</strong> Permite control bidireccional donde cada teclado controla un relé específico.</p>";
   html += "<p><strong>Estado actual:</strong> " + String(isTurnstileModeEnabled() ? "✓ ACTIVO" : "○ INACTIVO") + "</p>";
   if (isTurnstileModeEnabled()) {
     html +=
       "<p><strong>Mapeo actual:</strong></p>"
       "<ul>";
     html += "<li>Teclado 1 (GPIO 33/14) → Relé " + String(config.turnstile.keyboard1_relay) + "</li>";
     html += "<li>Teclado 2 (GPIO 4/16) → Relé " + String(config.turnstile.keyboard2_relay) + "</li>";
     html += "</ul>";
   }
   html += "</div>";
   
   html +=
     "<form action='/turnstile/config' method='post'>"
     "<div class='form-group'>"
     "<label for='turnstile_mode'>Modo de Operación:</label>"
     "<select name='turnstile_mode' id='turnstile_mode'>";
   html += "<option value='normal'" + String(!isTurnstileModeEnabled() ? " selected" : "") + ">1️⃣ Modo Normal</option>";
   html += "<option value='turnstile'" + String(isTurnstileModeEnabled() ? " selected" : "") + ">2️⃣ Modo Torno</option>";
   html +=
     "</select>"
     "</div>";
   
   html +=
     "<div class='form-group'>"
     "<label for='keyboard1_relay'>Teclado 1 (GPIO 33/14) → Relé:</label>"
     "<select name='keyboard1_relay' id='keyboard1_relay'>";
   html += "<option value='1'" + String(config.turnstile.keyboard1_relay == 1 ? " selected" : "") + ">Relé 1</option>";
   html += "<option value='2'" + String(config.turnstile.keyboard1_relay == 2 ? " selected" : "") + ">Relé 2</option>";
   html +=
     "</select>"
     "</div>";
   
   html +=
     "<div class='form-group'>"
     "<label for='keyboard2_relay'>Teclado 2 (GPIO 4/16) → Relé:</label>"
     "<select name='keyboard2_relay' id='keyboard2_relay'>";
   html += "<option value='1'" + String(config.turnstile.keyboard2_relay == 1 ? " selected" : "") + ">Relé 1</option>";
   html += "<option value='2'" + String(config.turnstile.keyboard2_relay == 2 ? " selected" : "") + ">Relé 2</option>";
   html +=
     "</select>"
     "</div>";
   
   html +=
     "<button type='submit'>Guardar Configuración del Torno</button>"
     "</form>";
   
   // Estado de solicitud pendiente
   if (isTurnstileModeEnabled() && pendingRequest.active) {
     html +=
       "<div class='security-status'>"
       "<h3>Solicitud Pendiente</h3>";
     html += "<p><strong>Código:</strong> " + String(pendingRequest.code) + " (" + String(pendingRequest.type) + ")</p>";
     html += "<p><strong>Teclado:</strong> " + String(pendingRequest.keyboard_id == 1 ? "Teclado 1" : "Teclado 2") + "</p>";
     html += "<p><strong>Relé a abrir:</strong> " + String(pendingRequest.relay_to_open) + "</p>";
     html += "<p><strong>Tiempo transcurrido:</strong> " + String((millis() - pendingRequest.timestamp) / 1000) + "s / 30s</p>";
     html +=
       "<a href='/turnstile/reset'><button class='btn-warning'>Cancelar Solicitud</button></a>"
       "</div>";
   }

  // Acciones rápidas
  html +=
    "<h2>Acciones</h2><div class='actions'>"
    "<a href='/rele?relay=1'><button>Relé 1 ON</button></a>"
    "<a href='/rele?relay=2'><button>Relé 2 ON</button></a>"
    "<a href='/codes'><button class='btn-info'>Gestión de Códigos</button></a>"
    "<a href='/remote-codes'><button class='btn-info'>Códigos Remotos</button></a>"
    "<a href='/digital_inputs'><button class='btn-info'>Entradas Digitales</button></a>"
    "<a href='/wifi'><button class='btn-info'>📶 Red WiFi</button></a>";
#if A2_FEATURE_GSM_MODEM
  html += "<a href='/gsm'><button class='btn-info'>📡 Módem 4G</button></a>";
#endif
#ifdef ENABLE_BLE
 html += "<a href='/ble'><button class='btn-info'>Config BLE</button></a>";
#endif
 html +=
   "<a href='/reboot'><button class='btn-warning'>Reiniciar</button></a>"
   "<a href='/reset'><button class='btn-danger'>Resetear</button></a>"
   "</div>";
 
   // Último acceso con información de teclado
   html += "<h2>Último acceso</h2><table><tr><th>Tipo</th><th>Código</th><th>Hora</th><th>Teclado</th></tr>";
   if (strlen(lastCode) > 0) {
      html += "<tr><td>" + String(lastType) + "</td><td>" + String(lastCode) + "</td><td>" + String(lastTime) + "</td><td>" + String(lastKeyboardId) + "</td></tr>";
   } else {
     html += "<tr><td colspan='4'>No hay registros todavía</td></tr>";
   }
   html += "</table>";
 
   // Estado de conexión
   html +=
     "<h2>Estado de Conexión</h2>"
     "<table>"
     "<tr><th>Elemento</th><th>Estado</th></tr>";
   html += "<tr><td>Red (Ethernet)</td><td>" + String(ethConnected ? "Conectado" : "Desconectado") + "</td></tr>";
 
   String mqttStatus = "Pendiente";
   if (mqttClient.connected()) mqttStatus = "Conectado";
   else if (!ethConnected) mqttStatus = "Sin conexión";
 
  html += "<tr><td>Servidor MQTT</td><td>" + mqttStatus + "</td></tr>";
   html += "<tr><td>Hora del sistema</td><td>" + String(currentTimeString) + "</td></tr>";
  html += "<tr><td>Última sincronización</td><td>" + String((millis() - lastTimeSync) / 1000) + " segundos</td></tr>";
  html += "<tr><td>Códigos almacenados</td><td>" + String(storedCodes->count) + "/" + String(MAX_CODES) + "</td></tr>";
  html += "<tr><td>Modo validación</td><td>" + String(storedCodes->localValidationFirst ? "Local primero" : "Remoto primero") + "</td></tr>";
  html += "<tr><td>Modo torno</td><td>" + String(isTurnstileModeEnabled() ? "✓ ACTIVO" : "○ INACTIVO") + "</td></tr>";
   if (isTurnstileModeEnabled()) {
     html += "<tr><td>Solicitud pendiente</td><td>" + String(pendingRequest.active ? "⏳ ACTIVA" : "✅ Ninguna") + "</td></tr>";
  }
  html += "</table>";

  // =================== SECCIÓN OTA ===================
  html +=
    "<div class='security-status'>"
    "<h2>Actualización de Firmware</h2>"
    "<table>"
    "<tr><th>Parámetro</th><th>Valor</th></tr>";
  html += "<tr><td>Versión Actual</td><td><strong>" + String(firmwareVersion) + "</strong></td></tr>";
  html += "<tr><td>Fecha de Compilación</td><td>" + String(firmwareBuild) + "</td></tr>";
  html += "<tr><td>Actualización Automática</td><td>" + String(otaConfig.autoUpdateEnabled ? "✅ Habilitada" : "❌ Deshabilitada") + "</td></tr>";
  html += "<tr><td>URL del Servidor</td><td>" + String(otaConfig.updateUrl) + "</td></tr>";
  html += "<tr><td>Intervalo de Verificación</td><td>" + String(otaConfig.checkInterval) + " horas</td></tr>";
  html += "<tr><td>Última Verificación</td><td>" + String(otaConfig.lastCheck > 0 ? "Hace " + String((millis() - otaConfig.lastCheck) / 3600000) + " horas" : "Nunca") + "</td></tr>";
  html += "</table>";
  
  html +=
    "<div style='margin-top: 15px;'>"
    "<a href='/ota'><button class='btn btn-primary'>Configurar Actualizaciones</button></a>"
    "<button onclick='checkForUpdates()' class='btn btn-warning' style='margin-left: 10px;'>Verificar Ahora</button>"
    "</div>"
    "</div>";

  html += "</div></main>";
   
   // Pie de página con datos de contacto
   html +=
     "<footer style='background-color: #2c3e50; color: white; padding: 20px; text-align: center; margin-top: 30px;'>"
     "<div style='max-width: 800px; margin: 0 auto;'>"
     "<h3 style='margin: 0 0 10px 0; color: #ecf0f1;'>Smart World And Things SLU</h3>"
     "<p style='margin: 5px 0; font-size: 14px;'>"
     "<strong>Web:</strong> <a href='https://www.swat-id.com' style='color: #3498db; text-decoration: none;'>www.swat-id.com</a> | "
     "<strong>Tel:</strong> 633 44 84 27 | "
     "<strong>Email:</strong> <a href='mailto:info@swat-id.com' style='color: #3498db; text-decoration: none;'>info@swat-id.com</a>"
     "</p>"
     "<p style='margin: 5px 0; font-size: 12px; color: #bdc3c7;'>Controladora A2 - SWATID | Sistema de Control de Acceso Dual Wiegand</p>"
     "</div>"
     "</footer>";
   
   html +=
     "<script>"
     "function toggleIpFields(){var d=document.getElementById('useDhcp').value=='1';"
     "document.getElementById('staticIpFields').style.display=d?'none':'block';"
     "var i=document.getElementById('dhcpInfo');if(i)i.style.display=d?'block':'none';}"
     "function checkForUpdates() {"
     "  fetch('/ota/check')"
     "    .then(response => response.json())"
     "    .then(data => {"
     "      alert('Verificación de actualizaciones completada');"
     "      location.reload();"
     "    })"
     "    .catch(error => {"
     "      alert('Error verificando actualizaciones: ' + error);"
     "    });"
     "}"
     "</script>"
     "</body></html>";
 
   server.send(200, "text/html", html);
 }
 
 void handleSave() {
   if (!webAuth()) return;
   
   if (server.hasArg("deviceName")) {
     String tempName = server.arg("deviceName");
    strncpy(deviceName, tempName.c_str(), sizeof(deviceName) - 1);
    deviceName[sizeof(deviceName) - 1] = '\0';
   }
   
   if (server.hasArg("releDuration")) {
     releDuration = server.arg("releDuration").toFloat();
   }
   
  if (server.hasArg("useDhcp")) {
    useDhcp = server.arg("useDhcp") == "1";
  }
  
  if (server.hasArg("validationMode")) {
    String validationMode = server.arg("validationMode");
    bool newLocalValidationFirst = (validationMode == "local");
    
    if (newLocalValidationFirst != storedCodes->localValidationFirst) {
      storedCodes->localValidationFirst = newLocalValidationFirst;
      saveStoredCodes();
      Serial.printf("⚙️ Modo de validación cambiado a: %s\n", 
                    storedCodes->localValidationFirst ? "Primero Local" : "Primero Remoto");
    }
  }
  
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
   
   saveConfiguration();
   
   server.sendHeader("Location", "/");
   server.send(303);
   
   publishError(4, "Configuración actualizada desde web");
   Serial.println("🌐 Configuración actualizada desde web");
 }
 
 void handleRele() {
   if (!webAuth()) return;
 
   int relay = server.hasArg("relay") ? server.arg("relay").toInt() : 1;
   if (relay < 1 || relay > 2) relay = 1;
 
   Serial.printf("🌐 Activación manual relé %d desde web\n", relay);
   controlReleWithDuration(releDuration, relay);
 
   // Publicar evento de apertura desde web
   publishAccessEvent("", "WEB_COMMAND", 0, true, "WEB");
 
   server.sendHeader("Location", "/");
   server.send(303);
   
   publishResponse(0, messageId++, "relay " + String(relay) + " activated from web", relay);
 }
 
 void handleReboot() {
   if (!webAuth()) return;
   
   server.send(200, "text/html", 
     "<html><body><h1>🔄 Reiniciando sistema dual...</h1>"
     "<script>setTimeout(function(){ window.location.href='/' }, 10000);</script>"
     "</body></html>");
   
   Serial.println("🔄 Reinicio solicitado desde web");
   publishError(4, "Reinicio iniciado desde web");
   delay(2000);
   ESP.restart();
 }
 
 void handleReset() {
   if (!webAuth()) return;
   
   resetToDefault();
   
   server.send(200, "text/html", 
     "<html><body><h1>🔧 Reseteando configuración...</h1>"
     "<script>setTimeout(function(){ window.location.href='/' }, 10000);</script>"
     "</body></html>");
   
   Serial.println("🔧 Reset a valores por defecto desde web");
   publishError(4, "Reset iniciado desde web");
   delay(2000);
   ESP.restart();
 }
 
 void handleChangePass() {
   if (!webAuth()) return;
 
   if (server.hasArg("newPassword")) {
     String newPass = server.arg("newPassword");
     if (newPass.length() >= 4 && newPass.length() < 32) {
       newPass.toCharArray(admin_password, sizeof(admin_password));
       saveConfiguration();
       
       server.send(200, "text/html", 
         "<html><body><h1>✅ Contraseña actualizada</h1>"
         "<a href='/'>Volver</a></body></html>");
       
       Serial.println("🔐 Contraseña actualizada desde web");
       publishError(4, "Contraseña modificada desde web");
     } else {
       server.send(200, "text/html", 
         "<html><body><h1>❌ Error: Contraseña debe tener 4-31 caracteres</h1>"
         "<a href='/'>Volver</a></body></html>");
     }
   } else {
     server.send(400, "text/html", 
       "<html><body><h1>❌ Error: Falta nueva contraseña</h1>"
       "<a href='/'>Volver</a></body></html>");
   }
 }
 
void handleCodes() {
  if (!webAuth()) return;

  // Obtener parámetros de paginación y búsqueda
  int page = server.arg("page").toInt();
  if (page < 1) page = 1;
  
  String searchTerm = server.arg("search");
  searchTerm.trim();
  
  const int ITEMS_PER_PAGE = 20;
  int startIndex = (page - 1) * ITEMS_PER_PAGE;
  int endIndex = startIndex + ITEMS_PER_PAGE;

  String html = webPageBegin("Gestión de Códigos - Controladora A2");
  html += R"=====(
   <h1>Gestión de Códigos - Controladora A2</h1>
 
   <div class='security-info'>
     <strong>🔒 Estado de Seguridad:</strong><br>
     • Acceso local: )=====";
 
   html += localAccessBlocked ? "🔒 BLOQUEADO" : "🔓 Permitido";
   html += "<br>• Intentos fallidos: " + String(failedAttempts) + "/" + String(maxFailedAttempts);
   html += "<br>• Duración bloqueo: " + String(blockDuration/1000) + " segundos";
 
   html += R"=====(
   </div>
 
  <div class='dual-info'>
    <strong>💡 Información del Sistema Dual:</strong><br>
    • <strong>NUEVO:</strong> Los códigos pueden restringirse a teclados específicos<br>
    • Teclado 1: GPIO 33/14 (Wiegand 1)<br>
    • Teclado 2: GPIO 4/16 (Wiegand 2)<br>
    • Cada código puede activar el relé 1 o 2 según configuración<br>
    • Capacidad máxima: 500 códigos<br>
    • Serial fijo MQTT: )=====";
   html += fixedSerialNumber;
 
  html += R"=====(
  </div>

  <!-- Sección de Importación/Exportación CSV -->
  <div style='background: #fff; padding: 20px; border-radius: 10px; margin: 20px 0; box-shadow: 0 2px 8px rgba(0,0,0,0.1);'>
    <h3>Gestión de Archivos CSV</h3>
    <p><strong>Formato CSV:</strong> Tipo,Codigo,Teclado,Rele,Fecha_Creacion</p>
    <p><strong>Tipos soportados:</strong> PIN (4-6 dígitos), TAG (1-16 caracteres) | <strong>Teclados:</strong> 1, 2 | <strong>Relés:</strong> 1, 2</p>
    
    <div style='display: flex; gap: 15px; align-items: center; flex-wrap: wrap; margin-top: 15px;'>
      <a href='/export/codes' style='text-decoration: none;'>
        <button style='background-color: #28a745; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer;'>
          Exportar a CSV
        </button>
      </a>
      
      <a href='/codes/template' style='text-decoration: none;'>
        <button style='background-color: #6c757d; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer;'>
          Descargar Plantilla
        </button>
      </a>
      
      <form action='/codes/bulk-import' method='post' enctype='multipart/form-data' style='display: flex; gap: 10px; align-items: center;'>
        <input type='file' name='csvFile' accept='.csv' required style='padding: 8px; border: 1px solid #ccc; border-radius: 4px;'>
        <button type='submit' style='background-color: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer;'>
          Importar CSV
        </button>
      </form>
    </div>
  </div>

  <!-- Sección de Lectura de Tags en Tiempo Real -->
  <div style='background: #fff; padding: 20px; border-radius: 10px; margin: 20px 0; box-shadow: 0 2px 8px rgba(0,0,0,0.1);'>
    <h3>Lectura de Tags en Tiempo Real</h3>
    
    <!-- Configuración -->
    <div id='configSection'>
      <h4>Configuración de Accesos:</h4>
      <div style='display: flex; gap: 20px; margin: 15px 0;'>
        <div>
          <h5>Teclado 1:</h5>
          <select id='k1r1' style='width: 120px; padding: 5px; margin: 2px;'>
            <option value='-1'>Deshabilitado</option>
            <option value='1'>Relé 1</option>
            <option value='2'>Relé 2</option>
          </select>
          <select id='k1r2' style='width: 120px; padding: 5px; margin: 2px;'>
            <option value='-1'>Deshabilitado</option>
            <option value='1'>Relé 1</option>
            <option value='2'>Relé 2</option>
          </select>
        </div>
        <div>
          <h5>Teclado 2:</h5>
          <select id='k2r1' style='width: 120px; padding: 5px; margin: 2px;'>
            <option value='-1'>Deshabilitado</option>
            <option value='1'>Relé 1</option>
            <option value='2'>Relé 2</option>
          </select>
          <select id='k2r2' style='width: 120px; padding: 5px; margin: 2px;'>
            <option value='-1'>Deshabilitado</option>
            <option value='1'>Relé 1</option>
            <option value='2'>Relé 2</option>
          </select>
        </div>
      </div>
      <label>
        <input type='checkbox' id='saveToMemory'> Guardar en memoria
      </label>
    </div>
    
    <!-- Controles -->
    <div style='margin: 20px 0;'>
      <button id='startReading' onclick='startTagReading()' style='background-color: #27ae60; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>
        Iniciar Lectura
      </button>
      <button id='stopReading' onclick='stopTagReading()' disabled style='background-color: #e74c3c; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>
        Parar Lectura
      </button>
      <button id='exportReadTags' onclick='exportReadTags()' disabled style='background-color: #3498db; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>
        Exportar CSV
      </button>
      <button id='loadReadTags' onclick='loadReadTags()' disabled style='background-color: #f39c12; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>
        Cargar a Memoria
      </button>
    </div>
    
    <!-- Estado -->
    <div style='margin: 15px 0; padding: 10px; background: #f8f9fa; border-radius: 5px;'>
      <p><strong>Estado:</strong> <span id='readingStatus'>Inactivo</span></p>
      <p><strong>Tags leídos:</strong> <span id='tagCount'>0</span></p>
    </div>
    
    <!-- Lista de tags leídos -->
    <div id='tagList' style='max-height: 200px; overflow-y: auto; border: 1px solid #ddd; padding: 10px; background: #f8f9fa; border-radius: 5px;'>
      <h4>Tags Leídos:</h4>
      <div id='tagListContent'>Ningún tag leído aún</div>
    </div>
  </div>

  <form action='/codes/add' method='post'>
    <h2>Añadir nuevo código</h2>
    <label for='type'>Tipo:</label>
    <select name='type'>
      <option value='PIN'>PIN (4-6 dígitos)</option>
      <option value='TAG'>TAG (tarjeta RFID/NFC)</option>
    </select>

    <label for='value'>Código:</label>
    <input type='text' name='value' maxlength='16' placeholder='Ej: 1234 o código de tarjeta' required>

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

  <!-- Buscador y filtros -->
  <div style='background: #f8f9fa; padding: 15px; border-radius: 8px; margin: 20px 0;'>
    <h3>Buscar Códigos</h3>
    <form method='GET' action='/codes' style='display: flex; gap: 10px; align-items: center; flex-wrap: wrap;'>
      <input type='text' name='search' placeholder='Buscar por código o tipo...' value=')=====";
  html += searchTerm;
  html += R"=====(' style='flex: 1; min-width: 200px; padding: 8px; border: 1px solid #ddd; border-radius: 4px;'>
      <button type='submit' style='background-color: #007bff; color: white; padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer;'>
        Buscar
      </button>
      <a href='/codes' style='background-color: #6c757d; color: white; padding: 8px 16px; text-decoration: none; border-radius: 4px;'>
        Limpiar
      </a>
    </form>
  </div>

   <h2>Códigos Almacenados ()=====";
   html += String(storedCodes->count) + "/" + String(MAX_CODES);
   html += R"=====()</h2>
  <table>
    <tr><th>Tipo</th><th>Valor</th><th>Teclado</th><th>Relé</th><th>Acción</th></tr>
 )=====" ;

  // Filtrar y paginar códigos
  int filteredCount = 0;
  int displayedCount = 0;
  
  for (int i = 0; i < storedCodes->count; i++) {
    // Aplicar filtro de búsqueda
    bool matchesFilter = true;
    if (searchTerm.length() > 0) {
      String codeType = String(storedCodes->codes[i].type);
      String codeValue = String(storedCodes->codes[i].value);
      String searchLower = searchTerm;
      searchLower.toLowerCase();
      
      String codeTypeLower = codeType;
      codeTypeLower.toLowerCase();
      String codeValueLower = codeValue;
      codeValueLower.toLowerCase();
      
      matchesFilter = (codeTypeLower.indexOf(searchLower) >= 0 || 
                      codeValueLower.indexOf(searchLower) >= 0);
    }
    
    if (matchesFilter) {
      filteredCount++;
      
      // Aplicar paginación
      if (filteredCount > startIndex && filteredCount <= endIndex) {
        html += "<tr>";
        html += "<td>" + String(storedCodes->codes[i].type) + "</td>";
        html += "<td>" + String(storedCodes->codes[i].value) + "</td>";
        
        String keyboardName = "Ambos";
        String keyboardIcon = "🔑";
        if (storedCodes->codes[i].keyboard_id == 1) {
          keyboardName = "Teclado 1";
          keyboardIcon = "🔑";
        } else if (storedCodes->codes[i].keyboard_id == 2) {
          keyboardName = "Teclado 2";
          keyboardIcon = "🔑";
        }
        
        html += "<td>" + keyboardIcon + " " + keyboardName + "</td>";
        html += "<td>⚡ Relé " + String(storedCodes->codes[i].relay) + "</td>";
        html += "<td><a class='delete' href='/codes/delete?type=" + String(storedCodes->codes[i].type);
        html += "&value=" + String(storedCodes->codes[i].value);
        html += "&keyboard=" + String(storedCodes->codes[i].keyboard_id) + "'>Eliminar</a></td>";
        html += "</tr>";
        displayedCount++;
      }
    }
  }

  if (displayedCount == 0) {
    if (searchTerm.length() > 0) {
      html += "<tr><td colspan='5'>No se encontraron códigos que coincidan con '" + searchTerm + "'</td></tr>";
    } else {
      html += "<tr><td colspan='5'>No hay códigos almacenados</td></tr>";
    }
  }
 
   html += R"=====(
   </table>
   
   <!-- Paginación -->
   )=====";
   
   // Generar paginación
   int totalPages = (filteredCount + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
   if (totalPages > 1) {
     html += "<div style='margin: 20px 0; text-align: center;'>";
     html += "<p style='margin-bottom: 10px;'>Página " + String(page) + " de " + String(totalPages) + " (Mostrando " + String(displayedCount) + " de " + String(filteredCount) + " códigos)</p>";
     
     // Botón anterior
     if (page > 1) {
       html += "<a href='/codes?page=" + String(page - 1);
       if (searchTerm.length() > 0) {
         html += "&search=" + searchTerm;
       }
       html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'>Anterior</a>";
     }
     
     // Números de página
     int startPage = max(1, page - 2);
     int endPage = min(totalPages, page + 2);
     
     for (int p = startPage; p <= endPage; p++) {
       if (p == page) {
         html += "<span style='margin: 0 5px; padding: 8px 12px; background-color: #6c757d; color: white; border-radius: 4px;'>" + String(p) + "</span>";
       } else {
         html += "<a href='/codes?page=" + String(p);
         if (searchTerm.length() > 0) {
           html += "&search=" + searchTerm;
         }
         html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'>" + String(p) + "</a>";
       }
     }
     
     // Botón siguiente
     if (page < totalPages) {
       html += "<a href='/codes?page=" + String(page + 1);
       if (searchTerm.length() > 0) {
         html += "&search=" + searchTerm;
       }
       html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'>Siguiente </a>";
     }
     
     html += "</div>";
   } else if (filteredCount > 0) {
     html += "<div style='margin: 20px 0; text-align: center;'>";
     html += "<p>Mostrando " + String(filteredCount) + " códigos</p>";
     html += "</div>";
   }
   
   html += R"=====(
   <br>
   
   
   
   <a href='/'><button style='background-color:#3498db'>Volver al inicio</button></a>
   
   <!-- Pie de página con datos de contacto -->
   <footer style='background-color: #2c3e50; color: white; padding: 20px; text-align: center; margin-top: 30px;'>
     <div style='max-width: 800px; margin: 0 auto;'>
       <h3 style='margin: 0 0 10px 0; color: #ecf0f1;'>Smart World And Things SLU</h3>
       <p style='margin: 5px 0; font-size: 14px;'>
         <strong>Web:</strong> <a href='https://www.swat-id.com' style='color: #3498db; text-decoration: none;'>www.swat-id.com</a> | 
         <strong>Tel:</strong> 633 44 84 27 | 
         <strong>Email:</strong> <a href='mailto:info@swat-id.com' style='color: #3498db; text-decoration: none;'>info@swat-id.com</a>
       </p>
       <p style='margin: 5px 0; font-size: 12px; color: #bdc3c7;'>Controladora A2 - SWATID | Sistema de Control de Acceso Dual Wiegand</p>
     </div>
   </footer>
   
   <!-- JavaScript para lectura de tags -->
   <script>
   let pollingInterval = null;
   
   function startTagReading() {
     // Obtener configuración del formulario
     const config = {
       k1r1: parseInt(document.getElementById('k1r1').value),
       k1r2: parseInt(document.getElementById('k1r2').value),
       k2r1: parseInt(document.getElementById('k2r1').value),
       k2r2: parseInt(document.getElementById('k2r2').value),
       saveToMemory: document.getElementById('saveToMemory').checked
     };
     
     // Validar configuración
     if (config.k1r1 === -1 && config.k1r2 === -1 && config.k2r1 === -1 && config.k2r2 === -1) {
       alert('Debe configurar al menos un acceso');
       return;
     }
     
     // Enviar configuración al servidor
     fetch('/codes/start-tag-reading', {
       method: 'POST',
       headers: {'Content-Type': 'application/json'},
       body: JSON.stringify(config)
     })
     .then(response => response.json())
     .then(data => {
       if (data.status === 'started') {
         // Actualizar interfaz
         document.getElementById('startReading').disabled = true;
         document.getElementById('stopReading').disabled = false;
         document.getElementById('readingStatus').textContent = 'Activo';
         document.getElementById('configSection').style.opacity = '0.5';
         
         // Iniciar polling para actualizar lista
         startPolling();
       } else {
         alert('Error: ' + data.error);
       }
     })
     .catch(error => {
       console.error('Error:', error);
       alert('Error al iniciar lectura');
     });
   }
   
   function stopTagReading() {
     fetch('/codes/stop-tag-reading', {method: 'POST'})
     .then(response => response.json())
     .then(data => {
       // Actualizar interfaz
       document.getElementById('startReading').disabled = false;
       document.getElementById('stopReading').disabled = true;
       document.getElementById('exportReadTags').disabled = false;
       document.getElementById('loadReadTags').disabled = false;
       document.getElementById('readingStatus').textContent = 'Finalizado';
       document.getElementById('configSection').style.opacity = '1';
       
       stopPolling();
       
       if (data.count > 0) {
         alert('Lectura finalizada. Se leyeron ' + data.count + ' tags.');
       }
     })
     .catch(error => {
       console.error('Error:', error);
       alert('Error al parar lectura');
     });
   }
   
   function exportReadTags() {
     window.location.href = '/codes/export-read-tags';
   }
   
   function loadReadTags() {
     if (confirm('¿Está seguro de que desea cargar todos los tags leídos a la memoria? Esta acción no se puede deshacer.')) {
       fetch('/codes/load-read-tags', {method: 'POST'})
       .then(response => response.json())
       .then(data => {
         if (data.success) {
           alert('✅ ' + data.loaded + ' tags cargados a memoria correctamente');
           // Actualizar la lista de tags para mostrar que están guardados
           updateTagList();
         } else {
           alert('❌ Error al cargar tags: ' + data.error);
         }
       })
       .catch(error => {
         console.error('Error:', error);
         alert('❌ Error al cargar tags a memoria');
       });
     }
   }
   
   function startPolling() {
     pollingInterval = setInterval(updateTagList, 1000); // Actualizar cada segundo
   }
   
   function stopPolling() {
     if (pollingInterval) {
       clearInterval(pollingInterval);
       pollingInterval = null;
     }
   }
   
   function updateTagList() {
     fetch('/codes/read-tags-status')
     .then(response => response.json())
     .then(data => {
       document.getElementById('tagCount').textContent = data.count;
       updateTagListDisplay(data.tags);
     })
     .catch(error => {
       console.error('Error actualizando lista:', error);
     });
   }
   
   function updateTagListDisplay(tags) {
     const content = document.getElementById('tagListContent');
     if (tags.length === 0) {
       content.innerHTML = 'Ningún tag leído aún';
     } else {
       let html = '<ul>';
       tags.forEach(tag => {
         const date = new Date(tag.timestamp);
         const timeStr = date.toLocaleTimeString();
         const savedIcon = tag.saved ? '✅' : '📝';
         html += `<li>${savedIcon} ${tag.code} (${timeStr})</li>`;
       });
       html += '</ul>';
       content.innerHTML = html;
     }
   }
   </script>
 </body></html>
 )=====" ;
 
   server.send(200, "text/html", html);
 }
 
 void handleCodesAdd() {
   if (!webAuth()) return;
 
  if (server.hasArg("type") && server.hasArg("value") && server.hasArg("relay") && server.hasArg("keyboard_id")) {
    String type = server.arg("type");
    String value = server.arg("value");
    int relay = server.arg("relay").toInt();
    int keyboardId = server.arg("keyboard_id").toInt();

    value.trim();
    type.trim();

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
        String keyboardName = (keyboardId == 0) ? "Ambos teclados" : 
                             (keyboardId == 1) ? "Teclado 1" : "Teclado 2";
        Serial.printf("✅ Código añadido: %s %s -> %s, Relé %d\n", type.c_str(), value.c_str(), keyboardName.c_str(), relay);
         server.sendHeader("Location", "/codes");
         server.send(303);
         return;
       }
     }
     
     server.send(200, "text/html",
       "<html><head><meta charset='UTF-8'></head><body><h1>❌ Error al añadir código</h1>"
       "<p>Verifique el formato y que no exista ya</p>"
       "<a href='/codes'>Volver</a></body></html>");
   } else {
     server.send(400, "text/html",
       "<html><head><meta charset='UTF-8'></head><body><h1>❌ Faltan parámetros</h1>"
       "<a href='/codes'>Volver</a></body></html>");
   }
 }
 
 void handleCodesDelete() {
   if (!webAuth()) return;
 
  if (server.hasArg("type") && server.hasArg("value")) {
    String type = server.arg("type");
    String value = server.arg("value");
    int keyboardId = server.hasArg("keyboard") ? server.arg("keyboard").toInt() : -1;

    bool deleted = deleteCode(type.c_str(), value.c_str(), keyboardId);
    if (deleted) {
      String keyboardName = (keyboardId == -1) ? "cualquier teclado" : 
                           (keyboardId == 0) ? "ambos teclados" :
                           (keyboardId == 1) ? "teclado 1" : "teclado 2";
      Serial.printf("🗑️ Código eliminado: %s %s (%s)\n", type.c_str(), value.c_str(), keyboardName.c_str());
    }
 
     server.sendHeader("Location", "/codes");
     server.send(303);
   } else {
     server.send(400, "text/html", 
       "<html><body><h1>❌ Faltan parámetros</h1>"
       "<a href='/codes'>Volver</a></body></html>");
   }
 }
 
 
 void handleNotFound() {
   server.send(404, "text/html", 
     "<html><body><h1>❌ Página no encontrada</h1>"
     "<a href='/'>Ir al inicio</a></body></html>");
 }
 
 void controlRele() {
   Serial.printf("⚡ Activando relé 1 por %.1f segundos (modo legacy)\n", releDuration);
   digitalWrite(RELE1_PIN, HIGH);
   delay(releDuration * 1000);
   digitalWrite(RELE1_PIN, LOW);
   Serial.println("⚡ Relé 1 apagado (modo legacy)");
 }
 
 // =================== FUNCIONES AUXILIARES ADICIONALES ===================
 String getLastSixMAC() {
   uint8_t mac[6];
   char macStr[7];
   
   if (ETH.macAddress(mac)) {
     sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
   } else {
     // Si no podemos obtener la MAC de Ethernet, usar WiFi
     WiFi.macAddress(mac);
     sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
   }
   
   return String(macStr);
 }
 
// =================== FUNCIONES BLE (v4.1) ===================
#ifdef ENABLE_BLE

// Calcular checksum para BLEAuthConfig
uint32_t calculateBLEChecksum(const BLEAuthConfig& cfg) {
  uint32_t sum = 0;
  for (int i = 0; i < BLE_KEY_SIZE; i++) {
    sum += cfg.superadmin_key[i];
  }
  for (int u = 0; u < BLE_MAX_USERS; u++) {
    sum += cfg.user_enabled[u];
    sum += cfg.user_permissions[u];
    for (int i = 0; i < BLE_KEY_SIZE; i++) {
      sum += cfg.user_keys[u][i];
    }
  }
  return sum ^ 0xB1E4C0DE;
}

// =================== FUNCIONES DE SEGURIDAD BLE v4.1 ===================

// Generar nonce aleatorio para challenge
void generateBLEChallenge() {
  esp_fill_random(bleSessionNonce, 16);
  bleChallengeReady = true;
  bleConnectionTime = millis();  // Reiniciar timeout de challenge
  Serial.println("🔐 [BLE] Challenge generado (16 bytes)");
  Serial.printf("🔐 [BLE] Nonce: ");
  for (int i = 0; i < 16; i++) {
    Serial.printf("%02X", bleSessionNonce[i]);
  }
  Serial.println();
}

// Generar token de sesión
void generateBLESessionToken() {
  esp_fill_random(bleSessionToken, 8);
  bleSessionValid = true;
  bleSessionTime = millis();
  Serial.println("🔐 [BLE] Token de sesión generado (8 bytes)");
  Serial.printf("🔐 [BLE] Token: ");
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X", bleSessionToken[i]);
  }
  Serial.println();
}

// Calcular SHA256 de clave + nonce
void calculateSHA256Response(const uint8_t* key, const uint8_t* nonce, uint8_t* output) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (1 = SHA224)
  mbedtls_sha256_update(&ctx, key, BLE_KEY_SIZE);  // 64 bytes de clave
  mbedtls_sha256_update(&ctx, nonce, 16);          // 16 bytes de nonce
  mbedtls_sha256_finish(&ctx, output);             // 32 bytes de salida
  mbedtls_sha256_free(&ctx);
}

// Verificar response del challenge contra todas las claves almacenadas
// Retorna: -1 = no coincide, 0 = superadmin, 1-5 = usuario (slot)
int verifyBLEChallengeResponse(const uint8_t* response) {
  if (!bleChallengeReady) {
    Serial.println("🔐 [BLE] Error: No hay challenge pendiente");
    return -1;
  }
  
  uint8_t expected[32];
  
  // Verificar contra superadmin
  if (bleAuthConfig.superadmin_registered) {
    calculateSHA256Response(bleAuthConfig.superadmin_key, bleSessionNonce, expected);
    if (memcmp(response, expected, 32) == 0) {
      Serial.println("🔐 [BLE] ✓ Response válido - SUPERADMIN");
      bleChallengeReady = false;
      return 0;
    }
  }
  
  // Verificar contra usuarios registrados
  for (int u = 0; u < BLE_MAX_USERS; u++) {
    if (bleAuthConfig.user_enabled[u]) {
      calculateSHA256Response(bleAuthConfig.user_keys[u], bleSessionNonce, expected);
      if (memcmp(response, expected, 32) == 0) {
        Serial.printf("🔐 [BLE] ✓ Response válido - Usuario %d (%s)\n", 
                      u + 1, bleAuthConfig.user_names[u]);
        bleChallengeReady = false;
        return u + 1;
      }
    }
  }
  
  Serial.println("🔐 [BLE] ✗ Response inválido - No coincide con ninguna clave");
  return -1;
}

// Verificar token de sesión en operaciones BLE
// Los primeros 8 bytes del comando deben ser el token válido
bool verifyBLESessionToken(const uint8_t* receivedToken) {
  if (!bleSessionValid) {
    Serial.println("🔐 [BLE] Token rechazado: No hay sesión activa");
    return false;
  }
  
  // Verificar timeout de sesión por inactividad
  unsigned long now = millis();
  if (now - bleSessionTime > BLE_SESSION_TIMEOUT_MS) {
    Serial.println("🔐 [BLE] Token rechazado: Sesión expirada por inactividad");
    bleSessionValid = false;
    bleAuthenticated = false;
    return false;
  }
  
  // Verificar token
  if (memcmp(receivedToken, bleSessionToken, 8) != 0) {
    Serial.println("🔐 [BLE] Token rechazado: No coincide");
    Serial.printf("🔐 [BLE] Recibido: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                  receivedToken[0], receivedToken[1], receivedToken[2], receivedToken[3],
                  receivedToken[4], receivedToken[5], receivedToken[6], receivedToken[7]);
    return false;
  }
  
  // Token válido - actualizar timestamp de actividad
  bleSessionTime = now;
  return true;
}

// Invalidar sesión BLE (al desconectar o por timeout)
void invalidateBLESession() {
  bleSessionValid = false;
  bleAuthenticated = false;
  bleChallengeReady = false;
  bleCurrentPermissions = 0;
  bleConnectedUser = "";
  memset(bleSessionToken, 0, 8);
  memset(bleSessionNonce, 0, 16);
  Serial.println("🔐 [BLE] Sesión invalidada");
}

// Convertir token a string hexadecimal
String tokenToHex(const uint8_t* token, size_t len) {
  String hex = "";
  for (size_t i = 0; i < len; i++) {
    char buf[3];
    sprintf(buf, "%02X", token[i]);
    hex += buf;
  }
  return hex;
}

// =================== FUNCIONES DE CLAVE MAESTRA Y HKDF (v4.1) ===================

// Calcular checksum para DeviceKeyConfig
uint32_t calculateDeviceKeyChecksum(const DeviceKeyConfig& cfg) {
  uint32_t sum = 0;
  for (int i = 0; i < DEVICE_MASTER_KEY_SIZE; i++) {
    sum += cfg.master_key[i];
  }
  sum += cfg.key_version;
  sum += cfg.generation_time;
  return sum ^ 0xDE41CE41;
}

// Cargar configuración de clave maestra desde EEPROM
void loadDeviceKeyConfig() {
  Serial.println("🔑 [MQTT] Cargando clave maestra del dispositivo...");
  
  EEPROM.get(EEPROM_DEVICE_KEY_OFFSET, deviceKeyConfig);
  
  if (deviceKeyConfig.validMarker != DEVICE_KEY_CONFIG_MARKER) {
    Serial.println("🔑 [MQTT] Primera ejecución - Generando clave maestra aleatoria...");
    
    // Generar clave maestra aleatoria (32 bytes)
    esp_fill_random(deviceKeyConfig.master_key, DEVICE_MASTER_KEY_SIZE);
    
    deviceKeyConfig.validMarker = DEVICE_KEY_CONFIG_MARKER;
    deviceKeyConfig.key_version = 1;
    deviceKeyConfig.generation_time = millis() / 1000;  // Segundos desde boot
    deviceKeyConfig.checksum = calculateDeviceKeyChecksum(deviceKeyConfig);
    
    // Guardar en EEPROM
    EEPROM.put(EEPROM_DEVICE_KEY_OFFSET, deviceKeyConfig);
    if (EEPROM.commit()) {
      Serial.println("🔑 [MQTT] ✓ Clave maestra generada y guardada");
      Serial.printf("🔑 [MQTT] Key preview: %02X%02X%02X%02X...%02X%02X%02X%02X\n",
                    deviceKeyConfig.master_key[0], deviceKeyConfig.master_key[1],
                    deviceKeyConfig.master_key[2], deviceKeyConfig.master_key[3],
                    deviceKeyConfig.master_key[28], deviceKeyConfig.master_key[29],
                    deviceKeyConfig.master_key[30], deviceKeyConfig.master_key[31]);
    } else {
      Serial.println("🔑 [MQTT] ✗ Error guardando clave maestra");
    }
  } else {
    // Verificar checksum
    uint32_t calculated = calculateDeviceKeyChecksum(deviceKeyConfig);
    if (deviceKeyConfig.checksum != calculated) {
      Serial.println("⚠️ [MQTT] Checksum de clave maestra inválido - Regenerando...");
      esp_fill_random(deviceKeyConfig.master_key, DEVICE_MASTER_KEY_SIZE);
      deviceKeyConfig.key_version++;
      deviceKeyConfig.checksum = calculateDeviceKeyChecksum(deviceKeyConfig);
      EEPROM.put(EEPROM_DEVICE_KEY_OFFSET, deviceKeyConfig);
      EEPROM.commit();
    }
    
    Serial.printf("🔑 [MQTT] ✓ Clave maestra cargada (versión %d)\n", deviceKeyConfig.key_version);
    Serial.printf("🔑 [MQTT] Key preview: %02X%02X%02X%02X...%02X%02X%02X%02X\n",
                  deviceKeyConfig.master_key[0], deviceKeyConfig.master_key[1],
                  deviceKeyConfig.master_key[2], deviceKeyConfig.master_key[3],
                  deviceKeyConfig.master_key[28], deviceKeyConfig.master_key[29],
                  deviceKeyConfig.master_key[30], deviceKeyConfig.master_key[31]);
  }
}

// Implementación manual de HKDF-SHA256 (Extract-and-Expand)
// Porque mbedtls_hkdf no está disponible en esta versión de ESP-IDF

// HKDF-Extract: PRK = HMAC-Hash(salt, IKM)
bool hkdfExtract(const uint8_t* salt, size_t saltLen, 
                 const uint8_t* ikm, size_t ikmLen,
                 uint8_t* prk) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) return false;
  
  if (mbedtls_md_setup(&ctx, md, 1) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }
  
  mbedtls_md_hmac_starts(&ctx, salt, saltLen);
  mbedtls_md_hmac_update(&ctx, ikm, ikmLen);
  mbedtls_md_hmac_finish(&ctx, prk);
  
  mbedtls_md_free(&ctx);
  return true;
}

// HKDF-Expand: OKM = T(1) || T(2) || ... donde T(i) = HMAC-Hash(PRK, T(i-1) || info || i)
bool hkdfExpand(const uint8_t* prk, size_t prkLen,
                const uint8_t* info, size_t infoLen,
                uint8_t* okm, size_t okmLen) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) return false;
  
  if (mbedtls_md_setup(&ctx, md, 1) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }
  
  uint8_t t[32];  // SHA256 output size
  size_t tLen = 0;
  size_t pos = 0;
  uint8_t counter = 1;
  
  while (pos < okmLen) {
    mbedtls_md_hmac_starts(&ctx, prk, prkLen);
    
    if (tLen > 0) {
      mbedtls_md_hmac_update(&ctx, t, tLen);
    }
    
    mbedtls_md_hmac_update(&ctx, info, infoLen);
    mbedtls_md_hmac_update(&ctx, &counter, 1);
    mbedtls_md_hmac_finish(&ctx, t);
    
    tLen = 32;  // SHA256 output
    size_t copyLen = (okmLen - pos < 32) ? (okmLen - pos) : 32;
    memcpy(okm + pos, t, copyLen);
    pos += copyLen;
    counter++;
  }
  
  mbedtls_md_free(&ctx);
  return true;
}

// Derivar clave de usuario usando HKDF-SHA256
// info = "user_key:" + user_id
// output = 64 bytes (BLE_KEY_SIZE)
bool deriveUserKey(const char* userId, uint8_t* outputKey) {
  Serial.printf("🔑 [MQTT] Derivando clave para usuario: %s\n", userId);
  
  // Construir info string
  String info = "user_key:" + String(userId);
  
  // Salt = serial number del dispositivo
  String salt = fixedSerialNumber;
  
  // Paso 1: Extract - PRK = HMAC(salt, master_key)
  uint8_t prk[32];  // SHA256 output
  if (!hkdfExtract((const uint8_t*)salt.c_str(), salt.length(),
                   deviceKeyConfig.master_key, DEVICE_MASTER_KEY_SIZE, prk)) {
    Serial.println("🔑 [MQTT] ✗ Error en HKDF-Extract");
    return false;
  }
  
  // Paso 2: Expand - OKM = HKDF-Expand(PRK, info, 64)
  if (!hkdfExpand(prk, 32, (const uint8_t*)info.c_str(), info.length(),
                  outputKey, BLE_KEY_SIZE)) {
    Serial.println("🔑 [MQTT] ✗ Error en HKDF-Expand");
    return false;
  }
  
  Serial.printf("🔑 [MQTT] ✓ Clave derivada: %02X%02X%02X%02X...%02X%02X%02X%02X\n",
                outputKey[0], outputKey[1], outputKey[2], outputKey[3],
                outputKey[60], outputKey[61], outputKey[62], outputKey[63]);
  return true;
}

// Obtener la clave maestra en formato hex (para registro inicial en servidor)
// NOTA: Esto solo debe exponerse durante el registro del dispositivo, no en producción
String getDeviceMasterKeyHex() {
  return tokenToHex(deviceKeyConfig.master_key, DEVICE_MASTER_KEY_SIZE);
}

// Cargar configuración BLE desde EEPROM
void loadBLEAuthConfig() {
  Serial.println("🔵 [BLE] Cargando configuración de autenticación...");
  Serial.printf("🔵 [BLE] Tamaño estructura BLEAuthConfig: %d bytes\n", sizeof(BLEAuthConfig));
  Serial.printf("🔵 [BLE] Offset EEPROM: %d\n", EEPROM_BLE_AUTH_OFFSET);
  
  EEPROM.get(EEPROM_BLE_AUTH_OFFSET, bleAuthConfig);
  
  // Verificar marcador válido
  if (bleAuthConfig.validMarker != BLE_AUTH_CONFIG_MARKER) {
    Serial.printf("🔵 [BLE] Marcador inválido: 0x%08X (esperado: 0x%08X)\n", 
                  bleAuthConfig.validMarker, BLE_AUTH_CONFIG_MARKER);
    Serial.println("🔵 [BLE] Primera ejecución - Inicializando configuración");
    memset(&bleAuthConfig, 0, sizeof(BLEAuthConfig));
    bleAuthConfig.validMarker = BLE_AUTH_CONFIG_MARKER;
    bleAuthConfig.superadmin_registered = 0;
    for (int i = 0; i < BLE_MAX_USERS; i++) {
      bleAuthConfig.user_enabled[i] = 0;
      bleAuthConfig.user_permissions[i] = 0;
    }
    saveBLEAuthConfig();
  } else {
    // Verificar checksum
    uint32_t storedChecksum = bleAuthConfig.checksum;
    uint32_t calculatedChecksum = calculateBLEChecksum(bleAuthConfig);
    
    if (storedChecksum != calculatedChecksum) {
      Serial.printf("⚠️ [BLE] Checksum inválido: guardado=0x%08X, calculado=0x%08X\n",
                    storedChecksum, calculatedChecksum);
      Serial.println("⚠️ [BLE] ADVERTENCIA: Datos pueden estar corruptos");
      // NO reinicializar automáticamente - mantener datos existentes pero advertir
    }
    
    Serial.printf("🔵 [BLE] Configuración cargada - Superadmin: %s (valor raw: %d)\n",
                  bleAuthConfig.superadmin_registered ? "SÍ REGISTRADO" : "NO registrado",
                  bleAuthConfig.superadmin_registered);
    
    // Mostrar primeros bytes de la clave del superadmin para debug
    if (bleAuthConfig.superadmin_registered) {
      Serial.printf("🔵 [BLE] Superadmin key (primeros 8 bytes): %02X%02X%02X%02X%02X%02X%02X%02X\n",
                    bleAuthConfig.superadmin_key[0], bleAuthConfig.superadmin_key[1],
                    bleAuthConfig.superadmin_key[2], bleAuthConfig.superadmin_key[3],
                    bleAuthConfig.superadmin_key[4], bleAuthConfig.superadmin_key[5],
                    bleAuthConfig.superadmin_key[6], bleAuthConfig.superadmin_key[7]);
    }
    
    int userCount = 0;
    for (int i = 0; i < BLE_MAX_USERS; i++) {
      if (bleAuthConfig.user_enabled[i]) {
        userCount++;
        Serial.printf("🔵 [BLE] Usuario %d habilitado: %s\n", i + 1, bleAuthConfig.user_names[i]);
      }
    }
    Serial.printf("🔵 [BLE] Total usuarios activos: %d\n", userCount);
  }
}

// Declaración forward de updateBLEDeviceInfo
void updateBLEDeviceInfo();

// Guardar configuración BLE en EEPROM
void saveBLEAuthConfig() {
  bleAuthConfig.validMarker = BLE_AUTH_CONFIG_MARKER;
  bleAuthConfig.checksum = calculateBLEChecksum(bleAuthConfig);
  
  EEPROM.put(EEPROM_BLE_AUTH_OFFSET, bleAuthConfig);
  if (EEPROM.commit()) {
    Serial.println("🔵 [BLE] Configuración guardada correctamente");
    // Actualizar la característica de info del dispositivo (FF08)
    // para que la APP vea los cambios en superadmin_registered y active_users
    updateBLEDeviceInfo();
  } else {
    Serial.println("❌ [BLE] Error al guardar configuración");
  }
}

// Verificar clave de autenticación
int verifyBLEKey(const uint8_t* key) {
  Serial.println("🔵 [BLE] === Verificando clave ===");
  
  // Verificar integridad de la configuración
  uint32_t calculatedChecksum = calculateBLEChecksum(bleAuthConfig);
  if (bleAuthConfig.checksum != calculatedChecksum) {
    Serial.println("⚠️ [BLE] ADVERTENCIA: Checksum de configuración no coincide");
    Serial.printf("⚠️ [BLE] Checksum guardado: 0x%08X, calculado: 0x%08X\n", 
                  bleAuthConfig.checksum, calculatedChecksum);
  }
  
  // Mostrar clave recibida (primeros 8 bytes)
  Serial.printf("🔵 [BLE] Clave a verificar: %02X%02X%02X%02X%02X%02X%02X%02X...\n",
                key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
  
  // Verificar si es superadmin
  if (bleAuthConfig.superadmin_registered) {
    Serial.println("🔵 [BLE] Comparando con clave de SUPERADMIN...");
    Serial.printf("🔵 [BLE] Superadmin key: %02X%02X%02X%02X%02X%02X%02X%02X...\n",
                  bleAuthConfig.superadmin_key[0], bleAuthConfig.superadmin_key[1],
                  bleAuthConfig.superadmin_key[2], bleAuthConfig.superadmin_key[3],
                  bleAuthConfig.superadmin_key[4], bleAuthConfig.superadmin_key[5],
                  bleAuthConfig.superadmin_key[6], bleAuthConfig.superadmin_key[7]);
    
    bool match = (memcmp(bleAuthConfig.superadmin_key, key, BLE_KEY_SIZE) == 0);
    if (match) {
      Serial.println("🔵 [BLE] ✓ Clave COINCIDE con SUPERADMIN");
      return 0; // 0 = superadmin
    } else {
      Serial.println("🔵 [BLE] ✗ Clave NO coincide con superadmin");
    }
  } else {
    Serial.println("🔵 [BLE] No hay superadmin registrado - Solo auto-registro permitido");
  }
  
  // Verificar usuarios registrados
  Serial.println("🔵 [BLE] Verificando contra usuarios registrados...");
  int usersChecked = 0;
  for (int u = 0; u < BLE_MAX_USERS; u++) {
    if (bleAuthConfig.user_enabled[u]) {
      usersChecked++;
      Serial.printf("🔵 [BLE] Slot %d: Usuario '%s' (permisos=0x%02X)\n", 
                    u + 1, bleAuthConfig.user_names[u], bleAuthConfig.user_permissions[u]);
      Serial.printf("🔵 [BLE] Slot %d key: %02X%02X%02X%02X%02X%02X%02X%02X...\n",
                    u + 1,
                    bleAuthConfig.user_keys[u][0], bleAuthConfig.user_keys[u][1],
                    bleAuthConfig.user_keys[u][2], bleAuthConfig.user_keys[u][3],
                    bleAuthConfig.user_keys[u][4], bleAuthConfig.user_keys[u][5],
                    bleAuthConfig.user_keys[u][6], bleAuthConfig.user_keys[u][7]);
      
      bool match = (memcmp(bleAuthConfig.user_keys[u], key, BLE_KEY_SIZE) == 0);
      if (match) {
        Serial.printf("🔵 [BLE] ✓ Clave COINCIDE con Usuario %d (%s)\n", u + 1, bleAuthConfig.user_names[u]);
        return u + 1; // 1-5 = usuarios
      }
    }
  }
  
  Serial.printf("🔵 [BLE] ✗ Clave NO coincide. Usuarios revisados: %d de %d\n", usersChecked, BLE_MAX_USERS);
  return -1; // No autenticado
}

// Callback para conexiones BLE (v4.1 con soporte de sesiones seguras)
class SWATIDServerCallbacks: public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    bleDeviceConnected = true;
    invalidateBLESession();  // Limpiar cualquier sesión anterior
    bleConnectionTime = millis();  // Registrar momento de conexión
    Serial.println("🔵 [BLE] Dispositivo conectado - Esperando autenticación...");
    Serial.printf("🔵 [BLE] Timeout de autenticación: %d segundos\n", BLE_AUTH_TIMEOUT_MS / 1000);
    Serial.println("🔐 [BLE] v4.1: Usar FF0B para obtener challenge, luego FF01 con SHA256(key+nonce)");
  }

  void onDisconnect(NimBLEServer* pServer) {
    bleDeviceConnected = false;
    invalidateBLESession();  // Invalidar sesión al desconectar
    bleConnectionTime = 0;
    Serial.println("🔵 [BLE] Dispositivo desconectado - Sesión invalidada");
    NimBLEDevice::startAdvertising();
  }
};

// =================== CALLBACK FF0B - CHALLENGE (v4.1) ===================
// Esta característica genera un nonce aleatorio para autenticación segura
// La APP debe: 1) Leer FF0B para obtener challenge, 2) Calcular SHA256(key+nonce), 3) Enviar a FF01
class ChallengeCharCallbacks: public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔐 [BLE] FF0B - Solicitud de challenge para auth seguro");
    
    // Generar nuevo nonce aleatorio (16 bytes)
    generateBLEChallenge();
    
    // Devolver el nonce
    pCharacteristic->setValue(bleSessionNonce, 16);
    
    Serial.println("🔐 [BLE] FF0B - Challenge enviado, esperando response en FF01...");
  }
};

// =================== CALLBACK FF01 - AUTENTICACIÓN (v4.1 con Challenge-Response) ===================
class AuthCharCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    Serial.printf("🔵 [BLE] FF01 - Recibidos %d bytes para autenticación\n", value.length());
    
    // =================== v4.1: CHALLENGE-RESPONSE (32 bytes = SHA256) ===================
    if (value.length() == 32 && bleChallengeReady) {
      Serial.println("🔐 [BLE] FF01 - Modo Challenge-Response (v4.1 seguro)");
      const uint8_t* response = (const uint8_t*)value.data();
      
      // Verificar response contra todas las claves
      int userIndex = verifyBLEChallengeResponse(response);
      
      if (userIndex >= 0) {
        // Autenticación exitosa - generar token de sesión
        generateBLESessionToken();
        
        bleAuthenticated = true;
        if (userIndex == 0) {
          bleCurrentPermissions = BLE_PERM_ADMIN;
          bleConnectedUser = "SUPERADMIN";
          Serial.println("🔐 [BLE] ✓ Challenge-Response OK - SUPERADMIN");
        } else {
          bleCurrentPermissions = bleAuthConfig.user_permissions[userIndex - 1];
          bleConnectedUser = String(bleAuthConfig.user_names[userIndex - 1]);
          Serial.printf("🔐 [BLE] ✓ Challenge-Response OK - Usuario %d (%s)\n", 
                        userIndex, bleConnectedUser.c_str());
        }
        
        // Actualizar valores de FF09 y FF0A tras autenticación
        updateFF09Value();
        updateFF0AValue(0);
        
        // Responder con token de sesión (8 bytes hex = 16 chars)
        String tokenHex = tokenToHex(bleSessionToken, 8);
        String response = "OK:TOKEN:" + tokenHex;
        pCharacteristic->setValue((uint8_t*)response.c_str(), response.length());
        Serial.printf("🔐 [BLE] Token de sesión: %s\n", tokenHex.c_str());
        
        // Publicar evento de autenticación exitosa
        if (mqttClient.connected()) {
          String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
          String eventMsg = String("{") +
            "\"event\":\"AUTH_SUCCESS\"," +
            "\"method\":\"challenge_response\"," +
            "\"user\":\"" + bleConnectedUser + "\"," +
            "\"timestamp\":\"" + getTimestamp() + "\"" +
          "}";
          mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
        }
      } else {
        // Response inválido
        invalidateBLESession();
        String err = "ERROR:INVALID_RESPONSE";
        pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
        
        // Publicar evento de fallo
        if (mqttClient.connected()) {
          String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
          String eventMsg = String("{") +
            "\"event\":\"AUTH_FAILED\"," +
            "\"method\":\"challenge_response\"," +
            "\"reason\":\"invalid_response\"," +
            "\"timestamp\":\"" + getTimestamp() + "\"" +
          "}";
          mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
        }
      }
      pCharacteristic->notify();
      return;
    }
    
    // =================== MODO LEGADO (64 bytes = clave directa) ===================
    // NOTA: Este modo se mantiene para compatibilidad con APPs antiguas
    // En una futura versión puede eliminarse para forzar Challenge-Response
    if (value.length() == BLE_KEY_SIZE) {
      Serial.println("⚠️ [BLE] FF01 - Modo LEGADO (clave directa) - Considere usar Challenge-Response");
      const uint8_t* key = (const uint8_t*)value.data();
      
      // Mostrar primeros bytes de la clave recibida para debug
      Serial.printf("🔵 [BLE] Clave recibida (primeros 8 bytes): %02X%02X%02X%02X%02X%02X%02X%02X\n",
                    key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
      
      Serial.printf("🔵 [BLE] Estado actual: superadmin_registered=%d\n", bleAuthConfig.superadmin_registered);
      
      // Si no hay superadmin registrado, el primero se convierte en superadmin
      if (!bleAuthConfig.superadmin_registered) {
        Serial.println("🔵 [BLE] *** NO HAY SUPERADMIN - Registrando nuevo ***");
        memcpy(bleAuthConfig.superadmin_key, key, BLE_KEY_SIZE);
        bleAuthConfig.superadmin_registered = 1;
        saveBLEAuthConfig();
        
        bleAuthenticated = true;
        bleCurrentPermissions = BLE_PERM_ADMIN;
        bleConnectedUser = "SUPERADMIN";
        
        // Generar token de sesión incluso en modo legado
        generateBLESessionToken();
        
        // Actualizar valores de FF09 y FF0A tras autenticación
        updateFF09Value();
        updateFF0AValue(0);
        
        Serial.println("🔵 [BLE] ¡Nuevo SUPERADMIN registrado!");
        String tokenHex = tokenToHex(bleSessionToken, 8);
        String ok = "OK:SUPERADMIN_REGISTERED:TOKEN:" + tokenHex;
        pCharacteristic->setValue((uint8_t*)ok.c_str(), ok.length());
        pCharacteristic->notify();
        return;
      }
      
      // YA HAY SUPERADMIN - Solo verificar credenciales, NO registrar nuevo
      Serial.println("🔵 [BLE] Ya existe SUPERADMIN - Verificando credenciales...");
      
      // Verificar credenciales
      int userIndex = verifyBLEKey(key);
      Serial.printf("🔵 [BLE] Resultado verificación: userIndex=%d\n", userIndex);
      
      if (userIndex >= 0) {
        bleAuthenticated = true;
        
        // Generar token de sesión
        generateBLESessionToken();
        
        if (userIndex == 0) {
          bleCurrentPermissions = BLE_PERM_ADMIN;
          bleConnectedUser = "SUPERADMIN";
          Serial.println("🔵 [BLE] ✓ Autenticado como SUPERADMIN");
        } else {
          bleCurrentPermissions = bleAuthConfig.user_permissions[userIndex - 1];
          bleConnectedUser = String(bleAuthConfig.user_names[userIndex - 1]);
          Serial.printf("🔵 [BLE] ✓ Autenticado como Usuario %d (%s)\n", userIndex, bleConnectedUser.c_str());
        }
        
        // Actualizar valores de FF09 y FF0A tras autenticación
        updateFF09Value();
        updateFF0AValue(0);
        
        // Responder con token (modo legado también genera token)
        String tokenHex = tokenToHex(bleSessionToken, 8);
        String ok = "OK:AUTHENTICATED:TOKEN:" + tokenHex;
        pCharacteristic->setValue((uint8_t*)ok.c_str(), ok.length());
      } else {
        invalidateBLESession();
        Serial.println("🔵 [BLE] ✗ Autenticación FALLIDA - Clave no coincide con ningún usuario");
        Serial.println("🔵 [BLE] ✗ NO SE PERMITE AUTO-REGISTRO - Solo superadmin puede añadir usuarios");
        
        // Publicar evento de intento fallido
        if (mqttClient.connected()) {
          String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
          String eventMsg = String("{") +
            "\"event\":\"AUTH_FAILED\"," +
            "\"device\":\"" + fixedSerialNumber + "\"," +
            "\"reason\":\"invalid_key\"," +
            "\"superadmin_exists\":" + (bleAuthConfig.superadmin_registered ? "true" : "false") + "," +
            "\"timestamp\":\"" + getTimestamp() + "\"" +
          "}";
          mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
        }
        
        String err = "ERROR:INVALID_KEY";
        pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      }
      pCharacteristic->notify();
      return;
    }
    
    // =================== COMANDOS DE ADMINISTRACIÓN ===================
    if (value.length() > BLE_KEY_SIZE && bleAuthenticated && bleCurrentPermissions == BLE_PERM_ADMIN) {
      // Comandos de administración (solo SUPERADMIN autenticado)
      String cmd = String(value.c_str());
      Serial.printf("🔵 [BLE] Comando admin recibido: %s\n", cmd.substring(0, 20).c_str());
      
      if (cmd.startsWith("ADD_USER:")) {
        // Formato: ADD_USER:<slot>:<key 64 bytes hex 128 chars>:<permissions>:<name>
        // Ejemplo: ADD_USER:1:A1B2C3...128chars...:3:Juan
        int firstColon = 9;
        int secondColon = cmd.indexOf(':', firstColon);
        int thirdColon = cmd.indexOf(':', secondColon + 1);
        int fourthColon = cmd.indexOf(':', thirdColon + 1);
        
        if (secondColon > firstColon) {
          int slot = cmd.substring(firstColon, secondColon).toInt();
          String keyHex = cmd.substring(secondColon + 1, thirdColon > 0 ? thirdColon : cmd.length());
          uint8_t permissions = BLE_PERM_RELAY_CONTROL | BLE_PERM_MODE_CHANGE;
          String userName = "Usuario" + String(slot);
          
          if (thirdColon > 0) {
            permissions = cmd.substring(thirdColon + 1, fourthColon > 0 ? fourthColon : cmd.length()).toInt();
          }
          if (fourthColon > 0) {
            userName = cmd.substring(fourthColon + 1);
            userName.trim();
          }
          
          Serial.printf("🔵 [BLE] ADD_USER: slot=%d, keyLen=%d, perm=%d, name=%s\n", 
                        slot, keyHex.length(), permissions, userName.c_str());
          
          if (slot >= 1 && slot <= BLE_MAX_USERS && keyHex.length() == BLE_KEY_SIZE * 2) {
            int idx = slot - 1;
            // Convertir hex a bytes
            for (int i = 0; i < BLE_KEY_SIZE; i++) {
              String byteStr = keyHex.substring(i * 2, i * 2 + 2);
              bleAuthConfig.user_keys[idx][i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
            }
            bleAuthConfig.user_enabled[idx] = 1;
            bleAuthConfig.user_permissions[idx] = permissions;
            strncpy(bleAuthConfig.user_names[idx], userName.c_str(), 15);
            bleAuthConfig.user_names[idx][15] = '\0';
            saveBLEAuthConfig();
            
            String ok = "OK:USER_ADDED:" + String(slot) + ":" + userName;
            pCharacteristic->setValue((uint8_t*)ok.c_str(), ok.length());
            Serial.printf("🔵 [BLE] ✓ Usuario %d (%s) añadido con permisos 0x%02X\n", slot, userName.c_str(), permissions);
          } else {
            String err = "ERROR:INVALID_PARAMS";
            pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
            Serial.println("🔵 [BLE] ✗ Parámetros inválidos para ADD_USER");
          }
        }
        
      } else if (cmd.startsWith("DEL_USER:")) {
        // Formato: DEL_USER:<slot>
        int slot = cmd.substring(9).toInt();
        if (slot >= 1 && slot <= BLE_MAX_USERS) {
          int idx = slot - 1;
          String userName = String(bleAuthConfig.user_names[idx]);
          memset(bleAuthConfig.user_keys[idx], 0, BLE_KEY_SIZE);
          bleAuthConfig.user_enabled[idx] = 0;
          bleAuthConfig.user_permissions[idx] = 0;
          memset(bleAuthConfig.user_names[idx], 0, 16);
          saveBLEAuthConfig();
          
          String ok = "OK:USER_DELETED:" + String(slot);
          pCharacteristic->setValue((uint8_t*)ok.c_str(), ok.length());
          Serial.printf("🔵 [BLE] ✓ Usuario %d (%s) eliminado\n", slot, userName.c_str());
        } else {
          String err = "ERROR:INVALID_SLOT";
          pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
        }
        
      } else if (cmd.startsWith("LIST_USERS")) {
        // Listar usuarios habilitados
        Serial.println("🔵 [BLE] Listando usuarios...");
        String response = "USERS:";
        for (int i = 0; i < BLE_MAX_USERS; i++) {
          if (bleAuthConfig.user_enabled[i]) {
            response += String(i + 1) + ":" + String(bleAuthConfig.user_names[i]) + ",";
          }
        }
        pCharacteristic->setValue((uint8_t*)response.c_str(), response.length());
        
      } else {
        String err = "ERROR:UNKNOWN_COMMAND";
        pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
        Serial.printf("🔵 [BLE] ✗ Comando desconocido: %s\n", cmd.c_str());
      }
      pCharacteristic->notify();
    } else {
      pCharacteristic->setValue("ERROR:INVALID_LENGTH");
      pCharacteristic->notify();
    }
  }
};

// Callback para control de relés
// Formato del comando BLE:
//   Byte 0: relay_id (1 o 2)
//   Byte 1: action (0=OFF, 1=ON con duración configurada, 2=PULSE con duración opcional)
//   Bytes 2-3 (opcional): duración en milisegundos (big-endian)
//
// Si NO se envía duración (solo 2 bytes), se usa la duración configurada del dispositivo (releDuration)
// Esto es similar al comportamiento de la web y MQTT
class RelayCharCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    if (!bleAuthenticated || !(bleCurrentPermissions & BLE_PERM_RELAY_CONTROL)) {
      pCharacteristic->setValue("ERROR:NOT_AUTHORIZED");
      pCharacteristic->notify();
      return;
    }

    std::string value = pCharacteristic->getValue();
    if (value.length() >= 2) {
      uint8_t relayId = value[0];
      uint8_t action = value[1];
      
      // Duración en milisegundos (opcional, big-endian)
      uint16_t durationMs = 0;
      if (value.length() >= 4) {
        durationMs = (value[2] << 8) | value[3];
      }

      if (relayId >= 1 && relayId <= 2) {
        int pin = (relayId == 1) ? RELE1_PIN : RELE2_PIN;

        if (action == 1 || action == 2) { // ON o PULSE (mismo comportamiento)
          // Si se envía duración, usarla; si no, usar la duración configurada
          float durSec = (durationMs > 0) ? (durationMs / 1000.0) : releDuration;
          
          // Usar la función estándar que también publica eventos MQTT
          controlReleWithDuration(durSec, relayId);
          
          Serial.printf("🔵 [BLE] Relé %d ACTIVADO por %s (%.1fs)\n", 
                        relayId, bleConnectedUser.c_str(), durSec);
          
          // Publicar evento BLE de acción
          if (mqttClient.connected()) {
            String topic = "swatidhome/events/" + fixedSerialNumber + "/ble";
            String msg = String("{") +
              "\"event\":\"RELAY_ACTIVATED\"," +
              "\"relay\":" + String(relayId) + "," +
              "\"duration\":" + String(durSec, 1) + "," +
              "\"source\":\"BLE\"," +
              "\"user\":\"" + bleConnectedUser + "\"," +
              "\"timestamp\":\"" + getTimestamp() + "\"" +
            "}";
            mqttClient.publish(topic.c_str(), msg.c_str());
          }
          
          pCharacteristic->setValue(action == 1 ? "OK:RELAY_ON" : "OK:RELAY_PULSE");
          
        } else if (action == 0) { // Desactivar inmediatamente
          digitalWrite(pin, LOW);
          releActive[relayId] = false;

          Serial.printf("🔵 [BLE] Relé %d DESACTIVADO por %s\n", relayId, bleConnectedUser.c_str());
          
          // Publicar evento MQTT
          if (mqttClient.connected()) {
            String topic = "swatidhome/events/" + fixedSerialNumber + "/ble";
            String msg = String("{") +
              "\"event\":\"RELAY_DEACTIVATED\"," +
              "\"relay\":" + String(relayId) + "," +
              "\"source\":\"BLE\"," +
              "\"user\":\"" + bleConnectedUser + "\"," +
              "\"timestamp\":\"" + getTimestamp() + "\"" +
            "}";
            mqttClient.publish(topic.c_str(), msg.c_str());
          }
          
          pCharacteristic->setValue("OK:RELAY_OFF");
        }
      } else {
        pCharacteristic->setValue("ERROR:INVALID_RELAY");
      }
      pCharacteristic->notify();
    }
  }
};

// Callback para cambio de modo
class ModeCharCallbacks: public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic) {
    uint8_t mode = config.turnstile.enabled ? 1 : 0;
    pCharacteristic->setValue(&mode, 1);
  }
  
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    if (!bleAuthenticated || !(bleCurrentPermissions & BLE_PERM_MODE_CHANGE)) {
      pCharacteristic->setValue("ERROR:NOT_AUTHORIZED");
      pCharacteristic->notify();
      return;
    }
    
    std::string value = pCharacteristic->getValue();
    if (value.length() >= 1) {
      uint8_t newMode = value[0];
      config.turnstile.enabled = (newMode == 1);
      saveConfiguration();
      
      Serial.printf("🔵 [BLE] Modo cambiado a %s por %s\n", 
                    config.turnstile.enabled ? "TORNO" : "NORMAL", 
                    bleConnectedUser.c_str());
      pCharacteristic->setValue(config.turnstile.enabled ? "OK:MODE_TURNSTILE" : "OK:MODE_NORMAL");
      pCharacteristic->notify();
    }
  }
};

// Estructura para código pendiente de guardar (procesado en loop())
struct PendingCode {
  bool pending;
  char type[4];
  char value[17];
  uint8_t keyboard;
  uint8_t relay;
} pendingCode = {false, "", "", 0, 0};

// Reinicio pendiente programado (para aplicar cambios de red)
unsigned long pendingRestartTime = 0;  // 0 = no hay reinicio pendiente

// Callback para añadir códigos (FF04)
// OPTIMIZADO: Responde rápido y difiere el guardado EEPROM al loop()
class AddCodeCharCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔵 [BLE] FF04 - Recibido");
    
    // Obtener valor rápidamente
    std::string value = pCharacteristic->getValue();
    
    // Verificaciones rápidas
    if (!bleAuthenticated) {
      pCharacteristic->setValue("ERROR:NOT_AUTHENTICATED");
      pCharacteristic->notify();
      return;
    }
    
    if (!(bleCurrentPermissions & BLE_PERM_ADD_CODES)) {
      pCharacteristic->setValue("ERROR:NO_PERMISSION");
      pCharacteristic->notify();
      return;
    }
    
    if (storedCodes == nullptr) {
      pCharacteristic->setValue("ERROR:STORAGE_NOT_READY");
      pCharacteristic->notify();
      return;
    }
    
    if (value.length() < 4) {
      pCharacteristic->setValue("ERROR:INVALID_LENGTH");
      pCharacteristic->notify();
      return;
    }
    
    uint8_t typeByte = value[0];
    uint8_t keyboard = value[1];
    uint8_t relay = value[2];
    
    if (typeByte > 1) {
      pCharacteristic->setValue("ERROR:INVALID_TYPE");
      pCharacteristic->notify();
      return;
    }
    if (keyboard > 2) {
      pCharacteristic->setValue("ERROR:INVALID_KEYBOARD");
      pCharacteristic->notify();
      return;
    }
    if (relay < 1 || relay > 2) {
      pCharacteristic->setValue("ERROR:INVALID_RELAY");
      pCharacteristic->notify();
      return;
    }
    
    size_t codeLen = value.length() - 3;
    if (codeLen == 0 || codeLen > 16) {
      pCharacteristic->setValue("ERROR:INVALID_CODE_LENGTH");
      pCharacteristic->notify();
      return;
    }
    
    // Copiar datos a buffer local
    char codeBuffer[17] = {0};
    memcpy(codeBuffer, value.data() + 3, codeLen);
    
    const char* typeStr = (typeByte == 0) ? "PIN" : "TAG";
    
    Serial.printf("🔵 [BLE] FF04 - %s '%s' kb=%d relay=%d\n", typeStr, codeBuffer, keyboard, relay);
    
    // Verificar duplicados (búsqueda rápida en RAM)
    for (int i = 0; i < storedCodes->count; i++) {
      if (strcmp(storedCodes->codes[i].type, typeStr) == 0 &&
          strcmp(storedCodes->codes[i].value, codeBuffer) == 0) {
        pCharacteristic->setValue("ERROR:CODE_EXISTS");
        pCharacteristic->notify();
        return;
      }
    }
    
    // Verificar espacio
    if (storedCodes->count >= MAX_CODES) {
      pCharacteristic->setValue("ERROR:STORAGE_FULL");
      pCharacteristic->notify();
      return;
    }
    
    // AÑADIR SOLO A RAM (MUY RÁPIDO)
    int idx = storedCodes->count;
    strncpy(storedCodes->codes[idx].type, typeStr, 3);
    storedCodes->codes[idx].type[3] = '\0';
    strncpy(storedCodes->codes[idx].value, codeBuffer, 16);
    storedCodes->codes[idx].value[16] = '\0';
    storedCodes->codes[idx].keyboard_id = keyboard;
    storedCodes->codes[idx].relay = relay;
    storedCodes->codes[idx].reserved = 0;
    storedCodes->count++;
    storedCodes->version = 2;
    
    // RESPONDER INMEDIATAMENTE (antes de guardar en EEPROM)
    char response[24];
    snprintf(response, sizeof(response), "OK:CODE_ADDED:%d", storedCodes->count);
    pCharacteristic->setValue(response);
    pCharacteristic->notify();
    
    Serial.printf("🔵 [BLE] FF04 ✅ Añadido a RAM, total=%d\n", storedCodes->count);
    
    // MARCAR PARA GUARDAR EN LOOP (evita bloquear el callback BLE)
    strncpy(pendingCode.type, typeStr, 3);
    strncpy(pendingCode.value, codeBuffer, 16);
    pendingCode.keyboard = keyboard;
    pendingCode.relay = relay;
    pendingCode.pending = true;
  }
};

// Callback para configuración de red (FF05)
// JSON optimizado para caber en MTU (~240 bytes)
class NetworkCharCallbacks: public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔵 [BLE] FF05 - Solicitud de configuración de red");
    
    // JSON compacto: solo campos esenciales
    DynamicJsonDocument doc(256);
    
    doc["dhcp"] = config.useDhcp;
    doc["eth"] = ethConnected;
    doc["ip"] = ETH.localIP().toString();
    doc["gw"] = ETH.gatewayIP().toString();
    doc["mask"] = ETH.subnetMask().toString();
    doc["mac"] = ETH.macAddress();
    
    // Solo incluir config estática si NO es DHCP
    if (!config.useDhcp) {
      char cfgIP[16], cfgGW[16], cfgMask[16];
      sprintf(cfgIP, "%d.%d.%d.%d", config.ip[0], config.ip[1], config.ip[2], config.ip[3]);
      sprintf(cfgGW, "%d.%d.%d.%d", config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]);
      sprintf(cfgMask, "%d.%d.%d.%d", config.subnet[0], config.subnet[1], config.subnet[2], config.subnet[3]);
      doc["cfg_ip"] = cfgIP;
      doc["cfg_gw"] = cfgGW;
      doc["cfg_mask"] = cfgMask;
    }
    
    String output;
    serializeJson(doc, output);
    
    Serial.printf("🔵 [BLE] FF05 JSON: %d bytes\n", output.length());
    
    // Añadir EOT y enviar
    if (output.length() <= 230) {
      output += '\x04';  // EOT marker
      pCharacteristic->setValue((uint8_t*)output.c_str(), output.length());
      pCharacteristic->notify();
      Serial.printf("🔵 [BLE] FF05 enviado con EOT (%d bytes)\n", output.length());
    } else {
      // Si es muy grande, usar chunking con EOT
      sendBLEWithEOT(pCharacteristic, output);
    }
  }
  
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔵 [BLE] FF05 - onWrite llamado");
    Serial.printf("🔵 [BLE] FF05 - Auth: %d, Permisos: 0x%02X, NETWORK_CONFIG: 0x%02X\n", 
                  bleAuthenticated, bleCurrentPermissions, BLE_PERM_NETWORK_CONFIG);
    
    if (!bleAuthenticated) {
      Serial.println("🔵 [BLE] FF05 - ERROR: No autenticado");
      String err = "ERROR:NOT_AUTHENTICATED";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
      return;
    }
    
    if (!(bleCurrentPermissions & BLE_PERM_NETWORK_CONFIG)) {
      Serial.println("🔵 [BLE] FF05 - ERROR: Sin permiso NETWORK_CONFIG");
      String err = "ERROR:NO_PERMISSION";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
      return;
    }
    
    std::string value = pCharacteristic->getValue();
    Serial.printf("🔵 [BLE] FF05 - Recibidos %d bytes\n", value.length());
    
    if (value.length() >= 17) {
      // Parsear datos binarios
      bool newDhcp = (value[0] != 0);  // 0 = IP fija, != 0 = DHCP
      
      Serial.printf("🔵 [BLE] FF05 - Byte DHCP: 0x%02X -> %s\n", 
                    (uint8_t)value[0], newDhcp ? "DHCP" : "IP Fija");
      
      // IMPORTANTE: Actualizar las VARIABLES GLOBALES que usa saveConfiguration()
      useDhcp = newDhcp;
      staticIP = IPAddress(value[1], value[2], value[3], value[4]);
      staticGateway = IPAddress(value[5], value[6], value[7], value[8]);
      staticSubnet = IPAddress(value[9], value[10], value[11], value[12]);
      staticDns = IPAddress(value[13], value[14], value[15], value[16]);
      
      // También actualizar config.* para consistencia
      config.useDhcp = newDhcp;
      memcpy(config.ip, value.data() + 1, 4);
      memcpy(config.gateway, value.data() + 5, 4);
      memcpy(config.subnet, value.data() + 9, 4);
      memcpy(config.dns, value.data() + 13, 4);
      
      Serial.printf("🔵 [BLE] FF05 - Nueva config: DHCP=%s, IP=%s\n",
                    useDhcp ? "true" : "false", staticIP.toString().c_str());
      Serial.printf("🔵 [BLE] FF05 - GW=%s, Mask=%s, DNS=%s\n",
                    staticGateway.toString().c_str(), 
                    staticSubnet.toString().c_str(),
                    staticDns.toString().c_str());
      
      saveConfiguration();
      Serial.println("🔵 [BLE] FF05 - Configuración guardada en EEPROM");
      
      Serial.printf("🔵 [BLE] Red configurada: DHCP=%s por %s\n", 
                    config.useDhcp ? "Sí" : "No", 
                    bleConnectedUser.c_str());
      
      // Responder OK antes del reinicio
      String ok = "OK:NETWORK_CONFIGURED:RESTARTING";
      pCharacteristic->setValue((uint8_t*)ok.c_str(), ok.length());
      pCharacteristic->notify();
      
      // Programar reinicio en 2 segundos (para que la respuesta llegue a la App)
      pendingRestartTime = millis() + 2000;
      Serial.println("🔵 [BLE] FF05 - Reinicio programado en 2 segundos para aplicar configuración de red");
    } else {
      Serial.printf("🔵 [BLE] FF05 - ERROR: Datos insuficientes (%d bytes, necesita 17)\n", value.length());
      String err = "ERROR:INVALID_DATA";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
    }
  }
};

// Callback para información del dispositivo (FF08) - Público, sin autenticación
class DevInfoCharCallbacks: public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔵 [BLE] FF08 - Solicitud de info del dispositivo");
    
    // Contar usuarios activos
    int activeUsers = 0;
    for (int i = 0; i < BLE_MAX_USERS; i++) {
      if (bleAuthConfig.user_enabled[i]) activeUsers++;
    }
    
    // Crear JSON usando ArduinoJson para evitar problemas de formato
    DynamicJsonDocument doc(512);
    doc["device_type"] = DEVICE_TYPE;
    doc["serial"] = fixedSerialNumber;
    doc["firmware_version"] = firmwareVersion;
    doc["firmware_variant"] = "BLE";
    doc["protocol_version"] = PROTOCOL_VERSION;
    
    JsonObject caps = doc.createNestedObject("capabilities");
    caps["relays"] = 2;
    caps["wiegand_inputs"] = 2;
    caps["digital_inputs"] = 2;
    caps["ble_users"] = BLE_MAX_USERS;
    
    doc["superadmin_registered"] = (bool)bleAuthConfig.superadmin_registered;
    doc["active_users"] = activeUsers;
    
    String output;
    serializeJson(doc, output);
    
    Serial.printf("🔵 [BLE] FF08 - Enviando %d bytes: %s\n", output.length(), output.substring(0, 50).c_str());
    pCharacteristic->setValue((uint8_t*)output.c_str(), output.length());
    pCharacteristic->notify();
  }
};

// Callback para estado del dispositivo (FF07)
class StatusCharCallbacks: public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔵 [BLE] FF07 - Solicitud de estado");
    
    // Crear JSON de estado
    DynamicJsonDocument doc(256);
    doc["relay1"] = (bool)digitalRead(RELE1_PIN);
    doc["relay2"] = (bool)digitalRead(RELE2_PIN);
    doc["mode"] = config.turnstile.enabled ? "torno" : "normal";
    doc["auth"] = bleAuthenticated;
    doc["user"] = bleConnectedUser;
    doc["eth_connected"] = ethConnected;
    doc["mqtt_connected"] = mqttClient.connected();
    
    String output;
    serializeJson(doc, output);
    
    pCharacteristic->setValue((uint8_t*)output.c_str(), output.length());
    pCharacteristic->notify();
    Serial.printf("🔵 [BLE] FF07 actualizado (%d bytes): %s\n", output.length(), output.c_str());
  }
};

// Callback para tiempo de relé
class RelayTimeCharCallbacks: public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic) {
    uint32_t durationMs = (uint32_t)(config.releDuration * 1000);
    pCharacteristic->setValue((uint8_t*)&durationMs, 4);
  }
  
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    if (!bleAuthenticated || !(bleCurrentPermissions & BLE_PERM_RELAY_CONTROL)) {
      pCharacteristic->setValue("ERROR:NOT_AUTHORIZED");
      pCharacteristic->notify();
      return;
    }
    
    std::string value = pCharacteristic->getValue();
    if (value.length() >= 4) {
      uint32_t durationMs = *((uint32_t*)value.data());
      config.releDuration = durationMs / 1000.0;
      releDuration = config.releDuration;
      saveConfiguration();
      
      Serial.printf("🔵 [BLE] Tiempo de relé: %.1fs por %s\n", 
                    releDuration, bleConnectedUser.c_str());
      pCharacteristic->setValue("OK:RELAY_TIME_SET");
      pCharacteristic->notify();
    }
  }
};

// =================== FUNCIONES HELPER PARA ACTUALIZAR FF09 y FF0A ===================
// Estas funciones actualizan los valores de las características BLE
// Se llaman después de la autenticación y cuando cambia el estado

// Función para enviar datos grandes por BLE usando chunking
// Protocolo: cada chunk tiene 1 byte de control + datos
// Byte de control: 0x01 = más chunks, 0x00 = último chunk
#define BLE_CHUNK_SIZE 236  // 240 - 4 bytes de overhead para control

void sendBLEChunked(NimBLECharacteristic* pChar, const String& data) {
  int totalLen = data.length();
  int offset = 0;
  int chunkNum = 0;
  int totalChunks = (totalLen + BLE_CHUNK_SIZE - 1) / BLE_CHUNK_SIZE;
  
  Serial.printf("🔵 [BLE] Enviando %d bytes en %d chunks\n", totalLen, totalChunks);
  
  while (offset < totalLen) {
    int remaining = totalLen - offset;
    int chunkLen = (remaining > BLE_CHUNK_SIZE) ? BLE_CHUNK_SIZE : remaining;
    bool isLast = (offset + chunkLen >= totalLen);
    
    // Crear buffer con byte de control
    // Formato: [flags][chunk_num][total_chunks][data...]
    // flags: 0x00 = último, 0x01 = más chunks
    uint8_t buffer[BLE_CHUNK_SIZE + 4];
    buffer[0] = isLast ? 0x00 : 0x01;  // Flag de continuación
    buffer[1] = (uint8_t)chunkNum;      // Número de chunk (0-255)
    buffer[2] = (uint8_t)totalChunks;   // Total de chunks
    buffer[3] = 0x00;                   // Reservado
    
    // Copiar datos
    memcpy(buffer + 4, data.c_str() + offset, chunkLen);
    
    pChar->setValue(buffer, chunkLen + 4);
    pChar->notify();
    
    Serial.printf("🔵 [BLE] Chunk %d/%d: %d bytes, last=%d\n", 
                  chunkNum + 1, totalChunks, chunkLen, isLast);
    
    offset += chunkLen;
    chunkNum++;
    
    // Pausa entre chunks para que el cliente procese
    if (!isLast) {
      delay(30);
    }
  }
  
  Serial.printf("🔵 [BLE] Envío chunked completado\n");
}

// Función alternativa: enviar datos JSON sin chunking binario
// Usa terminador \x04 (EOT) para indicar fin de transmisión
void sendBLEWithEOT(NimBLECharacteristic* pChar, const String& data) {
  int totalLen = data.length();
  int offset = 0;
  int chunkSize = 240;
  
  Serial.printf("🔵 [BLE] Enviando %d bytes con EOT\n", totalLen);
  
  while (offset < totalLen) {
    int remaining = totalLen - offset;
    int chunkLen = (remaining > chunkSize) ? chunkSize : remaining;
    bool isLast = (offset + chunkLen >= totalLen);
    
    String chunk = data.substring(offset, offset + chunkLen);
    
    // Añadir EOT al último chunk
    if (isLast) {
      chunk += '\x04';  // EOT (End Of Transmission)
    }
    
    pChar->setValue((uint8_t*)chunk.c_str(), chunk.length());
    pChar->notify();
    
    Serial.printf("🔵 [BLE] Chunk: offset=%d, len=%d, last=%d\n", offset, chunkLen, isLast);
    
    offset += chunkLen;
    
    if (!isLast) {
      delay(25);
    }
  }
}

void updateFF09Value() {
  if (pFullInfoChar == nullptr) {
    Serial.println("🔵 [BLE] FF09 - ERROR: pFullInfoChar es NULL");
    return;
  }
  
  if (!bleAuthenticated) {
    String notAuth = "{\"error\":\"NOT_AUTHORIZED\"}";
    pFullInfoChar->setValue((uint8_t*)notAuth.c_str(), notAuth.length());
    Serial.println("🔵 [BLE] FF09 - No autenticado");
    return;
  }
  
  // JSON optimizado con nombres cortos para caber en MTU
  DynamicJsonDocument doc(512);
  
  doc["type"] = DEVICE_TYPE;
  doc["sn"] = fixedSerialNumber;
  doc["name"] = deviceName;
  doc["fw"] = firmwareVersion;
  
  // Red (nombres cortos)
  JsonObject net = doc.createNestedObject("net");
  net["ip"] = ETH.localIP().toString();
  net["gw"] = ETH.gatewayIP().toString();
  net["mask"] = ETH.subnetMask().toString();
  net["dhcp"] = config.useDhcp;
  net["eth"] = ethConnected;
  
  // Estado (nombres cortos)
  JsonObject st = doc.createNestedObject("st");
  st["r1"] = (bool)digitalRead(RELE1_PIN);
  st["r2"] = (bool)digitalRead(RELE2_PIN);
  st["dur"] = releDuration;
  st["mode"] = config.turnstile.enabled ? 1 : 0;
  st["mqtt"] = mqttClient.connected();
  
  // Códigos (nombres cortos)
  JsonObject cd = doc.createNestedObject("cd");
  cd["loc"] = (storedCodes != nullptr) ? storedCodes->count : 0;
  cd["max"] = MAX_CODES;
  cd["rem"] = (storedRemoteCodes != nullptr) ? storedRemoteCodes->count : 0;
  
  doc["up"] = millis() / 1000;
  
  String output;
  serializeJson(doc, output);
  
  Serial.printf("🔵 [BLE] FF09 JSON: %d bytes\n", output.length());
  
  if (!doc.overflowed()) {
    // Añadir EOT y enviar
    if (output.length() <= 230) {
      output += '\x04';  // EOT marker
      pFullInfoChar->setValue((uint8_t*)output.c_str(), output.length());
      pFullInfoChar->notify();
      Serial.printf("🔵 [BLE] FF09 enviado con EOT (%d bytes)\n", output.length());
    } else {
      // Si es muy grande, usar chunking con EOT
      sendBLEWithEOT(pFullInfoChar, output);
    }
  } else {
    String overflow = "{\"error\":\"JSON_OVERFLOW\"}";
    pFullInfoChar->setValue((uint8_t*)overflow.c_str(), overflow.length());
    Serial.println("🔵 [BLE] FF09 - ERROR: JSON overflow");
  }
}

void updateFF0AValue(int page) {
  if (pCodesChar == nullptr) {
    Serial.println("🔵 [BLE] FF0A - ERROR: pCodesChar es NULL");
    return;
  }
  
  if (!bleAuthenticated) {
    String notAuth = "{\"error\":\"NOT_AUTHORIZED\"}";
    pCodesChar->setValue((uint8_t*)notAuth.c_str(), notAuth.length());
    Serial.println("🔵 [BLE] FF0A - No autenticado");
    return;
  }
  
  // Página muy pequeña (3 códigos) para garantizar que quepa en MTU
  DynamicJsonDocument doc(512);
  int totalCodes = (storedCodes != nullptr) ? storedCodes->count : 0;
  int pageSize = 3;  // Solo 3 códigos por página (~180 bytes max)
  int startIdx = page * pageSize;
  int endIdx = min(startIdx + pageSize, totalCodes);
  int totalPages = (totalCodes > 0) ? ((totalCodes + pageSize - 1) / pageSize) : 1;
  
  // JSON compacto con nombres cortos
  doc["n"] = totalCodes;        // count total
  doc["m"] = MAX_CODES;         // max
  doc["p"] = page;              // page actual
  doc["ps"] = pageSize;         // page size
  doc["tp"] = totalPages;       // total pages
  doc["more"] = (endIdx < totalCodes);
  
  JsonArray arr = doc.createNestedArray("c");  // codes array
  if (storedCodes != nullptr) {
    for (int i = startIdx; i < endIdx; i++) {
      JsonObject c = arr.createNestedObject();
      c["i"] = i;
      c["t"] = storedCodes->codes[i].type;
      c["v"] = storedCodes->codes[i].value;
      c["k"] = storedCodes->codes[i].keyboard_id;
      c["r"] = storedCodes->codes[i].relay;
    }
  }
  
  String output;
  serializeJson(doc, output);
  
  Serial.printf("🔵 [BLE] FF0A JSON: %d bytes (página %d/%d)\n", output.length(), page, totalPages);
  
  if (!doc.overflowed()) {
    // Usar EOT para indicar fin de transmisión
    if (output.length() <= 230) {
      // Cabe en un solo NOTIFY - añadir EOT
      output += '\x04';  // EOT marker
      pCodesChar->setValue((uint8_t*)output.c_str(), output.length());
      pCodesChar->notify();
      Serial.printf("🔵 [BLE] FF0A enviado con EOT (%d bytes)\n", output.length());
    } else {
      // Necesita chunking con EOT
      sendBLEWithEOT(pCodesChar, output);
    }
  } else {
    String overflow = "{\"error\":\"JSON_OVERFLOW\"}";
    pCodesChar->setValue((uint8_t*)overflow.c_str(), overflow.length());
    Serial.println("🔵 [BLE] FF0A - ERROR: JSON overflow");
  }
}

// Callback para información completa del dispositivo (FF09)
class FullInfoCharCallbacks: public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔵 [BLE] FF09 - Solicitud de configuración completa");
    updateFF09Value();  // Actualizar antes de que se lea
  }
};

// Callback para lista de códigos locales (FF0A)
class CodesCharCallbacks: public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔵 [BLE] FF0A - Solicitud de lista de códigos");
    updateFF0AValue(0);  // Actualizar antes de que se lea
  }
  
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    Serial.println("🔵 [BLE] FF0A - Solicitud de página específica");
    
    if (!bleAuthenticated) {
      String err = "{\"error\":\"NOT_AUTHORIZED\"}";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
      return;
    }
    
    std::string value = pCharacteristic->getValue();
    if (value.length() == 0) {
      String err = "{\"error\":\"EMPTY_REQUEST\"}";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
      return;
    }
    
    DynamicJsonDocument reqDoc(128);
    DeserializationError error = deserializeJson(reqDoc, value.c_str());
    
    if (error) {
      String err = "{\"error\":\"INVALID_JSON\"}";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
      return;
    }
    
    if (!reqDoc.containsKey("page")) {
      String err = "{\"error\":\"MISSING_PAGE\"}";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
      return;
    }
    
    int page = reqDoc["page"].as<int>();
    int totalCodes = (storedCodes != nullptr) ? storedCodes->count : 0;
    int pageSize = 3;  // Reducido para caber en MTU
    int totalPages = (totalCodes > 0) ? ((totalCodes + pageSize - 1) / pageSize) : 1;
    int startIdx = page * pageSize;
    int endIdx = min(startIdx + pageSize, totalCodes);
    
    // Validar página (permitir página 0 siempre)
    if (page < 0 || (page > 0 && startIdx >= totalCodes)) {
      String err = "{\"error\":\"INVALID_PAGE\"}";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
      Serial.printf("🔵 [BLE] FF0A - Error: Página %d inválida (total: %d)\n", page, totalPages);
      return;
    }
    
    // JSON compacto con nombres cortos
    DynamicJsonDocument doc(512);
    doc["n"] = totalCodes;        // count
    doc["m"] = MAX_CODES;         // max
    doc["p"] = page;              // page
    doc["ps"] = pageSize;         // page size
    doc["tp"] = totalPages;       // total pages
    doc["more"] = (endIdx < totalCodes);
    
    JsonArray arr = doc.createNestedArray("c");  // codes
    if (storedCodes != nullptr) {
      for (int i = startIdx; i < endIdx; i++) {
        JsonObject c = arr.createNestedObject();
        c["i"] = i;
        c["t"] = storedCodes->codes[i].type;
        c["v"] = storedCodes->codes[i].value;
        c["k"] = storedCodes->codes[i].keyboard_id;
        c["r"] = storedCodes->codes[i].relay;
      }
    }
    
    String output;
    serializeJson(doc, output);
    
    if (doc.overflowed()) {
      String err = "{\"error\":\"JSON_OVERFLOW\"}";
      pCharacteristic->setValue((uint8_t*)err.c_str(), err.length());
      pCharacteristic->notify();
      return;
    }
    
    // Añadir EOT y enviar
    output += '\x04';  // EOT marker
    pCharacteristic->setValue((uint8_t*)output.c_str(), output.length());
    pCharacteristic->notify();
    
    Serial.printf("🔵 [BLE] FF0A página %d/%d enviada con EOT (%d bytes)\n", page, totalPages, output.length());
  }
};

// Inicializar servidor BLE
void initBLE() {
  Serial.println("\n🔵 =================== INICIALIZANDO BLE ===================");

  // v4.1: Cargar clave maestra del dispositivo para HKDF
  loadDeviceKeyConfig();
  
  // Cargar configuración de autenticación
  loadBLEAuthConfig();
  
  // Inicializar NimBLE con el número de serie completo para fácil identificación
  // El serial ya tiene formato SWATID_XXXXXXXXXXXX (19 chars), cabe en límite BLE de 29 chars
  String bleName = fixedSerialNumber;
  NimBLEDevice::init(bleName.c_str());
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Máxima potencia
  
  Serial.printf("🔵 [BLE] Nombre del dispositivo: %s\n", bleName.c_str());
  
  // Crear servidor
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new SWATIDServerCallbacks());
  
  // Crear servicio
  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  
  // Característica de autenticación
  pAuthChar = pService->createCharacteristic(
    CHAR_AUTH_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
  );
  pAuthChar->setCallbacks(new AuthCharCallbacks());
  
  // Característica de control de relés
  // WRITE_NR añadido para soportar writeWithoutResponse de la App
  pRelayChar = pService->createCharacteristic(
    CHAR_RELAY_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
  );
  pRelayChar->setCallbacks(new RelayCharCallbacks());

  // Característica de modo
  // WRITE_NR añadido para soportar writeWithoutResponse de la App
  pModeChar = pService->createCharacteristic(
    CHAR_MODE_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
  );
  pModeChar->setCallbacks(new ModeCharCallbacks());

  // Característica para añadir códigos
  // WRITE_NR añadido para soportar writeWithoutResponse de la App
  pAddCodeChar = pService->createCharacteristic(
    CHAR_ADDCODE_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
  );
  pAddCodeChar->setCallbacks(new AddCodeCharCallbacks());

  // Característica de configuración de red
  // WRITE_NR añadido para soportar writeWithoutResponse de la App
  pNetworkChar = pService->createCharacteristic(
    CHAR_NETWORK_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
  );
  pNetworkChar->setCallbacks(new NetworkCharCallbacks());

  // Característica de tiempo de relé
  // WRITE_NR añadido para soportar writeWithoutResponse de la App
  pRelayTimeChar = pService->createCharacteristic(
    CHAR_RELAYTIME_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
  );
  pRelayTimeChar->setCallbacks(new RelayTimeCharCallbacks());
  
  // Característica de estado (FF07) - Solo lectura con notify
  pStatusChar = pService->createCharacteristic(
    CHAR_STATUS_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  pStatusChar->setCallbacks(new StatusCharCallbacks());
  // Valor inicial de FF07
  String ff07Init = "{\"relay1\":false,\"relay2\":false,\"mode\":\"normal\",\"auth\":false,\"user\":\"\",\"eth_connected\":false,\"mqtt_connected\":false}";
  pStatusChar->setValue((uint8_t*)ff07Init.c_str(), ff07Init.length());
  Serial.printf("🔵 [BLE] FF07 inicializado con %d bytes\n", ff07Init.length());
  
  // Característica de información del dispositivo (FF08) - Provisión automática
  // Esta característica permite a la APP identificar automáticamente el tipo
  // de dispositivo, versión de firmware y capacidades SIN necesidad de autenticarse
  pDevInfoChar = pService->createCharacteristic(
    CHAR_DEVINFO_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  pDevInfoChar->setCallbacks(new DevInfoCharCallbacks());

  // Establecer valor inicial de información del dispositivo
  int activeUsers = 0;
  for (int i = 0; i < BLE_MAX_USERS; i++) {
    if (bleAuthConfig.user_enabled[i]) activeUsers++;
  }

  String devInfo = String("{") +
    "\"device_type\":\"" + DEVICE_TYPE + "\"," +
    "\"serial\":\"" + fixedSerialNumber + "\"," +
    "\"firmware_version\":\"" + firmwareVersion + "\"," +
    "\"firmware_variant\":\"BLE\"," +
    "\"protocol_version\":" + PROTOCOL_VERSION + "," +
    "\"capabilities\":{" +
      "\"relays\":2," +
      "\"wiegand_inputs\":2," +
      "\"digital_inputs\":2," +
      "\"ble_users\":" + BLE_MAX_USERS +
    "}," +
    "\"superadmin_registered\":" + (bleAuthConfig.superadmin_registered ? "true" : "false") +
  "}";
  pDevInfoChar->setValue((uint8_t*)devInfo.c_str(), devInfo.length());
  Serial.printf("🔵 [BLE] FF08 inicializado con %d bytes\n", devInfo.length());

  // Característica de información completa (FF09) - Requiere autenticación
  // IMPORTANTE: NOTIFY es necesario para enviar datos actualizados
  pFullInfoChar = pService->createCharacteristic(
    CHAR_FULLINFO_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  pFullInfoChar->setCallbacks(new FullInfoCharCallbacks());
  String ff09Init = "{\"error\":\"NOT_AUTHORIZED\"}";
  pFullInfoChar->setValue((uint8_t*)ff09Init.c_str(), ff09Init.length());
  Serial.printf("🔵 [BLE] FF09 inicializado con %d bytes\n", ff09Init.length());

  // Característica de códigos locales (FF0A) - Requiere autenticación
  // WRITE_NR añadido para soportar writeWithoutResponse de la App
  pCodesChar = pService->createCharacteristic(
    CHAR_CODES_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
  );
  pCodesChar->setCallbacks(new CodesCharCallbacks());
  String ff0aInit = "{\"error\":\"NOT_AUTHORIZED\"}";
  pCodesChar->setValue((uint8_t*)ff0aInit.c_str(), ff0aInit.length());
  Serial.printf("🔵 [BLE] FF0A inicializado con %d bytes\n", ff0aInit.length());

  // =================== v4.1: Característica de Challenge (FF0B) ===================
  // Esta característica permite autenticación segura sin transmitir la clave
  // Flujo: 1) APP lee FF0B -> obtiene nonce 16 bytes
  //        2) APP calcula SHA256(key + nonce) 
  //        3) APP envía response (32 bytes) a FF01
  //        4) Dispositivo verifica y devuelve token de sesión
  pChallengeChar = pService->createCharacteristic(
    CHAR_CHALLENGE_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  pChallengeChar->setCallbacks(new ChallengeCharCallbacks());
  
  // Valor inicial vacío (se genera al leer)
  uint8_t emptyChallenge[16] = {0};
  pChallengeChar->setValue(emptyChallenge, 16);
  Serial.println("🔐 [BLE] FF0B (Challenge) inicializado - Auth seguro v4.1 habilitado");

  // Iniciar servicio
  pService->start();
  
  // Configurar advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  pAdvertising->start();
  
  Serial.printf("🔵 [BLE] Servidor iniciado: %s\n", bleName.c_str());
  Serial.println("🔵 [BLE] Esperando conexiones...");
  Serial.println("🔵 ==========================================================\n");
}

// Actualizar estado BLE (llamar desde loop)
void updateBLEStatus() {
  if (pStatusChar != nullptr) {
    // Crear JSON de estado usando ArduinoJson
    DynamicJsonDocument doc(256);
    doc["relay1"] = (bool)digitalRead(RELE1_PIN);
    doc["relay2"] = (bool)digitalRead(RELE2_PIN);
    doc["mode"] = config.turnstile.enabled ? "torno" : "normal";
    doc["auth"] = bleAuthenticated;
    doc["user"] = bleConnectedUser;
    doc["eth_connected"] = ethConnected;
    doc["mqtt_connected"] = mqttClient.connected();
    
    String output;
    serializeJson(doc, output);
    
    // Usar setValue con longitud explícita
    pStatusChar->setValue((uint8_t*)output.c_str(), output.length());
    // No llamamos notify() aquí porque se llama desde loop constantemente
    // Solo notificamos cuando hay cambios significativos
  }
}

// Actualizar información del dispositivo BLE (FF08) - Para provisión automática
void updateBLEDeviceInfo() {
  if (pDevInfoChar != nullptr) {
    int activeUsers = 0;
    for (int i = 0; i < BLE_MAX_USERS; i++) {
      if (bleAuthConfig.user_enabled[i]) activeUsers++;
    }
    
    DynamicJsonDocument doc(512);
    doc["device_type"] = DEVICE_TYPE;
    doc["serial"] = fixedSerialNumber;
    doc["firmware_version"] = firmwareVersion;
    doc["firmware_variant"] = "BLE";
    doc["protocol_version"] = PROTOCOL_VERSION;
    
    JsonObject caps = doc.createNestedObject("capabilities");
    caps["relays"] = 2;
    caps["wiegand_inputs"] = 2;
    caps["digital_inputs"] = 2;
    caps["ble_users"] = BLE_MAX_USERS;
    
    doc["superadmin_registered"] = (bool)bleAuthConfig.superadmin_registered;
    doc["active_users"] = activeUsers;
    
    String output;
    serializeJson(doc, output);
    
    pDevInfoChar->setValue((uint8_t*)output.c_str(), output.length());
    Serial.printf("🔵 [BLE] FF08 actualizado: %d bytes\n", output.length());
  }
}

#endif // ENABLE_BLE

// =================== FUNCIÓN SETUP ===================
void setup() {
   Serial.begin(115200);
   delay(1000);
   
   Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
   Serial.println("║                    KC868-A2 DUAL WIEGAND                    ║");
   Serial.printf("║                   Firmware %-24s     ║\n", firmwareFullVersion);
#ifdef ENABLE_BLE
   Serial.println("║         + Entradas Digitales + Servidor BLE                 ║");
#else
   Serial.println("║              + Entradas Digitales DI1/DI2                   ║");
#endif
   Serial.println("╚══════════════════════════════════════════════════════════════╝");
   
   // Inicializar RS-485 para compatibilidad
   RS485_Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RX2, RS485_TX2);
   delay(500);
   
   Serial.println("🔧 Configuración de Hardware:");
   Serial.printf("   TECLADO 1: D0=GPIO%d, D1=GPIO%d (Principal)\n", WIEGAND1_D0, WIEGAND1_D1);
   Serial.printf("   TECLADO 2: D0=GPIO%d, D1=GPIO%d (Secundario)\n", WIEGAND2_D0, WIEGAND2_D1);
   Serial.printf("   RS485: GPIO%d/%d (Compatibilidad)\n", RS485_RX2, RS485_TX2);
   Serial.printf("   Relés: R1=GPIO%d, R2=GPIO%d\n", RELE1_PIN, RELE2_PIN);
   
  // =================== INICIALIZACIÓN EEPROM ===================
  // Tamaño fijo: el mapa de memoria requiere 4096 bytes y está verificado
  // con static_assert en compilación (un tamaño menor rompería silenciosamente
  // toda la persistencia, así que no hay fallback a tamaños inferiores).
  Serial.println("\n📦 Inicializando EEPROM...");

  bool eepromOK = false;
  if (EEPROM.begin(EEPROM_TOTAL_SIZE)) {
    // Test NO destructivo: usa la zona scratch reservada (fuera de todas las
    // estructuras) y restaura el contenido original al terminar
    uint8_t orig0 = EEPROM.read(EEPROM_TEST_SCRATCH_OFFSET);
    uint8_t orig1 = EEPROM.read(EEPROM_TEST_SCRATCH_OFFSET + 1);

    EEPROM.write(EEPROM_TEST_SCRATCH_OFFSET, 0x55);
    EEPROM.write(EEPROM_TEST_SCRATCH_OFFSET + 1, 0xAA);
    if (EEPROM.commit() &&
        EEPROM.read(EEPROM_TEST_SCRATCH_OFFSET) == 0x55 &&
        EEPROM.read(EEPROM_TEST_SCRATCH_OFFSET + 1) == 0xAA) {
      eepromOK = true;
    }

    // Restaurar contenido original de la zona scratch
    EEPROM.write(EEPROM_TEST_SCRATCH_OFFSET, orig0);
    EEPROM.write(EEPROM_TEST_SCRATCH_OFFSET + 1, orig1);
    EEPROM.commit();
  }

  if (eepromOK) {
    Serial.printf("📦 EEPROM configurada: %d bytes ✅\n", EEPROM_TOTAL_SIZE);
  } else {
    Serial.println("❌ ERROR CRÍTICO: EEPROM no verificada - la persistencia puede fallar");
  }

  // Mapa de memoria (solapamientos verificados en compilación con static_assert)
  Serial.println("\n🔍 === MAPA DE MEMORIA EEPROM ===");
  Serial.printf("   Config:        0 - %d (%d bytes)\n", (int)sizeof(Config), (int)sizeof(Config));
  Serial.printf("   DigitalInput:  %d - %d (%d bytes)\n",
                EEPROM_DIGITAL_INPUT_OFFSET,
                EEPROM_DIGITAL_INPUT_OFFSET + (int)sizeof(DigitalInputConfig),
                (int)sizeof(DigitalInputConfig));
  Serial.printf("   StoredCodes:   %d - %d (%d bytes, max %d códigos)\n",
                EEPROM_CODES_OFFSET,
                EEPROM_CODES_OFFSET + (int)sizeof(StoredCodes),
                (int)sizeof(StoredCodes), MAX_CODES);
  Serial.printf("   RemoteCodes:   NVS \"%s\" (%d bytes, max %d códigos)\n",
                REMOTE_CODES_NVS_NS, (int)sizeof(StoredRemoteCodes), MAX_REMOTE_CODES);
#ifdef ENABLE_BLE
  Serial.printf("   DeviceKey:     %d - %d (%d bytes)\n",
                EEPROM_DEVICE_KEY_OFFSET,
                EEPROM_DEVICE_KEY_OFFSET + (int)sizeof(DeviceKeyConfig),
                (int)sizeof(DeviceKeyConfig));
  Serial.printf("   BLEAuth:       %d - %d (%d bytes)\n",
                EEPROM_BLE_AUTH_OFFSET,
                EEPROM_BLE_AUTH_OFFSET + (int)sizeof(BLEAuthConfig),
                (int)sizeof(BLEAuthConfig));
#endif
  Serial.printf("   Scratch test:  %d - %d\n", EEPROM_TEST_SCRATCH_OFFSET, EEPROM_TEST_SCRATCH_OFFSET + 2);
  Serial.printf("   Total EEPROM:  %d bytes\n", EEPROM_TOTAL_SIZE);
  Serial.println("=====================================\n");
  
  loadConfiguration();
  loadTurnstileConfig();  // Cargar configuración del modo torno
  loadStoredCodes();
  loadStoredRemoteCodes();  // Cargar códigos remotos
  
  // =================== INICIALIZACIÓN OTA ===================
  loadOTAConfig();
  initializeDeviceInfo();
  setupRollback();
  
  // Inicializar tiempo del sistema
  systemStartTime = millis();
  lastTimeSync = systemStartTime;
  timeSynced = false;
  strncpy(currentTimeString, "No sincronizado", sizeof(currentTimeString) - 1);
  currentTimeString[sizeof(currentTimeString) - 1] = '\0';
 
  Serial.printf("⚙️ Configuración cargada:\n");
  Serial.printf("   Serial MQTT (fijo): %s\n", fixedSerialNumber.c_str());
  Serial.printf("   Nombre dispositivo: %s\n", deviceName);
  Serial.printf("   DHCP: %s\n", useDhcp ? "SÍ" : "NO");
  Serial.printf("   Modo torno: %s\n", isTurnstileModeEnabled() ? "ACTIVO" : "INACTIVO");
  if (isTurnstileModeEnabled()) {
    Serial.printf("   • Teclado 1 → Relé %d\n", config.turnstile.keyboard1_relay);
    Serial.printf("   • Teclado 2 → Relé %d\n", config.turnstile.keyboard2_relay);
  }
   Serial.printf("   Acceso local: %s\n", localAccessBlocked ? "BLOQUEADO" : "PERMITIDO");
   Serial.printf("   Códigos almacenados: %d/%d\n", storedCodes->count, MAX_CODES);
 
#if !A2_BOARD_A2V3
  // Alimentar PHY LAN8720 (solo A2 clásico)
  pinMode(ETH_PHY_POWER_PIN, OUTPUT);
  digitalWrite(ETH_PHY_POWER_PIN, HIGH);
   delay(100);
   Serial.println("⚡ PHY LAN8720 alimentado");
#endif

  // RTC DS3231 (A2v3): sembrar la hora del sistema antes que nada de red,
  // para que franjas horarias y logs tengan hora aun sin conectividad
  rtcTimeBegin();
  // Pantalla de estado SSD1306 (A2v3)
  statusDisplayBegin();
   
   // Configurar pines de relés
   pinMode(RELE1_PIN, OUTPUT);
   digitalWrite(RELE1_PIN, LOW);
   pinMode(RELE2_PIN, OUTPUT);
   digitalWrite(RELE2_PIN, LOW);
   Serial.println("⚡ Relés inicializados (OFF)");
   
   // =================== CONFIGURAR ENTRADAS DIGITALES ===================
   pinMode(DI1_PIN, INPUT);  // GPIO36 no tiene pull-up interna
   pinMode(DI2_PIN, INPUT);  // GPIO39 no tiene pull-up interna
   
   // Inicializar estados con lectura actual
   di1State.lastState = digitalRead(DI1_PIN);
   di1State.currentState = di1State.lastState;
   di1State.relayActivated = false;
   di1State.waitingForLow = false;
   
   di2State.lastState = digitalRead(DI2_PIN);
   di2State.currentState = di2State.lastState;
   di2State.relayActivated = false;
   di2State.waitingForLow = false;
   
   Serial.printf("🔌 Entradas digitales configuradas: DI1=GPIO%d, DI2=GPIO%d\n", DI1_PIN, DI2_PIN);
   Serial.printf("   Estado inicial: DI1=%s, DI2=%s\n",
                 di1State.lastState ? "HIGH" : "LOW",
                 di2State.lastState ? "HIGH" : "LOW");
   
   // Cargar configuración de entradas digitales
   loadDigitalInputConfig();
   
   // Configurar teclado Wiegand 1
   pinMode(WIEGAND1_D0, INPUT_PULLUP);
   pinMode(WIEGAND1_D1, INPUT_PULLUP);
   attachInterrupt(digitalPinToInterrupt(WIEGAND1_D0), handleWiegand1D0, FALLING);
   attachInterrupt(digitalPinToInterrupt(WIEGAND1_D1), handleWiegand1D1, FALLING);
   Serial.printf("🔐 Teclado 1 configurado: GPIO%d/%d con interrupciones\n", WIEGAND1_D0, WIEGAND1_D1);
   
   // Configurar teclado Wiegand 2
   pinMode(WIEGAND2_D0, INPUT_PULLUP);
   pinMode(WIEGAND2_D1, INPUT_PULLUP);
   attachInterrupt(digitalPinToInterrupt(WIEGAND2_D0), handleWiegand2D0, FALLING);
   attachInterrupt(digitalPinToInterrupt(WIEGAND2_D1), handleWiegand2D1, FALLING);
   Serial.printf("🔐 Teclado 2 configurado: GPIO%d/%d con interrupciones\n", WIEGAND2_D0, WIEGAND2_D1);
   
   // Registrar callback de Ethernet
   WiFi.onEvent(WiFiEvent);
   
   // Iniciar Ethernet
  Serial.println("🌐 Inicializando Ethernet...");
#if A2_BOARD_A2V3
  // A2v3: W5500 por SPI dedicado (core Arduino 3.x). Pulso de reset previo.
  pinMode(W5500_RST_PIN, OUTPUT);
  digitalWrite(W5500_RST_PIN, LOW);
  delay(50);
  digitalWrite(W5500_RST_PIN, HIGH);
  delay(250);
  SPI.begin(W5500_SCK_PIN, W5500_MISO_PIN, W5500_MOSI_PIN, W5500_CS_PIN);
  if (!ETH.begin(ETH_PHY_W5500, 1, W5500_CS_PIN, W5500_INT_PIN, W5500_RST_PIN, SPI)) {
    Serial.println("❌ ETH.begin(W5500) falló - revisar cableado SPI");
  }
  if (!useDhcp) {
    ETH.config(staticIP, staticGateway, staticSubnet, staticDns);
  }
#else
  // A2 clásico: LAN8720 RMII (core Arduino 2.x)
  if (useDhcp) {
    // Compatibilidad con Arduino IDE v3.3.0 y PlatformIO
    // Arduino IDE v3.3.0 usa ETH_PHY_TYPE como primer parámetro
    // PlatformIO usa ETH_PHY_ADDR como primer parámetro
    #if !defined(PLATFORMIO)
      ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER_PIN, ETH_CLK_MODE);
    #else
      ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER_PIN, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
    #endif
  } else {
    // Compatibilidad con Arduino IDE v3.3.0 y PlatformIO
    // Arduino IDE v3.3.0 usa ETH_PHY_TYPE como primer parámetro
    // PlatformIO usa ETH_PHY_ADDR como primer parámetro
    #if !defined(PLATFORMIO)
      ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER_PIN, ETH_CLK_MODE);
    #else
      ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER_PIN, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
    #endif
    ETH.config(staticIP, staticGateway, staticSubnet, staticDns);
  }
#endif
   
   ETH.setHostname(deviceName);

  // Arranque NO bloqueante: la red conecta en segundo plano (evento GOT_IP).
  // El servidor web arranca ya (escucha en todas las interfaces) y la lógica
  // de accesos queda operativa de inmediato.
  setupWebServer();
  Serial.println("🌐 Ethernet conectando en segundo plano...");

  // WiFi de gestión: AP con SSID = serial del equipo, ventana de 60 s
  // (extensible con clientes conectados) + STA si está configurada en NVS.
  wifiManagerBegin(fixedSerialNumber);

  // Módem 4G: no-op salvo A2_FEATURE_GSM_MODEM (A2v3 o A2 con socket 4G)
  gsmModemBegin();
   
  Serial.println("\n🎯 SISTEMA DUAL WIEGAND CON SEGURIDAD COMPLETO LISTO");
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.printf("✅ Teclado 1 (GPIO%d/%d): Operativo\n", WIEGAND1_D0, WIEGAND1_D1);
  Serial.printf("✅ Teclado 2 (GPIO%d/%d): Operativo\n", WIEGAND2_D0, WIEGAND2_D1);
  Serial.println("🔒 Sistema de seguridad: Activo");
  Serial.println("📡 Serial MQTT fijo: " + fixedSerialNumber);
  Serial.println("💾 Capacidad códigos: " + String(MAX_CODES));
  Serial.println("💡 Ambos teclados pueden usarse simultáneamente");
  
  if (ethConnected) {
    Serial.println("🌐 Interfaz web: http://" + ip.toString());
  } else {
    Serial.println("🌐 Red: conectando en segundo plano (la IP se mostrará al obtenerla)");
    Serial.println("✅ Códigos locales: operativos desde ya (sin esperar a la red)");
  }

#ifdef ENABLE_BLE
  // Inicializar servidor BLE
  initBLE();
  Serial.println("🔵 BLE: Servidor activo y esperando conexiones");
#endif

  Serial.println("═══════════════════════════════════════════════════════════\n");
 }
 
 // =================== FUNCIÓN LOOP PRINCIPAL MEJORADA ===================
 void loop() {
   unsigned long currentTime = millis();

  // ========== GESTIÓN WIFI (AP ventana + STA) Y MÓDEM 4G ==========
  // No bloqueantes: la caída de cualquier interfaz nunca detiene los accesos
  wifiManagerLoop();
  gsmModemLoop();

  // ========== RTC + PANTALLA DE ESTADO (A2v3; no-op en A2) ==========
  rtcTimeLoop();
  statusDisplayLoop();

#ifdef ENABLE_BLE
  // ========== REINICIO PENDIENTE (para cambios de red BLE) ==========
  if (pendingRestartTime > 0 && currentTime >= pendingRestartTime) {
    Serial.println("🔄 [LOOP] Ejecutando reinicio programado para aplicar configuración de red...");
    delay(100);  // Pequeña pausa para asegurar que los logs se envían
    ESP.restart();
  }
  
  // ========== PROCESAR CÓDIGO BLE PENDIENTE (FF04) ==========
  // El guardado en EEPROM se difiere aquí para evitar crashes en el callback BLE
  if (pendingCode.pending) {
    pendingCode.pending = false;
    Serial.println("💾 [LOOP] Guardando código pendiente en EEPROM...");
    saveStoredCodes();
    Serial.printf("💾 [LOOP] Código '%s' guardado en EEPROM\n", pendingCode.value);
    
    // Publicar evento MQTT
    if (mqttClient.connected()) {
      String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/codes";
      String eventMsg = String("{") +
        "\"event\":\"CODE_ADDED\"," +
        "\"source\":\"BLE\"," +
        "\"user\":\"" + bleConnectedUser + "\"," +
        "\"code_type\":\"" + String(pendingCode.type) + "\"," +
        "\"code_value\":\"" + String(pendingCode.value) + "\"," +
        "\"keyboard\":" + String(pendingCode.keyboard) + "," +
        "\"relay\":" + String(pendingCode.relay) + "," +
        "\"total_codes\":" + String(storedCodes->count) + "," +
        "\"timestamp\":\"" + getTimestamp() + "\"" +
      "}";
      mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
    }
  }
#endif // ENABLE_BLE
   
  // ========== CONEXIÓN MQTT (NO BLOQUEANTE) ==========
  static unsigned long lastReconnectAttempt = 0;
  static int reconnectBackoff = 5000;  // Backoff exponencial inicial 5s
  
  // Failover/failback de interfaz (ETH↔WiFi desde Fase 1): reconectar MQTT
  NetIface mqttIface;
  if (netMqttIfaceChanged(&mqttIface) && mqttIface != NET_IFACE_NONE && mqttClient.connected()) {
    Serial.printf("🔄 Cambio de interfaz de red (→ %s): reconectando MQTT\n", netIfaceName(mqttIface));
    mqttClient.disconnect();
    mqttConnectionPending = true;
  }

  // Procesar conexión pendiente desde callback ETH
  if (mqttConnectionPending && netHasConnectivity()) {
    mqttConnectionPending = false;
    Serial.println("📡 Procesando conexión MQTT pendiente...");
    connectToMqtt();
    lastReconnectAttempt = currentTime;
    reconnectBackoff = 5000;  // Reset backoff
  }

  // Reconexión MQTT con backoff exponencial
  if (!mqttClient.connected() && netHasConnectivity() && !mqttConnectionPending) {
    if (currentTime - lastReconnectAttempt > reconnectBackoff) {
      lastReconnectAttempt = currentTime;
      Serial.printf("🔄 Reconexión MQTT (backoff: %ds)...\n", reconnectBackoff/1000);
      connectToMqtt();
      
      // Backoff exponencial: 5s -> 10s -> 20s -> 30s (max)
      if (!mqttClient.connected()) {
        reconnectBackoff = min(reconnectBackoff * 2, 30000);
      } else {
        reconnectBackoff = 5000;  // Reset en conexión exitosa
      }
    }
  }
  
  // Procesar cola MQTT si está conectado (no bloqueante)
  if (mqttClient.connected()) {
    if (!mqttClient.loop()) {
      Serial.println("❌ Error en mqttClient.loop()");
      mqttClient.disconnect();
      reconnectBackoff = 5000;  // Reset backoff para reconectar pronto
    }
  }
   
  // ========== PROCESAMIENTO DE TECLADOS ==========
  processWiegand1Data();
  processWiegand2Data();
  processRS485Keypad();
  
  // ========== PROCESAMIENTO DE ENTRADAS DIGITALES ==========
  processDigitalInput(1, di1State, digitalInputConfig.di1_enabled, 
                     digitalInputConfig.di1_relay, digitalInputConfig.di1_duration_ms, 
                     digitalInputConfig.di1_inverse);
  processDigitalInput(2, di2State, digitalInputConfig.di2_enabled,
                     digitalInputConfig.di2_relay, digitalInputConfig.di2_duration_ms,
                     digitalInputConfig.di2_inverse);
  
  // ========== VERIFICACIÓN DE TIMEOUT DEL MODO TORNO ==========
  checkPendingRequestTimeout();
   
   // ========== TIMEOUTS DE PIN (SOLO EN MODO TORNO CON SOLICITUD PENDIENTE) ==========
   // Solo limpiar PINs si estamos en modo torno y hay una solicitud pendiente
   // Esto evita interferir con la lectura normal de códigos
   if (isTurnstileModeEnabled() && pendingRequest.active) {
    if (strlen(currentPin1) > 0 && (currentTime - lastKeyPressTime1 > keyPressTimeout)) {
      Serial.printf("⏰ [TECLADO 1] Timeout PIN en modo torno (%lu ms) - Limpiando: '%s' (solicitud pendiente activa)\n", 
                    keyPressTimeout, currentPin1);
      currentPin1[0] = '\0';
    }
    
    if (strlen(currentPin2) > 0 && (currentTime - lastKeyPressTime2 > keyPressTimeout)) {
      Serial.printf("⏰ [TECLADO 2] Timeout PIN en modo torno (%lu ms) - Limpiando: '%s' (solicitud pendiente activa)\n", 
                    keyPressTimeout, currentPin2);
      currentPin2[0] = '\0';
    }
   }
 
   // ========== SISTEMA DE SEGURIDAD ==========
   checkAccessBlock();
   
  // ========== CONTROL DE RELÉS ==========
  checkRelayTimeout();
  
#ifdef ENABLE_BLE
  // ========== ACTUALIZACIÓN DE ESTADO BLE ==========
  static unsigned long lastBLEStatusUpdate = 0;
  if (currentTime - lastBLEStatusUpdate > 1000) { // Cada segundo
    lastBLEStatusUpdate = currentTime;
    if (bleDeviceConnected) {
      updateBLEStatus();
    }
  }
  
  // ========== TIMEOUT DE AUTENTICACIÓN BLE ==========
  // Si hay un dispositivo conectado pero no autenticado, desconectarlo tras timeout
  if (bleDeviceConnected && !bleAuthenticated && bleConnectionTime > 0) {
    if (currentTime - bleConnectionTime > BLE_AUTH_TIMEOUT_MS) {
      Serial.println("⚠️ [BLE] TIMEOUT DE AUTENTICACIÓN - Desconectando dispositivo no autenticado");
      Serial.printf("⚠️ [BLE] Tiempo conectado sin autenticar: %lu ms\n", currentTime - bleConnectionTime);
      
      // Publicar evento de timeout
      if (mqttClient.connected()) {
        String eventTopic = "swatidhome/events/" + fixedSerialNumber + "/ble";
        String eventMsg = String("{") +
          "\"event\":\"AUTH_TIMEOUT\"," +
          "\"device\":\"" + fixedSerialNumber + "\"," +
          "\"reason\":\"no_authentication_received\"," +
          "\"timeout_ms\":" + String(BLE_AUTH_TIMEOUT_MS) + "," +
          "\"timestamp\":\"" + getTimestamp() + "\"" +
        "}";
        mqttClient.publish(eventTopic.c_str(), eventMsg.c_str());
      }
      
      // Desconectar el cliente
      if (pServer != nullptr) {
        pServer->disconnect(0);
      }
      bleConnectionTime = 0;
    }
  }
#endif
  
  // ========== VERIFICACIÓN DE ACTUALIZACIONES OTA ==========
  if (otaConfig.autoUpdateEnabled && 
      currentTime - otaConfig.lastCheck > otaConfig.checkInterval * 3600000) {
    checkForUpdates();
  }
   
   // ========== SERVIDOR WEB ==========
   server.handleClient();
   
   // ========== DEBUG PERIÓDICO ==========
   static unsigned long lastDebugTime = 0;
  // Función de debug eliminada para reducir tamaño
  // if (currentTime - lastDebugTime > 30000) { // Cada 30 segundos
  //   lastDebugTime = currentTime;
  //   printSystemStatus(currentTime);
  // }
   
   // ========== KEEPALIVE MQTT ==========
   static unsigned long lastKeepalive = 0;
   if (currentTime - lastKeepalive > 60000) { // Cada minuto
     lastKeepalive = currentTime;
     
     if (mqttClient.connected()) {
       // Enviar ping de keepalive
       String keepaliveTopic = "swatidhome/keepalive/" + fixedSerialNumber;
       String keepalivePayload = "{\"status\":\"online\",\"uptime\":" + String(millis()) + "}";
       bool published = mqttClient.publish(keepaliveTopic.c_str(), keepalivePayload.c_str(), false);
       if (!published) {
         Serial.println("❌ Error enviando keepalive MQTT");
       }
     }
     
     // Verificar memoria baja
     if (ESP.getFreeHeap() < 10000) {
       Serial.printf("⚠️ ADVERTENCIA: Memoria baja: %d bytes\n", ESP.getFreeHeap());
       publishError(8, "Memoria baja detectada: " + String(ESP.getFreeHeap()) + " bytes");
     }
     
    // Aviso de MQTT caído prolongado (SIN reiniciar: un broker inalcanzable
    // no es un fallo del MCU; los accesos locales siguen operativos y la
    // reconexión con backoff sigue intentándolo)
    static unsigned long mqttDisconnectedTime = 0;
    if (!mqttClient.connected() && netHasConnectivity()) {
      if (mqttDisconnectedTime == 0) {
        mqttDisconnectedTime = currentTime;
      } else if (currentTime - mqttDisconnectedTime > 300000) { // 5 minutos
        Serial.printf("⚠️ MQTT sin conexión desde hace %lu min (accesos locales operativos)\n",
                      (currentTime - mqttDisconnectedTime) / 60000UL);
      }
    } else {
      mqttDisconnectedTime = 0;
    }
    
    // Log de estado de autonomía cuando no hay conectividad
    if (!netHasConnectivity()) {
      static unsigned long lastAutonomyLog = 0;
      if (currentTime - lastAutonomyLog > 300000) { // Cada 5 minutos
        lastAutonomyLog = currentTime;
        Serial.println("🏠 MODO AUTÓNOMO: Sistema funcionando sin conectividad Ethernet");
        Serial.printf("   Códigos locales disponibles: %d/%d\n", storedCodes->count, MAX_CODES);
        Serial.printf("   Modo validación: %s\n", storedCodes->localValidationFirst ? "Local primero" : "Remoto primero");
        Serial.println("   ✅ Sistema completamente operativo en modo autónomo");
      }
    }
   }
   
   // Pequeña pausa para evitar saturar el procesador
   delay(1);
 }
 
 /*
  * =================== DOCUMENTACIÓN DEL PROYECTO ===================
  * 
  * INSTALACIÓN Y CONFIGURACIÓN:
  * 
  * 1. PREPARACIÓN DEL ENTORNO:
  *    - Instalar Arduino IDE versión 1.8.19 o superior
  *    - Añadir soporte para ESP32: File -> Preferences -> Additional Board Manager URLs:
  *      https://dl.espressif.com/dl/package_esp32_index.json
  *    - Tools -> Board -> Boards Manager -> Buscar "ESP32" e instalar
  * 
  * 2. LIBRERÍAS REQUERIDAS:
  *    - PubSubClient por Nick O'Leary (versión 2.8.0 o superior)
  *    - ArduinoJson por Benoit Blanchon (versión 6.21.0 o superior)
  *    Instalación: Tools -> Manage Libraries -> Buscar e instalar
  * 
  * 3. CONFIGURACIÓN DE LA PLACA:
  *    - Board: "ESP32 Dev Module"
  *    - CPU Frequency: "240MHz (WiFi/BT)"
  *    - Flash Frequency: "80MHz"
  *    - Flash Mode: "QIO"
  *    - Flash Size: "4MB (32Mb)"
  *    - Partition Scheme: "Default 4MB with spiffs"
  *    - Upload Speed: "921600"
  * 
  * 4. CONEXIONES HARDWARE KC868-A2:
  *    - Relé 1: GPIO15
  *    - Relé 2: GPIO2
  *    - Teclado Wiegand 1: D0=GPIO33, D1=GPIO14
  *    - Teclado Wiegand 2: D0=GPIO4, D1=GPIO16
  *    - RS485: RX=GPIO35, TX=GPIO32
  *    - Ethernet LAN8720: MDC=GPIO23, MDIO=GPIO18, CLK=GPIO17, PWR=GPIO5
  * 
  * 5. CONFIGURACIÓN INICIAL:
  *    - Al primer arranque, el dispositivo generará un serial único
  *    - Conectar cable Ethernet y verificar asignación IP por DHCP
  *    - Acceder a la interfaz web: http://[IP_ASIGNADA]
  *    - Credenciales por defecto: admin/admin
  * 
  * =================== API MQTT COMPLETA ===================
  * 
  * TÓPICOS DE COMANDO (ENVÍO AL DISPOSITIVO):
  * 
  * 1. Activación de Relé:
  *    Tópico: swatidhome/command/[SERIAL]/relay
  *    Mensaje: {
  *      "message_id": 123,
  *      "device": "[SERIAL]",
  *      "message_type": 0,
  *      "message_info": {
  *        "relay_number": 1,
  *        "duration": 3.0
  *      }
  *    }
  * 
  * 2. Solicitud de Información:
  *    Tópico: swatidhome/command/[SERIAL]/info
  *    Mensaje: {
  *      "message_id": 124,
  *      "device": "[SERIAL]",
  *      "message_type": 1
  *    }
  * 
  * 3. Comandos de Sistema:
  *    Tópico: swatidhome/command/[SERIAL]/system
  *    Mensaje: {
  *      "message_id": 125,
  *      "device": "[SERIAL]",
  *      "message_type": 2,
  *      "message_info": "reboot"  // "reboot", "reset", "update"
  *    }
  * 
  * 4. Comandos de Seguridad:
  *    Tópico: swatidhome/command/[SERIAL]/security
  *    Mensaje: {
  *      "message_id": 126,
  *      "device": "[SERIAL]",
  *      "message_type": 3,
  *      "message_info": {
  *        "security_command": "block_local_access"
  *        // Opciones: "block_local_access", "unblock_local_access", 
  *        // "set_block_duration", "set_max_failed_attempts"
  *      }
  *    }
  * 
  * 5. Validación de Acceso:
  *    Tópico: swatidhome/command/[SERIAL]/access
  *    Mensaje automático del dispositivo para validación remota
  * 
  * 6. Respuesta de Validación:
  *    Tópico: swatidhome/command/[SERIAL]/granted
  *    Mensaje: {
  *      "access_granted": true,
  *      "code_type": "PIN",
  *      "code_value": "1234",
  *      "duration": 2000,
  *      "relay_number": 1
  *    }
  * 
  * TÓPICOS DE RESPUESTA (RECEPCIÓN DEL DISPOSITIVO):
  * 
  * 1. Información del Dispositivo:
  *    Tópico: swatidhome/[SERIAL]/info
  * 
  * 2. Errores del Sistema:
  *    Tópico: swatidhome/errors/[SERIAL]/rx
  * 
  * 3. Respuestas a Comandos:
  *    Tópico: swatidhome/response/[SERIAL]/rx
  * 
  * 4. Estado de Relés:
  *    Tópico: swatidhome/[SERIAL]/relay_enable
  * 
  * 5. Eventos de Acceso:
  *    Tópico: swatidhome/events/[SERIAL]/access
  * 
  * 6. Intentos de Acceso Fallidos:
  *    Tópico: swatidhome/events/[SERIAL]/failed_access
  * 
  * =================== CÓDIGOS DE ERROR ===================
  * 
  * 1: Error de conexión de red
  * 2: Error de configuración
  * 3: Mensaje de inicio/estado
  * 4: Cambio de configuración
  * 5: Sistema de seguridad
  * 6: Bloqueo de acceso activado
  * 
  * =================== FUNCIONALIDADES DE SEGURIDAD ===================
  * 
  * - Bloqueo automático tras intentos fallidos (configurable 1-10)
  * - Duración de bloqueo configurable (30-3600 segundos)
  * - Bloqueo/desbloqueo remoto vía MQTT
  * - Reset automático del contador tras 5 minutos sin actividad
  * - Notificaciones MQTT de todos los eventos de seguridad
  * - Validación dual: local primero o remoto primero
  * 
  * =================== EJEMPLO DE INTEGRACIÓN PYTHON ===================
  * 
  * import paho.mqtt.client as mqtt
  * import json
  * import time
  * 
  * def on_connect(client, userdata, flags, rc):
  *     print(f"Conectado con código: {rc}")
  *     client.subscribe("swatidhome/events/+/access")
  *     client.subscribe("swatidhome/events/+/failed_access")
  * 
  * def on_message(client, userdata, msg):
  *     try:
  *         data = json.loads(msg.payload.decode())
  *         if "access" in msg.topic:
  *             print(f"Acceso: {data['success']} - {data['source']}")
  *         elif "failed_access" in msg.topic:
  *             print(f"Acceso fallido: {data['reason']}")
  *     except:
  *         pass
  * 
  * def activate_relay(client, serial, relay=1, duration=2.0):
  *     message = {
  *         "message_id": int(time.time()),
  *         "device": serial,
  *         "message_type": 0,
  *         "message_info": {
  *             "relay_number": relay,
  *             "duration": duration
  *         }
  *     }
  *     client.publish(f"swatidhome/command/{serial}/relay", json.dumps(message))
  * 
  * client = mqtt.Client()
  * client.username_pw_set("swatidhome", "Swatid2025!")
  * client.on_connect = on_connect
  * client.on_message = on_message
  * client.connect("188.245.213.181", 1883, 60)
  * client.loop_forever()
  * 
  * =================== EJEMPLO DE INTEGRACIÓN NODE.JS ===================
  * 
  * const mqtt = require('mqtt');
  * const client = mqtt.connect('mqtt://188.245.213.181:1883', {
  *   username: 'swatidhome',
  *   password: 'Swatid2025!'
  * });
  * 
  * client.on('connect', () => {
  *   console.log('Conectado al broker MQTT');
  *   client.subscribe('swatidhome/events/+/access');
  *   client.subscribe('swatidhome/events/+/failed_access');
  * });
  * 
  * client.on('message', (topic, message) => {
  *   try {
  *     const data = JSON.parse(message.toString());
  *     if (topic.includes('access')) {
  *       console.log(`Acceso: ${data.success} - ${data.source}`);
  *     } else if (topic.includes('failed_access')) {
  *       console.log(`Acceso fallido: ${data.reason}`);
  *     }
  *   } catch (e) {
  *     console.error('Error parseando mensaje:', e);
  *   }
  * });
  * 
  * function activateRelay(serial, relay = 1, duration = 2.0) {
  *   const message = {
  *     message_id: Date.now(),
  *     device: serial,
  *     message_type: 0,
  *     message_info: {
  *       relay_number: relay,
  *       duration: duration
  *     }
  *   };
  *   client.publish(`swatidhome/command/${serial}/relay`, JSON.stringify(message));
  * }
  * 
  * =================== SOLUCIÓN DE PROBLEMAS ===================
  * 
  * 1. Dispositivo no conecta a Ethernet:
  *    - Verificar cable de red
  *    - Comprobar configuración DHCP del router
  *    - Revisar conexiones del PHY LAN8720
  * 
  * 2. Teclados Wiegand no responden:
  *    - Verificar alimentación 12V del teclado
  *    - Comprobar conexiones D0/D1
  *    - Revisar configuración de pines en el código
  * 
  * 3. MQTT desconectado:
  *    - Verificar credenciales MQTT
  *    - Comprobar conectividad de red
  *    - Revisar configuración del broker
  * 
  * 4. Problemas de memoria:
  *    - El sistema soporta 500 códigos máximo
  *    - EEPROM configurada para 4KB
  *    - Monitorear uso de memoria en Serial Monitor
  * 
  * 5. Interfaz web no accesible:
  *    - Verificar IP asignada en Serial Monitor
  *    - Comprobar credenciales admin/admin
  *    - Probar modo AP de emergencia
  * 
  * =================== LICENCIA Y SOPORTE ===================
  * 
  * Este código está desarrollado para la placa KinCony KC868-A2
  * Compatible con Arduino IDE y ESP32
  * 
  * Para soporte técnico:
  * - Revisar Serial Monitor para debug detallado
  * - Verificar todas las conexiones hardware
  * - Consultar documentación de la placa KC868-A2
  * 
 * Versión: 1.7.0-COMPLETE-SECURITY
 * Fecha: Junio 2025
 * Funcionalidades: Sistema dual Wiegand con seguridad avanzada
 */

// =================== HANDLERS WEB PARA CÓDIGOS REMOTOS ===================

void handleRemoteCodes() {
  if (!webAuth()) return;

  // Obtener parámetros de paginación y búsqueda
  int page = server.arg("page").toInt();
  if (page < 1) page = 1;
  
  String searchTerm = server.arg("search");
  searchTerm.trim();
  
  const int ITEMS_PER_PAGE = 20;
  int startIndex = (page - 1) * ITEMS_PER_PAGE;
  int endIndex = startIndex + ITEMS_PER_PAGE;

  String html = webPageBegin("Códigos Remotos - Controladora A2");
  html += R"=====(
  <h1>Gestión de Códigos Remotos - Controladora A2</h1>

  <div class='remote-info'>
    <strong>🌐 Información de Códigos Remotos:</strong><br>
    • Los códigos remotos funcionan sin conexión a internet<br>
    • Incluyen franjas horarias de validación<br>
    • Se validan localmente con restricciones de tiempo<br>
    • Capacidad máxima: 100 códigos remotos<br>
    • Serial fijo MQTT: )=====";
  html += fixedSerialNumber;

  html += R"=====(
  </div>

  <div class='time-info'>
    <strong>⏰ Franjas Horarias:</strong><br>
    • Formato: HH:MM - HH:MM (24 horas)<br>
    • Días de la semana: Lunes=1, Martes=2, ..., Domingo=64<br>
    • Múltiples franjas por código (máximo 4)<br>
    • Ejemplo: 08:00-14:00 Lunes-Viernes (1+2+4+8+16=31)
  </div>

  <div class='export-section'>
    <a href='/export/remote-codes'><button class='btn-info'>Exportar a CSV</button></a>
  </div>

  <form action='/remote-codes/add' method='post'>
    <h2>Añadir nuevo código remoto</h2>
    <label for='type'>Tipo:</label>
    <select name='type'>
      <option value='PIN'>PIN (4-6 dígitos)</option>
      <option value='TAG'>TAG (tarjeta RFID/NFC)</option>
    </select>

    <label for='value'>Código:</label>
    <input type='text' name='value' maxlength='16' placeholder='Ej: 1234 o código de tarjeta' required>

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

    <label for='time_slots_count'>Número de franjas horarias:</label>
    <select name='time_slots_count' onchange='toggleTimeSlots()'>
      <option value='0'>Sin restricción horaria</option>
      <option value='1'>1 franja horaria</option>
      <option value='2'>2 franjas horarias</option>
      <option value='3'>3 franjas horarias</option>
      <option value='4'>4 franjas horarias</option>
    </select>

    <div id='time_slots' style='display:none;'>
      <h3>Franjas Horarias</h3>
      <div id='time_slots_content'></div>
    </div>

    <button type='submit'>Añadir Código Remoto</button>
  </form>

  <!-- Buscador y filtros -->
  <div style='background: #f8f9fa; padding: 15px; border-radius: 8px; margin: 20px 0;'>
    <h3>Buscar Códigos Remotos</h3>
    <form method='GET' action='/remote-codes' style='display: flex; gap: 10px; align-items: center; flex-wrap: wrap;'>
      <input type='text' name='search' placeholder='Buscar por código o tipo...' value=')=====";
  html += searchTerm;
  html += R"=====(' style='flex: 1; min-width: 200px; padding: 8px; border: 1px solid #ddd; border-radius: 4px;'>
      <button type='submit' style='background-color: #007bff; color: white; padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer;'>
        Buscar
      </button>
      <a href='/remote-codes' style='background-color: #6c757d; color: white; padding: 8px 16px; text-decoration: none; border-radius: 4px;'>
        Limpiar
      </a>
    </form>
  </div>

  <h2>Códigos Remotos Almacenados ()=====";
  html += String(storedRemoteCodes->count) + "/" + String(MAX_REMOTE_CODES);
  html += R"=====()</h2>
  <table>
    <tr><th>Tipo</th><th>Valor</th><th>Teclado</th><th>Relé</th><th>Franjas Horarias</th><th>Acción</th></tr>
)=====";

  // Filtrar y paginar códigos remotos
  int filteredCount = 0;
  int displayedCount = 0;
  
  for (int i = 0; i < storedRemoteCodes->count; i++) {
    // Aplicar filtro de búsqueda
    bool matchesFilter = true;
    if (searchTerm.length() > 0) {
      String codeType = String(storedRemoteCodes->codes[i].type);
      String codeValue = String(storedRemoteCodes->codes[i].value);
      String searchLower = searchTerm;
      searchLower.toLowerCase();
      
      String codeTypeLower = codeType;
      codeTypeLower.toLowerCase();
      String codeValueLower = codeValue;
      codeValueLower.toLowerCase();
      
      matchesFilter = (codeTypeLower.indexOf(searchLower) >= 0 || 
                      codeValueLower.indexOf(searchLower) >= 0);
    }
    
    if (matchesFilter) {
      filteredCount++;
      
      // Aplicar paginación
      if (filteredCount > startIndex && filteredCount <= endIndex) {
        html += "<tr>";
        html += "<td>" + String(storedRemoteCodes->codes[i].type) + "</td>";
        html += "<td>" + String(storedRemoteCodes->codes[i].value) + "</td>";
        
        String keyboardName = "Ambos";
        String keyboardIcon = "🔑";
        if (storedRemoteCodes->codes[i].keyboard_id == 1) {
          keyboardName = "Teclado 1";
          keyboardIcon = "🔑";
        } else if (storedRemoteCodes->codes[i].keyboard_id == 2) {
          keyboardName = "Teclado 2";
          keyboardIcon = "🔑";
        }
        
        html += "<td>" + keyboardIcon + " " + keyboardName + "</td>";
        html += "<td>⚡ Relé " + String(storedRemoteCodes->codes[i].relay) + "</td>";
        
        // Mostrar franjas horarias
        html += "<td>";
        if (storedRemoteCodes->codes[i].time_slots_count == 0) {
          html += "<span style='color: #666; font-style: italic;'>Sin restricción</span>";
        } else {
          for (int j = 0; j < storedRemoteCodes->codes[i].time_slots_count; j++) {
            TimeSlot& slot = storedRemoteCodes->codes[i].time_slots[j];
            html += "<div class='time-slot' style='margin-bottom: 4px; padding: 4px 8px; background-color: #f0f8ff; border-left: 3px solid #007bff; border-radius: 3px;'>";
            html += "<strong>⏰ " + String(slot.start_hour) + ":" + String(slot.start_minute < 10 ? "0" : "") + String(slot.start_minute);
            html += " - " + String(slot.end_hour) + ":" + String(slot.end_minute < 10 ? "0" : "") + String(slot.end_minute) + "</strong>";
            html += "<br><small style='color: #555;'>📅 " + getDaysOfWeekString(slot.days_of_week) + "</small>";
            html += "</div>";
          }
        }
        html += "</td>";
        
        html += "<td><a class='delete' href='/remote-codes/delete?type=" + String(storedRemoteCodes->codes[i].type);
        html += "&value=" + String(storedRemoteCodes->codes[i].value) + "'>Eliminar</a></td>";
        html += "</tr>";
        displayedCount++;
      }
    }
  }

  if (displayedCount == 0) {
    if (searchTerm.length() > 0) {
      html += "<tr><td colspan='6'>No se encontraron códigos remotos que coincidan con '" + searchTerm + "'</td></tr>";
    } else {
      html += "<tr><td colspan='6'>No hay códigos remotos almacenados</td></tr>";
    }
  }

  html += R"=====(
  </table>
  
  <!-- Paginación -->
  )=====";
  
  // Generar paginación para códigos remotos
  int totalPages = (filteredCount + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  if (totalPages > 1) {
    html += "<div style='margin: 20px 0; text-align: center;'>";
    html += "<p style='margin-bottom: 10px;'>Página " + String(page) + " de " + String(totalPages) + " (Mostrando " + String(displayedCount) + " de " + String(filteredCount) + " códigos remotos)</p>";
    
    // Botón anterior
    if (page > 1) {
      html += "<a href='/remote-codes?page=" + String(page - 1);
      if (searchTerm.length() > 0) {
        html += "&search=" + searchTerm;
      }
      html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'>Anterior</a>";
    }
    
    // Números de página
    int startPage = max(1, page - 2);
    int endPage = min(totalPages, page + 2);
    
    for (int p = startPage; p <= endPage; p++) {
      if (p == page) {
        html += "<span style='margin: 0 5px; padding: 8px 12px; background-color: #6c757d; color: white; border-radius: 4px;'>" + String(p) + "</span>";
      } else {
        html += "<a href='/remote-codes?page=" + String(p);
        if (searchTerm.length() > 0) {
          html += "&search=" + searchTerm;
        }
        html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'>" + String(p) + "</a>";
      }
    }
    
    // Botón siguiente
    if (page < totalPages) {
      html += "<a href='/remote-codes?page=" + String(page + 1);
      if (searchTerm.length() > 0) {
        html += "&search=" + searchTerm;
      }
      html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'>Siguiente </a>";
    }
    
    html += "</div>";
  } else if (filteredCount > 0) {
    html += "<div style='margin: 20px 0; text-align: center;'>";
    html += "<p>Mostrando " + String(filteredCount) + " códigos remotos</p>";
    html += "</div>";
  }
  
  html += R"=====(
  <br>
  <a href='/remote-codes/delete-all'><button class='btn-danger'>Eliminar Todos los Códigos Remotos</button></a>
  <br><br>
  <a href='/'><button style='background-color:#3498db'>Volver al inicio</button></a>

  <!-- Pie de página con datos de contacto -->
  <footer style='background-color: #2c3e50; color: white; padding: 20px; text-align: center; margin-top: 30px;'>
    <div style='max-width: 800px; margin: 0 auto;'>
      <h3 style='margin: 0 0 10px 0; color: #ecf0f1;'>Smart World And Things SLU</h3>
      <p style='margin: 5px 0; font-size: 14px;'>
        <strong>Web:</strong> <a href='https://www.swat-id.com' style='color: #3498db; text-decoration: none;'>www.swat-id.com</a> | 
        <strong>Tel:</strong> 633 44 84 27 | 
        <strong>Email:</strong> <a href='mailto:info@swat-id.com' style='color: #3498db; text-decoration: none;'>info@swat-id.com</a>
      </p>
      <p style='margin: 5px 0; font-size: 12px; color: #bdc3c7;'>Controladora A2 - SWATID | Sistema de Control de Acceso Dual Wiegand</p>
    </div>
  </footer>

  <script>
    function toggleTimeSlots() {
      const count = document.querySelector('select[name="time_slots_count"]').value;
      const container = document.getElementById('time_slots');
      const content = document.getElementById('time_slots_content');
      
      if (count == '0') {
        container.style.display = 'none';
        return;
      }
      
      container.style.display = 'block';
      content.innerHTML = '';
      
      for (let i = 0; i < parseInt(count); i++) {
        content.innerHTML += `
          <div style='border: 1px solid #ccc; padding: 15px; margin: 10px 0; border-radius: 5px;'>
            <h4>Franja ${i + 1}</h4>
            <label>Hora inicio: <input type='time' name='start_hour_${i}' required></label>
            <label>Hora fin: <input type='time' name='end_hour_${i}' required></label>
            <label>Días de la semana:</label>
            <div>
              <input type='checkbox' name='day_${i}_1' value='1'> Lunes
              <input type='checkbox' name='day_${i}_2' value='2'> Martes
              <input type='checkbox' name='day_${i}_4' value='4'> Miércoles
              <input type='checkbox' name='day_${i}_8' value='8'> Jueves
              <input type='checkbox' name='day_${i}_16' value='16'> Viernes
              <input type='checkbox' name='day_${i}_32' value='32'> Sábado
              <input type='checkbox' name='day_${i}_64' value='64'> Domingo
            </div>
          </div>
        `;
      }
    }
  </script>
</body></html>
)=====";

  server.send(200, "text/html", html);
}

void handleRemoteCodesAdd() {
  if (!webAuth()) return;

  if (server.hasArg("type") && server.hasArg("value") && server.hasArg("relay") && server.hasArg("keyboard_id")) {
    String type = server.arg("type");
    String value = server.arg("value");
    int relay = server.arg("relay").toInt();
    int keyboardId = server.arg("keyboard_id").toInt();
    int timeSlotsCount = server.arg("time_slots_count").toInt();

    value.trim();
    type.trim();

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

    if (isValid && relay >= 1 && relay <= 2 && keyboardId >= 0 && keyboardId <= 2 && timeSlotsCount >= 0 && timeSlotsCount <= 4) {
      // Procesar franjas horarias
      TimeSlot timeSlots[4];
      int validTimeSlots = 0;
      
      for (int i = 0; i < timeSlotsCount; i++) {
        String startTime = server.arg("start_hour_" + String(i));
        String endTime = server.arg("end_hour_" + String(i));
        
        if (startTime != "" && endTime != "") {
          int startHour = startTime.substring(0, 2).toInt();
          int startMinute = startTime.substring(3, 5).toInt();
          int endHour = endTime.substring(0, 2).toInt();
          int endMinute = endTime.substring(3, 5).toInt();
          
          // Calcular días de la semana
          uint8_t daysOfWeek = 0;
          for (int day = 0; day < 7; day++) {
            String dayValue = String(1 << day);
            if (server.hasArg("day_" + String(i) + "_" + dayValue)) {
              daysOfWeek |= (1 << day);
            }
          }
          
          if (daysOfWeek > 0) {
            timeSlots[validTimeSlots].start_hour = startHour;
            timeSlots[validTimeSlots].start_minute = startMinute;
            timeSlots[validTimeSlots].end_hour = endHour;
            timeSlots[validTimeSlots].end_minute = endMinute;
            timeSlots[validTimeSlots].days_of_week = daysOfWeek;
            validTimeSlots++;
          }
        }
      }
      
      bool added = addRemoteCode(type.c_str(), value.c_str(), keyboardId, relay, timeSlots, validTimeSlots);
      if (added) {
        String keyboardName = (keyboardId == 0) ? "Ambos teclados" : 
                             (keyboardId == 1) ? "Teclado 1" : "Teclado 2";
        Serial.printf("✅ Código remoto añadido: %s %s -> %s, Relé %d, Franjas: %d\n", 
                      type.c_str(), value.c_str(), keyboardName.c_str(), relay, validTimeSlots);
        server.sendHeader("Location", "/remote-codes");
        server.send(303);
        return;
      }
    }
    
    server.send(200, "text/html",
      "<html><head><meta charset='UTF-8'></head><body><h1>❌ Error al añadir código remoto</h1>"
      "<p>Verifique el formato y que no exista ya</p>"
      "<a href='/remote-codes'>Volver</a></body></html>");
  } else {
    server.send(400, "text/html",
      "<html><head><meta charset='UTF-8'></head><body><h1>❌ Faltan parámetros</h1>"
      "<a href='/remote-codes'>Volver</a></body></html>");
  }
}

void handleRemoteCodesDelete() {
  if (!webAuth()) return;

  if (server.hasArg("type") && server.hasArg("value")) {
    String type = server.arg("type");
    String value = server.arg("value");

    bool deleted = deleteRemoteCode(type.c_str(), value.c_str());
    if (deleted) {
      Serial.printf("🗑️ Código remoto eliminado: %s %s\n", type.c_str(), value.c_str());
    }

    server.sendHeader("Location", "/remote-codes");
    server.send(303);
  } else {
    server.send(400, "text/html", 
      "<html><body><h1>❌ Faltan parámetros</h1>"
      "<a href='/remote-codes'>Volver</a></body></html>");
  }
}

void handleRemoteCodesDeleteAll() {
  if (!webAuth()) return;

  deleteAllRemoteCodes();
  Serial.println("🗑️ Todos los códigos remotos eliminados");

  server.sendHeader("Location", "/remote-codes");
  server.send(303);
}

// =================== FUNCIONES DE GESTIÓN DE MEMORIA OPTIMIZADA ===================
// (ver local_codes.h)
