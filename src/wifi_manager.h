#pragma once

#include <Arduino.h>
#include "target_features.h"

// ============================================================================
// wifi_manager — gestión WiFi completa (Fase 1 del plan v5.0.0)
//
// AP de gestión:
//   - SSID = id del equipo (serial fijo), contraseña por defecto "admin1234".
//   - Modo por defecto boot_window: AP activo WIFI_AP_WINDOW_S tras arrancar;
//     mientras haya algún cliente conectado la cuenta atrás se rearma; sin
//     clientes, al expirar se apaga. Modos always_on y disabled configurables.
//   - Activable en caliente desde web y MQTT (wifiApStartNow).
//
// STA (cliente WiFi):
//   - Credenciales en NVS (namespace a2acc_wifi); reconexión automática con
//     reintento de fondo. Eventos GOT_IP/DISCONNECTED alimentan net_manager
//     (prioridad ETH > WiFi y failover MQTT ya cableados en Fase 0.75).
//
// Escaneo de redes asíncrono (no bloquea el loop de accesos).
// Todo no-bloqueante: la caída de uno o todos los interfaces nunca detiene
// la lógica local (regla de campo del plan).
// ============================================================================

#if A2_FEATURE_WIFI_MGMT

// Modos del AP (persistidos en NVS)
enum WifiApMode : uint8_t {
  WIFI_AP_MODE_DISABLED   = 0,
  WIFI_AP_MODE_BOOT_WINDOW = 1,  // por defecto
  WIFI_AP_MODE_ALWAYS_ON  = 2,
};

// Ventana por defecto del AP al arrancar (spec: 1 minuto)
#define WIFI_AP_WINDOW_S_DEFAULT 60
// Contraseña de fábrica del AP (cambiable por web/MQTT, mín. 8 caracteres)
#define WIFI_AP_PASS_DEFAULT "admin1234"

// Arranque: carga NVS, levanta AP según modo y conecta STA si está habilitada.
// deviceId = serial fijo del equipo (SSID del AP). No bloquea.
void wifiManagerBegin(const String& deviceId);

// Tick periódico desde loop() (gestiona ventana AP y reintentos STA)
void wifiManagerLoop();

// --- AP ---
void wifiApStartNow(uint32_t seconds);  // 0 = timeout configurado
void wifiApStopNow();
bool wifiApActive();
int  wifiApClients();
int  wifiApRemainingS();                // -1 = sin cuenta atrás (always_on)

// --- STA ---
bool wifiStaConnected();
bool wifiStaConnectTo(const String& ssid, const String& pass);  // guarda + conecta
void wifiStaForget();                                           // borra credenciales
void wifiStaSetEnabled(bool en);

// --- Configuración AP ---
bool wifiSetApMode(uint8_t mode, uint32_t timeout_s);  // valida y persiste
bool wifiSetApPass(const String& pass);                // >= 8 caracteres

// --- Escaneo asíncrono ---
void wifiScanStart();

// --- Estado / integración ---
String wifiStatusJson();   // para /api/wifi/status y MQTT get_wifi (sin passwords)
String wifiScanJson();     // {scanning, networks:[{ssid,rssi,enc}]}
void wifiRegisterWebRoutes();  // /wifi, /api/wifi/* (llamar antes de server.begin)

#else  // stubs sin WiFi mgmt

inline void wifiManagerBegin(const String&) {}
inline void wifiManagerLoop() {}
inline void wifiApStartNow(uint32_t) {}
inline void wifiApStopNow() {}
inline bool wifiApActive() { return false; }
inline int  wifiApClients() { return 0; }
inline int  wifiApRemainingS() { return 0; }
inline bool wifiStaConnected() { return false; }
inline String wifiStatusJson() { return String("{}"); }
inline void wifiRegisterWebRoutes() {}

#endif  // A2_FEATURE_WIFI_MGMT
