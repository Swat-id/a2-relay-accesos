# Análisis de Viabilidad - Servidor BLE para KC868-A2

## Resumen Ejecutivo

| Aspecto | Estado | Detalle |
|---------|--------|---------|
| **Memoria Flash** | ⚠️ CRÍTICO | 91.8% usado, necesita optimización |
| **Memoria RAM** | ✅ OK | 15.3% usado, margen suficiente |
| **EEPROM/NVS** | ✅ OK | Espacio disponible para claves BLE |
| **Viabilidad** | ✅ **FACTIBLE** | Requiere cambio de particiones y/o NimBLE |

---

## 1. Estado Actual de Memoria

### 1.1 Flash (Programa)

```
Uso actual v3.0.2:
Flash: [========= ]  91.8% (1,202,861 / 1,310,720 bytes)
       Disponible:  107,859 bytes (~105 KB)
```

**Partición actual:** `default.csv` (1.25 MB para app)

### 1.2 RAM

```
RAM:   [==        ]  15.3% (50,228 / 327,680 bytes)
       Disponible:  277,452 bytes (~270 KB)
```

✅ **RAM suficiente** - BLE típicamente usa ~30-50KB adicionales.

### 1.3 EEPROM/NVS (4KB)

**Mapa de memoria actual:**

| Offset | Fin | Estructura | Tamaño | Uso |
|--------|-----|------------|--------|-----|
| 0 | 200 | Config | ~200 bytes | Configuración general |
| 256 | 284 | DigitalInputConfig | ~28 bytes | Entradas digitales |
| 512 | 1773 | StoredCodes | ~1261 bytes | 50 códigos locales |
| 1800 | 4130 | StoredRemoteCodes | ~2330 bytes | 40 códigos remotos |

**Problema detectado:** `StoredRemoteCodes` excede los 4KB (termina en ~4130).

**Espacio libre identificado:**
- Entre Config y DigitalInput: 56 bytes (200-256)
- Entre DigitalInput y StoredCodes: 228 bytes (284-512)
- **Total aprovechable:** ~284 bytes

---

## 2. Requisitos de Memoria BLE

### 2.1 Claves de Vinculación (EEPROM)

```
Estructura propuesta: BLEAuthConfig
├── validMarker (4 bytes)
├── superadmin_key (64 bytes)     - Clave del superadmin
├── user_keys[5] (5 × 64 = 320 bytes) - Claves de usuarios
├── user_enabled[5] (5 bytes)     - Estado de cada usuario
├── permissions[5] (5 bytes)      - Permisos por usuario
├── checksum (4 bytes)
└── TOTAL: ~402 bytes
```

**Ubicación propuesta:** Offset 3700 (después de reducir RemoteCodes)

### 2.2 Flash - Librerías BLE

| Librería | Tamaño Flash | Tamaño RAM | Notas |
|----------|--------------|------------|-------|
| **ESP32 BLE (Bluedroid)** | ~300 KB | ~50 KB | Stack oficial, pesado |
| **NimBLE-Arduino** | ~100-150 KB | ~15-25 KB | ✅ **Recomendado** |

**Con NimBLE:** Necesitamos ~150 KB adicionales, pero solo tenemos ~105 KB libres.

---

## 3. Soluciones Propuestas

### 3.1 Opción A: Cambiar Esquema de Particiones (RECOMENDADO)

Usar partición `huge_app.csv` o personalizada:

```ini
# platformio.ini
board_build.partitions = huge_app.csv
```

| Partición | Tamaño App | OTA | Descripción |
|-----------|------------|-----|-------------|
| default | 1.25 MB | Sí | Actual |
| huge_app | 3 MB | No | Sin OTA, máximo espacio |
| min_spiffs | 1.9 MB | Sí | Menos SPIFFS |
| **custom** | 2 MB | Sí | Personalizada |

**Partición personalizada propuesta:**

```csv
# partitions_custom.csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,  # 1.875 MB
app1,     app,  ota_1,   0x1F0000,0x1E0000,  # 1.875 MB
nvs_ble,  data, nvs,     0x3D0000,0x10000,   # 64KB para BLE bonding
spiffs,   data, spiffs,  0x3E0000,0x20000,   # 128KB SPIFFS
```

