# Procedimientos de Mantenimiento - KC868A2

## Descripción
Documentación completa de los procedimientos de mantenimiento del sistema KC868A2, incluyendo tareas preventivas, correctivas, monitoreo y resolución de problemas.

## Mantenimiento Preventivo

### 📅 Tareas Diarias

#### Verificación de Estado del Sistema
```bash
# Acceso a la interfaz web
curl -u admin:admin http://[IP_DISPOSITIVO]/

# Verificación de conectividad MQTT
mosquitto_sub -h 188.245.213.181 -p 1883 -u swatidhome -P Swatid2025! -t "swatidhome/keepalive/+/#"
```

#### Monitoreo de Logs
- **Serial Monitor**: Verificación de errores
- **MQTT**: Estado de conexión
- **Web**: Accesibilidad
- **Memoria**: Uso de heap

#### Verificación de Hardware
- **Teclados**: Respuesta a entrada
- **Relés**: Activación correcta
- **Ethernet**: Conectividad estable
- **Alimentación**: Voltaje estable

### 📅 Tareas Semanales

#### Limpieza de Códigos
- **Revisión**: Códigos no utilizados
- **Eliminación**: Códigos obsoletos
- **Optimización**: Espacio disponible
- **Backup**: Respaldo de códigos

#### Verificación de Seguridad
- **Intentos fallidos**: Revisión de logs
- **Bloqueos**: Verificación de activación
- **Configuración**: Parámetros de seguridad
- **Accesos**: Patrones de uso

#### Actualización de Configuración
- **Contraseñas**: Rotación si es necesario
- **Parámetros**: Ajustes según necesidades
- **Red**: Verificación de configuración
- **MQTT**: Estado de conexión

### 📅 Tareas Mensuales

#### Mantenimiento de Hardware
- **Limpieza**: Teclados y conectores
- **Verificación**: Conexiones firmes
- **Inspección**: Desgaste físico
- **Calibración**: Si es necesario

#### Análisis de Rendimiento
- **Memoria**: Uso y tendencias
- **Conectividad**: Estabilidad de red
- **MQTT**: Latencia y pérdida
- **Web**: Tiempo de respuesta

#### Backup de Configuración
- **EEPROM**: Respaldo completo
- **Códigos**: Exportación
- **Configuración**: Parámetros del sistema
- **Logs**: Historial de eventos

## Mantenimiento Correctivo

### 🔧 Resolución de Problemas Comunes

#### Problema: Dispositivo no conecta a Ethernet

**Síntomas**:
- LED de Ethernet apagado
- No se obtiene IP
- Modo AP activado

**Diagnóstico**:
```bash
# Verificar cable de red
ping [IP_DISPOSITIVO]

# Verificar configuración DHCP
nmap -sn 192.168.1.0/24

# Verificar logs del dispositivo
# Serial Monitor: "❌ Error: No se pudo obtener IP por DHCP"
```

**Soluciones**:
1. **Verificar cable**: Conexión física
2. **Configurar IP estática**: Si DHCP falla
3. **Reiniciar router**: Problemas de red
4. **Verificar PHY**: Hardware del dispositivo

**Comando de configuración IP estática**:
```json
{
  "message_id": 123,
  "device": "SWATID_XXXXXXXX",
  "message_type": 2,
  "message_info": {
    "device_name": "DeviceName",
    "use_dhcp": false,
    "static_ip": "192.168.1.100",
    "static_gateway": "192.168.1.1",
    "static_subnet": "255.255.255.0",
    "static_dns": "8.8.8.8"
  }
}
```

#### Problema: Teclados Wiegand no responden

**Síntomas**:
- No se detectan teclas
- No se leen tarjetas
- Sin respuesta a entrada

**Diagnóstico**:
```bash
# Verificar logs del dispositivo
# Serial Monitor: "🔐 [TECLADO X] Tecla detectada: Y"

# Verificar alimentación del teclado
# Voltaje: 12V DC
# Corriente: Suficiente para el teclado
```

**Soluciones**:
1. **Verificar alimentación**: 12V DC estable
2. **Comprobar conexiones**: D0/D1 correctos
3. **Verificar pines**: GPIO 33/14 y 4/16
4. **Reemplazar teclado**: Si es necesario

**Verificación de pines**:
- **Teclado 1**: GPIO 33 (D0), GPIO 14 (D1)
- **Teclado 2**: GPIO 4 (D0), GPIO 16 (D1)
- **Pull-up**: Activado internamente
- **Interrupciones**: Configuradas correctamente

#### Problema: MQTT desconectado

**Síntomas**:
- Sin comunicación remota
- Validación remota falla
- Logs de error MQTT

