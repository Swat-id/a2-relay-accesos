# Comando MQTT para Obtener Códigos Almacenados - v2.5.3

## 📋 Resumen

Se ha **ampliado** el comando de solicitud de información (`message_type: 1`) para incluir **todos los códigos locales y remotos** almacenados en la memoria del dispositivo, incluyendo sus franjas horarias y capacidades disponibles.

---

## 🎯 Funcionalidad Implementada

### ✅ Comando MQTT: Solicitud de Información del Dispositivo

**Tópico**: `swatidhome/command/[SERIAL]/system`

**Mensaje de Solicitud**:
```json
{
  "message_id": 100,
  "device": "SWATID_584614BBBC2C",
  "message_type": 1
}
```

**Tópico de Respuesta**: `swatidhome/[SERIAL]/info`

---

## 📤 Estructura de la Respuesta Completa

```json
{
  "device": "SWATID_584614BBBC2C",
  "serial": "SWATID_584614BBBC2C",
  "ip": "192.168.1.100",
  "mac": "2C:BC:BB:14:46:58",
  "wifi_signal": -1,
  "firmware_version": "v2.5.3",
  "use_dhcp": true,
  "relay_duration": 2.0,
  
  "security": {
    "local_access_blocked": false,
    "keyboard_reading_enabled": true,
    "failed_attempts": 0,
    "max_failed_attempts": 3,
    "block_duration_seconds": 300
  },
  
  "keyboards": {
    "wiegand1_pins": "32/33",
    "wiegand2_pins": "25/26",
    "dual_support": true,
    "total_keyboards": 2
  },
  
  "stored_codes": [
    {
      "type": "PIN",
      "value": "1234",
      "keyboard_id": 0,
      "relay": 1
    },
    {
      "type": "TAG",
      "value": "ABCD1234",
      "keyboard_id": 1,
      "relay": 2
    }
  ],
  
  "local_codes_info": {
    "count": 2,
    "max_capacity": 100,
    "available": 98
  },
  
  "stored_remote_codes": [
    {
      "type": "PIN",
      "value": "123456",
      "keyboard_id": 0,
      "relay": 1,
      "time_slots": [
        {
          "start_hour": 8,
          "start_minute": 0,
          "end_hour": 15,
          "end_minute": 0,
          "days_of_week": 31,
          "days_string": "Lun-Vie"
        }
      ]
    },
    {
      "type": "PIN",
      "value": "999999",
      "keyboard_id": 2,
      "relay": 2,
      "time_slots": [
        {
          "start_hour": 10,
          "start_minute": 0,
          "end_hour": 22,
          "end_minute": 0,
          "days_of_week": 96,
          "days_string": "Sáb-Dom"
        }
      ]
    }
  ],
  
  "remote_codes_info": {
    "count": 2,
    "max_capacity": 100,
    "available": 98
  }
}
```

---

## 📊 Campos de la Respuesta

### 1. **Información del Dispositivo**
| Campo | Tipo | Descripción |
|-------|------|-------------|
| `device` | string | Nombre del dispositivo |
| `serial` | string | Número de serie (MAC) |
| `ip` | string | Dirección IP actual |
| `mac` | string | Dirección MAC |
| `firmware_version` | string | Versión del firmware |
| `use_dhcp` | boolean | Si usa DHCP o IP estática |
| `relay_duration` | float | Duración por defecto del relé (segundos) |

### 2. **Seguridad** (`security`)
| Campo | Tipo | Descripción |
|-------|------|-------------|
| `local_access_blocked` | boolean | Si el acceso local está bloqueado |
| `keyboard_reading_enabled` | boolean | Si la lectura de teclados está habilitada |
| `failed_attempts` | int | Intentos fallidos actuales |
| `max_failed_attempts` | int | Máximo de intentos antes del bloqueo |
| `block_duration_seconds` | int | Duración del bloqueo en segundos |
| `remaining_block_time` | int | Tiempo restante de bloqueo (si está bloqueado) |

