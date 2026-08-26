# Documentación Técnica - SWATID-A2 Firmware v4.1

> **Versión:** v4.1.0-BLE  
> **Fecha:** Marzo 2026  
> **Estado:** ✅ Producción

---

## Contenido de esta Documentación

Esta carpeta contiene la documentación técnica completa del firmware SWATID-A2 v4.1, orientada a desarrolladores e integradores.

### Documentos Disponibles

| Documento | Descripción | Audiencia |
|-----------|-------------|-----------|
| [BLE_INTEGRATION.md](./BLE_INTEGRATION.md) | Integración completa BLE con NimBLE | Desarrolladores de Apps |
| [MQTT_INTEGRATION.md](./MQTT_INTEGRATION.md) | Protocolo MQTT y mensajería | Desarrolladores Backend |
| [FIRMWARE_STORAGE.md](./FIRMWARE_STORAGE.md) | Almacenamiento EEPROM + Portal Web | Desarrolladores Firmware |
| [WEB_PORTAL_IMPLEMENTATION.md](./WEB_PORTAL_IMPLEMENTATION.md) | Implementación del portal web | Desarrolladores Web/Firmware |

---

## Resumen del Sistema

### Arquitectura General

```
┌─────────────────────────────────────────────────────────────────────┐
│                          SWATID-A2 v4.1                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│   │     BLE      │    │   Ethernet   │    │   Wiegand    │          │
│   │   (NimBLE)   │    │    (LAN)     │    │  (Teclados)  │          │
│   └──────┬───────┘    └──────┬───────┘    └──────┬───────┘          │
│          │                   │                   │                   │
│          ▼                   ▼                   ▼                   │
│   ┌─────────────────────────────────────────────────────────┐       │
│   │                    Lógica Principal                      │       │
│   │   - Validación de códigos (local + remota)              │       │
│   │   - Control de relés                                     │       │
│   │   - Gestión de usuarios BLE                             │       │
│   │   - Servidor web de configuración                       │       │
│   └──────────────────────────┬──────────────────────────────┘       │
│                              │                                       │
│                              ▼                                       │
│   ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│   │    EEPROM    │    │    Relé 1    │    │    Relé 2    │          │
│   │  (8KB Flash) │    │              │    │              │          │
│   └──────────────┘    └──────────────┘    └──────────────┘          │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Características Principales v4.1

- **Autenticación BLE Challenge-Response**: Seguridad mejorada sin transmitir claves
- **Robustez de red**: Funciona sin MQTT, backoff exponencial en reconexión
- **Protocolo EOT**: Transmisión confiable de datos grandes por BLE
- **Guardado diferido**: Sin bloqueos en callbacks BLE
- **5 usuarios BLE**: SUPERADMIN + 4 usuarios con permisos configurables

---

## Flujos de Integración Clave

### 1. Autenticación BLE (Challenge-Response)

```
App                              Firmware
 │                                  │
 ├── Read FF08 (Device Info) ─────►│
 │◄──── JSON con estado ───────────┤
 │                                  │
 ├── Read FF0B (Challenge) ───────►│
 │◄──── 16 bytes aleatorios ───────┤
 │                                  │
 │ SHA256(key + challenge)          │
 │                                  │
 ├── Write FF01 (32 bytes) ───────►│
 │◄──── "OK:TOKEN:XXXXXXXX" ───────┤
 │                                  │
 └────── Sesión 5 min ─────────────┘
```

**Documentación:** [BLE_INTEGRATION.md § Sistema de Autenticación](./BLE_INTEGRATION.md#sistema-de-autenticación)

### 2. Validación de Acceso con Fallback

```
Código ingresado
       │
       ▼
┌─────────────────┐
│ localFirst=true │──► Validar LOCAL primero
└────────┬────────┘
         │ No encontrado
         ▼
┌─────────────────┐
│ MQTT conectado? │──► NO ──► Denegar
└────────┬────────┘
         │ SÍ
         ▼
┌─────────────────┐
│ Publicar a      │──► Esperar respuesta
│ broker MQTT     │
└────────┬────────┘
         │
         ▼
    granted/denied
```

**Documentación:** [MQTT_INTEGRATION.md § Validación de Acceso Remota](./MQTT_INTEGRATION.md#validación-de-acceso-remota)

### 3. Almacenamiento Persistente

```
EEPROM (8KB)
┌────────────────────┬────────┬──────────────┐
│ DeviceConfig       │ Codes  │ BLE Auth     │
│ (red, seguridad)   │ (50)   │ (5 usuarios) │
└────────────────────┴────────┴──────────────┘
       0                512        4096
