# Rama v3.0 - Documentación

## 📋 Información General

- **Rama**: v3.0
- **Versión Base**: 3.0.0
- **Fecha de Creación**: 11 de Diciembre, 2025
- **Estado**: Estable
- **Objetivo**: Corrección de bugs críticos y estabilización del sistema

---

## 🎯 Objetivo de la Rama

Esta rama consolida todas las correcciones y mejoras implementadas en la serie v2.5.x, estableciendo una base sólida y estable para futuras funcionalidades. El enfoque principal es la corrección de bugs críticos que afectaban la funcionalidad principal del dispositivo.

---

## 🔥 Cambios Principales

### 1. Corrección Crítica de Validación Remota
**Archivo**: `/src/main.ino`  
**Problema**: El dispositivo no procesaba correctamente los mensajes de validación remota, impidiendo la apertura de relés cuando el backend aprobaba accesos.

**Solución Implementada**:
- Corrección de la función `processRemoteValidationResponse()`
- Eliminación de código duplicado en callback MQTT
- Validación correcta de `message_id`
- Uso correcto de campos `relay_number` y `duration`

**Documentación Detallada**: `/docs/mejoras/correccion-mensaje-granted-v2.5.4.md`

---

## 📊 Estadísticas de la Versión

### Uso de Recursos
- **Flash**: 90.5% utilizado (1,185,949 / 1,310,720 bytes)
- **RAM**: 15.3% utilizado (50,180 / 327,680 bytes)
- **Tamaño del Firmware**: 1.13 MB

### Compatibilidad
- ✅ Backend v2.x
- ✅ Hardware KC868-A2
- ✅ ESP32 Dev Module
- ✅ Protocolo MQTT actual

---

## 🗂️ Estructura de Archivos

### Firmware
```
firmware/
├── SWATID-A2_v3.0.0.bin           # Firmware compilado
├── SWATID-A2_v3.0.0.sha256        # Hash de verificación
├── README_v3.0.0.md               # Documentación del firmware
├── RELEASE_NOTES_v3.0.0.md        # Notas de la versión
└── manifest_v3.0.0.json           # Manifest para OTA
```

### Documentación
```
docs/
├── v3.0/
│   ├── README.md                  # Este archivo
│   ├── contexto-desarrollo.md     # Contexto del desarrollo
│   └── pruebas.md                 # Documentación de pruebas
└── mejoras/
    └── correccion-mensaje-granted-v2.5.4.md  # Detalle de correcciones
```

---

## 🐛 Bugs Corregidos

### CRÍTICO
1. **[#SWAT-001] Validación remota no funcional**
   - Los relés no se abrían al aprobar acceso desde backend
   - Estado: ✅ RESUELTO

### ALTO
2. **[#SWAT-002] Error 7 en mensajes válidos**
   - Mensajes correctamente formateados devolvían error
   - Estado: ✅ RESUELTO

### MEDIO
3. **[#SWAT-003] message_id no validado correctamente**
   - No se verificaba correspondencia solicitud-respuesta
   - Estado: ✅ RESUELTO

---

## 🔄 Protocolo MQTT Actualizado

### Tópico: `/granted`

**Formato de Mensaje**:
```json
{
  "message_id": 17,
  "device": "SWATID_584614BBBC2C",
  "response": "APPROVED",
  "code_type": "PIN",
  "code_value": "333333",
  "duration": 2,
  "relay_number": 1,
  "reason": "Valid code"
}
```

**Campos Procesados**:
- `message_id`: Validación de correspondencia con solicitud
- `response`: "APPROVED" o "DENIED"
- `relay_number`: Relé a activar (1 o 2)
- `duration`: Tiempo de apertura en segundos
- `reason`: Motivo (informativo)

---

## ⚠️ Cambios Disruptivos

### Formato Legacy Deprecado
El formato antiguo con campo `access_granted` (boolean) **ya no está soportado**.

**NO usar**:
```json
{
  "access_granted": true,
  ...
}
```

**Usar**:
```json
{
  "response": "APPROVED",
  ...
}
```

---

## 🧪 Pruebas Realizadas

### Validación Remota
- ✅ Mensaje APPROVED abre relé correctamente
- ✅ Mensaje DENIED no abre relé
- ✅ Duration se respeta correctamente
- ✅ relay_number se procesa correctamente
- ✅ message_id se valida correctamente

### Regresión
- ✅ Autenticación local funciona
- ✅ Configuración WiFi funciona
- ✅ Conexión MQTT funciona
- ✅ Web interface funciona
- ✅ OTA funciona

**Documentación de Pruebas**: `/docs/v3.0/pruebas.md`

---

## 📦 Dependencias

### Librerías Principales
- ArduinoJson v6.21.5
- PubSubClient v2.8.0
- DNSServer, EEPROM, ESPmDNS v2.0.0
- Ethernet, HTTPClient v2.0.0
- Update, WebServer, WiFi v2.0.0

### Plataforma
- espressif32@5.4.0
- Arduino ESP32 v2.0.6

---

## 🚀 Instalación y Despliegue

### Compilación
```bash
cd "/Volumes/SWAT_WORK/01 - DESARROLLOS/01 - KC868A2/01 - CURSOR"
pio run
```

### Subida mediante USB
```bash
pio run --target upload --upload-port /dev/cu.usbserial-XXXX
```

### Actualización OTA
Configurar servidor OTA con `manifest_v3.0.0.json`

---

## 📈 Métricas de Desarrollo

- **Archivos Modificados**: 1 (`src/main.ino`)
- **Líneas Modificadas**: ~100 líneas
- **Bugs Críticos Resueltos**: 3
- **Mejoras de Logging**: Sí
- **Tests Realizados**: 10+ escenarios

---

## 🎯 Próximos Pasos (v3.1.0)

### Funcionalidades Planeadas
1. **Optimización de Flash**
   - Reducir uso del 90.5% actual
   - Liberar espacio para nuevas funcionalidades

2. **WiFi Directo**
   - Configuración sin AP intermedio
   - Gestión de múltiples redes

3. **Mejoras en OTA**
   - Proceso más robusto
   - Rollback automático en caso de fallo

4. **Gestión de Memoria**
   - Optimización de uso de RAM
   - Prevención de memory leaks

---

## 📞 Contacto y Soporte

Para reportar problemas o sugerencias relacionadas con esta rama:
1. Verificar formato de mensajes MQTT
2. Revisar logs del dispositivo
3. Consultar documentación en `/docs/v3.0/`
4. Reportar con logs completos

---

## 📚 Referencias

- [Manual Completo](/docs/Manual_Completo_Coms.md)
- [API MQTT](/docs/api/mqtt-api-reference.md)
- [Documentación de Hardware](/docs/hardware/pinout-diagram.md)
- [Procedimientos de Mantenimiento](/docs/processes/maintenance-procedures.md)

---

## 📝 Notas de Desarrollo

### Decisiones Técnicas
- Se optó por eliminar código legacy en lugar de mantener compatibilidad hacia atrás
- Se priorizó claridad del código sobre optimización prematura
- Se mejoró el logging para facilitar debugging futuro

### Lecciones Aprendidas
- Importancia de tests exhaustivos de integración
- Necesidad de logging detallado en operaciones críticas
- Valor de la documentación actualizada

---

## 🏷️ Tags y Versiones

- **v3.0.0**: Versión inicial de la rama
- **Estado**: Estable y lista para producción
- **Recomendación**: Actualizar todos los dispositivos

---

**Última Actualización**: 11 de Diciembre, 2025  
**Mantenedor**: Equipo SWAT ID  
**Rama Base**: main

