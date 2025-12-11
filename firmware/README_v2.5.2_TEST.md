# SWATID-A2 Firmware v2.5.2 - Versión de Prueba OTA

## 🧪 Información de la Versión de Prueba

- **Versión**: v2.5.2
- **Tipo**: Versión de prueba para actualización OTA
- **Fecha de compilación**: 2025-01-09
- **Tamaño del archivo**: 1,182,137 bytes
- **SHA256**: `7983f888f7077a378470da8180511795346e26a43b5b5b4e962af07847eb697b`
- **Dispositivo**: SWATID-A2 Controller - 2 x Wiegand

## 🎯 Propósito de esta Versión

Esta versión **v2.5.2** ha sido creada específicamente para **probar la funcionalidad de actualización OTA** del sistema. 

### ✅ **¿Qué cambia?**
- **Únicamente la versión visible** en el navegador web
- **Misma funcionalidad** que la versión v2.5.1
- **Mismo tamaño** y características técnicas

### 🔍 **¿Qué NO cambia?**
- Funcionalidades del sistema
- Interfaz de usuario
- Configuraciones
- Códigos almacenados
- Configuración de red

## 📦 Archivos Incluidos

- `SWATID-A2_v2.5.2_TEST.bin` - Firmware de prueba
- `SWATID-A2_v2.5.2_TEST.sha256` - Checksum de verificación
- `manifest_v2.5.2.json` - Metadatos de la versión de prueba
- `README_v2.5.2_TEST.md` - Este archivo de documentación

## 🧪 Proceso de Prueba OTA

### **Paso 1: Verificar Versión Actual**
1. Acceder a la interfaz web del dispositivo
2. Verificar que muestra **v2.5.1** en la página principal
3. Anotar la versión actual para confirmar el cambio

### **Paso 2: Realizar Actualización OTA**
1. Ir a la sección **"Configuración OTA"** (`/ota`)
2. Seleccionar el archivo `SWATID-A2_v2.5.2_TEST.bin`
3. Hacer clic en **"Subir y Actualizar"**
4. Confirmar la actualización cuando se solicite
5. Esperar a que se complete el proceso

### **Paso 3: Verificar Actualización**
1. Esperar a que el dispositivo se reinicie (3 segundos)
2. Acceder nuevamente a la interfaz web
3. **Verificar que la versión ahora muestra v2.5.2**
4. Confirmar que todas las funcionalidades siguen funcionando

## ✅ Criterios de Éxito

La prueba OTA se considera **exitosa** si:

- ✅ La actualización se completa sin errores
- ✅ El dispositivo se reinicia correctamente
- ✅ La versión visible cambia de **v2.5.1** a **v2.5.2**
- ✅ Todas las funcionalidades siguen operativas
- ✅ La configuración se mantiene intacta
- ✅ Los códigos almacenados se conservan

## 🔄 Rollback (Si es Necesario)

Si por alguna razón necesita volver a la versión anterior:

1. Usar el archivo `SWATID-A2_v2.5.1_OTA_FIXED.bin`
2. Seguir el mismo proceso de actualización OTA
3. La versión volverá a mostrar **v2.5.1**

## 📊 Información Técnica

### **Uso de Memoria:**
- **RAM**: 15.3% (50,180 bytes de 327,680 bytes)
- **Flash**: 90.2% (1,182,137 bytes de 1,310,720 bytes)

### **Compatibilidad:**
- **Hardware**: ESP32-D0WD-V3
- **Memoria Flash**: 4MB
- **Memoria RAM**: 320KB
- **Ethernet**: LAN8720
- **Puertos Wiegand**: 2

## 🚨 Notas Importantes

- ⚠️ **Esta es una versión de prueba** - no contiene nuevas funcionalidades
- ⚠️ **Solo cambia la versión visible** en el navegador
- ⚠️ **Ideal para verificar** que el sistema OTA funciona correctamente
- ⚠️ **Se puede usar en producción** sin problemas de funcionalidad

## 📞 Soporte

Para soporte técnico o reportar problemas:
- **Email**: soporte@swatid.com
- **Teléfono**: +34 633 44 84 27
- **Web**: https://www.swatid.com

## 📄 Licencia

Este firmware es propiedad de SWATID y está protegido por derechos de autor.
No se permite la redistribución sin autorización expresa.

---
**SWATID-A2 Controller v2.5.2** - Versión de prueba para actualización OTA
