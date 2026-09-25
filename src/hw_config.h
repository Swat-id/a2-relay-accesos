#pragma once

#include <ETH.h>
#include "target_features.h"

// ============================================================================
// CONFIGURACIÓN DE HARDWARE — dual target (plan v5.0.0 Fase 2)
//
//   A2 clásico : KC868-A2  — ESP32-WROOM + LAN8720 (RMII)
//   A2v3       : KC868-A2v3 — ESP32-S3 N16R8 + W5500 (SPI) + DS3231 + SSD1306
//
// Pines A2v3 según KinCony (foro tid=7958) y verificación en hardware del
// proyecto SWATID-A2-MODBUS. Ver docs/v5.0.0/MATRIZ-HARDWARE-A2-A2V3.md.
// ============================================================================

#if A2_BOARD_A2V3
// ============================ KC868-A2v3 (ESP32-S3) =========================
#define HW_BOARD_NAME "KC868-A2v3 (ESP32-S3)"

// ----------------------------- Relés (activo HIGH) -------------------------
const int RELE1_PIN = 40;
const int RELE2_PIN = 39;

// ------------------------- Teclados Wiegand --------------------------------
// MAPEADO EN PLACA REAL con sniffer de flancos (v5.0.2, 25-Sep-2026):
//   borne PCB "GPIO1" → chip GPIO18   (Teclado 1 D0)
//   borne PCB "GPIO2" → chip GPIO8    (Teclado 1 D1)
// Verificado tecleando 1111# : 13 flancos D0 / 7 flancos D1 = exacto.
// (El rotulado del PCB NO coincide con la lista "free gpio" del foro
// tid=7958; los chips 18/8 son los que MODBUS llamaba OneWire TMP1/TMP2.)
// Teclado 2 (v5.0.2): pines DEDICADOS, verificados con sniffer de flancos en
// placa real (25-Sep-2026): conectores PCB "IO4"/"IO5" → chips GPIO4/GPIO5.
// Tecleando 1111#: 13 flancos en IO4 (D0) / 7 en IO5 (D1) = firma exacta.
// El conector rotulado "SDA/SCL/GND/3V3" NO está conectado al MCU en esta
// revisión (verificado con toques a GND sobre 25 GPIOs vigilados): el bus
// I2C real (48/47) queda íntegro para LCD/RTC/24C02. Libres: IO6 e IO38.
// Nota: sin pull-up en PCB en IO4/IO5 — el firmware usa INPUT_PULLUP interno.
#define WIEGAND1_D0 18
#define WIEGAND1_D1 8
#define WIEGAND2_D0 4    // conector "IO4"
#define WIEGAND2_D1 5    // conector "IO5"
#define WIEGAND2_SHARES_DI 0   // teclado 2 con pines propios (como el A2 clásico)

// ------------------------ RS485 (MAX485 on-board) --------------------------
#define RS485_RX2 15
#define RS485_TX2 7
#define RS485_BAUD 9600
#define RS485_UART_NUM 1        // UART1 (la UART2 es del socket GSM en A2v3)

// ---------------------- Ethernet W5500 (SPI dedicado) ----------------------
#define W5500_CS_PIN   41
#define W5500_INT_PIN  2
#define W5500_RST_PIN  1
#define W5500_SCK_PIN  42
#define W5500_MISO_PIN 44
#define W5500_MOSI_PIN 43

// ---------------------- Entradas digitales ---------------------------------
const int DI1_PIN = 16;
const int DI2_PIN = 17;

// ---------------------- Socket GSM 4G (SIM7600E) ---------------------------
// Mapeo verificado en A2v3 real; swap auto-detectado y persistido en NVS
#define GSM_TX_PIN 10           // ESP TX -> RXD módulo
#define GSM_RX_PIN 9            // ESP RX <- TXD módulo
#define GSM_UART_NUM 2
#define GSM_SWAP_POSSIBLE 1

// ---------------------- I2C de placa (DS3231 + SSD1306 + 24C02) ------------
#define BOARD_I2C_SDA_PIN 48
#define BOARD_I2C_SCL_PIN 47
#define RTC_DS3231_I2C_ADDR  0x68
#define LCD_SSD1306_I2C_ADDR 0x3C

#else
// ============================ KC868-A2 clásico (ESP32) ======================
#define HW_BOARD_NAME "KC868-A2 (ESP32)"

// ----------------------------- Relés ---------------------------------------
const int RELE1_PIN = 15;
const int RELE2_PIN = 2;

// ------------------------- Teclados Wiegand --------------------------------
#define WIEGAND1_D0 33
#define WIEGAND1_D1 14
#define WIEGAND2_D0 4
#define WIEGAND2_D1 16

// ------------------------ RS485 (compatibilidad) ---------------------------
#define RS485_RX2 35
#define RS485_TX2 32
#define RS485_BAUD 9600
#define RS485_UART_NUM 2        // UART2 (histórico A2)

// ---------------------- Ethernet LAN8720 (RMII) ----------------------------
#define ETH_PHY_ADDR 0
#define ETH_PHY_MDC 23
#define ETH_PHY_MDIO 18
#define ETH_PHY_POWER_PIN 5
#define ETH_PHY_TYPE ETH_PHY_LAN8720
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT

// ---------------------- Entradas digitales ---------------------------------
const int DI1_PIN = 36;  // input only, sin pull-up interna
const int DI2_PIN = 39;  // input only, sin pull-up interna

// ---------------------- Socket GSM 4G (SIM800L/SIM7600) --------------------
// GPIO34 es solo entrada: el swap TX/RX es imposible en este target
#define GSM_TX_PIN 13
#define GSM_RX_PIN 34
#define GSM_UART_NUM 1          // UART1 (la UART2 la ocupa el RS485)
#define GSM_SWAP_POSSIBLE 0
#define WIEGAND2_SHARES_DI 0    // A2: teclado 2 tiene bornes propios (4/16)

#endif  // A2_BOARD_A2V3

// ------------------------- Temporizaciones de red --------------------------
// Timeout de conexión del socket MQTT (segundos)
#define MQTT_CONNECT_TIMEOUT_SEC 5

// Offsets horarios legacy (configTime): hora local = UTC + GMT + DST
#define TIME_GMT_OFFSET_SEC 3600
#define TIME_DST_OFFSET_SEC 3600
