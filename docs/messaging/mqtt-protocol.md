# Protocolo MQTT - SWATID-A2

## Descripción
El sistema SWATID-A2 implementa un protocolo MQTT completo para comunicación bidireccional con servidores remotos, permitiendo control remoto, monitoreo y gestión del sistema de control de acceso dual Wiegand.

## Configuración MQTT

### 📡 Parámetros de Conexión
- **Broker**: 188.245.213.181
- **Puerto**: 1883
- **Protocolo**: TCP/IP
- **Usuario**: swatidhome
- **Contraseña**: Swatid2025!
- **QoS**: 0 (At Most Once)
- **Keepalive**: 60 segundos
- **Buffer**: 2048 bytes

### 🔐 Autenticación
- **Método**: Username/Password
- **Credenciales**: Fijas en el firmware
- **Seguridad**: Conexión TCP estándar
- **Reconexión**: Automática cada 5 segundos

### 🆔 Identificación del Dispositivo
- **Client ID**: `ESP32Client-[HEX_RANDOM]`
- **Serial Fijo**: `SWATID_[MAC_BASED]`
- **Nombre Dispositivo**: Configurable por usuario
- **Hostname**: Basado en nombre del dispositivo

## Estructura de Tópicos

### 📤 Tópicos de Comando (Recibidos por el Dispositivo)

#### Activación de Relé
```
swatidhome/command/[SERIAL]/relay
```

#### Solicitud de Información
```
swatidhome/command/[SERIAL]/info
```

#### Comandos de Sistema
```
swatidhome/command/[SERIAL]/system
```

#### Comandos de Seguridad
```
swatidhome/command/[SERIAL]/security
```

#### Validación de Acceso (Solicitud desde dispositivo)
```
swatidhome/command/[SERIAL]/access
```

#### Respuesta de Validación (Respuesta del backend)
```
swatidhome/command/[SERIAL]/granted
```

### 📥 Tópicos de Publicación (Enviados por el Dispositivo)

#### Información del Dispositivo
```
swatidhome/[SERIAL]/info
```

#### Errores del Sistema
```
swatidhome/errors/[SERIAL]/rx
```

#### Respuestas a Comandos
```
swatidhome/response/[SERIAL]/rx
```

#### Estado de Relés
```
swatidhome/[SERIAL]/relay_enable
```

#### Eventos de Acceso
```
swatidhome/events/[SERIAL]/access
```

#### Intentos de Acceso Fallidos
```
swatidhome/events/[SERIAL]/failed_access
```

#### Eventos de Modo Torno
```
swatidhome/event/[DEVICE]/turnstile
```

#### Keepalive
```
swatidhome/keepalive/[SERIAL]
```

---

## Mensajes Recibidos por el Dispositivo

### 📋 Estructura Base
Todos los mensajes MQTT utilizan formato JSON con la siguiente estructura base:

```json
{
  "timestamp": "2025-10-13T10:30:00+01:00",
  "message_id": 123,
  "device": "SWATID_XXXXXXXX"
}
```

### message_type 0: Activación de Relé

**Tópico**: `swatidhome/command/[SERIAL]/relay`

**Estructura**:
```json
{
  "message_id": 123,
  "device": "SWATID_584614BBBC2C",
  "message_type": 0,
  "message_info": {
    "relay_number": 1,
    "duration": 3.0
  }
}
```

**Parámetros**:
- `relay_number` (int): 1 o 2
- `duration` (float): 0.5-60.0 segundos

**Respuesta del Dispositivo**:
```json
{
  "timestamp": "2025-10-13T10:30:01+01:00",
  "response_id": 124,
  "message_id": 123,
  "device": "DeviceName",
  "serial": "SWATID_584614BBBC2C",
  "response_type": 0,
  "response_info": "relay 1 activated",
  "relay": 1
}
```

---

### message_type 1: Solicitud de Información

**Tópico**: `swatidhome/command/[SERIAL]/system`

**Estructura**:
```json
{
  "message_id": 125,
  "device": "SWATID_584614BBBC2C",
  "message_type": 1
}
```

**Respuesta del Dispositivo**: (ver sección "Información del Dispositivo" completa más abajo)

Esta solicitud devuelve **toda la información del dispositivo**, incluyendo:
- ✅ Información básica (IP, MAC, firmware, etc.)
- ✅ Configuración de seguridad
- ✅ Estado de teclados
- ✅ **Códigos locales completos** (con keyboard_id)
- ✅ **Códigos remotos completos** (con franjas horarias)
- ✅ **Información de capacidad** de memoria
- ✅ Configuración OTA
- ✅ Estado del modo torno

