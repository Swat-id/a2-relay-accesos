#pragma once

#include <Arduino.h>
#include "target_features.h"

// ============================================================================
// gsm_modem — gestión del módem celular SIM7600E/SIM800 (Fase 1 GSM: estado)
//
// FSM AT no bloqueante (guía portable §5): detección del módulo, SIM/PIN,
// registro de red, cobertura (CSQ) y operador. SIN datos PPP (Fase 4 / v5.1).
//
// Hardware:
//   - A2v3 (ESP32-S3): TX=GPIO10, RX=GPIO9, con auto-detección de swap.
//   - A2 clásico (socket 4G): TX=GPIO13, RX=GPIO34. GPIO34 es solo entrada,
//     por lo que el swap NO es posible en este target (se ignora).
//   - UART1 con pines remapeados (la UART2 la ocupa el teclado RS485).
//
// Política PIN (obligatoria): AT+CPIN=<pin> UNA vez por arranque y por valor;
// error → PinError sin reintentos (riesgo PUK). El PIN nunca sale en JSON.
//
// Config NVS "a2acc_gsm": en, pin_swap, pin, apn_mode, apn, apn_user,
// apn_pass, data_mode (reservado para Fase 4).
// ============================================================================

enum GsmState : uint8_t {
  GSM_DISABLED = 0,   // en=0: sin probe al arrancar
  GSM_PROBING,        // detectando módulo (AT, alternando swap si procede)
  GSM_ABSENT,         // sin respuesta: reposo hasta rescan manual
  GSM_SIM_CHECK,      // CPIN?
  GSM_PIN_REQUIRED,   // SIM pide PIN y no hay PIN almacenado
  GSM_PIN_ERROR,      // PIN rechazado: SIN reintentos automáticos
  GSM_PUK_REQUIRED,   // aviso al operador; no se gestiona desde firmware
  GSM_NO_SIM,
  GSM_REGISTERING,    // esperando registro CEREG/CREG
  GSM_ATTACHED        // registrado; CSQ/operador en polling
};

struct GsmStatus {
  GsmState state = GSM_DISABLED;
  char model[24] = "";
  char imei[16] = "";
  char oper[32] = "";
  int8_t csq = -1;        // 0-31; -1 = desconocido
  bool pin_set = false;   // hay PIN almacenado (nunca se expone el valor)
  bool pin_swap = false;  // mapeo TX/RX invertido detectado
};

const char* gsmStateName(GsmState st);

#if A2_FEATURE_GSM_MODEM

void gsmModemBegin();
void gsmModemLoop();                    // tick no bloqueante desde loop()
const GsmStatus& gsmModemStatus();
void gsmModemRescan();                  // re-probe manual (hot-plug best effort)

// Configuración (persistida en NVS). El PIN es de solo escritura.
bool gsmSetEnabled(bool en);
bool gsmSetPin(const String& pin);      // "" = borrar
bool gsmSetApn(uint8_t apn_mode, const String& apn,
               const String& user, const String& pass);

String gsmStatusJson();                 // /api/gsm/status y MQTT get_gsm
void gsmRegisterWebRoutes();            // /api/gsm/status, /api/gsm/rescan

#else  // stubs

inline void gsmModemBegin() {}
inline void gsmModemLoop() {}
inline const GsmStatus& gsmModemStatus() {
  static const GsmStatus s;
  return s;
}
inline void gsmModemRescan() {}
inline bool gsmSetEnabled(bool) { return false; }
inline bool gsmSetPin(const String&) { return false; }
inline bool gsmSetApn(uint8_t, const String&, const String&, const String&) { return false; }
inline String gsmStatusJson() { return String("{\"state\":\"disabled\"}"); }
inline void gsmRegisterWebRoutes() {}

#endif  // A2_FEATURE_GSM_MODEM
