# ✅ FIRMWARE LISTO PARA PROBAR

**Fecha**: 11 de Diciembre, 2025  
**Versión**: v3.0.1 (con modos Normal/Inverso)  
**Estado**: ✅ **SUBIDO A LA PLACA Y LISTO**

---

## 🎉 MEJORAS IMPLEMENTADAS

### ✅ Entradas Digitales DI1/DI2 con Dos Modos

**Pines utilizados**:
```
DI1: GPIO36 (verificado libre, input-only)
DI2: GPIO39 (verificado libre, input-only)
```

### 🔵 Modo Normal (Predeterminado)
```
Pin LOW (sin pulsar) → Relé OFF
Pin HIGH (pulsado)   → Relé ON por X segundos
Tras X segundos      → Relé OFF (espera LOW)
Pin vuelve a LOW     → Listo para nuevo pulso
```

**Uso**: Botones de acceso, sensores, porteros automáticos

### 🔴 Modo Inverso (Fail-Safe)
```
Pin LOW (sin pulsar) → Relé ON (siempre activo)
Pin HIGH (pulsado)   → Relé OFF (desactiva)
Pin vuelve a LOW     → Relé ON (reactiva)
```

**Uso**: Parada de emergencia, sistemas de seguridad, fail-safe

---

## 📊 ESTADO DEL FIRMWARE

```
✅ Compilado: SUCCESS
✅ Flash: 91.6% (1,201,205 bytes)
✅ RAM: 15.3% (50,228 bytes)
✅ Subido a placa: ESP32 MAC 2c:bc:bb:14:46:58
✅ Puerto: /dev/cu.usbserial-3110
```

---

## 🚀 CÓMO PROBAR AHORA

### Paso 1: Monitorear el Puerto Serial

```bash
./monitor_di_simple.sh
```

Deberías ver:
```
🔌 INICIALIZACIÓN Entradas digitales configuradas: DI1=GPIO36, DI2=GPIO39
📊 ESTADO INICIAL Estado inicial: DI1=LOW, DI2=LOW
📥 CONFIGURACIÓN Cargada desde EEPROM
   DI1 (GPIO36): DESHABILITADA, Relé 1, 2.0s, Modo NORMAL
   DI2 (GPIO39): DESHABILITADA, Relé 2, 2.0s, Modo NORMAL
```

### Paso 2: Obtener IP del Dispositivo

En el monitor serial, busca líneas como:
```
✅ Conectado a Ethernet
   IP: 192.168.1.XXX
```

O busca en tu router el dispositivo con MAC: `2c:bc:bb:14:46:58`

### Paso 3: Acceder a la Interfaz Web

```
http://192.168.1.XXX/digital_inputs
```

Usuario: `admin`  
Contraseña: `admin` (o la que hayas configurado)

### Paso 4: Configurar y Probar

#### Test A: Modo Normal (Temporizado)

1. **Configurar DI1**:
   - ✅ Marcar "Habilitada"
   - Seleccionar "Relé 1"
   - Duración: "2.0" segundos
   - Modo: "🔵 Normal"
   - Guardar

2. **Conectar hardware**:
   ```
   Cable/Pinza → GPIO36 (pin físico del ESP32)
   Tocar brevemente en pin 3.3V
   ```

3. **Ver en el monitor**:
   ```
   [HH:MM:SS] 🔼 DI1 SUBIDA Pulso detectado (#1)
     └─▶ Relé 1 activado por 2.0s
   ```

4. **Verificar**: Relé 1 físico debería activarse 2 segundos

#### Test B: Modo Inverso (Fail-Safe)

1. **Configurar DI2**:
   - ✅ Marcar "Habilitada"
   - Seleccionar "Relé 2"
   - Modo: "🔴 Inverso"
   - Guardar

2. **Ver en el monitor**:
   ```
   ⚙️ [DI2] Configuración actualizada: HABILITADA, Relé 2, 2.0s, Modo INVERSO
   🔵 [DI2] Modo INVERSO - LOW detectado → Activando Relé 2 (permanente)
   ```

3. **Verificar**: Relé 2 debería activarse inmediatamente (porque por defecto pin está en LOW)

4. **Pulsar botón** (conectar GPIO39 a 3.3V):
   ```
   [HH:MM:SS] 🔴 DI2 INVERSO - HIGH detectado → Desactivando Relé 2
   ```

