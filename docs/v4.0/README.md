# Rama v4.0 - Servidor BLE para Configuración Móvil

## Objetivo

Implementar un servidor BLE (Bluetooth Low Energy) que permita configurar el dispositivo KC868-A2 desde una aplicación móvil, con sistema de autenticación por vinculación.

## Estado

- **Rama**: v4.0
- **Estado**: ✅ Implementado
- **Versión firmware**: v4.0.0-BLE
- **Fecha**: Febrero 2026

## Características Implementadas

### Sistema de Autenticación BLE

| Característica | Estado | Descripción |
|----------------|--------|-------------|
| Superadmin | ✅ | Primer usuario vinculado, permisos completos |
| Usuarios | ✅ | Hasta 5 usuarios con permisos configurables |
| Claves 64 bytes | ✅ | Almacenadas en EEPROM |
| Permisos granulares | ✅ | Control de relés, modo, códigos, red |

### Funcionalidades de Control vía BLE

| Función | Estado | UUID | Autenticación |
|---------|--------|------|---------------|
| Autenticación | ✅ | FF01 | No (es el login) |
| Control de Relés 1/2 | ✅ | FF02 | Sí |
| Cambiar Modo (Normal/Torno) | ✅ | FF03 | Sí |
| Añadir Tag/PIN local | ✅ | FF04 | Sí |
| Configurar IP (DHCP/Fija) | ✅ | FF05 | Sí |
| Configurar tiempo de relé | ✅ | FF06 | Sí |
| Estado del dispositivo | ✅ | FF07 | No (lectura) |
| **Info Dispositivo (Provisión)** | ✅ | **FF08** | **No (público)** |
| **Info Completa (IP, red, estado)** | ✅ | **FF09** | **Sí** |
| **Lista Códigos Locales** | ✅ | **FF0A** | **Sí** |

### Provisión Automática (FF08)

La característica **FF08** permite a la APP móvil identificar automáticamente el dispositivo **sin necesidad de autenticarse**:

```json
{
  "device_type": "SWATID-A2",
  "serial": "SWATID_B4F7D88813BF",
  "firmware_version": "4.0.0",
  "firmware_variant": "BLE",
  "protocol_version": 1,
  "capabilities": {
    "relays": 2,
    "wiegand_inputs": 2,
    "digital_inputs": 2,
    "ble_users": 5
  },
  "superadmin_registered": true,
  "active_users": 2
}
```

**Beneficios:**
- La APP identifica el tipo de dispositivo automáticamente (SWATID-A2, SWATID-B16, etc.)
- Verifica compatibilidad de protocolo antes de intentar autenticarse
- Muestra información del firmware al usuario
- Evita errores de conexión a dispositivos incompatibles

### Información Completa del Dispositivo (FF09)

La característica **FF09** proporciona información detallada del dispositivo **requiriendo autenticación**:

```json
{
  "device_type": "SWATID-A2",
  "serial": "SWATID_B4F7D88813BF",
  "name": "Controladora Principal",
  "firmware": "4.0.0-BLE",
  "network": {
    "ip": "192.168.1.100",
    "mac": "B4:F7:D8:88:13:BF",
    "dhcp": false,
    "connected": true,
    "gateway": "192.168.1.1",
    "subnet": "255.255.255.0",
    "dns": "8.8.8.8"
  },
  "status": {
    "relay1": false,
    "relay2": false,
    "relay_duration": 3.0,
    "mode": "normal",
    "mqtt_connected": true
  },
  "codes": {
    "local_count": 15,
    "local_max": 50,
    "remote_count": 25,
    "validation_mode": "local_first"
  },
  "uptime_seconds": 86400
}
```

**Información incluida:**
- Configuración de red completa (IP, MAC, gateway, DNS)
- Estado operacional de los relés
- Contadores de códigos locales y remotos
- Estado de conexión MQTT
- Tiempo de funcionamiento

### Lista de Códigos Locales (FF0A)

La característica **FF0A** permite obtener la lista de códigos locales configurados **requiriendo autenticación**:

