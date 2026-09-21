#include "wifi_manager.h"

#if A2_FEATURE_WIFI_MGMT

#include <WiFi.h>
#include <ETH.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "net_manager.h"
#include "web_common.h"

// Servicios de main.ino
extern WebServer server;
extern char deviceName[32];
extern PubSubClient mqttClient;

// ---------------------------------------------------------------------------
// Configuración (NVS namespace "a2acc_wifi")
// ---------------------------------------------------------------------------
#define WIFI_NVS_NS "a2acc_wifi"

static String   s_deviceId;            // SSID del AP
static uint8_t  s_apMode = WIFI_AP_MODE_BOOT_WINDOW;
static uint32_t s_apTimeoutS = WIFI_AP_WINDOW_S_DEFAULT;
static String   s_apPass = WIFI_AP_PASS_DEFAULT;
static uint8_t  s_staEn = 0;
static String   s_staSsid;
static String   s_staPass;

// Estado runtime
static bool     s_apActive = false;
static int32_t  s_apRemainingS = 0;    // -1 = sin cuenta atrás
static bool     s_staConnected = false;
static bool     s_mdnsStarted = false;
static bool     s_scanning = false;
static unsigned long s_scanStartedAt = 0;
static String   s_lastScanJson = "{\"scanning\":false,\"networks\":[]}";
static unsigned long s_lastTick = 0;
static unsigned long s_lastStaRetry = 0;

static void saveConfig() {
  Preferences p;
  if (!p.begin(WIFI_NVS_NS, false)) {
    Serial.println("❌ [WIFI] No se pudo abrir NVS para guardar");
    return;
  }
  p.putUChar("ap_mode", s_apMode);
  p.putUInt("ap_to", s_apTimeoutS);
  p.putString("ap_pass", s_apPass);
  p.putUChar("sta_en", s_staEn);
  p.putString("sta_ssid", s_staSsid);
  p.putString("sta_pass", s_staPass);
  p.end();
}

static void loadConfig() {
  Preferences p;
  if (!p.begin(WIFI_NVS_NS, true)) return;  // primera vez: defaults
  s_apMode    = p.getUChar("ap_mode", WIFI_AP_MODE_BOOT_WINDOW);
  s_apTimeoutS = p.getUInt("ap_to", WIFI_AP_WINDOW_S_DEFAULT);
  s_apPass    = p.getString("ap_pass", WIFI_AP_PASS_DEFAULT);
  s_staEn     = p.getUChar("sta_en", 0);
  s_staSsid   = p.getString("sta_ssid", "");
  s_staPass   = p.getString("sta_pass", "");
  p.end();
}

// ---------------------------------------------------------------------------
// Modo radio: aplica WIFI_OFF / AP / STA / AP_STA según necesidad actual
// ---------------------------------------------------------------------------
static bool staWanted() { return s_staEn && s_staSsid.length() > 0; }

static void applyRadioMode() {
  bool needAp = s_apActive;
  bool needSta = staWanted() || s_scanning;  // el escaneo necesita interfaz STA

  wifi_mode_t mode = WIFI_OFF;
  if (needAp && needSta) mode = WIFI_AP_STA;
  else if (needAp)       mode = WIFI_AP;
  else if (needSta)      mode = WIFI_STA;

  WiFi.mode(mode);
  if (mode != WIFI_OFF) {
    WiFi.setSleep(false);  // entorno industrial: sin modem-sleep
  }
}

