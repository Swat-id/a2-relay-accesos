#include "gsm_modem.h"

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

extern WebServer server;

// Pines y UART por target: ver hw_config.h
//   A2 clásico: TX=13 RX=34 (input-only → sin swap), UART1 (UART2 = RS485)
//   A2v3:       TX=10 RX=9  (swap auto),             UART2 (UART1 = RS485)
#include "hw_config.h"

#define GSM_BAUD 115200
#define GSM_NVS_NS "a2acc_gsm"
#define GSM_PROBE_ROUNDS 6      // intentos de AT antes de declarar Absent
#define GSM_TICK_MS 500
#define GSM_CMD_TIMEOUT_MS 1000
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
static String s_rxBuf;
static uint8_t s_pollStep = 0;

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

static void sendCmd(const char* cmd) {
  s_rxBuf = "";
  while (gsmSerial.available()) gsmSerial.read();  // vaciar restos
  gsmSerial.print(cmd);
  gsmSerial.print("\r\n");
  s_cmdSentAt = millis();
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
  if (millis() - s_cmdSentAt > GSM_CMD_TIMEOUT_MS) {
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
  CMD_NONE = 0, CMD_PROBE, CMD_ATI, CMD_GSN, CMD_CPIN_Q, CMD_CPIN_SET,
  CMD_CEREG, CMD_CREG, CMD_CSQ, CMD_COPS
};
static GsmCmd s_cmd = CMD_NONE;

static void gotoState(GsmState st) {
  if (s_st.state != st) {
    Serial.printf("📡 [GSM] %s → %s\n", gsmStateName(s_st.state), gsmStateName(st));
    s_st.state = st;
  }
}

static void startProbe() {
  s_probeCount = 0;
  s_probeSwapped = s_st.pin_swap;   // empezar por el mapeo aprendido
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
  startProbe();
  Serial.printf("📡 [GSM] Probe iniciado (TX=%d RX=%d%s)\n", GSM_TX_PIN, GSM_RX_PIN,
                GSM_SWAP_POSSIBLE ? ", swap auto" : "");
}

void gsmModemRescan() {
  if (!s_en) return;
  Serial.println("📡 [GSM] Rescan manual");
  s_pinTried = false;
  startProbe();
}

const GsmStatus& gsmModemStatus() { return s_st; }

bool gsmSetEnabled(bool en) {
  s_en = en ? 1 : 0;
  saveConfig();
  if (en) gsmModemRescan();
  else { gotoState(GSM_DISABLED); s_cmd = CMD_NONE; }
  return true;
}

bool gsmSetPin(const String& pin) {
  if (pin.length() > 8) return false;
  for (unsigned i = 0; i < pin.length(); i++)
    if (!isDigit(pin[i])) return false;
  s_pin = pin;
  s_st.pin_set = s_pin.length() > 0;
  s_pinTried = false;   // nuevo valor: se permite UN intento
  saveConfig();
  return true;
}

bool gsmSetApn(uint8_t apn_mode, const String& apn, const String& user, const String& pass) {
  s_apnMode = apn_mode ? 1 : 0;
  s_apn = apn;
  s_apnUser = user;
  s_apnPass = pass;
  saveConfig();
  return true;
}

// ---------------------------------------------------------------------------
// FSM principal (tick cada GSM_TICK_MS; cada comando AT con timeout corto)
// ---------------------------------------------------------------------------
void gsmModemLoop() {
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
          s_cmd = CMD_ATI;
          sendCmd("ATI");
          return;
        }
        if (++s_probeCount >= GSM_PROBE_ROUNDS) {
          gotoState(GSM_ABSENT);
          Serial.println("📡 [GSM] Sin módulo (probe agotado); rescan manual disponible");
          s_cmd = CMD_NONE;
          return;
        }
#if GSM_SWAP_POSSIBLE
        s_probeSwapped = !s_probeSwapped;
        openUart(s_probeSwapped);
#endif
        s_cmd = CMD_NONE;   // el próximo tick reenvía AT
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
        gotoState(GSM_SIM_CHECK);
        s_cmd = CMD_CPIN_Q;
        sendCmd("AT+CPIN?");
        return;

      case CMD_CPIN_Q:
        if (s_rxBuf.indexOf("READY") >= 0) {
          gotoState(GSM_REGISTERING);
          s_cmd = CMD_NONE;
        } else if (s_rxBuf.indexOf("SIM PIN") >= 0) {
          if (s_pin.length() > 0 && !s_pinTried) {
            s_pinTried = true;   // UN intento por arranque y valor
            Serial.println("📡 [GSM] Enviando PIN almacenado (intento único)");
            s_cmd = CMD_CPIN_SET;
            sendCmd(("AT+CPIN=" + s_pin).c_str());
          } else {
            gotoState(s_pinTried ? GSM_PIN_ERROR : GSM_PIN_REQUIRED);
            s_cmd = CMD_NONE;
          }
        } else if (s_rxBuf.indexOf("SIM PUK") >= 0) {
          gotoState(GSM_PUK_REQUIRED);
          s_cmd = CMD_NONE;
        } else {
          gotoState(GSM_NO_SIM);
          s_cmd = CMD_NONE;
        }
        return;

      case CMD_CPIN_SET:
        if (ok) {
          Serial.println("📡 [GSM] PIN aceptado");
          gotoState(GSM_REGISTERING);
        } else {
          Serial.println("❌ [GSM] PIN RECHAZADO - sin reintentos (riesgo PUK)");
          gotoState(GSM_PIN_ERROR);
        }
        s_cmd = CMD_NONE;
        return;

      case CMD_CEREG:
      case CMD_CREG: {
        // +CEREG: n,stat  → stat 1 (home) o 5 (roaming) = registrado
        int comma = s_rxBuf.lastIndexOf(',');
        int stat = (comma > 0) ? s_rxBuf.substring(comma + 1).toInt() : 0;
        if (stat == 1 || stat == 5) {
          if (s_st.state != GSM_ATTACHED) {
            gotoState(GSM_ATTACHED);
            Serial.println("📡 [GSM] Registrado en red");
          }
        } else if (s_cmd == CMD_CEREG) {
          s_cmd = CMD_CREG;             // sin LTE: probar 2G/3G
          sendCmd("AT+CREG?");
          return;
        } else if (s_st.state == GSM_ATTACHED) {
          gotoState(GSM_REGISTERING);   // perdió registro
        }
        s_cmd = CMD_NONE;
        return;
      }

      case CMD_CSQ: {
        int p = s_rxBuf.indexOf("+CSQ:");
        if (p >= 0) {
          int v = s_rxBuf.substring(p + 5).toInt();
          s_st.csq = (v >= 0 && v <= 31) ? (int8_t)v : -1;
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
      s_cmd = CMD_PROBE;
      sendCmd("AT");
      break;

    case GSM_REGISTERING:
      if (now - s_lastPoll >= 2000) {
        s_lastPoll = now;
        s_cmd = CMD_CEREG;
        sendCmd("AT+CEREG?");
      }
      break;

    case GSM_ATTACHED:
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
  if (s_st.csq >= 0) doc["rssi_dbm"] = -113 + 2 * s_st.csq;
  doc["pin_set"] = s_st.pin_set;       // NUNCA el PIN
  doc["pin_swap"] = s_st.pin_swap;
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
 <tr><td>Cobertura</td><td id='gCsq'>…</td></tr>
 <tr><td>PIN</td><td id='gPin'>…</td></tr>
 <tr><td>Mapeo TX/RX</td><td id='gSwap'>…</td></tr>
</table>
<div class='button-group'>
 <form method='post' action='/gsm' style='display:inline'>
  <input type='hidden' name='action' value='rescan'>
  <button type='submit' class='btn-info'>🔄 Re-detectar módulo</button>
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
 document.getElementById('gCsq').textContent=csqTxt(s.csq,s.rssi_dbm);
 document.getElementById('gPin').textContent=s.pin_set?'🔑 configurado':'— sin PIN';
 document.getElementById('gSwap').textContent=s.pin_swap?'invertido (auto)':'directo';
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
