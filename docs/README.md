# KC868A2 - Sistema de Control de Acceso Dual Wiegand

## Información General del Proyecto

### Descripción
El **KC868A2** es un sistema de control de acceso avanzado basado en la placa KinCony KC868-A2 con ESP32, que implementa un controlador de seguridad dual con soporte para dos teclados Wiegand simultáneos, sistema de mensajería MQTT, interfaz web completa y funcionalidades de seguridad avanzadas.

### Versión del Firmware
- **Versión**: 3.0.0
- **Fecha**: 11 de Diciembre, 2025
- **Estado**: Producción - Estable

### Ramas de Desarrollo
- **main**: Versión estable en producción
- **v3.0**: Corrección crítica de validación remota (actual)
- **Versiones anteriores**: v2.5.x (legacy)

### Características Principales

#### 🔐 Sistema Dual Wiegand
- **Teclado 1**: GPIO 33/14 (Principal)
- **Teclado 2**: GPIO 4/16 (Secundario)
- Soporte simultáneo para ambos teclados
- Procesamiento independiente de códigos PIN y TAG RFID/NFC

#### 🌐 Conectividad
- **Ethernet**: PHY LAN8720 con soporte DHCP/IP estática
- **MQTT**: Broker remoto con autenticación
- **Web**: Interfaz administrativa completa
- **RS485**: Compatibilidad con sistemas legacy

#### ⚡ Control de Relés
- **Relé 1**: GPIO 15
- **Relé 2**: GPIO 2
- Temporización individual configurable
- Control remoto vía MQTT y web

#### 🔒 Seguridad Avanzada
- Bloqueo automático tras intentos fallidos
- Configuración remota de parámetros de seguridad
- Almacenamiento local de hasta 500 códigos
- Validación dual (local/remoto)

## Estructura de Documentación

### 📁 Directorios de Documentación

```
docs/
├── README.md                    # Este archivo - Información general
├── v3.0/                       # Documentación rama v3.0 (ACTUAL)
│   ├── README.md               # Información de la rama v3.0
│   ├── contexto-desarrollo.md  # Contexto y análisis del desarrollo
│   └── pruebas.md              # Suite completa de pruebas
├── mejoras/                    # Documentación de mejoras y correcciones
│   └── correccion-mensaje-granted-v2.5.4.md
├── web-environment/            # Entorno web y funcionalidades
│   ├── interface-overview.md
│   ├── configuration-pages.md
│   └── security-management.md
├── messaging/                  # Sistema de mensajería MQTT
│   ├── mqtt-protocol.md
│   ├── message-types.md
│   └── integration-examples.md
├── processes/                  # Procesos y operativa
│   ├── system-operations.md
│   ├── security-processes.md
│   └── maintenance-procedures.md
├── api/                       # API completa
│   ├── mqtt-api-reference.md
│   ├── web-api-reference.md
│   └── integration-guides.md
├── hardware/                  # Hardware y conexiones
│   ├── pinout-diagram.md
│   └── ...
├── security/                  # Seguridad y autenticación
│   └── access-control.md
├── analisis/                  # Análisis técnicos
│   └── ...
└── OTAA/                      # Over-The-Air Updates
    └── ...
```

## Información Técnica

### Hardware Requerido
- **Placa**: KinCony KC868-A2
- **Microcontrolador**: ESP32
- **Ethernet**: PHY LAN8720
- **Almacenamiento**: EEPROM 4KB
- **Conectores**: 2x Wiegand, 2x Relé, 1x RS485

### Software Requerido
- **Build System**: PlatformIO Core 6.1.18+
- **Plataforma**: Espressif32@5.4.0
- **Framework**: Arduino ESP32 v2.0.6
- **Librerías**:
  - ArduinoJson 6.21.5
  - PubSubClient 2.8.0
  - ETH, WiFi, WebServer, Update (incluidas en ESP32)