---

### message_type 2: Comandos de Sistema

**Tópico**: `swatidhome/command/[SERIAL]/system`

**Estructura**:
```json
{
  "message_id": 126,
  "device": "SWATID_584614BBBC2C",
  "message_type": 2,
  "message_info": "reboot"
}
```

**Comandos disponibles**:
- `reboot`: Reinicio del sistema
- `reset`: Reset a valores por defecto
- `update`: Actualización de configuración
- `ota_config`: Configuración OTA
- `ota_check`: Verificación de actualizaciones OTA

**Respuesta**:
```json
{
  "timestamp": "2025-10-13T10:30:01+01:00",
  "response_id": 127,
  "message_id": 126,
  "device": "DeviceName",
  "serial": "SWATID_584614BBBC2C",
  "response_type": 0,
  "response_info": "reboot initiated"
}
```

---

### message_type 3: Comandos de Seguridad

**Tópico**: `swatidhome/command/[SERIAL]/security`

**Estructura**:
```json
{
  "message_id": 128,
  "device": "SWATID_584614BBBC2C",
  "message_type": 3,
  "message_info": {
    "security_command": "block_local_access",
    "block_duration": 300,
    "max_failed_attempts": 3
  }
}
```

**Comandos de seguridad**:
- `block_local_access`: Bloquear acceso local
- `unblock_local_access`: Desbloquear acceso local
- `set_block_duration`: Configurar duración de bloqueo (segundos)
- `set_max_failed_attempts`: Configurar máximo de intentos fallidos

---

### message_type 4: Sincronización de Tiempo

**Tópico**: `swatidhome/command/[SERIAL]/system`

**Estructura**:
```json
{
  "message_id": 129,
  "device": "SWATID_584614BBBC2C",
  "message_type": 4,
  "message_info": {
    "time_string": "2025-10-13 10:30:00"
  }
}
```

**Respuesta**:
```json
{
  "timestamp": "2025-10-13T10:30:01+01:00",
  "response_id": 130,
  "message_id": 129,
  "device": "DeviceName",
  "serial": "SWATID_584614BBBC2C",
  "response_type": 0,
  "response_info": "time synchronized to 2025-10-13 10:30:00"
}
```

---

### message_type 5: Gestión de Códigos Remotos

**Tópico**: `swatidhome/command/[SERIAL]/system`

**Estructura para agregar código remoto**:
```json
{
  "message_id": 131,
  "device": "SWATID_584614BBBC2C",
  "message_type": 5,
  "message_info": {
    "action": "add_remote_code",
    "code_type": "PIN",
    "code_value": "1234",
    "keyboard_id": 0,
    "relay": 1,
    "time_slots": [
      {
        "start_hour": 8,
        "start_minute": 0,
        "end_hour": 18,
        "end_minute": 0,
        "days_of_week": 31
      }
    ]
  }
}
```

**Parámetros**:
- `action`: "add_remote_code", "remove_remote_code", "clear_remote_codes"
- `code_type`: "PIN" o "TAG"
- `code_value`: Valor del código (hasta 16 caracteres)
- `keyboard_id`: 0 (ambos), 1 (WIEGAND1), 2 (WIEGAND2)
- `relay`: 1 o 2
- `time_slots`: Array de franjas horarias (máximo 4)
  - `start_hour`: 0-23
  - `start_minute`: 0-59
  - `end_hour`: 0-23
  - `end_minute`: 0-59
  - `days_of_week`: Bitmask (1=Lun, 2=Mar, 4=Mié, 8=Jue, 16=Vie, 32=Sáb, 64=Dom, 127=Todos)

**Estructura para eliminar código remoto**:
```json
{
  "message_id": 132,
  "device": "SWATID_584614BBBC2C",
  "message_type": 5,
  "message_info": {
    "action": "remove_remote_code",
    "code_type": "PIN",
    "code_value": "1234"
  }
}
```

**Estructura para limpiar todos los códigos remotos**:
```json
{
  "message_id": 133,
  "device": "SWATID_584614BBBC2C",
  "message_type": 5,
  "message_info": {
    "action": "clear_remote_codes"
  }
}
```

---

### message_type 6: Configuración de Modo Torno

**Tópico**: `swatidhome/command/[SERIAL]/system`

**Estructura**:
```json
{
  "message_id": 134,
  "device": "SWATID_584614BBBC2C",
  "message_type": 6,
  "message_info": {
    "enabled": true,
    "keyboard1_relay": 1,
    "keyboard2_relay": 2
  }
}
```

