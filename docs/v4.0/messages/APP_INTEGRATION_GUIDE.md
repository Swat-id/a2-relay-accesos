# Guía de Integración BLE para APP - SWATID-A2 v4.0

## Problema Conocido

En NimBLE (la librería BLE del ESP32), el callback `onRead` actualiza el valor para la **próxima** lectura, no la actual. Esto significa que si lees una característica antes de que el dispositivo haya actualizado su valor, recibirás datos basura.

## Solución: Flujo Correcto de Lectura

### Paso 1: Conectar y Autenticar

```
1. Conectar al dispositivo BLE (nombre = SWATID_XXXXXXXXXXXX)
2. Descubrir servicios y características
3. Escribir clave de 64 bytes en FF01 (Autenticación)
4. ESPERAR notificación de FF01 con "OK:AUTHENTICATED"
```

### Paso 2: Leer Configuración (DESPUÉS de autenticar)

```
5. ESPERAR 100-200ms después de recibir "OK:AUTHENTICATED"
6. Leer FF09 (Configuración completa) → Ahora devuelve JSON
7. Leer FF0A (Códigos locales) → Ahora devuelve JSON
```

## ⚠️ IMPORTANTE: Orden de Operaciones

```
❌ INCORRECTO:
   Conectar → Leer FF09 → Autenticar → Leer FF09
   (Primera lectura devuelve basura)

✅ CORRECTO:
   Conectar → Autenticar → Esperar OK → Esperar 100ms → Leer FF09
   (Lectura devuelve JSON válido)
```

## Características que Funcionan SIN Autenticación

| UUID | Característica | Formato | Descripción |
|------|----------------|---------|-------------|
| FF03 | Modo | 1 byte | `0x00`=normal, `0x01`=torno |
| FF06 | Tiempo relé | 4 bytes LE | Milisegundos (ej: `B80B0000` = 3000ms) |
| FF07 | Estado | JSON | Estado básico de relés |
| FF08 | Info dispositivo | JSON | Info pública para provisión |

## Características que Requieren Autenticación

| UUID | Característica | Formato | Cuándo se actualiza |
|------|----------------|---------|---------------------|
| FF09 | Config completa | JSON | Tras `OK:AUTHENTICATED` |
| FF0A | Códigos locales | JSON | Tras `OK:AUTHENTICATED` |
| FF05 | Config red | JSON | Tras `OK:AUTHENTICATED` |

## Código de Ejemplo - Flujo Completo

### Android (Kotlin)

```kotlin
class BleA2Manager(private val context: Context) {
    private var authenticated = false
    
    suspend fun connectAndReadConfig(deviceAddress: String, authKey: ByteArray): DeviceConfig? {
        // 1. Conectar
        val device = connect(deviceAddress)
        
        // 2. Autenticar
        val authResult = authenticate(device, authKey)
        if (!authResult.startsWith("OK:")) {
            throw Exception("Autenticación fallida: $authResult")
        }
        authenticated = true
        
        // 3. ESPERAR a que el dispositivo actualice los valores
        delay(150)  // Importante: dar tiempo al dispositivo
        
        // 4. Ahora sí leer FF09
        val configJson = readCharacteristic(device, FF09_UUID)
        
        // 5. Validar que es JSON
        if (!configJson.startsWith("{")) {
            // Si aún no es JSON, reintentar una vez más
            delay(100)
            val retry = readCharacteristic(device, FF09_UUID)
            if (!retry.startsWith("{")) {
                throw Exception("FF09 no devuelve JSON válido")
            }
            return parseConfig(retry)
        }
        
        return parseConfig(configJson)
    }
    
    private suspend fun authenticate(device: BleDevice, key: ByteArray): String {
        // Escribir clave en FF01
        writeCharacteristic(device, FF01_UUID, key)
        
        // Esperar notificación con resultado
        return waitForNotification(device, FF01_UUID, timeout = 5000)
    }
}
```

### iOS (Swift)

```swift
class BleA2Manager: NSObject, CBPeripheralDelegate {
    private var authenticated = false
    private var configCompletion: ((DeviceConfig?) -> Void)?
    
    func connectAndReadConfig(peripheral: CBPeripheral, authKey: Data, completion: @escaping (DeviceConfig?) -> Void) {
        self.configCompletion = completion
        
        // 1. Conectar
        centralManager.connect(peripheral)
        
        // La secuencia continúa en los callbacks...
    }
    
    // Después de escribir la clave de autenticación
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == FF01_UUID else { return }
        
        // Esperar notificación de FF01
        peripheral.setNotifyValue(true, for: characteristic)
    }
    
    // Cuando llega la notificación de autenticación
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value else { return }
        
        if characteristic.uuid == FF01_UUID {
            let response = String(data: data, encoding: .utf8) ?? ""
            
            if response.starts(with: "OK:") {
                authenticated = true
                
                // IMPORTANTE: Esperar antes de leer FF09
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.15) {
                    self.readFF09(peripheral: peripheral)
                }
            }
        }
        
        if characteristic.uuid == FF09_UUID {
            let json = String(data: data, encoding: .utf8) ?? ""
            
            if json.starts(with: "{") {
                // JSON válido
                let config = parseConfig(json)
                configCompletion?(config)
            } else {
                // Reintentar
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                    self.readFF09(peripheral: peripheral)
                }
            }
        }
    }
}
```

