# Páginas de Configuración - Detalle Técnico

## Descripción
Documentación detallada de todas las páginas de configuración disponibles en la interfaz web del KC868A2, incluyendo parámetros, validaciones y comportamientos específicos.

## Página Principal de Configuración (`/`)

### 📋 Formulario de Configuración del Dispositivo

#### Campos de Configuración

##### Nombre del Dispositivo
- **Campo**: `deviceName`
- **Tipo**: Texto
- **Longitud**: 1-31 caracteres
- **Valor por defecto**: Serial generado automáticamente
- **Validación**: 
  - No puede estar vacío
  - Caracteres alfanuméricos y guiones
  - Único en la red local
- **Persistencia**: Guardado en EEPROM
- **Efecto**: Cambio inmediato del hostname

##### Número de Serie MQTT
- **Campo**: `fixedSerial`
- **Tipo**: Texto (solo lectura)
- **Formato**: `SWATID_XXXXXXXX`
- **Generación**: Basado en MAC address del ESP32
- **Inmutabilidad**: No modificable por seguridad
- **Uso**: Identificación única en MQTT

##### Duración del Relé
- **Campo**: `releDuration`
- **Tipo**: Número decimal
- **Rango**: 0.5 - 60.0 segundos
- **Incremento**: 0.5 segundos
- **Valor por defecto**: 2.0 segundos
- **Aplicación**: Tiempo de activación de relés
- **Validación**: Rango numérico válido

##### Configuración de Red

###### Usar DHCP
- **Campo**: `useDhcp`
- **Tipo**: Selector (Sí/No)
- **Valor por defecto**: Sí
- **Efecto**: 
  - Sí: IP automática del router
  - No: Muestra campos de IP estática
- **Reinicio**: Requerido para aplicar cambios

###### IP Estática
- **Campo**: `staticIp`
- **Tipo**: Dirección IP
- **Formato**: xxx.xxx.xxx.xxx
- **Visibilidad**: Solo cuando DHCP = No
- **Validación**: Formato IP válido
- **Ejemplo**: 192.168.1.100

###### Puerta de Enlace
- **Campo**: `staticGateway`
- **Tipo**: Dirección IP
- **Formato**: xxx.xxx.xxx.xxx
- **Visibilidad**: Solo cuando DHCP = No
- **Validación**: Formato IP válido
- **Ejemplo**: 192.168.1.1

###### Máscara de Subred
- **Campo**: `staticSubnet`
- **Tipo**: Dirección IP
- **Formato**: xxx.xxx.xxx.xxx
- **Visibilidad**: Solo cuando DHCP = No
- **Valor por defecto**: 255.255.255.0
- **Validación**: Formato IP válido

###### DNS
- **Campo**: `staticDns`
- **Tipo**: Dirección IP
- **Formato**: xxx.xxx.xxx.xxx
- **Visibilidad**: Solo cuando DHCP = No
- **Valor por defecto**: 8.8.8.8
- **Validación**: Formato IP válido

### 🔐 Formulario de Cambio de Contraseña

#### Nueva Contraseña
- **Campo**: `newPassword`
- **Tipo**: Contraseña
- **Longitud**: 4-31 caracteres
- **Validación**:
  - Mínimo 4 caracteres
  - Máximo 31 caracteres
  - Cualquier carácter imprimible
- **Persistencia**: Guardado en EEPROM
- **Efecto**: Inmediato en próximos accesos

## Página de Gestión de Códigos (`/codes`)

### ⚙️ Configuración del Modo de Validación

#### Modo de Validación
- **Campo**: `mode`
- **Opciones**:
  - `local`: Validación local primero
  - `remote`: Validación remota primero
- **Comportamiento**:
  - **Local primero**: Busca en EEPROM, luego MQTT
  - **Remoto primero**: Envía a MQTT, fallback local
- **Persistencia**: Guardado en EEPROM
- **Aplicación**: Inmediata

### ➕ Formulario de Añadir Códigos

#### Tipo de Código
- **Campo**: `type`
- **Opciones**:
  - `PIN`: Código numérico de 4-6 dígitos
  - `TAG`: Código de tarjeta RFID/NFC
- **Validación**: Selección obligatoria

#### Valor del Código
- **Campo**: `value`
- **Tipo**: Texto
- **Longitud máxima**: 16 caracteres
- **Validaciones específicas**:
  - **PIN**: Solo dígitos, 4-6 caracteres
  - **TAG**: Cualquier carácter, 1-16 caracteres
- **Duplicados**: No permitidos
- **Ejemplos**:
  - PIN: `1234`, `567890`
  - TAG: `12345678`, `ABCD1234`

#### Relé a Activar
- **Campo**: `relay`
- **Opciones**:
  - `1`: Relé 1 (GPIO 15)
  - `2`: Relé 2 (GPIO 2)
- **Validación**: Valor numérico 1 o 2
- **Efecto**: Determina qué relé se activa

