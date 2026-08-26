# Análisis de Problema: Creación de Códigos BLE

> **Fecha:** 5 Febrero 2026  
> **Versión Firmware:** v4.1.0-BLE  
> **Estado:** ✅ CORREGIDO (v2 - usa addCode())

---

## Síntomas Reportados

1. **FF02 (Relay):** Respuesta corrupta `�4A?` en lugar de `OK:RELAY_ON` o `OK:RELAY_PULSE`
2. **FF04 (AddCode):** "Timeout waiting for confirmation" - código no se crea
3. **Error 201:** `GATT_WRITE_NOT_PERMITTED` al usar `writeWithoutResponse`

---

## Problemas Identificados

### Problema 1: Falta de propiedad `WRITE_NR`

La App usaba `writeWithoutResponse` pero el firmware solo tenía `NIMBLE_PROPERTY::WRITE`.

**Error en Android:**
```
Writing characteristic failed with status code 201
```
El código 201 = `GATT_WRITE_NOT_PERMITTED` significa que la característica no soporta el tipo de escritura solicitado.

### Problema 2: Timeout por operación EEPROM lenta

El callback `onWrite` de FF04 ejecutaba:
1. Validaciones
2. `addCode()` → `saveStoredCodes()` → `EEPROM.commit()`
3. Publicación MQTT
4. **Luego** enviaba la respuesta

El `EEPROM.commit()` puede tardar 50-200ms, causando timeout en el ACK de BLE.

---

## ✅ Correcciones Aplicadas

### Corrección 1: Añadir `WRITE_NR` a características

```cpp
// FF02, FF03, FF04, FF05, FF06, FF0A ahora tienen:
NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
```

Esto permite que la App use **cualquiera** de los dos métodos:
- `write` (con ACK)
- `writeWithoutResponse` (sin ACK)

### Corrección 2: Respuesta inmediata en FF04

Se reestructuró el callback para responder ANTES de guardar en EEPROM:

```cpp
// ANTES (problemático):
1. Validar datos
2. addCode() → saveStoredCodes() → EEPROM.commit()  // ⏱️ Lento
3. pCharacteristic->setValue("OK:CODE_ADDED")
4. pCharacteristic->notify()

// DESPUÉS (optimizado):
1. Validar datos (rápido)
2. Añadir a RAM (muy rápido)
3. pCharacteristic->setValue("OK:CODE_ADDED")  // ✅ Respuesta inmediata
4. pCharacteristic->notify()                   // ✅ ACK enviado
5. saveStoredCodes()                           // Después del ACK
6. Publicar MQTT                               // Después del ACK
```

### Corrección 3: Verificación de duplicados optimizada

Se añadió verificación rápida de duplicados antes de intentar añadir:
- `ERROR:CODE_EXISTS` si el código ya existe
- `ERROR:STORAGE_FULL` si no hay espacio

---

## Instrucciones para la App

### Método Recomendado (Más Confiable)

Usar `write` con response para operaciones críticas:

```typescript
// Para FF04 (AddCode) - Usar write con ACK
await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF04, command);
// El dispositivo enviará ACK automáticamente

// Para FF02 (Relay) - writeWithoutResponse está bien
await BleClient.writeWithoutResponse(deviceId, SERVICE_UUID, CHAR_FF02, command);
// Escuchar NOTIFY para la respuesta
```

### Importante: Limpiar Caché BLE en Android

Si el error 201 persiste después de actualizar el firmware:

1. **Método 1:** En la App, desconectar y reconectar el dispositivo
2. **Método 2:** En Android Settings → Bluetooth → Olvidar dispositivo → Volver a vincular
3. **Método 3:** En Android Settings → Apps → [Tu App] → Clear Cache

---

## Respuestas Esperadas de FF04

| Situación | Respuesta |
|-----------|-----------|
| Éxito | `OK:CODE_ADDED:N` (N = total códigos) |
| No autenticado | `ERROR:NOT_AUTHENTICATED` |
| Sin permiso | `ERROR:NO_PERMISSION` |
| Storage no listo | `ERROR:STORAGE_NOT_READY` |
| Datos inválidos | `ERROR:INVALID_LENGTH` |
| Tipo inválido | `ERROR:INVALID_TYPE` |
| Teclado inválido | `ERROR:INVALID_KEYBOARD` |
| Relé inválido | `ERROR:INVALID_RELAY` |
| Código muy largo | `ERROR:INVALID_CODE_LENGTH` |
| Código duplicado | `ERROR:CODE_EXISTS` |
| Memoria llena | `ERROR:STORAGE_FULL` |

---

## Verificación

Después de actualizar firmware y limpiar caché BLE:

1. ✅ FF04 AddCode recibe `OK:CODE_ADDED:N` sin timeout
2. ✅ FF02 Relay recibe `OK:RELAY_ON` o `OK:RELAY_PULSE`
3. ✅ Monitor serie muestra: `🔵 [BLE] ✓ Código 'XXXX' guardado (total: N)`
4. ✅ Código persiste después de reinicio

---

**Estado:** ✅ FIRMWARE ACTUALIZADO Y SUBIDO A LA PLACA
