# Release Notes - SWATID-A2 v4.0.0-BLE

**Fecha de lanzamiento:** 26 Febrero 2026

## Resumen

Esta versión introduce el **servidor BLE (Bluetooth Low Energy)** para configuración del dispositivo desde aplicaciones móviles, junto con un sistema completo de autenticación y gestión de usuarios vinculados.

## Nuevas Funcionalidades

### Servidor BLE

- **10 características BLE** para gestión completa del dispositivo
- Sistema de autenticación con claves de 64 bytes
- Soporte para **1 superadmin + 5 usuarios** vinculados
- Permisos granulares por usuario

### Características BLE Implementadas

| UUID | Función | Autenticación |
|------|---------|---------------|
| FF01 | Autenticación | No (es el login) |
| FF02 | Control de Relés | Sí |
| FF03 | Cambio de Modo | Sí |
| FF04 | Añadir Códigos | Sí |
| FF05 | Configuración de Red | Sí |
| FF06 | Tiempo de Relé | Sí |
| FF07 | Estado del Dispositivo | No |
| FF08 | Info para Provisión | No |
| FF09 | **Info Completa (IP, red, estado)** | Sí |
| FF0A | **Lista de Códigos Locales** | Sí |

### Provisión Automática (FF08)

La APP puede identificar automáticamente el tipo de dispositivo, versión de firmware y capacidades sin necesidad de autenticarse.

### Información Completa (FF09)

Nueva característica que devuelve en JSON:
- Información de red (IP real, gateway, DNS, MAC)
- Estado de relés y modo
- Contadores de códigos
- Estado MQTT
- Uptime

### Lista de Códigos (FF0A)

Permite obtener todos los códigos locales configurados:
- Tipo (PIN/TAG)
- Valor
- Teclado asignado
- Relé asociado
- Soporte de paginación (20 códigos por página)

## Mejoras

- Eventos MQTT para monitorizar desvinculaciones BLE
- Comportamiento unificado de relés desde BLE, Web y MQTT
- Documentación completa de protocolos en `/docs/v4.0/messages/`

## Requisitos

- **Hardware:** KC868-A2 con ESP32
- **Versión mínima para actualizar:** v3.0.0
- **Espacio requerido:** 1.44 MB

## Uso de Memoria

| Recurso | Uso |
|---------|-----|
| Flash | 74.7% (1,468,257 / 1,966,080 bytes) |
| RAM | 18.3% (59,904 / 327,680 bytes) |

## Archivos

| Archivo | Descripción |
|---------|-------------|
| `SWATID-A2_v4.0.0-BLE.bin` | Firmware compilado |
| `SWATID-A2_v4.0.0-BLE.sha256` | Checksum SHA256 |
| `manifest_v4.0.0-BLE.json` | Manifest para OTA |

## SHA256

```
92792ec9ef9dce1311468bc2be9ed7355bd70c9a2c256427082e6b4a339b9986
```

## Documentación

- [Protocolo BLE completo](../docs/v4.0/messages/ble-protocol.md)
- [Guía de lectura de configuración](../docs/v4.0/messages/ble-config-read.md)
- [Protocolo MQTT](../docs/v4.0/messages/mqtt-protocol.md)

## Notas de Actualización

1. La actualización desde v3.0.0 es directa vía OTA o upload manual
2. Las configuraciones existentes se mantienen
3. El servidor BLE se activa automáticamente
4. El nombre BLE será el número de serie del dispositivo

---

*SWAT-ID - Control de Accesos*
