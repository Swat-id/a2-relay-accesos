# Changelog v4.1.0

## Fecha: 5 de Febrero de 2026
## Rama: v4.1

---

## Resumen de Cambios

Esta versión implementa mejoras críticas de seguridad en la comunicación BLE y MQTT.

---

## Nuevas Características

### 1. Autenticación BLE Segura (Challenge-Response)

**Característica FF0B (Challenge)**
- Nueva característica BLE para autenticación segura
- Genera un nonce aleatorio de 16 bytes
- Permite autenticación sin transmitir la clave en claro

**Flujo de Autenticación v4.1:**
```
APP                                 Dispositivo
 │                                       │
 │  1. Lee FF0B (Challenge)              │
 │──────────────────────────────────────>│ Genera nonce 16 bytes
 │                                       │
 │  2. APP calcula:                      │
 │     response = SHA256(key + nonce)    │
 │                                       │
 │  3. Envía response a FF01 (32 bytes)  │
 │──────────────────────────────────────>│ Verifica response
 │                                       │ Genera token 8 bytes
 │  4. Recibe "OK:TOKEN:XXXXXXXX"        │
 │<──────────────────────────────────────│
 │                                       │
```

**Beneficios:**
- La clave nunca viaja por el aire
- Protección contra replay attacks (nonce único)
- Token de sesión con timeout de 5 minutos

### 2. Token de Sesión BLE

- Token de 8 bytes generado tras autenticación exitosa
- Timeout de sesión: 5 minutos de inactividad
- Timeout de autenticación: 30 segundos para completar challenge

### 3. Derivación Segura de Claves MQTT (HKDF)

**Nuevo comando MQTT: `add_user_derived`**

```json
{
    "message_type": 4,
    "message_id": "xyz",
    "message_info": {
        "action": "add_user_derived",
        "slot": 1,
        "user_id": "user123",
        "name": "Juan",
        "permissions": 15
    }
}
```

**Funcionamiento:**
- La clave se deriva usando HKDF-SHA256
- Parámetros: `key = HKDF(device_master_key, serial, "user_key:" + user_id)`
- La clave nunca viaja por MQTT
- Tanto el servidor como el dispositivo calculan la misma clave

### 4. Clave Maestra del Dispositivo

- Clave de 32 bytes generada automáticamente en primer boot
- Almacenada en EEPROM con checksum de integridad
- Usada como Input Key Material para HKDF

---

## Mejoras de Seguridad

| Aspecto | v4.0 | v4.1 |
|---------|------|------|
| Clave BLE en tráfico | Sí (64 bytes) | No (solo SHA256 response) |
| Replay attacks BLE | Vulnerable | Protegido (nonce único) |
| Clave usuario MQTT | En texto claro | Derivada (HKDF) |
| Token de sesión | No | Sí (8 bytes, 5 min timeout) |

---

## Compatibilidad

### Modo Legado (v4.0 APPs)
- APPs antiguas que envían clave de 64 bytes siguen funcionando
- El dispositivo detecta automáticamente el método de autenticación
- Respuesta incluye token: `OK:AUTHENTICATED:TOKEN:XXXXXXXX`

### Modo Seguro (v4.1 APPs)
- APPs nuevas usan Challenge-Response
- Respuesta: `OK:TOKEN:XXXXXXXX`

---

## UUIDs de Características BLE

| UUID | Nombre | Descripción |
|------|--------|-------------|
| FF00 | Service | Servicio principal SWAT-ID |
| FF01 | Auth | Autenticación (64 bytes legado / 32 bytes challenge-response) |
| FF02 | Relay | Control de relés |
| FF03 | Mode | Cambio de modo |
| FF04 | AddCode | Añadir códigos |
| FF05 | Network | Configuración de red |
| FF06 | RelayTime | Tiempo de activación de relés |
| FF07 | Status | Estado del dispositivo |
| FF08 | DevInfo | Info del dispositivo (público) |
| FF09 | FullInfo | Info completa (requiere auth) |
| FF0A | Codes | Lista de códigos (requiere auth) |
| **FF0B** | **Challenge** | **NUEVO: Challenge para auth seguro** |

---

## Archivos de Firmware

- `firmware/SWATID-A2_v4.1.0-BLE.bin` - Binario compilado
- `firmware/SWATID-A2_v4.1.0-BLE.sha256` - Checksum SHA256

---

## Uso de Memoria

```
RAM:   18.3% (59992 / 327680 bytes)
Flash: 76.2% (1497229 / 1966080 bytes)
```

---

## Documentación Relacionada

- `docs/v4.1/SECURITY_ANALYSIS.md` - Análisis de seguridad completo
- `docs/v4.1/BLE_AUTHENTICATION.md` - Flujo detallado de autenticación
- `docs/v4.1/MQTT_SECURE_USERS.md` - Gestión segura de usuarios MQTT
