# Manual de Compilación - SWATID-A2

Este documento describe cómo compilar el firmware SWATID-A2 para la placa KC868-A2 usando PlatformIO.

## Índice

1. [Estructura del Proyecto](#estructura-del-proyecto)
2. [Requisitos Previos](#requisitos-previos)
3. [Instalación del Entorno](#instalación-del-entorno)
4. [Archivos Necesarios para Compilar](#archivos-necesarios-para-compilar)
5. [Proceso de Compilación](#proceso-de-compilación)
6. [Carga del Firmware](#carga-del-firmware)
7. [Solución de Problemas](#solución-de-problemas)

---

## Estructura del Proyecto

El proyecto PlatformIO tiene la siguiente estructura mínima requerida:

```
01 - CURSOR/
├── platformio.ini          # Configuración del proyecto (OBLIGATORIO)
├── src/
│   └── main.ino            # Código fuente principal (OBLIGATORIO)
├── .gitignore              # Archivos a ignorar en Git
├── docs/                   # Documentación
└── firmware/               # Binarios compilados para distribución
```

### Directorio de Ejecución

**IMPORTANTE:** El comando `pio run` debe ejecutarse desde el directorio raíz del proyecto, es decir, donde se encuentra el archivo `platformio.ini`.

```bash
# Directorio correcto para compilar:
/ruta/al/proyecto/01 - CURSOR/

# Ejemplo:
cd "/Volumes/SWAT_WORK/01 - DESARROLLOS/01 - KC868A2/01 - CURSOR"
pio run
```

---

## Requisitos Previos

### Software Necesario

| Software | Versión Mínima | Descripción |
|----------|----------------|-------------|
| Python | 3.7+ | Requerido por PlatformIO |
| PlatformIO Core | 6.0+ | Herramienta de compilación |
| Git | 2.0+ | Control de versiones (opcional) |

### Hardware Soportado

- **Placa:** ESP32 Dev Module (esp32dev)
- **Plataforma:** Espressif 32 v5.4.0
- **Framework:** Arduino

---

## Instalación del Entorno

### Paso 1: Instalar Python

#### macOS
```bash
# Con Homebrew
brew install python3

# Verificar instalación
python3 --version
```

#### Windows
1. Descargar desde https://www.python.org/downloads/
2. Durante la instalación, marcar "Add Python to PATH"
3. Verificar en CMD: `python --version`

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install python3 python3-pip
python3 --version
```

### Paso 2: Instalar PlatformIO Core (CLI)

```bash
# Instalación mediante pip
pip install platformio

# O con pip3 en sistemas que lo requieran
pip3 install platformio

# Verificar instalación
pio --version
```

**Nota:** También puedes usar PlatformIO IDE (extensión de VSCode) que incluye el CLI.

### Paso 3: Instalar Extensión VSCode (Opcional)

1. Abrir Visual Studio Code
2. Ir a Extensiones (Ctrl+Shift+X / Cmd+Shift+X)
3. Buscar "PlatformIO IDE"
4. Instalar la extensión oficial

---

## Archivos Necesarios para Compilar

### Archivos Mínimos Requeridos

Para compilar en otro ordenador, necesitas copiar **únicamente** estos archivos:

```
01 - CURSOR/
├── platformio.ini          # Configuración y dependencias
└── src/
    └── main.ino            # Código fuente
```

### Contenido del platformio.ini

El proyecto define **dos entornos**. Detalle completo: [docs/v4.1.0/ENTORNOS_COMPILACION.md](../../../v4.1.0/ENTORNOS_COMPILACION.md).

```ini
[env:esp32dev]
platform = espressif32@5.4.0
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 115200

; Librerías necesarias (se descargan automáticamente)
lib_deps = 
    bblanchon/ArduinoJson@^6.21.3
    knolleary/PubSubClient@^2.8

; Configuración de compilación
build_flags = 
    -DCORE_DEBUG_LEVEL=0
    -DARDUINO_USB_CDC_ON_BOOT=0
    -DCONFIG_ARDUINO_USB_CDC_ON_BOOT=0
    -DPLATFORMIO=1
    -Os
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections

; Configuración de monitor
monitor_filters = 
    default
    time

; Entorno BLE (v4.x): pio run -e esp32dev_ble
; [env:esp32dev_ble] → ENABLE_BLE=1 + NimBLE + partitions_ble.csv
```

### Dependencias Automáticas

PlatformIO descarga automáticamente todas las dependencias al compilar:

| Librería | Versión | Uso |
|----------|---------|-----|
| ArduinoJson | ^6.21.3 | Manejo de JSON |
| PubSubClient | ^2.8 | Cliente MQTT |
| WiFi | Incluida | Conectividad WiFi |
| Ethernet | Incluida | Conectividad Ethernet |
| WebServer | Incluida | Servidor HTTP |
| HTTPClient | Incluida | Cliente HTTP |
| EEPROM | Incluida | Almacenamiento persistente |
| Update | Incluida | Actualizaciones OTA |
| DNSServer | Incluida | Servidor DNS (captive portal) |
| ESPmDNS | Incluida | Descubrimiento mDNS |

**Las librerías marcadas como "Incluida" vienen con el framework Arduino para ESP32.**

---

## Proceso de Compilación

### Compilación Básica

```bash
# 1. Navegar al directorio del proyecto
cd "/ruta/al/proyecto/01 - CURSOR"

# 2. Compilar el firmware
pio run
```

### Primera Compilación

En la primera compilación, PlatformIO:
1. Descarga la plataforma espressif32@5.4.0
2. Descarga el toolchain de compilación
3. Descarga las librerías especificadas en `lib_deps`
4. Compila el código fuente

**Tiempo estimado primera vez:** 2-5 minutos (depende de la conexión)  
**Tiempo estimado compilaciones posteriores:** 10-30 segundos

### Salida Esperada

```
Processing esp32dev (platform: espressif32@5.4.0; board: esp32dev; framework: arduino)
--------------------------------------------------------------------------------
...
Linking .pio/build/esp32dev/firmware.elf
Checking size .pio/build/esp32dev/firmware.elf
RAM:   [==        ]  15.3% (used 50228 bytes from 327680 bytes)
Flash: [========= ]  91.8% (used 1202861 bytes from 1310720 bytes)
Building .pio/build/esp32dev/firmware.bin
========================= [SUCCESS] Took 9.75 seconds =========================
```

### Ubicación del Firmware Compilado

```
.pio/build/esp32dev/
├── firmware.bin        # Firmware para subir vía USB o OTA
├── firmware.elf        # Archivo ELF con símbolos de debug
├── partitions.bin      # Tabla de particiones
└── bootloader.bin      # Bootloader
```

---

## Carga del Firmware

### Opción 1: Carga por USB

```bash
# Detectar puerto automáticamente
pio run --target upload

# Especificar puerto manualmente
pio run --target upload --upload-port /dev/cu.usbserial-XXXX    # macOS
pio run --target upload --upload-port COM3                       # Windows
pio run --target upload --upload-port /dev/ttyUSB0               # Linux
```

### Opción 2: Monitor Serial

```bash
# Abrir monitor serial después de subir
pio run --target upload --target monitor

# Solo monitor serial
pio device monitor
```

### Opción 3: Actualización OTA

El firmware se puede actualizar vía web accediendo a:
- `http://[IP_DISPOSITIVO]/update`

---

## Solución de Problemas

### Error: "Path is not writable"

```
Error: Invalid value for '-d' / '--project-dir': Path '...' is not writable.
```

**Solución:** Verificar permisos de escritura en el directorio del proyecto.

```bash
# macOS/Linux
chmod -R u+w "/ruta/al/proyecto"
```

### Error: "HomeDirPermissionsError"

```
platformio.exception.HomeDirPermissionsError: The directory `.platformio/.cache` is not owned...
```

**Solución:** Eliminar la caché de PlatformIO:

```bash
rm -rf ~/.platformio/.cache
```

### Error: Puerto USB no detectado

**macOS:**
```bash
ls /dev/cu.usb*
```

**Windows:** Verificar en Administrador de dispositivos → Puertos (COM y LPT)

**Linux:**
```bash
ls /dev/ttyUSB*
dmesg | tail -20
```

**Drivers necesarios:**
- CH340: https://www.wch.cn/download/CH341SER_MAC_ZIP.html
- CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

### Advertencias Comunes (No son errores)

```
warning: __VA_ARGS__ can only appear in the expansion of a C++11 variadic macro
```
→ **Ignorar:** Es una advertencia del preprocesador, no afecta la funcionalidad.

```
warning: "ETH_CLK_MODE" redefined
```
→ **Ignorar:** El código redefine intencionalmente esta macro para la configuración de Ethernet.

---

## Comandos Útiles de PlatformIO

| Comando | Descripción |
|---------|-------------|
| `pio run` | Compilar el proyecto |
| `pio run -t upload` | Compilar y subir al dispositivo |
| `pio run -t clean` | Limpiar archivos compilados |
| `pio device monitor` | Abrir monitor serial |
| `pio lib list` | Listar librerías instaladas |
| `pio lib update` | Actualizar librerías |
| `pio upgrade` | Actualizar PlatformIO |
| `pio run -v` | Compilar con salida detallada |

---

## Resumen Rápido

```bash
# En un nuevo ordenador:

# 1. Instalar PlatformIO
pip install platformio

# 2. Copiar los archivos del proyecto (platformio.ini + src/)

# 3. Compilar
cd "/ruta/al/proyecto"
pio run

# 4. Subir al dispositivo
pio run -t upload
```

---

*Documento generado para el proyecto SWATID-A2 v3.0*  
*Última actualización: Febrero 2026*
