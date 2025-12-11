# Firmware SWATID-A2 v2.5.3 - Gestión de Códigos Remotos vía MQTT + Límites Ampliados

## 🆕 Novedades en v2.5.3

### ✅ Gestión Completa de Códigos Remotos vía MQTT

Esta versión implementa el **soporte completo para gestión de códigos remotos** a través de mensajes MQTT, permitiendo:

- ✅ **Añadir códigos remotos** con franjas horarias
- ✅ **Eliminar códigos remotos** específicos
- ✅ **Limpiar todos los códigos remotos**
- ✅ **Respuestas de confirmación** automáticas vía MQTT
- ✅ **Validación completa** de parámetros y estructura JSON
- ✅ **Soporte para múltiples franjas horarias** (hasta 4 por código)

### 📈 Límites Ampliados de Memoria

- ✅ **100 códigos locales** (ampliado desde 20)
- ✅ **100 códigos remotos** (optimizado desde 500)
- ✅ **EEPROM aumentada a 10KB** para mayor capacidad
- ✅ **Memoria optimizada**: Solo 17.9% de HEAP usado (vs 24.6% anterior)
- ✅ **Ahorro de 21.9KB de RAM** comparado con configuración anterior

---

## 📋 Información del Firmware

- **Versión**: v2.5.3
- **Fecha de compilación**: Octubre 13, 2025
- **Tamaño**: 1.1 MB
- **SHA256**: `566e366d267cc2b3c4172074f0d8aa73859d92c0326f70aafd966367471697a3`
- **Archivo**: `SWATID-A2_v2.5.3_REMOTE_CODES.bin`

---

## 🔧 Instalación

### 📡 Actualización OTA (Vía Web)

1. Accede a la interfaz web de tu controladora
2. Ve a **Configuración OTA** o **Actualización de Firmware**
3. Selecciona el archivo `SWATID-A2_v2.5.3_REMOTE_CODES.bin`
4. Haz clic en **Subir y Actualizar**
5. Espera a que se complete la actualización (aprox. 30-60 segundos)
6. La controladora se reiniciará automáticamente

### 🔌 Actualización por Cable (PlatformIO)

```bash
pio run -t upload
```

---

## 📤 Gestión de Códigos Remotos vía MQTT

### **Tópico**: 
```
swatidhome/command/[SERIAL]/system
```

### **Añadir Código Remoto**

#### Estructura del Mensaje:
```json
{
  "message_id": 200,
  "device": "SWATID_584614BBBC2C",
  "message_type": 5,
  "message_info": {
    "action": "add_remote_code",
    "code_type": "PIN",
    "code_value": "123456",
    "keyboard_id": 0,
    "relay": 1,
    "time_slots": [
      {
        "start_hour": 8,
        "start_minute": 0,
        "end_hour": 15,
        "end_minute": 0,
        "days_of_week": 31
      }
    ]
  }
}
```

#### Parámetros:
- **`code_type`**: `"PIN"` o `"TAG"`
- **`code_value`**: Valor del código (hasta 16 caracteres)
- **`keyboard_id`**: 
  - `0` = Ambos teclados
  - `1` = WIEGAND1
  - `2` = WIEGAND2
- **`relay`**: `1` o `2`
- **`time_slots`**: Array de franjas horarias (máximo 4)
  - `start_hour`: 0-23
  - `start_minute`: 0-59
  - `end_hour`: 0-23
  - `end_minute`: 0-59
  - `days_of_week`: Bitmask
    - 1 = Lunes
    - 2 = Martes
    - 4 = Miércoles
    - 8 = Jueves
    - 16 = Viernes
    - 32 = Sábado
    - 64 = Domingo
    - 127 = Todos los días
    - 31 = Lunes a Viernes (1+2+4+8+16)

#### Ejemplo con mosquitto_pub:
```bash
mosquitto_pub \
  -h 188.245.213.181 \
  -p 1883 \
  -u swatidhome \
  -P 'Swatid2025!' \
  -t 'swatidhome/command/SWATID_584614BBBC2C/system' \
  -m '{
    "message_id": 200,
    "device": "SWATID_584614BBBC2C",
    "message_type": 5,
    "message_info": {
      "action": "add_remote_code",
      "code_type": "PIN",
      "code_value": "123456",
      "keyboard_id": 0,
      "relay": 1,
      "time_slots": [
        {
          "start_hour": 8,
          "start_minute": 0,
          "end_hour": 15,
          "end_minute": 0,
          "days_of_week": 31
        }
      ]
    }
  }'
```

#### Respuesta de la Controladora:
```json
{
  "message_id": 200,
  "device": "SWATID_584614BBBC2C",
  "response": 0,
  "message": "remote code added: PIN 123456"
}
```

---

### **Eliminar Código Remoto Específico**

```json
{
  "message_id": 201,
  "device": "SWATID_584614BBBC2C",
  "message_type": 5,
  "message_info": {
    "action": "remove_remote_code",
    "code_type": "PIN",
    "code_value": "123456"
  }
}
```

#### Respuesta:
```json
{
  "message_id": 201,
  "device": "SWATID_584614BBBC2C",
  "response": 0,
  "message": "remote code removed: PIN 123456"
}
```

---

### **Limpiar Todos los Códigos Remotos**

```json
{
  "message_id": 202,
  "device": "SWATID_584614BBBC2C",
  "message_type": 5,
  "message_info": {
    "action": "clear_remote_codes"
  }
}
```

#### Respuesta:
```json
{
  "message_id": 202,
  "device": "SWATID_584614BBBC2C",
  "response": 0,
  "message": "all remote codes cleared"
}
```

