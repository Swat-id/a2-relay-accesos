#include "gsm_modem.h"

const char* gsmRegStatName(int8_t stat) {
  switch (stat) {
    case 0: return "no registrado (sin busqueda)";
    case 1: return "registrado (home)";
    case 2: return "buscando red";
    case 3: return "REGISTRO DENEGADO";
    case 4: return "desconocido";
    case 5: return "registrado (roaming)";
    default: return "-";
  }
}

const char* gsmStateName(GsmState st) {
  switch (st) {
    case GSM_DISABLED:     return "disabled";
    case GSM_PROBING:      return "probing";
    case GSM_ABSENT:       return "absent";
    case GSM_SIM_CHECK:    return "sim_check";
    case GSM_PIN_REQUIRED: return "pin_required";
    case GSM_PIN_ERROR:    return "pin_error";
    case GSM_PUK_REQUIRED: return "puk_required";
    case GSM_NO_SIM:       return "no_sim";
    case GSM_REGISTERING:  return "registering";
    case GSM_ATTACHED:     return "attached";
  }
  return "?";
}

#if A2_FEATURE_GSM_MODEM

#include <Preferences.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "web_common.h"
#include "net_manager.h"

#if A2_BOARD_A2V3
// Datos 4G por PPP (core Arduino 3.x): nuestra FSM valida el bring-up (PIN,
// registro, diagnóstico) y al llegar a attached cede la UART a la librería
// PPP en modo CMUX (datos + comandos AT simultáneos)
#include <PPP.h>
#include <WiFi.h>   // WiFi.onEvent recibe también los eventos PPP
#define GSM_HAS_DATA 1
#else
#define GSM_HAS_DATA 0
#endif

extern WebServer server;

// Pines y UART por target: ver hw_config.h
//   A2 clásico: TX=13 RX=34 (input-only → sin swap), UART1 (UART2 = RS485)
//   A2v3:       TX=10 RX=9  (swap auto),             UART2 (UART1 = RS485)
#include "hw_config.h"

#define GSM_BAUD 115200
#define GSM_NVS_NS "a2acc_gsm"
// v5.0.1 (H1): el SIM7600E tarda 10-15 s en aceptar AT tras alimentarse.
// Gracia inicial antes del primer AT + más rondas de probe.
#define GSM_BOOT_GRACE_MS 8000
#define GSM_PROBE_ROUNDS 12
#define GSM_TICK_MS 500
#define GSM_CMD_TIMEOUT_MS 1000
#define GSM_CFUN_TIMEOUT_MS 10000  // AT+CFUN=1 puede tardar varios segundos
// v5.0.1 (H2): la SIM puede tardar más que el módem (busy al arrancar).
// Reintentos de CPIN? antes de declarar no_sim.
#define GSM_CPIN_RETRIES 8
#define GSM_CPIN_RETRY_MS 2000
#define GSM_POLL_PERIOD_MS 10000

static HardwareSerial gsmSerial(GSM_UART_NUM);

static GsmStatus s_st;

// Config NVS
static uint8_t s_en = 1;
static String  s_pin;
static uint8_t s_apnMode = 1;   // 1=auto, 0=manual
static String  s_apn, s_apnUser, s_apnPass;
static uint8_t s_dataMode = 0;  // reservado Fase 4

// Runtime FSM
static bool s_uartOpen = false;
static uint8_t s_probeCount = 0;
static bool s_probeSwapped = false;
static bool s_pinTried = false;       // un intento por arranque/valor
static unsigned long s_lastTick = 0;
static unsigned long s_lastPoll = 0;
static unsigned long s_cmdSentAt = 0;
static unsigned long s_cmdTimeoutMs = GSM_CMD_TIMEOUT_MS;
static String s_rxBuf;
static uint8_t s_pollStep = 0;
// v5.0.1: gracia de arranque, reintentos CPIN y chequeo CFUN
static unsigned long s_probeNotBefore = 0;
static bool s_graceLogged = false;
static uint8_t s_cpinRetries = 0;
static unsigned long s_nextCpinAt = 0;
static bool s_cfunChecked = false;

static void setLastError(const char* err) {
  strncpy(s_st.last_error, err, sizeof(s_st.last_error) - 1);
  s_st.last_error[sizeof(s_st.last_error) - 1] = '\0';
}

// Extrae el error de la respuesta (con CMEE=2 llega texto legible)
static void captureError(bool timedOut) {
  if (timedOut) { setLastError("timeout"); return; }
  int p = s_rxBuf.indexOf("+CME ERROR:");
  if (p < 0) p = s_rxBuf.indexOf("+CMS ERROR:");
  if (p >= 0) {
    int nl = s_rxBuf.indexOf('\r', p);
    String e = (nl > p) ? s_rxBuf.substring(p, nl) : s_rxBuf.substring(p);
    setLastError(e.c_str());
  } else if (s_rxBuf.indexOf("ERROR") >= 0) {
    setLastError("ERROR");
  }
}

// Log del buffer crudo (truncado, CR/LF visibles) para diagnóstico
static void logRawResponse(const char* tag) {
  String s = s_rxBuf.substring(0, 160);
  s.replace("\r", "");
  s.replace("\n", " | ");
  Serial.printf("📡 [GSM] %s respuesta cruda: %s\n", tag, s.c_str());
}

static void loadConfig() {
  Preferences p;
  if (!p.begin(GSM_NVS_NS, true)) return;
  s_en = p.getUChar("en", 1);
  s_st.pin_swap = p.getUChar("pin_swap", 0);
  s_pin = p.getString("pin", "");
  s_apnMode = p.getUChar("apn_mode", 1);
  s_apn = p.getString("apn", "");
  s_apnUser = p.getString("apn_user", "");
  s_apnPass = p.getString("apn_pass", "");
  s_dataMode = p.getUChar("data_mode", 0);
  p.end();
  s_st.pin_set = s_pin.length() > 0;
}

