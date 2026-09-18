#pragma once

#include <Arduino.h>
#include "target_features.h"

// ============================================================================
// status_display — OLED SSD1306 128×64 (A2v3): información clave en pantalla
//
//   fila 0  fecha + hora (RTC/sistema)
//   fila 1  E: IP Ethernet (o "sin enlace")
//   fila 2  W: IP WiFi STA (o estado)
//   fila 3  AP: on/off + nº clientes + segundos restantes
//   fila 4  4G: estado FSM + CSQ
//   fila 5  R1/R2: estado de los relés
//   fila 6  D1/D2: entradas digitales + MQTT ok/--
//   fila 7  id del equipo
//
// Detección runtime en 0x3C (bus I2C compartido con el DS3231; requiere
// Wire.begin() previo — lo hace rtcTimeBegin). Sin panel: un log y no-op.
// Refresco 1 Hz desde loop(), ~2-3 ms por I2C.
// ============================================================================

#if A2_FEATURE_LCD

void statusDisplayBegin();
void statusDisplayLoop();
bool statusDisplayPresent();

#else

inline void statusDisplayBegin() {}
inline void statusDisplayLoop() {}
inline bool statusDisplayPresent() { return false; }

#endif  // A2_FEATURE_LCD
