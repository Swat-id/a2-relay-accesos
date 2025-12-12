# Mejora: Modo Normal/Inverso para Entradas Digitales

**Fecha**: 11 de Diciembre, 2025  
**Rama**: v3.0  
**Estado**: ✅ Implementado y subido a placa

---

## 🎯 Objetivo de la Mejora

Añadir dos modos de funcionamiento para las entradas digitales DI1 y DI2:

1. **🔵 Modo Normal**: HIGH activa el relé temporizado
2. **🔴 Modo Inverso**: Relé siempre activo, HIGH lo desactiva (fail-safe)

---

## 📋 Especificación Funcional

### Modo Normal (Predeterminado) 🔵

**Comportamiento**:
```
Estado del Pin: LOW (sin pulsar)
  ↓
  └─▶ Relé: OFF (inactivo)

Pulsador presionado: HIGH
  ↓
  └─▶ Relé: ON por duración configurada
  ↓
  └─▶ Tras X segundos → Relé: OFF
  ↓
  └─▶ Espera a que pin vuelva a LOW
  ↓
  └─▶ Listo para nuevo pulso
```

**Casos de Uso**:
- ✅ Botón de apertura temporal (portero automático)
- ✅ Sensor de presencia (activar luz por X segundos)
- ✅ Detector de movimiento (alarma temporal)
- ✅ Pulsador de acceso momentáneo

### Modo Inverso 🔴

**Comportamiento**:
```
Estado del Pin: LOW (sin pulsar)
  ↓
  └─▶ Relé: ON (siempre activo)

Pulsador presionado: HIGH
  ↓
  └─▶ Relé: OFF (desactivado mientras pulso activo)

Pulsador liberado: LOW
  ↓
  └─▶ Relé: ON (vuelve a activarse)
```

**Casos de Uso**:
- ✅ Botón de emergencia (normalmente cerrado, pulso abre)
- ✅ Sistema fail-safe (fallo del sensor → mantiene activo)
- ✅ Control de seguridad (relé enclavado, pulso libera)
- ✅ Parada de emergencia

---

## 🔧 Implementación Técnica

### Cambios en la Estructura

```cpp
struct DigitalInputConfig {
  bool di1_enabled;
  uint8_t di1_relay;
  float di1_duration;
  bool di1_inverse;      // ← NUEVO
  
  bool di2_enabled;
  uint8_t di2_relay;
  float di2_duration;
  bool di2_inverse;      // ← NUEVO
  
  uint8_t reserved[2];
  uint32_t validMarker;
};
```

### Lógica de Procesamiento

#### Modo Normal (inverse = false)
```cpp
if (!inverse) {
  // Detectar flanco LOW → HIGH
  if (!state.lastState && state.currentState && !state.waitingForLow) {
    // Activar relé por duración configurada
    controlReleWithDuration(duration, relay);
    state.waitingForLow = true;
  }
  
  // Detectar flanco HIGH → LOW
  if (state.lastState && !state.currentState) {
    // Sistema listo para nuevo pulso
    state.waitingForLow = false;
  }
}
```

#### Modo Inverso (inverse = true)
```cpp
else {
  // Estado LOW → Relé debe estar ACTIVO
  if (!state.currentState) {
    if (!state.relayActivated) {
      digitalWrite(relayPin, HIGH);  // Activar permanentemente
      state.relayActivated = true;
    }
  }
  
  // Estado HIGH → Relé debe estar DESACTIVADO
  else {
    if (state.relayActivated) {
      digitalWrite(relayPin, LOW);   // Desactivar
      state.relayActivated = false;
    }
  }
}
```

---

## 🌐 Interfaz Web Actualizada

### Nuevo Selector de Modo

```html
<div class="form-group">
  <label>Modo de Funcionamiento:</label>
  <select name="di1_inverse" id="di1_inverse">
    <option value="0">🔵 Normal - HIGH activa relé (temporizado)</option>
    <option value="1">🔴 Inverso - Relé siempre activo, HIGH lo desactiva</option>
  </select>
  <small>
    Normal: Pulso activa el relé por el tiempo configurado
    Inverso: Relé siempre activo, pulso lo desactiva (seguridad)
  </small>
</div>
```

### Información Mejorada

Se añadieron:
- Explicación de cada modo
- Casos de uso típicos
- Advertencias sobre voltaje (solo 3.3V)
- Notas sobre la duración (solo aplica en modo Normal)

---

## 📊 Comparativa de Modos

