#include "net_manager.h"

#include <esp_arduino_version.h>

// En core Arduino 3.x (target S3) se fija el netif por defecto explícitamente
// para garantizar la prioridad ETH > WiFi STA también con ambas interfaces
// levantadas (en core 2.x lwIP puede preferir STA; limitación documentada).
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define NET_HAS_SET_DEFAULT 1
#include <ETH.h>
#include <WiFi.h>
#else
#define NET_HAS_SET_DEFAULT 0
#endif

// Estado propio, alimentado exclusivamente por los callbacks netOn*.
// (No se consulta ETH./WiFi. directamente: así la capa sirve igual para
// LAN8720, W5500 o STA sin ifdefs en los consumidores.)
static bool s_ethUp = false;
static bool s_wifiStaUp = false;
static NetIface s_lastIface = NET_IFACE_NONE;

static void applyDefaultNetif() {
#if NET_HAS_SET_DEFAULT
  if (s_ethUp) {
    ETH.setDefault();
  } else if (s_wifiStaUp) {
    WiFi.STA.setDefault();
  }
#endif
}

void netOnEthGotIp() {
  s_ethUp = true;
  applyDefaultNetif();
}

void netOnEthDown() {
  s_ethUp = false;
  applyDefaultNetif();
}

void netOnWifiStaGotIp() {
  s_wifiStaUp = true;
  applyDefaultNetif();
}

void netOnWifiStaDown() {
  s_wifiStaUp = false;
  applyDefaultNetif();
}

bool netEthUp() {
  return s_ethUp;
}

bool netWifiStaUp() {
  return s_wifiStaUp;
}

bool netHasConnectivity() {
  return s_ethUp || s_wifiStaUp;
}

NetIface netMqttPreferred() {
  if (s_ethUp) return NET_IFACE_ETH;
  if (s_wifiStaUp) return NET_IFACE_WIFI_STA;
  return NET_IFACE_NONE;
}

bool netMqttIfaceChanged(NetIface* current) {
  NetIface now = netMqttPreferred();
  if (current) *current = now;
  if (now != s_lastIface) {
    s_lastIface = now;
    return true;
  }
  return false;
}

const char* netIfaceName(NetIface iface) {
  switch (iface) {
    case NET_IFACE_ETH:      return "eth";
    case NET_IFACE_WIFI_STA: return "wifi";
    default:                 return "none";
  }
}
