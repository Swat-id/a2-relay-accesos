#include "local_codes.h"

#include <EEPROM.h>

StoredCodes* storedCodes = nullptr;

void initializeStoredCodes() {
  if (storedCodes == nullptr) {
    storedCodes = (StoredCodes*)malloc(sizeof(StoredCodes));
    if (storedCodes == nullptr) {
      Serial.println("❌ Error: No se pudo asignar memoria para storedCodes");
      return;
    }
    memset(storedCodes, 0, sizeof(StoredCodes));
    storedCodes->validMarker = 0xCAFEBABE;
    storedCodes->version = 2;
    storedCodes->localValidationFirst = true;
    storedCodes->count = 0;
  }
}

void loadStoredCodes() {
  Serial.println("🔄 Cargando códigos desde EEPROM...");
  
  if (storedCodes == nullptr) {
    initializeStoredCodes();
  }
  
  if (storedCodes == nullptr) {
    Serial.println("❌ Error: No se pudo inicializar storedCodes");
    return;
  }
  
  EEPROM.get(EEPROM_CODES_OFFSET, *storedCodes);
  
  // Verificar marcador de validación
  if (storedCodes->validMarker != 0xCAFEBABE) {
    Serial.println("🔧 Inicializando códigos por primera vez...");
    storedCodes->validMarker = 0xCAFEBABE;
    storedCodes->version = 2;
    storedCodes->localValidationFirst = true;
    storedCodes->count = 0;
    memset(storedCodes->codes, 0, sizeof(storedCodes->codes));
    saveStoredCodes();
    Serial.println("✅ Estructura de códigos inicializada correctamente");
  } else {
    if (storedCodes->count > MAX_CODES) {
      Serial.printf("⚠️ Contador inválido (%d > %d). Corrigiendo...\n", storedCodes->count, MAX_CODES);
      storedCodes->count = 0;
      saveStoredCodes();
    }
    Serial.printf("💾 Códigos cargados: %d códigos (versión %d)\n", 
                  storedCodes->count, storedCodes->version);
  }
}
 
void saveStoredCodes() {
  Serial.println("💾 saveStoredCodes() - INICIO");
  
  if (storedCodes == nullptr) {
    Serial.println("❌ Error: storedCodes no inicializado");
    return;
  }
  
  Serial.printf("💾 Datos a guardar: validMarker=0x%08X, count=%d, version=%d\n",
                storedCodes->validMarker, storedCodes->count, storedCodes->version);
  
  if (storedCodes->validMarker != 0xCAFEBABE) {
    Serial.println("❌ Error: Marcador de validación inválido antes de guardar");
    Serial.println("💾 Intentando corregir marcador...");
    storedCodes->validMarker = 0xCAFEBABE;
  }
  
  if (storedCodes->count > MAX_CODES) {
    Serial.printf("❌ Error: Contador de códigos inválido (%d > %d)\n", storedCodes->count, MAX_CODES);
    return;
  }
  
  // Mostrar qué códigos se van a guardar
  Serial.printf("💾 Guardando %d códigos:\n", storedCodes->count);
  for (int i = 0; i < storedCodes->count && i < 5; i++) {
    Serial.printf("   [%d] %s '%s' kb=%d relay=%d\n", i,
                  storedCodes->codes[i].type, storedCodes->codes[i].value,
                  storedCodes->codes[i].keyboard_id, storedCodes->codes[i].relay);
  }
  if (storedCodes->count > 5) {
    Serial.printf("   ... y %d códigos más\n", storedCodes->count - 5);
  }
  
  // Calcular tamaño a guardar
  size_t dataSize = sizeof(StoredCodes);
  Serial.printf("💾 Tamaño de estructura: %d bytes, EEPROM offset: %d\n", 
                dataSize, EEPROM_CODES_OFFSET);
  
  // Guardar en EEPROM
  EEPROM.put(EEPROM_CODES_OFFSET, *storedCodes);
  Serial.println("💾 EEPROM.put() completado, ejecutando commit()...");
  
  bool commitResult = EEPROM.commit();
  if (!commitResult) {
    Serial.println("❌ Error CRÍTICO: EEPROM.commit() retornó FALSE");
    return;
  }
  Serial.println("💾 EEPROM.commit() exitoso");
  
  // Verificar integridad después de guardar
  StoredCodes testCodes;
  EEPROM.get(EEPROM_CODES_OFFSET, testCodes);
  
  Serial.printf("💾 Verificación: validMarker=0x%08X (esperado 0xCAFEBABE), count=%d (esperado %d)\n",
                testCodes.validMarker, testCodes.count, storedCodes->count);
  
  if (testCodes.validMarker != 0xCAFEBABE) {
    Serial.println("❌ Error: Marcador de validación no coincide después de guardar");
    return;
  }
  
  if (testCodes.count != storedCodes->count) {
    Serial.printf("❌ Error: Contador no coincide después de guardar (%d != %d)\n", 
                  testCodes.count, storedCodes->count);
    return;
  }
  
  // Verificar primer y último código
  if (storedCodes->count > 0) {
    int lastIdx = storedCodes->count - 1;
    if (strcmp(testCodes.codes[lastIdx].value, storedCodes->codes[lastIdx].value) != 0) {
      Serial.printf("❌ Error: Último código no coincide! '%s' != '%s'\n",
                    testCodes.codes[lastIdx].value, storedCodes->codes[lastIdx].value);
      return;
    }
    Serial.printf("💾 ✓ Último código verificado: '%s'\n", testCodes.codes[lastIdx].value);
  }
  
  Serial.printf("💾 ✅ Códigos guardados correctamente en EEPROM: %d códigos (versión %d)\n", 
                storedCodes->count, storedCodes->version);
}
 
