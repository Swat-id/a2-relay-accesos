#include "net_manager.h"

#include <esp_arduino_version.h>

// En core Arduino 3.x (target S3) se fija el netif por defecto explícitamente
// para garantizar la prioridad ETH > WiFi STA también con ambas interfaces
// levantadas (en core 2.x lwIP puede preferir STA; limitación documentada).
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define NET_HAS_SET_DEFAULT 1
#include <ETH.h>
#include <WiFi.h>
#include "target_features.h"
#if A2_BOARD_A2V3
#include <PPP.h>
#define NET_HAS_PPP 1
#endif
#else
#define NET_HAS_SET_DEFAULT 0
#endif
#ifndef NET_HAS_PPP
#define NET_HAS_PPP 0
#endif

// Estado propio, alimentado exclusivamente por los callbacks netOn*.
// (No se consulta ETH./WiFi. directamente: así la capa sirve igual para
// LAN8720, W5500 o STA sin ifdefs en los consumidores.)
static bool s_ethUp = false;
static bool s_wifiStaUp = false;
static bool s_gsmUp = false;
static NetIface s_lastIface = NET_IFACE_NONE;

static void applyDefaultNetif() {
#if NET_HAS_SET_DEFAULT
  if (s_ethUp) {
    ETH.setDefault();
  } else if (s_wifiStaUp) {
    WiFi.STA.setDefault();
#if NET_HAS_PPP
  } else if (s_gsmUp) {
    PPP.setDefault();
#endif
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

void netOnGsmGotIp() {
  s_gsmUp = true;
  applyDefaultNetif();
}

void netOnGsmDown() {
  s_gsmUp = false;
  applyDefaultNetif();
}

bool netEthUp() {
  return s_ethUp;
}

bool netWifiStaUp() {
  return s_wifiStaUp;
}

bool netGsmUp() {
  return s_gsmUp;
}

bool netHasConnectivity() {
  return s_ethUp || s_wifiStaUp || s_gsmUp;
}

NetIface netMqttPreferred() {
  if (s_ethUp) return NET_IFACE_ETH;
  if (s_wifiStaUp) return NET_IFACE_WIFI_STA;
  if (s_gsmUp) return NET_IFACE_GSM;
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
    case NET_IFACE_GSM:      return "4g";
    default:                 return "none";
  }
}
