#pragma once

#include <Arduino.h>
#include "eeprom_layout.h"

// ============================================================================
// local_codes — códigos de acceso locales (EEPROM offset 512)
//
// Almacenamiento y validación de PIN/TAG locales (funcionan sin red).
// Extraído del monolito main.ino (Fase 0.75, tranche 1). Los handlers web,
// MQTT y BLE siguen en main.ino y acceden vía esta API + storedCodes.
// ============================================================================

// Buffer en RAM (malloc en initializeStoredCodes)
extern StoredCodes* storedCodes;

void initializeStoredCodes();
void loadStoredCodes();
void saveStoredCodes();

// keyboardId: 0 = válido en ambos teclados
bool addCode(const char* type, const char* value, int keyboardId, int relay);
bool addCode(const char* type, const char* value, int relay);  // keyboardId=0

bool isCodeStored(const char* type, const char* value, int keyboardId, int* relay);
bool isCodeStored(const char* type, const char* value, int* relay);
bool isCodeStored(const char* type, const char* value);