static void saveConfig() {
  Preferences p;
  if (!p.begin(GSM_NVS_NS, false)) return;
  p.putUChar("en", s_en);
  p.putUChar("pin_swap", s_st.pin_swap ? 1 : 0);
  p.putString("pin", s_pin);
  p.putUChar("apn_mode", s_apnMode);
  p.putString("apn", s_apn);
  p.putString("apn_user", s_apnUser);
  p.putString("apn_pass", s_apnPass);
  p.putUChar("data_mode", s_dataMode);
  p.end();
}

static void openUart(bool swapped) {
  if (s_uartOpen) gsmSerial.end();
  int tx = swapped ? GSM_RX_PIN : GSM_TX_PIN;
  int rx = swapped ? GSM_TX_PIN : GSM_RX_PIN;
  gsmSerial.begin(GSM_BAUD, SERIAL_8N1, rx, tx);
  s_uartOpen = true;
}

static void sendCmd(const char* cmd, unsigned long timeoutMs = GSM_CMD_TIMEOUT_MS) {
  s_rxBuf = "";
  while (gsmSerial.available()) gsmSerial.read();  // vaciar restos
  gsmSerial.print(cmd);
  gsmSerial.print("\r\n");
  s_cmdSentAt = millis();
  s_cmdTimeoutMs = timeoutMs;
}

// Lee lo disponible; true si la respuesta terminó (OK/ERROR) o expiró timeout
static bool readResponse(bool* timedOut) {
  while (gsmSerial.available()) {
    char c = (char)gsmSerial.read();
    if (s_rxBuf.length() < 512) s_rxBuf += c;
  }
  if (s_rxBuf.indexOf("OK\r\n") >= 0 || s_rxBuf.indexOf("ERROR") >= 0) {
    *timedOut = false;
    return true;
  }
  if (millis() - s_cmdSentAt > s_cmdTimeoutMs) {
    *timedOut = true;
    return true;
  }
  return false;
}

// Extrae la primera línea "útil" (ni eco ni vacía ni OK)
static String payloadLine() {
  int from = 0;
  while (from < (int)s_rxBuf.length()) {
    int nl = s_rxBuf.indexOf('\n', from);
    String line = (nl < 0) ? s_rxBuf.substring(from) : s_rxBuf.substring(from, nl);
    line.trim();
    from = (nl < 0) ? s_rxBuf.length() : nl + 1;
    if (line.length() == 0 || line.startsWith("AT") || line == "OK") continue;
    return line;
  }
  return "";
}

// Sub-estados de la secuencia de identificación / polling
enum GsmCmd : uint8_t {
  CMD_NONE = 0, CMD_PROBE, CMD_CMEE, CMD_ATI, CMD_GSN,
  CMD_CFUN_Q, CMD_CFUN_SET, CMD_CPIN_Q, CMD_SPIC, CMD_CPIN_SET, CMD_CLCK,
  CMD_CEREG, CMD_CREG, CMD_CSQ, CMD_COPS, CMD_COPS_SET, CMD_CPSI, CMD_CGDCONT
};
static GsmCmd s_cmd = CMD_NONE;

static void gotoState(GsmState st) {
  if (s_st.state != st) {
    Serial.printf("📡 [GSM] %s → %s\n", gsmStateName(s_st.state), gsmStateName(st));
    s_st.state = st;
  }
}

#if GSM_HAS_DATA
// ---------------------------------------------------------------------------
// Datos 4G (PPP) — solo A2v3
// ---------------------------------------------------------------------------
static bool s_pppStarted = false;
static unsigned long s_lastApnWarn = 0;
// APN auto: el asignado por la red en el attach LTE (leído de AT+CGDCONT?)
static String s_apnResolved;
static bool s_apnQueryDone = false;

static String gsmEffectiveApn() {
  return (s_apnMode == 1) ? s_apnResolved : s_apn;
}

static void onPppEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_PPP_GOT_IP:
      s_st.data_up = true;
      strncpy(s_st.data_ip, PPP.localIP().toString().c_str(), sizeof(s_st.data_ip) - 1);
      netOnGsmGotIp();
      Serial.printf("📡 [GSM] ✅ Datos 4G conectados (PPP): IP %s\n", s_st.data_ip);
      break;
    case ARDUINO_EVENT_PPP_LOST_IP:
    case ARDUINO_EVENT_PPP_DISCONNECTED:
    case ARDUINO_EVENT_PPP_STOP:
      if (s_st.data_up) Serial.println("📡 [GSM] Datos 4G desconectados");
      s_st.data_up = false;
      s_st.data_ip[0] = '\0';
      netOnGsmDown();
      break;
    default:
      break;
  }
}

static void gsmStartData() {
  String apn = gsmEffectiveApn();
  Serial.printf("📡 [GSM] Iniciando datos 4G (PPP/CMUX, APN '%s' %s)…\n",
                apn.c_str(), s_apnMode == 1 ? "[auto/red]" : "[manual]");
  // Ceder la UART a la librería PPP
  gsmSerial.end();
  s_uartOpen = false;
  s_cmd = CMD_NONE;

  static bool eventRegistered = false;
  if (!eventRegistered) {
    eventRegistered = true;
    WiFi.onEvent(onPppEvent);
  }

  PPP.setApn(apn.c_str());
  if (s_pin.length() > 0) PPP.setPin(s_pin.c_str());
  int tx = s_st.pin_swap ? GSM_RX_PIN : GSM_TX_PIN;
  int rx = s_st.pin_swap ? GSM_TX_PIN : GSM_RX_PIN;
  PPP.setPins(tx, rx);

  if (!PPP.begin(PPP_MODEM_SIM7600, GSM_UART_NUM)) {
    Serial.println("❌ [GSM] PPP.begin falló - reintento en el próximo rescan");
    setLastError("PPP.begin fallo");
    openUart(s_probeSwapped);   // recuperar la FSM cruda
    return;
  }
  s_pppStarted = true;
  s_st.data_started = true;

  // CMUX: datos + comandos AT simultáneos (estado sin cortar la conexión)
  if (!PPP.mode(ESP_MODEM_MODE_CMUX)) {
    Serial.println("⚠️ [GSM] CMUX no aceptado - intentando modo DATA puro");
    PPP.mode(ESP_MODEM_MODE_DATA);
  }
}

