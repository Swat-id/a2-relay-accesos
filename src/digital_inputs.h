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
  bool currentState;            // Estado actual
  bool relayActivated;          // Relé activado por esta entrada
  unsigned long activationTime; // Momento de activación
  bool waitingForLow;           // Esperando que el pulso baje
};

extern DigitalInputConfig digitalInputConfig;
extern DigitalInputState di1State;
extern DigitalInputState di2State;

uint32_t calculateDIChecksum(const DigitalInputConfig& cfg);
void loadDigitalInputConfig();
void saveDigitalInputConfig();

// Procesa una entrada (llamar desde loop). Modo normal: pulso HIGH activa el
// relé por duration_ms; modo inverso: relé mantenido activo mientras LOW.
void processDigitalInput(int inputNumber, DigitalInputState& state, uint8_t enabled,
                         uint8_t relay, uint32_t duration_ms, uint8_t inverse);

void publishDigitalInputEvent(int inputNumber, int relay, uint32_t duration_ms);