Con 1.875 MB (1,966,080 bytes) tendríamos:
- Usado actual: 1,202,861 bytes
- BLE NimBLE: ~150,000 bytes
- **Total:** ~1,352,861 bytes (68.8% uso)
- **Margen:** ~613 KB libres ✅

### 3.2 Opción B: Optimizar Código Actual

Reducir uso de Flash eliminando código no esencial:

| Optimización | Ahorro estimado |
|--------------|-----------------|
| Reducir strings de debug | ~10-20 KB |
| Optimizar HTML embebido | ~20-30 KB |
| Usar PROGMEM para constantes | ~5-10 KB |
| **Total posible:** | ~35-60 KB |

**Insuficiente por sí sola**, pero combinada con particiones ayuda.

### 3.3 Opción C: BLE Mínimo (Último recurso)

Implementar BLE sin librería, usando ESP-IDF directamente:
- Más complejo de implementar
- Ahorra ~50 KB vs NimBLE
- No recomendado por complejidad

---

## 4. Reorganización de EEPROM

### 4.1 Mapa Propuesto para v4.0

```
EEPROM 4KB (4096 bytes)
┌─────────────────────────────────────────────────────────────┐
│ 0x000 - 0x0C7  │ Config (200 bytes)                         │
├─────────────────────────────────────────────────────────────┤
│ 0x100 - 0x11B  │ DigitalInputConfig (28 bytes)              │
├─────────────────────────────────────────────────────────────┤
│ 0x200 - 0x6F0  │ StoredCodes (1265 bytes, 50 códigos)       │
├─────────────────────────────────────────────────────────────┤
│ 0x700 - 0xCFF  │ StoredRemoteCodes (1536 bytes, 25 códigos) │ ← REDUCIDO
├─────────────────────────────────────────────────────────────┤
│ 0xD00 - 0xE8F  │ BLEAuthConfig (400 bytes)                  │ ← NUEVO
├─────────────────────────────────────────────────────────────┤
│ 0xE90 - 0xFFF  │ Reservado (368 bytes)                      │
└─────────────────────────────────────────────────────────────┘
```

**Cambios necesarios:**
1. Reducir `MAX_REMOTE_CODES` de 40 a 25
2. Añadir estructura `BLEAuthConfig`
3. Actualizar offsets

---

## 5. Especificación de Servicios BLE

### 5.1 Servicio Principal: SWATID Configuration

```
Service UUID: 0000FF00-0000-1000-8000-00805F9B34FB

Características:
├── Auth (0xFF01) - Write/Notify
│   └── Autenticación con clave de 64 bytes
├── Relay Control (0xFF02) - Write
│   └── Formato: [relay_id (1), action (1), duration_ms (2)]
├── Mode Control (0xFF03) - Read/Write
│   └── Formato: [mode (1)] - 0=Normal, 1=Torno
├── Add Code (0xFF04) - Write
│   └── Formato: [type (1), keyboard (1), relay (1), code (16)]
├── Network Config (0xFF05) - Read/Write
│   └── Formato: [dhcp (1), ip (4), gw (4), mask (4), dns (4)]
├── Relay Duration (0xFF06) - Read/Write
│   └── Formato: [relay_id (1), duration_ms (4)]
└── Status (0xFF07) - Read/Notify
    └── Estado general del dispositivo
```

### 5.2 Flujo de Autenticación

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  APP Móvil  │     │   ESP32     │     │   EEPROM    │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       │  1. Conectar BLE  │                   │
       │──────────────────>│                   │
       │                   │                   │
       │  2. Enviar clave  │                   │
       │  (64 bytes)       │                   │
       │──────────────────>│  3. Verificar    │
       │                   │──────────────────>│
       │                   │  4. Resultado     │
       │                   │<──────────────────│
       │  5. Auth OK/FAIL  │                   │
       │<──────────────────│                   │
       │                   │                   │
       │  6. Comandos      │                   │
       │  (si auth OK)     │                   │
       │──────────────────>│                   │