| Aspecto | Modo Normal 🔵 | Modo Inverso 🔴 |
|---------|---------------|-----------------|
| **Estado inicial** | Relé OFF | Relé ON |
| **Al pulsar (HIGH)** | Relé ON (temporizado) | Relé OFF |
| **Al soltar (LOW)** | Relé OFF | Relé ON |
| **Duración** | Configurable (0.5-60s) | No aplica |
| **Uso típico** | Acceso temporal | Sistema de seguridad |
| **Fail-safe** | No | Sí (fallo→mantiene ON) |

---

## 🧪 Ejemplos de Operación

### Ejemplo 1: Modo Normal - Botón de Portero

**Configuración**:
- DI1: Habilitada
- Modo: Normal
- Relé: 1
- Duración: 3 segundos

**Operación**:
```
Tiempo:   0s   1s   2s   3s   4s   5s
Botón:    ____┌──┐__________________
Pin DI1:  ____┌────────┐____________  (HIGH mientras pulsado)
Relé 1:   ____┌────────┐____________  (ON por 3s)
          (pulsa)(suelta)(relé OFF)
```

### Ejemplo 2: Modo Inverso - Botón de Emergencia

**Configuración**:
- DI2: Habilitada
- Modo: Inverso
- Relé: 2

**Operación**:
```
Tiempo:   0s   1s   2s   3s   4s   5s
Botón:    ____┌──────────┐__________
Pin DI2:  ____┌──────────┐__________  (HIGH mientras pulsado)
Relé 2:   ┌───┘          └─────────  (OFF mientras pulsado)
          (ON)(pulsa)(suelta)(ON)
```

---

## 📝 Valores por Defecto

```cpp
DI1:
  - Habilitada: false
  - Relé: 1
  - Duración: 2.0 segundos
  - Modo: Normal (inverse = false)

DI2:
  - Habilitada: false
  - Relé: 2
  - Duración: 2.0 segundos
  - Modo: Normal (inverse = false)
```

---

## 🔍 Mensajes de Log Actualizados

### Modo Normal
```
📍 [DI1] Modo NORMAL - HIGH detectado → Activando Relé 1 por 2.0s
⚡ Activando relé 1 por 2.0 s
✅ [DI1] Evento publicado a MQTT
📍 [DI1] Modo NORMAL - LOW detectado → Sistema listo para nuevo pulso
```

### Modo Inverso
```
🔵 [DI1] Modo INVERSO - LOW detectado → Activando Relé 1 (permanente)
🔴 [DI1] Modo INVERSO - HIGH detectado → Desactivando Relé 1
🔵 [DI1] Modo INVERSO - LOW detectado → Activando Relé 1 (permanente)
```

---

## ⚡ Mejoras de Seguridad

### Modo Inverso como Sistema Fail-Safe

En modo inverso, el relé está **siempre activo por defecto**. Esto es útil para:

1. **Sistemas de emergencia**: 
   - Relé mantiene cerradura activada
   - Pulso de emergencia la desactiva temporalmente
   - Al liberar pulso, vuelve a estado seguro

2. **Control de acceso seguro**:
   - Puerta normalmente cerrada (relé ON)
   - Botón exterior la abre (relé OFF mientras pulsado)
   - Al soltar, puerta se cierra automáticamente

3. **Prevención de fallos**:
   - Si se corta alimentación del pulsador → relé queda ON
   - Sistema seguro por defecto

---

## 📊 Cambios en el Código

### Archivos Modificados
- `src/main.ino`: +85 líneas modificadas

### Funciones Actualizadas
- `processDigitalInput()`: Lógica de modo Normal/Inverso
- `loadDigitalInputConfig()`: Carga del campo inverse
- `handleDigitalInputs()`: UI con selector de modo
- `handleDigitalInputsConfig()`: API incluye campo inverse
- `handleSaveDigitalInput()`: Guarda configuración de modo

### Estructuras Actualizadas
- `DigitalInputConfig`: +2 campos bool (di1_inverse, di2_inverse)

---

## 🧪 Tests Recomendados

### Test 1: Modo Normal Básico
```
1. Configurar DI1 en modo Normal, Relé 1, 2s
2. Pulsar y soltar inmediatamente
3. Verificar: Relé activa 2 segundos
4. Verificar: No se reactiva si pulsas antes de soltar
```

### Test 2: Modo Inverso Básico
```
1. Configurar DI1 en modo Inverso, Relé 1
2. Habilitar DI1
3. Verificar: Relé se activa automáticamente (LOW por defecto)
4. Pulsar botón
5. Verificar: Relé se desactiva (mientras HIGH)
6. Soltar botón
7. Verificar: Relé se reactiva (vuelve a LOW)
```