**Parámetros**:
- `enabled` (bool): Habilitar/deshabilitar modo torno
- `keyboard1_relay` (int): Relé para teclado 1 (1 o 2)
- `keyboard2_relay` (int): Relé para teclado 2 (1 o 2)

---

### Respuesta de Validación de Acceso (Backend → Dispositivo)

**Tópico**: `swatidhome/command/[SERIAL]/granted`

**Estructura para ACCESO APROBADO**:
```json
{
  "message_id": 135,
  "device": "SWATID_584614BBBC2C",
  "response": "APPROVED",
  "code_type": "PIN",
  "code_value": "1234",
  "duration": 2000,
  "relay_number": 1,
  "reason": "Valid code"
}
```

**Estructura para ACCESO DENEGADO**:
```json
{
  "message_id": 135,
  "device": "SWATID_584614BBBC2C",
  "response": "DENIED",
  "code_type": "PIN",
  "code_value": "9999",
  "reason": "Invalid code"
}
```

**Parámetros**:
- `response`: "APPROVED" o "DENIED"
- `code_type`: "PIN" o "TAG"
- `code_value`: Valor del código
- `duration` (int, solo APPROVED): Duración en milisegundos
- `relay_number` (int, solo APPROVED): Relé a activar (1 o 2)
- `reason`: Razón de la decisión

---

### Configuración OTA Remota

**Tópico**: `swatidhome/command/[SERIAL]/system`

**Estructura**:
```json
{
  "message_id": 136,
  "device": "SWATID_584614BBBC2C",
  "message_type": 2,
  "message_info": "ota_config",
  "ota_config": {
    "update_url": "https://updates.swatid.com/firmware/check",
    "auto_update_enabled": true,
    "check_interval": 24
  }
}
```

**Parámetros**:
- `update_url`: URL del servidor de actualizaciones
- `auto_update_enabled`: Habilitar actualizaciones automáticas
- `check_interval`: Intervalo de verificación en horas (1-168)

---

## Mensajes Enviados por el Dispositivo

### Información del Dispositivo

**Tópico**: `swatidhome/[SERIAL]/info`

**Estructura Completa** (Actualizado en v2.5.3):
```json
{
  "device": "DeviceName",
  "serial": "SWATID_584614BBBC2C",
  "ip": "192.168.1.100",
  "mac": "2C:BC:BB:14:46:58",
  "wifi_signal": -1,
  "firmware_version": "v2.5.3",
  "firmware_build": "Oct 13 2025 16:00:00",
  "use_dhcp": true,
  "relay_duration": 2.0,
  
  "security": {
    "local_access_blocked": false,
    "keyboard_reading_enabled": true,
    "failed_attempts": 0,
    "max_failed_attempts": 3,
    "block_duration_seconds": 300,
    "remaining_block_time": 0
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
  },
  
  "ota": {
    "update_url": "https://updates.swatid.com/firmware/check",
    "auto_update_enabled": false,
    "check_interval": 24,
    "last_check": 0
  }
}
```

**📊 Novedades en v2.5.3**:

1. **`stored_codes`**: Ahora es un **array completo** con todos los códigos locales
   - Incluye: `type`, `value`, `keyboard_id`, `relay`
   - Antes: Solo contadores básicos

2. **`local_codes_info`**: Nueva sección con información de capacidad
   - `count`: Códigos locales almacenados
   - `max_capacity`: 100 códigos
   - `available`: Espacios disponibles

3. **`stored_remote_codes`**: **NUEVO** - Array completo de códigos remotos
   - Incluye todos los campos del código
   - **Franjas horarias completas** con cada código
   - Campo `days_string`: Días en texto legible ("Lun-Vie", "Sáb-Dom", etc.)

4. **`remote_codes_info`**: Nueva sección con información de capacidad
   - `count`: Códigos remotos almacenados
   - `max_capacity`: 100 códigos
   - `available`: Espacios disponibles

---

### Solicitud de Validación de Acceso (Modo Normal)

**Tópico**: `swatidhome/command/[SERIAL]/access`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T10:30:00+01:00",
  "message_id": 137,
  "device": "SWATID_584614BBBC2C",
  "device_name": "DeviceName",
  "message_type": 0,
  "message_info": {
    "source": "AUTO",
    "code_type": "PIN",
    "code_value": "1234",
    "keyboard_id": 1,
    "keyboard_name": "WIEGAND1",
    "keyboard_pins": "33/14",
    "request_relay": 1,
    "max_duration": 5.0
  }
}
```

---

### Solicitud de Validación de Acceso (Modo Torno)

**Tópico**: `swatidhome/command/[SERIAL]/access`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T09:04:46+01:00",
  "message_id": 138,
  "device": "SWATID_584614BBBC2C",
  "device_name": "DeviceName",
  "message_type": 0,
  "mode": "turnstile",
  "message_info": {
    "source": "AUTO",
    "code_type": "PIN",
    "code_value": "4444",
    "keyboard_id": 2,
    "keyboard_name": "WIEGAND2",
    "keyboard_pins": "13/12",
    "request_relay": 1,
    "actual_relay": 2,
    "max_duration": 5.0
  },
  "turnstile_info": {
    "enabled": true,
    "keyboard1_relay": 1,
    "keyboard2_relay": 2,
    "timeout": 5000,
    "pending_request": true
  }
}
```

