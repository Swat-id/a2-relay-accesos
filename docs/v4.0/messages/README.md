# Documentación de Mensajería - SWATID-A2 v4.0

Este directorio contiene la documentación completa de los protocolos de comunicación del firmware v4.0.

## Índice

| Documento | Descripción |
|-----------|-------------|
| [ble-protocol.md](ble-protocol.md) | Protocolo BLE completo para configuración móvil |
| [mqtt-protocol.md](mqtt-protocol.md) | Protocolo MQTT para comunicación con backend |
| [ble-config-read.md](ble-config-read.md) | Guía detallada para lectura de configuración vía BLE |
| [BLE_JSON_FORMAT.md](BLE_JSON_FORMAT.md) | Formato JSON exacto de FF09 y FF0A |
| [APP_INTEGRATION_GUIDE.md](APP_INTEGRATION_GUIDE.md) | **⚠️ IMPORTANTE: Guía de integración para la APP** |

## Resumen de Protocolos

### Bluetooth Low Energy (BLE)

- **Propósito**: Configuración local desde aplicación móvil
- **Seguridad**: Autenticación con clave de 64 bytes
- **Usuarios**: 1 superadmin + 5 usuarios vinculados
- **Funcionalidades**:
  - Control de relés
  - Cambio de modo (Normal/Torno)
  - Gestión de códigos de acceso
  - Configuración de red
  - Configuración de tiempos

### MQTT

- **Propósito**: Comunicación con servidor remoto/backend
- **Seguridad**: Usuario/contraseña + serial único
- **Funcionalidades**:
  - Control remoto de relés
  - Validación de accesos
  - Gestión de configuración
  - Monitoreo del dispositivo
  - Gestión de vinculaciones BLE

## Comparativa de Protocolos

| Característica | BLE | MQTT |
|----------------|-----|------|
| **Alcance** | Local (~10m) | Remoto (Internet) |
| **Latencia** | Baja (<100ms) | Variable |
| **Conectividad** | Sin Internet | Requiere Internet |
| **Autenticación** | Clave 64 bytes | Usuario/Contraseña |
| **Persistencia** | Sesión | Siempre disponible |
| **Casos de uso** | Configuración inicial, emergencia | Operación diaria |

## Versión

- **Firmware**: v4.0.0-BLE
- **Fecha**: Febrero 2026
- **Rama**: v4.0
