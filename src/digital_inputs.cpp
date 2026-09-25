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
DigitalInputState di1State = {};
DigitalInputState di2State = {};

// Debounce del contacto magnético (imanes/reed con rebote mecánico)
#define DOOR_DEBOUNCE_MS 50

static const char* diTypeName(uint8_t t) {
  return (t == DI_TYPE_DOOR) ? "IMAN/PUERTA" : "PULSADOR";
}

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
  // v5.0.2: tipos y polaridad de puerta también protegidos
  sum += (uint32_t)cfg.di1_type << 24;
  sum += (uint32_t)cfg.di2_type << 25;
  sum += (uint32_t)cfg.di1_open_level << 26;
  sum += (uint32_t)cfg.di2_open_level << 27;
  sum += (uint32_t)cfg.bornes_mode << 28;
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
    digitalInputConfig.di1_type = DI_TYPE_BUTTON;
    digitalInputConfig.di1_duration_ms = 2000;
    digitalInputConfig.di2_enabled = 0;
    digitalInputConfig.di2_relay = 2;
    digitalInputConfig.di2_inverse = 0;
    digitalInputConfig.di2_type = DI_TYPE_BUTTON;
    digitalInputConfig.di2_duration_ms = 2000;
    digitalInputConfig.di1_open_level = 0;
    digitalInputConfig.di2_open_level = 0;
    digitalInputConfig.bornes_mode = DI_BORNES_DIGITAL;
    digitalInputConfig.checksum = 0;

    saveDigitalInputConfig();
    Serial.println("✅ Configuración DI inicializada correctamente");
    return;
  }

  // Sanear tipos (el byte era 'reserved'=0 en v1 → pulsador por defecto)
  if (digitalInputConfig.di1_type > DI_TYPE_DOOR) digitalInputConfig.di1_type = DI_TYPE_BUTTON;
  if (digitalInputConfig.di2_type > DI_TYPE_DOOR) digitalInputConfig.di2_type = DI_TYPE_BUTTON;

  // Migración v1 → v2: el bloque nuevo (open_level) aún no existe en EEPROM
  if (digitalInputConfig.v2_marker != DI_CONFIG_V2_MARKER) {
    Serial.println("🔧 [DI] Migrando configuración v1 → v2 (tipos pulsador, open_level=LOW)");
    digitalInputConfig.di1_open_level = 0;
    digitalInputConfig.di2_open_level = 0;
    digitalInputConfig.bornes_mode = DI_BORNES_DIGITAL;
    saveDigitalInputConfig();   // escribe v2_marker y checksum ampliado
  } else {
    if (digitalInputConfig.di1_open_level > 1) digitalInputConfig.di1_open_level = 0;
    if (digitalInputConfig.di2_open_level > 1) digitalInputConfig.di2_open_level = 0;
    if (digitalInputConfig.bornes_mode > DI_BORNES_TECLADO2_I2C) digitalInputConfig.bornes_mode = DI_BORNES_DIGITAL;
#if !WIEGAND2_SHARES_DI
    // Target con teclado 2 en pines propios: el selector de bornes no aplica.
    // Sanear restos de configuraciones anteriores (DI siempre operativas).
    digitalInputConfig.bornes_mode = DI_BORNES_DIGITAL;
#endif
    // Checksum ampliado: si no cuadra, autocorregir sin perder configuración
    if (digitalInputConfig.checksum != calculateDIChecksum(digitalInputConfig)) {
      Serial.println("⚠️ [DI] Checksum no coincide - recalculando (config conservada)");
      saveDigitalInputConfig();
    }
  }

  Serial.printf("💾 Configuración DI cargada: DI1=%s(%s), DI2=%s(%s)\n",
                digitalInputConfig.di1_enabled ? "ON" : "OFF",
                diTypeName(digitalInputConfig.di1_type),
                digitalInputConfig.di2_enabled ? "ON" : "OFF",
                diTypeName(digitalInputConfig.di2_type));
}