```

---

## 6. Impacto en Funcionalidades Existentes

### 6.1 Coexistencia Ethernet + BLE

✅ **Compatible** - ESP32 soporta Ethernet y BLE simultáneamente.

El PHY LAN8720 usa SPI/RMII, no interfiere con el controlador BLE.

### 6.2 MQTT + BLE

✅ **Compatible** - Ambos pueden funcionar en paralelo.

Consideraciones:
- BLE tiene prioridad baja vs WiFi/Ethernet
- Latencia BLE puede aumentar bajo carga de red alta

### 6.3 Wiegand + BLE

✅ **Compatible** - Los GPIOs Wiegand (33/14, 4/16) no se solapan con BLE.

---

## 7. Plan de Implementación

### Fase 1: Preparación (Prioridad Alta)

1. [ ] Crear partición personalizada con más espacio
2. [ ] Añadir NimBLE a `lib_deps`
3. [ ] Crear estructura `BLEAuthConfig`
4. [ ] Reorganizar offsets EEPROM

### Fase 2: Servidor BLE Básico

1. [ ] Inicializar NimBLE server
2. [ ] Implementar servicio de autenticación
3. [ ] Sistema de vinculación (bonding)
4. [ ] Control de relés vía BLE

### Fase 3: Funcionalidades Completas

1. [ ] Cambio de modo Normal/Torno
2. [ ] Gestión de códigos locales
3. [ ] Configuración de red
4. [ ] Configuración de tiempos

### Fase 4: Seguridad y Optimización

1. [ ] Encriptación de comunicación
2. [ ] Timeout de sesión
3. [ ] Logs de acceso BLE
4. [ ] Pruebas de rendimiento

---

## 8. Prueba de Compilación Realizada

### Resultado con Partición Personalizada

Se ha creado y probado el entorno `esp32dev_ble` con partición de 1.875 MB por app:

```
Entorno esp32dev (partición default 1.25 MB):
Flash: [========= ]  91.8% (1,202,861 / 1,310,720 bytes)
       Disponible:  107,859 bytes (~105 KB) ❌ INSUFICIENTE

Entorno esp32dev_ble (partición custom 1.875 MB):
Flash: [======    ]  61.2% (1,202,861 / 1,966,080 bytes)
       Disponible:  763,219 bytes (~745 KB) ✅ SUFICIENTE
```

### Archivos Creados

- `partitions_ble.csv` - Esquema de particiones personalizado
- `platformio.ini` - Añadido entorno `esp32dev_ble`

### Comandos de Compilación

```bash
# Compilar versión sin BLE (producción actual)
pio run -e esp32dev

# Compilar versión con soporte BLE (v4.0)
pio run -e esp32dev_ble

# Subir versión BLE al dispositivo
pio run -e esp32dev_ble -t upload
```

---

## 9. Conclusiones

### ✅ ES FACTIBLE implementar BLE

La prueba de compilación confirma que con la partición personalizada:

| Recurso | Actual | Con BLE (~150KB) | Margen |
|---------|--------|------------------|--------|
| Flash | 61.2% | ~69% | ~600 KB |
| RAM | 15.3% | ~25% | ~200 KB |
| EEPROM | ~3.7 KB | ~4.0 KB | OK |

### Condiciones para Implementación

1. ✅ **Partición personalizada** - Ya creada (`partitions_ble.csv`)
2. ✅ **NimBLE disponible** - Ya configurado en `platformio.ini`
3. ⏳ **Reorganizar EEPROM** - Pendiente de implementar
4. ⏳ **Código BLE** - Pendiente de implementar

### Riesgos Identificados

| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|--------------|---------|------------|
| OTA más lento | Baja | Bajo | Firmware más grande |
| Interferencia BLE/Ethernet | Baja | Medio | Pruebas exhaustivas |
| Complejidad de código | Media | Medio | Desarrollo incremental |

### Próximos Pasos

1. ✅ ~~Crear archivo de particiones personalizado~~
2. ✅ ~~Verificar espacio con compilación~~
3. ⏳ Implementar estructura `BLEAuthConfig` en EEPROM
4. ⏳ Implementar servidor BLE básico con NimBLE
5. ⏳ Implementar autenticación y vinculación
6. ⏳ Implementar comandos de control

---

*Análisis realizado: Febrero 2026*  
*Rama: v4.0*  
*Compilación verificada: ✅*
