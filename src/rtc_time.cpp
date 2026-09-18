#include "rtc_time.h"

#if A2_FEATURE_RTC

#include <Wire.h>
#include <sys/time.h>
#include <time.h>
#include "hw_config.h"

// Offsets legacy del firmware (hw_config.h): hora local = epoch + GMT + DST.
// El RTC guarda hora local.
#define gmtOffset_sec TIME_GMT_OFFSET_SEC
#define daylightOffset_sec TIME_DST_OFFSET_SEC

static bool s_present = false;
static bool s_osf = false;
static bool s_seededFromRtc = false;
static unsigned long s_lastWriteMs = 0;

#define RTC_WRITE_PERIOD_MS (6UL * 3600UL * 1000UL)  // refresco cada 6 h

static uint8_t bcdToBin(uint8_t v) { return (uint8_t)(v - 6 * (v >> 4)); }
static uint8_t binToBcd(uint8_t v) { return (uint8_t)(v + 6 * (v / 10)); }

// Conversión tm→epoch tratando el tm como "UTC plano" (independiente de la TZ
// del sistema, que cambia cuando main llama a configTime). Algoritmo civil.
static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}
static time_t tmToEpochUtc(const struct tm* t) {
  return (time_t)(daysFromCivil(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday) * 86400LL
                  + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec);
}

static bool rtcReadReg(uint8_t reg, uint8_t* val) {
  Wire.beginTransmission(RTC_DS3231_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)RTC_DS3231_I2C_ADDR, (uint8_t)1) != 1) return false;
  *val = Wire.read();
  return true;
}

static bool rtcReadTime(struct tm* out) {
  Wire.beginTransmission(RTC_DS3231_I2C_ADDR);
  Wire.write(0);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)RTC_DS3231_I2C_ADDR, (uint8_t)7) != 7) return false;
  uint8_t ss = Wire.read(), mm = Wire.read(), hh = Wire.read();
  Wire.read();  // día de semana (no usado)
  uint8_t d = Wire.read(), mo = Wire.read(), yr = Wire.read();
  out->tm_sec  = bcdToBin(ss & 0x7F);
  out->tm_min  = bcdToBin(mm & 0x7F);
  out->tm_hour = bcdToBin(hh & 0x3F);  // modo 24 h (por defecto en KinCony)
  out->tm_mday = bcdToBin(d & 0x3F);
  out->tm_mon  = (int)bcdToBin(mo & 0x1F) - 1;
  out->tm_year = (int)bcdToBin(yr) + 100;  // 20xx
  out->tm_isdst = 0;
  return true;
}

static bool rtcWriteTime(const struct tm* ti) {
  Wire.beginTransmission(RTC_DS3231_I2C_ADDR);
  Wire.write(0);
  Wire.write(binToBcd((uint8_t)ti->tm_sec));
  Wire.write(binToBcd((uint8_t)ti->tm_min));
  Wire.write(binToBcd((uint8_t)ti->tm_hour));       // 24 h
  // Día de semana calculado del calendario (tm_wday puede venir sin rellenar)
  {
    int64_t days = daysFromCivil(ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
    uint8_t wday = (uint8_t)((days + 4) % 7);       // 1970-01-01 = jueves
    Wire.write(binToBcd((uint8_t)(wday + 1)));      // DS3231: 1-7
  }
  Wire.write(binToBcd((uint8_t)ti->tm_mday));
  Wire.write(binToBcd((uint8_t)(ti->tm_mon + 1)));
  Wire.write(binToBcd((uint8_t)(ti->tm_year - 100)));
  if (Wire.endTransmission() != 0) return false;

  // Limpiar OSF (registro de estado 0x0F, bit 7)
  uint8_t st;
  if (rtcReadReg(0x0F, &st)) {
    Wire.beginTransmission(RTC_DS3231_I2C_ADDR);
    Wire.write(0x0F);
    Wire.write(st & ~0x80);
    Wire.endTransmission();
  }
  s_osf = false;
  return true;
}

// Hora local del sistema (según offsets legacy)
static bool systemLocalTime(struct tm* out) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // sistema aún sin hora real (~2023-11)
  time_t local = now + gmtOffset_sec + daylightOffset_sec;
  gmtime_r(&local, out);
  return true;
}