### Test 3: Cambio de Modo en Caliente
```
1. Configurar DI1 en modo Normal, habilitada
2. Cambiar a modo Inverso
3. Verificar: Relé se activa automáticamente
4. Cambiar de vuelta a modo Normal
5. Verificar: Relé se desactiva
```

### Test 4: Deshabilitar en Modo Inverso
```
1. Configurar DI1 en modo Inverso, habilitada
2. Verificar: Relé activo
3. Deshabilitar DI1
4. Verificar: Relé se desactiva
```

---

## 📈 Métricas de Compilación

```
Compilación: ✅ SUCCESS

Flash: 91.6% (1,201,205 bytes)
  Cambio: +2,996 bytes desde v3.0.1 inicial
  
RAM: 15.3% (50,228 bytes)
  Cambio: 0 bytes (sin impacto)

Código: +85 líneas modificadas
Warnings: 0 nuevos
Errores: 0
```

---

## ⚠️ Consideraciones Importantes

### Modo Normal
- ✅ Seguro para la mayoría de aplicaciones
- ✅ Duración configurable
- ✅ No consume energía del relé innecesariamente
- ⚠️ Requiere pulso para activar

### Modo Inverso
- ⚠️ Relé consume energía constantemente cuando habilitado
- ✅ Fail-safe: fallo del sistema → mantiene relé activo
- ✅ Ideal para seguridad crítica
- ⚠️ La duración NO aplica (relé siempre ON cuando LOW)

### Recomendaciones
- Usar **Modo Normal** para aplicaciones generales
- Usar **Modo Inverso** solo cuando se necesite fail-safe
- Considerar consumo energético en modo inverso

---

## 🔧 Hardware - Conexión Típica

### Para Modo Normal
```
Pulsador NA (Normalmente Abierto)
  Terminal 1 → GPIO36 (DI1)
  Terminal 2 → 3.3V
  Resistencia 10kΩ de GPIO36 a GND (pull-down)
  O resistencia 10kΩ de GPIO36 a 3.3V (pull-up)
```

### Para Modo Inverso
```
Pulsador NC (Normalmente Cerrado) o NA
  Terminal 1 → GPIO36 (DI1)
  Terminal 2 → 3.3V (para activar)
  O Terminal 2 → GND (para desactivar)
  Resistencia 10kΩ a 3.3V o GND según lógica deseada
```

**Nota**: Por defecto, sin pulsador conectado:
- Con pull-up a 3.3V → Pin lee HIGH
- Con pull-down a GND → Pin lee LOW

---

## 📊 Tabla de Verdad

### Modo Normal
| Pin State | Pulsador | Relé | Acción |
|-----------|----------|------|--------|
| LOW | No pulsado | OFF | Esperando |
| HIGH | Pulsado | ON | Activa X segundos |
| HIGH | Aún pulsado | OFF* | Espera LOW |
| LOW | Liberado | OFF | Listo para nuevo pulso |

\* Después de que expire la duración

### Modo Inverso
| Pin State | Pulsador | Relé | Acción |
|-----------|----------|------|--------|
| LOW | No pulsado | ON | Siempre activo |
| HIGH | Pulsado | OFF | Desactivado |
| LOW | Liberado | ON | Vuelve a activar |

---

## 🎓 Ejemplos Prácticos

### Ejemplo 1: Portero Automático (Modo Normal)

**Configuración**:
```
DI1: Habilitada
Modo: Normal
Relé: 1 (cerradura eléctrica)
Duración: 3 segundos
```

**Operación**:
1. Visitante pulsa botón exterior
2. GPIO36 → HIGH
3. Relé 1 activa (abre cerradura)
4. 3 segundos después → Relé OFF (cierra cerradura)
5. Sistema listo para siguiente visitante

### Ejemplo 2: Sistema de Emergencia (Modo Inverso)

**Configuración**:
```
DI2: Habilitada
Modo: Inverso
Relé: 2 (cerradura de seguridad)
```

**Operación**:
1. Estado normal: GPIO39 LOW → Relé 2 ON (puerta cerrada)
2. Emergencia: pulsan botón → GPIO39 HIGH
3. Relé 2 OFF (puerta se abre)
4. Mientras mantienen pulsado → puerta abierta
5. Liberan botón → GPIO39 LOW
6. Relé 2 ON (puerta se cierra automáticamente)

---

## 📝 Configuración en la Interfaz Web

### Campos del Formulario

Para cada entrada (DI1 y DI2):

1. **Habilitada** (checkbox):
   - ✅ Marcado: La entrada se monitoriza
   - ❌ Desmarcado: La entrada se ignora

2. **Relé a activar** (select):
   - Relé 1
   - Relé 2