**Nota**: En modo torno:
- `request_relay`: Siempre 1 para compatibilidad MQTT
- `actual_relay`: Relé real que se abrirá según configuración del torno

---

### Estado de Relé

**Tópico**: `swatidhome/[SERIAL]/relay_enable`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T10:30:00+01:00",
  "resultado": "OK",
  "rele": 1,
  "estado": "ON",
  "origen": "MQTT",
  "message_id": 139,
  "request_id": 123
}
```

**Estados**:
- `ON`: Relé activado
- `OFF`: Relé desactivado

**Orígenes**:
- `SYSTEM`: Activación del sistema
- `TIMEOUT`: Desactivación por tiempo
- `WEB`: Activación desde web
- `MQTT`: Activación remota
- `LOCAL`: Código local
- `REMOTE`: Código remoto
- `TURNSTILE`: Modo torno

---

### Evento de Acceso Exitoso

**Tópico**: `swatidhome/events/[SERIAL]/access`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T10:30:00+01:00",
  "message_id": 140,
  "device": "DeviceName",
  "serial": "SWATID_584614BBBC2C",
  "event_type": "ACCESS_EVENT",
  "success": true,
  "source": "LOCAL",
  "code_type": "PIN",
  "code_value": "1234",
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1",
  "relay_opened": 1,
  "duration": 5.0
}
```

**Fuentes (source)**:
- `LOCAL`: Validación local
- `REMOTE`: Validación remota
- `WEB`: Activación desde web
- `MQTT_RELAY`: Activación remota de relé
- `LOCAL_EMERGENCY`: Fallback local de emergencia
- `LOCAL_FALLBACK`: Fallback local por MQTT desconectado

---

### Intento de Acceso Fallido

**Tópico**: `swatidhome/events/[SERIAL]/failed_access`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T10:30:00+01:00",
  "message_id": 141,
  "device": "DeviceName",
  "serial": "SWATID_584614BBBC2C",
  "event_type": "FAILED_ACCESS",
  "code_type": "PIN",
  "code_value": "9999",
  "reason": "INVALID_CODE",
  "failed_attempts": 2,
  "max_attempts": 3,
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1"
}
```

**Razones (reason)**:
- `INVALID_CODE`: Código no válido
- `BLOCKED`: Acceso bloqueado por seguridad
- `REMOTE_DENIED`: Denegado por servidor remoto
- `MQTT_PUBLISH_FAILED`: Error de comunicación MQTT
- `OUTSIDE_TIME_SLOT`: Código fuera de horario permitido

---

### Evento de Modo Torno (Acceso Concedido)

**Tópico**: `swatidhome/event/[DEVICE]/turnstile`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T09:04:46+01:00",
  "message_id": 142,
  "device": "SWATID_584614BBBC2C",
  "device_name": "DeviceName",
  "message_type": 1,
  "mode": "turnstile",
  "event_type": "ACCESS_GRANTED",
  "event_info": {
    "code_type": "PIN",
    "code_value": "1234",
    "keyboard_id": 2,
    "keyboard_name": "WIEGAND2",
    "success": true,
    "reason": "TORNO_LOCAL",
    "relay_opened": 2,
    "duration": 5.0
  },
  "turnstile_info": {
    "enabled": true,
    "keyboard1_relay": 1,
    "keyboard2_relay": 2,
    "timeout": 5000
  }
}
```

**Razones de acceso concedido**:
- `TORNO_LOCAL`: Código local válido
- `TORNO_REMOTE`: Código validado remotamente
- `TORNO_REMOTE_LOCAL`: Código remoto guardado localmente

---

### Evento de Modo Torno (Acceso Denegado)