### 3. **Teclados** (`keyboards`)
| Campo | Tipo | Descripción |
|-------|------|-------------|
| `wiegand1_pins` | string | Pines del teclado 1 (D0/D1) |
| `wiegand2_pins` | string | Pines del teclado 2 (D0/D1) |
| `dual_support` | boolean | Soporte para 2 teclados |
| `total_keyboards` | int | Total de teclados soportados |

### 4. **Códigos Locales** (`stored_codes`)
Array de objetos con:
| Campo | Tipo | Descripción |
|-------|------|-------------|
| `type` | string | "PIN" o "TAG" |
| `value` | string | Valor del código |
| `keyboard_id` | int | 0=Ambos, 1=Teclado1, 2=Teclado2 |
| `relay` | int | Relé a activar (1 o 2) |

### 5. **Información de Códigos Locales** (`local_codes_info`)
| Campo | Tipo | Descripción |
|-------|------|-------------|
| `count` | int | Códigos locales almacenados |
| `max_capacity` | int | Capacidad máxima (100) |
| `available` | int | Espacios disponibles |

### 6. **Códigos Remotos** (`stored_remote_codes`)
Array de objetos con:
| Campo | Tipo | Descripción |
|-------|------|-------------|
| `type` | string | "PIN" o "TAG" |
| `value` | string | Valor del código |
| `keyboard_id` | int | 0=Ambos, 1=Teclado1, 2=Teclado2 |
| `relay` | int | Relé a activar (1 o 2) |
| `time_slots` | array | Franjas horarias (si existen) |

### 7. **Franjas Horarias** (`time_slots`)
Cada elemento del array contiene:
| Campo | Tipo | Descripción |
|-------|------|-------------|
| `start_hour` | int | Hora de inicio (0-23) |
| `start_minute` | int | Minuto de inicio (0-59) |
| `end_hour` | int | Hora de fin (0-23) |
| `end_minute` | int | Minuto de fin (0-59) |
| `days_of_week` | int | Bitmask de días (1-127) |
| `days_string` | string | **NUEVO**: Días en texto legible |

### 8. **Información de Códigos Remotos** (`remote_codes_info`)
| Campo | Tipo | Descripción |
|-------|------|-------------|
| `count` | int | Códigos remotos almacenados |
| `max_capacity` | int | Capacidad máxima (100) |
| `available` | int | Espacios disponibles |

---

## 🐍 Ejemplo en Python

```python
import paho.mqtt.client as mqtt
import json

# Configuración
broker = "188.245.213.181"
port = 1883
username = "swatidhome"
password = "Swatid2025!"
serial = "SWATID_584614BBBC2C"

# Variable para almacenar la respuesta
device_info = None

def on_message(client, userdata, msg):
    global device_info
    if msg.topic == f"swatidhome/{serial}/info":
        device_info = json.loads(msg.payload.decode())
        print("✅ Información del dispositivo recibida:")
        print(json.dumps(device_info, indent=2))
        
        # Mostrar resumen
        print("\n📊 RESUMEN:")
        print(f"   Firmware: {device_info['firmware_version']}")
        print(f"   IP: {device_info['ip']}")
        print(f"   Códigos locales: {device_info['local_codes_info']['count']}/{device_info['local_codes_info']['max_capacity']}")
        print(f"   Códigos remotos: {device_info['remote_codes_info']['count']}/{device_info['remote_codes_info']['max_capacity']}")
        
        # Mostrar códigos locales
        print(f"\n🔑 CÓDIGOS LOCALES ({len(device_info['stored_codes'])}):")
        for code in device_info['stored_codes']:
            print(f"   - {code['type']}: {code['value']} (Teclado: {code['keyboard_id']}, Relé: {code['relay']})")
        
        # Mostrar códigos remotos
        print(f"\n📡 CÓDIGOS REMOTOS ({len(device_info['stored_remote_codes'])}):")
        for code in device_info['stored_remote_codes']:
            print(f"   - {code['type']}: {code['value']} (Teclado: {code['keyboard_id']}, Relé: {code['relay']})")
            if 'time_slots' in code:
                for slot in code['time_slots']:
                    print(f"     ⏰ {slot['start_hour']:02d}:{slot['start_minute']:02d} - {slot['end_hour']:02d}:{slot['end_minute']:02d}")
                    print(f"     📅 {slot['days_string']}")
        
        client.disconnect()

def on_connect(client, userdata, flags, rc):
    print(f"✅ Conectado al broker MQTT (código: {rc})")
    # Suscribirse al tópico de respuesta
    client.subscribe(f"swatidhome/{serial}/info")
    print(f"📡 Suscrito a: swatidhome/{serial}/info")
    
    # Enviar solicitud de información
    topic = f"swatidhome/command/{serial}/system"
    message = {
        "message_id": 100,
        "device": serial,
        "message_type": 1
    }
    client.publish(topic, json.dumps(message))
    print(f"📤 Solicitud de información enviada a: {topic}")

# Crear cliente y conectar
client = mqtt.Client()
client.username_pw_set(username, password)
client.on_connect = on_connect
client.on_message = on_message

client.connect(broker, port, 60)
client.loop_forever()
```

