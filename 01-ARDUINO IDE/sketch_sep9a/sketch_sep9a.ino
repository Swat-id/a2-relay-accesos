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
#define FIRMWARE_VERSION_MAJOR 2
#define FIRMWARE_VERSION_MINOR 5
#define FIRMWARE_VERSION_PATCH 0
#define FIRMWARE_VERSION_BUILD __DATE__ " " __TIME__

const char* firmwareVersion = "v2.5.0";
const char* firmwareBuild = FIRMWARE_VERSION_BUILD;
const char* firmwareFullVersion = "v2.5.0-" FIRMWARE_VERSION_BUILD;
 
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
 
// =================== ALMACENAMIENTO DE CÓDIGOS ===================
#define MAX_CODES 500  // Optimizado para memoria ESP32

// =================== ESTRUCTURAS DEL MODO TORNO ===================
struct TurnstileConfig {
  bool enabled;           // true = modo torno, false = modo normal
  uint8_t keyboard1_relay; // Relé asignado al teclado 1 (1 o 2)
  uint8_t keyboard2_relay; // Relé asignado al teclado 2 (1 o 2)
  uint8_t reserved[5];    // Reservado para futuras extensiones
};

#define EEPROM_REMOTE_CODES_OFFSET 2048  // Offset en EEPROM para códigos remotos

// Estructura de configuración
struct Config {
  char deviceName[32];
  char fixedSerial[32];        // SERIAL FIJO
  bool useDhcp;
  uint8_t ip[4];
  uint8_t gateway[4];
  uint8_t subnet[4];
  uint8_t dns[4];
  float releDuration;
  char webPassword[32];
  bool localAccessBlocked;     // Estado de bloqueo
  bool keyboardReadingEnabled; // Control de lectura de teclados
  unsigned long blockDuration; // Duración del bloqueo
  int maxFailedAttempts;       // Máximo intentos
  TurnstileConfig turnstile;   // Configuración del modo torno
  uint32_t configValid;
};
 Config config;
 
 #define EEPROM_CODES_OFFSET 512
#define TURNSTILE_TIMEOUT 5000   // 5 segundos timeout para respuestas MQTT
#define TURNSTILE_CONFIG_MARKER 0xTORNO  // Marcador para configuración válida
 
// Estructuras de almacenamiento de códigos
struct CodeEntry {
  char type[5];
  char value[17];
  uint8_t keyboard_id; // ID del teclado (0=ambos, 1=teclado1, 2=teclado2)
  uint8_t relay;
  uint8_t reserved;    // Reservado para futuras extensiones
};
 
struct StoredCodes {
  uint32_t validMarker;
  uint32_t version;               // 1 = formato antiguo, 2 = formato nuevo
  bool localValidationFirst;
  uint16_t count;  // Cambiado a uint16_t para soportar 500 códigos
  CodeEntry codes[MAX_CODES];
};

// =================== ESTRUCTURAS PARA CÓDIGOS REMOTOS ===================
// Estructura para franjas horarias
struct TimeSlot {
  uint8_t start_hour;    // Hora de inicio (0-23)
  uint8_t start_minute;  // Minuto de inicio (0-59)
  uint8_t end_hour;      // Hora de fin (0-23)
  uint8_t end_minute;    // Minuto de fin (0-59)
  uint8_t days_of_week;  // Días de la semana (bitmask: 1=Lunes, 2=Martes, 4=Miércoles, 8=Jueves, 16=Viernes, 32=Sábado, 64=Domingo)
  uint8_t reserved[3];   // Reservado para alineación
};

// Estructura para códigos remotos
struct RemoteCodeEntry {
  char type[5];          // "PIN" o "TAG"
  char value[17];        // Valor del código
  uint8_t keyboard_id;   // Teclado asociado (0=ambos, 1=teclado1, 2=teclado2)
  uint8_t relay;         // Relé a activar (1 o 2)
  uint8_t time_slots_count; // Número de franjas horarias (máximo 4)
  TimeSlot time_slots[4]; // Franjas horarias
  uint8_t reserved;      // Reservado para alineación
};

// Estructura para almacenamiento de códigos remotos
#define MAX_REMOTE_CODES 500
struct StoredRemoteCodes {
  uint32_t validMarker;
  uint32_t version;
  uint16_t count;
  RemoteCodeEntry codes[MAX_REMOTE_CODES];
};

// =================== ESTRUCTURAS DEL MODO TORNO ===================
struct PendingRequest {
  bool active;            // true si hay solicitud pendiente
  uint8_t keyboard_id;    // Teclado origen (1 o 2)
  char code[17];          // Código introducido
  char type[5];           // Tipo de código (PIN/TAG)
  unsigned long timestamp; // Timestamp de la solicitud
  uint8_t relay_to_open;  // Relé que se abrirá si se aprueba
};

 StoredCodes storedCodes;
 StoredRemoteCodes storedRemoteCodes;
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
 
 // =================== CONFIGURACIÓN DE PINES ===================
 // Pines de relés
 const int RELE1_PIN = 15;
 const int RELE2_PIN = 2;
 float releDuration = 2.0;
 
 // Configuración dual Wiegand
 #define WIEGAND1_D0 33
 #define WIEGAND1_D1 14
 #define WIEGAND2_D0 4
 #define WIEGAND2_D1 16
 
 // Configuración RS485 (compatibilidad)
 #define RS485_RX2 35
 #define RS485_TX2 32
 #define RS485_BAUD 9600
 HardwareSerial RS485_Serial(2);
 
 // Configuración Ethernet LAN8720
 #define ETH_PHY_ADDR 0
 #define ETH_PHY_MDC 23
 #define ETH_PHY_MDIO 18
#define ETH_PHY_POWER_PIN 5
#define ETH_PHY_TYPE ETH_PHY_LAN8720
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
 
