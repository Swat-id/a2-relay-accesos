# Manual Completo de Comunicaciones - Controladora KC868A2

## 📋 Índice

1. [Información General](#información-general)
2. [Interfaz Web](#interfaz-web)
3. [API MQTT](#api-mqtt)
4. [Comandos de Seguridad](#comandos-de-seguridad)
5. [Gestión de Códigos](#gestión-de-códigos)
6. [Configuración del Sistema](#configuración-del-sistema)
7. [Modo Torno](#modo-torno)
8. [Códigos Remotos](#códigos-remotos)
9. [Exportación e Importación](#exportación-e-importación)
10. [Mensajes de Error](#mensajes-de-error)
11. [Ejemplos Prácticos](#ejemplos-prácticos)

---

## 📡 Información General

### Configuración de Red
- **IP por defecto**: 192.168.1.100
- **Puerto web**: 80
- **Credenciales**: admin/admin
- **MQTT Broker**: 188.245.213.181:1883
- **Usuario MQTT**: swatidhome
- **Contraseña MQTT**: Swatid2025!

### Identificación del Dispositivo
- **Serial MQTT**: SWATID_584614BBBC2C (fijo)
- **Nombre dispositivo**: Configurable via web
- **MAC Address**: Única por dispositivo

---

## 🌐 Interfaz Web

### Páginas Principales

#### 1. Página Principal (`/`)
**Funcionalidades:**
- Estado del sistema en tiempo real
- Configuración del dispositivo
- Control manual de relés
- Información de conexión
- Gestión de modo de validación

**Configuración del Dispositivo:**
```html
POST /save
- deviceName: Nombre del dispositivo
- releDuration: Duración del relé (segundos)
- useDhcp: Usar DHCP (1/0)
- staticIp: IP estática (si no DHCP)
- staticGateway: Puerta de enlace
- staticSubnet: Máscara de subred
- staticDns: DNS
- validationMode: Modo de validación (local/remote)
```

#### 2. Gestión de Códigos (`/codes`)
**Funcionalidades:**
- Listado de códigos locales
- Añadir nuevos códigos
- Eliminar códigos existentes
- Exportar a CSV
- Importar desde CSV

**Añadir Código:**
```html
POST /codes/add
- type: PIN o TAG
- value: Valor del código
- keyboard: Teclado (0=ambos, 1=teclado1, 2=teclado2)
- relay: Relé a activar (1 o 2)
```

**Eliminar Código:**
```html
GET /codes/delete?type=PIN&value=1234
```

**Exportar Códigos:**
```html
GET /export/codes
Content-Type: text/csv
Content-Disposition: attachment; filename=codigos_locales_[timestamp].csv
```

**Importar Códigos:**
```html
POST /import/codes
Content-Type: multipart/form-data
- csvFile: Archivo CSV con códigos
```

#### 3. Códigos Remotos (`/remote-codes`)
**Funcionalidades:**
- Listado de códigos remotos
- Añadir códigos con franjas horarias
- Eliminar códigos remotos
- Exportar a CSV

**Añadir Código Remoto:**
```html
POST /remote-codes/add
- type: PIN o TAG
- value: Valor del código
- keyboard: Teclado (0=ambos, 1=teclado1, 2=teclado2)
- relay: Relé a activar (1 o 2)
- timeSlots: Franjas horarias (JSON)
```

**Eliminar Código Remoto:**
```html
GET /remote-codes/delete?type=PIN&value=1234
```

**Eliminar Todos los Códigos Remotos:**
```html
GET /remote-codes/delete-all
```

#### 4. Configuración del Modo Torno (`/turnstile`)
**Funcionalidades:**
- Activar/desactivar modo torno
- Configurar mapeo teclado-relé
- Estado de solicitudes pendientes
- Cancelar solicitudes pendientes

**Configurar Modo Torno:**
```html
POST /turnstile/config
- enabled: Activar modo torno (1/0)
- keyboard1_relay: Relé para teclado 1 (1 o 2)
- keyboard2_relay: Relé para teclado 2 (1 o 2)
```

**Cancelar Solicitud Pendiente:**
```html
GET /turnstile/reset
```

#### 5. Control Manual (`/rele`)
**Funcionalidades:**
- Control directo de relés
- Apertura temporal

**Control de Relé:**
```html
POST /rele
- rele: Número de relé (1 o 2)
- duration: Duración en segundos
```

#### 6. Gestión del Sistema
**Reiniciar Sistema:**
```html
GET /reboot
```

**Reset Completo:**
```html
GET /reset
```

**Cambiar Contraseña:**
```html
POST /changepass
- newPassword: Nueva contraseña
```

---

## 📡 API MQTT

### Configuración MQTT
- **Broker**: 188.245.213.181:1883
- **Usuario**: swatidhome
- **Contraseña**: Swatid2025!
- **Serial**: SWATID_584614BBBC2C (fijo)

### Topics Principales

#### 1. Validación de Códigos
**Topic de Solicitud:**
```
swatidhome/SWATID_584614BBBC2C/request
```

**Mensaje de Solicitud:**
```json
{
  "timestamp": "2025-01-09T10:30:00+01:00",
  "message_id": 123,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "request_type": "VALIDATE_CODE",
  "code": "1234",
  "type": "PIN",
  "keyboard_id": 1,
  "request_relay": 1
}
```

**Topic de Respuesta:**
```
swatidhome/SWATID_584614BBBC2C/response
```

**Mensaje de Respuesta (Aprobado):**
```json
{
  "timestamp": "2025-01-09T10:30:01+01:00",
  "message_id": 123,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "response": "APPROVED",
  "relay": 1,
  "duration": 3.0
}
```

**Mensaje de Respuesta (Denegado):**
```json
{
  "timestamp": "2025-01-09T10:30:01+01:00",
  "message_id": 123,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "response": "DENIED",
  "reason": "INVALID_CODE"
}
```

#### 2. Modo Torno
**Topic de Solicitud (Modo Torno):**
```
swatidhome/SWATID_584614BBBC2C/turnstile/request
```

**Mensaje de Solicitud (Modo Torno):**
```json
{
  "timestamp": "2025-01-09T10:30:00+01:00",
  "message_id": 124,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "request_type": "TURNSTILE_VALIDATE",
  "code": "5678",
  "type": "PIN",
  "keyboard_id": 2,
  "request_relay": 1,
  "turnstile_config": {
    "keyboard1_relay": 1,
    "keyboard2_relay": 2
  }
}
```

**Topic de Respuesta (Modo Torno):**
```
swatidhome/SWATID_584614BBBC2C/turnstile/response
```

#### 3. Eventos del Sistema
**Topic de Eventos:**
```
swatidhome/SWATID_584614BBBC2C/events
```

**Tipos de Eventos:**
- `ACCESS_GRANTED`: Acceso concedido
- `ACCESS_DENIED`: Acceso denegado
- `RELAY_ACTIVATED`: Relé activado
- `SYSTEM_ERROR`: Error del sistema
- `CONFIGURATION_CHANGED`: Configuración cambiada

**Ejemplo de Evento:**
```json
{
  "timestamp": "2025-01-09T10:30:00+01:00",
  "message_id": 125,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "event_type": "ACCESS_GRANTED",
  "code": "1234",
  "type": "PIN",
  "keyboard_id": 1,
  "relay": 1,
  "source": "LOCAL"
}
```

#### 4. Gestión de Códigos Remotos
**Topic de Códigos Remotos:**
```
swatidhome/SWATID_584614BBBC2C/remote-codes
```

**Añadir Código Remoto:**
```json
{
  "timestamp": "2025-01-09T10:30:00+01:00",
  "message_id": 126,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "action": "ADD_REMOTE_CODE",
  "code": {
    "type": "PIN",
    "value": "9999",
    "keyboard_id": 1,
    "relay": 1,
    "time_slots": [
      {
        "start_hour": 8,
        "start_minute": 0,
        "end_hour": 14,
        "end_minute": 0,
        "days_of_week": 31
      }
    ]
  }
}
```

**Eliminar Código Remoto:**
```json
{
  "timestamp": "2025-01-09T10:30:00+01:00",
  "message_id": 127,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "action": "DELETE_REMOTE_CODE",
  "type": "PIN",
  "value": "9999"
}
```

**Eliminar Todos los Códigos Remotos:**
```json
{
  "timestamp": "2025-01-09T10:30:00+01:00",
  "message_id": 128,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "action": "DELETE_ALL_REMOTE_CODES"
}
```

---

## 🔐 Gestión de Códigos

### Tipos de Códigos

#### 1. Códigos Locales
- **Almacenamiento**: EEPROM local
- **Capacidad**: 500 códigos máximo
- **Persistencia**: Permanente
- **Edición**: Via interfaz web
- **Validación**: Inmediata

**Estructura:**
```cpp
struct CodeEntry {
  char type[5];        // "PIN" o "TAG"
  char value[17];      // Valor del código
  uint8_t keyboard_id; // Teclado (0=ambos, 1=teclado1, 2=teclado2)
  uint8_t relay;       // Relé a activar (1 o 2)
  uint8_t reserved[3]; // Reservado
};
```

#### 2. Códigos Remotos
- **Almacenamiento**: EEPROM local (sincronizado)
- **Capacidad**: 100 códigos máximo
- **Persistencia**: Temporal (gestionado por servidor)
- **Edición**: Solo via MQTT
- **Validación**: Con franjas horarias

**Estructura:**
```cpp
struct RemoteCodeEntry {
  char type[5];          // "PIN" o "TAG"
  char value[17];        // Valor del código
  uint8_t keyboard_id;   // Teclado (0=ambos, 1=teclado1, 2=teclado2)
  uint8_t relay;         // Relé a activar (1 o 2)
  uint8_t time_slots_count; // Número de franjas horarias (máximo 4)
  TimeSlot time_slots[4]; // Franjas horarias
  uint8_t reserved;
};

struct TimeSlot {
  uint8_t start_hour;    // Hora de inicio (0-23)
  uint8_t start_minute;  // Minuto de inicio (0-59)
  uint8_t end_hour;      // Hora de fin (0-23)
  uint8_t end_minute;    // Minuto de fin (0-59)
  uint8_t days_of_week;  // Días de la semana (bitmask)
  uint8_t reserved[3];
};
```

### Validación de Códigos

#### Modo Normal
1. **Primero Local**: Valida códigos locales → MQTT si no encuentra
2. **Primero Remoto**: Envía a MQTT → Fallback local si falla

#### Modo Torno
1. **Primero Local**: Valida códigos locales → MQTT si no encuentra
2. **Primero Remoto**: Envía a MQTT → Fallback local si falla
3. **Mapeo de Relés**: Según configuración del torno

### Franjas Horarias
- **Formato**: HH:MM - HH:MM (24 horas)
- **Días de la semana**: Bitmask (1=Lunes, 2=Martes, 4=Miércoles, 8=Jueves, 16=Viernes, 32=Sábado, 64=Domingo)
- **Múltiples franjas**: Máximo 4 por código

#### Tabla de Referencia - days_of_week
| Valor | Días | Descripción |
|-------|------|-------------|
| `1` | Lunes | Solo Lunes |
| `2` | Martes | Solo Martes |
| `4` | Miércoles | Solo Miércoles |
| `8` | Jueves | Solo Jueves |
| `16` | Viernes | Solo Viernes |
| `32` | Sábado | Solo Sábado |
| `64` | Domingo | Solo Domingo |
| `3` | Lunes, Martes | 1+2 |
| `7` | Lunes a Miércoles | 1+2+4 |
| `15` | Lunes a Jueves | 1+2+4+8 |
| `31` | Lunes a Viernes | 1+2+4+8+16 |
| `63` | Lunes a Sábado | 1+2+4+8+16+32 |
| `127` | Todos los días | 1+2+4+8+16+32+64 |
| `96` | Sábado, Domingo | 32+64 |
| `65` | Lunes, Domingo | 1+64 |
| `48` | Viernes, Sábado | 16+32 |

---

## ⚙️ Configuración del Sistema

### Parámetros de Red
```cpp
// Configuración Ethernet
#define ETH_PHY_TYPE ETH_PHY_LAN8720
#define ETH_PHY_ADDR 0
#define ETH_PHY_MDC 23
#define ETH_PHY_MDIO 18
#define ETH_PHY_POWER_PIN 16
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
```

### Parámetros de Seguridad
```cpp
// Configuración de seguridad
const int maxFailedAttempts = 5;        // Intentos fallidos máximos
const unsigned long blockDuration = 300000; // Duración de bloqueo (5 min)
const unsigned long keyPressTimeout = 5000; // Timeout de teclas (5 seg)
```

### Parámetros de Modo Torno
```cpp
// Configuración del modo torno
#define TURNSTILE_TIMEOUT 5000          // Timeout de solicitud (5 seg)
```

### Almacenamiento EEPROM
```cpp
// Offsets de EEPROM
#define EEPROM_CONFIG_OFFSET 0          // Configuración general
#define EEPROM_CODES_OFFSET 512         // Códigos locales
#define EEPROM_REMOTE_CODES_OFFSET 2048 // Códigos remotos
#define EEPROM_TURNSTILE_OFFSET 4096    // Configuración torno
```

---

## 🔄 Modo Torno

### Configuración
- **Activación**: Via interfaz web
- **Mapeo**: Teclado 1 → Relé X, Teclado 2 → Relé Y
- **Timeout**: 5 segundos para respuesta remota
- **Fallback**: Apertura local si MQTT falla

### Flujo de Validación
1. **Recepción de código** en cualquier teclado
2. **Validación local** (si está configurado "Primero Local")
3. **Envío a MQTT** con mapeo de relé
4. **Espera de respuesta** (máximo 5 segundos)
5. **Apertura de relé** según teclado origen
6. **Fallback local** si MQTT falla

### Estados del Sistema
- **Solicitud pendiente**: Código enviado a MQTT, esperando respuesta
- **Timeout**: Apertura automática tras 5 segundos
- **Cancelación**: Cancelar solicitud pendiente manualmente

---

## 🔒 Comandos de Seguridad

### Configuración de Comandos
**Topic de Comandos:**
```
swatidhome/command/SWATID_584614BBBC2C/security
```

**Topic de Respuesta:**
```
swatidhome/response/SWATID_584614BBBC2C/rx
```

### Comandos Disponibles

#### 1. Bloquear Acceso Local
**Comando:**
```json
{
  "message_id": 12345,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "message_type": 3,
  "message_info": {
    "security_command": "block_local_access"
  }
}
```

**Respuesta:**
```json
{
  "message_id": 12345,
  "response_type": 0,
  "response_info": "local access blocked"
}
```

#### 2. Desbloquear Acceso Local
**Comando:**
```json
{
  "message_id": 12346,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "message_type": 3,
  "message_info": {
    "security_command": "unblock_local_access"
  }
}
```

**Respuesta:**
```json
{
  "message_id": 12346,
  "response_type": 0,
  "response_info": "local access unblocked"
}
```

#### 3. Deshabilitar Lectura de Teclados
**Comando:**
```json
{
  "message_id": 12347,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "message_type": 3,
  "message_info": {
    "security_command": "disable_keyboard_reading"
  }
}
```

**Respuesta:**
```json
{
  "message_id": 12347,
  "response_type": 0,
  "response_info": "keyboard reading disabled"
}
```

#### 4. Habilitar Lectura de Teclados
**Comando:**
```json
{
  "message_id": 12348,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "message_type": 3,
  "message_info": {
    "security_command": "enable_keyboard_reading"
  }
}
```

**Respuesta:**
```json
{
  "message_id": 12348,
  "response_type": 0,
  "response_info": "keyboard reading enabled"
}
```

#### 5. Establecer Duración de Bloqueo
**Comando:**
```json
{
  "message_id": 12349,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "message_type": 3,
  "message_info": {
    "security_command": "set_block_duration",
    "duration_seconds": 120
  }
}
```

**Respuesta:**
```json
{
  "message_id": 12349,
  "response_type": 0,
  "response_info": "block duration updated to 120 seconds"
}
```

### Diferencias entre Bloqueos

| Tipo | Variable | Efecto | Duración |
|------|----------|--------|----------|
| **Acceso Local** | `localAccessBlocked` | Bloquea validación de códigos | Temporal (60s por defecto) |
| **Lectura Teclados** | `keyboardReadingEnabled` | Bloquea lectura de teclados | Permanente hasta cambio |

### Estados de Seguridad

#### Información de Estado
**Topic de Información:**
```
swatidhome/SWATID_584614BBBC2C/info
```

**Respuesta de Estado:**
```json
{
  "timestamp": "2025-01-09T10:30:00+01:00",
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "security": {
    "local_access_blocked": false,
    "keyboard_reading_enabled": true,
    "failed_attempts": 0,
    "max_failed_attempts": 3,
    "block_duration_seconds": 60
  }
}
```

### Interfaz Web de Seguridad

#### Botones de Control
- **Bloquear/Desbloquear Acceso Local**: Control temporal de validación
- **Habilitar/Deshabilitar Lectura Teclados**: Control permanente de lectura

#### Estados Visuales
- **🔓 Habilitada**: Lectura de teclados activa
- **🔒 Deshabilitada**: Lectura de teclados bloqueada
- **🔓 Permitido**: Acceso local activo
- **🔒 BLOQUEADO**: Acceso local temporalmente bloqueado

---

## 📊 Exportación e Importación

### Exportación CSV

#### Códigos Locales
**Formato:**
```csv
Tipo,Codigo,Teclado,Rele,Fecha_Creacion
PIN,1234,1,1,Local
TAG,ABCD1234,2,2,Local
```

#### Códigos Remotos
**Formato:**
```csv
Tipo,Codigo,Teclado,Rele,Franjas_Horarias,Fecha_Creacion
PIN,9999,1,1,"08:00-14:00",Remoto
PIN,8888,2,2,"08:00-17:00; 09:00-13:00",Remoto
TAG,ABCD1234,0,1,"06:00-14:00; 14:00-22:00; 08:00-20:00; 00:00-23:59",Remoto
TAG,EFGH5678,2,2,"09:00-17:00",Remoto
```

**Explicación de las franjas horarias en CSV:**
- **PIN 9999**: Una franja (Lunes a Viernes 8:00-14:00)
- **PIN 8888**: Dos franjas (Lunes a Viernes 8:00-17:00 + Sábados y Domingos 9:00-13:00)
- **TAG ABCD1234**: Cuatro franjas (Turno mañana + Turno tarde + Fines de semana + Domingos 24h)
- **TAG EFGH5678**: Una franja (Lunes a Viernes 9:00-17:00)

### Importación CSV
- **Solo códigos locales**: Los códigos remotos no se pueden importar
- **Validación automática**: Verificación de formato y datos
- **Reporte de errores**: Lista detallada de problemas
- **Transaccional**: Solo se importan códigos válidos

---

## ❌ Mensajes de Error

### Códigos de Error MQTT
```json
{
  "timestamp": "2025-01-09T10:30:00+01:00",
  "message_id": 129,
  "device": "SWATID_PUERTA",
  "serial": "SWATID_584614BBBC2C",
  "error_code": 1,
  "description": "PIN con longitud incorrecta ingresado en teclado 1"
}
```

**Códigos de Error:**
- `1`: PIN con longitud incorrecta
- `2`: Código no encontrado
- `3`: Sistema bloqueado por intentos fallidos
- `4`: Error de configuración
- `5`: Error de comunicación MQTT
- `6`: Error de validación de franjas horarias

### Mensajes de Log
- `✅`: Operación exitosa
- `❌`: Error o fallo
- `⚠️`: Advertencia
- `📡`: Comunicación MQTT
- `🔧`: Configuración
- `🧹`: Limpieza o reset

---

## 💡 Ejemplos Prácticos

### 1. Configurar Código Local
```bash
# Via interfaz web
curl -X POST http://192.168.1.100/codes/add \
  -u admin:admin \
  -d "type=PIN&value=1234&keyboard=1&relay=1"
```

### 2. Validar Código via MQTT
```bash
# Publicar solicitud
mosquitto_pub -h 188.245.213.181 -p 1883 \
  -u swatidhome -P Swatid2025! \
  -t "swatidhome/SWATID_584614BBBC2C/request" \
  -m '{
    "timestamp": "2025-01-09T10:30:00+01:00",
    "message_id": 130,
    "device": "SWATID_PUERTA",
    "serial": "SWATID_584614BBBC2C",
    "request_type": "VALIDATE_CODE",
    "code": "1234",
    "type": "PIN",
    "keyboard_id": 1,
    "request_relay": 1
  }'
```

### 3. Añadir Código Remoto

#### Ejemplo 1: Código con una franja horaria (Lunes a Viernes 8:00-14:00)
```bash
mosquitto_pub -h 188.245.213.181 -p 1883 \
  -u swatidhome -P Swatid2025! \
  -t "swatidhome/SWATID_584614BBBC2C/remote-codes" \
  -m '{
    "timestamp": "2025-01-09T10:30:00+01:00",
    "message_id": 131,
    "device": "SWATID_PUERTA",
    "serial": "SWATID_584614BBBC2C",
    "action": "ADD_REMOTE_CODE",
    "code": {
      "type": "PIN",
      "value": "9999",
      "keyboard_id": 1,
      "relay": 1,
      "time_slots": [
        {
          "start_hour": 8,
          "start_minute": 0,
          "end_hour": 14,
          "end_minute": 0,
          "days_of_week": 31
        }
      ]
    }
  }'
```

#### Ejemplo 2: Código con múltiples franjas horarias (Horario de oficina + fines de semana)
```bash
mosquitto_pub -h 188.245.213.181 -p 1883 \
  -u swatidhome -P Swatid2025! \
  -t "swatidhome/SWATID_584614BBBC2C/remote-codes" \
  -m '{
    "timestamp": "2025-01-09T10:30:00+01:00",
    "message_id": 132,
    "device": "SWATID_PUERTA",
    "serial": "SWATID_584614BBBC2C",
    "action": "ADD_REMOTE_CODE",
    "code": {
      "type": "PIN",
      "value": "8888",
      "keyboard_id": 2,
      "relay": 2,
      "time_slots": [
        {
          "start_hour": 8,
          "start_minute": 0,
          "end_hour": 17,
          "end_minute": 0,
          "days_of_week": 31
        },
        {
          "start_hour": 9,
          "start_minute": 0,
          "end_hour": 13,
          "end_minute": 0,
          "days_of_week": 96
        }
      ]
    }
  }'
```

#### Ejemplo 3: Código con horario complejo (Múltiples turnos)
```bash
mosquitto_pub -h 188.245.213.181 -p 1883 \
  -u swatidhome -P Swatid2025! \
  -t "swatidhome/SWATID_584614BBBC2C/remote-codes" \
  -m '{
    "timestamp": "2025-01-09T10:30:00+01:00",
    "message_id": 133,
    "device": "SWATID_PUERTA",
    "serial": "SWATID_584614BBBC2C",
    "action": "ADD_REMOTE_CODE",
    "code": {
      "type": "TAG",
      "value": "ABCD1234",
      "keyboard_id": 0,
      "relay": 1,
      "time_slots": [
        {
          "start_hour": 6,
          "start_minute": 0,
          "end_hour": 14,
          "end_minute": 0,
          "days_of_week": 31
        },
        {
          "start_hour": 14,
          "start_minute": 0,
          "end_hour": 22,
          "end_minute": 0,
          "days_of_week": 31
        },
        {
          "start_hour": 8,
          "start_minute": 0,
          "end_hour": 20,
          "end_minute": 0,
          "days_of_week": 96
        },
        {
          "start_hour": 0,
          "start_minute": 0,
          "end_hour": 23,
          "end_minute": 59,
          "days_of_week": 64
        }
      ]
    }
  }'
```

**Explicación de los ejemplos:**

- **Ejemplo 1**: PIN 9999 válido Lunes a Viernes de 8:00 a 14:00
- **Ejemplo 2**: PIN 8888 con dos franjas:
  - Lunes a Viernes: 8:00-17:00 (days_of_week: 31)
  - Sábados y Domingos: 9:00-13:00 (days_of_week: 96)
- **Ejemplo 3**: TAG ABCD1234 con horario de 24/7:
  - Turno mañana: Lunes a Viernes 6:00-14:00 (days_of_week: 31)
  - Turno tarde: Lunes a Viernes 14:00-22:00 (days_of_week: 31)
  - Fines de semana: Sábados y Domingos 8:00-20:00 (days_of_week: 96)
  - Domingos especiales: Todo el día (days_of_week: 64)

#### Casos de Uso Comunes

**1. Horario de Oficina (Lunes a Viernes 9:00-17:00)**
```json
"time_slots": [{
  "start_hour": 9, "start_minute": 0,
  "end_hour": 17, "end_minute": 0,
  "days_of_week": 31
}]
```

**2. Acceso 24/7 (Todos los días)**
```json
"time_slots": [{
  "start_hour": 0, "start_minute": 0,
  "end_hour": 23, "end_minute": 59,
  "days_of_week": 127
}]
```

**3. Solo Fines de Semana (Sábados y Domingos)**
```json
"time_slots": [{
  "start_hour": 8, "start_minute": 0,
  "end_hour": 20, "end_minute": 0,
  "days_of_week": 96
}]
```

**4. Turnos de Trabajo (Mañana y Tarde)**
```json
"time_slots": [
  {
    "start_hour": 6, "start_minute": 0,
    "end_hour": 14, "end_minute": 0,
    "days_of_week": 31
  },
  {
    "start_hour": 14, "start_minute": 0,
    "end_hour": 22, "end_minute": 0,
    "days_of_week": 31
  }
]
```

**5. Días Específicos (Lunes, Miércoles, Viernes)**
```json
"time_slots": [{
  "start_hour": 8, "start_minute": 0,
  "end_hour": 18, "end_minute": 0,
  "days_of_week": 21
}]
```
*Nota: 21 = 1(Lunes) + 4(Miércoles) + 16(Viernes)*

### 4. Configurar Modo Torno
```bash
# Via interfaz web
curl -X POST http://192.168.1.100/turnstile/config \
  -u admin:admin \
  -d "enabled=1&keyboard1_relay=1&keyboard2_relay=2"
```

### 5. Exportar Códigos
```bash
# Descargar códigos locales
curl -u admin:admin \
  http://192.168.1.100/export/codes \
  -o codigos_locales.csv
```

### 6. Importar Códigos
```bash
# Subir archivo CSV
curl -X POST http://192.168.1.100/import/codes \
  -u admin:admin \
  -F "csvFile=@codigos_locales.csv"
```

---

## 🔧 Troubleshooting

### Problemas Comunes

#### 1. No se conecta a MQTT
- Verificar credenciales
- Comprobar conectividad de red
- Revisar configuración del broker

#### 2. Códigos no se validan
- Verificar formato de mensaje MQTT
- Comprobar serial del dispositivo
- Revisar logs del sistema

#### 3. Modo torno no funciona
- Verificar configuración de mapeo
- Comprobar timeout de solicitudes
- Revisar estado de MQTT

#### 4. Importación CSV falla
- Verificar formato del archivo
- Comprobar codificación UTF-8
- Revisar validación de campos

### Logs del Sistema
- **Serial Monitor**: 115200 baudios
- **Nivel de detalle**: Alto
- **Formato**: Timestamp + Emoji + Mensaje
- **Rotación**: Automática

---

## 📚 Referencias

### Documentación Técnica
- [Análisis del Modo Torno](mejoras/analisis-modo-torno.md)
- [Propuesta de Mejoras](mejoras/propuesta-mejora.md)
- [API MQTT](api/mqtt-api-reference.md)

### Configuración de Red
- **Ethernet**: LAN8720
- **Protocolo**: TCP/IP
- **DHCP**: Soportado
- **IP Estática**: Configurable

### Hardware
- **Microcontrolador**: ESP32
- **Teclados**: Dual Wiegand (GPIO 33/14 y 4/16)
- **Relés**: 2 relés independientes
- **Almacenamiento**: EEPROM 4KB

---

*Documento generado automáticamente - Controladora KC868A2 v1.0*
*Última actualización: 2025-01-09*