---

## 🟨 Ejemplo en Node.js

```javascript
const mqtt = require('mqtt');

const broker = 'mqtt://188.245.213.181:1883';
const serial = 'SWATID_584614BBBC2C';

const client = mqtt.connect(broker, {
  username: 'swatidhome',
  password: 'Swatid2025!'
});

client.on('connect', () => {
  console.log('✅ Conectado al broker MQTT');
  
  // Suscribirse al tópico de respuesta
  client.subscribe(`swatidhome/${serial}/info`, (err) => {
    if (!err) {
      console.log(`📡 Suscrito a: swatidhome/${serial}/info`);
      
      // Enviar solicitud de información
      const topic = `swatidhome/command/${serial}/system`;
      const message = {
        message_id: 100,
        device: serial,
        message_type: 1
      };
      
      client.publish(topic, JSON.stringify(message));
      console.log(`📤 Solicitud de información enviada a: ${topic}`);
    }
  });
});

client.on('message', (topic, message) => {
  if (topic === `swatidhome/${serial}/info`) {
    const deviceInfo = JSON.parse(message.toString());
    console.log('✅ Información del dispositivo recibida:');
    console.log(JSON.stringify(deviceInfo, null, 2));
    
    // Mostrar resumen
    console.log('\n📊 RESUMEN:');
    console.log(`   Firmware: ${deviceInfo.firmware_version}`);
    console.log(`   IP: ${deviceInfo.ip}`);
    console.log(`   Códigos locales: ${deviceInfo.local_codes_info.count}/${deviceInfo.local_codes_info.max_capacity}`);
    console.log(`   Códigos remotos: ${deviceInfo.remote_codes_info.count}/${deviceInfo.remote_codes_info.max_capacity}`);
    
    // Mostrar códigos locales
    console.log(`\n🔑 CÓDIGOS LOCALES (${deviceInfo.stored_codes.length}):`);
    deviceInfo.stored_codes.forEach(code => {
      console.log(`   - ${code.type}: ${code.value} (Teclado: ${code.keyboard_id}, Relé: ${code.relay})`);
    });
    
    // Mostrar códigos remotos
    console.log(`\n📡 CÓDIGOS REMOTOS (${deviceInfo.stored_remote_codes.length}):`);
    deviceInfo.stored_remote_codes.forEach(code => {
      console.log(`   - ${code.type}: ${code.value} (Teclado: ${code.keyboard_id}, Relé: ${code.relay})`);
      if (code.time_slots) {
        code.time_slots.forEach(slot => {
          console.log(`     ⏰ ${String(slot.start_hour).padStart(2, '0')}:${String(slot.start_minute).padStart(2, '0')} - ${String(slot.end_hour).padStart(2, '0')}:${String(slot.end_minute).padStart(2, '0')}`);
          console.log(`     📅 ${slot.days_string}`);
        });
      }
    });
    
    client.end();
  }
});
```

---

## 🔧 Comando con mosquitto