**Diagnóstico**:
```bash
# Verificar conectividad al broker
ping 188.245.213.181

# Verificar puerto MQTT
telnet 188.245.213.181 1883

# Verificar credenciales
mosquitto_pub -h 188.245.213.181 -p 1883 -u swatidhome -P Swatid2025! -t "test" -m "test"
```

**Soluciones**:
1. **Verificar red**: Conectividad a internet
2. **Verificar broker**: Estado del servidor
3. **Verificar credenciales**: Usuario/contraseña
4. **Reiniciar dispositivo**: Si es necesario

**Comando de reinicio**:
```json
{
  "message_id": 124,
  "device": "SWATID_XXXXXXXX",
  "message_type": 2,
  "message_info": "reboot"
}
```

#### Problema: Memoria baja

**Síntomas**:
- Errores de memoria
- Comportamiento errático
- Reinicios automáticos

**Diagnóstico**:
```bash
# Verificar logs del dispositivo
# Serial Monitor: "⚠️ ADVERTENCIA: Memoria baja: X bytes"

# Verificar uso de memoria
# Heap libre: < 10KB
# Heap mínimo: < 5KB
```

**Soluciones**:
1. **Reiniciar dispositivo**: Liberar memoria
2. **Reducir códigos**: Eliminar códigos no utilizados
3. **Verificar configuración**: Parámetros optimizados
4. **Actualizar firmware**: Si es necesario

### 🚨 Procedimientos de Emergencia

#### Bloqueo de Acceso Local

**Situación**: Acceso local bloqueado por intentos fallidos

**Solución**:
```json
{
  "message_id": 125,
  "device": "SWATID_XXXXXXXX",
  "message_type": 3,
  "message_info": {
    "security_command": "unblock_local_access"
  }
}
```

**Verificación**:
- Estado de bloqueo: Desbloqueado
- Intentos fallidos: 0
- Acceso local: Permitido

#### Reset de Configuración

**Situación**: Configuración corrupta o problemas graves

**Solución**:
```json
{
  "message_id": 126,
  "device": "SWATID_XXXXXXXX",
  "message_type": 2,
  "message_info": "reset"
}
```

**Efectos**:
- Configuración restaurada a valores por defecto
- Códigos eliminados
- Reinicio automático
- Acceso web: admin/admin

#### Reinicio de Emergencia

**Situación**: Sistema no responde

**Solución**:
1. **Desconectar alimentación**: 10 segundos
2. **Reconectar**: Reinicio completo
3. **Verificar**: Estado del sistema
4. **Configurar**: Si es necesario

## Monitoreo y Alertas

### 📊 Métricas de Monitoreo

#### Conectividad
- **Ethernet**: Estado de conexión
- **MQTT**: Estado de conexión
- **Web**: Accesibilidad
- **Latencia**: Tiempo de respuesta

#### Hardware
- **Teclados**: Estado operativo
- **Relés**: Funcionamiento
- **Memoria**: Uso de heap
- **Temperatura**: Monitoreo

#### Seguridad
- **Intentos fallidos**: Contador
- **Bloqueos**: Estado activo
- **Accesos**: Eventos por hora
- **Errores**: Códigos y frecuencia

### 🔔 Sistema de Alertas

#### Alertas Críticas
- **MQTT desconectado**: > 5 minutos
- **Memoria baja**: < 10KB
- **Bloqueo activo**: Acceso local bloqueado
- **Hardware fallido**: Teclados o relés

#### Alertas de Advertencia
- **Intentos fallidos**: > 50% del máximo
- **Latencia alta**: > 1 segundo
- **Errores frecuentes**: > 10 por hora
- **Conectividad inestable**: Reconexiones frecuentes

#### Configuración de Alertas
```python
# Ejemplo de configuración de alertas
alerts = {
    "mqtt_disconnected": {
        "threshold": 300,  # 5 minutos
        "action": "email",
        "recipients": ["admin@example.com"]
    },
    "memory_low": {
        "threshold": 10000,  # 10KB
        "action": "restart",
        "delay": 60  # 1 minuto
    },
    "access_blocked": {
        "threshold": 1,  # 1 bloqueo
        "action": "notification",
        "immediate": True
    }
}
```

## Backup y Restauración

### 💾 Procedimientos de Backup