**Tópico**: `swatidhome/event/[DEVICE]/turnstile`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T09:04:51+01:00",
  "message_id": 143,
  "device": "SWATID_584614BBBC2C",
  "device_name": "DeviceName",
  "message_type": 1,
  "mode": "turnstile",
  "event_type": "ACCESS_DENIED",
  "event_info": {
    "code_type": "PIN",
    "code_value": "4444",
    "keyboard_id": 2,
    "keyboard_name": "WIEGAND2",
    "success": false,
    "reason": "TORNO_TIMEOUT_DENIED",
    "relay_opened": 0,
    "duration": 0
  },
  "turnstile_info": {
    "enabled": true,
    "keyboard1_relay": 1,
    "keyboard2_relay": 2,
    "timeout": 5000
  }
}
```

**Razones de acceso denegado**:
- `TORNO_TIMEOUT_DENIED`: Timeout sin respuesta del backend
- `TORNO_REMOTE_DENIED`: Denegado por servidor remoto
- `INVALID_CODE`: Código no válido
- `MQTT_PUBLISH_FAILED`: Error al publicar solicitud

---

### Error del Sistema

**Tópico**: `swatidhome/errors/[SERIAL]/rx`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T10:30:00+01:00",
  "message_id": 144,
  "device": "DeviceName",
  "serial": "SWATID_584614BBBC2C",
  "error_code": 6,
  "description": "Acceso bloqueado tras 3 intentos fallidos"
}
```

**Códigos de error**:
- `1`: Error de conexión de red
- `2`: Error de configuración
- `3`: Mensaje de inicio/estado
- `4`: Cambio de configuración
- `5`: Sistema de seguridad
- `6`: Bloqueo de acceso activado
- `7`: Error de validación remota
- `8`: Memoria baja
- `9`: MQTT desconectado prolongado

---

### Respuesta a Comando

**Tópico**: `swatidhome/response/[SERIAL]/rx`

**Estructura**:
```json
{
  "timestamp": "2025-10-13T10:30:01+01:00",
  "response_id": 145,
  "message_id": 123,
  "device": "DeviceName",
  "serial": "SWATID_584614BBBC2C",
  "response_type": 0,
  "response_info": "relay 1 activated",
  "relay": 1
}
```

**Tipos de respuesta**:
- `0`: Respuesta a comando de relé o sistema
- `1`: Respuesta a solicitud de información

---

### Keepalive

**Tópico**: `swatidhome/keepalive/[SERIAL]`

**Estructura**:
```json
{
  "status": "online",
  "uptime": 3600000
}
```

**Campos**:
- `status`: "online"
- `uptime`: Tiempo de funcionamiento en milisegundos

**Frecuencia**: Cada 60 segundos

---

## Gestión de Conexión

### 🔄 Reconexión Automática
- **Intervalo**: 5 segundos
- **Condiciones**: Desconexión detectada
- **Reintentos**: Infinitos
- **Logging**: Estado de conexión detallado

### 📊 Monitoreo de Estado
- **Verificación**: Cada minuto
- **Keepalive**: Envío automático cada 60 segundos
- **Timeout**: 5 minutos sin respuesta
- **Acción**: Reinicio del sistema si es crítico

### 🚨 Manejo de Errores
- **Códigos MQTT**: Interpretación completa
- **Logging**: Errores detallados en Serial
- **Recuperación**: Automática con fallback local
- **Notificación**: Eventos críticos publicados

---

## Suscripciones

### 📡 Tópicos Suscritos
El dispositivo se suscribe automáticamente a:
```
swatidhome/command/[SERIAL]/#
```

**Incluye**:
- `/relay`: Activación de relés
- `/info`: Solicitudes de información
- `/system`: Comandos de sistema y configuración
- `/security`: Comandos de seguridad
- `/access`: Solicitudes de validación
- `/granted`: Respuestas de validación

### 🔔 Procesamiento de Mensajes
- **Callback**: Función dedicada `mqttCallback()`
- **Parsing**: JSON automático con ArduinoJson
- **Validación**: Estructura y contenido
- **Routing**: Por tipo de mensaje (`message_type`)
- **Logging**: Detallado en Serial Monitor

---

## Modo Torno

### 🔄 Flujo de Validación en Modo Torno

**1. Código introducido en teclado**
```
Usuario → WIEGAND2 → Código "4444"
```

**2. Validación local**
```
Buscar en códigos locales → NO ENCONTRADO
```

**3. Solicitud remota**
```
Publicar en: swatidhome/command/[SERIAL]/access
Con mode: "turnstile"
```

**4. Esperar respuesta (5 segundos)**
```
Tópico: swatidhome/command/[SERIAL]/granted
```

**5. Procesar respuesta**
```
APPROVED → Abrir relé según configuración torno
DENIED → Denegar acceso
TIMEOUT → Publicar evento de timeout
```