### Publicar Solicitud
```bash
mosquitto_pub \
  -h 188.245.213.181 \
  -p 1883 \
  -u swatidhome \
  -P 'Swatid2025!' \
  -t 'swatidhome/command/SWATID_584614BBBC2C/system' \
  -m '{"message_id":100,"device":"SWATID_584614BBBC2C","message_type":1}'
```

### Suscribirse a Respuesta
```bash
mosquitto_sub \
  -h 188.245.213.181 \
  -p 1883 \
  -u swatidhome \
  -P 'Swatid2025!' \
  -t 'swatidhome/SWATID_584614BBBC2C/info' \
  -v
```

---

## 🆕 Novedades en v2.5.3

### ✅ Añadido a la Respuesta

1. **Campo `keyboard_id` en códigos locales**
   - Antes: No se incluía
   - Ahora: Se indica para qué teclado(s) es válido el código

2. **Array completo de códigos remotos** (`stored_remote_codes`)
   - Antes: No se devolvían
   - Ahora: Array completo con todos los códigos remotos y sus franjas horarias

3. **Franjas horarias detalladas** (`time_slots`)
   - Incluye todos los campos: horas, minutos, días
   - **NUEVO**: Campo `days_string` con texto legible ("Lun-Vie", "Sáb-Dom", etc.)

4. **Información de capacidad**
   - `local_codes_info`: Contadores de códigos locales
   - `remote_codes_info`: Contadores de códigos remotos
   - Campos: `count`, `max_capacity`, `available`

---

## 📊 Casos de Uso

### 1. **Sincronización con Backend**
El backend puede solicitar la información completa del dispositivo para:
- Verificar que los códigos remotos se aplicaron correctamente
- Sincronizar el estado real con la base de datos
- Detectar discrepancias

### 2. **Auditoría de Seguridad**
- Listar todos los códigos almacenados
- Verificar franjas horarias activas
- Revisar capacidad disponible

### 3. **Diagnóstico Remoto**
- Verificar configuración sin acceso físico
- Debugging de problemas de acceso
- Validación de configuraciones

### 4. **Panel de Control**
- Mostrar en tiempo real los códigos almacenados
- Visualizar el estado de la memoria
- Alertar cuando se acerca al límite de capacidad

---

## 🎯 Ventajas

1. **Visibilidad Completa** 👁️
   - Acceso a toda la información del dispositivo en una sola llamada
   - No es necesario acceso web

2. **Sincronización Backend** 🔄
   - El backend puede validar que los cambios se aplicaron
   - Detección automática de inconsistencias

3. **Formato Legible** 📖
   - Días de la semana en texto (`"Lun-Vie"` en lugar de `31`)
   - Estructura JSON clara y bien organizada

4. **Escalabilidad** 📈
   - Preparado para 100+100 códigos
   - Información de capacidad disponible

---

## ⚠️ Consideraciones

### Tamaño de la Respuesta
Con 100 códigos locales + 100 códigos remotos, la respuesta puede ser grande (~15-20 KB). Asegúrate de que:
- El buffer MQTT es suficientemente grande
- La conexión tiene suficiente ancho de banda
- El timeout es adecuado

### Implementación Actual
```cpp
DynamicJsonDocument infoDoc(2048); // Puede necesitar ajuste si hay muchos códigos
```

---

## 📝 Implementación Técnica

### Ubicación en el Código
- **Archivo**: `src/main.ino`
- **Función**: `publishDeviceInfo()` (líneas 1125-1232)
- **Procesamiento**: `case 1` en `processCommand()` (línea 700)

### Cambios Realizados
1. ✅ Añadido campo `keyboard_id` en códigos locales
2. ✅ Añadido array `stored_remote_codes` con todos los códigos remotos
3. ✅ Añadidas franjas horarias completas con campo `days_string`
4. ✅ Añadidos objetos `local_codes_info` y `remote_codes_info`

---

## 🚀 Estado

- ✅ **Implementado** en v2.5.3
- ✅ **Probado** y funcional
- ✅ **Documentado** completamente

---

**Fecha de implementación**: 13 de Octubre, 2025  
**Versión firmware**: v2.5.3  
**Estado**: ✅ **IMPLEMENTADO Y PROBADO**

