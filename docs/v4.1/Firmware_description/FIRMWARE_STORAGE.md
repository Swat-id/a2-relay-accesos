# Almacenamiento en Memoria No Volátil - SWATID-A2 v4.1

> **Versión Firmware:** v4.1.0  
> **Última actualización:** Marzo 2026  
> **Tecnología:** EEPROM emulada en Flash (ESP32)  
> **Tamaño total:** 8192 bytes (8KB)

---

## Índice

1. [Arquitectura de Almacenamiento](#arquitectura-de-almacenamiento)
2. [Mapa de Memoria EEPROM](#mapa-de-memoria-eeprom)
3. [Estructura de Configuración Principal](#estructura-de-configuración-principal)
4. [Almacenamiento de Códigos Locales](#almacenamiento-de-códigos-locales)
5. [Autenticación BLE](#autenticación-ble)
6. [Funciones de Persistencia](#funciones-de-persistencia)
7. [Validación e Integridad](#validación-e-integridad)
8. [Problemas Resueltos](#problemas-resueltos)
9. [Buenas Prácticas](#buenas-prácticas)
10. [Portal Web de Administración](#portal-web-de-administración)

---

## Arquitectura de Almacenamiento

### Diagrama de Memoria

```
EEPROM (8KB)
┌────────────────────────────────────────┐ 0x0000
│          Configuración Principal        │
│              (DeviceConfig)             │
│              ~200 bytes                 │
├────────────────────────────────────────┤ ~0x00C8
│                                        │
│              Reservado                  │
│                                        │
├────────────────────────────────────────┤ 0x0200 (512)
│          Códigos Locales               │
│           (StoredCodes)                │
│     50 códigos × ~25 bytes             │
│           ~1300 bytes                  │
├────────────────────────────────────────┤ 0x0700
│                                        │
│              Reservado                  │
│                                        │
├────────────────────────────────────────┤ 0x1000 (4096)
│       Configuración BLE Auth           │
│         (BLEAuthConfig)                │
│         5 usuarios × ~72 bytes         │
│           ~400 bytes                   │
├────────────────────────────────────────┤ 0x1200
│                                        │
│         Códigos Remotos                │
│        (RemoteStoredCodes)             │
│                                        │
├────────────────────────────────────────┤ 0x1800
│                                        │
│              Reservado                  │
│                                        │
└────────────────────────────────────────┘ 0x2000 (8192)
```

### Inicialización EEPROM

```cpp
#define EEPROM_SIZE 8192

void setup() {
  EEPROM.begin(EEPROM_SIZE);
  loadConfiguration();       // Offset 0
  loadStoredCodes();         // Offset 512
  loadBLEAuthConfig();       // Offset 4096
  loadRemoteStoredCodes();   // Offset 6144
}
```

---

## Mapa de Memoria EEPROM

### Direcciones y Tamaños

| Offset | Tamaño | Contenido | Marcador de Validez |
|--------|--------|-----------|---------------------|
| 0x0000 | ~200B | DeviceConfig | `0xABCD1234` |
| 0x0200 | ~1300B | StoredCodes (50 códigos) | `0xCAFEBABE` |
| 0x1000 | ~400B | BLEAuthConfig (5 usuarios) | `0xBLE4AUTH` |
| 0x1800 | ~600B | RemoteStoredCodes | `0xREMOTE01` |

### Constantes de Offset

```cpp
#define EEPROM_CONFIG_OFFSET      0       // Configuración principal
#define EEPROM_CODES_OFFSET       512     // Códigos locales
#define EEPROM_BLE_AUTH_OFFSET    4096    // Autenticación BLE
#define EEPROM_REMOTE_CODES_OFFSET 6144   // Códigos remotos
```

---

## Estructura de Configuración Principal

### DeviceConfig

```cpp
struct DeviceConfig {
  // Identificación
  char deviceName[32];       // Nombre editable del dispositivo
  char fixedSerial[32];      // Serial único generado al inicio
  
  // Configuración de red
  bool useDhcp;              // true = DHCP, false = IP fija
  uint8_t ip[4];             // IP estática configurada
  uint8_t gateway[4];        // Gateway configurado
  uint8_t subnet[4];         // Máscara de subred
  uint8_t dns[4];            // Servidor DNS
  
  // Configuración de relés
  float releDuration;        // Tiempo de activación (segundos)
  
  // Seguridad
  bool localAccessBlocked;   // Bloqueo de acceso local
  bool keyboardReadingEnabled; // Lectura de teclados habilitada
  unsigned long blockDuration; // Duración del bloqueo (ms)
  int maxFailedAttempts;     // Máximo intentos fallidos
  char webPassword[32];      // Contraseña de administración web
  
  // Modo torno
  struct {
    bool enabled;            // Modo torno activo
    uint8_t keyboard1_relay; // Relé para teclado 1
    uint8_t keyboard2_relay; // Relé para teclado 2
    uint8_t reserved[10];    // Reservado para futuro
  } turnstile;
  
  // Marcador de validez
  uint32_t configValid;      // 0xABCD1234 si es válido
};
```

### Funciones de Configuración

```cpp
// Guardar configuración
void saveConfiguration() {
  // Copiar variables globales a estructura
  strncpy(config.deviceName, deviceName, sizeof(config.deviceName));
  config.useDhcp = useDhcp;
  
  for (int i = 0; i < 4; i++) {
    config.ip[i] = staticIP[i];
    config.gateway[i] = staticGateway[i];
    config.subnet[i] = staticSubnet[i];
    config.dns[i] = staticDns[i];
  }
  
  config.releDuration = releDuration;
  config.localAccessBlocked = localAccessBlocked;
  config.keyboardReadingEnabled = keyboardReadingEnabled;
  config.configValid = 0xABCD1234;
  
  EEPROM.put(EEPROM_CONFIG_OFFSET, config);
  EEPROM.commit();
  Serial.println("💾 Configuración guardada");
}

// Cargar configuración
void loadConfiguration() {
  EEPROM.get(EEPROM_CONFIG_OFFSET, config);
  
  if (config.configValid == 0xABCD1234) {
    // Copiar a variables globales
    strncpy(deviceName, config.deviceName, sizeof(deviceName));
    useDhcp = config.useDhcp;
    // ...
    Serial.println("💾 Configuración cargada desde EEPROM");
  } else {
    // Primera ejecución - valores por defecto
    initializeDefaults();
  }
}
```

---

## Almacenamiento de Códigos Locales

### Estructura de Código

```cpp
#define MAX_CODES 50  // Máximo de códigos locales

struct CodeEntry {
  char type[4];       // "PIN" o "TAG"
  char value[17];     // Valor del código (hasta 16 caracteres)
  uint8_t keyboard_id; // Teclado asociado (1 o 2)
  uint8_t relay;      // Relé a activar (1 o 2)
  uint8_t reserved[2]; // Reservado para futuro
};

struct StoredCodes {
  uint32_t validMarker;      // 0xCAFEBABE si es válido
  uint32_t version;          // 1 = antiguo, 2 = actual
  bool localValidationFirst; // true = validar local primero
  uint16_t count;            // Número de códigos almacenados
  CodeEntry codes[MAX_CODES]; // Array de códigos
};
```

### Funciones de Códigos

```cpp
// Guardar códigos
void saveStoredCodes() {
  if (storedCodes == nullptr) return;
  
  storedCodes->validMarker = 0xCAFEBABE;
  storedCodes->version = 2;
  
  EEPROM.put(EEPROM_CODES_OFFSET, *storedCodes);
  bool success = EEPROM.commit();
  
  if (success) {
    Serial.printf("💾 Códigos guardados: %d códigos\n", storedCodes->count);
  } else {
    Serial.println("❌ Error guardando códigos en EEPROM");
  }
}

// Cargar códigos
void loadStoredCodes() {
  storedCodes = (StoredCodes*)malloc(sizeof(StoredCodes));
  EEPROM.get(EEPROM_CODES_OFFSET, *storedCodes);
  
  if (storedCodes->validMarker == 0xCAFEBABE) {
    Serial.printf("💾 Códigos cargados: %d códigos\n", storedCodes->count);
  } else {
    // Inicializar vacío
    storedCodes->validMarker = 0xCAFEBABE;
    storedCodes->version = 2;
    storedCodes->localValidationFirst = true;  // Por defecto: local primero
    storedCodes->count = 0;
    memset(storedCodes->codes, 0, sizeof(storedCodes->codes));
    saveStoredCodes();
  }
}

// Añadir código
bool addCode(const char* type, const char* value, uint8_t keyboard, uint8_t relay) {
  if (storedCodes == nullptr || storedCodes->count >= MAX_CODES) {
    return false;
  }
  
  // Verificar duplicado
  for (int i = 0; i < storedCodes->count; i++) {
    if (strcmp(storedCodes->codes[i].value, value) == 0 &&
        strcmp(storedCodes->codes[i].type, type) == 0) {
      return false;  // Ya existe
    }
  }
  
  // Añadir nuevo código
  int idx = storedCodes->count;
  strncpy(storedCodes->codes[idx].type, type, 3);
  storedCodes->codes[idx].type[3] = '\0';
  strncpy(storedCodes->codes[idx].value, value, 16);
  storedCodes->codes[idx].value[16] = '\0';
  storedCodes->codes[idx].keyboard_id = keyboard;
  storedCodes->codes[idx].relay = relay;
  
  storedCodes->count++;
  storedCodes->version = 2;
  
  saveStoredCodes();
  return true;
}

// Eliminar código
bool deleteCode(int index) {
  if (storedCodes == nullptr || index < 0 || index >= storedCodes->count) {
    return false;
  }
  
  // Mover códigos para llenar el hueco
  for (int i = index; i < storedCodes->count - 1; i++) {
    storedCodes->codes[i] = storedCodes->codes[i + 1];
  }
  
  storedCodes->count--;
  saveStoredCodes();
  return true;
}
```

---

## Autenticación BLE

### Estructura de Usuario BLE

```cpp
#define BLE_MAX_USERS 5

struct BLEAuthConfig {
  uint32_t validMarker;              // 0xBLE4AUTH si es válido
  
  // SUPERADMIN (slot 0)
  bool superadmin_registered;
  uint8_t superadmin_key[64];        // Clave de 64 bytes
  
  // Usuarios adicionales (slots 1-4)
  bool user_enabled[BLE_MAX_USERS];
  char user_names[BLE_MAX_USERS][32];
  uint8_t user_keys[BLE_MAX_USERS][64];
  uint8_t user_permissions[BLE_MAX_USERS];
  
  // Clave maestra del dispositivo (para HKDF)
  uint8_t device_master_key[32];
  bool device_key_initialized;
};
```

### Funciones BLE Auth

```cpp
// Guardar configuración BLE
void saveBLEAuthConfig() {
  bleAuthConfig.validMarker = 0xBLE4AUTH;
  EEPROM.put(EEPROM_BLE_AUTH_OFFSET, bleAuthConfig);
  EEPROM.commit();
  Serial.println("💾 Configuración BLE guardada");
}

// Cargar configuración BLE
void loadBLEAuthConfig() {
  EEPROM.get(EEPROM_BLE_AUTH_OFFSET, bleAuthConfig);
  
  if (bleAuthConfig.validMarker != 0xBLE4AUTH) {
    // Inicializar valores por defecto
    memset(&bleAuthConfig, 0, sizeof(bleAuthConfig));
    bleAuthConfig.validMarker = 0xBLE4AUTH;
    bleAuthConfig.superadmin_registered = false;
    
    for (int i = 0; i < BLE_MAX_USERS; i++) {
      bleAuthConfig.user_enabled[i] = false;
      bleAuthConfig.user_permissions[i] = 0;
    }
    
    saveBLEAuthConfig();
    Serial.println("🔵 [BLE] Configuración inicializada");
  }
}

// Registrar SUPERADMIN
bool registerSuperadmin(const uint8_t* key) {
  if (bleAuthConfig.superadmin_registered) {
    return false;  // Ya registrado
  }
  
  memcpy(bleAuthConfig.superadmin_key, key, 64);
  bleAuthConfig.superadmin_registered = true;
  bleAuthConfig.user_enabled[0] = true;
  bleAuthConfig.user_permissions[0] = 0xFF;  // Todos los permisos
  strncpy(bleAuthConfig.user_names[0], "SUPERADMIN", 31);
  
  saveBLEAuthConfig();
  return true;
}
```

---

## Funciones de Persistencia

### saveConfiguration()

Guarda la configuración principal del dispositivo.

**Qué guarda:**
- Nombre del dispositivo
- Serial fijo
- Configuración de red (DHCP, IP, Gateway, Subnet, DNS)
- Tiempo de relé
- Estado de bloqueo
- Configuración de teclados
- Modo torno
- Contraseña web

**Cuándo se llama:**
- Al cambiar configuración desde web
- Al cambiar configuración desde BLE (FF05, FF06, FF03)
- Al recibir comando MQTT de configuración
- Al cambiar estado de seguridad

### saveStoredCodes()

Guarda los códigos PIN/TAG locales.

**Qué guarda:**
- Array de códigos (hasta 50)
- Contador de códigos
- Modo de validación (local_first o remote_first)
- Versión de estructura

**Cuándo se llama:**
- Al añadir código (BLE FF04, Web, MQTT)
- Al eliminar código
- Al sincronizar códigos remotamente

### saveBLEAuthConfig()

Guarda la configuración de autenticación BLE.

**Qué guarda:**
- Estado de registro de SUPERADMIN
- Claves de usuarios (64 bytes cada una)
- Nombres de usuarios
- Permisos de usuarios
- Clave maestra del dispositivo

**Cuándo se llama:**
- Al registrar SUPERADMIN
- Al añadir/eliminar usuarios BLE
- Al cambiar permisos

---

## Validación e Integridad

### Marcadores de Validez

Cada estructura tiene un marcador único para detectar datos corruptos o no inicializados:

| Estructura | Marcador | Valor |
|------------|----------|-------|
| DeviceConfig | configValid | `0xABCD1234` |
| StoredCodes | validMarker | `0xCAFEBABE` |
| BLEAuthConfig | validMarker | `0xBLE4AUTH` |
| RemoteStoredCodes | validMarker | `0xREMOTE01` |

### Verificación al Cargar

```cpp
void loadStoredCodes() {
  EEPROM.get(EEPROM_CODES_OFFSET, *storedCodes);
  
  if (storedCodes->validMarker != 0xCAFEBABE) {
    Serial.println("⚠️ Códigos no válidos - Inicializando...");
    initializeStoredCodes();
  } else if (storedCodes->version != 2) {
    Serial.println("⚠️ Versión antigua - Migrando...");
    migrateStoredCodes();
  }
}
```

### Versionado de Estructuras

El campo `version` permite migración de datos cuando cambia el formato:

```cpp
void migrateStoredCodes() {
  if (storedCodes->version == 1) {
    // Migración de v1 a v2
    // v1 tenía campos diferentes
    storedCodes->localValidationFirst = true;
    storedCodes->version = 2;
    saveStoredCodes();
    Serial.println("✅ Migración de códigos completada");
  }
}
```

---

## Problemas Resueltos

### Problema 1: Configuración de Red No Persistía

**Síntoma:** Al cambiar IP por BLE, el cambio no se guardaba.

**Causa:** `saveConfiguration()` usaba variables globales (`useDhcp`, `staticIP`), pero el callback BLE modificaba solo `config.*`.

**Solución:**
```cpp
// En FF05 onWrite - actualizar AMBAS
useDhcp = newDhcp;                    // Variable global
staticIP = IPAddress(value[1]...);   // Variable global

config.useDhcp = newDhcp;             // Estructura config
memcpy(config.ip, value + 1, 4);      // Estructura config

saveConfiguration();  // Ahora guarda correctamente
```

### Problema 2: Códigos BLE No Persistían

**Síntoma:** Códigos añadidos por BLE se perdían al reiniciar.

**Causa:** `EEPROM.commit()` dentro del callback BLE causaba crash por timeout/watchdog.

**Solución:** Guardado diferido al loop():
```cpp
// En callback BLE
pendingCode.pending = true;
// Responder inmediatamente

// En loop()
if (pendingCode.pending) {
  pendingCode.pending = false;
  saveStoredCodes();  // Aquí es seguro
}
```

### Problema 3: Datos Corruptos Tras Corte de Energía

**Síntoma:** Configuración se corrompía si había corte durante escritura.

**Solución:** Verificación de marcador y reinicialización:
```cpp
if (config.configValid != 0xABCD1234) {
  Serial.println("⚠️ Config corrupta - Restaurando defaults");
  initializeDefaults();
  saveConfiguration();
}
```

---

## Buenas Prácticas

### 1. Minimizar Escrituras EEPROM

La Flash tiene ciclos de escritura limitados (~100,000). Evitar escrituras innecesarias:

```cpp
// MAL: Guardar en cada cambio mínimo
void setRelayDuration(float duration) {
  config.releDuration = duration;
  saveConfiguration();  // Escritura innecesaria si no cambió
}

// BIEN: Verificar si cambió
void setRelayDuration(float duration) {
  if (config.releDuration != duration) {
    config.releDuration = duration;
    saveConfiguration();
  }
}
```

### 2. No Bloquear en Callbacks

```cpp
// MAL: Guardar directamente en callback BLE
void onWrite(NimBLECharacteristic* pChar) {
  addCode(...);
  saveStoredCodes();  // BLOQUEANTE - puede causar crash
  pChar->notify();
}

// BIEN: Diferir guardado
void onWrite(NimBLECharacteristic* pChar) {
  addCodeToRAM(...);
  pendingCode.pending = true;
  pChar->notify();  // Responder rápido
}

// En loop()
if (pendingCode.pending) {
  saveStoredCodes();  // Seguro aquí
  pendingCode.pending = false;
}
```

### 3. Usar Marcadores de Validez

```cpp
// Siempre verificar antes de usar
if (storedCodes->validMarker == 0xCAFEBABE) {
  // Datos válidos
} else {
  // Inicializar
}
```

### 4. Mantener Coherencia RAM/EEPROM

```cpp
// Actualizar variables globales Y estructura
useDhcp = newValue;           // Global
config.useDhcp = newValue;    // Estructura
saveConfiguration();          // EEPROM
```

### 5. Logging de Operaciones

```cpp
void saveConfiguration() {
  EEPROM.put(0, config);
  bool success = EEPROM.commit();
  
  if (success) {
    Serial.println("💾 Configuración guardada correctamente");
  } else {
    Serial.println("❌ ERROR: Fallo al guardar configuración");
    publishError(5, "Error de escritura EEPROM");
  }
}
```

---

## Resumen de Estructuras

| Estructura | Offset | Tamaño | Marcador | Contenido |
|------------|--------|--------|----------|-----------|
| DeviceConfig | 0 | ~200B | 0xABCD1234 | Config general, red, seguridad |
| StoredCodes | 512 | ~1300B | 0xCAFEBABE | 50 códigos PIN/TAG locales |
| BLEAuthConfig | 4096 | ~400B | 0xBLE4AUTH | 5 usuarios BLE, claves |
| RemoteStoredCodes | 6144 | ~600B | 0xREMOTE01 | Códigos sincronizados |

---

## Comandos de Debug

### Ver Estado de Memoria (Serial)

```
💾 === ESTADO EEPROM ===
💾 Config: Válida (0xABCD1234)
💾 Códigos: 5/50 (0xCAFEBABE)
💾 BLE Auth: SUPERADMIN registrado
💾 Usuarios BLE: 1 activos
💾 ======================
```

### Reset de Fábrica (Web)

Endpoint: `GET /factory_reset`

```cpp
void handleFactoryReset() {
  // Invalidar todas las estructuras
  config.configValid = 0;
  storedCodes->validMarker = 0;
  bleAuthConfig.validMarker = 0;
  
  EEPROM.put(0, config);
  EEPROM.put(512, *storedCodes);
  EEPROM.put(4096, bleAuthConfig);
  EEPROM.commit();
  
  ESP.restart();  // Reiniciar para aplicar defaults
}
```

---

## Portal Web de Administración

### Arquitectura del Servidor Web

El firmware incluye un servidor web completo para configuración y monitoreo:

```cpp
#include <WebServer.h>

WebServer server(80);  // Puerto HTTP estándar

void setup() {
  setupWebServer();
  server.begin();
}

void loop() {
  server.handleClient();  // Procesar peticiones HTTP
}
```

### Sistema de Autenticación

El servidor web usa **HTTP Basic Authentication** para proteger todos los endpoints:

```cpp
// Credenciales por defecto
const char* admin_user = "admin";      // Usuario fijo
char admin_password[32] = "admin";     // Contraseña modificable

// Verificación en cada handler
void handleRoot() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();  // Solicitar credenciales
  }
  // ... contenido de la página
}
```

### Almacenamiento de Contraseña

La contraseña web se guarda en EEPROM junto con la configuración principal:

```cpp
struct DeviceConfig {
  // ... otros campos
  char webPassword[32];    // Contraseña de administración web
  // ...
};

// Al guardar configuración
void saveConfiguration() {
  strncpy(config.webPassword, admin_password, sizeof(config.webPassword));
  EEPROM.put(EEPROM_CONFIG_OFFSET, config);
  EEPROM.commit();
}

// Al cargar configuración
void loadConfiguration() {
  EEPROM.get(EEPROM_CONFIG_OFFSET, config);
  if (config.configValid == 0xABCD1234) {
    strncpy(admin_password, config.webPassword, sizeof(admin_password));
  } else {
    // Valor por defecto si no hay configuración
    strncpy(admin_password, "admin", sizeof(admin_password));
  }
}
```

### Cambio de Contraseña

**Endpoint:** `POST /changepass`

```cpp
void handleChangePass() {
  if (!server.authenticate(admin_user, admin_password)) {
    return server.requestAuthentication();
  }
  
  if (server.hasArg("newPassword")) {
    String newPass = server.arg("newPassword");
    if (newPass.length() >= 4 && newPass.length() < 32) {
      newPass.toCharArray(admin_password, sizeof(admin_password));
      saveConfiguration();  // Persistir en EEPROM
      server.send(200, "text/html", "Contraseña actualizada");
    }
  }
}
```

**Formulario HTML:**
```html
<form action="/changepass" method="POST">
  <input type="password" name="newPassword" minlength="4" maxlength="31">
  <button type="submit">Cambiar Contraseña</button>
</form>
```

### Mapa de Endpoints

#### Configuración Principal

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/` | GET | Página principal con estado |
| `/save` | POST | Guardar configuración general |
| `/changepass` | POST | Cambiar contraseña |
| `/reboot` | GET | Reiniciar dispositivo |
| `/reset` | GET | Restaurar valores por defecto |

#### Control de Relés

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/rele` | GET | Activar/desactivar relé |

**Parámetros:**
- `relay`: Número de relé (1 o 2)
- `action`: on, off, pulse
- `duration`: Duración en ms (para pulse)

#### Gestión de Códigos Locales

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/codes` | GET | Página de códigos locales |
| `/codes/add` | POST | Añadir código |
| `/codes/delete` | GET | Eliminar código |
| `/codes/bulk-import` | POST | Importar CSV masivo |
| `/codes/template` | GET | Descargar plantilla CSV |
| `/export/codes` | GET | Exportar códigos a CSV |

#### Lectura de Tags en Tiempo Real

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/codes/start-tag-reading` | POST | Iniciar modo lectura |
| `/codes/stop-tag-reading` | POST | Detener modo lectura |
| `/codes/read-tags-status` | GET | Estado de lectura |
| `/codes/export-read-tags` | GET | Exportar tags leídos |
| `/codes/load-read-tags` | POST | Cargar tags a memoria |

#### Códigos Remotos

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/remote-codes` | GET | Página de códigos remotos |
| `/remote-codes/add` | POST | Añadir código remoto |
| `/remote-codes/delete` | GET | Eliminar código remoto |
| `/remote-codes/delete-all` | GET | Eliminar todos |
| `/export/remote-codes` | GET | Exportar a CSV |

#### Modo Torno

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/turnstile/config` | POST | Configurar modo torno |
| `/turnstile/reset` | GET | Cancelar solicitud pendiente |

#### Seguridad

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/security/block-access` | GET | Bloquear acceso local |
| `/security/unblock-access` | GET | Desbloquear acceso |
| `/security/disable-keyboards` | GET | Deshabilitar teclados |
| `/security/enable-keyboards` | GET | Habilitar teclados |

#### Entradas Digitales

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/digital_inputs` | GET | Página de entradas digitales |
| `/api/digital_inputs_status` | GET | Estado actual (JSON) |
| `/api/digital_inputs_config` | GET | Configuración (JSON) |
| `/save_digital_input` | POST | Guardar configuración |

#### Sincronización de Hora

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/time/sync` | GET | Página de sincronización |
| `/time/update` | POST | Actualizar hora |

#### Actualización OTA

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/ota` | GET | Página de actualización |
| `/ota/upload` | POST | Subir firmware |
| `/ota/config` | POST | Configurar servidor OTA |
| `/ota/check` | GET | Verificar actualizaciones |
| `/ota/status` | GET | Estado de actualización |

#### Gestión BLE

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/ble` | GET | Página de gestión BLE |
| `/ble/clear-all` | GET | Limpiar todas las vinculaciones |
| `/ble/clear-superadmin` | GET | Eliminar SUPERADMIN |
| `/ble/clear-user` | GET | Eliminar usuario específico |
| `/api/ble/status` | GET | Estado BLE (JSON) |

### Ejemplos de Uso

#### Acceso a la Interfaz Web

```bash
# Navegador
http://192.168.5.86/

# cURL con autenticación
curl -u admin:admin http://192.168.5.86/
```

#### Cambiar Contraseña vía API

```bash
curl -X POST -u admin:admin \
  -d "newPassword=nueva_clave_segura" \
  http://192.168.5.86/changepass
```

#### Activar Relé vía Web

```bash
# Pulso de 3 segundos en relé 1
curl -u admin:admin \
  "http://192.168.5.86/rele?relay=1&action=pulse&duration=3000"
```

#### Obtener Estado BLE (JSON)

```bash
curl -u admin:admin http://192.168.5.86/api/ble/status
```

**Respuesta:**
```json
{
  "ble_enabled": true,
  "connected": false,
  "superadmin_registered": true,
  "active_users": 1,
  "users": [
    {"slot": 0, "name": "SUPERADMIN", "enabled": true, "permissions": 255}
  ]
}
```

### Seguridad del Portal Web

#### Recomendaciones

1. **Cambiar contraseña por defecto**: La contraseña `admin` debe cambiarse inmediatamente
2. **Red segura**: Usar el portal solo en redes internas confiables
3. **HTTPS no disponible**: El tráfico HTTP no está cifrado

#### Limitaciones

- **Sin HTTPS**: Las credenciales se transmiten en Base64 (no cifrado)
- **Sin límite de intentos**: No hay protección contra fuerza bruta en web
- **Usuario fijo**: El nombre de usuario `admin` no puede cambiarse

#### Log de Accesos

```
📊 [WEB] GET / - 192.168.5.100
🔐 [WEB] POST /changepass - Contraseña cambiada
⚠️ [WEB] 401 Unauthorized - /codes
```

### Interfaz de Usuario

La interfaz web usa HTML embebido con CSS inline para una experiencia moderna:

```cpp
String html = "<!DOCTYPE html><html><head>";
html += "<meta charset='UTF-8'>";
html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
html += "<title>SWATID-A2 Control Panel</title>";
html += "<style>";
html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
html += ".card{background:white;border-radius:8px;padding:20px;margin:10px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
html += "button{background:#2196F3;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;}";
html += "button:hover{background:#1976D2;}";
html += "</style></head><body>";
// ... contenido
html += "</body></html>";
```

### Estructura de Página Principal

```
┌─────────────────────────────────────────────┐
│           SWATID-A2 Control Panel           │
│              v4.1.0-BLE                     │
├─────────────────────────────────────────────┤
│  📊 Estado del Sistema                      │
│  ├─ Ethernet: ✅ Conectado (192.168.5.86)   │
│  ├─ MQTT: ✅ Conectado                      │
│  ├─ BLE: ✅ Activo                          │
│  └─ Uptime: 2h 34m                          │
├─────────────────────────────────────────────┤
│  ⚡ Control de Relés                         │
│  ├─ Relé 1: [OFF] [ON] [PULSE]              │
│  └─ Relé 2: [OFF] [ON] [PULSE]              │
├─────────────────────────────────────────────┤
│  🔧 Configuración                            │
│  ├─ [Red] [Códigos] [Seguridad]             │
│  ├─ [Torno] [Entradas] [OTA]                │
│  └─ [BLE] [Hora] [Reiniciar]                │
└─────────────────────────────────────────────┘
```

---

**Estado:** ✅ DOCUMENTACIÓN COMPLETA - FIRMWARE v4.1.0