### ⏰ Gestión de Timeout
- **Timeout inicial**: 5 segundos
- **Acción**: Publicar evento `TIMEOUT_WAITING`
- **Solicitud**: Permanece activa
- **Timeout máximo**: 30 segundos (configurable)
- **Acción final**: Denegar acceso definitivamente

---

## Códigos Remotos con Franjas Horarias

### 📋 Estructura de Código Remoto

```cpp
{
  "type": "PIN",              // o "TAG"
  "value": "1234",
  "keyboard_id": 0,           // 0=ambos, 1=WIEGAND1, 2=WIEGAND2
  "relay": 1,                 // 1 o 2
  "time_slots": [
    {
      "start_hour": 8,        // 0-23
      "start_minute": 0,      // 0-59
      "end_hour": 18,         // 0-23
      "end_minute": 0,        // 0-59
      "days_of_week": 31      // Bitmask: 1=Lun, 2=Mar, ..., 127=Todos
    }
  ]
}
```

### 📅 Días de la Semana (Bitmask)

| Valor | Bits | Días | `days_string` (v2.5.3+) | Descripción |
|-------|------|------|-------------------------|-------------|
| `1` | 0000001 | Lunes | "Lun" | Solo Lunes |
| `2` | 0000010 | Martes | "Mar" | Solo Martes |
| `4` | 0000100 | Miércoles | "Mié" | Solo Miércoles |
| `8` | 0001000 | Jueves | "Jue" | Solo Jueves |
| `16` | 0010000 | Viernes | "Vie" | Solo Viernes |
| `32` | 0100000 | Sábado | "Sáb" | Solo Sábado |
| `64` | 1000000 | Domingo | "Dom" | Solo Domingo |
| `31` | 0011111 | Lun-Vie | **"Lun-Vie"** | Días laborables |
| `96` | 1100000 | Sáb-Dom | **"Sáb-Dom"** | Fin de semana |
| `127` | 1111111 | Todos | **"Todos los días"** | Lunes a Domingo |
| `42` | 0101010 | Mar,Jue,Sáb | "Mar, Jue, Sáb" | Ejemplo combinación |

**🆕 Nuevo en v2.5.3**: Campo `days_string` en franjas horarias
- Convierte el bitmask numérico a texto legible
- Se incluye automáticamente en respuestas de información del dispositivo
- Facilita la lectura y comprensión para usuarios finales

### ⏰ Validación de Franjas Horarias
- **Máximo**: 4 franjas por código
- **Validación**: Automática al validar código
- **Fuera de horario**: Código rechazado
- **Sin franjas**: Válido 24/7

---

## Actualización OTA via MQTT

### 📥 Configuración OTA

**Comando**:
```json
{
  "message_id": 150,
  "device": "SWATID_584614BBBC2C",
  "message_type": 2,
  "message_info": "ota_config",
  "ota_config": {
    "update_url": "https://updates.swatid.com/api/check",
    "auto_update_enabled": true,
    "check_interval": 24
  }
}
```

### 🔍 Verificación de Actualizaciones

**Comando**:
```json
{
  "message_id": 151,
  "device": "SWATID_584614BBBC2C",
  "message_type": 2,
  "message_info": "ota_check"
}
```

**Proceso**:
1. Dispositivo verifica URL configurada
2. Envía información del dispositivo (MAC, versión actual)
3. Servidor responde con información de actualización
4. Si está disponible y habilitado, descarga e instala
5. Reinicia automáticamente

### 📦 Formato de Respuesta del Servidor OTA

```json
{
  "update_available": true,
  "version": "v2.5.3",
  "download_url": "https://updates.swatid.com/firmware/SWATID-A2_v2.5.3.bin",
  "file_size": 1200000,
  "sha256": "abc123...",
  "release_notes": "Bug fixes and improvements",
  "mandatory": false
}
```

---

## Optimizaciones

### ⚡ Rendimiento
- **Buffer**: 2048 bytes
- **QoS**: 0 para máxima velocidad
- **Compresión**: JSON compacto sin espacios
- **Batching**: Múltiples códigos en un mensaje

### 💾 Persistencia
- **Retain**: No utilizado
- **Durabilidad**: No requerida
- **Recovery**: Reconexión automática
- **State**: Mantenido en memoria y EEPROM

### 🔒 Seguridad
- **Autenticación**: Username/Password
- **Autorización**: Por tópico y dispositivo
- **Validación**: Estructura de mensajes
- **Sanitización**: Datos de entrada
- **Timeout**: Protección contra solicitudes colgadas

---

## Integración

### 🐍 Python

