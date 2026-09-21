#pragma once

#include <Arduino.h>
#include "eeprom_layout.h"

// ============================================================================
// digital_inputs — entradas digitales DI1/DI2 (EEPROM offset 256)
//
// Configuración persistente, procesamiento Normal/Inverso y evento MQTT.
// Extraído del monolito main.ino (Fase 0.75, tranche 2). Los handlers web
// siguen en main.ino y acceden vía esta API + los estados expuestos.
// ============================================================================

// Estado en tiempo real de una entrada digital
struct DigitalInputState {
  bool lastState;               // Último estado leído (LOW/HIGH)
  bool currentState;            // Estado actual (crudo, LOW/HIGH)
  bool relayActivated;          // Relé activado por esta entrada (pulsador)
  unsigned long activationTime; // Momento de activación (pulsador)
  bool waitingForLow;           // Esperando que el pulso baje (pulsador)
  // --- tipo imán/puerta (v5.0.2) ---
  bool doorKnown;               // Ya hay estado lógico estable (tras debounce)
  bool doorOpen;                // Estado lógico actual de la puerta
  uint8_t rawLast;              // Último nivel crudo visto (debounce)
  unsigned long rawStableSince; // Desde cuándo es estable el nivel crudo
  bool needSync;                // Publicar estado al (re)conectar MQTT
  bool bootDone;                // Primer sync enviado (reason boot vs sync)
};

extern DigitalInputConfig digitalInputConfig;
extern DigitalInputState di1State;
extern DigitalInputState di2State;

uint32_t calculateDIChecksum(const DigitalInputConfig& cfg);
void loadDigitalInputConfig();
void saveDigitalInputConfig();

// Procesa ambas entradas según su tipo configurado (llamar desde loop):
//   pulsador → lógica Normal/Inverso clásica (relé + digital_input_trigger)
//   imán     → supervisión con debounce; door_contact en cada cambio
void digitalInputsLoop();

// Lógica clásica de pulsador (usada internamente; se mantiene exportada)
void processDigitalInput(int inputNumber, DigitalInputState& state, uint8_t enabled,
                         uint8_t relay, uint32_t duration_ms, uint8_t inverse);

void publishDigitalInputEvent(int inputNumber, int relay, uint32_t duration_ms);

// Evento de contacto de puerta (contrato NUEVO, distinto de trigger)
void publishDoorContactEvent(int inputNumber, bool doorOpen, int rawLevel,
                             uint8_t openLevel, const char* reason);
