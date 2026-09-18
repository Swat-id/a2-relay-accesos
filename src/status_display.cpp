#include "status_display.h"

#if A2_FEATURE_LCD

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ETH.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "hw_config.h"
#include "net_manager.h"
#include "wifi_manager.h"
#include "gsm_modem.h"
#include "digital_inputs.h"

// Servicios de main.ino
extern PubSubClient mqttClient;
extern String fixedSerialNumber;
#define gmtOffset_sec TIME_GMT_OFFSET_SEC
#define daylightOffset_sec TIME_DST_OFFSET_SEC

static Adafruit_SSD1306* s_oled = nullptr;
static bool s_present = false;
static unsigned long s_lastRender = 0;

void statusDisplayBegin() {
  // Wire.begin ya hecho por rtcTimeBegin (mismo bus); repetirlo es inocuo
  Wire.begin(BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);

  Wire.beginTransmission(LCD_SSD1306_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("🖥️ [LCD] SSD1306 no detectado (0x3C) - pantalla desactivada");
    return;
  }

  s_oled = new Adafruit_SSD1306(128, 64, &Wire, -1);
  if (!s_oled->begin(SSD1306_SWITCHCAPVCC, LCD_SSD1306_I2C_ADDR)) {
    Serial.println("🖥️ [LCD] Fallo inicializando SSD1306");
    delete s_oled;
    s_oled = nullptr;
    return;
  }
  s_present = true;

  s_oled->clearDisplay();
  s_oled->setTextSize(1);
  s_oled->setTextColor(SSD1306_WHITE);
  s_oled->setCursor(10, 24);
  s_oled->print("SWATID A2v3");
  s_oled->setCursor(10, 36);
  s_oled->print("Arrancando...");
  s_oled->display();
  Serial.println("🖥️ [LCD] SSD1306 128x64 inicializado");
}

bool statusDisplayPresent() { return s_present; }

// Línea de 21 caracteres como máximo (fuente 6x8)
static void line(int row, const char* text) {
  s_oled->setCursor(0, row * 8);
  s_oled->print(text);
}

void statusDisplayLoop() {
  if (!s_present) return;
  unsigned long now = millis();
  if (now - s_lastRender < 1000) return;   // 1 Hz
  s_lastRender = now;

  char buf[24];
  s_oled->clearDisplay();
  s_oled->setTextSize(1);
  s_oled->setTextColor(SSD1306_WHITE);

  // fila 0: fecha + hora local
  time_t t = time(nullptr);
  if (t > 1700000000) {
    time_t local = t + gmtOffset_sec + daylightOffset_sec;
    struct tm ti;
    gmtime_r(&local, &ti);
    snprintf(buf, sizeof(buf), "%02d/%02d/%02d  %02d:%02d:%02d",
             ti.tm_mday, ti.tm_mon + 1, ti.tm_year % 100,
             ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    snprintf(buf, sizeof(buf), "--/--/--  --:--:--");
  }
  line(0, buf);

  // fila 1: Ethernet
  if (netEthUp()) {
    snprintf(buf, sizeof(buf), "E:%s", ETH.localIP().toString().c_str());
  } else {
    snprintf(buf, sizeof(buf), "E:sin enlace");
  }
  line(1, buf);

  // fila 2: WiFi STA
  if (wifiStaConnected()) {
    snprintf(buf, sizeof(buf), "W:%s", WiFi.localIP().toString().c_str());
  } else {
    snprintf(buf, sizeof(buf), "W:--");
  }
  line(2, buf);

  // fila 3: AP + clientes (+ segundos restantes de la ventana)
  if (wifiApActive()) {
    int rem = wifiApRemainingS();
    if (rem >= 0) {
      snprintf(buf, sizeof(buf), "AP:ON %dcli %ds", wifiApClients(), rem);
    } else {
      snprintf(buf, sizeof(buf), "AP:ON %dcli fijo", wifiApClients());
    }
  } else {
    snprintf(buf, sizeof(buf), "AP:off");
  }
  line(3, buf);

  // fila 4: 4G
  {
    const GsmStatus& g = gsmModemStatus();
    if (g.csq >= 0) {
      snprintf(buf, sizeof(buf), "4G:%s CSQ%d", gsmStateName(g.state), g.csq);
    } else {
      snprintf(buf, sizeof(buf), "4G:%s", gsmStateName(g.state));
    }
  }
  line(4, buf);

  // fila 5: relés (lectura real del pin)
  snprintf(buf, sizeof(buf), "R1:%s  R2:%s",
           digitalRead(RELE1_PIN) ? "ON " : "off",
           digitalRead(RELE2_PIN) ? "ON " : "off");
  line(5, buf);

  // fila 6: entradas digitales + MQTT
  snprintf(buf, sizeof(buf), "D1:%c D2:%c  M:%s",
           di1State.currentState ? 'H' : 'L',
           di2State.currentState ? 'H' : 'L',
           mqttClient.connected() ? "ok" : "--");
  line(6, buf);

  // fila 7: id del equipo
  line(7, fixedSerialNumber.c_str());

  s_oled->display();
}

#endif  // A2_FEATURE_LCD