```python
import paho.mqtt.client as mqtt
import json

def on_connect(client, userdata, flags, rc):
    print(f"Conectado con código: {rc}")
    # Suscribirse a todos los mensajes del dispositivo
    client.subscribe("swatidhome/+/info")
    client.subscribe("swatidhome/events/+/#")
    client.subscribe("swatidhome/keepalive/+")

def on_message(client, userdata, msg):
    print(f"Tópico: {msg.topic}")
    try:
        data = json.loads(msg.payload.decode())
        print(f"Mensaje: {json.dumps(data, indent=2)}")
    except:
        print(f"Payload: {msg.payload.decode()}")

# Configuración del cliente
client = mqtt.Client()
client.username_pw_set("swatidhome", "Swatid2025!")
client.on_connect = on_connect
client.on_message = on_message

# Conectar al broker
client.connect("188.245.213.181", 1883, 60)

# Enviar comando de activación de relé
def activate_relay(serial, relay_number, duration):
    topic = f"swatidhome/command/{serial}/relay"
    message = {
        "message_id": 1,
        "device": serial,
        "message_type": 0,
        "message_info": {
            "relay_number": relay_number,
            "duration": duration
        }
    }
    client.publish(topic, json.dumps(message))

# Responder a solicitud de validación
def respond_access(serial, message_id, approved, relay=1, duration=2000):
    topic = f"swatidhome/command/{serial}/granted"
    message = {
        "message_id": message_id,
        "device": serial,
        "response": "APPROVED" if approved else "DENIED",
        "relay_number": relay,
        "duration": duration,
        "reason": "Valid code" if approved else "Invalid code"
    }
    client.publish(topic, json.dumps(message))

client.loop_forever()
```

### 🟨 Node.js

```javascript
const mqtt = require('mqtt');

const client = mqtt.connect('mqtt://188.245.213.181:1883', {
  username: 'swatidhome',
  password: 'Swatid2025!'
});

client.on('connect', () => {
  console.log('Conectado al broker MQTT');
  
  // Suscribirse a eventos
  client.subscribe('swatidhome/+/info');
  client.subscribe('swatidhome/events/+/#');
  client.subscribe('swatidhome/keepalive/+');
});

client.on('message', (topic, message) => {
  console.log(`Tópico: ${topic}`);
  try {
    const data = JSON.parse(message.toString());
    console.log('Mensaje:', JSON.stringify(data, null, 2));
  } catch (e) {
    console.log('Payload:', message.toString());
  }
});

// Activar relé
function activateRelay(serial, relayNumber, duration) {
  const topic = `swatidhome/command/${serial}/relay`;
  const message = {
    message_id: Date.now(),
    device: serial,
    message_type: 0,
    message_info: {
      relay_number: relayNumber,
      duration: duration
    }
  };
  client.publish(topic, JSON.stringify(message));
}

// Responder a solicitud de validación
function respondAccess(serial, messageId, approved, relay = 1, duration = 2000) {
  const topic = `swatidhome/command/${serial}/granted`;
  const message = {
    message_id: messageId,
    device: serial,
    response: approved ? 'APPROVED' : 'DENIED',
    relay_number: relay,
    duration: duration,
    reason: approved ? 'Valid code' : 'Invalid code'
  };
  client.publish(topic, JSON.stringify(message));
}
```

### 🔧 C++ (ESP32)

```cpp
#include <PubSubClient.h>
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Mensaje recibido en %s\n", topic);
  
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, payload, length);
  
  int messageType = doc["message_type"];
  
  switch(messageType) {
    case 0: // Activación de relé
      int relay = doc["message_info"]["relay_number"];
      float duration = doc["message_info"]["duration"];
      // Activar relé
      break;
    case 1: // Solicitud de información
      // Enviar información del dispositivo
      break;
    // ... otros tipos
  }
}

void setup() {
  client.setServer("188.245.213.181", 1883);
  client.setCallback(callback);
  client.setBufferSize(2048);
  
  if (client.connect("ESP32Client", "swatidhome", "Swatid2025!")) {
    client.subscribe("swatidhome/command/SWATID_XXX/#");
  }
}

void loop() {
  if (!client.connected()) {
    // Reconectar
  }
  client.loop();
}
```

---

## Resumen de message_type

| Tipo | Dirección | Propósito | Tópico |
|------|-----------|-----------|---------|
| 0 | Backend → Dispositivo | Activación de relé | `/relay` |
| 0 | Dispositivo → Backend | Solicitud de validación | `/access` |
| 1 | Backend → Dispositivo | Solicitud de información | `/info` |
| 1 | Dispositivo → Backend | Evento (acceso, error) | `/events/*` |
| 2 | Backend → Dispositivo | Comando de sistema | `/system` |
| 3 | Backend → Dispositivo | Comando de seguridad | `/security` |
| 4 | Backend → Dispositivo | Sincronización de tiempo | `/system` |
| 5 | Backend → Dispositivo | Gestión códigos remotos | `/system` |
| 6 | Backend → Dispositivo | Configuración modo torno | `/system` |

