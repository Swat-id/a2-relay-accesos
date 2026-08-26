# 🔍 Guía de Monitoreo de Entradas Digitales DI1/DI2

**Fecha**: 11 de Diciembre, 2025  
**Firmware**: v3.0.1 con Entradas Digitales  
**Puerto**: /dev/cu.usbserial-3110

---

## 🎯 Objetivo

Esta guía te ayudará a **monitorear en tiempo real** los pulsos recibidos en las entradas digitales **DI1 (GPIO36)** y **DI2 (GPIO39)**.

---

## 📋 Opciones de Monitoreo

### Opción 1: Script Python Especializado (Recomendado) ⭐

El script `monitor_digital_inputs.py` filtra y muestra **solo** los eventos de las entradas digitales con formato colorido y legible.

#### Ejecutar:
```bash
python3 monitor_digital_inputs.py
```

#### Qué verás:
```
================================================================================
   Monitor de Entradas Digitales DI1 (GPIO36) y DI2 (GPIO39)
================================================================================

Puerto: /dev/cu.usbserial-3110
Baudrate: 115200
Hora de inicio: 16:55:30

Esperando eventos de entradas digitales...
────────────────────────────────────────────────────────────────────────────────

[16:55:32.123] 🔼 DI1 SUBIDA Pulso detectado (#1)
  └─▶ Relé 1 activado por 2.0s
[16:55:34.456] 🔽 DI1 BAJADA Sistema listo para nuevo pulso

[16:55:35.789] 🔼 DI2 SUBIDA Pulso detectado (#1)
  └─▶ Relé 2 activado por 3.0s
[16:55:38.012] 🔽 DI2 BAJADA Sistema listo para nuevo pulso
```

#### Detener:
Presiona `Ctrl+C` para ver el resumen y salir.

---

### Opción 2: Monitor Serial Completo (Ver Todo)

Para ver **todos** los mensajes del ESP32 (no solo entradas digitales):

```bash
screen /dev/cu.usbserial-3110 115200
```

#### Salir de screen:
1. Presiona `Ctrl+A`
2. Luego presiona `K`
3. Confirma con `Y`

---

### Opción 3: Monitor con cat (Simple)

```bash
stty -f /dev/cu.usbserial-3110 115200 && cat /dev/cu.usbserial-3110
```

#### Detener:
Presiona `Ctrl+C`

---

### Opción 4: Monitor PlatformIO (Si funciona)

```bash
pio device monitor --port /dev/cu.usbserial-3110 --baud 115200
```

---

## 📊 Mensajes Esperados

### Al Iniciar el ESP32

```
╔══════════════════════════════════════════════════════════════╗
║                    KC868-A2 DUAL WIEGAND                    ║
║                  Firmware v2.5.0                  ║
╚══════════════════════════════════════════════════════════════╝

...

⚡ Relés inicializados (OFF)

🔌 Entradas digitales configuradas: DI1=GPIO36, DI2=GPIO39
   Estado inicial: DI1=LOW, DI2=LOW

📥 [DI] Configuración de entradas digitales cargada:
   DI1 (GPIO36): DESHABILITADA, Relé 1, 2.0s
   DI2 (GPIO39): DESHABILITADA, Relé 2, 2.0s
```

### Al Detectar Pulso en DI1 (si está habilitada)

```
📍 [DI1] Flanco de subida detectado → Activando Relé 1 por 2.0s
⚡ Activando relé 1 por 2.0 s
✅ [DI1] Evento publicado a MQTT
```

### Al Terminar el Pulso

```
📍 [DI1] Flanco de bajada detectado → Sistema listo para nuevo pulso
⏹️ Relé 1 apagado automáticamente
```

### Al Configurar desde Web

```
⚙️ [DI1] Configuración actualizada: HABILITADA, Relé 1, 2.0s
💾 [DI] Configuración de entradas digitales guardada en EEPROM
```

---

## 🧪 Cómo Probar

### Test 1: Sin Hardware (Ver Estado Inicial)

1. **Ejecutar el monitor**:
   ```bash
   python3 monitor_digital_inputs.py
   ```

2. **Reiniciar el ESP32** (botón reset en la placa)

3. **Verificar mensajes**:
   - Debe mostrar: "Entradas digitales configuradas"
   - Debe mostrar: "Estado inicial: DI1=LOW, DI2=LOW"
   - Debe mostrar: "Configuración cargada"

### Test 2: Con Hardware Conectado

#### Preparar Hardware:
```
Pulsador o Cable:
  - Un terminal → GPIO36 (DI1)
  - Otro terminal → 3.3V
  - Resistencia 10kΩ de GPIO36 a 3.3V (pull-up)
```

#### Pasos:
1. **Habilitar DI1** en la web (`/digital_inputs`):
   - Marcar "Habilitada"
   - Seleccionar Relé 1
   - Duración: 2.0 segundos
   - Guardar

2. **Ejecutar monitor**:
   ```bash
   python3 monitor_digital_inputs.py
   ```

3. **Pulsar el botón** (conectar GPIO36 a 3.3V brevemente)

4. **Ver resultado**:
   ```
   [HH:MM:SS.mmm] 🔼 DI1 SUBIDA Pulso detectado (#1)
     └─▶ Relé 1 activado por 2.0s
   [HH:MM:SS.mmm] 🔽 DI1 BAJADA Sistema listo para nuevo pulso
   ```