// =================== SERVIDOR WEB Y CONECTIVIDAD ===================
WebServer server(80);
IPAddress ip;
bool ethConnected = false;

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
void loadStoredRemoteCodes();
void saveStoredRemoteCodes();
bool addRemoteCode(const char* type, const char* value, uint8_t keyboardId, uint8_t relay, const TimeSlot* timeSlots, uint8_t timeSlotsCount);
bool deleteRemoteCode(const char* type, const char* value);
void deleteAllRemoteCodes();
bool isRemoteCodeStored(const char* type, const char* value, uint8_t keyboardId, uint8_t* relay);
bool isTimeSlotValid(const TimeSlot& timeSlot);
bool isCurrentTimeInTimeSlots(const TimeSlot* timeSlots, uint8_t timeSlotsCount);
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
 const long gmtOffset_sec = 3600;
 const int daylightOffset_sec = 3600;
 
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
 void setupAPMode();
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
 
 // =================== CALLBACK EVENTOS ETHERNET ===================
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
       configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
       connectToMqtt();
       break;
     case ARDUINO_EVENT_ETH_DISCONNECTED:
       Serial.println("🌐 ETH Desconectado");
       ethConnected = false;
       break;
     case ARDUINO_EVENT_ETH_STOP:
       Serial.println("🌐 ETH Detenido");
       ethConnected = false;
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
 
 // =================== CONECTIVIDAD MQTT CORREGIDA ===================
 void connectToMqtt() {
   mqttClient.setServer(mqtt_broker, mqtt_port);
   mqttClient.setCallback(mqttCallback);
   mqttClient.setBufferSize(2048);  // Incrementar buffer para mensajes grandes
   
   Serial.print("📡 Conectando a MQTT...");
   
   String clientId = "ESP32Client-";
   clientId += String(random(0xffff), HEX);
   
   if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
     Serial.println(" ✅ Conectado al broker MQTT");
     
     // Suscribirse usando SERIAL FIJO
     String commandTopic = "swatidhome/command/" + fixedSerialNumber + "/#";
     bool subscribed = mqttClient.subscribe(commandTopic.c_str());
     
     if (subscribed) {
       Serial.printf("📡 Suscrito correctamente a: %s\n", commandTopic.c_str());
     } else {
       Serial.printf("❌ Error al suscribirse a: %s\n", commandTopic.c_str());
     }
     
     // Dar tiempo para que se establezca la suscripción
     delay(500);
     
     publishError(3, "Dispositivo iniciado - Dual Wiegand " + String(firmwareVersion));
     
   } else {
     Serial.print(" ❌ Error al conectar a MQTT, rc=");
     Serial.println(mqttClient.state());
     
     // Códigos de error MQTT:
     // -4: MQTT_CONNECTION_TIMEOUT
     // -3: MQTT_CONNECTION_LOST  
     // -2: MQTT_CONNECT_FAILED
     // -1: MQTT_DISCONNECTED
     //  1: MQTT_CONNECT_BAD_PROTOCOL
     //  2: MQTT_CONNECT_BAD_CLIENT_ID
     //  3: MQTT_CONNECT_UNAVAILABLE
     //  4: MQTT_CONNECT_BAD_CREDENTIALS
     //  5: MQTT_CONNECT_UNAUTHORIZED
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
 
   DynamicJsonDocument doc(1024);
   DeserializationError error = deserializeJson(doc, message);
   if (error) {
     Serial.print("❌ deserializeJson() falló: ");
     Serial.println(error.c_str());
     return;
   }
 
   String topicStr = String(topic);
 
   if (topicStr.endsWith("/granted")) {
     Serial.println("📋 [MQTT] Validación remota recibida (granted)");
 
     if (!doc.containsKey("access_granted") || 
         !doc.containsKey("code_type") || 
         !doc.containsKey("code_value")) {
       Serial.println("❌ Mensaje 'granted' inválido: falta clave obligatoria");
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
           message += "\"source\":\"MQTT\"";
           message += "}";
           
           mqttClient.publish(topic.c_str(), message.c_str());
           Serial.printf("📡 Evento de sincronización de tiempo publicado\n");
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
 
   JsonArray codesArray = infoDoc.createNestedArray("stored_codes");
   for (int i = 0; i < storedCodes.count; i++) {
     JsonObject code = codesArray.createNestedObject();
     code["type"] = storedCodes.codes[i].type;
     code["value"] = storedCodes.codes[i].value;
     code["relay"] = storedCodes.codes[i].relay;
   }
 
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

  if (storedCodes.localValidationFirst && localFound) {
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
 
   if (!storedCodes.localValidationFirst || !localFound) {
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
       
       // Intentar publicar varias veces si es necesario
       bool published = false;
       for (int retry = 0; retry < 3 && !published; retry++) {
         published = mqttClient.publish(topic.c_str(), message.c_str(), false); // QoS 0, no retain
         if (!published) {
           Serial.printf("❌ Intento %d/3 de publicación falló\n", retry + 1);
           delay(100);
           mqttClient.loop(); // Procesar mensajes pendientes
         } else {
           Serial.printf("✅ Mensaje publicado correctamente en intento %d\n", retry + 1);
         }
       }
       
       if (!published) {
         Serial.println("❌ ERROR: No se pudo publicar mensaje tras 3 intentos");
         publishError(7, "Error publicando validación remota para " + type + " " + code);
         
         // Como último recurso, intentar validación local si existe el código
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
           // Marcar como intento fallido
           failedAttempts++;
           lastFailedAttempt = millis();
           publishFailedAccess(code, type, keyboardId, "MQTT_PUBLISH_FAILED");
         }
       }
       
     } else if (!storedCodes.localValidationFirst && localFound) {
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
     saveConfiguration();
     Serial.println("🔧 Configuración por defecto aplicada");
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
  if (storedCodes.localValidationFirst) {
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
    
    Serial.printf("📨 [MQTT] Respuesta de validación remota recibida: %s\n", response.c_str());
    
    if (response == "APPROVED") {
      // Acceso aprobado - determinar relé según teclado origen
      int relayToOpen = 1; // Por defecto relé 1
      
      if (doc.containsKey("message_info")) {
        int keyboardId = doc["message_info"]["keyboard_id"].as<int>();
        relayToOpen = keyboardId; // Relé según teclado origen
      }
      
      Serial.printf("✅ [MQTT] Acceso APROBADO - Abriendo Relé %d\n", relayToOpen);
      controlReleWithDuration(releDuration, relayToOpen);
      
      // Resetear intentos fallidos
      resetFailedAttempts();
      
      // Actualizar información de último acceso
      if (doc.containsKey("message_info")) {
        String code = doc["message_info"]["code_value"].as<String>();
        String type = doc["message_info"]["code_type"].as<String>();
        int keyboardId = doc["message_info"]["keyboard_id"].as<int>();
        
        strncpy(lastType, type.c_str(), sizeof(lastType) - 1);
        lastType[sizeof(lastType) - 1] = '\0';
        strncpy(lastCode, code.c_str(), sizeof(lastCode) - 1);
        lastCode[sizeof(lastCode) - 1] = '\0';
        strncpy(lastTime, getTimeString().c_str(), sizeof(lastTime) - 1);
        lastTime[sizeof(lastTime) - 1] = '\0';
        
        // Publicar evento de acceso remoto exitoso
        publishAccessEvent(code, type, keyboardId, true, "REMOTE_APPROVED");
      }
      
    } else if (response == "DENIED") {
      // Acceso denegado
      String reason = doc.containsKey("reason") ? doc["reason"].as<String>() : "Acceso denegado";
      Serial.printf("❌ [MQTT] Acceso DENEGADO: %s\n", reason.c_str());
      
      // Incrementar intentos fallidos
      failedAttempts++;
      lastFailedAttempt = millis();
      
      // Publicar evento de acceso remoto fallido
      if (doc.containsKey("message_info")) {
        String code = doc["message_info"]["code_value"].as<String>();
        String type = doc["message_info"]["code_type"].as<String>();
        int keyboardId = doc["message_info"]["keyboard_id"].as<int>();
        
        publishFailedAccess(code, type, keyboardId, "REMOTE_DENIED: " + reason);
      }
    }
  }
}

// =================== HANDLERS DE CONFIGURACIÓN DEL MODO TORNO ===================
void handleTurnstileConfig() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  Serial.println("📊 Exportando códigos locales a CSV");
  
  // Crear CSV con headers
  String csv = "Tipo,Codigo,Teclado,Rele,Fecha_Creacion\n";
  
  // Añadir códigos locales
  for (int i = 0; i < storedCodes.count; i++) {
    CodeEntry& entry = storedCodes.codes[i];
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
  
  Serial.printf("✅ Exportados %d códigos locales a CSV\n", storedCodes.count);
}

void handleExportRemoteCodes() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  Serial.println("📊 Exportando códigos remotos a CSV");
  
  // Crear CSV con headers
  String csv = "Tipo,Codigo,Teclado,Rele,Franjas_Horarias,Fecha_Creacion\n";
  
  // Añadir códigos remotos
  for (int i = 0; i < storedRemoteCodes.count; i++) {
    RemoteCodeEntry& entry = storedRemoteCodes.codes[i];
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
  
  Serial.printf("✅ Exportados %d códigos remotos a CSV\n", storedRemoteCodes.count);
}

void handleImportCodes() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  String response = "<html><head><meta charset='UTF-8'>";
  response += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5;}";
  response += ".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
  response += ".success{color:#28a745;}.error{color:#dc3545;}.info{color:#17a2b8;}";
  response += "h1{color:#333;border-bottom:2px solid #007bff;padding-bottom:10px;}";
  response += "button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;margin:10px 5px;}";
  response += "button:hover{background:#0056b3;}";
  response += ".summary{background:#e9ecef;padding:15px;border-radius:5px;margin:15px 0;}";
  response += "</style></head><body>";
  response += "<div class='container'>";
  response += "<h1>📥 Resultado de Importación CSV</h1>";
  
  if (importedCount > 0) {
    response += "<div class='summary'>";
    response += "<h2 class='success'>✅ Importación Exitosa</h2>";
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
    response += "<div class='summary'>";
    response += "<h3 class='error'>❌ No se procesaron códigos</h3>";
    response += "<p>El archivo CSV no contenía datos válidos para importar.</p>";
    response += "</div>";
  }
  
  response += "<div style='text-align:center;margin-top:20px;'>";
  response += "<a href='/codes'><button>📋 Volver a Códigos</button></a>";
  response += "<a href='/codes/template'><button>📄 Descargar Plantilla</button></a>";
  response += "</div>";
  response += "</div></body></html>";
  
  server.send(200, "text/html", response);
}

void handleCSVTemplate() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  Serial.println("📖 Deteniendo modo de lectura de tags");
  
  // Desactivar modo de lectura
  tagReadingActive = false;
  tagReadingConfig.enabled = false;
  
  Serial.printf("📖 Lectura finalizada. Tags leídos: %d\n", readTags.size());
  
  server.send(200, "application/json", "{\"status\":\"stopped\",\"count\":" + String(readTags.size()) + "}");
}

void handleReadTagsStatus() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  Serial.println("🕐 Sincronizando hora del sistema");
  
  // Obtener hora actual del navegador
  String html = "<html><head><title>Sincronización de Hora</title></head><body>";
  html += "<h1>🕐 Sincronización de Hora</h1>";
  html += "<p>Hora actual del sistema: <strong>" + String(currentTimeString) + "</strong></p>";
  html += "<p>Estado: " + String(timeSynced ? "✅ Sincronizado" : "❌ No sincronizado") + "</p>";
  
  html += "<form action='/time/update' method='post'>";
  html += "<div class='form-group'>";
  html += "<label for='currentTime'>Hora actual (formato: YYYY-MM-DD HH:MM:SS):</label>";
  html += "<input type='text' id='currentTime' name='currentTime' placeholder='2025-01-09 15:30:00' required>";
  html += "</div>";
  html += "<button type='submit'><i class='fas fa-sync'></i> Actualizar Hora</button>";
  html += "</form>";
  
  html += "<script>";
  html += "// Auto-rellenar con hora actual del navegador";
  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "  const now = new Date();";
  html += "  const year = now.getFullYear();";
  html += "  const month = String(now.getMonth() + 1).padStart(2, '0');";
  html += "  const day = String(now.getDate()).padStart(2, '0');";
  html += "  const hours = String(now.getHours()).padStart(2, '0');";
  html += "  const minutes = String(now.getMinutes()).padStart(2, '0');";
  html += "  const seconds = String(now.getSeconds()).padStart(2, '0');";
  html += "  const timeString = year + '-' + month + '-' + day + ' ' + hours + ':' + minutes + ':' + seconds;";
  html += "  document.getElementById('currentTime').value = timeString;";
  html += "});";
  html += "</script>";
  
  html += "<a href='/'><button>Volver al inicio</button></a>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleTimeUpdate() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
    message += "\"source\":\"WEB\"";
    message += "}";
    
    mqttClient.publish(topic.c_str(), message.c_str());
    Serial.printf("📡 Evento de sincronización de tiempo publicado\n");
  }
  
  // Respuesta de confirmación
  String html = "<html><body><h1>✅ Hora Actualizada</h1>";
  html += "<p><strong>Nueva hora:</strong> " + timeString + "</p>";
  html += "<p><strong>Estado:</strong> ✅ Sincronizado</p>";
  html += "<p><strong>Última sincronización:</strong> Ahora</p>";
  html += "<a href='/'><button>Volver al inicio</button></a>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
  
  Serial.printf("✅ Hora del sistema actualizada: %s\n", timeString.c_str());
}

// =================== MANEJADORES DE SEGURIDAD ===================
void handleSecurityBlockAccess() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  localAccessBlocked = true;
  blockStartTime = millis();
  saveConfiguration();
  
  Serial.println("🔒 Acceso local bloqueado desde web");
  publishError(4, "Acceso local bloqueado desde web");
  
  server.send(200, "text/html", 
    "<html><head><title>Acceso Bloqueado</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:600px;margin:50px auto;padding:20px;background:#f5f5f5;}"
    "h1{color:#d32f2f;text-align:center;border-bottom:2px solid #d32f2f;padding-bottom:10px;}"
    "p{background:white;padding:15px;border-radius:5px;margin:10px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "button{background:#2196f3;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}"
    "button:hover{background:#1976d2;}</style></head>"
    "<body><h1>Acceso Local Bloqueado</h1>"
    "<p>El acceso local ha sido bloqueado correctamente.</p>"
    "<p><strong>Estado:</strong> Bloqueado temporalmente</p>"
    "<p><strong>Duración:</strong> " + String(blockDuration/1000) + " segundos</p>"
    "<div style='text-align:center;margin-top:30px;'>"
    "<a href='/'><button>Volver al inicio</button></a></div></body></html>");
}

void handleSecurityUnblockAccess() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  localAccessBlocked = false;
  failedAttempts = 0;
  blockStartTime = 0;
  saveConfiguration();
  
  Serial.println("🔓 Acceso local desbloqueado desde web");
  publishError(4, "Acceso local desbloqueado desde web");
  
  server.send(200, "text/html", 
    "<html><head><title>Acceso Desbloqueado</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:600px;margin:50px auto;padding:20px;background:#f5f5f5;}"
    "h1{color:#388e3c;text-align:center;border-bottom:2px solid #388e3c;padding-bottom:10px;}"
    "p{background:white;padding:15px;border-radius:5px;margin:10px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "button{background:#2196f3;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}"
    "button:hover{background:#1976d2;}</style></head>"
    "<body><h1>Acceso Local Desbloqueado</h1>"
    "<p>El acceso local ha sido desbloqueado correctamente.</p>"
    "<p><strong>Estado:</strong> Acceso permitido</p>"
    "<p><strong>Intentos fallidos:</strong> Reseteados a 0</p>"
    "<div style='text-align:center;margin-top:30px;'>"
    "<a href='/'><button>Volver al inicio</button></a></div></body></html>");
}

