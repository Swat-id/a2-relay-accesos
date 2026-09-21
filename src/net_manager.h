#pragma once

#include <Arduino.h>

// ============================================================================
// net_manager — estado agregado de conectividad y prioridad de interfaz
//
// API según la guía portable (docs/v5.0.0/GUIA-PORTABLE-WIFI-GSM-RED.md §4).
// La aplicación (MQTT, SNTP…) consulta esta capa y NUNCA una interfaz
// concreta. Prioridad de transporte: Ethernet > WiFi STA (> GSM, Fase 4).
//
// Los eventos de interfaz (main / wifi_manager) alimentan el estado con los
// callbacks netOn*. En este paso solo Ethernet está cableado; WiFi STA se
// conecta en la Fase 1 sin tocar a los consumidores.
// ============================================================================

enum NetIface : uint8_t {
  NET_IFACE_NONE = 0,
  NET_IFACE_ETH,
  NET_IFACE_WIFI_STA,
  NET_IFACE_GSM,        // datos 4G por PPP (A2v3)
};

// --- Eventos de interfaz (llamar desde los handlers de eventos) ---
void netOnEthGotIp();
void netOnEthDown();
void netOnWifiStaGotIp();
void netOnWifiStaDown();
void netOnGsmGotIp();
void netOnGsmDown();

// --- Consulta de estado ---
bool netEthUp();
bool netWifiStaUp();
bool netGsmUp();
bool netHasConnectivity();

// Interfaz preferida para MQTT/TCP saliente (ETH > WiFi STA > None)
NetIface netMqttPreferred();

// true si la interfaz preferida cambió desde la última consulta (failover /
// failback). El consumidor debe entonces reconectar su transporte.
bool netMqttIfaceChanged(NetIface* current);

// Nombre legible de la interfaz (para logs / JSON de estado)
const char* netIfaceName(NetIface iface);
