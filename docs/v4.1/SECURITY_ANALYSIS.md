# Análisis de Seguridad v4.1

## Fecha: 5 de Febrero de 2026
## Rama: v4.1

---

## Resumen Ejecutivo

Este documento analiza tres problemas críticos de seguridad y funcionalidad identificados en el firmware v4.0.0-BLE:

1. **Seguridad BLE**: Transmisión de clave completa en cada operación
2. **Seguridad MQTT**: Transmisión de claves de usuario en texto claro
3. **Persistencia de Códigos**: Problemas con el guardado de códigos en memoria

---

## 1. PROBLEMA: Seguridad en Comunicación BLE

### Estado Actual

```
┌─────────────────┐                    ┌─────────────────┐
│      APP        │                    │    Dispositivo   │
└────────┬────────┘                    └────────┬────────┘
         │                                      │
         │  1. Conexión BLE                     │
         │─────────────────────────────────────>│
         │                                      │
         │  2. Envía clave 64 bytes (FF01)     │
         │─────────────────────────────────────>│
         │                                      │
         │  3. OK:AUTHENTICATED / ERROR        │
         │<─────────────────────────────────────│
         │                                      │
         │  4. Cada operación verifican        │
         │     bleAuthenticated (booleano)     │
         │                                      │
```

### Problemas Identificados

1. **Clave transmitida en claro**: La clave de 64 bytes se envía sin cifrar
2. **Sin token de sesión**: Si alguien captura la clave BLE, puede usarla indefinidamente
3. **Sin protección contra replay attacks**: Un atacante puede reenviar la clave capturada
4. **Autenticación estática**: La misma clave siempre produce el mismo resultado

### Solución Propuesta: Challenge-Response con Token de Sesión

```
┌─────────────────┐                    ┌─────────────────┐
│      APP        │                    │    Dispositivo   │
└────────┬────────┘                    └────────┬────────┘
         │                                      │
         │  1. Conexión BLE                     │
         │─────────────────────────────────────>│
         │                                      │
         │  2. REQUEST_CHALLENGE                │
         │─────────────────────────────────────>│
         │                                      │
         │  3. CHALLENGE: nonce (16 bytes random)│
         │<─────────────────────────────────────│
         │                                      │
         │  APP calcula:                        │
         │  response = SHA256(key + nonce)      │
         │                                      │
         │  4. AUTH_RESPONSE: response (32 bytes)│
         │─────────────────────────────────────>│
         │                                      │
         │  Dispositivo calcula:                │
         │  expected = SHA256(stored_key + nonce)│
         │  if (response == expected):          │
         │    sessionToken = random(8 bytes)    │
         │                                      │
         │  5. OK:TOKEN:XXXXXXXX (hex)          │
         │<─────────────────────────────────────│
         │                                      │
         │  6. Operaciones incluyen token       │
         │     en cabecera de cada comando      │
         │─────────────────────────────────────>│
         │                                      │
```

### Implementación Técnica

#### Nuevas Variables Globales

```cpp
// Token de sesión BLE (8 bytes = 16 hex chars)
uint8_t bleSessionNonce[16];        // Challenge generado por el dispositivo
uint8_t bleSessionToken[8];         // Token de sesión para operaciones
bool bleSessionValid = false;       // Si hay sesión activa
unsigned long bleSessionTime = 0;   // Tiempo de inicio de sesión

#define BLE_SESSION_TIMEOUT_MS 300000  // 5 minutos de inactividad
```

#### Nueva Característica BLE: FF00 (Challenge)

```cpp
// UUID: 0000FF00-0000-1000-8000-00805F9B34FB
// Propiedades: READ | NOTIFY

// Al leer: genera un nuevo nonce y lo devuelve
void ChallengeCharCallbacks::onRead(NimBLECharacteristic* pChar) {
    esp_fill_random(bleSessionNonce, 16);
    pChar->setValue(bleSessionNonce, 16);
    pChar->notify();
}
```