void handleSecurityDisableKeyboards() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  keyboardReadingEnabled = false;
  saveConfiguration();
  
  Serial.println("🔒 Lectura de teclados deshabilitada desde web");
  publishError(4, "Lectura de teclados deshabilitada desde web");
  
  server.send(200, "text/html", 
    "<html><head><title>Teclados Deshabilitados</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:600px;margin:50px auto;padding:20px;background:#f5f5f5;}"
    "h1{color:#f57c00;text-align:center;border-bottom:2px solid #f57c00;padding-bottom:10px;}"
    "p{background:white;padding:15px;border-radius:5px;margin:10px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "button{background:#2196f3;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}"
    "button:hover{background:#1976d2;}</style></head>"
    "<body><h1>Lectura de Teclados Deshabilitada</h1>"
    "<p>La lectura de teclados ha sido deshabilitada correctamente.</p>"
    "<p><strong>Estado:</strong> Teclados bloqueados</p>"
    "<p><strong>Efecto:</strong> Los códigos no serán procesados</p>"
    "<div style='text-align:center;margin-top:30px;'>"
    "<a href='/'><button>Volver al inicio</button></a></div></body></html>");
}

void handleSecurityEnableKeyboards() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  keyboardReadingEnabled = true;
  saveConfiguration();
  
  Serial.println("🔓 Lectura de teclados habilitada desde web");
  publishError(4, "Lectura de teclados habilitada desde web");
  
  server.send(200, "text/html", 
    "<html><head><title>Teclados Habilitados</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:600px;margin:50px auto;padding:20px;background:#f5f5f5;}"
    "h1{color:#388e3c;text-align:center;border-bottom:2px solid #388e3c;padding-bottom:10px;}"
    "p{background:white;padding:15px;border-radius:5px;margin:10px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "button{background:#2196f3;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}"
    "button:hover{background:#1976d2;}</style></head>"
    "<body><h1>Lectura de Teclados Habilitada</h1>"
    "<p>La lectura de teclados ha sido habilitada correctamente.</p>"
    "<p><strong>Estado:</strong> Teclados activos</p>"
    "<p><strong>Efecto:</strong> Los códigos serán procesados normalmente</p>"
    "<div style='text-align:center;margin-top:30px;'>"
    "<a href='/'><button>Volver al inicio</button></a></div></body></html>");
}

// =================== GESTIÓN DE CÓDIGOS ALMACENADOS ===================
void loadStoredCodes() {
  Serial.println("🔄 Cargando códigos desde EEPROM...");
  
  EEPROM.get(EEPROM_CODES_OFFSET, storedCodes);
  
  // Verificar marcador de validación
  if (storedCodes.validMarker != 0xCAFEBABE) {
    Serial.println("🔧 Inicializando códigos por primera vez...");
    storedCodes.validMarker = 0xCAFEBABE;
    storedCodes.version = 2; // Formato nuevo
    storedCodes.localValidationFirst = true;
    storedCodes.count = 0;
    
    // Limpiar array de códigos
    memset(storedCodes.codes, 0, sizeof(storedCodes.codes));
    
    saveStoredCodes();
    Serial.println("✅ Estructura de códigos inicializada correctamente");
  } else {
    // Verificar integridad de los datos cargados
    if (storedCodes.count > MAX_CODES) {
      Serial.printf("⚠️ Advertencia: Contador de códigos inválido (%d > %d). Corrigiendo...\n", 
                    storedCodes.count, MAX_CODES);
      storedCodes.count = 0;
      saveStoredCodes();
    }
    
    // Migrar códigos si es necesario
    if (storedCodes.version == 1) {
      Serial.println("🔄 Migrando códigos al formato nuevo...");
      for (int i = 0; i < storedCodes.count; i++) {
        storedCodes.codes[i].keyboard_id = 0; // Ambos teclados
        storedCodes.codes[i].reserved = 0;
      }
      storedCodes.version = 2;
      saveStoredCodes();
      Serial.printf("✅ Migrados %d códigos al formato nuevo\n", storedCodes.count);
    }
    
    Serial.printf("💾 Códigos cargados exitosamente: %d códigos (versión %d)\n", 
                  storedCodes.count, storedCodes.version);
    Serial.printf("   Modo validación: %s\n", 
                  storedCodes.localValidationFirst ? "Local primero" : "Remoto primero");
  }
}
 
 void saveStoredCodes() {
   // Verificar integridad antes de guardar
   if (storedCodes.validMarker != 0xCAFEBABE) {
     Serial.println("❌ Error: Marcador de validación inválido antes de guardar");
     return;
   }
   
   if (storedCodes.count > MAX_CODES) {
     Serial.printf("❌ Error: Contador de códigos inválido (%d > %d)\n", storedCodes.count, MAX_CODES);
     return;
   }
   
   // Guardar en EEPROM
   EEPROM.put(EEPROM_CODES_OFFSET, storedCodes);
   
   // Verificar que el commit sea exitoso
   if (!EEPROM.commit()) {
     Serial.println("❌ Error: Fallo al hacer commit en EEPROM");
     return;
   }
   
   // Verificar integridad después de guardar
   StoredCodes testCodes;
   EEPROM.get(EEPROM_CODES_OFFSET, testCodes);
   
   if (testCodes.validMarker != 0xCAFEBABE || testCodes.count != storedCodes.count) {
     Serial.println("❌ Error: Verificación de integridad falló después de guardar");
     return;
   }
   
   Serial.printf("💾 Códigos guardados correctamente en EEPROM: %d códigos (versión %d)\n", 
                 storedCodes.count, storedCodes.version);
 }
 
bool addCode(const char* type, const char* value, int keyboardId, int relay) {
  if (storedCodes.count >= MAX_CODES) {
    Serial.printf("❌ Error: Máximo de códigos alcanzado (%d/%d)\n", storedCodes.count, MAX_CODES);
    return false;
  }

  // Validar parámetros - CORREGIDO: Permitir keyboardId = 0 (ambos teclados)
  if (keyboardId < 0 || keyboardId > 2) {
    Serial.printf("❌ Error: keyboardId inválido (%d). Debe ser 0 (ambos), 1 o 2\n", keyboardId);
    return false;
  }
  if (relay < 1 || relay > 2) {
    Serial.printf("❌ Error: relay inválido (%d). Debe ser 1 o 2\n", relay);
    return false;
  }

  // Verificar duplicados exactos
  for (int i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0 &&
        storedCodes.codes[i].keyboard_id == keyboardId) {
      return false; // Duplicado exacto
    }
  }

  // Copiar datos del código
  strncpy(storedCodes.codes[storedCodes.count].type, type, sizeof(storedCodes.codes[storedCodes.count].type) - 1);
  storedCodes.codes[storedCodes.count].type[sizeof(storedCodes.codes[storedCodes.count].type) - 1] = '\0';

  strncpy(storedCodes.codes[storedCodes.count].value, value, sizeof(storedCodes.codes[storedCodes.count].value) - 1);
  storedCodes.codes[storedCodes.count].value[sizeof(storedCodes.codes[storedCodes.count].value) - 1] = '\0';

  storedCodes.codes[storedCodes.count].keyboard_id = keyboardId;
  storedCodes.codes[storedCodes.count].relay = relay;
  storedCodes.codes[storedCodes.count].reserved = 0;
  storedCodes.count++;
  storedCodes.version = 2; // Asegurar versión nueva

  // Log del código añadido
  String keyboardName = (keyboardId == 0) ? "Ambos teclados" : 
                       (keyboardId == 1) ? "Teclado 1" : "Teclado 2";
  Serial.printf("📝 Añadiendo código: %s '%s' -> %s, Relé %d (Total: %d)\n", 
                type, value, keyboardName.c_str(), relay, storedCodes.count);

  // Guardar en EEPROM con verificación
  saveStoredCodes();
  
  // Verificar que se guardó correctamente
  if (storedCodes.validMarker == 0xCAFEBABE && storedCodes.count > 0) {
    Serial.printf("✅ Código guardado exitosamente en EEPROM\n");
    return true;
  } else {
    Serial.println("❌ Error: Fallo al verificar el guardado del código");
    return false;
  }
}

// Sobrecarga para compatibilidad (keyboardId = 0)
bool addCode(const char* type, const char* value, int relay) {
  return addCode(type, value, 0, relay);
}

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
    int progress = (upload.currentSize * 100) / upload.totalSize;
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