```

**Documentación:** [FIRMWARE_STORAGE.md § Mapa de Memoria](./FIRMWARE_STORAGE.md#mapa-de-memoria-eeprom)

### 4. Portal Web de Administración

```
┌─────────────────────────────────────────────┐
│           SWATID-A2 Control Panel           │
├─────────────────────────────────────────────┤
│  📊 Estado    ⚡ Relés    🔧 Configuración   │
├─────────────────────────────────────────────┤
│  Autenticación: HTTP Basic (admin/admin)   │
│  Puerto: 80                                 │
│  Contraseña: Persistida en EEPROM          │
└─────────────────────────────────────────────┘
```

**Documentación:** [FIRMWARE_STORAGE.md § Portal Web](./FIRMWARE_STORAGE.md#portal-web-de-administración)

### 5. Implementación del Portal Web (Para Terceros)

El documento [WEB_PORTAL_IMPLEMENTATION.md](./WEB_PORTAL_IMPLEMENTATION.md) contiene:

- **Arquitectura del servidor** con registro de rutas
- **Sistema de autenticación HTTP Basic** con persistencia
- **Patrones de código replicables** para:
  - Handlers GET/POST con validación
  - Renderizado de tablas con paginación
  - Formularios con campos condicionales
  - APIs JSON
  - Exportación/importación CSV
  - Upload de archivos
- **Estructuras de datos** para códigos y usuarios BLE
- **Recomendaciones** de seguridad, rendimiento y usabilidad

---

## Correcciones Críticas Implementadas

### Bug 1: Códigos BLE No Persistían

**Problema:** El guardado en EEPROM dentro del callback BLE causaba crash.

**Solución:** Guardado diferido al `loop()` mediante flag `pendingCode.pending`.

**Archivo:** `src/main.ino` - Sección FF04 callbacks

### Bug 2: Configuración de Red BLE No Guardaba

**Problema:** `saveConfiguration()` usaba variables globales, pero el callback modificaba `config.*`.

**Solución:** Actualizar tanto variables globales como `config.*` antes de guardar.

**Archivo:** `src/main.ino` - Sección `NetworkCharCallbacks::onWrite`

### Bug 3: Sistema Colgaba sin MQTT

**Problema:** `connectToMqtt()` en callback ETH bloqueaba el sistema.

**Solución:** 
1. Flag `mqttConnectionPending` para diferir conexión a `loop()`
2. Timeout de 5s en conexión MQTT
3. Backoff exponencial (5s → 10s → 20s → 30s)
4. Fallback inmediato a validación local

**Archivo:** `src/main.ino` - Secciones `WiFiEvent`, `connectToMqtt`, `loop`

### Bug 4: JSON Truncado por MTU BLE

**Problema:** MTU de 252 bytes truncaba respuestas JSON grandes.

**Solución:**
1. JSON compacto con nombres cortos (`device_type` → `type`)
2. Paginación (3 códigos por página)
3. Marcador EOT (`0x04`) para fin de transmisión

**Archivos:** `src/main.ino` - Funciones `sendBLEWithEOT`, `updateFF09Value`, `updateFF0AValue`

---

## Tabla de Características BLE

| UUID | Nombre | Auth | Operaciones |
|------|--------|------|-------------|
| FF01 | Auth | ✗ | Registro/Login |
| FF02 | Relay | ✓ | Control relés |
| FF03 | Mode | ✓ | Modo normal/torno |
| FF04 | AddCode | ✓ | Añadir PIN/TAG |
| FF05 | Network | ✓ | Config Ethernet |
| FF06 | RelayTime | ✓ | Tiempo de pulso |
| FF07 | Status | ✓ | Estado actual |
| FF08 | DevInfo | ✗ | Info pública |
| FF09 | FullInfo | ✓ | Config completa |
| FF0A | Codes | ✓ | Lista de códigos |
| FF0B | Challenge | ✗ | Challenge v4.1 |

---

## Tópicos MQTT

### Suscripciones

```
swatidhome/command/{serial}/access   # Respuestas validación
swatidhome/command/{serial}/granted  # Acceso concedido
swatidhome/command/{serial}/denied   # Acceso denegado
swatidhome/command/{serial}/relay    # Control remoto
swatidhome/command/{serial}/config   # Configuración
swatidhome/command/{serial}/codes    # Gestión códigos
swatidhome/command/{serial}/security # Comandos seguridad
```

### Publicaciones

```
swatidhome/command/{serial}/access   # Solicitud validación
swatidhome/events/{serial}/access    # Eventos de acceso
swatidhome/events/{serial}/error     # Errores
swatidhome/events/{serial}/codes     # Eventos de códigos
swatidhome/events/{serial}/ble       # Eventos BLE
swatidhome/events/{serial}/relay     # Eventos de relé
```

---

## Requisitos de Integración

### Para Apps Móviles (BLE)

1. **Capacitor BLE Plugin** o equivalente nativo
2. Soporte para MTU negociación (recomendado 512)
3. Manejo de NOTIFY con acumulación y detección EOT
4. SHA-256 para challenge-response
5. Almacenamiento seguro de clave BLE

### Para Backend (MQTT)

1. **Broker MQTT 3.1.1** compatible
2. JSON parser
3. Timeout de respuesta configurable (recomendado 5s)
4. Manejo de reconexión del dispositivo

---

## Herramientas de Debug

### Monitor Serial

```bash
# Velocidad: 115200 baud
pio device monitor -b 115200
```

### Logs Típicos

```
🌐 ETH Dirección IP: 192.168.5.86
📡 Conectando a MQTT... ✅ Conectado
🔵 [BLE] Nuevo cliente conectado: AA:BB:CC:DD:EE:FF
🔵 [BLE] Challenge enviado: e7c338e2b3e1d000...
🔵 [BLE] Auth v4.1 - Token generado: 13BA189BFB5E1462
🔑 [TECLADO 1] Tecla detectada: #
✅ [WIEGAND1] Código válido LOCAL - Relé 1
🔓 Relé 1 ACTIVADO (2.0s)
```

---

## Contacto y Soporte

**Repositorio:** Privado  
**Rama:** v4.1  
**Compilación:** PlatformIO + ESP32 Arduino Framework

---

**Estado del Firmware:** ✅ ESTABLE - PRODUCCIÓN
