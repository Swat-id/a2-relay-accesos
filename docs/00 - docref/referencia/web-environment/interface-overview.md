# Interfaz Web - Visión General

## Descripción
La interfaz web del KC868A2 proporciona una consola administrativa completa para la gestión del sistema de control de acceso dual Wiegand. Está diseñada con un enfoque moderno, responsivo y fácil de usar.

## Características de la Interfaz

### 🎨 Diseño y Usabilidad
- **Responsive Design**: Adaptable a dispositivos móviles y desktop
- **Iconografía**: Font Awesome 6.5.0 para iconos intuitivos
- **Tema**: Colores profesionales con esquema azul/gris
- **Navegación**: Estructura clara y lógica
- **Accesibilidad**: Contraste adecuado y elementos claramente identificables

### 🔐 Autenticación
- **Método**: HTTP Basic Authentication
- **Usuario por defecto**: `admin`
- **Contraseña por defecto**: `admin`
- **Seguridad**: Cambio de contraseña obligatorio en primera configuración
- **Sesión**: Persistente durante la navegación

## Estructura de la Interfaz

### 📊 Panel Principal (`/`)
La página principal proporciona una vista completa del estado del sistema:

#### Header
- **Título**: "KC868-A2 Dual Wiegand Controller"
- **Información del dispositivo**: Nombre y serial fijo
- **IP del dispositivo**: Dirección de red actual
- **Versión del firmware**: 1.7.0-COMPLETE-SECURITY

#### Estado de Seguridad
- **Indicador visual**: Código de colores (verde/amarillo/rojo)
- **Acceso local**: Estado actual (permitido/bloqueado)
- **Intentos fallidos**: Contador actual vs máximo
- **Duración de bloqueo**: Tiempo configurado
- **Tiempo restante**: Si está bloqueado

#### Estado de Teclados Duales
- **Teclado 1**: GPIO 33/14 - Estado operativo
- **Teclado 2**: GPIO 4/16 - Estado operativo
- **Funcionalidad**: Ambos teclados operativos simultáneamente
- **Información**: Pines GPIO y función asignada

#### Configuración del Dispositivo
- **Nombre del dispositivo**: Editable por el usuario
- **Serial MQTT**: Fijo, no modificable
- **Duración del relé**: Configurable en segundos
- **Configuración de red**: DHCP/IP estática
- **Campos de IP estática**: Aparecen/ocultan según configuración

#### Cambio de Contraseña
- **Formulario dedicado**: Para actualizar credenciales web
- **Validación**: Longitud mínima 4 caracteres, máxima 31
- **Confirmación**: Actualización inmediata

#### Acciones Rápidas
- **Activación de relés**: Botones directos para Relé 1 y 2
- **Gestión de códigos**: Acceso directo al módulo de códigos
- **Reinicio**: Reinicio del sistema
- **Reset**: Restauración a valores por defecto

#### Historial de Accesos
- **Último acceso**: Tipo, código, hora y teclado utilizado
- **Información detallada**: Para auditoría y monitoreo

#### Estado de Conexión
- **Red Ethernet**: Estado de conectividad
- **Servidor MQTT**: Estado de conexión
- **Códigos almacenados**: Contador actual vs máximo
- **Modo de validación**: Local primero o remoto primero

### 🔧 Página de Gestión de Códigos (`/codes`)
Interfaz especializada para la administración de códigos de acceso:

#### Información de Seguridad
- **Estado actual**: Bloqueo y contadores
- **Configuración**: Parámetros de seguridad activos

#### Información del Sistema Dual
- **Teclados activos**: 2 teclados simultáneos
- **Capacidad**: 500 códigos máximo
- **Serial MQTT**: Identificación única

#### Modo de Validación
- **Selector**: Local primero vs Remoto primero
- **Guardado**: Actualización inmediata de configuración

#### Añadir Códigos
- **Tipo**: PIN (4-6 dígitos) o TAG (RFID/NFC)
- **Código**: Valor del código de acceso
- **Relé**: Selección del relé a activar (1 o 2)
- **Validación**: Verificación de formato y duplicados

#### Lista de Códigos
- **Tabla completa**: Todos los códigos almacenados
- **Información**: Tipo, valor, relé asignado
- **Acciones**: Eliminación individual
- **Contador**: Códigos actuales vs máximo

## Funcionalidades Interactivas

### ⚙️ Configuración en Tiempo Real
- **Guardado automático**: Cambios aplicados inmediatamente
- **Validación**: Verificación de parámetros antes de guardar
- **Feedback**: Confirmación visual de cambios
- **Persistencia**: Configuración guardada en EEPROM

### 🔄 Acciones del Sistema
- **Activación de relés**: Control directo desde la interfaz
- **Reinicio**: Reinicio completo del sistema
- **Reset**: Restauración a configuración por defecto
- **Cambio de contraseña**: Actualización segura de credenciales

### 📱 Responsividad
- **Mobile First**: Diseño optimizado para móviles
- **Breakpoints**: Adaptación automática a diferentes tamaños
- **Touch Friendly**: Botones y elementos táctiles apropiados
- **Legibilidad**: Texto y elementos claramente visibles

## Seguridad de la Interfaz

### 🔒 Autenticación
- **HTTP Basic Auth**: Método estándar y seguro
- **Credenciales**: Usuario y contraseña requeridos
- **Sesión**: Mantenida durante la navegación
- **Logout**: Cierre de sesión al cerrar navegador

### 🛡️ Protección
- **Validación de entrada**: Todos los campos validados
- **Sanitización**: Datos limpiados antes de procesar
- **Límites**: Restricciones en tamaños y formatos
- **Error handling**: Manejo seguro de errores

## Acceso y Navegación

### 🌐 Acceso
- **URL**: `http://[IP_DISPOSITIVO]`
- **Puerto**: 80 (HTTP estándar)
- **Protocolo**: HTTP (para uso interno)
- **mDNS**: Soporte para `http://[nombre].local`

### 🧭 Navegación
- **Estructura plana**: Sin submenús complejos
- **Enlaces directos**: Acceso rápido a funciones
- **Breadcrumbs**: Indicación clara de ubicación
- **Botones de acción**: Claramente identificados

## Compatibilidad

### 🌍 Navegadores Soportados
- **Chrome**: Versión 90+
- **Firefox**: Versión 88+
- **Safari**: Versión 14+
- **Edge**: Versión 90+
- **Mobile**: iOS Safari, Chrome Mobile

### 📱 Dispositivos
- **Desktop**: PC, Mac, Linux
- **Tablet**: iPad, Android tablets
- **Mobile**: iPhone, Android phones
- **Resolución**: 320px - 1920px+

## Características Técnicas

### ⚡ Rendimiento
- **Carga rápida**: HTML optimizado
- **CDN**: Font Awesome desde CDN
- **Compresión**: Código minificado
- **Caché**: Headers apropiados

### 🔧 Tecnologías
- **HTML5**: Estructura semántica
- **CSS3**: Estilos modernos
- **JavaScript**: Funcionalidad interactiva
- **Font Awesome**: Iconografía profesional

## Monitoreo y Logs

### 📊 Información en Tiempo Real
- **Estado del sistema**: Actualizado automáticamente
- **Conectividad**: Estado de red y MQTT
- **Seguridad**: Contadores y bloqueos
- **Hardware**: Estado de teclados y relés

### 📝 Logging
- **Acciones del usuario**: Registradas en sistema
- **Cambios de configuración**: Auditados
- **Errores**: Capturados y reportados
- **Accesos**: Historial mantenido

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