## Páginas de Acción

### ⚡ Activación de Relés (`/rele`)

#### Parámetros
- **Parámetro**: `relay`
- **Tipo**: Entero
- **Valores válidos**: 1, 2
- **Valor por defecto**: 1
- **Comportamiento**: 
  - Activa el relé especificado
  - Usa duración configurada
  - Redirecciona a página principal

### 🔄 Reinicio del Sistema (`/reboot`)

#### Comportamiento
- **Acción**: Reinicio completo del ESP32
- **Tiempo de espera**: 2 segundos
- **Redirección**: Automática tras 10 segundos
- **Efecto**: Reinicio completo del sistema

### 🔧 Reset a Valores por Defecto (`/reset`)

#### Valores Restaurados
- **Nombre dispositivo**: `SWATID_DEFAULT`
- **DHCP**: Activado
- **Duración relé**: 2.0 segundos
- **Acceso local**: Desbloqueado
- **Intentos fallidos**: 0
- **Duración bloqueo**: 60 segundos
- **Máximo intentos**: 3
- **Contraseña web**: `admin`

#### Comportamiento
- **Acción**: Restauración completa
- **Tiempo de espera**: 2 segundos
- **Reinicio**: Automático tras reset
- **Efecto**: Sistema como nuevo

### 🗑️ Eliminación de Códigos (`/codes/delete`)

#### Parámetros
- **Parámetro**: `type`
- **Valores**: `PIN`, `TAG`
- **Parámetro**: `value`
- **Tipo**: Texto (valor exacto del código)

#### Comportamiento
- **Validación**: Verificación de existencia
- **Eliminación**: Inmediata si existe
- **Redirección**: A página de códigos
- **Feedback**: Confirmación visual

## Validaciones y Restricciones

### 📝 Validaciones de Entrada

#### Campos de Texto
- **Longitud**: Límites específicos por campo
- **Caracteres**: Validación de formato
- **Duplicados**: Verificación de unicidad
- **Sanitización**: Limpieza de caracteres especiales

#### Campos Numéricos
- **Rango**: Valores mínimos y máximos
- **Tipo**: Entero o decimal según campo
- **Precisión**: Decimales permitidos
- **Formato**: Validación de formato numérico

#### Campos de Red
- **IP**: Formato xxx.xxx.xxx.xxx
- **Rango**: Direcciones IP válidas
- **Subred**: Máscaras válidas
- **DNS**: Servidores DNS válidos

### 🚫 Restricciones del Sistema

#### Límites de Capacidad
- **Códigos máximos**: 500
- **Longitud de nombres**: 31 caracteres
- **Duración relé**: 0.5-60 segundos
- **Contraseñas**: 4-31 caracteres

#### Restricciones de Seguridad
- **Serial MQTT**: No modificable
- **Acceso web**: Requiere autenticación
- **Cambios críticos**: Requieren reinicio
- **Configuración**: Validación estricta

## Comportamientos Especiales

### 🔄 JavaScript Interactivo

#### Toggle de Campos IP
```javascript
function toggleIpFields() {
    var useDhcp = document.getElementById('useDhcp').value;
    var staticFields = document.getElementById('staticIpFields');
    staticFields.style.display = (useDhcp == '1') ? 'none' : 'block';
}
```

#### Validación en Tiempo Real
- **Campos requeridos**: Marcados visualmente
- **Formatos**: Validación mientras se escribe
- **Rangos**: Verificación de límites
- **Duplicados**: Alerta inmediata

### 📱 Responsividad

#### Breakpoints
- **Mobile**: < 768px
- **Tablet**: 768px - 1024px
- **Desktop**: > 1024px

#### Adaptaciones
- **Formularios**: Apilamiento vertical en móvil
- **Tablas**: Scroll horizontal en pantallas pequeñas
- **Botones**: Tamaño táctil apropiado
- **Texto**: Legibilidad en todos los tamaños

## Persistencia y Sincronización

### 💾 Almacenamiento
- **EEPROM**: Configuración principal
- **Offset 0**: Configuración del sistema
- **Offset 512**: Códigos almacenados
- **Validación**: Marcadores de integridad

### 🔄 Sincronización
- **Cambios web**: Aplicados inmediatamente
- **Cambios MQTT**: Sincronizados con web
- **Reinicio**: Aplicación de cambios de red
- **Backup**: Automático en EEPROM

## Manejo de Errores

### ❌ Errores de Validación
- **Mensajes claros**: Descripción específica del error
- **Campos marcados**: Indicación visual del problema
- **Sugerencias**: Cómo corregir el error
- **Persistencia**: Datos válidos mantenidos

### 🔧 Errores del Sistema
- **Conexión**: Indicación de problemas de red
- **MQTT**: Estado de conectividad
- **Hardware**: Estado de teclados y relés
- **Memoria**: Advertencias de uso

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
