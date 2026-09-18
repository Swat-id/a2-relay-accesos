#include "remote_codes.h"

#include <Preferences.h>
#include <time.h>

StoredRemoteCodes* storedRemoteCodes = nullptr;
static Preferences remoteCodesPrefs;

void initializeStoredRemoteCodes() {
  if (storedRemoteCodes == nullptr) {
    storedRemoteCodes = (StoredRemoteCodes*)malloc(sizeof(StoredRemoteCodes));
    if (storedRemoteCodes == nullptr) {
      Serial.println("❌ Error: No se pudo asignar memoria para storedRemoteCodes");
      return;
    }
    memset(storedRemoteCodes, 0, sizeof(StoredRemoteCodes));
    storedRemoteCodes->validMarker = 0xDEADBEEF;
    storedRemoteCodes->version = 1;
    storedRemoteCodes->count = 0;
  }
}

void loadStoredRemoteCodes() {
  if (storedRemoteCodes == nullptr) {
    initializeStoredRemoteCodes();
  }

  if (storedRemoteCodes == nullptr) {
    Serial.println("❌ Error: No se pudo inicializar storedRemoteCodes");
    return;
  }

  bool loaded = false;
  if (remoteCodesPrefs.begin(REMOTE_CODES_NVS_NS, true)) {
    size_t len = remoteCodesPrefs.getBytesLength(REMOTE_CODES_NVS_KEY);
    if (len == sizeof(StoredRemoteCodes)) {
      loaded = remoteCodesPrefs.getBytes(REMOTE_CODES_NVS_KEY, storedRemoteCodes,
                                         sizeof(StoredRemoteCodes)) == sizeof(StoredRemoteCodes);
    } else if (len > 0) {
      Serial.printf("⚠️ Códigos remotos NVS con tamaño inesperado (%d ≠ %d) - se reinicializan\n",
                    (int)len, (int)sizeof(StoredRemoteCodes));
    }
    remoteCodesPrefs.end();
  }

  if (!loaded || storedRemoteCodes->validMarker != 0xDEADBEEF ||
      storedRemoteCodes->version != 1 || storedRemoteCodes->count > MAX_REMOTE_CODES) {
    Serial.println("📦 Inicializando códigos remotos por primera vez");
    storedRemoteCodes->validMarker = 0xDEADBEEF;
    storedRemoteCodes->version = 1;
    storedRemoteCodes->count = 0;
    memset(storedRemoteCodes->codes, 0, sizeof(storedRemoteCodes->codes));
    saveStoredRemoteCodes();
  }

  Serial.printf("📦 Códigos remotos cargados: %d/%d\n", storedRemoteCodes->count, MAX_REMOTE_CODES);
}

void saveStoredRemoteCodes() {
  if (storedRemoteCodes == nullptr) {
    Serial.println("❌ Error: storedRemoteCodes no inicializado");
    return;
  }

  bool saved = false;
  if (remoteCodesPrefs.begin(REMOTE_CODES_NVS_NS, false)) {
    saved = remoteCodesPrefs.putBytes(REMOTE_CODES_NVS_KEY, storedRemoteCodes,
                                      sizeof(StoredRemoteCodes)) == sizeof(StoredRemoteCodes);
    remoteCodesPrefs.end();
  }

  if (saved) {
    Serial.printf("💾 Códigos remotos guardados en NVS: %d códigos\n", storedRemoteCodes->count);
  } else {
    Serial.println("❌ Error guardando códigos remotos en NVS (sin espacio o NVS corrupta)");
  }
}

bool addRemoteCode(const char* type, const char* value, uint8_t keyboardId,
                   uint8_t relay, const TimeSlot* timeSlots, uint8_t timeSlotsCount) {
  if (storedRemoteCodes->count >= MAX_REMOTE_CODES) {
    Serial.println("❌ No se puede añadir código remoto: memoria llena");
    return false;
  }

  // Verificar si el código ya existe
  for (int i = 0; i < storedRemoteCodes->count; i++) {
    if (strcmp(storedRemoteCodes->codes[i].type, type) == 0 &&
        strcmp(storedRemoteCodes->codes[i].value, value) == 0) {
      Serial.printf("⚠️ Código remoto ya existe: %s %s\n", type, value);
      return false;
    }
  }

  // Añadir nuevo código
  RemoteCodeEntry* newCode = &storedRemoteCodes->codes[storedRemoteCodes->count];
  strncpy(newCode->type, type, sizeof(newCode->type) - 1);
  newCode->type[sizeof(newCode->type) - 1] = '\0';
  strncpy(newCode->value, value, sizeof(newCode->value) - 1);
  newCode->value[sizeof(newCode->value) - 1] = '\0';
  newCode->keyboard_id = keyboardId;
  newCode->relay = relay;
  newCode->time_slots_count = min(timeSlotsCount, (uint8_t)4);

  // Copiar franjas horarias
  for (int i = 0; i < newCode->time_slots_count; i++) {
    newCode->time_slots[i] = timeSlots[i];
  }

  storedRemoteCodes->count++;
  saveStoredRemoteCodes();

  Serial.printf("✅ Código remoto añadido: %s %s (Keypad: %d, Relé: %d, Franjas: %d)\n",
                type, value, keyboardId, relay, timeSlotsCount);
  return true;
}

