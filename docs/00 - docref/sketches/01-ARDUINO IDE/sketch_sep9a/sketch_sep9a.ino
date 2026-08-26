#include <Arduino.h>
#include <ETH.h>
#include <SPIFFS.h>
#include <Preferences.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ======================================================
// KC868-A2 / LAN8720 - Configuración Ethernet (tu base)
// ======================================================
#define ETH_PHY_ADDR       0
#define ETH_PHY_MDC        23
#define ETH_PHY_MDIO       18
#define ETH_PHY_POWER_PIN  5
#define ETH_PHY_TYPE       ETH_PHY_LAN8720
#define ETH_CLK_MODE       ETH_CLOCK_GPIO17_OUT

// ======================================================
// Persistencia
// ======================================================
Preferences prefs;
static const char* NVS_NS   = "cfg";
static const char* JSON_FILE = "/config.json";

// ======================================================
// Web
// ======================================================
AsyncWebServer server(80);

// Estado de red
static volatile bool eth_connected = false;

// ======================================================
// HTML
// ======================================================
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<title>KC868 Ethernet Test</title>
<style>
body{font-family:Arial;margin:20px} input{padding:6px;margin:4px 0} button{padding:8px 12px}
pre{background:#f4f4f4;padding:10px;border-radius:6px;white-space:pre-wrap}
</style></head><body>
<h2>KC868 - Ethernet DHCP + Guardado + Reinicio</h2>

<form method="POST" action="/save">
  <label>Device name</label><br>
  <input name="device" type="text" value="%DEVICE%"><br><br>

  <label>DI1 enabled</label>
  <input name="di1_en" type="checkbox" %DI1EN%><br>

  <label>DI1 relay</label><br>
  <input name="di1_relay" type="number" min="1" max="4" value="%DI1RELAY%"><br>

  <label>DI1 duration (ms)</label><br>
  <input name="di1_ms" type="number" min="50" max="60000" value="%DI1MS%"><br><br>

  <button type="submit">Guardar</button>
</form>

<br>
<a href="/restart"><button>Reiniciar equipo</button></a>
<hr>
<pre>%INFO%</pre>
</body></html>
)HTML";

// ======================================================
// NVS helpers
// ======================================================
String nvsGetString(const char* key, const String& def) {
  prefs.begin(NVS_NS, true);
  String v = prefs.getString(key, def);
  prefs.end();
  return v;
}
int nvsGetInt(const char* key, int def) {
  prefs.begin(NVS_NS, true);
  int v = prefs.getInt(key, def);
  prefs.end();
  return v;
}
bool nvsGetBool(const char* key, bool def) {
  prefs.begin(NVS_NS, true);
  bool v = prefs.getBool(key, def);
  prefs.end();
  return v;
}

void nvsSet(const String& device, bool di1en, int di1relay, int di1ms) {
  prefs.begin(NVS_NS, false);
  prefs.putString("device", device);
  prefs.putBool("di1en", di1en);
  prefs.putInt("di1relay", di1relay);
  prefs.putInt("di1ms", di1ms);
  prefs.end();
}

void saveJson(const String& device, bool di1en, int di1relay, int di1ms) {
  File f = SPIFFS.open(JSON_FILE, "w");
  if (!f) return;

  String j;
  j.reserve(220);
  j += "{";
  j += "\"device\":\"" + device + "\",";
  j += "\"di1_enabled\":" + String(di1en ? "true" : "false") + ",";
  j += "\"di1_relay\":" + String(di1relay) + ",";
  j += "\"di1_ms\":" + String(di1ms);
  j += "}";

  f.print(j);
  f.close();
}

// ======================================================
// Eventos Ethernet (core 3.x usa ARDUINO_EVENT_*)
// ======================================================
void onNetEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("🟢 ETH START");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("🟢 ETH CONNECTED (link up)");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      eth_connected = true;
      Serial.println("✅ ETH GOT IP (DHCP)");
      Serial.print("   IP : "); Serial.println(ETH.localIP());
      Serial.print("   GW : "); Serial.println(ETH.gatewayIP());
      Serial.print("   MSK: "); Serial.println(ETH.subnetMask());
      Serial.print("   MAC: "); Serial.println(ETH.macAddress());
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      eth_connected = false;
      Serial.println("🟠 ETH DISCONNECTED (link down)");
      break;

    case ARDUINO_EVENT_ETH_STOP:
      eth_connected = false;
      Serial.println("🔴 ETH STOP");
      break;

    default:
      break;
  }
}