// Recuperación del bus I2C: tras un reset por software del MCU, un esclavo
// (DS3231/SSD1306) puede quedarse a mitad de transacción reteniendo SDA en
// bajo y el bus queda colgado (verificado en placa A2v3). Se generan hasta 9
// pulsos de SCL y una condición STOP antes de inicializar Wire.
static void i2cBusRecover(int sda, int scl) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delayMicroseconds(10);
  if (digitalRead(sda) == HIGH) return;   // bus libre

  Serial.println("🕐 [RTC] Bus I2C retenido - ejecutando recuperación");
  pinMode(scl, OUTPUT_OPEN_DRAIN);
  for (int i = 0; i < 9 && digitalRead(sda) == LOW; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(10);
    digitalWrite(scl, HIGH);
    delayMicroseconds(10);
  }
  // Condición STOP: SDA de bajo a alto con SCL alto
  pinMode(sda, OUTPUT_OPEN_DRAIN);
  digitalWrite(sda, LOW);
  delayMicroseconds(10);
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);
  digitalWrite(sda, HIGH);
  delayMicroseconds(10);
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
}

void rtcTimeBegin() {
  i2cBusRecover(BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
  Wire.begin(BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);

  uint8_t st;
  s_present = rtcReadReg(0x0F, &st);
  if (!s_present) {
    Serial.println("🕐 [RTC] DS3231 no detectado en el bus I2C");
    return;
  }
  s_osf = (st & 0x80) != 0;

  struct tm ti;
  if (!s_osf && rtcReadTime(&ti) && ti.tm_year >= 124 && ti.tm_year < 200) {
    // Sembrar reloj del sistema: epoch = hora local RTC - offsets
    time_t local = tmToEpochUtc(&ti);
    struct timeval tv = { .tv_sec = local - gmtOffset_sec - daylightOffset_sec,
                          .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    s_seededFromRtc = true;
    Serial.printf("🕐 [RTC] Hora sembrada desde DS3231: %02d/%02d/%04d %02d:%02d:%02d\n",
                  ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900,
                  ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    Serial.printf("🕐 [RTC] DS3231 presente pero %s - pendiente de puesta en hora\n",
                  s_osf ? "con OSF (pila/hora no fiable)" : "con fecha implausible");
  }
}

void rtcTimeLoop() {
  if (!s_present) return;

  unsigned long now = millis();
  // Primera escritura ~60 s tras tener hora fiable de red; luego cada 6 h
  bool due = (s_lastWriteMs == 0) ? (now > 60000) : (now - s_lastWriteMs > RTC_WRITE_PERIOD_MS);
  if (!due) return;

  struct tm lt;
  if (!systemLocalTime(&lt)) return;   // sin hora fiable todavía

  // Si la hora del sistema vino SOLO del RTC, no reescribir (evita deriva
  // circular); esperar a una fuente externa (SNTP/MQTT)
  if (s_seededFromRtc && s_lastWriteMs == 0) {
    // ¿Hay diferencia notable con el RTC? (SNTP ya corrigió el sistema)
    struct tm rt;
    if (rtcReadTime(&rt)) {
      time_t a = tmToEpochUtc(&lt), b = tmToEpochUtc(&rt);
      if (labs((long)(a - b)) < 5) return;   // sigue siendo la hora del RTC
    }
  }

  if (rtcWriteTime(&lt)) {
    s_lastWriteMs = now;
    Serial.printf("🕐 [RTC] DS3231 actualizado: %02d/%02d/%04d %02d:%02d:%02d\n",
                  lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900,
                  lt.tm_hour, lt.tm_min, lt.tm_sec);
  }
}

bool rtcPresent() { return s_present; }
bool rtcOscStopped() { return s_osf; }

bool rtcSetFromLocalString(const String& localTime) {
  // "YYYY-MM-DD HH:MM:SS"
  struct tm ti = {};
  if (sscanf(localTime.c_str(), "%d-%d-%d %d:%d:%d",
             &ti.tm_year, &ti.tm_mon, &ti.tm_mday,
             &ti.tm_hour, &ti.tm_min, &ti.tm_sec) != 6) return false;
  if (ti.tm_year < 2024 || ti.tm_year > 2099) return false;
  ti.tm_year -= 1900;
  ti.tm_mon -= 1;

  // Reloj del sistema
  time_t local = tmToEpochUtc(&ti);
  struct timeval tv = { .tv_sec = local - gmtOffset_sec - daylightOffset_sec,
                        .tv_usec = 0 };
  settimeofday(&tv, nullptr);

  // RTC
  bool ok = s_present ? rtcWriteTime(&ti) : false;
  if (ok) s_lastWriteMs = millis();
  Serial.printf("🕐 [RTC] Puesta en hora manual/MQTT: %s (%s)\n",
                localTime.c_str(), ok ? "RTC actualizado" : "solo sistema");
  return true;
}

String rtcStatusJson() {
  String s = "{\"present\":";
  s += s_present ? "true" : "false";
  s += ",\"osf\":";
  s += s_osf ? "true" : "false";
  s += ",\"seeded\":";
  s += s_seededFromRtc ? "true" : "false";
  s += "}";
  return s;
}

#endif  // A2_FEATURE_RTC