---

## 🆕 Cambios en v2.5.3

### Nuevas Funcionalidades

#### 1. **Obtención Completa de Códigos Almacenados**
- El comando `message_type: 1` ahora devuelve **todos los códigos locales y remotos**
- Incluye información completa de cada código con sus propiedades
- Añadida información de capacidad de memoria (`local_codes_info`, `remote_codes_info`)

#### 2. **Campo `days_string` en Franjas Horarias**
- Nuevo campo que convierte el bitmask numérico a texto legible
- Ejemplos: `31` → `"Lun-Vie"`, `127` → `"Todos los días"`, `96` → `"Sáb-Dom"`
- Se incluye automáticamente en todas las respuestas que contienen franjas horarias

#### 3. **Gestión de Códigos Remotos vía MQTT**
- ✅ `add_remote_code`: Añadir códigos remotos con franjas horarias
- ✅ `remove_remote_code`: Eliminar códigos remotos específicos
- ✅ `clear_remote_codes`: Limpiar todos los códigos remotos
- ✅ Respuestas de confirmación/error automáticas

#### 4. **Ampliación de Límites de Memoria**
- Códigos locales: 20 → **100 códigos** (+400%)
- Códigos remotos: 500 → **100 códigos** (optimizado)
- EEPROM: 4KB → **10KB** (+150%)
- Uso de HEAP: 24.6% → **17.9%** (-27.2%)

### Mejoras en la Información del Dispositivo

**Antes (v2.5.2)**:
```json
{
  "stored_codes": {
    "local_count": 5,
    "remote_count": 10,
    "max_local": 20,
    "max_remote": 500
  }
}
```

**Después (v2.5.3)**:
```json
{
  "stored_codes": [
    {
      "type": "PIN",
      "value": "1234",
      "keyboard_id": 0,
      "relay": 1
    }
  ],
  "local_codes_info": {
    "count": 1,
    "max_capacity": 100,
    "available": 99
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
    }
  ],
  "remote_codes_info": {
    "count": 1,
    "max_capacity": 100,
    "available": 99
  }
}
```

### Cambios en Mensajes Existentes

| Mensaje | Campo | Cambio |
|---------|-------|--------|
| Información del dispositivo | `stored_codes` | Ahora es un array completo de objetos (antes: solo contadores) |
| Información del dispositivo | `stored_codes[].keyboard_id` | **NUEVO**: Indica para qué teclado(s) es válido |
| Información del dispositivo | `stored_remote_codes` | **NUEVO**: Array completo de códigos remotos |
| Información del dispositivo | `local_codes_info` | **NUEVO**: Información de capacidad |
| Información del dispositivo | `remote_codes_info` | **NUEVO**: Información de capacidad |
| Franjas horarias | `days_string` | **NUEVO**: Días en texto legible |

### Compatibilidad

- ✅ **Retrocompatible** con plataformas existentes
- ✅ Los campos antiguos se mantienen cuando es posible
- ✅ Nuevos campos son **opcionales** para el backend
- ⚠️ **Recomendado**: Actualizar el backend para aprovechar los nuevos campos

### Migración para Desarrolladores

**Si tu backend consume `stored_codes`**:
```javascript
// Antes (v2.5.2)
const localCount = deviceInfo.stored_codes.local_count;

// Después (v2.5.3) - Opción 1: Usar el nuevo campo
const localCount = deviceInfo.local_codes_info.count;

// Después (v2.5.3) - Opción 2: Contar el array
const localCount = deviceInfo.stored_codes.length;
```

**Para obtener códigos remotos**:
```javascript
// Nuevo en v2.5.3
const remoteCodes = deviceInfo.stored_remote_codes;
remoteCodes.forEach(code => {
  console.log(`${code.type}: ${code.value}`);
  if (code.time_slots) {
    code.time_slots.forEach(slot => {
      console.log(`  ${slot.days_string}: ${slot.start_hour}:${slot.start_minute}-${slot.end_hour}:${slot.end_minute}`);
    });
  }
});
```

---

**Última actualización**: Octubre 13, 2025  
**Versión del Documento**: 2.1  
**Versión de Firmware Compatible**: v2.5.3+  
**Dispositivo**: SWATID-A2 Controller - 2 x Wiegand