// Guardar configuración de entradas digitales en EEPROM
void saveDigitalInputConfig() {
  digitalInputConfig.validMarker = DIGITAL_INPUT_CONFIG_MARKER;
  digitalInputConfig.v2_marker = DI_CONFIG_V2_MARKER;
  digitalInputConfig.checksum = calculateDIChecksum(digitalInputConfig);

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

// ---------------------------------------------------------------------------
// Tipo IMÁN / CONTACTO DE PUERTA (v5.0.2): solo supervisión, NUNCA relé.
// Debounce por nivel estable; mensaje door_contact en cada cambio lógico y
// sincronización del estado al (re)conectar MQTT (reason boot/sync).
// ---------------------------------------------------------------------------
static void processDoorInput(int inputNumber, DigitalInputState& state,
                             uint8_t enabled, uint8_t open_level) {
  if (!enabled) {
    state.doorKnown = false;
    state.needSync = false;
    return;
  }

  int pin = (inputNumber == 1) ? DI1_PIN : DI2_PIN;
  uint8_t raw = (uint8_t)digitalRead(pin);
  state.currentState = raw;   // crudo, para API/LCD
  state.lastState = raw;

  unsigned long now = millis();
  if (raw != state.rawLast) {        // nivel cambió: reiniciar debounce
    state.rawLast = raw;
    state.rawStableSince = now;
    return;
  }
  if (now - state.rawStableSince < DOOR_DEBOUNCE_MS) return;

  bool open = (raw == open_level);

  if (!state.doorKnown) {
    // Primer estado estable tras arranque/habilitación
    state.doorKnown = true;
    state.doorOpen = open;
    state.needSync = true;           // informar al backend en cuanto haya MQTT
    Serial.printf("🚪 [DI%d] Estado inicial de puerta: %s (nivel %s)\n",
                  inputNumber, open ? "ABIERTA" : "CERRADA", raw ? "HIGH" : "LOW");
  } else if (open != state.doorOpen) {
    state.doorOpen = open;
    Serial.printf("🚪 [DI%d] Puerta %s (nivel %s)\n",
                  inputNumber, open ? "ABIERTA" : "CERRADA", raw ? "HIGH" : "LOW");
    if (mqttClient.connected()) {
      publishDoorContactEvent(inputNumber, open, raw, open_level, "change");
    } else {
      // Sin broker: marcar para sincronizar el estado actual al reconectar
      state.needSync = true;
    }
  }

  // Sync pendiente (arranque o cambios perdidos sin broker)
  if (state.needSync && mqttClient.connected()) {
    state.needSync = false;
    publishDoorContactEvent(inputNumber, state.doorOpen, raw, open_level,
                            state.bootDone ? "sync" : "boot");
    state.bootDone = true;
  }
}

// Procesa ambas entradas según el tipo configurado
void digitalInputsLoop() {
#if WIEGAND2_SHARES_DI
  // A2v3: bornes DI1/DI2 cedidos al Teclado Wiegand 2 — sin proceso de DI
  if (digitalInputConfig.bornes_mode == DI_BORNES_WIEGAND2) return;
#endif
  if (digitalInputConfig.di1_type == DI_TYPE_DOOR) {
    processDoorInput(1, di1State, digitalInputConfig.di1_enabled,
                     digitalInputConfig.di1_open_level);
  } else {
    processDigitalInput(1, di1State, digitalInputConfig.di1_enabled,
                        digitalInputConfig.di1_relay,
                        digitalInputConfig.di1_duration_ms,
                        digitalInputConfig.di1_inverse);
  }
  if (digitalInputConfig.di2_type == DI_TYPE_DOOR) {
    processDoorInput(2, di2State, digitalInputConfig.di2_enabled,
                     digitalInputConfig.di2_open_level);
  } else {
    processDigitalInput(2, di2State, digitalInputConfig.di2_enabled,
                        digitalInputConfig.di2_relay,
                        digitalInputConfig.di2_duration_ms,
                        digitalInputConfig.di2_inverse);
  }
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

// Evento de contacto de puerta (v5.0.2). Contrato NUEVO en el mismo topic:
// event="door_contact" — nunca lleva relay/duration para que los parsers de
// digital_input_trigger existentes no lo confundan.
void publishDoorContactEvent(int inputNumber, bool doorOpen, int rawLevel,
                             uint8_t openLevel, const char* reason) {
  if (!mqttClient.connected()) return;

  DynamicJsonDocument doc(256);
  doc["timestamp"] = getTimestamp();
  doc["event"] = "door_contact";
  doc["input"] = inputNumber;
  doc["gpio"] = (inputNumber == 1) ? DI1_PIN : DI2_PIN;
  doc["door_state"] = doorOpen ? "open" : "closed";
  doc["raw_level"] = rawLevel ? "HIGH" : "LOW";
  doc["open_level"] = openLevel ? "HIGH" : "LOW";
  doc["reason"] = reason;   // "boot" | "change" | "sync"
  doc["message_id"] = String(messageId++);

  String output;
  serializeJson(doc, output);
  String topic = "swatidhome/" + fixedSerialNumber + "/digital_input";

  if (mqttClient.publish(topic.c_str(), output.c_str())) {
    Serial.printf("✅ [DI%d] door_contact publicado: %s (%s)\n",
                  inputNumber, doorOpen ? "open" : "closed", reason);
  } else {
    Serial.printf("❌ [DI%d] Error publicando door_contact\n", inputNumber);
  }
}