bool deleteRemoteCode(const char* type, const char* value) {
  if (storedRemoteCodes == nullptr) return false;

  for (int i = 0; i < storedRemoteCodes->count; i++) {
    if (strcmp(storedRemoteCodes->codes[i].type, type) == 0 &&
        strcmp(storedRemoteCodes->codes[i].value, value) == 0) {

      // Mover todos los códigos posteriores una posición hacia atrás
      for (int j = i; j < storedRemoteCodes->count - 1; j++) {
        storedRemoteCodes->codes[j] = storedRemoteCodes->codes[j + 1];
      }

      storedRemoteCodes->count--;
      saveStoredRemoteCodes();

      Serial.printf("✅ Código remoto eliminado: %s %s\n", type, value);
      return true;
    }
  }

  Serial.printf("❌ Código remoto no encontrado: %s %s\n", type, value);
  return false;
}

void deleteAllRemoteCodes() {
  if (storedRemoteCodes == nullptr) return;

  storedRemoteCodes->count = 0;
  memset(storedRemoteCodes->codes, 0, sizeof(storedRemoteCodes->codes));
  saveStoredRemoteCodes();
  Serial.println("✅ Todos los códigos remotos eliminados");
}

bool isRemoteCodeStored(const char* type, const char* value, uint8_t keyboardId,
                        uint8_t* relay) {
  if (storedRemoteCodes == nullptr) return false;

  for (int i = 0; i < storedRemoteCodes->count; i++) {
    RemoteCodeEntry* code = &storedRemoteCodes->codes[i];

    if (strcmp(code->type, type) == 0 && strcmp(code->value, value) == 0) {
      // Verificar keypad (0 = ambos, o específico)
      if (code->keyboard_id == 0 || code->keyboard_id == keyboardId) {
        // Verificar franjas horarias si existen
        if (code->time_slots_count > 0) {
          if (!isCurrentTimeInTimeSlots(code->time_slots, code->time_slots_count)) {
            Serial.printf("⏰ Código remoto fuera de horario: %s %s\n", type, value);
            return false;
          }
        }

        if (relay) {
          *relay = code->relay;
        }
        Serial.printf("✅ Código remoto válido: %s %s (Relé: %d)\n", type, value, code->relay);
        return true;
      }
    }
  }

  return false;
}

bool isTimeSlotValid(const TimeSlot& timeSlot) {
  return (timeSlot.start_hour < 24 && timeSlot.start_minute < 60 &&
          timeSlot.end_hour < 24 && timeSlot.end_minute < 60 &&
          timeSlot.days_of_week > 0 && timeSlot.days_of_week <= 127);
}

// Convierte el bitmask de días de la semana a una cadena legible
String getDaysOfWeekString(uint8_t days_of_week) {
  String result = "";
  const char* dayNames[] = {"Lun", "Mar", "Mié", "Jue", "Vie", "Sáb", "Dom"};
  const uint8_t dayBits[] = {1, 2, 4, 8, 16, 32, 64};

  // Casos especiales
  if (days_of_week == 127) {
    return "Todos los días";
  }
  if (days_of_week == 31) {
    return "Lun-Vie";
  }
  if (days_of_week == 96) {
    return "Sáb-Dom";
  }

  // Construcción personalizada
  int dayCount = 0;
  for (int i = 0; i < 7; i++) {
    if (days_of_week & dayBits[i]) {
      if (dayCount > 0) {
        result += ", ";
      }
      result += dayNames[i];
      dayCount++;
    }
  }

  return result.length() > 0 ? result : "Ninguno";
}

bool isCurrentTimeInTimeSlots(const TimeSlot* timeSlots, uint8_t timeSlotsCount) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("⚠️ No se puede obtener la hora actual para validación");
    return true; // Si no hay hora, permitir acceso
  }

  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentWeekday = timeinfo.tm_wday; // 0=Domingo, 1=Lunes, ..., 6=Sábado

  // Convertir domingo (0) a bit 64, lunes (1) a bit 1, etc.
  uint8_t currentDayBit = (currentWeekday == 0) ? 64 : (1 << (currentWeekday - 1));

  for (int i = 0; i < timeSlotsCount; i++) {
    const TimeSlot& slot = timeSlots[i];

    // Verificar día de la semana
    if (!(slot.days_of_week & currentDayBit)) {
      continue;
    }

    // Verificar hora
    int currentTotalMinutes = currentHour * 60 + currentMinute;
    int startTotalMinutes = slot.start_hour * 60 + slot.start_minute;
    int endTotalMinutes = slot.end_hour * 60 + slot.end_minute;

    if (currentTotalMinutes >= startTotalMinutes && currentTotalMinutes <= endTotalMinutes) {
      return true;
    }
  }

  return false;
}