#### Backup de Configuración
```python
import json
import paho.mqtt.client as mqtt

def backup_configuration(device_serial):
    client = mqtt.Client()
    client.username_pw_set("swatidhome", "Swatid2025!")
    client.connect("188.245.213.181", 1883, 60)
    
    # Solicitar información del dispositivo
    message = {
        "message_id": int(time.time()),
        "device": device_serial,
        "message_type": 1
    }
    
    topic = f"swatidhome/command/{device_serial}/info"
    client.publish(topic, json.dumps(message))
    
    # Esperar respuesta y guardar
    # Implementar callback para recibir respuesta
    
    client.disconnect()
```

#### Backup de Códigos
```python
def backup_codes(device_serial):
    # Obtener información del dispositivo
    device_info = get_device_info(device_serial)
    
    # Extraer códigos
    codes = device_info.get("stored_codes", [])
    
    # Guardar en archivo
    with open(f"backup_codes_{device_serial}.json", "w") as f:
        json.dump(codes, f, indent=2)
    
    print(f"Backup de {len(codes)} códigos guardado")
```

### 🔄 Procedimientos de Restauración

#### Restauración de Configuración
```python
def restore_configuration(device_serial, config_file):
    with open(config_file, "r") as f:
        config = json.load(f)
    
    client = mqtt.Client()
    client.username_pw_set("swatidhome", "Swatid2025!")
    client.connect("188.245.213.181", 1883, 60)
    
    # Enviar configuración
    message = {
        "message_id": int(time.time()),
        "device": device_serial,
        "message_type": 2,
        "message_info": config
    }
    
    topic = f"swatidhome/command/{device_serial}/system"
    client.publish(topic, json.dumps(message))
    
    client.disconnect()
```

#### Restauración de Códigos
```python
def restore_codes(device_serial, codes_file):
    with open(codes_file, "r") as f:
        codes = json.load(f)
    
    # Eliminar códigos existentes
    clear_all_codes(device_serial)
    
    # Añadir códigos del backup
    for code in codes:
        add_code(device_serial, code["type"], code["value"], code["relay"])
    
    print(f"Restaurados {len(codes)} códigos")
```

## Actualizaciones y Mejoras

### 🔄 Actualización de Firmware

#### Preparación
1. **Backup**: Configuración y códigos
2. **Verificación**: Compatibilidad de versión
3. **Preparación**: Herramientas necesarias
4. **Planificación**: Ventana de mantenimiento

#### Procedimiento
1. **Descarga**: Firmware actualizado
2. **Verificación**: Checksum del archivo
3. **Carga**: Via Arduino IDE
4. **Verificación**: Funcionamiento correcto
5. **Restauración**: Configuración y códigos

#### Post-actualización
1. **Verificación**: Todas las funcionalidades
2. **Configuración**: Parámetros restaurados
3. **Pruebas**: Teclados y relés
4. **Monitoreo**: Estado del sistema

### 📈 Optimizaciones

#### Rendimiento
- **Memoria**: Optimización de uso
- **Red**: Reducción de latencia
- **MQTT**: Mejora de throughput
- **Web**: Optimización de respuesta

#### Seguridad
- **Autenticación**: Fortalecimiento
- **Encriptación**: Mejoras de seguridad
- **Auditoría**: Logging mejorado
- **Monitoreo**: Alertas avanzadas

## Documentación de Mantenimiento

### 📝 Registros de Mantenimiento

#### Formato de Registro
```json
{
  "date": "2025-06-15",
  "time": "10:30:00",
  "technician": "Admin",
  "device": "SWATID_12345678",
  "type": "preventive",
  "tasks": [
    "Verificación de estado",
    "Limpieza de códigos",
    "Verificación de seguridad"
  ],
  "issues_found": [],
  "actions_taken": [
    "Eliminados 5 códigos obsoletos",
    "Verificado funcionamiento de teclados"
  ],
  "next_maintenance": "2025-06-22"
}
```

#### Historial de Mantenimiento
- **Fecha**: Timestamp de la tarea
- **Técnico**: Responsable del mantenimiento
- **Tipo**: Preventivo, correctivo, emergencia
- **Tareas**: Lista de actividades realizadas
- **Problemas**: Issues encontrados
- **Acciones**: Soluciones implementadas
- **Próximo**: Fecha del siguiente mantenimiento

### 📊 Reportes de Mantenimiento

#### Reporte Semanal
- **Estado general**: Resumen del sistema
- **Problemas encontrados**: Issues identificados
- **Acciones realizadas**: Soluciones implementadas
- **Métricas**: Estadísticas de rendimiento
- **Recomendaciones**: Mejoras sugeridas

#### Reporte Mensual
- **Análisis de tendencias**: Patrones identificados
- **Eficiencia**: Métricas de rendimiento
- **Costos**: Análisis de mantenimiento
- **Planificación**: Tareas futuras
- **Mejoras**: Optimizaciones implementadas

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