5. **Soltar botón**:
   ```
   [HH:MM:SS] 🔵 DI2 INVERSO - LOW detectado → Activando Relé 2 (permanente)
   ```

---

## 🔌 CONEXIÓN HARDWARE PARA PRUEBAS

### Opción Simple (Para Pruebas)

**Sin Pulsador - Solo Cables**:
```
Cable 1: GPIO36 → usar para tocar 3.3V (DI1)
Cable 2: GPIO39 → usar para tocar 3.3V (DI2)

Resistencia pull-up 10kΩ:
  - De GPIO36 a 3.3V (opcional pero recomendado)
  - De GPIO39 a 3.3V (opcional pero recomendado)
```

### Con Pulsador (Instalación Real)

**Modo Normal**:
```
Pulsador NA (Normalmente Abierto):
  Terminal 1 → GPIO36 (o GPIO39)
  Terminal 2 → 3.3V
  Pull-up 10kΩ de GPIO a GND
```

**Modo Inverso**:
```
Pulsador NA:
  Terminal 1 → GPIO36 (o GPIO39)
  Terminal 2 → 3.3V (para desactivar)
  Pull-down 10kΩ de GPIO a GND
```

---

## ⚠️ IMPORTANTE

### Seguridad Eléctrica
- ⚠️ **SOLO 3.3V** - NO conectar 5V (dañarás el ESP32)
- ⚠️ GPIO36 y GPIO39 son **input-only** (sin pull-up interna)
- ✅ Usar resistencia pull-up/down externa de 10kΩ

### Configuración
- Por defecto: ambas entradas **DESHABILITADAS**
- **Debes habilitar** en la web para que funcionen
- Configuración se guarda en EEPROM (persiste tras reinicio)

### Modo Inverso
- ⚠️ Relé consume energía constantemente cuando habilitado
- Solo usar cuando necesites fail-safe
- Al habilitar DI en modo inverso → relé se activa inmediatamente

---

## 📝 TABLA RESUMEN

| Característica | Modo Normal 🔵 | Modo Inverso 🔴 |
|----------------|---------------|-----------------|
| Estado inicial | Relé OFF | Relé ON |
| Al pulsar (HIGH) | Relé ON temporizado | Relé OFF |
| Al soltar (LOW) | Relé OFF | Relé ON |
| Duración | Configurable | No aplica |
| Consumo | Solo al activar | Constante |
| Fail-safe | No | Sí |
| Uso típico | Acceso temporal | Seguridad |

---

## 🎯 COMANDOS ÚTILES

```bash
# Monitorear eventos de entradas digitales
./monitor_di_simple.sh

# Ver todos los logs del ESP32
screen /dev/cu.usbserial-3110 115200

# Listar dispositivos
pio device list

# Recompilar y subir (si haces cambios)
pio run --target upload --upload-port /dev/cu.usbserial-3110

# Ver estado de git
git status
git log --oneline -5
```

---

## 📂 DOCUMENTACIÓN CREADA

```
✅ docs/v3.0/mejora-modo-normal-inverso.md  - Documentación completa
✅ LISTO_PARA_PROBAR.md                     - Esta guía
✅ COMO_MONITOREAR.md                       - Guía de monitoreo
✅ FIRMWARE_SUBIDO.md                       - Info de carga
✅ monitor_di_simple.sh                     - Script de monitor
```

---

## 🎊 RESUMEN FINAL

El firmware está:
- ✅ **Compilado** sin errores
- ✅ **Subido** a la placa ESP32
- ✅ **Mejorado** con modo Normal/Inverso
- ✅ **Documentado** completamente
- ✅ **Listo** para probar

**Pines**:
- DI1: GPIO36 ✅
- DI2: GPIO39 ✅

**Funcionalidad**:
- Modo Normal: Temporizado
- Modo Inverso: Fail-safe
- Configuración web
- Monitoreo en tiempo real

---

## 🚀 SIGUIENTE PASO

**Ejecuta el monitor ahora**:

```bash
./monitor_di_simple.sh
```

Y observa los mensajes de inicialización. Luego accede a la web para configurar y probar.

---

**¡Todo listo para probar!** 🎉

---

**Firmware subido**: 11 de Diciembre, 2025 17:05  
**Puerto**: /dev/cu.usbserial-3110  
**Commits en v3.0**: 10  
**Estado**: ✅ **LISTO PARA TESTING**