### Configuración de Red
- **Broker MQTT**: 188.245.213.181:1883
- **Usuario MQTT**: swatidhome
- **Protocolo**: TCP/IP con autenticación
- **Puerto Web**: 80

## Funcionalidades Implementadas

### ✅ Completadas (v3.0.0)
- [x] Sistema dual Wiegand operativo
- [x] Interfaz web completa
- [x] Sistema MQTT con autenticación
- [x] Control de relés con temporización
- [x] Sistema de seguridad avanzado
- [x] Almacenamiento de códigos (500 máximo)
- [x] **Validación local y remota (CORREGIDA v3.0.0)** ✨
- [x] Modo AP de emergencia
- [x] Monitoreo de sistema
- [x] Logging y auditoría mejorada
- [x] Actualización OTA

### 🔄 En Desarrollo (v3.1.0)
- [ ] Optimización de uso de Flash (actualmente 90.5%)
- [ ] WiFi directo sin AP intermedio
- [ ] Gestión de múltiples redes WiFi
- [ ] Tests automatizados
- [ ] Métricas de telemetría

### 🚀 Planificado (v4.0)
- [ ] Integración con sistemas de videovigilancia
- [ ] API REST adicional
- [ ] Sistema de backup automático
- [ ] Dashboard de monitoreo avanzado

## Estado del Sistema

### Conectividad
- **Ethernet**: ✅ Operativo
- **MQTT**: ✅ Conectado
- **Web**: ✅ Accesible
- **Teclados**: ✅ Ambos operativos

### Seguridad
- **Acceso Local**: 🔓 Permitido
- **Intentos Fallidos**: 0/3
- **Códigos Almacenados**: Variable
- **Modo Validación**: Configurable

## Acceso Rápido

### Interfaz Web
- **URL**: http://[IP_DISPOSITIVO]
- **Usuario**: admin
- **Contraseña**: admin (cambiar en primera configuración)

### MQTT
- **Broker**: 188.245.213.181:1883
- **Usuario**: swatidhome
- **Contraseña**: Swatid2025!

### Serial Monitor
- **Baudrate**: 115200
- **Debug**: Completo con emojis
- **Logs**: Tiempo real

## Soporte y Mantenimiento

### Monitoreo
- Estado del sistema cada 30 segundos
- Keepalive MQTT cada minuto
- Verificación de memoria automática
- Logs detallados de eventos

### Mantenimiento
- Reinicio remoto vía MQTT/web
- Reset a configuración por defecto
- Actualización de configuración en caliente
- Backup automático de códigos

## Contacto y Soporte

Para soporte técnico o consultas sobre el sistema:
- Revisar documentación completa en subdirectorios
- Consultar logs del Serial Monitor
- Verificar conexiones hardware
- Revisar configuración de red

## 🆕 Novedades en v3.0.0

### Corrección Crítica de Validación Remota
La versión 3.0.0 resuelve un **bug crítico** que impedía el funcionamiento de la validación remota:
- ✅ Los relés ahora se abren correctamente cuando el backend aprueba un acceso
- ✅ Procesamiento correcto de campos `relay_number` y `duration`
- ✅ Validación de `message_id` entre solicitud y respuesta
- ✅ Eliminación de código duplicado que causaba errores

**Documentación completa**: [`/docs/v3.0/README.md`](/docs/v3.0/README.md)

### Archivos del Firmware v3.0.0
- [`firmware/SWATID-A2_v3.0.0.bin`](/firmware/SWATID-A2_v3.0.0.bin) - Firmware compilado
- [`firmware/README_v3.0.0.md`](/firmware/README_v3.0.0.md) - Documentación técnica
- [`firmware/RELEASE_NOTES_v3.0.0.md`](/firmware/RELEASE_NOTES_v3.0.0.md) - Notas de la versión
- [`firmware/manifest_v3.0.0.json`](/firmware/manifest_v3.0.0.json) - Manifest para OTA

---

**Última actualización**: 11 de Diciembre, 2025  
**Versión del documento**: 2.0  
**Estado**: Documentación completa - Rama v3.0