static void gsmStopData() {
  if (!s_pppStarted) return;
  PPP.end();
  s_pppStarted = false;
  s_st.data_started = false;
  s_st.data_up = false;
  s_st.data_ip[0] = '\0';
  netOnGsmDown();
}

// Sondeo de estado con PPP activo (vía CMUX)
static void gsmDataLoop() {
  unsigned long now = millis();
  if (now - s_lastPoll < GSM_POLL_PERIOD_MS) return;
  s_lastPoll = now;

  int r = PPP.RSSI();
  s_st.csq = (r >= 0 && r <= 31) ? (int8_t)r : ((r == 99) ? 99 : s_st.csq);
  String op = PPP.operatorName();
  if (op.length()) strncpy(s_st.oper, op.c_str(), sizeof(s_st.oper) - 1);
  bool att = PPP.attached();
  if (att && s_st.state != GSM_ATTACHED) gotoState(GSM_ATTACHED);
  if (!att && s_st.state == GSM_ATTACHED) gotoState(GSM_REGISTERING);
}
#endif  // GSM_HAS_DATA

static void startProbe() {
  s_probeCount = 0;
  s_probeSwapped = s_st.pin_swap;   // empezar por el mapeo aprendido
  s_cpinRetries = 0;
  s_cfunChecked = false;
#if GSM_HAS_DATA
  s_apnQueryDone = false;
  s_apnResolved = "";
#endif
  s_st.last_error[0] = '\0';
  openUart(s_probeSwapped);
  gotoState(GSM_PROBING);
  s_cmd = CMD_NONE;
}

void gsmModemBegin() {
  loadConfig();
  if (!s_en) {
    gotoState(GSM_DISABLED);
    Serial.println("📡 [GSM] Deshabilitado por configuración");
    return;
  }
  // Gracia de arranque: el SIM7600E tarda 10-15 s en aceptar AT (H1 v5.0.1)
  s_probeNotBefore = millis() + GSM_BOOT_GRACE_MS;
  s_graceLogged = false;
  startProbe();
  Serial.printf("📡 [GSM] Probe programado en %d s (TX=%d RX=%d%s)\n",
                GSM_BOOT_GRACE_MS / 1000, GSM_TX_PIN, GSM_RX_PIN,
                GSM_SWAP_POSSIBLE ? ", swap auto" : "");
}

void gsmModemRescan() {
  if (!s_en) return;
#if GSM_HAS_DATA
  gsmStopData();   // recuperar la UART para la FSM cruda
#endif
  s_pinTried = false;
  // Si el módulo está presente, reiniciarlo de verdad (AT+CRESET): el SIM7600
  // se alimenta del socket y NO se reinicia con el reset del ESP32, así que
  // solo relee la SIM en su propio arranque. Esto permite detectar una SIM
  // insertada en caliente sin cortar la alimentación del equipo.
  if (s_uartOpen && s_st.state != GSM_ABSENT && s_st.state != GSM_DISABLED) {
    Serial.println("📡 [GSM] Rescan: reiniciando módulo (AT+CRESET, ~15 s)…");
    gsmSerial.print("AT+CRESET\r\n");
    s_probeNotBefore = millis() + 15000;   // arranque completo del módulo
  } else {
    Serial.println("📡 [GSM] Rescan manual");
    s_probeNotBefore = millis() + 500;     // sin módulo previo: gracia mínima
  }
  s_graceLogged = false;
  startProbe();
}

const GsmStatus& gsmModemStatus() { return s_st; }

bool gsmSetEnabled(bool en) {
  s_en = en ? 1 : 0;
  saveConfig();
  if (en) gsmModemRescan();
  else {
#if GSM_HAS_DATA
    gsmStopData();
#endif
    gotoState(GSM_DISABLED);
    s_cmd = CMD_NONE;
  }
  return true;
}

// Si estamos parados en pin_required/pin_error, re-entrar en sim_check para
// aplicar el nuevo PIN sin reiniciar (v5.0.1)
static void retrySimCheck() {
  if (s_st.state == GSM_PIN_REQUIRED || s_st.state == GSM_PIN_ERROR ||
      s_st.state == GSM_NO_SIM) {
    gotoState(GSM_SIM_CHECK);
    s_cfunChecked = true;
    s_cpinRetries = 0;
    s_nextCpinAt = millis() + 200;
  }
}

static bool s_clckPending = false;   // tras desbloquear: desactivar PIN (CLCK)

bool gsmSetPin(const String& pin) {
  if (pin.length() > 8) return false;
  for (unsigned i = 0; i < pin.length(); i++)
    if (!isDigit(pin[i])) return false;
  s_pin = pin;
  s_st.pin_set = s_pin.length() > 0;
  s_pinTried = false;   // nuevo valor: se permite UN intento
  s_clckPending = false;
  saveConfig();
  retrySimCheck();
  return true;
}

bool gsmDisableSimPin(const String& pin) {
  if (pin.length() < 4 || pin.length() > 8) return false;
  for (unsigned i = 0; i < pin.length(); i++)
    if (!isDigit(pin[i])) return false;
  s_pin = pin;
  s_st.pin_set = true;
  s_pinTried = false;
  s_clckPending = true;   // SIM bloqueada: tras CPIN OK → CLCK
                          // SIM ya desbloqueada: el scheduler envía CLCK directo
  saveConfig();
  Serial.println("📡 [GSM] Solicitado desbloqueo + desactivación permanente del PIN");
  retrySimCheck();
  return true;
}

bool gsmSetApn(uint8_t apn_mode, const String& apn, const String& user, const String& pass) {
  bool changed = (apn != s_apn);
  s_apnMode = apn_mode ? 1 : 0;
  s_apn = apn;
  s_apnUser = user;
  s_apnPass = pass;
  saveConfig();
#if GSM_HAS_DATA
  // APN nuevo con datos activos: reiniciar el ciclo para aplicarlo
  if (changed && s_pppStarted) gsmModemRescan();
#else
  (void)changed;
#endif
  return true;
}

