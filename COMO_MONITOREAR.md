# 🔍 CÓMO MONITOREAR LAS ENTRADAS DIGITALES

**Versión Rápida** - 11 de Diciembre, 2025

---

## 🚀 INICIO RÁPIDO

### Opción 1: Script Bash (Sin dependencias) ⭐ RECOMENDADO

```bash
./monitor_di_simple.sh
```

**Ventajas**:
- ✅ No requiere instalar nada
- ✅ Funciona inmediatamente
- ✅ Formato colorido y legible
- ✅ Contador de pulsos automático
- ✅ Estadísticas al salir (Ctrl+C)

---

### Opción 2: Script Python (Si tienes pyserial)

```bash
python3 monitor_digital_inputs.py
```

**Requiere**:
```bash
pip3 install pyserial --break-system-packages
```

---

### Opción 3: Screen (Ver todo)

```bash
screen /dev/cu.usbserial-3110 115200
```

**Salir**: `Ctrl+A` luego `K` luego `Y`

---

## 📊 QUÉ VERÁS

### Al Iniciar
```
================================================================================
   Monitor de Entradas Digitales DI1 (GPIO36) y DI2 (GPIO39)
================================================================================

Esperando eventos...
────────────────────────────────────────────────────────────────────────────────

[16:55:30.123] 🔌 INICIALIZACIÓN Entradas digitales configuradas: DI1=GPIO36, DI2=GPIO39
[16:55:30.456] 📊 ESTADO INICIAL Estado inicial: DI1=LOW, DI2=LOW
[16:55:30.789] 📥 CONFIGURACIÓN Cargada desde EEPROM
```

### Al Detectar Pulso (con DI habilitada)
```
[16:55:32.123] 🔼 DI1 SUBIDA Pulso detectado (#1)
  └─▶ Relé 1 activado por 2.0s
[16:55:34.456] 🔽 DI1 BAJADA Sistema listo para nuevo pulso
```

### Al Configurar desde Web
```
[16:55:35.789] ⚙️  DI1 CONFIG HABILITADA, Relé 1, 2.0s
```

---

## ⚡ COMANDOS RÁPIDOS

```bash
# Monitorear (opción más simple)
./monitor_di_simple.sh

# Listar dispositivos conectados
ls -la /dev/cu.usbserial*
pio device list

# Ver archivos creados
ls -l monitor*.sh monitor*.py

# Hacer script ejecutable (si es necesario)
chmod +x monitor_di_simple.sh
chmod +x monitor_digital_inputs.py

# Reiniciar ESP32 (físicamente: botón RESET en la placa)
# O desde código:
pio run --target upload --upload-port /dev/cu.usbserial-3110
```

---

## 🧪 PROBAR AHORA

### 1. Ejecutar Monitor
```bash
./monitor_di_simple.sh
```

### 2. Habilitar Entrada (en navegador)
```
http://[IP_DEL_ESP32]/digital_inputs
```
- Marcar "Habilitada"
- Seleccionar Relé 1
- Duración: 2.0 segundos
- Guardar

### 3. Conectar Hardware Temporal
```
Cable/Pinza:
  GPIO36 → tocar brevemente en 3.3V
  (o GPIO39 para DI2)
```

### 4. Ver Resultado
Deberías ver en el monitor:
```
[HH:MM:SS.mmm] 🔼 DI1 SUBIDA Pulso detectado (#1)
  └─▶ Relé 1 activado por 2.0s
```

---

## 🔧 SOLUCIÓN RÁPIDA DE PROBLEMAS

### No veo ningún mensaje
```bash
# 1. Verificar que el puerto es correcto
ls -la /dev/cu.usbserial*

# 2. Reiniciar el ESP32 (botón RESET)

# 3. Verificar baudrate
# Debe ser 115200
```

### No detecta pulsos
```bash
# 1. Verificar que DI1/DI2 esté HABILITADA en la web
http://[IP]/digital_inputs

# 2. Verificar hardware:
# - Resistencia pull-up 10kΩ a 3.3V
# - Conexión a GPIO36 o GPIO39
# - Voltaje correcto (3.3V, NO 5V)
```

### Script no es ejecutable
```bash
chmod +x monitor_di_simple.sh
./monitor_di_simple.sh
```

---

## 📚 DOCUMENTACIÓN COMPLETA

- **Guía detallada**: `GUIA_MONITOREO_DI.md`
- **Implementación**: `docs/v3.0/implementacion-entradas-digitales-completada.md`
- **Firmware subido**: `FIRMWARE_SUBIDO.md`

---

## 💡 CONSEJO

**Antes de probar hardware**, ejecuta el monitor y:
1. Reinicia el ESP32 (botón RESET)
2. Verifica que aparecen los mensajes de inicialización
3. Esto confirma que el monitor funciona correctamente

Luego puedes proceder a probar con hardware real.

---

**Herramientas disponibles**:
- ✅ `monitor_di_simple.sh` (Bash, sin dependencias)
- ✅ `monitor_digital_inputs.py` (Python, requiere pyserial)
- ✅ `screen` o `cat` (comandos Unix básicos)

**Recomendación**: Usar `monitor_di_simple.sh` por su simplicidad.

---

**Puerto**: `/dev/cu.usbserial-3110`  
**Baudrate**: `115200`  
**MAC del ESP32**: `2c:bc:bb:14:46:58`