bool addCode(const char* type, const char* value, int keyboardId, int relay) {
  Serial.println("═══════════════════════════════════════════");
  Serial.println("📝 addCode() - INICIO");
  Serial.printf("📝 Parámetros: type='%s', value='%s', keyboard=%d, relay=%d\n", 
                type, value, keyboardId, relay);
  
  // Verificar puntero
  if (storedCodes == nullptr) {
    Serial.println("❌ Error CRÍTICO: storedCodes es nullptr!");
    return false;
  }
  
  Serial.printf("📝 Estado actual: count=%d, validMarker=0x%08X, version=%d\n",
                storedCodes->count, storedCodes->validMarker, storedCodes->version);
  
  if (storedCodes->count >= MAX_CODES) {
    Serial.printf("❌ Error: Máximo de códigos alcanzado (%d/%d)\n", storedCodes->count, MAX_CODES);
    return false;
  }

  // Validar parámetros
  if (keyboardId < 0 || keyboardId > 2) {
    Serial.printf("❌ Error: keyboardId inválido (%d). Debe ser 0 (ambos), 1 o 2\n", keyboardId);
    return false;
  }
  if (relay < 1 || relay > 2) {
    Serial.printf("❌ Error: relay inválido (%d). Debe ser 1 o 2\n", relay);
    return false;
  }
  
  // Validar value
  if (value == nullptr || strlen(value) == 0) {
    Serial.println("❌ Error: value es NULL o vacío");
    return false;
  }
  if (strlen(value) > 16) {
    Serial.printf("❌ Error: value demasiado largo (%d > 16)\n", strlen(value));
    return false;
  }

  // Verificar duplicados exactos
  Serial.printf("📝 Buscando duplicados entre %d códigos existentes...\n", storedCodes->count);
  for (int i = 0; i < storedCodes->count; i++) {
    if (strcmp(storedCodes->codes[i].type, type) == 0 &&
        strcmp(storedCodes->codes[i].value, value) == 0 &&
        storedCodes->codes[i].keyboard_id == keyboardId) {
      Serial.printf("⚠️ Duplicado encontrado en posición %d\n", i);
      return false;
    }
  }
  Serial.println("📝 No hay duplicados, procediendo a guardar...");

  // Guardar en el slot correspondiente
  int idx = storedCodes->count;
  Serial.printf("📝 Guardando en slot %d...\n", idx);
  
  // Copiar datos del código
  strncpy(storedCodes->codes[idx].type, type, sizeof(storedCodes->codes[idx].type) - 1);
  storedCodes->codes[idx].type[sizeof(storedCodes->codes[idx].type) - 1] = '\0';

  strncpy(storedCodes->codes[idx].value, value, sizeof(storedCodes->codes[idx].value) - 1);
  storedCodes->codes[idx].value[sizeof(storedCodes->codes[idx].value) - 1] = '\0';

  storedCodes->codes[idx].keyboard_id = keyboardId;
  storedCodes->codes[idx].relay = relay;
  storedCodes->codes[idx].reserved = 0;
  storedCodes->count++;
  storedCodes->version = 2;

  Serial.printf("📝 Datos en memoria: type='%s', value='%s', kb=%d, relay=%d\n",
                storedCodes->codes[idx].type, storedCodes->codes[idx].value,
                storedCodes->codes[idx].keyboard_id, storedCodes->codes[idx].relay);
  Serial.printf("📝 Nuevo count=%d, llamando saveStoredCodes()...\n", storedCodes->count);

  // Guardar en EEPROM
  saveStoredCodes();
  
  // Verificar que se guardó leyendo de nuevo
  StoredCodes verification;
  EEPROM.get(EEPROM_CODES_OFFSET, verification);
  
  Serial.printf("📝 Verificación post-guardado: count=%d, validMarker=0x%08X\n",
                verification.count, verification.validMarker);
  
  if (verification.validMarker == 0xCAFEBABE && verification.count == storedCodes->count) {
    // Verificar que el código está en la posición correcta
    if (strcmp(verification.codes[idx].value, value) == 0) {
      Serial.println("✅ ÉXITO: Código guardado y verificado en EEPROM");
      Serial.println("═══════════════════════════════════════════");
      return true;
    } else {
      Serial.printf("❌ Error: Código en EEPROM no coincide! Esperado='%s', Leído='%s'\n",
                    value, verification.codes[idx].value);
    }
  } else {
    Serial.println("❌ Error: Verificación de EEPROM falló");
    Serial.printf("❌ validMarker: esperado=0xCAFEBABE, leído=0x%08X\n", verification.validMarker);
    Serial.printf("❌ count: esperado=%d, leído=%d\n", storedCodes->count, verification.count);
  }
  
  Serial.println("═══════════════════════════════════════════");
  return false;
}

// Sobrecarga para compatibilidad (keyboardId = 0)
bool addCode(const char* type, const char* value, int relay) {
  return addCode(type, value, 0, relay);
}

bool isCodeStored(const char* type, const char* value, int keyboardId, int* relay) {
  for (int i = 0; i < storedCodes->count; i++) {
    if (strcmp(storedCodes->codes[i].type, type) == 0 &&
        strcmp(storedCodes->codes[i].value, value) == 0) {
      
      if (storedCodes->version == 1) {
        // Formato antiguo: válido en cualquier teclado
        if (relay != nullptr) *relay = storedCodes->codes[i].relay;
        return true;
      } else {
        // Formato nuevo: verificar teclado
        if (storedCodes->codes[i].keyboard_id == 0 || 
            storedCodes->codes[i].keyboard_id == keyboardId) {
          if (relay != nullptr) *relay = storedCodes->codes[i].relay;
          return true;
        }
      }
    }
  }
  return false;
}

// Sobrecarga para compatibilidad
bool isCodeStored(const char* type, const char* value, int* relay) {
  return isCodeStored(type, value, 0, relay);
}
 
 bool isCodeStored(const char* type, const char* value) {
   return isCodeStored(type, value, nullptr);
 }