// ---------------------------------------------------------------------------
// FSM principal (tick cada GSM_TICK_MS; cada comando AT con timeout corto)
// ---------------------------------------------------------------------------
void gsmModemLoop() {
#if GSM_HAS_DATA
  if (s_pppStarted) {
    gsmDataLoop();   // la FSM cruda queda parada: la UART es de PPP
    return;
  }
#endif
  if (s_st.state == GSM_DISABLED || s_st.state == GSM_ABSENT ||
      s_st.state == GSM_PIN_ERROR || s_st.state == GSM_PUK_REQUIRED) {
    return;  // estados de reposo (rescan/config los reactivan)
  }

  unsigned long now = millis();

  // ¿Hay comando en vuelo?
  if (s_cmd != CMD_NONE) {
    bool timedOut;
    if (!readResponse(&timedOut)) return;   // aún esperando

    String line = payloadLine();
    bool ok = !timedOut && s_rxBuf.indexOf("ERROR") < 0;

    switch (s_cmd) {
      case CMD_PROBE:
        if (ok) {
          if (s_probeSwapped != s_st.pin_swap) {
            s_st.pin_swap = s_probeSwapped;
            saveConfig();   // aprender el mapeo
          }
          Serial.printf("📡 [GSM] Módulo detectado%s\n", s_probeSwapped ? " (TX/RX invertidos)" : "");
          // Errores CME en texto legible para todo el diagnóstico posterior
          s_cmd = CMD_CMEE;
          sendCmd("AT+CMEE=2");
          return;
        }
        Serial.printf("📡 [GSM] Probe %d/%d sin respuesta\n", s_probeCount + 1, GSM_PROBE_ROUNDS);
        if (++s_probeCount >= GSM_PROBE_ROUNDS) {
          gotoState(GSM_ABSENT);
          setLastError("sin respuesta AT (probe agotado)");
          Serial.println("📡 [GSM] Sin módulo (probe agotado); rescan manual disponible");
          s_cmd = CMD_NONE;
          return;
        }
        // RECUPERACIÓN (v5.0.2): el SIM7600 no se reinicia con el ESP32 — si
        // el ESP se reinició/reflasheó con los datos 4G activos, el módulo
        // sigue en modo DATA o CMUX y no atiende AT planos. Intercalar las
        // secuencias de salida de ambos modos entre rondas de probe:
        if (s_probeCount == 3 || s_probeCount == 5 || s_probeCount == 8) {
          // Las secuencias de recuperación van por el mapeo APRENDIDO
          if (s_probeSwapped != s_st.pin_swap) {
            s_probeSwapped = s_st.pin_swap;
            openUart(s_probeSwapped);
          }
          if (s_probeCount == 3) {
            // Salir de modo DATA: escape +++ (guardas de ~1 s cubiertas por
            // el propio ritmo del probe: el último AT fue hace ≥1,5 s)
            Serial.println("📡 [GSM] Recuperación: escape de modo DATA (+++)");
            gsmSerial.print("+++");
          } else {
            // Cerrar multiplexado CMUX: trama CLD (3GPP TS 27.010, DLCI 0)
            Serial.println("📡 [GSM] Recuperación: cierre de CMUX (CLD)");
            static const uint8_t cmuxCld[] = {0xF9, 0x03, 0xEF, 0x05, 0xC3, 0x01, 0xF2, 0xF9};
            gsmSerial.write(cmuxCld, sizeof(cmuxCld));
            gsmSerial.flush();
          }
        }
#if GSM_SWAP_POSSIBLE
        else if (s_probeCount < 3 || s_probeCount > 8) {
          // Detección de mapeo invertido solo fuera de la fase de recuperación
          s_probeSwapped = !s_probeSwapped;
          openUart(s_probeSwapped);
        }
#endif
        s_cmd = CMD_NONE;   // el próximo tick reenvía AT
        return;

      case CMD_CMEE:
        // Respuesta indiferente: continuar con la identificación
        // (CGMM devuelve solo el modelo; ATI multi-línea daba el fabricante)
        s_cmd = CMD_ATI;
        sendCmd("AT+CGMM");
        return;

      case CMD_ATI:
        if (ok && line.length()) {
          strncpy(s_st.model, line.c_str(), sizeof(s_st.model) - 1);
        }
        s_cmd = CMD_GSN;
        sendCmd("AT+GSN");
        return;

      case CMD_GSN:
        if (ok && line.length() >= 14 && line.length() <= 16) {
          strncpy(s_st.imei, line.c_str(), sizeof(s_st.imei) - 1);
        }
        // Antes de CPIN: verificar funcionalidad completa (CFUN) — un módulo
        // en modo mínimo/avión responde AT pero no ve la SIM
        gotoState(GSM_SIM_CHECK);
        s_cmd = CMD_CFUN_Q;
        sendCmd("AT+CFUN?");
        return;

      case CMD_CFUN_Q:
        if (s_rxBuf.indexOf("+CFUN: 1") >= 0) {
          s_cfunChecked = true;
          s_nextCpinAt = now;          // pasar a CPIN ya
        } else {
          logRawResponse("CFUN?");
          Serial.println("📡 [GSM] Funcionalidad no completa - enviando AT+CFUN=1");
          s_cmd = CMD_CFUN_SET;
          sendCmd("AT+CFUN=1", GSM_CFUN_TIMEOUT_MS);
          return;
        }
        s_cmd = CMD_NONE;
        return;

      case CMD_CFUN_SET:
        s_cfunChecked = true;
        // La SIM se reinicializa tras CFUN=1: dar margen antes del CPIN
        s_nextCpinAt = now + 3000;
        if (!ok) {
          captureError(timedOut);
          logRawResponse("CFUN=1");
        }
        s_cmd = CMD_NONE;
        return;

      case CMD_CPIN_Q:
        if (s_rxBuf.indexOf("READY") >= 0) {
          s_st.last_error[0] = '\0';
          gotoState(GSM_REGISTERING);
          s_cmd = CMD_NONE;
        } else if (s_rxBuf.indexOf("SIM PIN") >= 0) {
          // Evidencia siempre: respuesta exacta del módem + intentos restantes
          logRawResponse("CPIN?");
          s_cmd = CMD_SPIC;
          sendCmd("AT+SPIC");
        } else if (s_rxBuf.indexOf("SIM PUK") >= 0) {
          logRawResponse("CPIN?");
          gotoState(GSM_PUK_REQUIRED);
          s_cmd = CMD_NONE;
        } else {
          // v5.0.1 (H2): SIM ocupada/arrancando ≠ SIM ausente. Registrar el
          // error real y reintentar antes de declarar no_sim.
          captureError(timedOut);
          logRawResponse("CPIN?");
          if (++s_cpinRetries < GSM_CPIN_RETRIES) {
            Serial.printf("📡 [GSM] CPIN sin resultado (%s) - reintento %d/%d en %d ms\n",
                          s_st.last_error, s_cpinRetries, GSM_CPIN_RETRIES,
                          GSM_CPIN_RETRY_MS);
            s_nextCpinAt = now + GSM_CPIN_RETRY_MS;
          } else {
            gotoState(GSM_NO_SIM);
            Serial.printf("📡 [GSM] SIM no disponible tras %d intentos (último error: %s)\n",
                          GSM_CPIN_RETRIES, s_st.last_error);
          }
          s_cmd = CMD_NONE;
        }
        return;

      case CMD_SPIC: {
        // +SPIC: pin1,pin2,puk1,puk2 (intentos restantes) — informativo
        int p = s_rxBuf.indexOf("+SPIC:");
        if (p >= 0) {
          int nl = s_rxBuf.indexOf('\r', p);
          String v = (nl > p) ? s_rxBuf.substring(p + 6, nl) : s_rxBuf.substring(p + 6);
          v.trim();
          strncpy(s_st.pin_attempts, v.c_str(), sizeof(s_st.pin_attempts) - 1);
          Serial.printf("📡 [GSM] Intentos restantes (PIN1,PIN2,PUK1,PUK2): %s\n",
                        s_st.pin_attempts);
        }
        // Decisión de PIN (política: UN intento por arranque y por valor)
        if (s_pin.length() > 0 && !s_pinTried) {
          s_pinTried = true;
          Serial.println("📡 [GSM] Enviando PIN almacenado (intento único)");
          s_cmd = CMD_CPIN_SET;
          sendCmd(("AT+CPIN=" + s_pin).c_str());
        } else {
          gotoState(s_pinTried ? GSM_PIN_ERROR : GSM_PIN_REQUIRED);
          s_cmd = CMD_NONE;
        }
        return;
      }

      case CMD_CPIN_SET:
        if (ok) {
          Serial.println("📡 [GSM] PIN aceptado");
          if (s_clckPending) {
            // Desactivar el PIN de la SIM permanentemente (M2M en campo)
            s_cmd = CMD_CLCK;
            sendCmd(("AT+CLCK=\"SC\",0,\"" + s_pin + "\"").c_str(), 5000);
            return;
          }
          gotoState(GSM_REGISTERING);
        } else {
          captureError(timedOut);
          logRawResponse("CPIN=");
          s_clckPending = false;
          Serial.println("❌ [GSM] PIN RECHAZADO - sin reintentos (riesgo PUK)");
          gotoState(GSM_PIN_ERROR);
        }
        s_cmd = CMD_NONE;
        return;

      case CMD_CLCK:
        s_clckPending = false;
        if (ok) {
          Serial.println("📡 [GSM] ✅ PIN de la SIM DESACTIVADO permanentemente");
          s_pin = "";              // ya no hace falta almacenarlo
          s_st.pin_set = false;
          saveConfig();
        } else {
          captureError(timedOut);
          logRawResponse("CLCK");
          Serial.println("⚠️ [GSM] No se pudo desactivar el PIN (la SIM queda desbloqueada esta sesión)");
        }
        // Si veníamos del flujo de desbloqueo, continuar a registro; si la SIM
        // ya estaba operativa (registering/attached), mantener el estado
        if (s_st.state == GSM_SIM_CHECK) gotoState(GSM_REGISTERING);
        s_cmd = CMD_NONE;
        return;

      case CMD_CEREG:
      case CMD_CREG: {
        // +CEREG: n,stat  → stat 1 (home) o 5 (roaming) = registrado
        int comma = s_rxBuf.lastIndexOf(',');
        int stat = (comma > 0) ? s_rxBuf.substring(comma + 1).toInt() : 0;
        if (stat == 1 || stat == 5) {
          if ((int8_t)stat != s_st.reg_stat) {
            Serial.printf("📡 [GSM] %s: stat=%d (%s)\n",
                          s_cmd == CMD_CEREG ? "CEREG" : "CREG", stat, gsmRegStatName(stat));
          }
          s_st.reg_stat = (int8_t)stat;
          if (s_st.state != GSM_ATTACHED) {
            gotoState(GSM_ATTACHED);
            Serial.println("📡 [GSM] Registrado en red");
          }
        } else if (s_cmd == CMD_CEREG) {
          s_cmd = CMD_CREG;             // sin LTE: probar 2G/3G
          sendCmd("AT+CREG?");
          return;
        } else {
          // Ni LTE ni 2G/3G registrado: dejar evidencia del motivo
          if ((int8_t)stat != s_st.reg_stat) {
            Serial.printf("📡 [GSM] CREG: stat=%d (%s)\n", stat, gsmRegStatName(stat));
            if (stat == 3) {
              setLastError("registro denegado por la red (stat=3)");
              Serial.println("❌ [GSM] Registro DENEGADO - revisar aprovisionamiento M2M / roaming del operador");
            }
          }
          s_st.reg_stat = (int8_t)stat;
          if (s_st.state == GSM_ATTACHED) {
            gotoState(GSM_REGISTERING);   // perdió registro
          }
        }
        s_cmd = CMD_NONE;
        return;
      }

      case CMD_CSQ: {
        int p = s_rxBuf.indexOf("+CSQ:");
        if (p >= 0) {
          int v = s_rxBuf.substring(p + 5).toInt();
          // 0-31 = medida; 99 = SIN SEÑAL detectable (≠ -1 "no consultado")
          s_st.csq = (v >= 0 && v <= 31) ? (int8_t)v : ((v == 99) ? 99 : -1);
        }
        s_cmd = CMD_COPS;
        sendCmd("AT+COPS?");
        return;
      }

      case CMD_COPS: {
        int q1 = s_rxBuf.indexOf('"');
        if (q1 >= 0) {
          int q2 = s_rxBuf.indexOf('"', q1 + 1);
          if (q2 > q1) {
            String op = s_rxBuf.substring(q1 + 1, q2);
            strncpy(s_st.oper, op.c_str(), sizeof(s_st.oper) - 1);
          }
        }
        s_cmd = CMD_NONE;
        return;
      }

      case CMD_COPS_SET:
        if (ok) {
          Serial.println("📡 [GSM] Selección automática de operador solicitada (COPS=0)");
        } else {
          captureError(timedOut);
          logRawResponse("COPS=0");
        }
        s_cmd = CMD_NONE;
        return;

#if GSM_HAS_DATA
      case CMD_CGDCONT: {
        // "+CGDCONT: 1,"IP","<apn red>",..." — APN del bearer por defecto LTE
        s_apnQueryDone = true;
        int p = s_rxBuf.indexOf("+CGDCONT: 1,");
        if (p < 0) p = s_rxBuf.indexOf("+CGDCONT:");
        if (p >= 0) {
          int q1 = s_rxBuf.indexOf('"', p);              // abre tipo PDP
          int q2 = (q1 >= 0) ? s_rxBuf.indexOf('"', q1 + 1) : -1;
          int q3 = (q2 >= 0) ? s_rxBuf.indexOf('"', q2 + 1) : -1;
          int q4 = (q3 >= 0) ? s_rxBuf.indexOf('"', q3 + 1) : -1;
          if (q4 > q3) {
            s_apnResolved = s_rxBuf.substring(q3 + 1, q4);
          }
        }
        if (s_apnResolved.length()) {
          Serial.printf("📡 [GSM] APN asignado por la red: '%s'\n", s_apnResolved.c_str());
        } else {
          logRawResponse("CGDCONT?");
          Serial.println("📡 [GSM] Sin APN visible en el bearer - se intentará PPP con APN vacío");
        }
        s_cmd = CMD_NONE;
        return;
      }
#endif

      case CMD_CPSI: {
        int p = s_rxBuf.indexOf("+CPSI:");
        if (p >= 0) {
          int nl = s_rxBuf.indexOf('\r', p);
          String v = (nl > p) ? s_rxBuf.substring(p + 6, nl) : s_rxBuf.substring(p + 6);
          v.trim();
          if (strncmp(s_st.radio, v.c_str(), sizeof(s_st.radio) - 1) != 0) {
            Serial.printf("📡 [GSM] Radio (CPSI): %s\n", v.c_str());
          }
          strncpy(s_st.radio, v.c_str(), sizeof(s_st.radio) - 1);
        }
        s_cmd = CMD_NONE;
        return;
      }

      default:
        s_cmd = CMD_NONE;
        return;
    }
  }

  // Sin comando en vuelo: programar el siguiente según estado
  if (now - s_lastTick < GSM_TICK_MS) return;
  s_lastTick = now;

  switch (s_st.state) {
    case GSM_PROBING:
      if ((long)(now - s_probeNotBefore) < 0) {
        if (!s_graceLogged) {
          s_graceLogged = true;
          Serial.println("📡 [GSM] Esperando arranque del módulo…");
        }
        break;   // gracia de arranque del SIM7600E
      }
      s_cmd = CMD_PROBE;
      sendCmd("AT");
      break;

    case GSM_SIM_CHECK:
      // Reintentos de CPIN? programados (SIM busy al arrancar, o tras CFUN=1)
      if (s_cfunChecked && (long)(now - s_nextCpinAt) >= 0) {
        s_cmd = CMD_CPIN_Q;
        sendCmd("AT+CPIN?");
      }
      break;

    case GSM_REGISTERING:
      // Petición pendiente de quitar PIN con la SIM ya desbloqueada
      if (s_clckPending && s_pin.length() > 0) {
        s_cmd = CMD_CLCK;
        sendCmd(("AT+CLCK=\"SC\",0,\"" + s_pin + "\"").c_str(), 5000);
        break;
      }
      if (now - s_lastPoll >= 2000) {
        s_lastPoll = now;
        // Si el módem NO está buscando (stat=0), forzar selección automática
        // de operador — la NVM del módulo puede haber quedado en modo manual
        // o la búsqueda pudo agotarse antes de conectar la antena
        static unsigned long lastCopsKick = 0;
        if (s_st.reg_stat <= 0 && (lastCopsKick == 0 || now - lastCopsKick > 30000)) {
          lastCopsKick = now;
          Serial.println("📡 [GSM] stat=0: forzando búsqueda (AT+COPS=0)…");
          s_cmd = CMD_COPS_SET;
          sendCmd("AT+COPS=0", 15000);
          break;
        }
        // Rotar registro / cobertura / estado radio
        switch (s_pollStep++ % 3) {
          case 0: s_cmd = CMD_CEREG; sendCmd("AT+CEREG?"); break;
          case 1: s_cmd = CMD_CSQ; sendCmd("AT+CSQ"); break;
          case 2: s_cmd = CMD_CPSI; sendCmd("AT+CPSI?"); break;
        }
        // Resumen periódico para el log (cada ~30 s)
        static unsigned long lastSummary = 0;
        if (now - lastSummary > 30000) {
          lastSummary = now;
          Serial.printf("📡 [GSM] Registrando… stat=%d (%s), CSQ=%d, radio=%s\n",
                        s_st.reg_stat, gsmRegStatName(s_st.reg_stat), s_st.csq,
                        s_st.radio[0] ? s_st.radio : "-");
        }
      }
      break;

    case GSM_ATTACHED:
      if (s_clckPending && s_pin.length() > 0) {
        s_cmd = CMD_CLCK;
        sendCmd(("AT+CLCK=\"SC\",0,\"" + s_pin + "\"").c_str(), 5000);
        break;
      }
#if GSM_HAS_DATA
      // Registrado: arrancar los datos 4G (PPP)
      if (!s_pppStarted && s_dataMode != 2) {
        if (s_apnMode == 1 && !s_apnQueryDone) {
          // APN automático: leer el asignado por la red en el attach LTE
          s_cmd = CMD_CGDCONT;
          sendCmd("AT+CGDCONT?", 2000);
          break;
        }
        if (s_apnMode == 0 && s_apn.length() == 0) {
          if (millis() - s_lastApnWarn > 60000) {
            s_lastApnWarn = millis();
            Serial.println("⚠️ [GSM] Modo APN manual sin APN - datos 4G en espera (configurar en /gsm)");
            setLastError("APN manual sin configurar");
          }
        } else {
          gsmStartData();
          break;
        }
      }
#endif
      if (now - s_lastPoll >= GSM_POLL_PERIOD_MS) {
        s_lastPoll = now;
        // Alternar: registro / cobertura+operador
        if (s_pollStep++ & 1) {
          s_cmd = CMD_CEREG;
          sendCmd("AT+CEREG?");
        } else {
          s_cmd = CMD_CSQ;
          sendCmd("AT+CSQ");
        }
      }
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Estado JSON + rutas web
// ---------------------------------------------------------------------------
String gsmStatusJson() {
  DynamicJsonDocument doc(512);
  doc["state"] = gsmStateName(s_st.state);
  doc["enabled"] = (bool)s_en;
  doc["model"] = s_st.model;
  doc["imei"] = s_st.imei;
  doc["operator"] = s_st.oper;
  doc["csq"] = s_st.csq;
  if (s_st.csq >= 0 && s_st.csq <= 31) doc["rssi_dbm"] = -113 + 2 * s_st.csq;
  doc["pin_set"] = s_st.pin_set;       // NUNCA el PIN
  doc["pin_swap"] = s_st.pin_swap;
  doc["last_error"] = s_st.last_error; // diagnóstico (CME verbose)
  doc["pin_attempts"] = s_st.pin_attempts; // AT+SPIC: PIN1,PIN2,PUK1,PUK2 restantes
  doc["reg_stat"] = s_st.reg_stat;
  doc["reg_stat_text"] = gsmRegStatName(s_st.reg_stat);
  doc["radio"] = s_st.radio;           // AT+CPSI?: tecnología/banda o NO SERVICE
  doc["data_started"] = s_st.data_started;
  doc["data_up"] = s_st.data_up;       // true = MQTT puede salir por 4G
  doc["data_ip"] = s_st.data_ip;
#if GSM_HAS_DATA
  doc["apn_active"] = gsmEffectiveApn(); // APN en uso (auto: el de la red)
#endif
  doc["apn_mode"] = s_apnMode;
  doc["apn"] = s_apn;
  doc["apn_user"] = s_apnUser;         // el password de APN nunca se expone
  doc["data_mode"] = s_dataMode;       // reservado Fase 4
  String out;
  serializeJson(doc, out);
  return out;
}

static void handleGsmStatusApi() {
  if (!webAuth()) return;
  server.send(200, "application/json", gsmStatusJson());
}

static void handleGsmRescanApi() {
  if (!webAuth()) return;
  gsmModemRescan();
  server.send(200, "application/json", "{\"rescan\":true}");
}

static void handleGsmPost() {
  if (!webAuth()) return;
  String action = server.arg("action");
  String msg = "ok";

  if (action == "save_cfg") {
    gsmSetEnabled(server.arg("enabled") == "1");
    // El password de APN solo se cambia si el campo llega relleno
    String pass = server.hasArg("apn_pass") && server.arg("apn_pass").length()
                      ? server.arg("apn_pass") : s_apnPass;
    gsmSetApn(server.arg("apn_mode").toInt(), server.arg("apn"),
              server.arg("apn_user"), pass);
  } else if (action == "set_pin") {
    if (!gsmSetPin(server.arg("pin")))
      msg = "PIN invalido (max 8 digitos)";
  } else if (action == "disable_pin") {
    if (!gsmDisableSimPin(server.arg("pin")))
      msg = "PIN invalido (4-8 digitos)";
  } else if (action == "clear_pin") {
    gsmSetPin("");
  } else if (action == "rescan") {
    gsmModemRescan();
  } else {
    msg = "accion desconocida";
  }

  server.sendHeader("Location", "/gsm?msg=" + msg);
  server.send(303);
}

static void handleGsmPage() {
  if (!webAuth()) return;

  String html = webPageBegin("4G - Controladora A2");
  html += "<header><h1>📡 Módem 4G</h1></header><main><div class='container'>";

  if (server.hasArg("msg") && server.arg("msg") != "ok") {
    html += "<div class='status not-synced'>⚠️ " + server.arg("msg") + "</div>";
  }

  html += R"=====(
<h2>Estado</h2>
<table>
 <tr><th>Parámetro</th><th>Valor</th></tr>
 <tr><td>Estado</td><td id='gState'>…</td></tr>
 <tr><td>Módulo</td><td id='gModel'>…</td></tr>
 <tr><td>IMEI</td><td id='gImei'>…</td></tr>
 <tr><td>Operador</td><td id='gOper'>…</td></tr>
 <tr><td>Registro de red</td><td id='gReg'>…</td></tr>
 <tr><td>Radio</td><td id='gRadio'>…</td></tr>
 <tr><td>Datos 4G (PPP)</td><td id='gData'>…</td></tr>
 <tr><td>Cobertura</td><td id='gCsq'>…</td></tr>
 <tr><td>PIN</td><td id='gPin'>…</td></tr>
 <tr><td>Intentos restantes</td><td id='gSpic'>…</td></tr>
 <tr><td>Mapeo TX/RX</td><td id='gSwap'>…</td></tr>
 <tr><td>Último error AT</td><td id='gErr'>…</td></tr>
</table>
<div class='button-group'>
 <form method='post' action='/gsm' style='display:inline'>
  <input type='hidden' name='action' value='rescan'>
  <button type='submit' class='btn-info'>🔄 Reiniciar módulo y re-detectar SIM (~20 s)</button>
 </form>
</div>

<h2>Configuración</h2>
<div class='card'>
 <form method='post' action='/gsm'>
  <input type='hidden' name='action' value='save_cfg'>
  <label><input type='checkbox' name='enabled' value='1' id='fEn'>
   <span class='checkbox-label'>Módem habilitado (probe al arrancar)</span></label>
  <label>APN</label>
  <select name='apn_mode' id='fApnMode'>
   <option value='1'>Automático (red del operador)</option>
   <option value='0'>Manual</option>
  </select>
  <input type='text' name='apn' id='fApn' maxlength='63' placeholder='APN (solo modo manual)'>
  <label>Usuario APN</label>
  <input type='text' name='apn_user' id='fApnUser' maxlength='32'>
  <label>Password APN (vacío = no cambiar)</label>
  <input type='password' name='apn_pass' maxlength='32'>
  <button type='submit' class='btn-success'>Guardar configuración</button>
 </form>
</div>

<h2>PIN de la SIM</h2>
<div class='security-info'>
 <p>⚠️ El PIN se envía <strong>una sola vez</strong> por arranque y por valor.
 Si es rechazado, el equipo NO reintenta (riesgo de bloqueo por PUK):
 corrígelo aquí y se permitirá un nuevo intento.</p>
 <form method='post' action='/gsm'>
  <input type='hidden' name='action' value='set_pin'>
  <label>PIN (4-8 dígitos; nunca se muestra)</label>
  <input type='password' name='pin' maxlength='8' pattern='[0-9]{4,8}'>
  <button type='submit' class='btn-warning'>Guardar PIN</button>
 </form>
 <form method='post' action='/gsm'>
  <input type='hidden' name='action' value='clear_pin'>
  <button type='submit' class='btn-danger'>Borrar PIN almacenado</button>
 </form>
 <hr>
 <p><strong>Recomendado para SIM M2M:</strong> desbloquear y <strong>quitar el
 PIN de la SIM permanentemente</strong> — el equipo no dependerá del PIN en
 futuros arranques ni cambios de placa.</p>
 <form method='post' action='/gsm'>
  <input type='hidden' name='action' value='disable_pin'>
  <label>PIN actual de la SIM</label>
  <input type='password' name='pin' maxlength='8' pattern='[0-9]{4,8}' required>
  <button type='submit' class='btn-success'>Desbloquear y quitar PIN permanentemente</button>
 </form>
</div>
<div class='button-group'>
 <a href='/wifi'><button class='btn-info'>📶 Red WiFi</button></a>
 <a href='/'><button class='btn-secondary'>🏠 Inicio</button></a>
</div>

<script>
const stNames={disabled:'⚪ Deshabilitado',probing:'⏳ Detectando módulo…',
 absent:'❌ Sin módulo',sim_check:'⏳ Comprobando SIM…',
 pin_required:'🔑 PIN requerido',pin_error:'❌ PIN RECHAZADO (sin reintentos)',
 puk_required:'🚫 PUK requerido (avisar operador)',no_sim:'❌ Sin SIM',
 registering:'⏳ Registrando en red…',attached:'✅ Registrado'};
function csqTxt(c,dbm){if(c<0)return '—';if(c==0||c==99)return 'Sin señal';
 let q=c<8?'Mala':c<13?'Regular':c<20?'Buena':'Excelente';
 return q+' ('+c+'/31, '+dbm+' dBm)';}
function upd(){fetch('/api/gsm/status').then(r=>r.json()).then(s=>{
 document.getElementById('gState').textContent=stNames[s.state]||s.state;
 document.getElementById('gModel').textContent=s.model||'—';
 document.getElementById('gImei').textContent=s.imei||'—';
 document.getElementById('gOper').textContent=s.operator||'—';
 document.getElementById('gReg').textContent=s.reg_stat_text||'—';
 document.getElementById('gRadio').textContent=s.radio||'—';
 document.getElementById('gData').textContent=s.data_up?('✅ conectados · IP '+s.data_ip):(s.data_started?'⏳ conectando':'—');
 document.getElementById('gCsq').textContent=csqTxt(s.csq,s.rssi_dbm);
 document.getElementById('gPin').textContent=s.pin_set?'🔑 configurado':'— sin PIN';
 document.getElementById('gSwap').textContent=s.pin_swap?'invertido (auto)':'directo';
 document.getElementById('gErr').textContent=s.last_error||'—';
 document.getElementById('gSpic').textContent=s.pin_attempts?('PIN1,PIN2,PUK1,PUK2: '+s.pin_attempts):'—';
 if(!window._cfgSet){document.getElementById('fEn').checked=s.enabled;
  document.getElementById('fApnMode').value=s.apn_mode;
  document.getElementById('fApn').value=s.apn||'';
  document.getElementById('fApnUser').value=s.apn_user||'';window._cfgSet=1;}
}).catch(()=>{});}
upd();setInterval(upd,3000);
</script>
)=====";

  html += "</div></main>";
  html += WEB_PAGE_END;
  server.send(200, "text/html", html);
}

void gsmRegisterWebRoutes() {
  server.on("/gsm", HTTP_GET, handleGsmPage);
  server.on("/gsm", HTTP_POST, handleGsmPost);
  server.on("/api/gsm/status", HTTP_GET, handleGsmStatusApi);
  server.on("/api/gsm/rescan", HTTP_POST, handleGsmRescanApi);
}

#endif  // A2_FEATURE_GSM_MODEM