#### Modificación FF01 (Auth) para Challenge-Response

```cpp
void AuthCharCallbacks::onWrite(NimBLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    
    if (value.length() == 32) {  // Response de 32 bytes (SHA256)
        // Verificar contra cada clave almacenada
        for (int u = -1; u < BLE_MAX_USERS; u++) {  // -1 = superadmin
            uint8_t* storedKey = (u == -1) ? 
                bleAuthConfig.superadmin_key : 
                bleAuthConfig.user_keys[u];
            
            if (u == -1 && !bleAuthConfig.superadmin_registered) continue;
            if (u >= 0 && !bleAuthConfig.user_enabled[u]) continue;
            
            // Calcular expected = SHA256(storedKey + nonce)
            uint8_t expected[32];
            mbedtls_sha256_context ctx;
            mbedtls_sha256_init(&ctx);
            mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256
            mbedtls_sha256_update(&ctx, storedKey, BLE_KEY_SIZE);
            mbedtls_sha256_update(&ctx, bleSessionNonce, 16);
            mbedtls_sha256_finish(&ctx, expected);
            mbedtls_sha256_free(&ctx);
            
            if (memcmp(value.c_str(), expected, 32) == 0) {
                // Autenticación exitosa
                esp_fill_random(bleSessionToken, 8);
                bleSessionValid = true;
                bleSessionTime = millis();
                
                // Responder con token
                char tokenHex[17];
                for (int i = 0; i < 8; i++) {
                    sprintf(&tokenHex[i*2], "%02X", bleSessionToken[i]);
                }
                String response = "OK:TOKEN:" + String(tokenHex);
                pChar->setValue(response.c_str());
                pChar->notify();
                return;
            }
        }
        // Ninguna clave coincide
        pChar->setValue("ERROR:INVALID_RESPONSE");
        pChar->notify();
    }
}
```

#### Verificación de Token en Operaciones

```cpp
// Cada característica verifica el token en los primeros 8 bytes
bool verifySessionToken(const uint8_t* receivedToken) {
    if (!bleSessionValid) return false;
    
    // Verificar timeout de sesión
    if (millis() - bleSessionTime > BLE_SESSION_TIMEOUT_MS) {
        bleSessionValid = false;
        return false;
    }
    
    // Actualizar timestamp de actividad
    bleSessionTime = millis();
    
    return memcmp(receivedToken, bleSessionToken, 8) == 0;
}

// Ejemplo de uso en FF02 (Relay Control)
void RelayCharCallbacks::onWrite(NimBLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    
    if (value.length() < 9) {  // 8 bytes token + 1 byte comando mínimo
        pChar->setValue("ERROR:INVALID_FORMAT");
        pChar->notify();
        return;
    }
    
    if (!verifySessionToken((const uint8_t*)value.c_str())) {
        pChar->setValue("ERROR:INVALID_TOKEN");
        pChar->notify();
        return;
    }
    
    // Procesar comando (desde byte 8 en adelante)
    uint8_t command = value[8];
    // ... resto de la lógica
}
```

### Beneficios de esta Solución

| Aspecto | Antes (v4.0) | Después (v4.1) |
|---------|--------------|----------------|
| Clave en tráfico | Sí (64 bytes) | No (solo response SHA256) |
| Replay attacks | Vulnerable | Protegido (nonce único) |
| Intercepción | Exposición total | Sin exposición de clave |
| Token de sesión | No existe | Sí (8 bytes, temporal) |
| Timeout de sesión | Solo conexión | Sesión + inactividad |

### Impacto en la APP

La APP deberá actualizar su flujo de autenticación:

```typescript
// Flujo anterior
async function authenticate(deviceId: string, key: Uint8Array) {
    await ble.write(deviceId, 'FF01', key);  // 64 bytes
}

// Flujo nuevo
async function authenticate(deviceId: string, key: Uint8Array) {
    // 1. Obtener challenge
    const nonce = await ble.read(deviceId, 'FF00');  // 16 bytes
    
    // 2. Calcular response
    const data = new Uint8Array([...key, ...nonce]);
    const response = await crypto.subtle.digest('SHA-256', data);
    
    // 3. Enviar response
    const result = await ble.write(deviceId, 'FF01', new Uint8Array(response));
    
    // 4. Extraer token de respuesta "OK:TOKEN:XXXXXXXX"
    const token = parseToken(result);  // 8 bytes
    
    // 5. Usar token en todas las operaciones siguientes
    return token;
}

// Operaciones con token
async function activateRelay(deviceId: string, token: Uint8Array, relay: number) {
    const command = new Uint8Array([...token, relay]);  // token + comando
    await ble.write(deviceId, 'FF02', command);
}
```

---

## 2. PROBLEMA: Seguridad MQTT para Claves de Usuario

### Estado Actual

```json
// Comando actual para añadir usuario BLE vía MQTT
{
    "message_type": 4,
    "message_id": "xyz123",
    "message_info": {
        "action": "add_user",
        "slot": 1,
        "key": "0A1B2C3D4E5F...64bytesHex...",  // ← EXPUESTO EN CLARO
        "name": "Usuario1",
        "permissions": 15
    }
}
```

### Problemas Identificados

1. **Clave en texto claro**: La clave hexadecimal viaja sin cifrar por MQTT
2. **Sin cifrado**: Aunque MQTT pueda usar TLS, el payload es legible
3. **Log de mensajes**: Las claves pueden quedar en logs del broker/servidor

### Solución Propuesta: Clave Precompartida con Derivación

#### Opción A: Cifrado AES con Clave de Dispositivo

```
┌─────────────────┐                    ┌─────────────────┐                    ┌─────────────────┐
│    Servidor     │                    │     Broker      │                    │   Dispositivo   │
└────────┬────────┘                    └────────┬────────┘                    └────────┬────────┘
         │                                      │                                      │
         │  El servidor conoce:                 │                                      │
         │  - device_key (clave maestra)        │  El dispositivo tiene:               │
         │  - user_key (a enviar)               │  - device_key (en EEPROM)            │
         │                                      │                                      │
         │  Servidor calcula:                   │                                      │
         │  nonce = random(12 bytes)            │                                      │
         │  encrypted = AES-GCM-256(            │                                      │
         │    key=device_key,                   │                                      │
         │    nonce=nonce,                      │                                      │
         │    plaintext=user_key                │                                      │
         │  )                                   │                                      │
         │                                      │                                      │
         │  { "action": "add_user_secure",      │                                      │
         │    "slot": 1,                        │                                      │
         │    "nonce": "XXX",                   │─────────────────────────────────────>│
         │    "encrypted_key": "YYY",           │                                      │
         │    "tag": "ZZZ" }                    │                                      │
         │                                      │                                      │
         │                                      │  Dispositivo descifra:               │
         │                                      │  user_key = AES-GCM-256-decrypt(     │
         │                                      │    key=device_key,                   │
         │                                      │    nonce=nonce,                      │
         │                                      │    ciphertext=encrypted_key,         │
         │                                      │    tag=tag                           │
         │                                      │  )                                   │
         │                                      │                                      │
```

#### Implementación en el Dispositivo

