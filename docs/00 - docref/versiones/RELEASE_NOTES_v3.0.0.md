# Release Notes - SWATID-A2 v3.0.0

**Fecha de Lanzamiento**: 11 de Diciembre, 2025

## 🎉 Nueva Versión Mayor - v3.0.0

Esta es una versión mayor que consolida todas las correcciones y mejoras implementadas en la serie v2.5.x, estableciendo una base sólida para futuras funcionalidades.

---

## 🔥 Cambios Críticos

### ✅ Corrección del Procesamiento de Validación Remota (Bug Crítico)

**Impacto**: CRÍTICO - Funcionalidad principal no operativa  
**Afecta a**: Todos los dispositivos con validación remota habilitada  
**Estado**: ✅ RESUELTO

#### Descripción del Problema
El dispositivo no procesaba correctamente los mensajes de respuesta de validación remota enviados al tópico `swatidhome/command/[SERIAL]/granted`, lo que impedía la apertura del relé cuando el backend aprobaba un acceso.

#### Síntomas Previos
- ❌ No abría el relé cuando el backend aprobaba el acceso
- ❌ Devolvía error_code 7: "Mensaje de validación remota inválido recibido"
- ❌ No registraba el acceso como exitoso
- ❌ No procesaba la información de duración y relé

#### Solución Implementada
1. **Corrección de búsqueda de campos**
   - Los campos ahora se extraen correctamente del nivel raíz del JSON
   - Se utilizan los campos `relay_number` y `duration` proporcionados en el mensaje
   - Se eliminó la lógica incorrecta que calculaba el relé basándose en `keyboard_id`

2. **Eliminación de código duplicado**
   - Removido el bloque de código antiguo que procesaba formato legacy (líneas 597-655)
   - Mantenido solo el código moderno que procesa `response: "APPROVED"`
   - Resuelto el conflicto que causaba que el mensaje nunca llegara al procesador correcto

3. **Mejoras en la lógica de validación**
   - Validación correcta de `message_id` entre solicitud y respuesta
   - Mejor manejo de errores y logging
   - Mensajes de debug más informativos

---

## 🆕 Nuevas Funcionalidades

### Ninguna
Esta versión se centra en la corrección de bugs críticos y estabilización del código existente.

---

## 🔧 Mejoras

### Logging y Debugging
- ✅ Mensajes de log más detallados en el procesamiento de validación remota
- ✅ Visualización de valores extraídos del mensaje para facilitar debug
- ✅ Mejor identificación de errores en el procesamiento MQTT

### Calidad del Código
- ✅ Eliminación de código duplicado en callback MQTT
- ✅ Simplificación de la lógica de procesamiento de mensajes
- ✅ Mejor organización del código de validación remota

---

## 🐛 Bugs Corregidos

### Alta Prioridad
1. **[CRÍTICO] Validación remota no funcional** (#SWAT-001)
   - Los relés no se abrían al aprobar acceso desde backend
   - Causado por búsqueda incorrecta de campos en JSON
   - **Solución**: Corrección completa de `processRemoteValidationResponse()`

2. **[ALTO] Error 7 en mensajes válidos** (#SWAT-002)
   - Mensajes correctamente formateados devolvían error
   - Causado por código duplicado procesando el mismo tópico
   - **Solución**: Eliminación de código legacy duplicado

3. **[MEDIO] message_id no validado correctamente** (#SWAT-003)
   - No se verificaba que la respuesta correspondiera a la solicitud
   - Podría causar aperturas por respuestas antiguas
   - **Solución**: Validación correcta de message_id

---

## 📊 Estadísticas Técnicas

### Uso de Recursos
- **Flash**: 90.5% (1,185,949 / 1,310,720 bytes)
- **RAM**: 15.3% (50,180 / 327,680 bytes)
- **Librerías**: 10 dependencias principales

### Compatibilidad
- ✅ Compatible con backend v2.x
- ✅ Compatible con protocolo MQTT actual
- ✅ Compatible con hardware KC868-A2
- ✅ Compatible con ESP32 Dev Module

---

## 🔄 Cambios de API

### Ningún Cambio Disruptivo
Esta versión mantiene compatibilidad total con versiones anteriores en cuanto a API.

### Formato de Mensaje `/granted` (Actualizado)
El dispositivo ahora procesa correctamente el formato moderno:

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

**Campos utilizados**:
- `message_id`: Para validar que la respuesta corresponde a la solicitud
- `response`: "APPROVED" o "DENIED"
- `relay_number`: Número del relé a abrir (1 o 2)
- `duration`: Duración en segundos para mantener el relé abierto
- `reason`: Motivo de la decisión (solo informativo)

---

## ⚠️ Deprecaciones

### Formato Legacy de `/granted`
El formato antiguo con campo `access_granted` (boolean) ya no está soportado.

**Formato deprecado** (ya no funciona):
```json
{
  "access_granted": true,
  ...
}
```

**Usar en su lugar**:
```json
{
  "response": "APPROVED",
  ...
}
```

---

## 📦 Archivos Incluidos

- `SWATID-A2_v3.0.0.bin` - Firmware principal
- `SWATID-A2_v3.0.0.sha256` - Hash de verificación
- `README_v3.0.0.md` - Documentación de la versión
- `RELEASE_NOTES_v3.0.0.md` - Este archivo

---

## 🔐 Verificación de Integridad

```bash
SHA256: 51c136fd184ac9b09eb95f299d6eed4b8858b2950928270cd01d371488caa469
```

Para verificar:
```bash
shasum -a 256 SWATID-A2_v3.0.0.bin
```

---

## 📋 Checklist de Actualización

Antes de actualizar, asegúrate de:
- [ ] Hacer backup de la configuración actual
- [ ] Verificar que el backend soporta el formato de mensaje correcto
- [ ] Tener acceso físico al dispositivo en caso de problemas
- [ ] Documentar la configuración MQTT actual
- [ ] Probar primero en un dispositivo de desarrollo

---

## 🚀 Instrucciones de Actualización

### Opción 1: OTA (Recomendado)
1. Configurar manifest.json en servidor OTA
2. El dispositivo descargará automáticamente
3. Verificar funcionamiento tras actualización

### Opción 2: Cable USB
```bash
cd /path/to/project
pio run --target upload --upload-port /dev/cu.usbserial-XXXX
```

---

## 🧪 Pruebas Realizadas

### Pruebas de Validación Remota
- ✅ Mensaje APPROVED abre relé correctamente
- ✅ Mensaje DENIED no abre relé
- ✅ Duration se respeta correctamente
- ✅ relay_number se procesa correctamente
- ✅ message_id se valida correctamente

### Pruebas de Regresión
- ✅ Autenticación local funciona
- ✅ Configuración WiFi funciona
- ✅ Conexión MQTT funciona
- ✅ Web interface funciona
- ✅ OTA funciona

---

## 📞 Soporte

Si encuentras algún problema con esta versión:
1. Verifica el formato de los mensajes MQTT
2. Revisa los logs del dispositivo
3. Consulta la documentación en `/docs`
4. Reporta issues con logs completos

---

## 🎯 Próximos Pasos (v3.1.0)

Funcionalidades planeadas para la próxima versión:
- Optimización de uso de Flash
- Mejoras en el proceso de OTA
- Configuración WiFi directa sin AP
- Gestión mejorada de múltiples redes WiFi

---

## 👥 Créditos

- **Desarrollo**: Equipo SWAT ID
- **Fecha de Compilación**: 11 de Diciembre, 2025
- **Basado en**: Correcciones acumuladas de v2.5.x

---

## 📄 Licencia

[Incluir información de licencia según corresponda]