static void startStaConnect() {
  if (!staWanted()) return;
  Serial.printf("📶 [WIFI] Conectando STA a '%s'…\n", s_staSsid.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.begin(s_staSsid.c_str(), s_staPass.c_str());
  s_lastStaRetry = millis();
}

// ---------------------------------------------------------------------------
// Eventos STA → net_manager (failover MQTT ya cableado en main)
// ---------------------------------------------------------------------------
static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      s_staConnected = true;
      netOnWifiStaGotIp();
      Serial.printf("📶 [WIFI] STA conectada: %s (RSSI %d dBm)\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (s_staConnected) {
        Serial.println("📶 [WIFI] STA desconectada");
      }
      s_staConnected = false;
      netOnWifiStaDown();
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.println("📶 [WIFI] Cliente conectado al AP");
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.println("📶 [WIFI] Cliente desconectado del AP");
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------
void wifiApStartNow(uint32_t seconds) {
  s_apActive = true;
  s_apRemainingS = (s_apMode == WIFI_AP_MODE_ALWAYS_ON)
                       ? -1
                       : (int32_t)(seconds ? seconds : s_apTimeoutS);
  applyRadioMode();
  WiFi.softAP(s_deviceId.c_str(), s_apPass.c_str());

  Serial.printf("📶 [WIFI] AP activo: SSID '%s', IP %s",
                s_deviceId.c_str(), WiFi.softAPIP().toString().c_str());
  if (s_apRemainingS > 0) Serial.printf(" (ventana %ld s, se extiende con clientes)", (long)s_apRemainingS);
  Serial.println();

  if (!s_mdnsStarted && MDNS.begin(deviceName)) {
    s_mdnsStarted = true;
    MDNS.addService("http", "tcp", 80);
    Serial.printf("📶 [WIFI] mDNS: http://%s.local\n", deviceName);
  }
}

void wifiApStopNow() {
  if (!s_apActive) return;
  s_apActive = false;
  s_apRemainingS = 0;
  WiFi.softAPdisconnect(true);
  applyRadioMode();
  Serial.println("📶 [WIFI] AP apagado");
}

bool wifiApActive() { return s_apActive; }
int wifiApClients() { return s_apActive ? WiFi.softAPgetStationNum() : 0; }
int wifiApRemainingS() { return s_apActive ? s_apRemainingS : 0; }

bool wifiStaConnected() { return s_staConnected; }

bool wifiStaConnectTo(const String& ssid, const String& pass) {
  if (ssid.length() == 0 || ssid.length() > 32) return false;
  s_staSsid = ssid;
  s_staPass = pass;
  s_staEn = 1;
  saveConfig();
  applyRadioMode();
  startStaConnect();
  return true;
}

void wifiStaForget() {
  s_staEn = 0;
  s_staSsid = "";
  s_staPass = "";
  saveConfig();
  s_staConnected = false;
  netOnWifiStaDown();
  WiFi.disconnect(false /*no apagar radio: puede haber AP*/);
  applyRadioMode();
  Serial.println("📶 [WIFI] Credenciales STA borradas");
}

void wifiStaSetEnabled(bool en) {
  s_staEn = en ? 1 : 0;
  saveConfig();
  if (en) {
    applyRadioMode();
    startStaConnect();
  } else {
    s_staConnected = false;
    netOnWifiStaDown();
    WiFi.disconnect(false);
    applyRadioMode();
  }
}

bool wifiSetApMode(uint8_t mode, uint32_t timeout_s) {
  if (mode > WIFI_AP_MODE_ALWAYS_ON) return false;
  if (timeout_s < 10 || timeout_s > 3600) timeout_s = WIFI_AP_WINDOW_S_DEFAULT;
  s_apMode = mode;
  s_apTimeoutS = timeout_s;
  saveConfig();
  // Aplicar en caliente
  if (mode == WIFI_AP_MODE_ALWAYS_ON) {
    if (!s_apActive) wifiApStartNow(0);
    s_apRemainingS = -1;
  } else if (mode == WIFI_AP_MODE_DISABLED) {
    wifiApStopNow();
  } else if (s_apActive && s_apRemainingS == -1) {
    s_apRemainingS = s_apTimeoutS;  // de always_on a ventana
  }
  return true;
}

bool wifiSetApPass(const String& pass) {
  if (pass.length() < 8 || pass.length() > 63) return false;
  s_apPass = pass;
  saveConfig();
  if (s_apActive) WiFi.softAP(s_deviceId.c_str(), s_apPass.c_str());  // reaplicar
  return true;
}

void wifiScanStart() {
  if (s_scanning) return;
  s_scanning = true;
  s_scanStartedAt = millis();
  applyRadioMode();               // asegura interfaz STA para escanear
  WiFi.scanNetworks(true /*async*/);
  Serial.println("📶 [WIFI] Escaneo de redes iniciado");
}

// Serializa los resultados del escaneo y los cachea (los resultados en el
// driver pueden perderse al reajustar el modo radio)
static void cacheScanResults() {
  DynamicJsonDocument doc(2048);
  int n = WiFi.scanComplete();
  doc["scanning"] = false;
  JsonArray nets = doc.createNestedArray("networks");
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject e = nets.createNestedObject();
    e["ssid"] = WiFi.SSID(i);
    e["rssi"] = WiFi.RSSI(i);
    e["enc"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }
  s_lastScanJson = "";
  serializeJson(doc, s_lastScanJson);
  WiFi.scanDelete();
}

// ---------------------------------------------------------------------------
// Arranque y tick
// ---------------------------------------------------------------------------
void wifiManagerBegin(const String& deviceId) {
  s_deviceId = deviceId;
  loadConfig();
  WiFi.onEvent(onWifiEvent);

  Serial.printf("📶 [WIFI] Config: AP modo=%u timeout=%lus | STA %s '%s'\n",
                s_apMode, (unsigned long)s_apTimeoutS,
                s_staEn ? "ON" : "OFF", s_staSsid.c_str());

  if (s_apMode != WIFI_AP_MODE_DISABLED) {
    wifiApStartNow(0);  // boot_window o always_on
  }
  if (staWanted()) {
    applyRadioMode();
    startStaConnect();
  }
}

void wifiManagerLoop() {
  unsigned long now = millis();
  if (now - s_lastTick < 1000) return;   // tick 1 s
  s_lastTick = now;

  // Fin de escaneo: cachear resultados ANTES de reajustar el modo radio
  if (s_scanning) {
    int sc = WiFi.scanComplete();
    if (sc >= 0) {
      Serial.printf("📶 [WIFI] Escaneo completado: %d redes\n", sc);
      cacheScanResults();
      s_scanning = false;
      applyRadioMode();  // si la STA no hace falta, volver a solo-AP
    } else if (now - s_scanStartedAt > 15000) {
      Serial.println("⚠️ [WIFI] Escaneo sin resultado (timeout)");
      s_scanning = false;
      applyRadioMode();
    }
  }

  // Ventana del AP: se rearma mientras haya clientes conectados
  if (s_apActive && s_apRemainingS >= 0) {
    if (WiFi.softAPgetStationNum() > 0) {
      s_apRemainingS = s_apTimeoutS;
    } else if (--s_apRemainingS <= 0) {
      Serial.println("📶 [WIFI] Ventana del AP expirada sin clientes");
      wifiApStopNow();
    }
  }

  // Red de seguridad STA: si autoReconnect no lo consigue, reintento cada 30 s
  if (staWanted() && !s_staConnected && (now - s_lastStaRetry > 30000)) {
    startStaConnect();
  }
}

// ---------------------------------------------------------------------------
// Estado JSON (compartido por /api/wifi/status y MQTT get_wifi). Sin passwords.
// ---------------------------------------------------------------------------
String wifiStatusJson() {
  DynamicJsonDocument doc(768);
  JsonObject ap = doc.createNestedObject("ap");
  ap["active"] = s_apActive;
  ap["mode"] = s_apMode;                 // 0=disabled 1=boot_window 2=always_on
  ap["timeout_s"] = s_apTimeoutS;
  ap["remaining_s"] = wifiApRemainingS();
  ap["clients"] = wifiApClients();
  ap["ssid"] = s_deviceId;
  ap["ip"] = s_apActive ? WiFi.softAPIP().toString() : "";
  ap["pass_default"] = (s_apPass == WIFI_AP_PASS_DEFAULT);

  JsonObject sta = doc.createNestedObject("sta");
  sta["enabled"] = (bool)s_staEn;
  sta["ssid"] = s_staSsid;
  sta["connected"] = s_staConnected;
  sta["ip"] = s_staConnected ? WiFi.localIP().toString() : "";
  sta["rssi"] = s_staConnected ? WiFi.RSSI() : 0;

  doc["eth_up"] = netEthUp();
  doc["eth_ip"] = netEthUp() ? ETH.localIP().toString() : "";
  doc["gsm_up"] = netGsmUp();
  doc["mqtt_iface"] = netIfaceName(netMqttPreferred());
  doc["mqtt_connected"] = mqttClient.connected();

  String out;
  serializeJson(doc, out);
  return out;
}

String wifiScanJson() {
  if (s_scanning) return String("{\"scanning\":true,\"networks\":[]}");
  return s_lastScanJson;
}

// ---------------------------------------------------------------------------
// Web: página /wifi + APIs (usa el CSS común de web_common)
// ---------------------------------------------------------------------------
static void handleWifiStatusApi() {
  if (!webAuth()) return;
  server.send(200, "application/json", wifiStatusJson());
}

static void handleWifiScanApi() {
  if (!webAuth()) return;
  if (server.hasArg("start")) wifiScanStart();
  server.send(200, "application/json", wifiScanJson());
}

static void handleWifiPost() {
  if (!webAuth()) return;
  String action = server.arg("action");
  String msg = "ok";

  if (action == "ap_on") {
    wifiApStartNow(server.arg("seconds").toInt());
  } else if (action == "ap_off") {
    wifiApStopNow();
  } else if (action == "save_ap") {
    if (!wifiSetApMode(server.arg("ap_mode").toInt(), server.arg("ap_timeout").toInt()))
      msg = "modo AP invalido";
  } else if (action == "set_ap_pass") {
    if (!wifiSetApPass(server.arg("ap_pass")))
      msg = "password minimo 8 caracteres";
  } else if (action == "connect_sta") {
    if (!wifiStaConnectTo(server.arg("ssid"), server.arg("pass")))
      msg = "ssid invalido";
  } else if (action == "forget_sta") {
    wifiStaForget();
  } else {
    msg = "accion desconocida";
  }

  server.sendHeader("Location", "/wifi?msg=" + msg);
  server.send(303);
}

static void handleWifiPage() {
  if (!webAuth()) return;

  String html = webPageBegin("WiFi - Controladora A2");
  html += "<header><h1>📶 Red WiFi</h1><p>" + s_deviceId + "</p></header><main><div class='container'>";

  if (server.hasArg("msg") && server.arg("msg") != "ok") {
    html += "<div class='status not-synced'>⚠️ " + server.arg("msg") + "</div>";
  }

  // Estado (rellenado por JS con poll de 3 s)
  html += R"=====(
<h2>Estado</h2>
<table>
 <tr><th>Interfaz</th><th>Estado</th><th>Detalle</th></tr>
 <tr><td>Ethernet</td><td id='ethSt'>…</td><td>prioridad 1 para MQTT</td></tr>
 <tr><td>WiFi STA</td><td id='staSt'>…</td><td id='staDet'></td></tr>
 <tr><td>AP</td><td id='apSt'>…</td><td id='apDet'></td></tr>
 <tr><td>MQTT vía</td><td id='mqttIf' colspan='2'>…</td></tr>
</table>

<h2>Punto de acceso (AP)</h2>
<div class='card'>
 <form method='post' action='/wifi'>
  <input type='hidden' name='action' value='save_ap'>
  <label>Modo</label>
  <select name='ap_mode' id='apMode'>
   <option value='0'>Desactivado</option>
   <option value='1'>Ventana al arrancar</option>
   <option value='2'>Siempre activo</option>
  </select>
  <label>Ventana (segundos, 10-3600)</label>
  <input type='number' name='ap_timeout' id='apTimeout' min='10' max='3600'>
  <button type='submit'>Guardar modo AP</button>
 </form>
 <form method='post' action='/wifi'>
  <input type='hidden' name='action' value='ap_on'>
  <button type='submit' class='btn-success'>Activar AP ahora</button>
 </form>
 <form method='post' action='/wifi'>
  <input type='hidden' name='action' value='ap_off'>
  <button type='submit' class='btn-warning'>Apagar AP</button>
 </form>
 <form method='post' action='/wifi'>
  <input type='hidden' name='action' value='set_ap_pass'>
  <label>Nueva contraseña del AP (mín. 8)</label>
  <input type='text' name='ap_pass' minlength='8' maxlength='63' required>
  <button type='submit' class='btn-warning'>Cambiar contraseña AP</button>
 </form>
</div>

<h2>Cliente WiFi (STA)</h2>
<div class='card'>
 <button onclick='doScan()' class='btn-info'>🔍 Escanear redes</button>
 <div id='scanRes'></div>
 <form method='post' action='/wifi'>
  <input type='hidden' name='action' value='connect_sta'>
  <label>SSID</label>
  <input type='text' name='ssid' id='ssidField' maxlength='32' required>
  <label>Contraseña</label>
  <input type='text' name='pass' maxlength='63'>
  <button type='submit' class='btn-success'>Conectar y guardar</button>
 </form>
 <form method='post' action='/wifi'>
  <input type='hidden' name='action' value='forget_sta'>
  <button type='submit' class='btn-danger'>Olvidar red</button>
 </form>
</div>
<div class='button-group'><a href='/'><button class='btn-secondary'>🏠 Inicio</button></a></div>

<script>
function upd(){fetch('/api/wifi/status').then(r=>r.json()).then(s=>{
 document.getElementById('ethSt').textContent=s.eth_up?'✅ conectado':'❌ sin enlace';
 document.getElementById('staSt').textContent=s.sta.enabled?(s.sta.connected?'✅ conectada':'⏳ conectando'):'—';
 document.getElementById('staDet').textContent=s.sta.ssid?(s.sta.ssid+(s.sta.connected?' · '+s.sta.ip+' · '+s.sta.rssi+' dBm':'')):'sin configurar';
 document.getElementById('apSt').textContent=s.ap.active?'✅ activo':'—';
 document.getElementById('apDet').textContent=s.ap.active?(s.ap.clients+' clientes'+(s.ap.remaining_s>=0?' · quedan '+s.ap.remaining_s+' s':' · siempre activo')+' · '+s.ap.ip):'';
 document.getElementById('mqttIf').textContent=s.mqtt_iface;
 if(!window._cfgSet){document.getElementById('apMode').value=s.ap.mode;
  document.getElementById('apTimeout').value=s.ap.timeout_s;window._cfgSet=1;}
}).catch(()=>{});}
function doScan(){fetch('/api/wifi/scan?start=1').then(()=>setTimeout(pollScan,2000));}
function pollScan(){fetch('/api/wifi/scan').then(r=>r.json()).then(s=>{
 if(s.scanning){setTimeout(pollScan,1500);return;}
 let h='<table><tr><th>SSID</th><th>Señal</th><th></th></tr>';
 s.networks.forEach(n=>{h+='<tr><td>'+n.ssid+'</td><td>'+n.rssi+' dBm'+(n.enc?' 🔒':'')+
  "</td><td><button onclick=\"document.getElementById('ssidField').value='"+n.ssid+"'\">Usar</button></td></tr>";});
 document.getElementById('scanRes').innerHTML=h+'</table>';});}
upd();setInterval(upd,3000);
</script>
)=====";

  html += "</div></main>";
  html += WEB_PAGE_END;
  server.send(200, "text/html", html);
}

void wifiRegisterWebRoutes() {
  server.on("/wifi", HTTP_GET, handleWifiPage);
  server.on("/wifi", HTTP_POST, handleWifiPost);
  server.on("/api/wifi/status", HTTP_GET, handleWifiStatusApi);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScanApi);
}

#endif  // A2_FEATURE_WIFI_MGMT