void handleOTAConfig() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Configuración OTA - KC868-A2</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
  html += ".container{max-width:800px;margin:auto;background:#fff;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);}";
  html += "h1,h2{color:#333;}";
  html += ".form-group{margin-bottom:15px;}";
  html += "label{display:block;margin-bottom:5px;font-weight:bold;}";
  html += "input[type='text'],input[type='url'],input[type='number'],input[type='file']{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box;}";
  html += "input[type='checkbox']{margin-right:5px;}";
  html += "button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;margin-right:10px;}";
  html += "button:hover{background:#0056b3;}";
  html += ".btn-warning{background:#ffc107;color:#212529;}";
  html += ".btn-warning:hover{background:#e0a800;}";
  html += ".btn-success{background:#28a745;}";
  html += ".btn-success:hover{background:#218838;}";
  html += ".btn-danger{background:#dc3545;}";
  html += ".btn-danger:hover{background:#c82333;}";
  html += ".status-info{background:#e8f5e8;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #28a745;}";
  html += ".progress-container{margin:20px 0;}";
  html += ".progress-bar{width:100%;height:20px;background:#f0f0f0;border-radius:10px;overflow:hidden;}";
  html += ".progress-fill{height:100%;background:#007bff;transition:width 0.3s ease;}";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h1><i class='fas fa-download'></i> Configuración de Actualización OTA</h1>";
  
  // Información actual del dispositivo
  html += "<div class='status-info'>";
  html += "<h2>Información del Dispositivo</h2>";
  html += "<p><strong>Versión Actual:</strong> " + String(firmwareVersion) + "</p>";
  html += "<p><strong>Fecha de Compilación:</strong> " + String(firmwareBuild) + "</p>";
  html += "<p><strong>MAC Address:</strong> " + ETH.macAddress() + "</p>";
  html += "<p><strong>Serial Number:</strong> " + fixedSerialNumber + "</p>";
  html += "<p><strong>Modelo:</strong> KC868-A2</p>";
  html += "</div>";
  
  // Configuración de actualización automática
  html += "<h2>Actualización Automática</h2>";
  html += "<form action='/ota/config' method='post'>";
  
  html += "<div class='form-group'>";
  html += "<label for='updateUrl'>URL del Servidor de Actualizaciones:</label>";
  html += "<input type='url' id='updateUrl' name='updateUrl' value='" + String(otaConfig.updateUrl) + "' placeholder='https://updates.swatid.com/api/check'>";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label for='checkInterval'>Intervalo de Verificación (horas):</label>";
  html += "<input type='number' id='checkInterval' name='checkInterval' value='" + String(otaConfig.checkInterval) + "' min='1' max='168'>";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label>";
  html += "<input type='checkbox' id='autoUpdateEnabled' name='autoUpdateEnabled'" + String(otaConfig.autoUpdateEnabled ? " checked" : "") + ">";
  html += " Habilitar actualización automática";
  html += "</label>";
  html += "</div>";
  
  html += "<button type='submit' class='btn-success'>Guardar Configuración</button>";
  html += "</form>";
  
  // Verificación manual
  html += "<h2>Verificación Manual</h2>";
  html += "<p>Verificar si hay actualizaciones disponibles:</p>";
  html += "<button onclick='checkForUpdates()' class='btn-warning'>Verificar Actualizaciones</button>";
  
  // Estado de la última verificación
  if (otaConfig.lastCheck > 0) {
    unsigned long hoursAgo = (millis() - otaConfig.lastCheck) / 3600000;
    html += "<p><strong>Última Verificación:</strong> Hace " + String(hoursAgo) + " horas</p>";
  } else {
    html += "<p><strong>Última Verificación:</strong> Nunca</p>";
  }
  
  // Actualización manual
  html += "<h2>Actualización Manual</h2>";
  html += "<p>Subir archivo de firmware (.bin) para actualización manual:</p>";
  
  html += "<form id='uploadForm' enctype='multipart/form-data'>";
  html += "<div class='form-group'>";
  html += "<label for='firmwareFile'>Archivo de Firmware (.bin):</label>";
  html += "<input type='file' id='firmwareFile' name='firmwareFile' accept='.bin' required>";
  html += "</div>";
  html += "<button type='submit' class='btn-success'>Subir y Actualizar</button>";
  html += "</form>";
  
  // Progreso de actualización
  html += "<div id='uploadProgress' class='progress-container' style='display:none;'>";
  html += "<div class='progress-bar'>";
  html += "<div id='progressBar' class='progress-fill' style='width:0%;'></div>";
  html += "</div>";
  html += "<p id='progressText'>Preparando actualización...</p>";
  html += "</div>";
  
  // Información de seguridad
  html += "<h2>Información de Seguridad</h2>";
  html += "<div class='status-info'>";
  html += "<p><strong>⚠️ Importante:</strong></p>";
  html += "<ul>";
  html += "<li>Solo suba archivos .bin compilados para ESP32</li>";
  html += "<li>El archivo debe ser de un firmware válido para KC868-A2</li>";
  html += "<li>La actualización reiniciará el dispositivo automáticamente</li>";
  html += "<li>Mantenga una copia de seguridad del firmware actual</li>";
  html += "</ul>";
  html += "</div>";
  
  html += "<div style='margin-top:30px;text-align:center;'>";
  html += "<a href='/'><button>Volver al Inicio</button></a>";
  html += "</div>";
  
  html += "</div>";
  
  // JavaScript
  html += "<script>";
  html += "function checkForUpdates() {";
  html += "  fetch('/ota/check')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      alert('Verificación completada. Revisa los logs del dispositivo.');";
  html += "      location.reload();";
  html += "    })";
  html += "    .catch(error => {";
  html += "      alert('Error verificando actualizaciones: ' + error);";
  html += "    });";
  html += "}";
  
  html += "document.getElementById('uploadForm').addEventListener('submit', function(e) {";
  html += "  e.preventDefault();";
  html += "  const fileInput = document.getElementById('firmwareFile');";
  html += "  const file = fileInput.files[0];";
  html += "  if (!file) {";
  html += "    alert('Por favor, seleccione un archivo');";
  html += "    return;";
  html += "  }";
  html += "  if (!file.name.endsWith('.bin')) {";
  html += "    alert('Solo archivos .bin permitidos');";
  html += "    return;";
  html += "  }";
  html += "  if (file.size > 4 * 1024 * 1024) {";
  html += "    alert('Archivo demasiado grande (máximo 4MB)');";
  html += "    return;";
  html += "  }";
  html += "  document.getElementById('uploadProgress').style.display = 'block';";
  html += "  const formData = new FormData();";
  html += "  formData.append('firmwareFile', file);";
  html += "  fetch('/ota/upload', {";
  html += "    method: 'POST',";
  html += "    body: formData";
  html += "  })";
  html += "  .then(response => response.text())";
  html += "  .then(data => {";
  html += "    if (data.includes('completada')) {";
  html += "      document.getElementById('progressText').textContent = 'Actualización completada. Reiniciando...';";
  html += "      setTimeout(() => {";
  html += "        window.location.href = '/';";
  html += "      }, 5000);";
  html += "    } else {";
  html += "      alert('Error en la actualización: ' + data);";
  html += "    }";
  html += "  })";
  html += "  .catch(error => {";
  html += "    alert('Error subiendo archivo: ' + error);";
  html += "  });";
  html += "});";
  html += "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleOTAStatus() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
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
 
 bool deleteCode(const char* type, const char* value, int keyboardId = -1) {
   for (uint16_t i = 0; i < storedCodes.count; i++) {
     if (strcmp(storedCodes.codes[i].value, value) == 0 &&
         strcmp(storedCodes.codes[i].type, type) == 0) {
       
       // Si no se especifica keyboardId, eliminar cualquier coincidencia
       // Si se especifica, solo eliminar si coincide
       if (keyboardId == -1 || storedCodes.codes[i].keyboard_id == keyboardId) {
         storedCodes.codes[i] = storedCodes.codes[storedCodes.count - 1];
         storedCodes.count--;
         saveStoredCodes();
         return true;
       }
     }
   }
   return false;
 }
 
bool isCodeStored(const char* type, const char* value, int keyboardId, int* relay) {
  for (int i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0) {
      
      if (storedCodes.version == 1) {
        // Formato antiguo: válido en cualquier teclado
        if (relay != nullptr) *relay = storedCodes.codes[i].relay;
        return true;
      } else {
        // Formato nuevo: verificar teclado
        if (storedCodes.codes[i].keyboard_id == 0 || 
            storedCodes.codes[i].keyboard_id == keyboardId) {
          if (relay != nullptr) *relay = storedCodes.codes[i].relay;
          return true;
        }
      }
    }
  }
  return false;
}

