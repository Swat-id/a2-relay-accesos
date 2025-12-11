# Gestión de Seguridad - Interfaz Web

## Descripción
Documentación completa del sistema de seguridad implementado en la interfaz web del KC868A2, incluyendo monitoreo, configuración y gestión de parámetros de seguridad.

## Panel de Estado de Seguridad

### 🔒 Indicadores de Estado

#### Acceso Local
- **Estado**: Permitido / Bloqueado
- **Indicador visual**: 
  - 🟢 Verde: Acceso permitido
  - 🔴 Rojo: Acceso bloqueado
- **Información adicional**: Tiempo restante si está bloqueado
- **Actualización**: Tiempo real

#### Intentos Fallidos
- **Formato**: `X/Y` (actual/máximo)
- **Rango**: 0-10 intentos
- **Reset automático**: Tras 5 minutos sin actividad
- **Bloqueo**: Automático al alcanzar el máximo

#### Duración de Bloqueo
- **Unidad**: Segundos
- **Rango**: 30-3600 segundos
- **Valor por defecto**: 60 segundos
- **Configuración**: Remota vía MQTT

#### Tiempo Restante
- **Visibilidad**: Solo cuando está bloqueado
- **Formato**: Segundos restantes
- **Actualización**: Cada segundo
- **Desbloqueo**: Automático al llegar a 0

### 📊 Tabla de Estado de Seguridad

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| Acceso Local | 🔓 Permitido / 🔒 BLOQUEADO | Estado actual del acceso |
| Intentos fallidos | X/Y | Contador actual vs máximo |
| Duración bloqueo | X segundos | Tiempo configurado de bloqueo |
| Tiempo restante | X segundos | Solo si está bloqueado |

## Configuración de Seguridad

### ⚙️ Parámetros Configurables

#### Máximo de Intentos Fallidos
- **Rango**: 1-10 intentos
- **Valor por defecto**: 3 intentos
- **Configuración**: Remota vía MQTT
- **Efecto**: Bloqueo automático al alcanzar

#### Duración del Bloqueo
- **Rango**: 30-3600 segundos
- **Valor por defecto**: 60 segundos
- **Configuración**: Remota vía MQTT
- **Efecto**: Tiempo de bloqueo tras intentos fallidos

#### Modo de Validación
- **Opciones**:
  - Local primero: Busca en EEPROM antes que MQTT
  - Remoto primero: Envía a MQTT antes que local
- **Configuración**: Interfaz web
- **Efecto**: Orden de validación de códigos

### 🔧 Comandos de Seguridad MQTT

#### Bloquear Acceso Local
```json
{
  "message_id": 123,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "block_local_access"
  }
}
```

#### Desbloquear Acceso Local
```json
{
  "message_id": 124,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "unblock_local_access"
  }
}
```

#### Configurar Duración de Bloqueo
```json
{
  "message_id": 125,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "set_block_duration",
    "duration_seconds": 120
  }
}
```

#### Configurar Máximo de Intentos
```json
{
  "message_id": 126,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "set_max_failed_attempts",
    "max_attempts": 5
  }
}
```

## Monitoreo de Seguridad

### 📈 Eventos de Seguridad

