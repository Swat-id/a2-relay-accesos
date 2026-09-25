#pragma once

#include <Arduino.h>
#include "target_features.h"

// ============================================================================
// rtc_time — RTC DS3231 (A2v3): el equipo mantiene su propia hora
//
// - Al arrancar: si el DS3231 responde y su hora es válida (sin OSF), siembra
//   el reloj del sistema → getLocalTime()/franjas horarias funcionan sin red.
// - Cuando el sistema obtiene hora buena (SNTP al llegar IP, o sincronización
//   MQTT), se reescribe el RTC periódicamente para mantenerlo en hora.
// - El RTC guarda HORA LOCAL (criterio del equipo, coherente con la web/LCD).
//
// I2C de placa (SDA=48, SCL=47), dirección 0x68. Sin librerías externas.
// ============================================================================

#if A2_FEATURE_RTC

// Inicializa I2C, detecta el RTC y siembra el reloj del sistema si procede.
void rtcTimeBegin();

// Mantiene el RTC en hora cuando el sistema tiene hora fiable (llamar en loop)
void rtcTimeLoop();

bool rtcPresent();
bool rtcOscStopped();           // OSF: pila agotada / hora no fiable

// Escribe el RTC (y el reloj del sistema) desde hora local explícita.
// Formato "YYYY-MM-DD HH:MM:SS". Para la sincronización MQTT.
bool rtcSetFromLocalString(const String& localTime);

String rtcStatusJson();

// Recuperación del bus I2C en CALIENTE (Wire ya inicializado): reinicia el
// driver, libera un esclavo que retenga SDA y vuelve a inicializar Wire.
// Para cuando el bus se corrompe en runtime (p.ej. colisión con el Teclado
// Wiegand 2 compartiendo SDA/SCL, bornes_mode=2). Devuelve true si el bus
// quedó libre (SDA y SCL en alto).
bool i2cRuntimeRecover();

// true mientras el bus I2C esté operativo. Si el bus queda retenido (fallo
// eléctrico, esclavo colgado irrecuperable) se marca muerto: RTC y LCD se
// desactivan y el RESTO de servicios (accesos, relés, web, MQTT, red) siguen
// funcionando con total normalidad.
bool i2cBusOk();

#else

inline void rtcTimeBegin() {}
inline void rtcTimeLoop() {}
inline bool rtcPresent() { return false; }
inline bool rtcOscStopped() { return false; }
inline bool rtcSetFromLocalString(const String&) { return false; }
inline String rtcStatusJson() { return String("{\"present\":false}"); }
inline bool i2cRuntimeRecover() { return false; }
inline bool i2cBusOk() { return true; }

#endif  // A2_FEATURE_RTC
