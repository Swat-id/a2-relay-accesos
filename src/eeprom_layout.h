#pragma once

#include <Arduino.h>

// ============================================================================
// MAPA DE PERSISTENCIA — EEPROM (4 KB) + NVS
//
// Única fuente de verdad de las estructuras persistentes y sus offsets.
// Los solapes se verifican en compilación (static_assert al final): un
// cambio de tamaño u offset incompatible NO compila.
//
//   Config          0    → ≤ 256
//   DigitalInput    256  → ≤ 512
//   StoredCodes     512  → ≤ 1800
//   (reservado)     1800 → 3199   antigua zona de códigos remotos; desde el
//                                 saneamiento v0.5 van en NVS ("a2acc_rc")
//   DeviceKey (BLE) 3200 → ≤ 3400
//   BLEAuth   (BLE) 3400 → ≤ 4090
//   Scratch test    4090 → 4092
//   Total           4096
// ============================================================================

#define EEPROM_TOTAL_SIZE 4096
// 2 bytes de scratch para el test de arranque: fuera de todas las estructuras
#define EEPROM_TEST_SCRATCH_OFFSET 4090

// --------------------------- Offsets EEPROM --------------------------------
#define EEPROM_DIGITAL_INPUT_OFFSET 256
#define EEPROM_CODES_OFFSET 512
// Zona reservada (no reutilizar sin revisar los offsets BLE)
#define EEPROM_REMOTE_CODES_OFFSET 1800

// --------------------------- Marcadores ------------------------------------
#define DIGITAL_INPUT_CONFIG_MARKER 0xD1D1D1D1
#define TURNSTILE_CONFIG_MARKER 0x544F524E  // "TORN" en ASCII

// --------------------------- Capacidades -----------------------------------
#define MAX_CODES 50          // Códigos locales (limitado por EEPROM 4 KB)
#define MAX_REMOTE_CODES 40   // Códigos remotos (blob NVS)

// Persistencia de códigos remotos en NVS (no caben en la EEPROM de 4 KB
// junto a las zonas BLE 3200/3400). Namespace según convención v5 (a2acc_*).
#define REMOTE_CODES_NVS_NS  "a2acc_rc"
#define REMOTE_CODES_NVS_KEY "codes"

// =================== CONFIGURACIÓN PRINCIPAL (offset 0) ====================

struct TurnstileConfig {
  bool enabled;            // true = modo torno, false = modo normal
  uint8_t keyboard1_relay; // Relé asignado al teclado 1 (1 o 2)
  uint8_t keyboard2_relay; // Relé asignado al teclado 2 (1 o 2)
  uint8_t reserved[5];     // Reservado para futuras extensiones
};

struct Config {
  char deviceName[32];
  char fixedSerial[32];        // SERIAL FIJO
  bool useDhcp;
  uint8_t ip[4];
  uint8_t gateway[4];
  uint8_t subnet[4];
  uint8_t dns[4];
  float releDuration;
  char webPassword[32];
  bool localAccessBlocked;     // Estado de bloqueo
  bool keyboardReadingEnabled; // Control de lectura de teclados
  unsigned long blockDuration; // Duración del bloqueo
  int maxFailedAttempts;       // Máximo intentos
  TurnstileConfig turnstile;   // Configuración del modo torno
  uint32_t configValid;
};

// =================== ENTRADAS DIGITALES (offset 256) =======================
// Tipos de entrada (v5.0.2)
#define DI_TYPE_BUTTON 0   // pulsador: dispara relé (comportamiento clásico)
#define DI_TYPE_DOOR   1   // imán/contacto de puerta: solo supervisión + MQTT
#define DI_CONFIG_V2_MARKER 0xD2
#define DI_BORNES_DIGITAL      0  // bornes DI1/DI2 = entradas digitales (default)
#define DI_BORNES_WIEGAND2     1  // bornes DI1/DI2 = Teclado Wiegand 2 (A2v3; DI fuera de servicio)
#define DI_BORNES_TECLADO2_I2C 2  // Teclado 2 en conector I2C SDA/SCL (A2v3; DI operativas)

// Empaquetada para evitar problemas de alineamiento.
// COMPATIBILIDAD: los primeros 24 bytes son el layout v1 intacto — el campo
// diN_type ocupa el antiguo diN_reserved (que siempre valía 0 = pulsador).
// El bloque v2 va DESPUÉS del layout v1 y se valida con v2_marker: equipos
// que suben desde v1 migran automáticamente sin perder configuración.
#pragma pack(push, 1)
struct DigitalInputConfig {
  uint32_t validMarker;     // DIGITAL_INPUT_CONFIG_MARKER
  uint8_t di1_enabled;
  uint8_t di1_relay;        // Relé asignado (solo tipo pulsador)
  uint8_t di1_inverse;      // Modo inverso (solo tipo pulsador)
  uint8_t di1_type;         // v5.0.2 (ex-reserved): DI_TYPE_BUTTON / DI_TYPE_DOOR
  uint32_t di1_duration_ms; // Duración pulso (solo tipo pulsador)
  uint8_t di2_enabled;
  uint8_t di2_relay;
  uint8_t di2_inverse;
  uint8_t di2_type;
  uint32_t di2_duration_ms;
  uint32_t checksum;
  // ---- bloque v2 (v5.0.2) ----
  uint8_t di1_open_level;   // imán: nivel eléctrico que significa ABIERTA (0=LOW, 1=HIGH)
  uint8_t di2_open_level;
  uint8_t v2_marker;        // DI_CONFIG_V2_MARKER cuando el bloque v2 es válido
  uint8_t bornes_mode;      // A2v3: dónde vive el Teclado 2 — 0=sin teclado 2
                            // (DI1/DI2 digitales), 1=en bornes DI1/DI2 (chips
                            // 16/17), 2=en conector I2C SDA/SCL (chips 48/47)
};
#pragma pack(pop)

