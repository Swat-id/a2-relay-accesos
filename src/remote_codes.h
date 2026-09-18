#pragma once

#include <Arduino.h>
#include "eeprom_layout.h"

// ============================================================================
// remote_codes — caché local de códigos de validación remota
//
// Almacenamiento (NVS "a2acc_rc"), altas/bajas y validación con franjas
// horarias. Primer módulo funcional extraído del monolito main.ino
// (Fase 0.75). Las rutas web y los comandos MQTT que lo usan siguen en
// main.ino y acceden vía esta API + el puntero storedRemoteCodes.
// ============================================================================

// Buffer en RAM (malloc en initializeStoredRemoteCodes). Los handlers web y
// MQTT lo recorren directamente para listados.
extern StoredRemoteCodes* storedRemoteCodes;

void initializeStoredRemoteCodes();
void loadStoredRemoteCodes();
void saveStoredRemoteCodes();

bool addRemoteCode(const char* type, const char* value, uint8_t keyboardId,
                   uint8_t relay, const TimeSlot* timeSlots, uint8_t timeSlotsCount);
bool deleteRemoteCode(const char* type, const char* value);
void deleteAllRemoteCodes();

// Validación (aplica franjas horarias si el código las tiene)
bool isRemoteCodeStored(const char* type, const char* value, uint8_t keyboardId,
                        uint8_t* relay);

bool isTimeSlotValid(const TimeSlot& timeSlot);
bool isCurrentTimeInTimeSlots(const TimeSlot* timeSlots, uint8_t timeSlotsCount);
String getDaysOfWeekString(uint8_t days_of_week);