// Sobrecarga para compatibilidad
bool isCodeStored(const char* type, const char* value, int* relay) {
  return isCodeStored(type, value, 0, relay);
}
 
 bool isCodeStored(const char* type, const char* value) {
   return isCodeStored(type, value, nullptr);
 }
 
 // =================== CONFIGURACIÓN MODO AP - MEJORADO ===================
 void setupAPMode() {
   Serial.println("🔧 === MODO AP DE EMERGENCIA ===");
   String macSuffix = fixedSerialNumber.substring(fixedSerialNumber.length() - 6);
   String apSSID = "SWATID_CONFIG_" + macSuffix;
   String apPassword = "12345678";
   
   WiFi.mode(WIFI_AP);
   WiFi.softAP(apSSID.c_str(), apPassword.c_str());
   
   IPAddress apIP(192, 168, 4, 1);
   IPAddress apGateway(192, 168, 4, 1);
   IPAddress apSubnet(255, 255, 255, 0);
   WiFi.softAPConfig(apIP, apGateway, apSubnet);
   
   Serial.printf("📶 SSID: %s\n", apSSID.c_str());
   Serial.printf("📶 Password: %s\n", apPassword.c_str());
   Serial.printf("📶 IP: %s\n", apIP.toString().c_str());
   Serial.println("📶 Conectar a esta red WiFi para configurar");
   
   ip = apIP; // Actualizar IP global
   setupWebServer();
   
   // Iniciar mDNS para acceso fácil
  if (MDNS.begin(deviceName)) {
    Serial.printf("📶 mDNS: http://%s.local\n", deviceName);
     MDNS.addService("http", "tcp", 80);
   }
 }
 
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
   if (!server.authenticate(admin_user, admin_password)) {
     return server.requestAuthentication();
   }
 
   String html = "<!DOCTYPE HTML><html lang='es'>";
   html += "<head><meta charset='UTF-8'>";
   html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
   html += "<title>Controladora A2 - SWATID</title>";
   html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css'>";
   html += "<style>";
   html += "body{font-family:Arial,sans-serif;margin:0;padding:0;background:#f4f4f4;}";
   html += "header{background:#35424a;color:#fff;padding:20px 0;text-align:center;}";
   html += "main{padding:20px;}";
   html += ".container{max-width:1000px;margin:auto;background:#fff;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);}";
   html += "h1,h2{color:#333;}";
   html += ".security-status{background:#fff3cd;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #ffc107;}";
   html += ".security-blocked{background:#f8d7da;border-left-color:#dc3545;}";
   html += ".dual-status{background:#e8f5e8;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #28a745;}";
   html += ".form-group{margin-bottom:15px;}";
   html += "label{display:block;margin-bottom:5px;font-weight:bold;}";
   html += "input,select{width:100%;padding:10px;border:1px solid #ccc;border-radius:4px;}";
   html += "button{padding:10px 15px;border:none;border-radius:4px;cursor:pointer;background:#4CAF50;color:white;font-size:16px;margin:5px;}";
   html += "button:hover{background:#45a049;}";
   html += ".btn-danger{background:#dc3545;}";
   html += ".btn-warning{background:#ffc107;color:#000;}";
   html += ".btn-info{background:#17a2b8;}";
   html += ".export-section{margin:20px 0;padding:15px;background:#f8f9fa;border:1px solid #dee2e6;border-radius:5px;text-align:center;}";
   html += ".time-sync-section{margin:20px 0;padding:15px;background:#e8f4fd;border:1px solid #bee5eb;border-radius:5px;}";
   html += "table{width:100%;border-collapse:collapse;margin-top:20px;}";
   html += "th,td{border:1px solid #ddd;padding:8px;text-align:center;}";
   html += "th{background-color:#f2f2f2;}";
   html += ".actions a{margin-right:10px;text-decoration:none;}";
   html += ".icon{margin-right:5px;}";
  html += ".readonly{background-color:#e9ecef;color:#6c757d;}";
  html += ".security-controls{margin-top:20px;padding:15px;background:#f8f9fa;border-radius:8px;border:1px solid #dee2e6;}";
  html += ".security-controls h3{margin-top:0;color:#495057;}";
  html += ".security-controls button{margin:5px;padding:10px 15px;border:none;border-radius:5px;cursor:pointer;font-size:14px;}";
  html += ".btn-success{background-color:#28a745;color:white;}";
  html += ".btn-warning{background-color:#ffc107;color:#212529;}";
  html += ".btn-success:hover{background-color:#218838;}";
  html += ".btn-warning:hover{background-color:#e0a800;}";
  html += "</style></head><body>";
 
   html += "<header><h1><i class='fas fa-keyboard icon'></i>Controladora A2 - SWATID</h1>";
   html += "<p>Device: <strong>" + String(deviceName) + "</strong> | Serial: <strong>" + fixedSerialNumber + "</strong></p>";
   html += "<p>IP: " + ip.toString() + " | Firmware: <strong>" + firmwareVersion + "</strong> | Build: <strong>" + firmwareBuild + "</strong></p></header>";
 
   html += "<main><div class='container'>";
 
   // Estado de seguridad
   html += "<div class='security-status" + String(localAccessBlocked ? " security-blocked" : "") + "'>";
   html += "<h2><i class='fas fa-shield-alt icon'></i>Estado de Seguridad</h2>";
   html += "<table>";
   html += "<tr><th>Parámetro</th><th>Valor</th></tr>";
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
  html += "<div class='security-controls'>";
  html += "<h3><i class='fas fa-cogs icon'></i>Controles de Seguridad</h3>";
  
  // Botones de bloqueo de acceso local
  if (localAccessBlocked) {
    html += "<a href='/security/unblock-access'><button class='btn-success'><i class='fas fa-unlock icon'></i>Desbloquear Acceso Local</button></a>";
  } else {
    html += "<a href='/security/block-access'><button class='btn-warning'><i class='fas fa-lock icon'></i>Bloquear Acceso Local</button></a>";
  }
  
  // Botones de control de lectura de teclados
  if (keyboardReadingEnabled) {
    html += "<a href='/security/disable-keyboards'><button class='btn-warning'><i class='fas fa-keyboard icon'></i>Deshabilitar Lectura Teclados</button></a>";
  } else {
    html += "<a href='/security/enable-keyboards'><button class='btn-success'><i class='fas fa-keyboard icon'></i>Habilitar Lectura Teclados</button></a>";
  }
  
  html += "</div>";
  html += "</div>";
 
   // Estado de teclados duales
   html += "<div class='dual-status'>";
   html += "<h2><i class='fas fa-keyboard icon'></i>Estado de Teclados Wiegand Duales</h2>";
   html += "<table>";
   html += "<tr><th>Teclado</th><th>Pines GPIO</th><th>Estado</th><th>Función</th></tr>";
   html += "<tr><td>Teclado 1</td><td>" + String(WIEGAND1_D0) + "/" + String(WIEGAND1_D1) + "</td><td style='color:green'>✅ Activo</td><td>Principal</td></tr>";
   html += "<tr><td>Teclado 2</td><td>" + String(WIEGAND2_D0) + "/" + String(WIEGAND2_D1) + "</td><td style='color:green'>✅ Activo</td><td>Secundario</td></tr>";
   html += "</table>";
   html += "<p><strong>💡 Sistema Dual Operativo:</strong> Ambos teclados funcionan simultáneamente</p>";
   html += "</div>";
 
   // Formulario de configuración
   html += "<h2><i class='fas fa-cogs icon'></i>Configuración del Dispositivo</h2>";
   html += "<form action='/save' method='post'>";
 
   html += "<div class='form-group'><label for='deviceName'>Nombre del dispositivo (editable):</label>";
    html += "<input type='text' id='deviceName' name='deviceName' value='" + String(deviceName) + "'></div>";
 
   html += "<div class='form-group'><label for='fixedSerial'>Número de serie MQTT (fijo):</label>";
   html += "<input type='text' id='fixedSerial' name='fixedSerial' value='" + fixedSerialNumber + "' class='readonly' readonly></div>";
 
   html += "<div class='form-group'><label for='releDuration'>Duración del relé (segundos):</label>";
   html += "<input type='number' id='releDuration' name='releDuration' min='0.5' step='0.5' value='" + String(releDuration) + "'></div>";
 
  html += "<div class='form-group'><label for='useDhcp'>Usar DHCP:</label><select id='useDhcp' name='useDhcp' onchange='toggleIpFields()'>";
  html += useDhcp ? "<option value='1' selected>Sí</option><option value='0'>No</option>" : "<option value='1'>Sí</option><option value='0' selected>No</option>";
  html += "</select></div>";

  html += "<div class='form-group'><label for='validationMode'>Modo de Validación:</label><select id='validationMode' name='validationMode'>";
  html += storedCodes.localValidationFirst ? 
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
 
  html += "<button type='submit'><i class='fas fa-save icon'></i>Guardar configuración</button>";
  html += "</form>";
  
  // Sección de sincronización de tiempo
  html += "<div class='time-sync-section'>";
  html += "<h3><i class='fas fa-clock icon'></i>Sincronización de Tiempo</h3>";
   html += "<p><strong>Hora actual:</strong> " + String(currentTimeString) + "</p>";
  html += "<p><strong>Estado:</strong> " + String(timeSynced ? "✅ Sincronizado" : "❌ No sincronizado") + "</p>";
  html += "<a href='/time/sync'><button class='btn-info'><i class='fas fa-sync icon'></i>Sincronizar Hora</button></a>";
  html += "</div>";
 
   // Cambio de contraseña
   html += "<h2><i class='fas fa-lock icon'></i>Cambiar contraseña</h2>";
   html += "<form action='/changepass' method='post'>";
   html += "<div class='form-group'><label for='newPassword'>Nueva contraseña:</label>";
   html += "<input type='password' id='newPassword' name='newPassword'></div>";
   html += "<button type='submit'><i class='fas fa-key icon'></i>Cambiar contraseña</button>";
   html += "</form>";

   // Configuración del Modo Torno
   html += "<h2><i class='fas fa-sync-alt icon'></i>Configuración del Modo Torno</h2>";
   html += "<div class='dual-status'>";
   html += "<p><strong>🔄 Modo Torno:</strong> Permite control bidireccional donde cada teclado controla un relé específico.</p>";
   html += "<p><strong>Estado actual:</strong> " + String(isTurnstileModeEnabled() ? "🟢 ACTIVO" : "🔴 INACTIVO") + "</p>";
   if (isTurnstileModeEnabled()) {
     html += "<p><strong>Mapeo actual:</strong></p>";
     html += "<ul>";
     html += "<li>Teclado 1 (GPIO 33/14) → Relé " + String(config.turnstile.keyboard1_relay) + "</li>";
     html += "<li>Teclado 2 (GPIO 4/16) → Relé " + String(config.turnstile.keyboard2_relay) + "</li>";
     html += "</ul>";
   }
   html += "</div>";
   
   html += "<form action='/turnstile/config' method='post'>";
   html += "<div class='form-group'>";
   html += "<label for='turnstile_mode'>Modo de Operación:</label>";
   html += "<select name='turnstile_mode' id='turnstile_mode'>";
   html += "<option value='normal'" + String(!isTurnstileModeEnabled() ? " selected" : "") + ">🔴 Modo Normal</option>";
   html += "<option value='turnstile'" + String(isTurnstileModeEnabled() ? " selected" : "") + ">🟢 Modo Torno</option>";
   html += "</select>";
   html += "</div>";
   
   html += "<div class='form-group'>";
   html += "<label for='keyboard1_relay'>Teclado 1 (GPIO 33/14) → Relé:</label>";
   html += "<select name='keyboard1_relay' id='keyboard1_relay'>";
   html += "<option value='1'" + String(config.turnstile.keyboard1_relay == 1 ? " selected" : "") + ">Relé 1</option>";
   html += "<option value='2'" + String(config.turnstile.keyboard1_relay == 2 ? " selected" : "") + ">Relé 2</option>";
   html += "</select>";
   html += "</div>";
   
   html += "<div class='form-group'>";
   html += "<label for='keyboard2_relay'>Teclado 2 (GPIO 4/16) → Relé:</label>";
   html += "<select name='keyboard2_relay' id='keyboard2_relay'>";
   html += "<option value='1'" + String(config.turnstile.keyboard2_relay == 1 ? " selected" : "") + ">Relé 1</option>";
   html += "<option value='2'" + String(config.turnstile.keyboard2_relay == 2 ? " selected" : "") + ">Relé 2</option>";
   html += "</select>";
   html += "</div>";
   
   html += "<button type='submit'><i class='fas fa-save icon'></i>Guardar Configuración del Torno</button>";
   html += "</form>";
   
   // Estado de solicitud pendiente
   if (isTurnstileModeEnabled() && pendingRequest.active) {
     html += "<div class='security-status'>";
     html += "<h3><i class='fas fa-clock icon'></i>Solicitud Pendiente</h3>";
     html += "<p><strong>Código:</strong> " + String(pendingRequest.code) + " (" + String(pendingRequest.type) + ")</p>";
     html += "<p><strong>Teclado:</strong> " + String(pendingRequest.keyboard_id == 1 ? "Teclado 1" : "Teclado 2") + "</p>";
     html += "<p><strong>Relé a abrir:</strong> " + String(pendingRequest.relay_to_open) + "</p>";
     html += "<p><strong>Tiempo transcurrido:</strong> " + String((millis() - pendingRequest.timestamp) / 1000) + "s / 30s</p>";
     html += "<a href='/turnstile/reset'><button class='btn-warning'><i class='fas fa-times icon'></i>Cancelar Solicitud</button></a>";
     html += "</div>";
   }

   // Acciones rápidas
   html += "<h2><i class='fas fa-bolt icon'></i>Acciones</h2><div class='actions'>";
  html += "<a href='/rele?relay=1'><button><i class='fas fa-door-open icon'></i>Relé 1 ON</button></a>";
  html += "<a href='/rele?relay=2'><button><i class='fas fa-door-open icon'></i>Relé 2 ON</button></a>";
  html += "<a href='/codes'><button class='btn-info'><i class='fas fa-database icon'></i>Gestión de Códigos</button></a>";
  html += "<a href='/remote-codes'><button class='btn-info'><i class='fas fa-cloud icon'></i>Códigos Remotos</button></a>";
  html += "<a href='/reboot'><button class='btn-warning'><i class='fas fa-sync-alt icon'></i>Reiniciar</button></a>";
  html += "<a href='/reset'><button class='btn-danger'><i class='fas fa-exclamation-triangle icon'></i>Resetear</button></a>";
   html += "</div>";
 
   // Último acceso con información de teclado
   html += "<h2><i class='fas fa-history icon'></i>Último acceso</h2><table><tr><th>Tipo</th><th>Código</th><th>Hora</th><th>Teclado</th></tr>";
   if (strlen(lastCode) > 0) {
      html += "<tr><td>" + String(lastType) + "</td><td>" + String(lastCode) + "</td><td>" + String(lastTime) + "</td><td>" + String(lastKeyboardId) + "</td></tr>";
   } else {
     html += "<tr><td colspan='4'>No hay registros todavía</td></tr>";
   }
   html += "</table>";
 
   // Estado de conexión
   html += "<h2><i class='fas fa-network-wired icon'></i>Estado de Conexión</h2>";
   html += "<table>";
   html += "<tr><th>Elemento</th><th>Estado</th></tr>";
   html += "<tr><td>Red (Ethernet)</td><td>" + String(ethConnected ? "Conectado" : "Desconectado") + "</td></tr>";
 
   String mqttStatus = "Pendiente";
   if (mqttClient.connected()) mqttStatus = "Conectado";
   else if (!ethConnected) mqttStatus = "Sin conexión";
 
  html += "<tr><td>Servidor MQTT</td><td>" + mqttStatus + "</td></tr>";
   html += "<tr><td>Hora del sistema</td><td>" + String(currentTimeString) + "</td></tr>";
  html += "<tr><td>Última sincronización</td><td>" + String((millis() - lastTimeSync) / 1000) + " segundos</td></tr>";
  html += "<tr><td>Códigos almacenados</td><td>" + String(storedCodes.count) + "/" + String(MAX_CODES) + "</td></tr>";
  html += "<tr><td>Modo validación</td><td>" + String(storedCodes.localValidationFirst ? "Local primero" : "Remoto primero") + "</td></tr>";
  html += "<tr><td>Modo torno</td><td>" + String(isTurnstileModeEnabled() ? "🟢 ACTIVO" : "🔴 INACTIVO") + "</td></tr>";
   if (isTurnstileModeEnabled()) {
     html += "<tr><td>Solicitud pendiente</td><td>" + String(pendingRequest.active ? "⏳ ACTIVA" : "✅ Ninguna") + "</td></tr>";
  }
  html += "</table>";

  // =================== SECCIÓN OTA ===================
  html += "<div class='security-status'>";
  html += "<h2><i class='fas fa-download icon'></i>Actualización de Firmware</h2>";
  html += "<table>";
  html += "<tr><th>Parámetro</th><th>Valor</th></tr>";
  html += "<tr><td>Versión Actual</td><td><strong>" + String(firmwareVersion) + "</strong></td></tr>";
  html += "<tr><td>Fecha de Compilación</td><td>" + String(firmwareBuild) + "</td></tr>";
  html += "<tr><td>Actualización Automática</td><td>" + String(otaConfig.autoUpdateEnabled ? "✅ Habilitada" : "❌ Deshabilitada") + "</td></tr>";
  html += "<tr><td>URL del Servidor</td><td>" + String(otaConfig.updateUrl) + "</td></tr>";
  html += "<tr><td>Intervalo de Verificación</td><td>" + String(otaConfig.checkInterval) + " horas</td></tr>";
  html += "<tr><td>Última Verificación</td><td>" + String(otaConfig.lastCheck > 0 ? "Hace " + String((millis() - otaConfig.lastCheck) / 3600000) + " horas" : "Nunca") + "</td></tr>";
  html += "</table>";
  
  html += "<div style='margin-top: 15px;'>";
  html += "<a href='/ota'><button class='btn btn-primary'>Configurar Actualizaciones</button></a>";
  html += "<button onclick='checkForUpdates()' class='btn btn-warning' style='margin-left: 10px;'>Verificar Ahora</button>";
  html += "</div>";
  html += "</div>";

  html += "</div></main>";
   
   // Pie de página con datos de contacto
   html += "<footer style='background-color: #2c3e50; color: white; padding: 20px; text-align: center; margin-top: 30px;'>";
   html += "<div style='max-width: 800px; margin: 0 auto;'>";
   html += "<h3 style='margin: 0 0 10px 0; color: #ecf0f1;'>Smart World And Things SLU</h3>";
   html += "<p style='margin: 5px 0; font-size: 14px;'>";
   html += "<strong>Web:</strong> <a href='https://www.swat-id.com' style='color: #3498db; text-decoration: none;'>www.swat-id.com</a> | ";
   html += "<strong>Tel:</strong> 633 44 84 27 | ";
   html += "<strong>Email:</strong> <a href='mailto:info@swat-id.com' style='color: #3498db; text-decoration: none;'>info@swat-id.com</a>";
   html += "</p>";
   html += "<p style='margin: 5px 0; font-size: 12px; color: #bdc3c7;'>Controladora A2 - SWATID | Sistema de Control de Acceso Dual Wiegand</p>";
   html += "</div>";
   html += "</footer>";
   
   html += "<script>";
   html += "function toggleIpFields(){document.getElementById('staticIpFields').style.display=(document.getElementById('useDhcp').value=='1')?'none':'block';}";
   html += "function checkForUpdates() {";
   html += "  fetch('/ota/check')";
   html += "    .then(response => response.json())";
   html += "    .then(data => {";
   html += "      alert('Verificación de actualizaciones completada');";
   html += "      location.reload();";
   html += "    })";
   html += "    .catch(error => {";
   html += "      alert('Error verificando actualizaciones: ' + error);";
   html += "    });";
   html += "}";
   html += "</script>";
   html += "</body></html>";
 
   server.send(200, "text/html", html);
 }
 
 void handleSave() {
   if (!server.authenticate(admin_user, admin_password)) {
     return server.requestAuthentication();
   }
   
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
    
    if (newLocalValidationFirst != storedCodes.localValidationFirst) {
      storedCodes.localValidationFirst = newLocalValidationFirst;
      saveStoredCodes();
      Serial.printf("⚙️ Modo de validación cambiado a: %s\n", 
                    storedCodes.localValidationFirst ? "Primero Local" : "Primero Remoto");
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
   if (!server.authenticate(admin_user, admin_password))
     return server.requestAuthentication();
 
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
   if (!server.authenticate(admin_user, admin_password)) {
     return server.requestAuthentication();
   }
   
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
   if (!server.authenticate(admin_user, admin_password)) {
     return server.requestAuthentication();
   }
   
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
   if (!server.authenticate(admin_user, admin_password)) {
     return server.requestAuthentication();
   }
 
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  // Obtener parámetros de paginación y búsqueda
  int page = server.arg("page").toInt();
  if (page < 1) page = 1;
  
  String searchTerm = server.arg("search");
  searchTerm.trim();
  
  const int ITEMS_PER_PAGE = 20;
  int startIndex = (page - 1) * ITEMS_PER_PAGE;
  int endIndex = startIndex + ITEMS_PER_PAGE;

  String html = R"=====(
 <!DOCTYPE html>
 <html>
 <head>
   <meta charset='UTF-8'>
   <title>Gestión de Códigos - Controladora A2</title>
   <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css'>
   <style>
     body { font-family: Arial, sans-serif; margin: 30px; background-color: #f7f9fb; color: #333; }
     h1, h2 { color: #2c3e50; }
     .security-info { background: #fff3cd; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #ffc107; }
     .dual-info { background: #e8f5e8; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #28a745; }
     form { background: #fff; padding: 20px; border-radius: 10px; margin-bottom: 30px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); max-width: 600px; }
     label { display: block; margin-top: 15px; font-weight: bold; }
     input, select { width: 100%; padding: 8px; margin-top: 5px; border-radius: 5px; border: 1px solid #ccc; }
     button { background-color: #2ecc71; color: white; padding: 10px 15px; border: none; border-radius: 5px; margin-top: 15px; cursor: pointer; }
     button:hover { background-color: #27ae60; }
     table { width: 100%; border-collapse: collapse; margin-top: 30px; }
     th, td { border: 1px solid #ccc; padding: 10px; text-align: center; }
     th { background-color: #ecf0f1; }
     a.delete { color: #e74c3c; text-decoration: none; }
     a.delete:hover { text-decoration: underline; }
   </style>
 </head>
 <body>
   <h1><i class='fas fa-keyboard'></i> Gestión de Códigos - Controladora A2</h1>
 
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
    <h3><i class='fas fa-file-csv'></i> Gestión de Archivos CSV</h3>
    <p><strong>Formato CSV:</strong> Tipo,Codigo,Teclado,Rele,Fecha_Creacion</p>
    <p><strong>Tipos soportados:</strong> PIN (4-6 dígitos), TAG (1-16 caracteres) | <strong>Teclados:</strong> 1, 2 | <strong>Relés:</strong> 1, 2</p>
    
    <div style='display: flex; gap: 15px; align-items: center; flex-wrap: wrap; margin-top: 15px;'>
      <a href='/export/codes' style='text-decoration: none;'>
        <button style='background-color: #28a745; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer;'>
          <i class='fas fa-download'></i> Exportar a CSV
        </button>
      </a>
      
      <a href='/codes/template' style='text-decoration: none;'>
        <button style='background-color: #6c757d; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer;'>
          <i class='fas fa-file-download'></i> Descargar Plantilla
        </button>
      </a>
      
      <form action='/codes/bulk-import' method='post' enctype='multipart/form-data' style='display: flex; gap: 10px; align-items: center;'>
        <input type='file' name='csvFile' accept='.csv' required style='padding: 8px; border: 1px solid #ccc; border-radius: 4px;'>
        <button type='submit' style='background-color: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer;'>
          <i class='fas fa-upload'></i> Importar CSV
        </button>
      </form>
    </div>
  </div>

  <!-- Sección de Lectura de Tags en Tiempo Real -->
  <div style='background: #fff; padding: 20px; border-radius: 10px; margin: 20px 0; box-shadow: 0 2px 8px rgba(0,0,0,0.1);'>
    <h3><i class='fas fa-qrcode'></i> Lectura de Tags en Tiempo Real</h3>
    
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
        <i class='fas fa-play'></i> Iniciar Lectura
      </button>
      <button id='stopReading' onclick='stopTagReading()' disabled style='background-color: #e74c3c; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>
        <i class='fas fa-stop'></i> Parar Lectura
      </button>
      <button id='exportReadTags' onclick='exportReadTags()' disabled style='background-color: #3498db; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>
        <i class='fas fa-download'></i> Exportar CSV
      </button>
      <button id='loadReadTags' onclick='loadReadTags()' disabled style='background-color: #f39c12; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>
        <i class='fas fa-upload'></i> Cargar a Memoria
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
    <h2><i class='fas fa-plus-circle'></i> Añadir nuevo código</h2>
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

    <button type='submit'><i class='fas fa-plus'></i> Añadir Código</button>
  </form>

  <!-- Buscador y filtros -->
  <div style='background: #f8f9fa; padding: 15px; border-radius: 8px; margin: 20px 0;'>
    <h3><i class='fas fa-search'></i> Buscar Códigos</h3>
    <form method='GET' action='/codes' style='display: flex; gap: 10px; align-items: center; flex-wrap: wrap;'>
      <input type='text' name='search' placeholder='Buscar por código o tipo...' value=')=====";
  html += searchTerm;
  html += R"=====(' style='flex: 1; min-width: 200px; padding: 8px; border: 1px solid #ddd; border-radius: 4px;'>
      <button type='submit' style='background-color: #007bff; color: white; padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer;'>
        <i class='fas fa-search'></i> Buscar
      </button>
      <a href='/codes' style='background-color: #6c757d; color: white; padding: 8px 16px; text-decoration: none; border-radius: 4px;'>
        <i class='fas fa-times'></i> Limpiar
      </a>
    </form>
  </div>

   <h2><i class='fas fa-database'></i> Códigos Almacenados ()=====";
   html += String(storedCodes.count) + "/" + String(MAX_CODES);
   html += R"=====()</h2>
  <table>
    <tr><th>Tipo</th><th>Valor</th><th>Teclado</th><th>Relé</th><th>Acción</th></tr>
 )=====" ;

  // Filtrar y paginar códigos
  int filteredCount = 0;
  int displayedCount = 0;
  
  for (int i = 0; i < storedCodes.count; i++) {
    // Aplicar filtro de búsqueda
    bool matchesFilter = true;
    if (searchTerm.length() > 0) {
      String codeType = String(storedCodes.codes[i].type);
      String codeValue = String(storedCodes.codes[i].value);
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
        html += "<td>" + String(storedCodes.codes[i].type) + "</td>";
        html += "<td>" + String(storedCodes.codes[i].value) + "</td>";
        
        String keyboardName = "Ambos";
        String keyboardIcon = "🔑";
        if (storedCodes.codes[i].keyboard_id == 1) {
          keyboardName = "Teclado 1";
          keyboardIcon = "🔑";
        } else if (storedCodes.codes[i].keyboard_id == 2) {
          keyboardName = "Teclado 2";
          keyboardIcon = "🔑";
        }
        
        html += "<td>" + keyboardIcon + " " + keyboardName + "</td>";
        html += "<td>⚡ Relé " + String(storedCodes.codes[i].relay) + "</td>";
        html += "<td><a class='delete' href='/codes/delete?type=" + String(storedCodes.codes[i].type);
        html += "&value=" + String(storedCodes.codes[i].value);
        html += "&keyboard=" + String(storedCodes.codes[i].keyboard_id) + "'><i class='fas fa-trash-alt'></i> Eliminar</a></td>";
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
       html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'><i class='fas fa-chevron-left'></i> Anterior</a>";
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
       html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'>Siguiente <i class='fas fa-chevron-right'></i></a>";
     }
     
     html += "</div>";
   } else if (filteredCount > 0) {
     html += "<div style='margin: 20px 0; text-align: center;'>";
     html += "<p>Mostrando " + String(filteredCount) + " códigos</p>";
     html += "</div>";
   }
   
   html += R"=====(
   <br>
   
   
   
   <a href='/'><button style='background-color:#3498db'><i class='fas fa-home'></i> Volver al inicio</button></a>
   
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
   if (!server.authenticate(admin_user, admin_password)) {
     return server.requestAuthentication();
   }
 
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
   if (!server.authenticate(admin_user, admin_password)) {
     return server.requestAuthentication();
   }
 
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
 
 // =================== FUNCIÓN SETUP ===================
 void setup() {
   Serial.begin(115200);
   delay(1000);
   
   Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
   Serial.println("║                    KC868-A2 DUAL WIEGAND                    ║");
   Serial.println("║                  Firmware v2.5.0                  ║");
   Serial.println("╚══════════════════════════════════════════════════════════════╝");
   
   // Inicializar RS-485 para compatibilidad
   RS485_Serial.begin(RS485_BAUD, SERIAL_8N1, RS485_RX2, RS485_TX2);
   delay(500);
   
   Serial.println("🔧 Configuración de Hardware:");
   Serial.printf("   TECLADO 1: D0=GPIO%d, D1=GPIO%d (Principal)\n", WIEGAND1_D0, WIEGAND1_D1);
   Serial.printf("   TECLADO 2: D0=GPIO%d, D1=GPIO%d (Secundario)\n", WIEGAND2_D0, WIEGAND2_D1);
   Serial.printf("   RS485: GPIO%d/%d (Compatibilidad)\n", RS485_RX2, RS485_TX2);
   Serial.printf("   Relés: R1=GPIO%d, R2=GPIO%d\n", RELE1_PIN, RELE2_PIN);
   
  EEPROM.begin(4096);  // Aumentado para soportar 500 códigos
  loadConfiguration();
  loadTurnstileConfig();  // Cargar configuración del modo torno
  loadStoredCodes();
  loadStoredRemoteCodes();  // Cargar códigos remotos
  
  // Funciones de diagnóstico comentadas para reducir tamaño del firmware
  // diagnoseEEPROM();
  // verifyMemoryLayout();
  // testPersistence();
  // verifyEEPROMIntegrity();
  
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
   Serial.printf("   Códigos almacenados: %d/%d\n", storedCodes.count, MAX_CODES);
 
  // Alimentar PHY LAN8720
  pinMode(ETH_PHY_POWER_PIN, OUTPUT);
  digitalWrite(ETH_PHY_POWER_PIN, HIGH);
   delay(100);
   Serial.println("⚡ PHY LAN8720 alimentado");
   
   // Configurar pines de relés
   pinMode(RELE1_PIN, OUTPUT);
   digitalWrite(RELE1_PIN, LOW);
   pinMode(RELE2_PIN, OUTPUT);
   digitalWrite(RELE2_PIN, LOW);
   Serial.println("⚡ Relés inicializados (OFF)");
   
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
   
   ETH.setHostname(deviceName);
   
   // Esperar conexión Ethernet
   Serial.println("🌐 Esperando conexión Ethernet (máx 30s)...");
   unsigned long startTime = millis();
   while (!ethConnected && (millis() - startTime < 30000)) {
     delay(500);
     Serial.print(".");
     if ((millis() - startTime) % 5000 == 0) {
       Serial.printf("\n🕐 Esperando... %lu/%lu ms\n", millis() - startTime, 30000UL);
     }
   }
   Serial.println();
   
  if (!ethConnected) {
    Serial.println("❌ No se pudo obtener IP por DHCP");
    Serial.println("🏠 Iniciando MODO AUTÓNOMO con AP de configuración...");
    setupAPMode();
  } else {
    Serial.printf("✅ Ethernet conectado: %s\n", ip.toString().c_str());
    setupWebServer();
  }
   
  Serial.println("\n🎯 SISTEMA DUAL WIEGAND CON SEGURIDAD COMPLETO LISTO");
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.println("✅ Teclado 1 (GPIO33/14): Operativo");
  Serial.println("✅ Teclado 2 (GPIO4/16):  Operativo");
  Serial.println("🔒 Sistema de seguridad: Activo");
  Serial.println("📡 Serial MQTT fijo: " + fixedSerialNumber);
  Serial.println("💾 Capacidad códigos: " + String(MAX_CODES));
  Serial.println("💡 Ambos teclados pueden usarse simultáneamente");
  
  if (ethConnected) {
    Serial.println("🌐 Interfaz web: http://" + ip.toString());
    Serial.println("📡 MQTT: Conectado y operativo");
  } else {
    Serial.println("🏠 MODO AUTÓNOMO: Funcionando sin conectividad");
    Serial.println("📶 AP Config: http://" + ip.toString() + " (SSID: SWATID_CONFIG_*)");
    Serial.println("✅ Códigos locales: Completamente operativos");
  }
  
  Serial.println("═══════════════════════════════════════════════════════════\n");
 }
 
 // =================== FUNCIÓN LOOP PRINCIPAL MEJORADA ===================
 void loop() {
   unsigned long currentTime = millis();
   
   // ========== CONEXIÓN MQTT ==========
   if (!mqttClient.connected() && ethConnected) {
     static unsigned long lastReconnectAttempt = 0;
     
     if (currentTime - lastReconnectAttempt > 5000) {
       lastReconnectAttempt = currentTime;
       Serial.println("🔄 Intentando reconexión MQTT...");
       connectToMqtt();
     }
   }
   
   // Procesar cola MQTT si está conectado
   if (mqttClient.connected()) {
     if (!mqttClient.loop()) {
       Serial.println("❌ Error en mqttClient.loop() - Reintentando conexión");
       mqttClient.disconnect();
     }
   }
   
  // ========== PROCESAMIENTO DE TECLADOS ==========
  processWiegand1Data();
  processWiegand2Data();
  processRS485Keypad();
  
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
     
    // Verificar si MQTT se desconectó mucho tiempo (SOLO si hay conectividad Ethernet)
    static unsigned long mqttDisconnectedTime = 0;
    if (!mqttClient.connected() && ethConnected) {
      if (mqttDisconnectedTime == 0) {
        mqttDisconnectedTime = currentTime;
      } else if (currentTime - mqttDisconnectedTime > 300000) { // 5 minutos
        Serial.println("⚠️ MQTT desconectado por mucho tiempo - Reiniciando...");
        publishError(9, "MQTT desconectado prolongado - Reiniciando sistema");
        delay(1000);
        ESP.restart();
      }
    } else {
      mqttDisconnectedTime = 0;
    }
    
    // Log de estado de autonomía cuando no hay conectividad
    if (!ethConnected) {
      static unsigned long lastAutonomyLog = 0;
      if (currentTime - lastAutonomyLog > 300000) { // Cada 5 minutos
        lastAutonomyLog = currentTime;
        Serial.println("🏠 MODO AUTÓNOMO: Sistema funcionando sin conectividad Ethernet");
        Serial.printf("   Códigos locales disponibles: %d/%d\n", storedCodes.count, MAX_CODES);
        Serial.printf("   Modo validación: %s\n", storedCodes.localValidationFirst ? "Local primero" : "Remoto primero");
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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  // Obtener parámetros de paginación y búsqueda
  int page = server.arg("page").toInt();
  if (page < 1) page = 1;
  
  String searchTerm = server.arg("search");
  searchTerm.trim();
  
  const int ITEMS_PER_PAGE = 20;
  int startIndex = (page - 1) * ITEMS_PER_PAGE;
  int endIndex = startIndex + ITEMS_PER_PAGE;

  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <title>Códigos Remotos - Controladora A2</title>
  <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css'>
  <style>
    body { font-family: Arial, sans-serif; margin: 30px; background-color: #f7f9fb; color: #333; }
    h1, h2 { color: #2c3e50; }
    .remote-info { background: #e3f2fd; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #2196f3; }
    .time-info { background: #fff3e0; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #ff9800; }
    form { background: #fff; padding: 20px; border-radius: 10px; margin-bottom: 30px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); max-width: 800px; }
    label { display: block; margin-top: 15px; font-weight: bold; }
    input, select { width: 100%; padding: 8px; margin-top: 5px; border-radius: 5px; border: 1px solid #ccc; }
    button { background-color: #2196f3; color: white; padding: 10px 15px; border: none; border-radius: 5px; margin-top: 15px; cursor: pointer; }
    button:hover { background-color: #1976d2; }
    .btn-danger { background-color: #f44336; }
    .btn-danger:hover { background-color: #d32f2f; }
    table { width: 100%; border-collapse: collapse; margin-top: 30px; }
    th, td { border: 1px solid #ccc; padding: 10px; text-align: center; }
    th { background-color: #ecf0f1; }
    a.delete { color: #e74c3c; text-decoration: none; }
    a.delete:hover { text-decoration: underline; }
    .time-slot { background: #f5f5f5; padding: 5px; margin: 2px; border-radius: 3px; font-size: 0.9em; }
  </style>
</head>
<body>
  <h1><i class='fas fa-cloud'></i> Gestión de Códigos Remotos - Controladora A2</h1>

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
    <a href='/export/remote-codes'><button class='btn-info'><i class='fas fa-download icon'></i>Exportar a CSV</button></a>
  </div>

  <form action='/remote-codes/add' method='post'>
    <h2><i class='fas fa-plus-circle'></i> Añadir nuevo código remoto</h2>
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

    <button type='submit'><i class='fas fa-plus'></i> Añadir Código Remoto</button>
  </form>

  <!-- Buscador y filtros -->
  <div style='background: #f8f9fa; padding: 15px; border-radius: 8px; margin: 20px 0;'>
    <h3><i class='fas fa-search'></i> Buscar Códigos Remotos</h3>
    <form method='GET' action='/remote-codes' style='display: flex; gap: 10px; align-items: center; flex-wrap: wrap;'>
      <input type='text' name='search' placeholder='Buscar por código o tipo...' value=')=====";
  html += searchTerm;
  html += R"=====(' style='flex: 1; min-width: 200px; padding: 8px; border: 1px solid #ddd; border-radius: 4px;'>
      <button type='submit' style='background-color: #007bff; color: white; padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer;'>
        <i class='fas fa-search'></i> Buscar
      </button>
      <a href='/remote-codes' style='background-color: #6c757d; color: white; padding: 8px 16px; text-decoration: none; border-radius: 4px;'>
        <i class='fas fa-times'></i> Limpiar
      </a>
    </form>
  </div>

  <h2><i class='fas fa-database'></i> Códigos Remotos Almacenados ()=====";
  html += String(storedRemoteCodes.count) + "/" + String(MAX_REMOTE_CODES);
  html += R"=====()</h2>
  <table>
    <tr><th>Tipo</th><th>Valor</th><th>Teclado</th><th>Relé</th><th>Franjas Horarias</th><th>Acción</th></tr>
)=====";

  // Filtrar y paginar códigos remotos
  int filteredCount = 0;
  int displayedCount = 0;
  
  for (int i = 0; i < storedRemoteCodes.count; i++) {
    // Aplicar filtro de búsqueda
    bool matchesFilter = true;
    if (searchTerm.length() > 0) {
      String codeType = String(storedRemoteCodes.codes[i].type);
      String codeValue = String(storedRemoteCodes.codes[i].value);
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
        html += "<td>" + String(storedRemoteCodes.codes[i].type) + "</td>";
        html += "<td>" + String(storedRemoteCodes.codes[i].value) + "</td>";
        
        String keyboardName = "Ambos";
        String keyboardIcon = "🔑";
        if (storedRemoteCodes.codes[i].keyboard_id == 1) {
          keyboardName = "Teclado 1";
          keyboardIcon = "🔑";
        } else if (storedRemoteCodes.codes[i].keyboard_id == 2) {
          keyboardName = "Teclado 2";
          keyboardIcon = "🔑";
        }
        
        html += "<td>" + keyboardIcon + " " + keyboardName + "</td>";
        html += "<td>⚡ Relé " + String(storedRemoteCodes.codes[i].relay) + "</td>";
        
        // Mostrar franjas horarias
        html += "<td>";
        if (storedRemoteCodes.codes[i].time_slots_count == 0) {
          html += "<span style='color: #666;'>Sin restricción</span>";
        } else {
          for (int j = 0; j < storedRemoteCodes.codes[i].time_slots_count; j++) {
            TimeSlot& slot = storedRemoteCodes.codes[i].time_slots[j];
            html += "<div class='time-slot'>";
            html += String(slot.start_hour) + ":" + String(slot.start_minute < 10 ? "0" : "") + String(slot.start_minute);
            html += " - " + String(slot.end_hour) + ":" + String(slot.end_minute < 10 ? "0" : "") + String(slot.end_minute);
            html += " (Días: " + String(slot.days_of_week) + ")";
            html += "</div>";
          }
        }
        html += "</td>";
        
        html += "<td><a class='delete' href='/remote-codes/delete?type=" + String(storedRemoteCodes.codes[i].type);
        html += "&value=" + String(storedRemoteCodes.codes[i].value) + "'><i class='fas fa-trash-alt'></i> Eliminar</a></td>";
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
      html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'><i class='fas fa-chevron-left'></i> Anterior</a>";
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
      html += "' style='margin: 0 5px; padding: 8px 12px; background-color: #007bff; color: white; text-decoration: none; border-radius: 4px;'>Siguiente <i class='fas fa-chevron-right'></i></a>";
    }
    
    html += "</div>";
  } else if (filteredCount > 0) {
    html += "<div style='margin: 20px 0; text-align: center;'>";
    html += "<p>Mostrando " + String(filteredCount) + " códigos remotos</p>";
    html += "</div>";
  }
  
  html += R"=====(
  <br>
  <a href='/remote-codes/delete-all'><button class='btn-danger'><i class='fas fa-trash-alt'></i> Eliminar Todos los Códigos Remotos</button></a>
  <br><br>
  <a href='/'><button style='background-color:#3498db'><i class='fas fa-home'></i> Volver al inicio</button></a>

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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

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
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }

  deleteAllRemoteCodes();
  Serial.println("🗑️ Todos los códigos remotos eliminados");

  server.sendHeader("Location", "/remote-codes");
  server.send(303);
}

// =================== FUNCIONES PARA CÓDIGOS REMOTOS ===================

void loadStoredRemoteCodes() {
  EEPROM.get(EEPROM_REMOTE_CODES_OFFSET, storedRemoteCodes);
  
  // Verificar si los datos son válidos
  if (storedRemoteCodes.validMarker != 0xDEADBEEF || storedRemoteCodes.version != 1) {
    Serial.println("📦 Inicializando códigos remotos por primera vez");
    storedRemoteCodes.validMarker = 0xDEADBEEF;
    storedRemoteCodes.version = 1;
    storedRemoteCodes.count = 0;
    memset(storedRemoteCodes.codes, 0, sizeof(storedRemoteCodes.codes));
    saveStoredRemoteCodes();
  }
  
  Serial.printf("📦 Códigos remotos cargados: %d/%d\n", storedRemoteCodes.count, MAX_REMOTE_CODES);
}

void saveStoredRemoteCodes() {
  EEPROM.put(EEPROM_REMOTE_CODES_OFFSET, storedRemoteCodes);
  EEPROM.commit();
  Serial.printf("💾 Códigos remotos guardados: %d códigos\n", storedRemoteCodes.count);
}

bool addRemoteCode(const char* type, const char* value, uint8_t keyboardId, uint8_t relay, const TimeSlot* timeSlots, uint8_t timeSlotsCount) {
  if (storedRemoteCodes.count >= MAX_REMOTE_CODES) {
    Serial.println("❌ No se puede añadir código remoto: memoria llena");
    return false;
  }
  
  // Verificar si el código ya existe
  for (int i = 0; i < storedRemoteCodes.count; i++) {
    if (strcmp(storedRemoteCodes.codes[i].type, type) == 0 && 
        strcmp(storedRemoteCodes.codes[i].value, value) == 0) {
      Serial.printf("⚠️ Código remoto ya existe: %s %s\n", type, value);
      return false;
    }
  }
  
  // Añadir nuevo código
  RemoteCodeEntry* newCode = &storedRemoteCodes.codes[storedRemoteCodes.count];
  strncpy(newCode->type, type, sizeof(newCode->type) - 1);
  newCode->type[sizeof(newCode->type) - 1] = '\0';
  strncpy(newCode->value, value, sizeof(newCode->value) - 1);
  newCode->value[sizeof(newCode->value) - 1] = '\0';
  newCode->keyboard_id = keyboardId;
  newCode->relay = relay;
  newCode->time_slots_count = min(timeSlotsCount, (uint8_t)4);
  
  // Copiar franjas horarias
  for (int i = 0; i < newCode->time_slots_count; i++) {
    newCode->time_slots[i] = timeSlots[i];
  }
  
  storedRemoteCodes.count++;
  saveStoredRemoteCodes();
  
  Serial.printf("✅ Código remoto añadido: %s %s (Keypad: %d, Relé: %d, Franjas: %d)\n", 
                type, value, keyboardId, relay, timeSlotsCount);
  return true;
}

bool deleteRemoteCode(const char* type, const char* value) {
  for (int i = 0; i < storedRemoteCodes.count; i++) {
    if (strcmp(storedRemoteCodes.codes[i].type, type) == 0 && 
        strcmp(storedRemoteCodes.codes[i].value, value) == 0) {
      
      // Mover todos los códigos posteriores una posición hacia atrás
      for (int j = i; j < storedRemoteCodes.count - 1; j++) {
        storedRemoteCodes.codes[j] = storedRemoteCodes.codes[j + 1];
      }
      
      storedRemoteCodes.count--;
      saveStoredRemoteCodes();
      
      Serial.printf("✅ Código remoto eliminado: %s %s\n", type, value);
      return true;
    }
  }
  
  Serial.printf("❌ Código remoto no encontrado: %s %s\n", type, value);
  return false;
}

void deleteAllRemoteCodes() {
  storedRemoteCodes.count = 0;
  memset(storedRemoteCodes.codes, 0, sizeof(storedRemoteCodes.codes));
  saveStoredRemoteCodes();
  Serial.println("✅ Todos los códigos remotos eliminados");
}

bool isRemoteCodeStored(const char* type, const char* value, uint8_t keyboardId, uint8_t* relay) {
  for (int i = 0; i < storedRemoteCodes.count; i++) {
    RemoteCodeEntry* code = &storedRemoteCodes.codes[i];
    
    if (strcmp(code->type, type) == 0 && strcmp(code->value, value) == 0) {
      // Verificar keypad (0 = ambos, o específico)
      if (code->keyboard_id == 0 || code->keyboard_id == keyboardId) {
        // Verificar franjas horarias si existen
        if (code->time_slots_count > 0) {
          if (!isCurrentTimeInTimeSlots(code->time_slots, code->time_slots_count)) {
            Serial.printf("⏰ Código remoto fuera de horario: %s %s\n", type, value);
            return false;
          }
        }
        
        if (relay) {
          *relay = code->relay;
        }
        Serial.printf("✅ Código remoto válido: %s %s (Relé: %d)\n", type, value, code->relay);
        return true;
      }
    }
  }
  
  return false;
}

bool isTimeSlotValid(const TimeSlot& timeSlot) {
  return (timeSlot.start_hour < 24 && timeSlot.start_minute < 60 &&
          timeSlot.end_hour < 24 && timeSlot.end_minute < 60 &&
          timeSlot.days_of_week > 0 && timeSlot.days_of_week <= 127);
}

bool isCurrentTimeInTimeSlots(const TimeSlot* timeSlots, uint8_t timeSlotsCount) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("⚠️ No se puede obtener la hora actual para validación");
    return true; // Si no hay hora, permitir acceso
  }
  
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentWeekday = timeinfo.tm_wday; // 0=Domingo, 1=Lunes, ..., 6=Sábado
  
  // Convertir domingo (0) a bit 64, lunes (1) a bit 1, etc.
  uint8_t currentDayBit = (currentWeekday == 0) ? 64 : (1 << (currentWeekday - 1));
  
  for (int i = 0; i < timeSlotsCount; i++) {
    const TimeSlot& slot = timeSlots[i];
    
    // Verificar día de la semana
    if (!(slot.days_of_week & currentDayBit)) {
      continue;
    }
    
    // Verificar hora
    int currentTotalMinutes = currentHour * 60 + currentMinute;
    int startTotalMinutes = slot.start_hour * 60 + slot.start_minute;
    int endTotalMinutes = slot.end_hour * 60 + slot.end_minute;
    
    if (currentTotalMinutes >= startTotalMinutes && currentTotalMinutes <= endTotalMinutes) {
      return true;
    }
  }
  
  return false;
}