## ✅ RECOMENDADO: Usar NOTIFY en lugar de READ

**Desde v4.0.0-BLE (build 2026-02-27)**, FF09 y FF0A tienen NOTIFY habilitado y envían automáticamente el JSON tras autenticación:

```
1. Suscribirse a notificaciones de FF09 y FF0A
2. Autenticar via FF01
3. El dispositivo enviará NOTIFY con el JSON de FF09 automáticamente
4. El dispositivo enviará NOTIFY con el JSON de FF0A automáticamente
5. La APP recibe el JSON sin necesidad de hacer READ
```

**Flujo recomendado con NOTIFY:**
```
┌─────────────────────────────────────────────────────────┐
│                FLUJO CON NOTIFY (RECOMENDADO)           │
├─────────────────────────────────────────────────────────┤
│  1. Conectar al dispositivo                             │
│  2. Leer FF08 (verificar comunicación) ──────► JSON OK  │
│  3. Suscribirse a NOTIFY de FF09                        │
│  4. Suscribirse a NOTIFY de FF0A                        │
│  5. Escribir clave en FF01                              │
│  6. Esperar NOTIFY de FF01 ──────────────► "OK:AUTH..." │
│  7. Esperar NOTIFY de FF09 ──────────────► JSON config  │
│  8. Esperar NOTIFY de FF0A ──────────────► JSON codes   │
└─────────────────────────────────────────────────────────┘
```

Este flujo elimina los problemas de timing ya que no depende de READ.

## Verificación de Funcionamiento

### FF08 (Info Pública) - No requiere autenticación

Lee FF08 primero para verificar que el BLE funciona:

```json
{
  "device_type": "SWATID-A2",
  "serial": "SWATID_8813BFB4F7DB",
  "firmware_version": "v4.0.0",
  "firmware_variant": "BLE",
  "protocol_version": 1,
  "capabilities": {
    "relays": 2,
    "wiegand_inputs": 2,
    "digital_inputs": 2,
    "ble_users": 5
  },
  "superadmin_registered": true,
  "active_users": 0
}
```

Si FF08 devuelve JSON válido, el dispositivo está funcionando correctamente.

### Diagnóstico de Problemas

| Síntoma | Causa | Solución |
|---------|-------|----------|
| FF09 devuelve 4 bytes | Leíste antes de autenticar | Autenticar primero, esperar OK |
| FF09 devuelve `{"error":"NOT_AUTHORIZED"}` | No autenticado | Verificar clave de autenticación |
| FF09 devuelve basura después de OK | Leíste muy rápido | Añadir delay de 100-200ms |
| FF08 devuelve JSON, FF09 no | Falta autenticación | Seguir flujo correcto |

## Resumen del Flujo

```
┌─────────────────────────────────────────────────────────┐
│                    FLUJO CORRECTO                       │
├─────────────────────────────────────────────────────────┤
│  1. Conectar al dispositivo                             │
│  2. Leer FF08 (verificar comunicación) ──────► JSON OK  │
│  3. Escribir clave en FF01                              │
│  4. Esperar NOTIFY de FF01 ──────────────► "OK:AUTH..." │
│  5. DELAY 100-200ms                                     │
│  6. Leer FF09 ───────────────────────────► JSON config  │
│  7. Leer FF0A ───────────────────────────► JSON codes   │
└─────────────────────────────────────────────────────────┘
```

---

## Contacto

Si después de seguir este flujo siguen habiendo problemas, verificar:

1. **Puerto serie del dispositivo**: Conectar monitor serial a 115200 baudios
2. **Buscar logs**: `🔵 [BLE] FF09 - Solicitud de configuración completa`
3. **Si no aparece el log**: El callback no se está ejecutando
4. **Si aparece el log**: El valor se actualizó, pero la lectura fue muy rápida

---

## Correcciones en v4.0.0-BLE (build 2026-02-27)

1. **FF09 ahora tiene NOTIFY**: Envía JSON automáticamente tras autenticación
2. **FF0A ahora notifica**: Envía JSON de códigos automáticamente tras autenticación
3. **setValue con longitud explícita**: Evita problemas de memoria con punteros
4. **Logs mejorados**: El dispositivo ahora muestra en serial el JSON exacto que envía

### Logs a buscar en el monitor serial:

```
🔵 [BLE] FF09 actualizado (XXX bytes): {"device_type":"SWATID-A2",...
🔵 [BLE] FF0A actualizado (página 0, XXX bytes): {"count":0,...
```

Si estos logs aparecen pero la APP recibe basura, el problema está en cómo la APP interpreta los datos (usar notificaciones en lugar de read).

---

*Última actualización: 27 Febrero 2026*
*Firmware: v4.0.0-BLE*