```json
{
  "count": 45,
  "max": 50,
  "validation_mode": "local_first",
  "codes": [
    {"id": 0, "type": "TAG", "value": "12345678", "keyboard": 0, "relay": 1},
    {"id": 1, "type": "PIN", "value": "1234", "keyboard": 1, "relay": 1}
  ],
  "more": true,
  "showing": 20
}
```

**Funcionalidades:**
- Lista de todos los códigos TAG y PIN configurados
- Información del teclado y relé asignado a cada código
- Soporte de paginación para listas grandes (20 códigos por página)
- Indicador de si hay más códigos disponibles

### Gestión de Vinculaciones

| Función | BLE | MQTT | Web |
|---------|-----|------|-----|
| Ver estado | ❌ | ✅ | ✅ |
| Eliminar superadmin | ❌ | ✅ | ✅ |
| Eliminar usuario | ❌ | ✅ | ✅ |
| Eliminar todas | ❌ | ✅ | ✅ |
| Añadir usuario | ✅* | ❌ | ❌ |

*Solo superadmin puede añadir usuarios vía BLE

## Documentación

### Análisis y Diseño

- [Análisis de Viabilidad](analisis-viabilidad-ble.md) - Análisis técnico de memoria y factibilidad

### Protocolos de Mensajería

- [messages/README.md](messages/README.md) - Índice de documentación de mensajería
- [messages/ble-protocol.md](messages/ble-protocol.md) - **Protocolo BLE completo**
- [messages/mqtt-protocol.md](messages/mqtt-protocol.md) - **Protocolo MQTT completo** (incluye comandos BLE)

## Uso de Memoria

| Recurso | v3.0 (sin BLE) | v4.0 (con BLE) |
|---------|----------------|----------------|
| **Flash** | 91.8% | 73.9% (partición ampliada) |
| **RAM** | 15.3% | 18.3% |
| **EEPROM** | ~3.7 KB | ~4.0 KB (+400 bytes BLE) |

## Compilación

```bash
# Compilar versión sin BLE (v3.x)
pio run -e esp32dev

# Compilar versión con BLE (v4.x)
pio run -e esp32dev_ble

# Subir versión BLE
pio run -e esp32dev_ble -t upload
```

## Acceso Web

Nueva página de gestión BLE disponible en:

```
http://[IP_DISPOSITIVO]/ble
```

Funciones:
- Ver estado del servidor BLE
- Ver superadmin y usuarios vinculados
- Eliminar vinculaciones individuales o todas

## Comandos MQTT para BLE

El message_type 6 permite gestionar vinculaciones BLE remotamente:

```json
// Limpiar todas las vinculaciones
{"device": "SWATID_XXX", "message_type": 6, "message_info": {"action": "clear_all"}}

// Limpiar superadmin
{"device": "SWATID_XXX", "message_type": 6, "message_info": {"action": "clear_superadmin"}}

// Limpiar usuario (slot 1-5)
{"device": "SWATID_XXX", "message_type": 6, "message_info": {"action": "clear_user", "slot": 1}}

// Obtener estado BLE
{"device": "SWATID_XXX", "message_type": 6, "message_info": {"action": "get_status"}}
```

## Nombre BLE del Dispositivo

El dispositivo aparece en Bluetooth con el **número de serie completo**:

```
SWATID_XXXXXXXXXXXX
```

Esto permite identificar claramente cada controladora al buscar dispositivos.

## Permisos BLE

| Bit | Valor | Permiso | Descripción |
|-----|-------|---------|-------------|
| 0 | 0x01 | RELAY_CONTROL | Control de relés |
| 1 | 0x02 | MODE_CHANGE | Cambio de modo |
| 2 | 0x04 | ADD_CODES | Añadir códigos |
| 3 | 0x08 | NETWORK_CONFIG | Configuración de red |
| 7 | 0xFF | ADMIN | Todos los permisos |

## Archivos Modificados/Creados

| Archivo | Cambio |
|---------|--------|
| `src/main.ino` | Servidor BLE, página web, comando MQTT |
| `platformio.ini` | Nuevo entorno esp32dev_ble |
| `partitions_ble.csv` | Partición con más espacio para app |
| `docs/v4.0/messages/` | Documentación de protocolos |

---

*Documentación actualizada: Febrero 2026*