// =================== CÓDIGOS LOCALES (offset 512) ==========================

struct CodeEntry {
  char type[5];
  char value[17];
  uint8_t keyboard_id; // 0=ambos, 1=teclado1, 2=teclado2
  uint8_t relay;
  uint8_t reserved;
};

struct StoredCodes {
  uint32_t validMarker;
  uint32_t version;               // 1 = formato antiguo, 2 = formato nuevo
  bool localValidationFirst;
  uint16_t count;
  CodeEntry codes[MAX_CODES];
};

// =================== CÓDIGOS REMOTOS (NVS "a2acc_rc") ======================

struct TimeSlot {
  uint8_t start_hour;    // 0-23
  uint8_t start_minute;  // 0-59
  uint8_t end_hour;      // 0-23
  uint8_t end_minute;    // 0-59
  uint8_t days_of_week;  // bitmask: 1=Lun … 32=Sáb, 64=Dom
  uint8_t reserved[3];
};

struct RemoteCodeEntry {
  char type[5];             // "PIN" o "TAG"
  char value[17];
  uint8_t keyboard_id;      // 0=ambos, 1=teclado1, 2=teclado2
  uint8_t relay;            // 1 o 2
  uint8_t time_slots_count; // máximo 4
  TimeSlot time_slots[4];
  uint8_t reserved;
};

struct StoredRemoteCodes {
  uint32_t validMarker;
  uint32_t version;
  uint16_t count;
  RemoteCodeEntry codes[MAX_REMOTE_CODES];
};

// =================== PERSISTENCIA BLE (solo env BLE) =======================
#ifdef ENABLE_BLE

#define BLE_AUTH_CONFIG_MARKER 0xB1E4C0DE
#define EEPROM_BLE_AUTH_OFFSET 3400
#define BLE_KEY_SIZE 64
#define BLE_MAX_USERS 5

#define DEVICE_KEY_CONFIG_MARKER 0xDE41CE41
#define EEPROM_DEVICE_KEY_OFFSET 3200
#define DEVICE_MASTER_KEY_SIZE 32  // 256 bits para HKDF

#pragma pack(push, 1)
struct DeviceKeyConfig {
  uint32_t validMarker;
  uint8_t master_key[DEVICE_MASTER_KEY_SIZE];
  uint8_t key_version;       // Para rotación
  uint32_t generation_time;
  uint32_t checksum;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BLEAuthConfig {
  uint32_t validMarker;
  uint8_t superadmin_key[BLE_KEY_SIZE];
  uint8_t user_keys[BLE_MAX_USERS][BLE_KEY_SIZE];
  uint8_t user_enabled[BLE_MAX_USERS];
  uint8_t user_permissions[BLE_MAX_USERS];
  char user_names[BLE_MAX_USERS][16];
  uint8_t superadmin_registered;
  uint32_t checksum;
};
#pragma pack(pop)

#endif // ENABLE_BLE

// =================== VERIFICACIÓN DEL MAPA (compilación) ===================

static_assert(sizeof(Config) <= EEPROM_DIGITAL_INPUT_OFFSET,
              "Config se solapa con DigitalInputConfig");
static_assert(EEPROM_DIGITAL_INPUT_OFFSET + sizeof(DigitalInputConfig) <= EEPROM_CODES_OFFSET,
              "DigitalInputConfig se solapa con StoredCodes");
static_assert(EEPROM_CODES_OFFSET + sizeof(StoredCodes) <= EEPROM_REMOTE_CODES_OFFSET,
              "StoredCodes invade la zona reservada 1800+");
static_assert(EEPROM_TEST_SCRATCH_OFFSET + 2 <= EEPROM_TOTAL_SIZE,
              "Scratch de test fuera de la EEPROM");

#ifdef ENABLE_BLE
static_assert(EEPROM_DEVICE_KEY_OFFSET >= EEPROM_REMOTE_CODES_OFFSET,
              "DeviceKeyConfig invade StoredCodes");
static_assert(EEPROM_DEVICE_KEY_OFFSET + sizeof(DeviceKeyConfig) <= EEPROM_BLE_AUTH_OFFSET,
              "DeviceKeyConfig se solapa con BLEAuthConfig");
static_assert(EEPROM_BLE_AUTH_OFFSET + sizeof(BLEAuthConfig) <= EEPROM_TEST_SCRATCH_OFFSET,
              "BLEAuthConfig invade el scratch de test / fin de EEPROM");
#endif