#### Accesos Exitosos
- **Fuente**: LOCAL, REMOTE, WEB, MQTT_RELAY
- **Información**: Código, tipo, teclado, timestamp
- **MQTT Topic**: `swatidhome/events/[SERIAL]/access`
- **Formato**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "message_id": 123,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "event_type": "ACCESS_EVENT",
  "success": true,
  "source": "LOCAL",
  "code_type": "PIN",
  "code_value": "1234",
  "keyboard_id": 1,
  "keyboard_name": "WIEGAND1"
}
```

#### Intentos Fallidos
- **Razones**: INVALID_CODE, BLOCKED, REMOTE_DENIED, MQTT_PUBLISH_FAILED
- **Información**: Código, tipo, teclado, razón, contadores
- **MQTT Topic**: `swatidhome/events/[SERIAL]/failed_access`
- **Formato**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "message_id": 124,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
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

#### Errores del Sistema
- **Códigos de error**: 1-9
- **MQTT Topic**: `swatidhome/errors/[SERIAL]/rx`
- **Formato**:
```json
{
  "timestamp": "2025-06-15T10:30:00+01:00",
  "message_id": 125,
  "device": "DeviceName",
  "serial": "SWATID_XXXXXXXX",
  "error_code": 6,
  "description": "Acceso bloqueado tras 3 intentos fallidos"
}
```

### 🔍 Códigos de Error de Seguridad

| Código | Descripción | Acción Requerida |
|--------|-------------|------------------|
| 1 | Error de conexión de red | Verificar conectividad |
| 2 | Error de configuración | Revisar parámetros |
| 3 | Mensaje de inicio/estado | Informativo |
| 4 | Cambio de configuración | Informativo |
| 5 | Sistema de seguridad | Informativo |
| 6 | Bloqueo de acceso activado | Desbloquear si necesario |
| 7 | Error de validación remota | Verificar MQTT |
| 8 | Memoria baja | Reiniciar sistema |
| 9 | MQTT desconectado prolongado | Verificar conectividad |

## Gestión de Códigos de Acceso

### 📋 Administración de Códigos

#### Tipos de Códigos
- **PIN**: Códigos numéricos de 4-6 dígitos
- **TAG**: Códigos de tarjetas RFID/NFC

#### Capacidad del Sistema
- **Máximo**: 500 códigos
- **Almacenamiento**: EEPROM
- **Persistencia**: Permanente
- **Backup**: Automático

#### Validación de Códigos
- **Local**: Búsqueda en EEPROM
- **Remota**: Envío a servidor MQTT
- **Fallback**: Local si remoto falla
- **Tiempo de respuesta**: < 1 segundo

### 🔄 Proceso de Validación

#### Flujo Local Primero
1. Usuario ingresa código
2. Búsqueda en EEPROM local
3. Si encontrado: Activar relé
4. Si no encontrado: Enviar a MQTT
5. Procesar respuesta remota

#### Flujo Remoto Primero
1. Usuario ingresa código
2. Enviar a servidor MQTT
3. Procesar respuesta remota
4. Si falla: Búsqueda local
5. Activar relé según resultado

## Configuración de Red y MQTT

### 🌐 Parámetros de Red
- **Ethernet**: PHY LAN8720
- **DHCP**: Configurable
- **IP estática**: Opcional
- **DNS**: Configurable

### 📡 Configuración MQTT
- **Broker**: 188.245.213.181:1883
- **Usuario**: swatidhome
- **Contraseña**: Swatid2025!
- **QoS**: 0 (at most once)
- **Keepalive**: 60 segundos

### 🔐 Autenticación Web
- **Método**: HTTP Basic Authentication
- **Usuario**: admin
- **Contraseña**: Configurable (4-31 caracteres)
- **Sesión**: Persistente
- **Cambio**: Desde interfaz web

## Logs y Auditoría

### 📝 Registro de Eventos
- **Accesos**: Todos los intentos registrados
- **Configuración**: Cambios auditados
- **Errores**: Capturados y reportados
- **Seguridad**: Eventos de bloqueo/desbloqueo

### 🔍 Información de Auditoría
- **Timestamp**: ISO 8601 con timezone
- **Usuario**: Identificación del origen
- **Acción**: Descripción de la operación
- **Resultado**: Éxito o fallo
- **Detalles**: Información adicional

### 📊 Monitoreo en Tiempo Real
- **Estado del sistema**: Actualizado cada 30 segundos
- **Conectividad**: Verificación continua
- **Memoria**: Monitoreo de uso
- **MQTT**: Estado de conexión

## Procedimientos de Emergencia

### 🚨 Bloqueo de Emergencia
1. Acceder vía MQTT
2. Enviar comando `block_local_access`
3. Confirmar bloqueo en interfaz web
4. Monitorear estado

### 🔓 Desbloqueo de Emergencia
1. Acceder vía MQTT
2. Enviar comando `unblock_local_access`
3. Confirmar desbloqueo en interfaz web
4. Verificar funcionamiento

### 🔄 Reset de Seguridad
1. Acceder a interfaz web
2. Ir a página de reset
3. Confirmar restauración
4. Reconfigurar parámetros

## Mejores Prácticas

### 🛡️ Configuración Segura
- **Cambiar contraseña**: Inmediatamente tras instalación
- **Configurar red**: IP estática para producción
- **Monitorear logs**: Revisar eventos regularmente
- **Backup de códigos**: Mantener copia de seguridad

### 🔧 Mantenimiento
- **Revisar estado**: Diariamente
- **Actualizar códigos**: Según necesidades
- **Monitorear memoria**: Verificar uso
- **Verificar conectividad**: MQTT y red

### 📊 Monitoreo
- **Eventos de seguridad**: Alertas automáticas
- **Estado del sistema**: Dashboard en tiempo real
- **Conectividad**: Verificación continua
- **Rendimiento**: Métricas de sistema

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
