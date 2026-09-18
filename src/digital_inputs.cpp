#include "digital_inputs.h"

#include <EEPROM.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "hw_config.h"

// Servicios de main.ino usados por este módulo (se moverán a sus propios
// módulos en tranches posteriores: mqtt_service, access_logic)
extern PubSubClient mqttClient;
extern String fixedSerialNumber;
extern unsigned long messageId;
String getTimestamp();
void controlReleWithDuration(float duration, int relay);

DigitalInputConfig digitalInputConfig;
DigitalInputState di1State = {false, false, false, 0, false};
DigitalInputState di2State = {false, false, false, 0, false};

uint32_t calculateDIChecksum(const DigitalInputConfig& cfg) {
  uint32_t sum = 0;
  sum += cfg.di1_enabled;
  sum += cfg.di1_relay << 8;
  sum += cfg.di1_inverse << 16;
  sum += cfg.di1_duration_ms;
  sum += cfg.di2_enabled;
  sum += cfg.di2_relay << 8;
  sum += cfg.di2_inverse << 16;
  sum += cfg.di2_duration_ms;
  return sum ^ 0x55AA55AA;  // XOR con patrón distintivo
}

void loadDigitalInputConfig() {
  Serial.println("🔄 Cargando configuración DI desde EEPROM...");
  
  EEPROM.get(EEPROM_DIGITAL_INPUT_OFFSET, digitalInputConfig);
  
  // Verificar marcador de validación
  if (digitalInputConfig.validMarker != DIGITAL_INPUT_CONFIG_MARKER) {
    Serial.println("🔧 Inicializando configuración DI por primera vez...");
    digitalInputConfig.validMarker = DIGITAL_INPUT_CONFIG_MARKER;
    digitalInputConfig.di1_enabled = 0;
    digitalInputConfig.di1_relay = 1;
    digitalInputConfig.di1_inverse = 0;
    digitalInputConfig.di1_duration_ms = 2000;
    digitalInputConfig.di2_enabled = 0;
    digitalInputConfig.di2_relay = 2;
    digitalInputConfig.di2_inverse = 0;
    digitalInputConfig.di2_duration_ms = 2000;
    digitalInputConfig.checksum = 0;
    
    saveDigitalInputConfig();
    Serial.println("✅ Configuración DI inicializada correctamente");
  } else {
    Serial.printf("💾 Configuración DI cargada: DI1=%s, DI2=%s\n",
                  digitalInputConfig.di1_enabled ? "ON" : "OFF",
                  digitalInputConfig.di2_enabled ? "ON" : "OFF");
  }
}

// Guardar configuración de entradas digitales en EEPROM
void saveDigitalInputConfig() {
  digitalInputConfig.validMarker = DIGITAL_INPUT_CONFIG_MARKER;
  
  // Guardar en EEPROM (mismo estilo que saveStoredCodes original)
  EEPROM.put(EEPROM_DIGITAL_INPUT_OFFSET, digitalInputConfig);
  
  if (!EEPROM.commit()) {
    Serial.println("❌ [DI] Error: Fallo al hacer commit en EEPROM");
    return;
  }
  
  // Verificar integridad después de guardar
  DigitalInputConfig verify;
  EEPROM.get(EEPROM_DIGITAL_INPUT_OFFSET, verify);
  
  if (verify.validMarker != DIGITAL_INPUT_CONFIG_MARKER) {
    Serial.println("❌ [DI] Error: Verificación de integridad falló");
    return;
  }
  
  Serial.printf("💾 [DI] Configuración guardada: DI1=%s, DI2=%s\n",
                digitalInputConfig.di1_enabled ? "ON" : "OFF",
                digitalInputConfig.di2_enabled ? "ON" : "OFF");
}

