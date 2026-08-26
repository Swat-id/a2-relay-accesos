# 🧪 Instrucciones para Probar la Actualización OTA

## 📋 Resumen de la Prueba

**Objetivo**: Verificar que la funcionalidad de actualización OTA funciona correctamente.

**Versión actual**: v2.5.1 (en la controladora)
**Versión de prueba**: v2.5.2 (archivo a subir)
**Cambio esperado**: Solo la versión visible en el navegador

## 🚀 Pasos para la Prueba

### **1. Verificar Estado Actual**
- Acceder a la interfaz web del dispositivo
- Verificar que muestra **"v2.5.1"** en la página principal
- Anotar la versión actual

### **2. Acceder a Configuración OTA**
- Ir a la URL: `http://[IP_DISPOSITIVO]/ota`
- Iniciar sesión con las credenciales de administrador
- Verificar que la página de configuración OTA se carga correctamente

### **3. Realizar Actualización**
- Hacer clic en **"Seleccionar archivo"**
- Seleccionar: `SWATID-A2_v2.5.2_TEST.bin`
- Hacer clic en **"Subir y Actualizar"**
- Confirmar la actualización cuando se solicite
- **Esperar** a que se complete el proceso (puede tomar 1-2 minutos)

### **4. Verificar Resultado**
- Esperar a que el dispositivo se reinicie (3 segundos)
- Acceder nuevamente a la interfaz web
- **Verificar que la versión ahora muestra "v2.5.2"**
- Confirmar que todas las funcionalidades siguen funcionando

## ✅ Criterios de Éxito

La prueba se considera **exitosa** si:

- ✅ La actualización se completa sin errores
- ✅ El dispositivo se reinicia correctamente
- ✅ La versión cambia de **v2.5.1** a **v2.5.2**
- ✅ Todas las funcionalidades siguen operativas
- ✅ La configuración se mantiene intacta

## ❌ Posibles Problemas y Soluciones

### **Error: "TypeError: Failed to fetch"**
- **Causa**: Problema de conexión o JavaScript
- **Solución**: Verificar que el dispositivo esté conectado y recargar la página

### **Error: "Archivo demasiado grande"**
- **Causa**: El archivo excede 4MB
- **Solución**: Verificar que se está usando el archivo correcto

### **Error: "Solo archivos .bin permitidos"**
- **Causa**: Archivo con extensión incorrecta
- **Solución**: Asegurarse de usar `SWATID-A2_v2.5.2_TEST.bin`

### **Actualización se cuelga**
- **Causa**: Problema de red o memoria
- **Solución**: Reiniciar el dispositivo y volver a intentar

## 🔄 Rollback (Si es Necesario)

Si necesita volver a la versión anterior:

1. Usar el archivo: `SWATID-A2_v2.5.1_OTA_FIXED.bin`
2. Seguir el mismo proceso de actualización
3. La versión volverá a **v2.5.1**

## 📊 Información de Archivos

### **Archivo de Prueba:**
- **Nombre**: `SWATID-A2_v2.5.2_TEST.bin`
- **Tamaño**: 1,182,137 bytes
- **SHA256**: `7983f888f7077a378470da8180511795346e26a43b5b5b4e962af07847eb697b`

### **Archivo de Rollback:**
- **Nombre**: `SWATID-A2_v2.5.1_OTA_FIXED.bin`
- **Tamaño**: 1,187,904 bytes
- **SHA256**: `a096a79c609e6b510bbdd3f866a69b29927e14b20b367a027bf728643cb41437`

## 📝 Notas Importantes

- ⚠️ **Esta es una versión de prueba** - no contiene nuevas funcionalidades
- ⚠️ **Solo cambia la versión visible** en el navegador
- ⚠️ **Ideal para verificar** que el sistema OTA funciona correctamente
- ⚠️ **Se puede usar en producción** sin problemas de funcionalidad

## 🎯 Resultado Esperado

Después de la actualización exitosa:

- **Antes**: Página web muestra "v2.5.1"
- **Después**: Página web muestra "v2.5.2"
- **Funcionalidad**: Todo sigue funcionando igual
- **Configuración**: Se mantiene intacta

---

**¡Listo para probar la actualización OTA!** 🚀
