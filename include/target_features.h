#pragma once

// ============================================================================
// FEATURE FLAGS POR TARGET (plan v5.0.0 — dual-target A2 / A2v3)
//
// Valores por defecto para el A2 clásico; cada entorno de platformio.ini
// puede sobreescribirlos con -D<FLAG>=<valor> en build_flags.
//
//   A2_FEATURE_WIFI_MGMT  WiFi de gestión (AP de emergencia hoy; AP/STA
//                         completo con wifi_manager en Fase 1)
//   A2_FEATURE_GSM_MODEM  Módem celular SIM7600E (Fase 3; stub hasta entonces)
//   A2_FEATURE_ETHERNET   Ethernet (LAN8720 en A2, W5500 en A2v3)
//   A2_BOARD_A2V3         Placa KC868-A2v3 (ESP32-S3) en lugar de A2 clásico
// ============================================================================

#ifndef A2_FEATURE_WIFI_MGMT
#define A2_FEATURE_WIFI_MGMT 1
#endif

#ifndef A2_FEATURE_GSM_MODEM
#define A2_FEATURE_GSM_MODEM 0
#endif

#ifndef A2_FEATURE_ETHERNET
#define A2_FEATURE_ETHERNET 1
#endif

#ifndef A2_BOARD_A2V3
#define A2_BOARD_A2V3 0
#endif

// Pantalla OLED SSD1306 128x64 (bus I2C de placa; solo A2v3)
#ifndef A2_FEATURE_LCD
#define A2_FEATURE_LCD 0
#endif

// RTC DS3231 (bus I2C de placa; solo A2v3) — hora propia mantenida
#ifndef A2_FEATURE_RTC
#define A2_FEATURE_RTC 0
#endif
