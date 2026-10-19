#include "binding_service.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include "mbedtls/md.h"
#include "esp_random.h"
#include "hw_config.h"
#include "target_features.h"
#include "eeprom_layout.h"

// Servicios de main.ino
extern String fixedSerialNumber;
extern const char* firmwareVersion;

#ifdef ENABLE_BLE
extern BLEAuthConfig bleAuthConfig;
void saveBLEAuthConfig();
#endif

// --------------------------- Persistencia ----------------------------------

#define BINDING_NVS_NS "a2acc_bind"
#define BINDING_NVS_KEY "store"
#define BINDING_STORE_MARKER 0xB1ADC0DE

#pragma pack(push, 1)
struct BindingStore {
  uint32_t validMarker;
  uint8_t platform_key[BINDING_KEY_SIZE];
  uint8_t platform_set;
  uint8_t admin_key[BINDING_KEY_SIZE];
  uint8_t admin_set;
  char admin_name[16];
  uint8_t user_keys[BINDING_MAX_USERS][BINDING_KEY_SIZE];
  uint8_t user_enabled[BINDING_MAX_USERS];
  uint8_t user_permissions[BINDING_MAX_USERS];
  char user_names[BINDING_MAX_USERS][16];
  uint32_t checksum;
};
#pragma pack(pop)

static BindingStore s_store;
static bool s_loaded = false;
static Preferences s_prefs;

// Ventana remota y nonce
static unsigned long s_windowUntil = 0;       // millis(); 0 = cerrada
static char s_nonce[129] = "";                // 64 bytes → 128 hex + NUL
static unsigned long s_nonceUntil = 0;        // millis(); 0 = sin nonce

static void (*s_eventFn)(const String&) = nullptr;

// Firmantes: -1 = nadie, 0 = admin de placa, 1..5 usuarios, 100 = plataforma
#define SIGNER_NONE (-1)
#define SIGNER_ADMIN 0
#define SIGNER_PLATFORM 100

static uint32_t storeChecksum(const BindingStore& st) {
  const uint8_t* p = (const uint8_t*)&st;
  size_t n = sizeof(BindingStore) - sizeof(uint32_t);
  uint32_t sum = 0xB1D0;
  for (size_t i = 0; i < n; i++) sum = (sum * 31u) ^ p[i];
  return sum;
}

static bool saveStore() {
  s_store.validMarker = BINDING_STORE_MARKER;
  s_store.checksum = storeChecksum(s_store);
  if (!s_prefs.begin(BINDING_NVS_NS, false)) return false;
  size_t written = s_prefs.putBytes(BINDING_NVS_KEY, &s_store, sizeof(s_store));
  s_prefs.end();
  if (written != sizeof(s_store)) {
    Serial.println("❌ [VINCULO] Error guardando en NVS");
    return false;
  }
  return true;
}

#ifdef ENABLE_BLE
// Guard anti-eco: cuando el cambio nace en el propio binding (syncToBle →
// saveBLEAuthConfig → bindingSyncFromBle) no hay nada nuevo que reflejar.
static bool s_syncingToBle = false;

// Sincroniza las claves hacia la config BLE para que la app siga
// autenticando con el mismo material (admin = superadmin BLE histórico).
static void syncToBle() {
  if (s_store.admin_set) {
    memcpy(bleAuthConfig.superadmin_key, s_store.admin_key, BINDING_KEY_SIZE);
    bleAuthConfig.superadmin_registered = 1;
  } else {
    memset(bleAuthConfig.superadmin_key, 0, BINDING_KEY_SIZE);
    bleAuthConfig.superadmin_registered = 0;
  }
  for (int i = 0; i < BINDING_MAX_USERS; i++) {
    memcpy(bleAuthConfig.user_keys[i], s_store.user_keys[i], BINDING_KEY_SIZE);
    bleAuthConfig.user_enabled[i] = s_store.user_enabled[i];
    bleAuthConfig.user_permissions[i] = s_store.user_permissions[i];
    strncpy(bleAuthConfig.user_names[i], s_store.user_names[i], 15);
    bleAuthConfig.user_names[i][15] = '\0';
  }
  s_syncingToBle = true;
  saveBLEAuthConfig();
  s_syncingToBle = false;
}
#endif

