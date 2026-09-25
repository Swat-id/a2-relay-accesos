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
#include "rtc_time.h"    // i2cRuntimeRecover (bus compartido con Teclado 2)
#include "i2c_guard.h"   // coexistencia bus I2C ↔ Teclado Wiegand 2

// Servicios de main.ino
extern PubSubClient mqttClient;
extern String fixedSerialNumber;
#define gmtOffset_sec TIME_GMT_OFFSET_SEC
#define daylightOffset_sec TIME_DST_OFFSET_SEC

static Adafruit_SSD1306* s_oled = nullptr;
static bool s_present = false;
static unsigned long s_lastRender = 0;
static uint8_t s_i2cFails = 0;          // fallos consecutivos de bus → recuperación
static unsigned long s_lastReinit = 0;  // último reintento de detección
static bool s_absentLogged = false;     // no repetir el log en cada reintento

void statusDisplayBegin() {
  s_present = false;
  s_i2cFails = 0;

  // Bus marcado como muerto: un intento de recuperación; si sigue retenido,
  // la pantalla queda fuera y se reintenta desde statusDisplayLoop cada 60 s.
  // El resto del sistema no depende de este resultado.
  if (!i2cBusOk() && !i2cRuntimeRecover()) {
    if (!s_absentLogged) {
      Serial.println("🖥️ [LCD] Bus I2C no funcional - pantalla desactivada (reintento cada 60 s)");
      s_absentLogged = true;
    }
    return;
  }

  // Wire.begin ya hecho por rtcTimeBegin (mismo bus); repetirlo es inocuo
  Wire.begin(BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);

  Wire.beginTransmission(LCD_SSD1306_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    if (!s_absentLogged) {
      Serial.println("🖥️ [LCD] SSD1306 no detectado (0x3C) - pantalla desactivada");
      s_absentLogged = true;
    }
    return;
  }

  if (!s_oled) s_oled = new Adafruit_SSD1306(128, 64, &Wire, -1);
  if (!s_oled->begin(SSD1306_SWITCHCAPVCC, LCD_SSD1306_I2C_ADDR)) {
    Serial.println("🖥️ [LCD] Fallo inicializando SSD1306");
    return;
  }
  s_present = true;
  s_absentLogged = false;

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
  unsigned long now = millis();

  // Pantalla ausente o suspendida: reintentar la detección cada 60 s por si
  // el bus/el OLED vuelven (recuperación eléctrica, recableado…). Nunca
  // bloquea: un reintento fallido cuesta unos ms una vez por minuto.
  if (!s_present) {
    if (now - s_lastReinit < 60000) return;
    s_lastReinit = now;
    if (!i2cQuietForWiegand()) return;
    i2cSectionBegin();
    statusDisplayBegin();
    i2cSectionEnd();
    return;   // si ha vuelto, el siguiente ciclo ya renderiza
  }

  if (now - s_lastRender < 1000) return;   // 1 Hz

  // Teclado Wiegand 2 compartiendo SDA/SCL (bornes_mode=2): no usar el bus
  // mientras el teclado transmite ni justo después (no "comerse" una tecla).
  // Se reintenta en la siguiente pasada; el reloj queda congelado unos
  // segundos durante el tecleo, nada más.
  if (!i2cQuietForWiegand()) return;
  s_lastRender = now;

  // Silenciar los ISRs del teclado 2 durante NUESTRO tráfico I2C y sondear
  // la salud del bus (una colisión puede dejar un esclavo colgado)
  i2cSectionBegin();
  Wire.beginTransmission(LCD_SSD1306_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    s_i2cFails++;
    // A los 3 fallos, un intento de recuperación; si no libera el bus o los
    // fallos persisten, SUSPENDER la pantalla (reintento cada 60 s) en vez de
    // reintentar en bucle: el resto de servicios no se ve afectado.
    bool suspend = false;
    if (s_i2cFails == 3) {
      suspend = !i2cRuntimeRecover();
    } else if (s_i2cFails >= 6) {
      suspend = true;
    }
    if (suspend) {
      Serial.println("🖥️ [LCD] Pantalla suspendida por fallos de bus I2C - reintento cada 60 s");
      s_present = false;
      s_i2cFails = 0;
      s_lastReinit = now;
    }
    i2cSectionEnd();
    return;
  }
  s_i2cFails = 0;

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
  // Pulsador: nivel H/L. Imán de puerta: A (abierta) / C (cerrada) / ? (aún sin estado)
#if WIEGAND2_SHARES_DI
  if (digitalInputConfig.bornes_mode == DI_BORNES_WIEGAND2) {
    // Bornes DI cedidos al segundo teclado Wiegand
    snprintf(buf, sizeof(buf), "DI:teclado2  M:%s",
             mqttClient.connected() ? "ok" : "--");
    line(6, buf);
    line(7, fixedSerialNumber.c_str());
    s_oled->display();
    i2cSectionEnd();
    return;
  }
#endif
  char d1, d2;
  if (digitalInputConfig.di1_type == DI_TYPE_DOOR) {
    d1 = !di1State.doorKnown ? '?' : (di1State.doorOpen ? 'A' : 'C');
  } else {
    d1 = di1State.currentState ? 'H' : 'L';
  }
  if (digitalInputConfig.di2_type == DI_TYPE_DOOR) {
    d2 = !di2State.doorKnown ? '?' : (di2State.doorOpen ? 'A' : 'C');
  } else {
    d2 = di2State.currentState ? 'H' : 'L';
  }
  snprintf(buf, sizeof(buf), "D1:%c D2:%c  M:%s", d1, d2,
           mqttClient.connected() ? "ok" : "--");
  line(6, buf);

  // fila 7: id del equipo
  line(7, fixedSerialNumber.c_str());

  s_oled->display();
  i2cSectionEnd();
}

#endif  // A2_FEATURE_LCD