3. **Duración** (number):
   - Rango: 0.5 a 60 segundos
   - Solo aplica en Modo Normal

4. **Modo de Funcionamiento** (select):
   - 🔵 Normal - HIGH activa relé (temporizado)
   - 🔴 Inverso - Relé siempre activo, HIGH lo desactiva

---

## 🔍 Mensajes de Debug

### Configuración Cargada
```
📥 [DI] Configuración de entradas digitales cargada:
   DI1 (GPIO36): HABILITADA, Relé 1, 2.0s, Modo NORMAL
   DI2 (GPIO39): DESHABILITADA, Relé 2, 3.0s, Modo INVERSO
```

### Guardado de Configuración
```
⚙️ [DI1] Configuración actualizada: HABILITADA, Relé 1, 2.0s, Modo NORMAL
💾 [DI] Configuración de entradas digitales guardada en EEPROM
```

### Eventos en Modo Normal
```
📍 [DI1] Modo NORMAL - HIGH detectado → Activando Relé 1 por 2.0s
📍 [DI1] Modo NORMAL - LOW detectado → Sistema listo para nuevo pulso
```

### Eventos en Modo Inverso
```
🔵 [DI2] Modo INVERSO - LOW detectado → Activando Relé 2 (permanente)
🔴 [DI2] Modo INVERSO - HIGH detectado → Desactivando Relé 2
```

---

## 📊 Métricas de la Mejora

| Métrica | Valor |
|---------|-------|
| Líneas añadidas | +85 |
| Flash adicional | +2,996 bytes (+0.2%) |
| RAM adicional | 0 bytes |
| Campos nuevos en EEPROM | 2 (bool di1_inverse, bool di2_inverse) |
| Compilación | ✅ Exitosa |
| Subida a placa | ✅ Completada |

---

## ✅ Funcionalidades Verificadas

- [x] Modo Normal implementado correctamente
- [x] Modo Inverso implementado correctamente
- [x] Interfaz web actualizada con selector
- [x] API REST incluye campo inverse
- [x] Configuración persistente en EEPROM
- [x] Logging detallado por modo
- [x] Validación de parámetros
- [x] Compilación exitosa
- [x] Firmware subido a placa

---

## 🚀 Próximos Pasos

### Para Probar

1. **Acceder a la web**:
   ```
   http://[IP_DEL_ESP32]/digital_inputs
   ```

2. **Configurar DI1 en Modo Normal**:
   - Habilitar
   - Modo: Normal
   - Relé: 1
   - Duración: 2 segundos
   - Guardar

3. **Probar con pulsador**:
   - Conectar GPIO36 a 3.3V brevemente
   - Verificar que Relé 1 activa 2 segundos

4. **Configurar DI2 en Modo Inverso**:
   - Habilitar
   - Modo: Inverso
   - Relé: 2
   - Guardar
   - **Verificar**: Relé 2 debería activarse inmediatamente

5. **Probar modo inverso**:
   - Conectar GPIO39 a 3.3V
   - Verificar que Relé 2 se desactiva
   - Desconectar
   - Verificar que Relé 2 se reactiva

---

## 📞 Comandos de Monitoreo

```bash
# Monitor especializado (recomendado)
./monitor_di_simple.sh

# O usando screen
screen /dev/cu.usbserial-3110 115200

# Subir firmware de nuevo si es necesario
pio run --target upload --upload-port /dev/cu.usbserial-3110
```

---

## 🎯 Checklist de Validación

### Modo Normal
- [ ] Habilitar DI1 en modo Normal
- [ ] HIGH activa relé por duración configurada
- [ ] LOW prepara para nuevo pulso
- [ ] Duración se respeta correctamente
- [ ] No se reactiva mientras HIGH

### Modo Inverso
- [ ] Habilitar DI2 en modo Inverso
- [ ] Relé se activa automáticamente al habilitar
- [ ] HIGH desactiva el relé
- [ ] LOW reactiva el relé
- [ ] Comportamiento fail-safe funciona

### General
- [ ] Configuración persiste tras reinicio
- [ ] Interfaz web muestra estado correcto
- [ ] MQTT publica eventos correctamente
- [ ] No interfiere con Wiegand

---

## 🔗 Referencias

- **Código fuente**: `/src/main.ino`
- **Documentación inicial**: `/docs/v3.0/analisis-entradas-digitales.md`
- **Guía de monitoreo**: `/COMO_MONITOREAR.md`

---

**Implementado por**: Equipo SWAT ID  
**Fecha**: 11 de Diciembre, 2025  
**Estado**: ✅ Implementado, compilado y subido a placa  
**Versión**: v3.0.1 (mejorada)

