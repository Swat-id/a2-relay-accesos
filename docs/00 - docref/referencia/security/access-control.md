# Control de Acceso - KC868A2

## Descripción
Sistema de control de acceso implementado en el KC868A2, incluyendo validación de códigos, gestión de permisos y procedimientos de seguridad.

## Tipos de Códigos

### 🔢 Códigos PIN
- **Formato**: 4-6 dígitos numéricos
- **Ejemplos**: 1234, 567890
- **Validación**: Solo dígitos 0-9
- **Almacenamiento**: EEPROM

### 🏷️ Códigos TAG
- **Formato**: 1-16 caracteres
- **Ejemplos**: 12345678, ABCD1234
- **Validación**: Cualquier carácter imprimible
- **Almacenamiento**: EEPROM

## Sistema de Validación

### 🔍 Modos de Validación
- **Local Primero**: Busca en EEPROM antes que MQTT
- **Remoto Primero**: Envía a MQTT antes que local
- **Fallback**: Local si MQTT falla

### 📊 Capacidad
- **Máximo**: 500 códigos
- **Almacenamiento**: EEPROM 4KB
- **Persistencia**: Permanente
- **Backup**: Automático

## Gestión de Seguridad

### 🔒 Bloqueo de Acceso
- **Activación**: Tras intentos fallidos
- **Duración**: Configurable (30-3600 segundos)
- **Desbloqueo**: Automático o remoto
- **Notificación**: MQTT y logs

### 📈 Monitoreo
- **Intentos fallidos**: Contador en tiempo real
- **Eventos**: Registro completo
- **Alertas**: Notificaciones automáticas
- **Auditoría**: Historial detallado

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