```cpp
// En EEPROM: clave maestra del dispositivo (32 bytes)
// Esta clave se genera una vez y se comparte con el servidor durante el registro
uint8_t deviceMasterKey[32];  // Almacenada en EEPROM seguro

// Función para descifrar clave de usuario
bool decryptUserKey(
    const uint8_t* nonce,        // 12 bytes
    const uint8_t* encrypted,    // 64 bytes (clave usuario cifrada)
    const uint8_t* tag,          // 16 bytes (autenticación)
    uint8_t* decryptedKey        // Output: 64 bytes
) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, 
                                  deviceMasterKey, 256);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }
    
    ret = mbedtls_gcm_auth_decrypt(&gcm,
        64,              // length
        nonce, 12,       // IV
        NULL, 0,         // AAD (optional)
        tag, 16,         // tag
        encrypted,       // input
        decryptedKey     // output
    );
    
    mbedtls_gcm_free(&gcm);
    return (ret == 0);
}

// Handler MQTT para add_user_secure
if (bleAction == "add_user_secure") {
    String nonceHex = doc["message_info"]["nonce"].as<String>();
    String encryptedHex = doc["message_info"]["encrypted_key"].as<String>();
    String tagHex = doc["message_info"]["tag"].as<String>();
    int slot = doc["message_info"]["slot"].as<int>();
    
    // Convertir de hex a bytes
    uint8_t nonce[12], encrypted[64], tag[16], decryptedKey[64];
    hexToBytes(nonceHex, nonce, 12);
    hexToBytes(encryptedHex, encrypted, 64);
    hexToBytes(tagHex, tag, 16);
    
    if (decryptUserKey(nonce, encrypted, tag, decryptedKey)) {
        // Guardar clave descifrada
        int idx = slot - 1;
        memcpy(bleAuthConfig.user_keys[idx], decryptedKey, BLE_KEY_SIZE);
        bleAuthConfig.user_enabled[idx] = 1;
        saveBLEAuthConfig();
        
        // Limpiar memoria sensible
        memset(decryptedKey, 0, 64);
        
        publishResponse(0, receivedMessageId, "user key set securely");
    } else {
        publishResponse(1, receivedMessageId, "decryption failed");
    }
}
```

#### Opción B: Derivación de Clave con HKDF (Más Simple)

Si el overhead de AES-GCM es demasiado, usar derivación de clave:

```cpp
// El servidor y dispositivo comparten:
// - device_master_key (32 bytes, nunca se transmite)
// - serial_number (conocido)

// Comando MQTT:
{
    "action": "add_user_derived",
    "slot": 1,
    "user_id": "user123",     // Identificador único del usuario
    "name": "Usuario1",
    "permissions": 15
}

// Tanto servidor como dispositivo calculan:
// user_key = HKDF(
//     key = device_master_key,
//     salt = serial_number,
//     info = "user_key:" + user_id,
//     length = 64
// )

// La clave nunca viaja por MQTT, solo el user_id
```

### Comparación de Opciones

| Aspecto | Opción A (AES-GCM) | Opción B (HKDF) |
|---------|-------------------|-----------------|
| Seguridad | Alta (cifrado) | Alta (derivación) |
| Complejidad | Media | Baja |
| Tamaño mensaje | Mayor (+nonce+tag) | Menor |
| Flexibilidad | Clave arbitraria | Clave determinística |
| RAM requerida | ~2KB más | ~500 bytes más |
| Biblioteca | mbedtls (incluida) | mbedtls (incluida) |

### Recomendación

**Opción B (HKDF)** es más simple y adecuada para este caso porque:
- No necesita transmitir la clave real
- Menor tamaño de mensaje MQTT
- Más fácil de implementar
- La APP puede derivar la misma clave localmente

---

## 3. PROBLEMA: Persistencia de Códigos

### Estado Actual

Según el análisis del código:

```cpp
// addCode() en línea 4840-4885
bool addCode(const char* type, const char* value, int keyboardId, int relay) {
    // ... validaciones ...
    
    storedCodes->count++;
    storedCodes->version = 2;
    
    saveStoredCodes();  // ← SÍ SE LLAMA
    
    // Verificación de guardado
    if (storedCodes->validMarker == 0xCAFEBABE && storedCodes->count > 0) {
        Serial.printf("✅ Código guardado exitosamente en EEPROM\n");
    }
    return true;
}
```

### Diagnóstico

El código **SÍ llama a `saveStoredCodes()`**, pero hay posibles puntos de fallo:

1. **Verificación de puntero**: `storedCodes` podría no estar inicializado
2. **Commit de EEPROM**: El commit podría fallar silenciosamente
3. **Espacio EEPROM**: Podría estar lleno o corrupto
4. **Timing BLE**: La operación BLE puede terminar antes del guardado

### Verificación Necesaria

Para confirmar el problema, añadir logging extensivo:

```cpp
bool addCode(const char* type, const char* value, int keyboardId, int relay) {
    Serial.println("=== DEBUG addCode START ===");
    Serial.printf("  storedCodes ptr: %p\n", (void*)storedCodes);
    
    if (storedCodes == nullptr) {
        Serial.println("  ERROR: storedCodes es NULL!");
        return false;
    }
    
    Serial.printf("  validMarker: 0x%08X (expected 0xCAFEBABE)\n", storedCodes->validMarker);
    Serial.printf("  count antes: %d\n", storedCodes->count);
    
    // ... resto del código ...
    
    storedCodes->count++;
    Serial.printf("  count después: %d\n", storedCodes->count);
    
    Serial.println("  Llamando saveStoredCodes()...");
    saveStoredCodes();
    
    // Verificar inmediatamente
    StoredCodes testRead;
    EEPROM.get(EEPROM_CODES_OFFSET, testRead);
    Serial.printf("  Verificación post-guardado:\n");
    Serial.printf("    testRead.validMarker: 0x%08X\n", testRead.validMarker);
    Serial.printf("    testRead.count: %d\n", testRead.count);
    
    Serial.println("=== DEBUG addCode END ===");
    return true;
}
```

### Posibles Causas del Problema Reportado

1. **La APP no espera confirmación**: Timeout antes de que el dispositivo responda
2. **El notify() no se ejecuta**: La app no recibe la confirmación
3. **Problema de EEPROM**: El guardado falla pero no se reporta
4. **Problema de memoria**: storedCodes no está inicializado

### Solución Propuesta

#### A. Mejorar Confirmación BLE (FF04)

```cpp
void AddCodeCharCallbacks::onWrite(NimBLECharacteristic* pChar) {
    // ... validaciones previas ...
    
    Serial.println("🔵 [BLE] FF04 - Iniciando addCode...");
    
    bool added = addCode(typeStr, code.c_str(), keyboard, relay);
    
    String response;
    if (added) {
        // Crear respuesta JSON con detalles
        DynamicJsonDocument doc(256);
        doc["status"] = "OK";
        doc["action"] = "CODE_ADDED";
        doc["type"] = typeStr;
        doc["code"] = code.substring(0, 4) + "***";  // Parcialmente oculto
        doc["keyboard"] = keyboard;
        doc["relay"] = relay;
        doc["total"] = storedCodes->count;
        
        serializeJson(doc, response);
    } else {
        response = "{\"status\":\"ERROR\",\"reason\":\"CODE_EXISTS_OR_FULL\"}";
    }
    
    // Asegurar que se envía la respuesta
    pChar->setValue((uint8_t*)response.c_str(), response.length());
    
    // Delay pequeño antes de notify para asegurar estabilidad
    delay(10);
    
    pChar->notify();
    Serial.printf("🔵 [BLE] FF04 - Respuesta enviada: %s\n", response.c_str());
}
```

#### B. Verificación Proactiva de EEPROM

```cpp
// Añadir función de verificación de integridad
bool verifyEEPROMIntegrity() {
    StoredCodes testCodes;
    EEPROM.get(EEPROM_CODES_OFFSET, testCodes);
    
    if (testCodes.validMarker != 0xCAFEBABE) {
        Serial.println("⚠️ EEPROM: Marcador inválido");
        return false;
    }
    
    if (testCodes.count > MAX_CODES) {
        Serial.println("⚠️ EEPROM: Contador corrupto");
        return false;
    }
    
    return true;
}

// Llamar periódicamente en loop() o después de operaciones críticas
```

