# Firmware SWATID-A2 v3.0.0

## 📦 Información del Firmware

- **Versión**: 3.0.0
- **Fecha de Compilación**: 11 de Diciembre, 2025
- **Plataforma**: ESP32 (espressif32@5.4.0)
- **Framework**: Arduino ESP32 v2.0.6
- **Archivo**: `SWATID-A2_v3.0.0.bin`
- **SHA256**: `51c136fd184ac9b09eb95f299d6eed4b8858b2950928270cd01d371488caa469`

## 📊 Información Técnica

- **Tamaño del Firmware**: 1,185,949 bytes (1.13 MB)
- **Uso de Flash**: 90.5% de 1,310,720 bytes
- **Uso de RAM**: 15.3% - 50,180 bytes de 327,680 bytes
- **Velocidad de Monitor Serial**: 115200 baud

## 🎯 Cambios Principales en v3.0.0

### 🔧 Corrección del Procesamiento de Mensaje MQTT "granted" (v2.5.4)

Esta versión incluye la corrección crítica del procesamiento de mensajes de validación remota que impedía la apertura del relé cuando el backend aprobaba un acceso.

#### Problemas Corregidos:

1. **Búsqueda Incorrecta de Campos en Respuesta de Validación**
   - ✅ Corregida la búsqueda de campos que estaba mirando en `doc["message_info"]["campo"]` en lugar de `doc["campo"]`
   - ✅ Ahora usa correctamente `relay_number` y `duration` del mensaje recibido
   - ✅ Mejorado el logging para facilitar el debug

2. **Código Duplicado para Procesamiento de `/granted`**
   - ✅ Eliminado el código antiguo que procesaba formato legacy con `access_granted` (boolean)
   - ✅ Mantenido solo el código nuevo que procesa formato moderno con `response` (string)
   - ✅ Resuelto el conflicto que causaba error 7 antes de procesar correctamente el mensaje

3. **Sincronización de message_id**
   - ✅ Corregida la comparación de `message_id` en la respuesta con el enviado originalmente
   - ✅ Ahora valida correctamente que la respuesta corresponde a la solicitud enviada

### 📝 Formato de Mensaje Soportado

```json
{
  "message_id": 17,
  "device": "SWATID_584614BBBC2C",
  "response": "APPROVED",
  "code_type": "PIN",
  "code_value": "333333",
  "duration": 2,
  "relay_number": 1,
  "reason": "Valid code"
}
```

### 🔍 Mejoras en Logging

- Logging detallado del procesamiento de validación remota
- Información clara de los valores extraídos del mensaje
- Mejor identificación de errores en el procesamiento

## 📚 Dependencias

- **ArduinoJson**: v6.21.5
- **PubSubClient**: v2.8.0
- **DNSServer**: v2.0.0
- **EEPROM**: v2.0.0
- **ESPmDNS**: v2.0.0
- **Ethernet**: v2.0.0
- **HTTPClient**: v2.0.0
- **Update**: v2.0.0
- **WebServer**: v2.0.0
- **WiFi**: v2.0.0

## 🔒 Flags de Compilación

```ini
-DCORE_DEBUG_LEVEL=0
-DARDUINO_USB_CDC_ON_BOOT=0
-DCONFIG_ARDUINO_USB_CDC_ON_BOOT=0
-DPLATFORMIO=1
-Os
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
```

## 📖 Documentación Detallada

Para más información sobre los cambios implementados, consulta:
- `/docs/mejoras/correccion-mensaje-granted-v2.5.4.md`

## ⚠️ Notas Importantes

- El firmware está al 90.5% de capacidad de Flash. Considerar optimización para futuras versiones.
- Esta versión corrige un bug crítico que impedía el funcionamiento de la validación remota.
- Se recomienda actualizar todos los dispositivos a esta versión para asegurar el correcto funcionamiento de la validación remota.

## 🚀 Instalación

### Mediante OTA (Over-The-Air)
Configurar el servidor OTA con el manifest correspondiente.

### Mediante Cable USB
```bash
pio run --target upload
```

O especificando el puerto:
```bash
pio run --target upload --upload-port /dev/cu.usbserial-XXXX
```

## 🔗 Enlaces

- [Documentación Completa](/docs/README.md)
- [API MQTT](/docs/api/mqtt-api-reference.md)
- [Procedimientos de Mantenimiento](/docs/processes/maintenance-procedures.md)