---

## 🐍 Ejemplo en Python

```python
import paho.mqtt.client as mqtt
import json

# Configuración MQTT
broker = "188.245.213.181"
port = 1883
username = "swatidhome"
password = "Swatid2025!"

# Serial de tu controladora
serial = "SWATID_584614BBBC2C"

# Conectar al broker
client = mqtt.Client()
client.username_pw_set(username, password)
client.connect(broker, port, 60)

# Preparar el mensaje para añadir código remoto
topic = f"swatidhome/command/{serial}/system"
message = {
    "message_id": 200,
    "device": serial,
    "message_type": 5,
    "message_info": {
        "action": "add_remote_code",
        "code_type": "PIN",
        "code_value": "123456",
        "keyboard_id": 0,
        "relay": 1,
        "time_slots": [
            {
                "start_hour": 8,
                "start_minute": 0,
                "end_hour": 15,
                "end_minute": 0,
                "days_of_week": 31  # Lunes a Viernes
            }
        ]
    }
}

# Publicar el mensaje
client.publish(topic, json.dumps(message))
print(f"✅ Código remoto 123456 enviado a {serial}")
print(f"📅 Horario: Lunes a Viernes, 08:00 - 15:00")
print(f"🚪 Relé: 1")

client.disconnect()
```

---

## 🔍 Validaciones y Manejo de Errores

### ✅ Validaciones Implementadas

1. **Parámetros obligatorios**: Se verifica que `code_type`, `code_value`, `keyboard_id` y `relay` estén presentes
2. **Límite de códigos**: Se verifica que no se exceda `MAX_REMOTE_CODES` (500 códigos)
3. **Duplicados**: Se evita añadir códigos que ya existan
4. **Franjas horarias**: Se procesan correctamente hasta 4 franjas por código
5. **Formato JSON**: Se valida la estructura completa del mensaje

### ❌ Mensajes de Error

#### Faltan Parámetros:
```json
{
  "response": 1,
  "message": "missing parameters for add_remote_code"
}
```

#### Memoria Llena:
```json
{
  "response": 1,
  "message": "failed to add remote code (may already exist or memory full)"
}
```

#### Código No Encontrado:
```json
{
  "response": 1,
  "message": "remote code not found"
}
```

---

## 📊 Límites y Capacidades

| Característica | Límite | Anterior (v2.5.2) |
|----------------|--------|-------------------|
| **Códigos locales** | **100** | 20 |
| **Códigos remotos** | **100** | 500 (pero causaba problemas de memoria) |
| Franjas horarias por código | 4 | 4 |
| Longitud del código | 16 caracteres | 16 caracteres |
| Teclados soportados | 2 (WIEGAND1 y WIEGAND2) | 2 |
| **EEPROM asignada** | **10KB (10240 bytes)** | 4KB (4096 bytes) |
| **Uso de HEAP** | **17.9%** | 24.6% |

---

## 🔄 Compatibilidad

- ✅ Compatible con todas las versiones anteriores de la plataforma SWATID
- ✅ Mantiene compatibilidad con códigos locales almacenados
- ✅ No afecta la funcionalidad existente de modo torno
- ✅ Integrado con sistema de validación de franjas horarias

---

## 📝 Registro de Cambios (v2.5.3)

### Nuevas Funcionalidades
- ✅ Implementado `case 5` en `processCommand()` para gestión de códigos remotos vía MQTT
- ✅ Procesamiento completo de franjas horarias desde JSON
- ✅ Validación de parámetros obligatorios
- ✅ Respuestas automáticas de confirmación/error
- ✅ Soporte para acciones: `add_remote_code`, `remove_remote_code`, `clear_remote_codes`

### Mejoras de Memoria
- ✅ **MAX_CODES ampliado de 20 a 100** (5x más capacidad)
- ✅ **MAX_REMOTE_CODES optimizado de 500 a 100** (balance memoria/capacidad)
- ✅ **EEPROM aumentada de 4KB a 10KB** (2.5x más espacio)
- ✅ **Layout EEPROM reorganizado**:
  - Config: 0-511 (512 bytes)
  - StoredCodes: 512-3123 (2612 bytes, 100 códigos)
  - Gap: 3124-3199 (76 bytes reservados)
  - StoredRemoteCodes: 3200-9211 (6012 bytes, 100 códigos)
  - Libre: 9212-10239 (1028 bytes disponibles)
- ✅ **Reducción del uso de HEAP**: 80724 bytes → 58804 bytes (ahorro de 21.9KB)
- ✅ **Uso de HEAP optimizado**: 24.6% → 17.9% (mejor rendimiento y estabilidad)

### Mejoras Técnicas
- ✅ Procesamiento eficiente de arrays JSON sin conversiones intermedias
- ✅ Manejo robusto de errores y casos edge
- ✅ Logging detallado para debugging
- ✅ Estructuras de datos optimizadas para menor footprint de memoria

---

## 🛠️ Soporte Técnico

Para más información sobre el protocolo MQTT y otros tipos de mensajes, consulta:
- `/docs/messaging/mqtt-protocol.md`
- `/docs/messaging/message-types.md`
- `/docs/messaging/integration-examples.md`

---

## 📞 Contacto

**SWATID - Sistemas de Control de Acceso**
- 📧 Email: soporte@swatid.com
- 📱 Teléfono: +34 686 103 132
- 🌐 Web: www.swatid.com

---

**¡Disfruta de la nueva funcionalidad de gestión remota de códigos! 🚀**