---

## 4. Plan de Implementación v4.1

### Fase 1: Diagnóstico y Logging (Inmediato)

| Tarea | Prioridad | Esfuerzo |
|-------|-----------|----------|
| Añadir logging detallado a addCode() | Alta | 30 min |
| Añadir logging detallado a FF04 | Alta | 30 min |
| Verificar con pruebas reales | Alta | 1 hora |

### Fase 2: Seguridad BLE (2-3 días)

| Tarea | Prioridad | Esfuerzo |
|-------|-----------|----------|
| Implementar FF00 (Challenge) | Alta | 2 horas |
| Modificar FF01 (Challenge-Response) | Alta | 3 horas |
| Añadir token de sesión | Alta | 2 horas |
| Modificar todas las características para usar token | Media | 4 horas |
| Pruebas de integración | Alta | 2 horas |
| Actualizar documentación APP | Media | 2 horas |

### Fase 3: Seguridad MQTT (1-2 días)

| Tarea | Prioridad | Esfuerzo |
|-------|-----------|----------|
| Implementar device_master_key en EEPROM | Alta | 1 hora |
| Implementar HKDF para derivación | Alta | 2 horas |
| Añadir comando add_user_derived | Media | 1 hora |
| Pruebas con servidor | Media | 2 horas |

### Fase 4: Persistencia de Códigos (1 día)

| Tarea | Prioridad | Esfuerzo |
|-------|-----------|----------|
| Implementar logging de diagnóstico | Alta | 1 hora |
| Identificar punto de fallo | Alta | 2 horas |
| Corregir el problema | Alta | Variable |
| Añadir verificación post-guardado | Media | 1 hora |

---

## 5. Recursos Necesarios

### Bibliotecas ESP32 (ya incluidas)

- `mbedtls` - Para SHA256, AES-GCM, HKDF
- `NimBLE-Arduino` - Para BLE
- `ArduinoJson` - Para parseo JSON

### Memoria Adicional Estimada

| Componente | Flash | RAM |
|------------|-------|-----|
| Challenge-Response | +2KB | +100 bytes |
| Token de sesión | +500 bytes | +50 bytes |
| HKDF (MQTT) | +1KB | +200 bytes |
| **Total** | **~3.5KB** | **~350 bytes** |

### Compatibilidad

- **Firmware**: Retrocompatible con dispositivos existentes (primera autenticación genera token)
- **APP**: Requiere actualización para nuevo flujo de autenticación
- **Servidor**: Requiere actualización para envío seguro de claves

---

## 6. Conclusiones

Los tres problemas identificados tienen soluciones viables:

1. **BLE**: Challenge-Response + Token de sesión elimina la exposición de claves
2. **MQTT**: HKDF permite añadir usuarios sin transmitir claves
3. **Códigos**: Requiere diagnóstico pero el código base parece correcto

La implementación debe hacerse por fases para minimizar riesgos y permitir pruebas incrementales.

---

## Apéndice: Código de Referencia

### SHA256 con mbedtls

```cpp
#include "mbedtls/sha256.h"

void sha256(const uint8_t* input, size_t len, uint8_t* output) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (1 = SHA224)
    mbedtls_sha256_update(&ctx, input, len);
    mbedtls_sha256_finish(&ctx, output);
    mbedtls_sha256_free(&ctx);
}
```

### Generación de Números Aleatorios

```cpp
#include "esp_random.h"

void generateRandom(uint8_t* buffer, size_t len) {
    esp_fill_random(buffer, len);
}
```

### HKDF con mbedtls

```cpp
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"

bool hkdf_sha256(
    const uint8_t* key, size_t key_len,
    const uint8_t* salt, size_t salt_len,
    const uint8_t* info, size_t info_len,
    uint8_t* output, size_t output_len
) {
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return mbedtls_hkdf(md, salt, salt_len, key, key_len, 
                        info, info_len, output, output_len) == 0;
}
```
