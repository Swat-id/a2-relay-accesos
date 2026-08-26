# Diferencias entre entornos PlatformIO — v4.1.0

Documento único de referencia para los dos entornos de compilación definidos en `platformio.ini`. Ambos comparten el mismo código fuente (`src/main.ino`) y se diferencian por flags de compilación, librerías y particiones.

## Resumen rápido

| Aspecto | `esp32dev` | `esp32dev_ble` |
|---------|------------|----------------|
| Propósito | Firmware sin BLE (línea v3.x) | Firmware con BLE (línea v4.1.x) |
| Flag clave | *(sin `ENABLE_BLE`)* | `-DENABLE_BLE=1` |
| Versión reportada | `v3.0.2` / `v3.0.2-EEPROM` | `v4.1.0` / `v4.1.0-BLE` |
| Librería NimBLE | No | Sí (`h2zero/NimBLE-Arduino@^1.4.0`) |
| Particiones | Por defecto ESP32 (app ~1.25 MB) | `partitions_ble.csv` (app 1.875 MB) |
| Uso típico | Dispositivos sin app móvil BLE | Producción con configuración y control vía APP |

## Cómo compilar

Desde la raíz del proyecto (donde está `platformio.ini`):

```bash
# Solo sin BLE
pio run -e esp32dev

# Solo con BLE
pio run -e esp32dev_ble

# Ambos (validación)
pio run -e esp32dev -e esp32dev_ble
```

Binarios generados:

- `.pio/build/esp32dev/firmware.bin`
- `.pio/build/esp32dev_ble/firmware.bin`

## Diferencias de configuración (`platformio.ini`)

### Común a ambos

- Platform: `espressif32@5.4.0`
- Board: `esp32dev`
- Framework: Arduino
- Monitor: 115200
- Librerías base: ArduinoJson, PubSubClient
- Flags de optimización: `-Os`, `--gc-sections`, USB CDC desactivado

### Solo `esp32dev_ble`

```ini
board_build.partitions = partitions_ble.csv
lib_deps = ... h2zero/NimBLE-Arduino@^1.4.0
build_flags = ... -DENABLE_BLE=1
```

### Tabla de particiones BLE (`partitions_ble.csv`)

| Nombre | Tipo | Offset | Tamaño |
|--------|------|--------|--------|
| nvs | data/nvs | 0x9000 | 20 KB |
| otadata | data/ota | 0xe000 | 8 KB |
| app0 | app/ota_0 | 0x10000 | **1.875 MB** |
| app1 | app/ota_1 | 0x1F0000 | **1.875 MB** |
| spiffs | data/spiffs | 0x3D0000 | 192 KB |

El entorno sin BLE usa la tabla por defecto del board (slots OTA más pequeños). NimBLE aumenta el tamaño del firmware; por eso hace falta la partición ampliada.

## Diferencias en el código (`src/main.ino`)

El código se condiciona con `#ifdef ENABLE_BLE`:

| Área | Sin BLE (`esp32dev`) | Con BLE (`esp32dev_ble`) |
|------|----------------------|--------------------------|
| Includes | ETH, WiFi, Web, MQTT, JSON, EEPROM… | + NimBLE, mbedTLS SHA256/HMAC, `esp_random` |
| Versión firmware | 3.0.2 | 4.1.0-BLE |
| Servidor BLE | No compilado | Servicio FF00, chars FF01–FF0B |
| Auth BLE | — | Challenge-Response + token de sesión |
| Clave maestra / HKDF | — | `DeviceKeyConfig` + derivación usuarios MQTT |
| Páginas web BLE | No | Gestión/clear usuarios BLE |
| Loop | Sin lógica BLE | Reinicio diferido, guardado FF04, status y timeout auth |
| EEPROM BLE | Offsets BLE no usados | Auth BLE (~3400), device key (~3200) |

Funcionalidad compartida (siempre presente): Wiegand dual, relés, Ethernet, MQTT, web admin, códigos locales/remotos, torno, entradas digitales, OTA.

## Tamaños de build (referencia, rama v4.1.0)

Valores medidos tras validación de compilación (26 Ago 2026):

| Entorno | RAM | Flash (app) | Binario |
|---------|-----|-------------|---------|
| `esp32dev` | 15.3% (50 228 / 327 680 B) | 92.2% (1 209 073 / 1 310 720 B) | ~1.16 MB |
| `esp32dev_ble` | 18.3% (60 024 / 327 680 B) | 76.3% (1 499 505 / 1 966 080 B) | ~1.44 MB |

> El entorno sin BLE va más ajustado en flash (partición default ~1.25 MB). El BLE usa más RAM/Flash absolutos pero cabe holgado en la partición ampliada.

## Cuándo usar cada entorno

- **`esp32dev`**: despliegues sin app BLE, menor footprint, compatibilidad con línea v3 / EEPROM clásica.
- **`esp32dev_ble`**: dispositivos que se aprovisionan o controlan desde la app móvil (v4.1), con autenticación segura y HKDF.

No mezclar binarios y tablas de partición: flashear `esp32dev_ble` implica usar (o haber flasheado alguna vez) la tabla `partitions_ble.csv`. Un cambio de tabla suele requerir erase completo.

## Validación

Ambos entornos deben compilar en limpio:

```bash
pio run -e esp32dev -e esp32dev_ble
```

Resultado esperado: `2 succeeded`.

## Nota técnica (prototipos PlatformIO)

PlatformIO/Arduino genera prototipos automáticos de funciones al inicio del `.ino`. Las funciones BLE con tipos `NimBLE*` / `DeviceKeyConfig` / `BLEAuthConfig` deben tener **forward declarations dentro de `#ifdef ENABLE_BLE`**, y el uso en `loop()`/`setup()` debe ir también protegido. Sin eso, `esp32dev` falla al compilar aunque el cuerpo BLE esté `#ifdef`-guardado.

---

**Rama**: `v4.1.0`  
**Última actualización**: 26 Agosto 2026