// ======================================================
// Web page builder
// ======================================================
String buildIndexPage() {
  String device  = nvsGetString("device", "KC868-A2");
  bool di1en     = nvsGetBool("di1en", false);
  int di1relay   = nvsGetInt("di1relay", 1);
  int di1ms      = nvsGetInt("di1ms", 2000);

  String info;
  info.reserve(800);

  info += "Ethernet:\n";
  info += String("  Link: ") + (ETH.linkUp() ? "UP" : "DOWN") + "\n";
  info += String("  Connected: ") + (eth_connected ? "YES" : "NO") + "\n";
  info += String("  IP: ") + ETH.localIP().toString() + "\n\n";

  info += "NVS (Preferences):\n";
  info += "  device: " + device + "\n";
  info += "  di1_enabled: " + String(di1en ? "true" : "false") + "\n";
  info += "  di1_relay: " + String(di1relay) + "\n";
  info += "  di1_ms: " + String(di1ms) + "\n\n";

  info += "SPIFFS JSON:\n";
  if (SPIFFS.exists(JSON_FILE)) {
    File f = SPIFFS.open(JSON_FILE, "r");
    if (f) { info += f.readString(); f.close(); }
    else { info += "<error abriendo archivo>"; }
  } else {
    info += "<no existe>";
  }
  info += "\n";

  String page = FPSTR(INDEX_HTML);
  page.replace("%DEVICE%", device);
  page.replace("%DI1EN%", di1en ? "checked" : "");
  page.replace("%DI1RELAY%", String(di1relay));
  page.replace("%DI1MS%", String(di1ms));
  page.replace("%INFO%", info);

  return page;
}

// ======================================================
// Setup Ethernet con firma correcta (core 3.3.3)
// ======================================================
void setupEthernet() {
  Serial.println("🔧 Iniciando Ethernet LAN8720 (DHCP) ...");

  // Registrar eventos
  WiFi.onEvent(onNetEvent);

  // FIRMA CORRECTA EN TU CORE (esp32 3.3.3):
  // begin(type, phy_addr, mdc, mdio, power, clk_mode)
  bool ok = ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER_PIN, ETH_CLK_MODE);

  Serial.print("ETH.begin(): ");
  Serial.println(ok ? "OK" : "FALLO");

  // Espera hasta 10s a IP (se imprimirá por evento GOT_IP)
  uint32_t t0 = millis();
  while (!eth_connected && (millis() - t0) < 10000) {
    delay(100);
  }

  if (!eth_connected) {
    Serial.println("⚠️ No hay IP aún (revisa cable/DHCP). El servidor web arrancará igualmente.");
  }
}

// ======================================================
// Setup Web
// ======================================================
void setupWeb() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html; charset=utf-8", buildIndexPage());
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest* request) {
    String device = "KC868-A2";
    bool di1en = false;
    int di1relay = 1;
    int di1ms = 2000;

    if (request->hasParam("device", true))
      device = request->getParam("device", true)->value();

    di1en = request->hasParam("di1_en", true);

    if (request->hasParam("di1_relay", true))
      di1relay = request->getParam("di1_relay", true)->value().toInt();

    if (request->hasParam("di1_ms", true))
      di1ms = request->getParam("di1_ms", true)->value().toInt();

    // Sanitizado
    if (di1relay < 1) di1relay = 1;
    if (di1relay > 4) di1relay = 4;
    if (di1ms < 50) di1ms = 50;
    if (di1ms > 60000) di1ms = 60000;

    nvsSet(device, di1en, di1relay, di1ms);
    saveJson(device, di1en, di1relay, di1ms);

    Serial.println("💾 Guardado OK (NVS + SPIFFS)");
    request->redirect("/");
  });

  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain; charset=utf-8", "Reiniciando...");
    delay(250);
    ESP.restart();
  });

  server.begin();
  Serial.println("🌐 Servidor HTTP (Async) iniciado en puerto 80");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("=== KC868 Ethernet DHCP + Persistencia (Async) ===");

  if (!SPIFFS.begin(true)) Serial.println("❌ SPIFFS mount failed");
  else Serial.println("✅ SPIFFS OK");

  setupEthernet();
  setupWeb();

  Serial.println("✅ Listo. Abre la IP mostrada por Serial en el navegador.");
}

void loop() {
  // AsyncWebServer no necesita handleClient()
}