5. **Verificar relé**:
   - El Relé 1 físico debería activarse
   - Permanecer activo 2 segundos
   - Desactivarse automáticamente

### Test 3: Pulsos Múltiples

1. **Con el monitor activo**
2. **Pulsar varias veces** (dejando que el pulso baje entre cada uno)
3. **Observar contador**: `Pulso detectado (#1)`, `(#2)`, `(#3)`...
4. **Ver estadísticas** cada 50 pulsos

---

## 🔧 Troubleshooting

### Problema: No aparece ningún mensaje

**Causas posibles**:
1. Entradas están **deshabilitadas** (configuración por defecto)
2. Puerto serial incorrecto
3. Placa no conectada

**Solución**:
```bash
# Verificar puerto
ls -la /dev/cu.usbserial*

# Ver dispositivos conectados
pio device list

# Habilitar entradas via web
http://[IP_DEL_DISPOSITIVO]/digital_inputs
```

### Problema: No detecta pulsos

**Causas posibles**:
1. Entradas deshabilitadas en la configuración
2. Sin resistencia pull-up
3. Voltaje incorrecto (debe ser 3.3V)
4. Mal contacto

**Solución**:
```bash
# 1. Verificar configuración
# En la web, ir a /digital_inputs y verificar que esté "Habilitada"

# 2. Verificar hardware
# - Usar multímetro para medir voltaje en GPIO36/39
# - Debe mostrar ~3.3V con resistencia pull-up
# - Debe bajar a ~0V al conectar a GND

# 3. Ver logs completos
screen /dev/cu.usbserial-3110 115200
# Y buscar mensajes de error
```

### Problema: Detecta pulsos pero relé no activa

**Causas posibles**:
1. Relé mal configurado en la interfaz web
2. Problema con el relé físico
3. Problema de alimentación

**Solución**:
1. Verificar configuración web (relé correcto seleccionado)
2. Probar activar relé manualmente desde web (`/rele?relay=1`)
3. Verificar alimentación de la placa

---

## 📈 Análisis de Eventos

### Interpretar los Mensajes

#### Flanco de Subida
```
[16:55:32.123] 🔼 DI1 SUBIDA Pulso detectado (#1)
  └─▶ Relé 1 activado por 2.0s
```
- `16:55:32.123`: Timestamp exacto del evento
- `DI1`: Entrada que detectó el pulso
- `SUBIDA`: Transición LOW→HIGH
- `(#1)`: Contador de pulsos en esta sesión
- `Relé 1`: Relé activado
- `2.0s`: Duración configurada

#### Flanco de Bajada
```
[16:55:34.456] 🔽 DI1 BAJADA Sistema listo para nuevo pulso
```
- `BAJADA`: Transición HIGH→LOW
- `Sistema listo`: Puede recibir un nuevo pulso

#### Configuración
```
[16:55:30.789] ⚙️ DI1 CONFIG HABILITADA, Relé 1, 2.0s
```
- Cambio de configuración desde la web
- Muestra el nuevo estado

### Calcular Latencia

La latencia entre pulso físico y activación del relé es:
```
Tiempo = Timestamp(Relé activado) - Timestamp(Flanco detectado)
```

Típicamente: **< 10ms**

---

## 🎯 Casos de Uso

### 1. Debugging de Instalación
```bash
# Monitorear durante instalación para verificar detección
python3 monitor_digital_inputs.py

# Conectar cables uno por uno
# Verificar que cada conexión genera un evento
```

### 2. Conteo de Eventos
```bash
# El monitor cuenta automáticamente los pulsos
# Ver estadísticas cada 50 eventos
# O ver resumen al salir (Ctrl+C)
```

### 3. Análisis de Temporización
```bash
# Ver timestamps para analizar:
# - Frecuencia de pulsos
# - Duración de activación
# - Tiempo entre pulsos
```

### 4. Validación de MQTT
```bash
# Verificar que cada pulso genera un mensaje MQTT
# Buscar: "📡 MQTT ENVIADO"
```

---

## 📊 Comandos Rápidos

```bash
# Monitor especializado (solo entradas digitales)
python3 monitor_digital_inputs.py

# Monitor completo (todos los mensajes)
screen /dev/cu.usbserial-3110 115200

# Ver dispositivos conectados
pio device list

# Verificar puerto
ls -la /dev/cu.usbserial*

# Reiniciar ESP32 (si tienes esptool)
esptool.py --port /dev/cu.usbserial-3110 run

# O subir firmware de nuevo (reinicia automáticamente)
pio run --target upload --upload-port /dev/cu.usbserial-3110
```

---

## 🔗 Referencias

- **Código fuente**: `/src/main.ino` (líneas 2442-2500)
- **Configuración web**: `http://[IP]/digital_inputs`
- **Documentación completa**: `/docs/v3.0/analisis-entradas-digitales.md`
- **Script de monitor**: `/monitor_digital_inputs.py`

---

## 💡 Tips

1. **Mantén el monitor abierto** mientras pruebas hardware
2. **Usa el contador de pulsos** para verificar detección
3. **Revisa los timestamps** para analizar temporización
4. **Ver el resumen al salir** (Ctrl+C) para estadísticas
5. **Si no ves mensajes**, verifica que las entradas estén **habilitadas** en la web

---

**Creado**: 11 de Diciembre, 2025  
**Actualizado**: 11 de Diciembre, 2025  
**Versión**: 1.0