static void publishEvent(const char* event, int slot, const char* role,
                         const char* name) {
  if (!s_eventFn) return;
  DynamicJsonDocument doc(384);
  doc["event"] = event;
  doc["device"] = fixedSerialNumber;
  doc["slot"] = slot;
  doc["role"] = role;
  if (name && name[0]) doc["name"] = name;
  doc["source"] = "MQTT";
  String out;
  serializeJson(doc, out);
  s_eventFn(out);
}

// --------------------------- Utilidades hex/HMAC ---------------------------

static bool hexToBytes(const String& hex, uint8_t* out, size_t outLen) {
  if (hex.length() != outLen * 2) return false;
  for (size_t i = 0; i < outLen; i++) {
    char hi = hex[2 * i], lo = hex[2 * i + 1];
    auto nib = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    int h = nib(hi), l = nib(lo);
    if (h < 0 || l < 0) return false;
    out[i] = (uint8_t)((h << 4) | l);
  }
  return true;
}

static void bytesToHex(const uint8_t* in, size_t len, char* out) {
  static const char* d = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[2 * i] = d[in[i] >> 4];
    out[2 * i + 1] = d[in[i] & 0x0F];
  }
  out[2 * len] = '\0';
}

// HMAC-SHA256(key, unhex(nonce) || utf8(payload)) == hmacHex ?
static bool hmacMatches(const uint8_t* key, const uint8_t* nonceBytes,
                        size_t nonceLen, const String& payload,
                        const uint8_t* expected) {
  uint8_t mac[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  if (mbedtls_md_setup(&ctx, info, 1) != 0) { mbedtls_md_free(&ctx); return false; }
  mbedtls_md_hmac_starts(&ctx, key, BINDING_KEY_SIZE);
  mbedtls_md_hmac_update(&ctx, nonceBytes, nonceLen);
  mbedtls_md_hmac_update(&ctx, (const uint8_t*)payload.c_str(), payload.length());
  mbedtls_md_hmac_finish(&ctx, mac);
  mbedtls_md_free(&ctx);
  // Comparación en tiempo constante
  uint8_t diff = 0;
  for (int i = 0; i < 32; i++) diff |= (uint8_t)(mac[i] ^ expected[i]);
  return diff == 0;
}

// --------------------------- API pública -----------------------------------

void bindingSetEventPublisher(void (*fn)(const String&)) { s_eventFn = fn; }

void bindingOpenWindow(uint32_t seconds) {
  s_windowUntil = millis() + seconds * 1000UL;
  Serial.printf("🔗 [VINCULO] Ventana de vinculación ABIERTA %lu s\n",
                (unsigned long)seconds);
}

void bindingCloseWindow() {
  s_windowUntil = 0;
  Serial.println("🔗 [VINCULO] Ventana de vinculación cerrada");
}

bool bindingWindowOpen() {
  return s_windowUntil != 0 && (long)(s_windowUntil - millis()) > 0;
}

int bindingWindowRemainingS() {
  if (!bindingWindowOpen()) return 0;
  return (int)((s_windowUntil - millis()) / 1000UL);
}

bool bindingAdminSet() { return s_store.admin_set != 0; }
bool bindingPlatformSet() { return s_store.platform_set != 0; }

const uint8_t* bindingPlatformKeyPtr() {
  return s_store.platform_set ? s_store.platform_key : nullptr;
}

#ifdef ENABLE_BLE
// BLE → binding: refleja en el almacén de vinculación cualquier mutación
// hecha por el canal BLE (TOFU del primer FF01, gestión tipo 6). La clave
// de plataforma NUNCA se toca desde aquí.
void bindingSyncFromBle() {
  if (s_syncingToBle) return;   // el cambio nació en el propio binding
  if (!s_loaded) return;        // arranque: aún sin cargar (la migración
                                // de bindingServiceBegin cubre ese caso)
  bool changed = false;

  if (bleAuthConfig.superadmin_registered) {
    if (!s_store.admin_set ||
        memcmp(s_store.admin_key, bleAuthConfig.superadmin_key, BINDING_KEY_SIZE) != 0) {
      memcpy(s_store.admin_key, bleAuthConfig.superadmin_key, BINDING_KEY_SIZE);
      s_store.admin_set = 1;
      if (!s_store.admin_name[0])
        strncpy(s_store.admin_name, "admin", sizeof(s_store.admin_name) - 1);
      changed = true;
    }
  } else if (s_store.admin_set) {
    memset(s_store.admin_key, 0, BINDING_KEY_SIZE);
    s_store.admin_set = 0;
    s_store.admin_name[0] = '\0';
    changed = true;
  }

  for (int i = 0; i < BINDING_MAX_USERS; i++) {
    bool en = bleAuthConfig.user_enabled[i] != 0;
    if (en != (s_store.user_enabled[i] != 0) ||
        (en && (memcmp(s_store.user_keys[i], bleAuthConfig.user_keys[i], BINDING_KEY_SIZE) != 0 ||
                s_store.user_permissions[i] != bleAuthConfig.user_permissions[i] ||
                strncmp(s_store.user_names[i], bleAuthConfig.user_names[i], 15) != 0))) {
      memcpy(s_store.user_keys[i], bleAuthConfig.user_keys[i], BINDING_KEY_SIZE);
      s_store.user_enabled[i] = bleAuthConfig.user_enabled[i];
      s_store.user_permissions[i] = bleAuthConfig.user_permissions[i];
      strncpy(s_store.user_names[i], bleAuthConfig.user_names[i], 15);
      s_store.user_names[i][15] = '\0';
      if (!en) {
        memset(s_store.user_keys[i], 0, BINDING_KEY_SIZE);
        s_store.user_permissions[i] = 0;
        s_store.user_names[i][0] = '\0';
      }
      changed = true;
    }
  }

  if (changed) {
    saveStore();
    Serial.println("🔗 [VINCULO] Sincronizado desde BLE (TOFU/gestión tipo 6)");
  }
}
#else
void bindingSyncFromBle() {}
#endif

int bindingUsersCount() {
  int n = 0;
  for (int i = 0; i < BINDING_MAX_USERS; i++)
    if (s_store.user_enabled[i]) n++;
  return n;
}

bool bindingSetPlatformKeyHex(const String& hex128) {
  uint8_t key[BINDING_KEY_SIZE];
  if (!hexToBytes(hex128, key, BINDING_KEY_SIZE)) return false;
  memcpy(s_store.platform_key, key, BINDING_KEY_SIZE);
  s_store.platform_set = 1;
  if (!saveStore()) return false;
  Serial.println("🔗 [VINCULO] Clave de superadmin de PLATAFORMA provisionada");
  publishEvent("BINDING_ADDED", -1, "platform_superadmin", "");
  return true;
}

bool bindingClearPlatformKey() {
  memset(s_store.platform_key, 0, BINDING_KEY_SIZE);
  s_store.platform_set = 0;
  if (!saveStore()) return false;
  Serial.println("🔗 [VINCULO] Clave de plataforma eliminada (fábrica)");
  publishEvent("BINDING_CLEARED", -1, "platform_superadmin", "");
  return true;
}

void bindingServiceBegin() {
  memset(&s_store, 0, sizeof(s_store));
  bool ok = false;
  if (s_prefs.begin(BINDING_NVS_NS, true)) {
    if (s_prefs.getBytesLength(BINDING_NVS_KEY) == sizeof(BindingStore)) {
      s_prefs.getBytes(BINDING_NVS_KEY, &s_store, sizeof(BindingStore));
      ok = (s_store.validMarker == BINDING_STORE_MARKER &&
            s_store.checksum == storeChecksum(s_store));
    }
    s_prefs.end();
  }
  if (!ok) {
    memset(&s_store, 0, sizeof(s_store));
#ifdef ENABLE_BLE
    // Migración única desde BLEAuthConfig: la clave "superadmin" BLE
    // histórica era en la práctica el dueño del equipo → ADMIN de placa.
    // La plataforma queda vacía hasta el provisioning de fábrica.
    if (bleAuthConfig.validMarker == BLE_AUTH_CONFIG_MARKER &&
        bleAuthConfig.superadmin_registered) {
      memcpy(s_store.admin_key, bleAuthConfig.superadmin_key, BINDING_KEY_SIZE);
      s_store.admin_set = 1;
      strncpy(s_store.admin_name, "admin", sizeof(s_store.admin_name) - 1);
      for (int i = 0; i < BINDING_MAX_USERS; i++) {
        memcpy(s_store.user_keys[i], bleAuthConfig.user_keys[i], BINDING_KEY_SIZE);
        s_store.user_enabled[i] = bleAuthConfig.user_enabled[i];
        s_store.user_permissions[i] = bleAuthConfig.user_permissions[i];
        strncpy(s_store.user_names[i], bleAuthConfig.user_names[i], 15);
        s_store.user_names[i][15] = '\0';
      }
      Serial.println("🔗 [VINCULO] Migración BLE→binding: superadmin BLE = admin de placa");
    }
#endif
    saveStore();
  }
  s_loaded = true;

  // Ventana TOFU de arranque (estilo X1): 10 min
  bindingOpenWindow(BINDING_WINDOW_BOOT_S);

  Serial.printf("🔗 [VINCULO] Servicio activo: plataforma=%s admin=%s usuarios=%d/5\n",
                s_store.platform_set ? "sí" : "no",
                s_store.admin_set ? "sí" : "no", bindingUsersCount());
}

String bindingStatusJson() {
  DynamicJsonDocument doc(512);
  doc["platform_superadmin_registered"] = (bool)s_store.platform_set;
  doc["admin_registered"] = (bool)s_store.admin_set;
  doc["users"] = bindingUsersCount();
  doc["max_users"] = BINDING_MAX_USERS;
  doc["window_open"] = bindingWindowOpen();
  doc["window_remaining_s"] = bindingWindowRemainingS();
  String out;
  serializeJson(doc, out);
  return out;
}

// --------------------------- Respuestas ------------------------------------

static String okResponse(std::initializer_list<std::pair<const char*, String>> extra) {
  DynamicJsonDocument doc(768);
  doc["ok"] = true;
  doc["serial"] = fixedSerialNumber;
  for (auto& kv : extra) {
    // Valores numéricos/booleanos sencillos viajan como string aquí; los
    // campos estructurados usan los builders dedicados (devinfo).
    doc[kv.first] = kv.second;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

static String errResponse(const char* code) {
  DynamicJsonDocument doc(256);
  doc["ok"] = false;
  doc["serial"] = fixedSerialNumber;
  doc["error"] = code;
  String out;
  serializeJson(doc, out);
  return out;
}

static String devinfoResponse() {
  DynamicJsonDocument doc(768);
  doc["ok"] = true;
  doc["serial"] = fixedSerialNumber;
  doc["firmware_version"] = firmwareVersion;
  JsonObject caps = doc.createNestedObject("capabilities");
  caps["board"] = HW_BOARD_NAME;
  caps["relays"] = 2;
  caps["keyboards"] = 2;
  caps["digital_inputs"] = 2;
  caps["slots"] = 1 + BINDING_MAX_USERS;   // admin + 5 (plataforma aparte)
#ifdef ENABLE_BLE
  caps["ble"] = true;
#else
  caps["ble"] = false;
#endif
#if A2_FEATURE_GSM_MODEM
  caps["gsm"] = true;
#else
  caps["gsm"] = false;
#endif
  caps["mqtt"] = true;
  // Compat X1: "superadmin_registered" = dueño de placa (admin slot 0)
  doc["superadmin_registered"] = (bool)s_store.admin_set;
  doc["admin_registered"] = (bool)s_store.admin_set;
  doc["platform_superadmin_registered"] = (bool)s_store.platform_set;
  doc["available_for_linking"] = !s_store.admin_set;
  int used = (s_store.admin_set ? 1 : 0) + bindingUsersCount();
  doc["slots_used"] = used;
  doc["slots_available"] = (1 + BINDING_MAX_USERS) - used;
  doc["remote_link_window_open"] = bindingWindowOpen();
  doc["remote_link_window_s"] = bindingWindowRemainingS();
  String out;
  serializeJson(doc, out);
  return out;
}

static String challengeResponse() {
  uint8_t nonce[32];
  esp_fill_random(nonce, sizeof(nonce));
  bytesToHex(nonce, sizeof(nonce), s_nonce);
  s_nonceUntil = millis() + BINDING_NONCE_TTL_S * 1000UL;
  DynamicJsonDocument doc(320);
  doc["ok"] = true;
  doc["serial"] = fixedSerialNumber;
  doc["nonce"] = s_nonce;
  doc["expires_s"] = BINDING_NONCE_TTL_S;
  String out;
  serializeJson(doc, out);
  return out;
}

// TOFU: alta del admin de placa
static String tofuLink(const String& keyHex) {
  if (s_store.admin_set) return errResponse("device_already_owned");
  if (!bindingWindowOpen()) return errResponse("remote_linking_window_closed");
  uint8_t key[BINDING_KEY_SIZE];
  if (!hexToBytes(keyHex, key, BINDING_KEY_SIZE)) return errResponse("invalid_key");

  memcpy(s_store.admin_key, key, BINDING_KEY_SIZE);
  s_store.admin_set = 1;
  strncpy(s_store.admin_name, "admin", sizeof(s_store.admin_name) - 1);
  if (!saveStore()) return errResponse("nvs_write_failed");
#ifdef ENABLE_BLE
  syncToBle();
#endif
  bindingCloseWindow();
  publishEvent("BINDING_ADDED", 0, "admin", s_store.admin_name);
  Serial.println("🔗 [VINCULO] Admin de placa vinculado (TOFU)");

  DynamicJsonDocument doc(320);
  doc["ok"] = true;
  doc["serial"] = fixedSerialNumber;
  doc["linked"] = true;
  doc["slot"] = 0;
  doc["role"] = "admin";
  doc["role_alias"] = "superadmin";   // compat X1
  String out;
  serializeJson(doc, out);
  return out;
}

// Identifica al firmante del sobre; SIGNER_NONE si ninguna clave verifica
static int findSigner(const String& nonceHex, const String& hmacHex,
                      const String& payload) {
  uint8_t nonceBytes[32], mac[32];
  if (!hexToBytes(nonceHex, nonceBytes, sizeof(nonceBytes))) return SIGNER_NONE;
  if (!hexToBytes(hmacHex, mac, sizeof(mac))) return SIGNER_NONE;

  if (s_store.platform_set &&
      hmacMatches(s_store.platform_key, nonceBytes, sizeof(nonceBytes), payload, mac))
    return SIGNER_PLATFORM;
  if (s_store.admin_set &&
      hmacMatches(s_store.admin_key, nonceBytes, sizeof(nonceBytes), payload, mac))
    return SIGNER_ADMIN;
  for (int i = 0; i < BINDING_MAX_USERS; i++) {
    if (s_store.user_enabled[i] &&
        hmacMatches(s_store.user_keys[i], nonceBytes, sizeof(nonceBytes), payload, mac))
      return 1 + i;
  }
  return SIGNER_NONE;
}

// Comandos firmados (payload ya parseado + firmante identificado)
static String handleSignedCommand(int signer, JsonDocument& cmd) {
  String command = cmd["command"].as<String>();
  bool isAdminLevel = (signer == SIGNER_ADMIN || signer == SIGNER_PLATFORM);

  // ---- Alta de usuario: link POST con slot/new_key -------------------------
  if (command == "link") {
    if (!isAdminLevel) return errResponse("admin_required");
    int slot = cmd.containsKey("slot") ? cmd["slot"].as<int>() : -1;
    String newKey = cmd["new_key"].as<String>();
    String name = cmd.containsKey("name") ? cmd["name"].as<String>() : "";
    int permissions = cmd.containsKey("permissions") ? cmd["permissions"].as<int>() : 1;
    if (slot < 1 || slot > BINDING_MAX_USERS)
      return errResponse("invalid_new_key_or_slot");
    uint8_t key[BINDING_KEY_SIZE];
    if (!hexToBytes(newKey, key, BINDING_KEY_SIZE))
      return errResponse("invalid_new_key_or_slot");
    if (permissions < 0 || permissions > 255) permissions = 1;

    bool existed = s_store.user_enabled[slot - 1];
    memcpy(s_store.user_keys[slot - 1], key, BINDING_KEY_SIZE);
    s_store.user_enabled[slot - 1] = 1;
    s_store.user_permissions[slot - 1] = (uint8_t)permissions;
    strncpy(s_store.user_names[slot - 1], name.c_str(), 15);
    s_store.user_names[slot - 1][15] = '\0';
    if (!saveStore()) return errResponse("nvs_write_failed");
#ifdef ENABLE_BLE
    syncToBle();
#endif
    publishEvent(existed ? "BINDING_UPDATED" : "BINDING_ADDED", slot, "user",
                 s_store.user_names[slot - 1]);

    DynamicJsonDocument doc(320);
    doc["ok"] = true;
    doc["serial"] = fixedSerialNumber;
    doc["linked"] = true;
    doc["slot"] = slot;
    doc["role"] = "user";
    doc["name"] = s_store.user_names[slot - 1];
    doc["permissions"] = permissions;
    String out;
    serializeJson(doc, out);
    return out;
  }

  // ---- Rotación de clave propia -------------------------------------------
  if (command == "update_vinculo") {
    String newKey = cmd["new_key"].as<String>();
    uint8_t key[BINDING_KEY_SIZE];
    if (!hexToBytes(newKey, key, BINDING_KEY_SIZE))
      return errResponse("invalid_new_key_or_slot");
    const char* role;
    int slotOut;
    if (signer == SIGNER_PLATFORM) {
      memcpy(s_store.platform_key, key, BINDING_KEY_SIZE);
      role = "platform_superadmin"; slotOut = -1;
    } else if (signer == SIGNER_ADMIN) {
      memcpy(s_store.admin_key, key, BINDING_KEY_SIZE);
      role = "admin"; slotOut = 0;
    } else {
      memcpy(s_store.user_keys[signer - 1], key, BINDING_KEY_SIZE);
      role = "user"; slotOut = signer;
    }
    if (!saveStore()) return errResponse("nvs_write_failed");
#ifdef ENABLE_BLE
    syncToBle();
#endif
    publishEvent("BINDING_UPDATED", slotOut, role, "");
    DynamicJsonDocument doc(256);
    doc["ok"] = true;
    doc["serial"] = fixedSerialNumber;
    doc["updated"] = true;
    doc["slot"] = slotOut;
    doc["role"] = role;
    String out;
    serializeJson(doc, out);
    return out;
  }

  // ---- Bajas ---------------------------------------------------------------
  if (command == "unlink") {
    bool hasSlot = cmd.containsKey("slot");
    int slot = hasSlot ? cmd["slot"].as<int>() : -99;

    if (!hasSlot) {
      // Baja propia
      if (signer == SIGNER_PLATFORM)
        // La plataforma solo se borra por procedimiento de fábrica (web)
        return errResponse("factory_procedure_required");
      if (signer == SIGNER_ADMIN) {
        // El admin se va: se limpian también los usuarios. Plataforma intacta.
        memset(s_store.admin_key, 0, BINDING_KEY_SIZE);
        s_store.admin_set = 0;
        s_store.admin_name[0] = '\0';
        for (int i = 0; i < BINDING_MAX_USERS; i++) {
          memset(s_store.user_keys[i], 0, BINDING_KEY_SIZE);
          s_store.user_enabled[i] = 0;
          s_store.user_permissions[i] = 0;
          s_store.user_names[i][0] = '\0';
        }
        if (!saveStore()) return errResponse("nvs_write_failed");
#ifdef ENABLE_BLE
        syncToBle();
#endif
        publishEvent("BINDING_CLEARED", 0, "admin", "");
        Serial.println("🔗 [VINCULO] Admin desvinculado - usuarios limpiados (plataforma intacta)");
        return okResponse({{"unlinked", "admin_and_users"}});
      }
      // Usuario normal se desvincula a sí mismo
      int idx = signer - 1;
      memset(s_store.user_keys[idx], 0, BINDING_KEY_SIZE);
      s_store.user_enabled[idx] = 0;
      s_store.user_permissions[idx] = 0;
      s_store.user_names[idx][0] = '\0';
      if (!saveStore()) return errResponse("nvs_write_failed");
#ifdef ENABLE_BLE
      syncToBle();
#endif
      publishEvent("BINDING_CLEARED", signer, "user", "");
      return okResponse({{"unlinked", String(signer)}});
    }

    // Baja de tercero (requiere admin o plataforma)
    if (!isAdminLevel) return errResponse("admin_required");
    if (slot == 0) {
      // Quitar al admin: solo la plataforma puede; limpia también usuarios
      if (signer != SIGNER_PLATFORM) return errResponse("platform_required");
      memset(s_store.admin_key, 0, BINDING_KEY_SIZE);
      s_store.admin_set = 0;
      s_store.admin_name[0] = '\0';
      for (int i = 0; i < BINDING_MAX_USERS; i++) {
        memset(s_store.user_keys[i], 0, BINDING_KEY_SIZE);
        s_store.user_enabled[i] = 0;
        s_store.user_permissions[i] = 0;
        s_store.user_names[i][0] = '\0';
      }
      if (!saveStore()) return errResponse("nvs_write_failed");
#ifdef ENABLE_BLE
      syncToBle();
#endif
      publishEvent("BINDING_CLEARED", 0, "admin", "");
      return okResponse({{"unlinked", "admin_and_users"}});
    }
    if (slot < 1 || slot > BINDING_MAX_USERS)
      return errResponse("invalid_new_key_or_slot");
    if (!s_store.user_enabled[slot - 1]) return errResponse("not_linked");
    memset(s_store.user_keys[slot - 1], 0, BINDING_KEY_SIZE);
    s_store.user_enabled[slot - 1] = 0;
    s_store.user_permissions[slot - 1] = 0;
    s_store.user_names[slot - 1][0] = '\0';
    if (!saveStore()) return errResponse("nvs_write_failed");
#ifdef ENABLE_BLE
    syncToBle();
#endif
    publishEvent("BINDING_CLEARED", slot, "user", "");
    return okResponse({{"unlinked", String(slot)}});
  }

  // ---- Listado firmado (admin/plataforma): usuarios sin claves ------------
  if (command == "list") {
    if (!isAdminLevel) return errResponse("admin_required");
    DynamicJsonDocument doc(1024);
    doc["ok"] = true;
    doc["serial"] = fixedSerialNumber;
    doc["platform_superadmin_registered"] = (bool)s_store.platform_set;
    JsonArray arr = doc.createNestedArray("bindings");
    if (s_store.admin_set) {
      JsonObject a = arr.createNestedObject();
      a["slot"] = 0;
      a["role"] = "admin";
      a["name"] = s_store.admin_name;
      a["permissions"] = 255;
    }
    for (int i = 0; i < BINDING_MAX_USERS; i++) {
      if (!s_store.user_enabled[i]) continue;
      JsonObject u = arr.createNestedObject();
      u["slot"] = 1 + i;
      u["role"] = "user";
      u["name"] = s_store.user_names[i];
      u["permissions"] = s_store.user_permissions[i];
    }
    String out;
    serializeJson(doc, out);
    return out;
  }

  return errResponse("unknown_command");
}

String bindingHandleMessage(const String& message) {
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, message) != DeserializationError::Ok)
    return errResponse("invalid_json");

  // ---- Sobre autenticado {nonce, hmac, payload} ---------------------------
  if (doc.containsKey("hmac") && doc.containsKey("payload")) {
    String nonce = doc["nonce"].as<String>();
    String hmacHex = doc["hmac"].as<String>();
    String payload = doc["payload"].as<String>();

    if (s_nonceUntil == 0 || (long)(s_nonceUntil - millis()) <= 0 ||
        nonce != String(s_nonce))
      return errResponse("nonce_expired");
    // El nonce se consume SIEMPRE que se intenta usar (anti-replay)
    s_nonceUntil = 0;
    s_nonce[0] = '\0';

    int signer = findSigner(nonce, hmacHex, payload);
    if (signer == SIGNER_NONE) return errResponse("invalid_signature");

    DynamicJsonDocument cmd(1024);
    if (deserializeJson(cmd, payload) != DeserializationError::Ok)
      return errResponse("invalid_json");
    return handleSignedCommand(signer, cmd);
  }

  // ---- Comandos públicos ---------------------------------------------------
  String command = doc["command"].as<String>();
  String type = doc.containsKey("type") ? doc["type"].as<String>() : "";

  if (command == "devinfo") return devinfoResponse();
  if (command == "challenge") return challengeResponse();
  if (command == "link" && type == "POST") {
    if (!doc.containsKey("key")) return errResponse("missing_key");
    return tofuLink(doc["key"].as<String>());
  }
  if (command == "link") return errResponse("auth_required");

  return errResponse("unknown_command");
}