// Procesar entrada digital con modo Normal o Inverso
void processDigitalInput(int inputNumber, DigitalInputState &state, uint8_t enabled, uint8_t relay, uint32_t duration_ms, uint8_t inverse) {
  if (!enabled) {
    // Si está deshabilitada y el relé estaba activo por esta entrada en modo inverso, desactivarlo
    if (state.relayActivated && inverse) {
      int relayPin = (relay == 1) ? RELE1_PIN : RELE2_PIN;
      digitalWrite(relayPin, LOW);
      state.relayActivated = false;
      Serial.printf("🔴 [DI%d] Entrada deshabilitada → Relé %d desactivado\n", inputNumber, relay);
    }
    return;
  }
  
  // Leer estado actual del pin
  int pin = (inputNumber == 1) ? DI1_PIN : DI2_PIN;
  state.currentState = digitalRead(pin);
  
  float duration_sec = duration_ms / 1000.0f;
  
  // MODO NORMAL: HIGH activa el relé por duración configurada
  if (!inverse) {
    // Detectar flanco de subida (LOW → HIGH)
    if (!state.lastState && state.currentState && !state.waitingForLow) {
      Serial.printf("📍 [DI%d] Modo NORMAL - HIGH detectado → Activando Relé %d por %dms (%.1fs)\n", 
                    inputNumber, relay, duration_ms, duration_sec);
      
      // Activar relé con la duración especificada (en segundos)
      controlReleWithDuration(duration_sec, relay);
      
      // Actualizar estado
      state.relayActivated = true;
      state.activationTime = millis();
      state.waitingForLow = true;
      
      // Publicar evento MQTT
      publishDigitalInputEvent(inputNumber, relay, duration_ms);
    }
    
    // Detectar flanco de bajada (HIGH → LOW)
    if (state.lastState && !state.currentState) {
      Serial.printf("📍 [DI%d] Modo NORMAL - LOW detectado → Sistema listo para nuevo pulso\n", inputNumber);
      state.waitingForLow = false;
    }
  }
  // MODO INVERSO: Relé siempre activo, HIGH lo desactiva
  else {
    int relayPin = (relay == 1) ? RELE1_PIN : RELE2_PIN;
    
    // Estado LOW → Relé debe estar activo
    if (!state.currentState) {
      if (!state.relayActivated) {
        Serial.printf("🔵 [DI%d] Modo INVERSO - LOW detectado → Activando Relé %d (permanente)\n", 
                      inputNumber, relay);
        digitalWrite(relayPin, HIGH);
        state.relayActivated = true;
        state.waitingForLow = false;
        
        // Publicar evento MQTT solo en cambios de estado
        if (state.lastState != state.currentState) {
          publishDigitalInputEvent(inputNumber, relay, 0xFFFFFFFF);  // Indica modo permanente
        }
      }
    }
    // Estado HIGH → Relé debe estar desactivado
    else {
      if (state.relayActivated || !state.waitingForLow) {
        Serial.printf("🔴 [DI%d] Modo INVERSO - HIGH detectado → Desactivando Relé %d\n", 
                      inputNumber, relay);
        digitalWrite(relayPin, LOW);
        state.relayActivated = false;
        state.waitingForLow = true;
        
        // Publicar evento MQTT solo en cambios de estado
        if (state.lastState != state.currentState) {
          publishDigitalInputEvent(inputNumber, relay, 0);  // 0 indica desactivación
        }
      }
    }
  }
  
  // Actualizar último estado para la próxima lectura
  state.lastState = state.currentState;
}

// Publicar evento de entrada digital vía MQTT
void publishDigitalInputEvent(int inputNumber, int relay, uint32_t duration_ms) {
  if (!mqttClient.connected()) return;
  
  DynamicJsonDocument doc(256);
  doc["timestamp"] = getTimestamp();
  doc["event"] = "digital_input_trigger";
  doc["input"] = inputNumber;
  doc["gpio"] = (inputNumber == 1) ? DI1_PIN : DI2_PIN;
  doc["relay"] = relay;
  doc["duration_ms"] = duration_ms;
  doc["duration"] = duration_ms / 1000.0f;  // También en segundos para compatibilidad
  doc["message_id"] = String(messageId++);
  
  String output;
  serializeJson(doc, output);
  String topic = "swatidhome/" + fixedSerialNumber + "/digital_input";
  
  bool published = mqttClient.publish(topic.c_str(), output.c_str());
  if (!published) {
    Serial.printf("❌ [DI%d] Error publicando evento MQTT\n", inputNumber);
  } else {
    Serial.printf("✅ [DI%d] Evento publicado a MQTT\n", inputNumber);
  }
}

