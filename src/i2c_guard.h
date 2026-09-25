#pragma once

#include <Arduino.h>

// ============================================================================
// i2c_guard — coexistencia del bus I2C con el Teclado Wiegand 2 (A2v3)
//
// En el A2v3 el conector I2C (SDA=48, SCL=47) es el único punto de conexión
// libre para el segundo teclado Wiegand (como en el A2 clásico, donde ese
// mismo conector expone GPIO4/16 = sus pines Wiegand2 nativos). La diferencia
// es que en el A2v3 el bus lleva tráfico real (SSD1306, DS3231, 24C02), así
// que cuando bornes_mode = 2 (teclado 2 en I2C) hay que separar en el tiempo
// ambos usos:
//
//  - i2cSectionBegin()/i2cSectionEnd(): envuelven CADA transacción I2C del
//    firmware. Silencian los ISRs Wiegand2 (los flancos de SCL/SDA generados
//    por la propia transacción no se cuentan como bits) y descartan cualquier
//    trama parcial que quedara a medias.
//  - i2cQuietForWiegand(): false mientras el teclado 2 está transmitiendo o
//    lo ha hecho hace poco. El que quiera usar el bus (render LCD, escritura
//    RTC) debe posponer su transacción para no corromper ni la trama del
//    teclado ni la suya propia.
//
// Con bornes_mode != 2 todas estas funciones son inocuas (no-op lógico).
// Definidas en main.ino (necesitan el estado wiegand2*).
// ============================================================================

void i2cSectionBegin();
void i2cSectionEnd();
bool i2cQuietForWiegand();

// Re-engancha los ISRs del Teclado 2 en SDA/SCL si bornes_mode=2. Necesario
// tras cualquier reconfiguración de los pines 48/47 (i2cRuntimeRecover:
// pinMode/Wire.begin resetean el tipo de interrupción del GPIO).
void wiegand2ReattachIfI2C();
