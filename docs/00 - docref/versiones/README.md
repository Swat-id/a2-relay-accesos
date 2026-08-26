# SWATID-A2 Firmware v2.5.1

## 📋 Información del Firmware

- **Versión**: v2.5.1
- **Fecha de compilación**: 2025-01-09
- **Tamaño del archivo**: 1,178,829 bytes
- **SHA256**: `22e01f17a1ba236b921d1c0238beaf7f42538fdf50ad70d9172946423f619959`
- **Dispositivo**: SWATID-A2 Controller - 2 x Wiegand

## 🆕 Nuevas Características v2.5.1

### 🕐 Sincronización de Tiempo Mejorada
- **Interfaz web mejorada** para sincronización de tiempo
- **Visualización en tiempo real** de la hora del dispositivo y del navegador
- **Proceso simplificado** con botones separados para sincronizar y guardar
- **JavaScript integrado** para obtener la hora del navegador local
- **Confirmación visual** mejorada del proceso de sincronización

### 🔧 Optimizaciones Técnicas
- **Gestión de memoria optimizada** con punteros dinámicos
- **Reducción del uso de stack** para evitar stack overflow
- **Mejor estabilidad** del sistema
- **Interfaz responsive** mejorada

## 📦 Archivos Incluidos

- `SWATID-A2_v2.5.1.bin` - Firmware principal
- `manifest.json` - Metadatos del firmware
- `README.md` - Este archivo de documentación

## 🚀 Instalación

### Método 1: Actualización OTA (Recomendado)
1. Acceder a la interfaz web del dispositivo
2. Ir a la sección "Configuración OTA"
3. Subir el archivo `SWATID-A2_v2.5.1.bin`
4. Confirmar la actualización

### Método 2: Cable USB
1. Conectar el dispositivo via USB
2. Usar PlatformIO o Arduino IDE
3. Cargar el firmware desde el archivo `.bin`

## ⚠️ Requisitos Previos

- **Versión mínima**: v2.0.0
- **Hardware compatible**: ESP32-D0WD-V3
- **Memoria Flash**: 4MB mínimo
- **Memoria RAM**: 320KB

## 🔄 Proceso de Sincronización de Tiempo

### Nuevo Proceso Simplificado:
1. **Acceder** a la sección "Sincronización de Tiempo" en la web
2. **Visualizar** la hora actual del dispositivo y del navegador
3. **Hacer clic** en "Sincronizar con Navegador"
4. **Confirmar** la hora mostrada
5. **Hacer clic** en "Guardar Cambios"
6. **Verificar** la confirmación de actualización

### Características de la Nueva Interfaz:
- ✅ **Hora del dispositivo** mostrada en tiempo real
- ✅ **Hora del navegador** actualizada cada segundo
- ✅ **Zona horaria** detectada automáticamente
- ✅ **Estado de sincronización** claramente indicado
- ✅ **Proceso en dos pasos** para mayor seguridad

## 🛠️ Características Técnicas

### Gestión de Memoria Optimizada:
- Uso de punteros dinámicos para estructuras grandes
- Reducción del uso de stack memory
- Verificaciones de nullptr para mayor estabilidad
- Gestión eficiente de EEPROM

### Interfaz Web Mejorada:
- Diseño responsive para móviles y desktop
- JavaScript moderno para funcionalidad en tiempo real
- Confirmaciones visuales claras
- Navegación intuitiva

## 📊 Uso de Memoria

- **RAM**: 15.3% (50,180 bytes de 327,680 bytes)
- **Flash**: 89.9% (1,178,829 bytes de 1,310,720 bytes)

## 🔧 Solución de Problemas

### Si la sincronización de tiempo no funciona:
1. Verificar que el navegador tenga JavaScript habilitado
2. Comprobar la zona horaria del navegador
3. Asegurarse de que la hora del sistema del navegador sea correcta

### Si hay problemas de memoria:
1. Reiniciar el dispositivo
2. Verificar que no haya códigos corruptos en EEPROM
3. Limpiar códigos innecesarios si es necesario

## 📞 Soporte

Para soporte técnico o reportar problemas:
- **Email**: soporte@swatid.com
- **Teléfono**: +34 633 44 84 27
- **Web**: https://www.swatid.com

## 📄 Licencia

Este firmware es propiedad de SWATID y está protegido por derechos de autor.
No se permite la redistribución sin autorización expresa.

---
**SWATID-A2 Controller v2.5.1** - Controlador de acceso dual Wiegand