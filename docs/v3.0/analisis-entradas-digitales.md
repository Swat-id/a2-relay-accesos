# Análisis de Implementación - Entradas Digitales (DI1/DI2)

## 📋 Información General

- **Fecha de Análisis**: 11 de Diciembre, 2025
- **Rama**: v3.0
- **Versión Base**: v3.0.0
- **Funcionalidad**: Control de relés mediante entradas digitales
- **Estado**: En análisis

---

## 🎯 Objetivo de la Funcionalidad

Implementar la gestión de dos entradas digitales (DI1 y DI2) que permitan activar los relés de forma independiente mediante pulsos externos, sin afectar las funcionalidades existentes del sistema.

---

## 📊 Especificaciones

### Hardware

#### Pines Asignados
- **DI1 (Entrada Digital 1)**: GPIO 36
- **DI2 (Entrada Digital 2)**: GPIO 39

**Características de los pines**:
- Pines **input-only** del ESP32
- No tienen resistencias pull-up/pull-down internas
- Ideales para entradas digitales
- ADC1_CH0 (GPIO36) y ADC1_CH3 (GPIO39)
- Voltaje de lectura: 0-3.3V

#### Verificación de Disponibilidad

**Pines actualmente en uso**:
```
- GPIO 15: RELE1_PIN (salida)
- GPIO 2:  RELE2_PIN (salida)
- GPIO 33: WIEGAND1_D0 (entrada con interrupción)
- GPIO 14: WIEGAND1_D1 (entrada con interrupción)
- GPIO 4:  WIEGAND2_D0 (entrada con interrupción)
- GPIO 16: WIEGAND2_D1 (entrada con interrupción)
- GPIO 5:  ETH_PHY_POWER_PIN (salida)
- GPIO 17: ETH_CLK (salida)
- GPIO 23: ETH_MDC (salida)
- GPIO 18: ETH_MDIO (bidireccional)
- GPIO 19: ETH_TXD0 (salida)
- GPIO 22: ETH_TXEN (salida)
- GPIO 25: ETH_RXD0 (entrada)
- GPIO 26: ETH_RXD1 (entrada)
- GPIO 27: ETH_CRS_DV (entrada)
- GPIO 32: RS485_RX2 (entrada)
- GPIO 13: RS485_TX2 (salida)
```

✅ **GPIO 36 y GPIO 39 están LIBRES y disponibles**

---

## 🔧 Especificaciones Funcionales

### Comportamiento Requerido

#### Detección de Pulsos
1. **Flanco de Subida**: Detectar transición LOW → HIGH
2. **Activación del Relé**: Al detectar flanco de subida, activar el relé configurado
3. **Duración**: El relé permanece activo durante el tiempo configurado
4. **No Re-activación**: Si el pulso continúa activo al finalizar el tiempo, NO se reinicia
5. **Nuevo Pulso**: Solo un nuevo flanco de subida (después de un LOW) reinicia el conteo

#### Diagrama de Estados

```
Estado Inicial: LOW (sin pulso)
                ↓
    [Detecta HIGH] → Activa Relé → Inicia Timer
                ↓                        ↓
          Pulso activo            Timer corriendo
                ↓                        ↓
        [Timer finaliza] → Desactiva Relé
                ↓                        ↓
          Pulso aún HIGH         Espera pulso LOW
                ↓                        ↓
          [Detecta LOW] ← Estado Inicial
                ↓
          [Detecta HIGH] → Nuevo ciclo
```

#### Ejemplo de Operación

```
Tiempo:    0s   1s   2s   3s   4s   5s   6s   7s   8s
Pulso DI1: ___┌────┐________┌──────────────┐_______
Relé 1:    ___┌──────┐______┌──────┐_______________
           (2s duración)    (2s dur.)

Explicación:
- t=0s: Flanco subida → Activa Relé (2s configurados)
- t=2s: Timer expira → Desactiva Relé (pulso aún HIGH)
- t=3s: Pulso baja → Sistema listo
- t=4s: Nuevo flanco → Activa Relé (2s)
- t=6s: Timer expira → Desactiva Relé
- t=7s: Pulso baja → Sistema listo
```

---

## 🗄️ Estructura de Datos

### Configuración en EEPROM

```cpp
// Estructura para configuración de entradas digitales
struct DigitalInputConfig {
  bool di1_enabled;        // DI1 habilitada/deshabilitada
  uint8_t di1_relay;       // Relé asignado a DI1 (1 o 2)
  float di1_duration;      // Duración de activación DI1 (segundos)
  
  bool di2_enabled;        // DI2 habilitada/deshabilitada
  uint8_t di2_relay;       // Relé asignado a DI2 (1 o 2)
  float di2_duration;      // Duración de activación DI2 (segundos)
  
  uint8_t di1_mode;        // Modo de activación (0=flanco, 1=nivel)
  uint8_t di2_mode;        // Modo de activación (0=flanco, 1=nivel)
  
  uint32_t validMarker;    // Marcador de validación (0xDIGI7A1)
};
```

**Tamaño**: ~20 bytes  
**Offset propuesto**: 9700 (hay espacio disponible en EEPROM de 10240 bytes)

### Variables de Estado en Runtime

```cpp
// Pines de entradas digitales
const int DI1_PIN = 36;
const int DI2_PIN = 39;

// Estado de las entradas digitales
struct DigitalInputState {
  bool lastState;          // Último estado leído (LOW/HIGH)
  bool currentState;       // Estado actual
  bool relayActivated;     // Relé activado por esta entrada
  unsigned long activationTime; // Momento de activación
  bool waitingForLow;      // Esperando que el pulso baje
};

DigitalInputState di1State = {false, false, false, 0, false};
DigitalInputState di2State = {false, false, false, 0, false};

DigitalInputConfig digitalInputConfig;
```

---

## 💻 Implementación del Código

### 1. Definiciones y Estructuras (línea ~240)

```cpp
// =================== CONFIGURACIÓN DE ENTRADAS DIGITALES ===================
const int DI1_PIN = 36;  // Entrada digital 1
const int DI2_PIN = 39;  // Entrada digital 2

struct DigitalInputConfig {
  bool di1_enabled;
  uint8_t di1_relay;
  float di1_duration;
  bool di2_enabled;
  uint8_t di2_relay;
  float di2_duration;
  uint8_t di1_mode;        // 0=flanco, 1=nivel (futuro)
  uint8_t di2_mode;        // 0=flanco, 1=nivel (futuro)
  uint32_t validMarker;    // 0xDIGI7A1
};

struct DigitalInputState {
  bool lastState;
  bool currentState;
  bool relayActivated;
  unsigned long activationTime;
  bool waitingForLow;
};

#define DIGITAL_INPUT_CONFIG_MARKER 0xDIGI7A1
#define EEPROM_DIGITAL_INPUT_OFFSET 9700

DigitalInputConfig digitalInputConfig;
DigitalInputState di1State = {false, false, false, 0, false};
DigitalInputState di2State = {false, false, false, 0, false};
```

### 2. Funciones de Configuración

```cpp
// =================== GESTIÓN DE ENTRADAS DIGITALES ===================

// Cargar configuración desde EEPROM
void loadDigitalInputConfig() {
  EEPROM.get(EEPROM_DIGITAL_INPUT_OFFSET, digitalInputConfig);
  
  if (digitalInputConfig.validMarker != DIGITAL_INPUT_CONFIG_MARKER) {
    Serial.println("⚙️ Configuración de entradas digitales no válida, usando valores por defecto");
    
    // Valores por defecto
    digitalInputConfig.di1_enabled = false;
    digitalInputConfig.di1_relay = 1;
    digitalInputConfig.di1_duration = 2.0;
    
    digitalInputConfig.di2_enabled = false;
    digitalInputConfig.di2_relay = 2;
    digitalInputConfig.di2_duration = 2.0;
    
    digitalInputConfig.di1_mode = 0;  // Flanco
    digitalInputConfig.di2_mode = 0;  // Flanco
    
    digitalInputConfig.validMarker = DIGITAL_INPUT_CONFIG_MARKER;
    
    saveDigitalInputConfig();
  }
  
  Serial.println("📥 Configuración de entradas digitales cargada:");
  Serial.printf("   DI1: %s, Relé %d, %.1fs\n", 
                digitalInputConfig.di1_enabled ? "HABILITADA" : "DESHABILITADA",
                digitalInputConfig.di1_relay,
                digitalInputConfig.di1_duration);
  Serial.printf("   DI2: %s, Relé %d, %.1fs\n", 
                digitalInputConfig.di2_enabled ? "HABILITADA" : "DESHABILITADA",
                digitalInputConfig.di2_relay,
                digitalInputConfig.di2_duration);
}

// Guardar configuración en EEPROM
void saveDigitalInputConfig() {
  digitalInputConfig.validMarker = DIGITAL_INPUT_CONFIG_MARKER;
  EEPROM.put(EEPROM_DIGITAL_INPUT_OFFSET, digitalInputConfig);
  EEPROM.commit();
  Serial.println("💾 Configuración de entradas digitales guardada");
}

// Procesar entrada digital (detectar flancos)
void processDigitalInput(int inputNumber, DigitalInputState &state, 
                        bool enabled, uint8_t relay, float duration) {
  if (!enabled) return;
  
  // Leer estado actual
  int pin = (inputNumber == 1) ? DI1_PIN : DI2_PIN;
  state.currentState = digitalRead(pin);
  
  // Detectar flanco de subida (LOW → HIGH)
  if (!state.lastState && state.currentState && !state.waitingForLow) {
    Serial.printf("📍 [DI%d] Flanco de subida detectado → Activando Relé %d por %.1fs\n", 
                  inputNumber, relay, duration);
    
    // Activar relé
    controlReleWithDuration(duration, relay);
    
    state.relayActivated = true;
    state.activationTime = millis();
    state.waitingForLow = true;
    
    // Publicar evento MQTT
    publishDigitalInputEvent(inputNumber, relay, duration);
  }
  
  // Detectar flanco de bajada (HIGH → LOW)
  if (state.lastState && !state.currentState) {
    Serial.printf("📍 [DI%d] Flanco de bajada detectado → Sistema listo para nuevo pulso\n", 
                  inputNumber);
    state.waitingForLow = false;
  }
  
  // Actualizar último estado
  state.lastState = state.currentState;
}

// Publicar evento de entrada digital via MQTT
void publishDigitalInputEvent(int inputNumber, int relay, float duration) {
  if (!mqttClient.connected()) return;
  
  DynamicJsonDocument doc(256);
  doc["timestamp"] = getTimestamp();
  doc["event"] = "digital_input_trigger";
  doc["input"] = inputNumber;
  doc["relay"] = relay;
  doc["duration"] = duration;
  doc["message_id"] = String(messageId++);
  
  String output;
  serializeJson(doc, output);
  String topic = "swatidhome/" + fixedSerialNumber + "/digital_input";
  
  mqttClient.publish(topic.c_str(), output.c_str());
}
```

### 3. Modificación del setup() (línea ~5620)

```cpp
// Después de configurar relés y antes de Wiegand

// =================== CONFIGURACIÓN DE ENTRADAS DIGITALES ===================
pinMode(DI1_PIN, INPUT);  // GPIO36 no tiene pull-up interna
pinMode(DI2_PIN, INPUT);  // GPIO39 no tiene pull-up interna

// Inicializar estados
di1State.lastState = digitalRead(DI1_PIN);
di1State.currentState = di1State.lastState;
di2State.lastState = digitalRead(DI2_PIN);
di2State.currentState = di2State.lastState;

Serial.printf("🔌 Entradas digitales configuradas: DI1=GPIO%d, DI2=GPIO%d\n", 
              DI1_PIN, DI2_PIN);
Serial.printf("   Estado inicial: DI1=%s, DI2=%s\n",
              di1State.lastState ? "HIGH" : "LOW",
              di2State.lastState ? "HIGH" : "LOW");

loadDigitalInputConfig();
```

### 4. Modificación del loop() (línea ~5740)

```cpp
// Después de processRS485Keypad() y antes de checkPendingRequestTimeout()

// ========== PROCESAMIENTO DE ENTRADAS DIGITALES ==========
processDigitalInput(1, di1State, digitalInputConfig.di1_enabled, 
                   digitalInputConfig.di1_relay, digitalInputConfig.di1_duration);
processDigitalInput(2, di2State, digitalInputConfig.di2_enabled,
                   digitalInputConfig.di2_relay, digitalInputConfig.di2_duration);
```

---

## 🌐 Interfaz Web

### Nueva Página: `/digital_inputs`

```html
<!DOCTYPE html>
<html>
<head>
  <title>Entradas Digitales</title>
  <meta charset="UTF-8">
  <style>
    /* Usar estilos existentes del sistema */
    .input-config {
      background: #f5f5f5;
      padding: 20px;
      margin: 10px 0;
      border-radius: 8px;
      border: 2px solid #ddd;
    }
    .input-config.enabled {
      border-color: #4CAF50;
      background: #f1f8f4;
    }
    .status-indicator {
      display: inline-block;
      width: 12px;
      height: 12px;
      border-radius: 50%;
      margin-right: 8px;
    }
    .status-low { background: #ccc; }
    .status-high { background: #4CAF50; }
  </style>
</head>
<body>
  <h1>⚡ Entradas Digitales</h1>
  
  <!-- Estado en Tiempo Real -->
  <div class="status-panel">
    <h3>📊 Estado Actual</h3>
    <p>
      <span class="status-indicator status-low" id="di1-status"></span>
      DI1 (GPIO36): <span id="di1-value">LOW</span>
    </p>
    <p>
      <span class="status-indicator status-low" id="di2-status"></span>
      DI2 (GPIO39): <span id="di2-value">LOW</span>
    </p>
  </div>
  
  <!-- Configuración DI1 -->
  <div class="input-config" id="di1-config">
    <h3>🔌 Entrada Digital 1 (DI1 - GPIO36)</h3>
    <form action="/save_digital_input" method="POST">
      <input type="hidden" name="input" value="1">
      
      <label>
        <input type="checkbox" name="di1_enabled" id="di1_enabled" value="1">
        Habilitada
      </label><br><br>
      
      <label>Relé a activar:</label>
      <select name="di1_relay" id="di1_relay">
        <option value="1">Relé 1</option>
        <option value="2">Relé 2</option>
      </select><br><br>
      
      <label>Duración (segundos):</label>
      <input type="number" name="di1_duration" id="di1_duration" 
             min="0.5" max="60" step="0.5" value="2.0"><br><br>
      
      <button type="submit">💾 Guardar Configuración DI1</button>
    </form>
  </div>
  
  <!-- Configuración DI2 -->
  <div class="input-config" id="di2-config">
    <h3>🔌 Entrada Digital 2 (DI2 - GPIO39)</h3>
    <form action="/save_digital_input" method="POST">
      <input type="hidden" name="input" value="2">
      
      <label>
        <input type="checkbox" name="di2_enabled" id="di2_enabled" value="1">
        Habilitada
      </label><br><br>
      
      <label>Relé a activar:</label>
      <select name="di2_relay" id="di2_relay">
        <option value="1">Relé 1</option>
        <option value="2">Relé 2</option>
      </select><br><br>
      
      <label>Duración (segundos):</label>
      <input type="number" name="di2_duration" id="di2_duration" 
             min="0.5" max="60" step="0.5" value="2.0"><br><br>
      
      <button type="submit">💾 Guardar Configuración DI2</button>
    </form>
  </div>
  
  <!-- Información -->
  <div class="info-panel">
    <h3>ℹ️ Información</h3>
    <ul>
      <li>DI1 y DI2 son entradas digitales de 3.3V</li>
      <li>Detección por flanco de subida (0V → 3.3V)</li>
      <li>El relé se activa durante el tiempo configurado</li>
      <li>No se reactiva hasta que el pulso baje y vuelva a subir</li>
      <li>No interfiere con teclados Wiegand ni otras funcionalidades</li>
    </ul>
  </div>
  
  <script>
    // Auto-refresh del estado cada 500ms
    setInterval(function() {
      fetch('/api/digital_inputs_status')
        .then(response => response.json())
        .then(data => {
          // Actualizar DI1
          document.getElementById('di1-value').textContent = data.di1_state ? 'HIGH' : 'LOW';
          document.getElementById('di1-status').className = 
            'status-indicator ' + (data.di1_state ? 'status-high' : 'status-low');
          
          // Actualizar DI2
          document.getElementById('di2-value').textContent = data.di2_state ? 'HIGH' : 'LOW';
          document.getElementById('di2-status').className = 
            'status-indicator ' + (data.di2_state ? 'status-high' : 'status-low');
          
          // Actualizar clases de configuración
          document.getElementById('di1-config').className = 
            'input-config' + (data.di1_enabled ? ' enabled' : '');
          document.getElementById('di2-config').className = 
            'input-config' + (data.di2_enabled ? ' enabled' : '');
        });
    }, 500);
    
    // Cargar configuración actual
    fetch('/api/digital_inputs_config')
      .then(response => response.json())
      .then(data => {
        document.getElementById('di1_enabled').checked = data.di1_enabled;
        document.getElementById('di1_relay').value = data.di1_relay;
        document.getElementById('di1_duration').value = data.di1_duration;
        
        document.getElementById('di2_enabled').checked = data.di2_enabled;
        document.getElementById('di2_relay').value = data.di2_relay;
        document.getElementById('di2_duration').value = data.di2_duration;
      });
  </script>
</body>
</html>
```

### Handlers del Servidor Web

```cpp
// Handler para la página de entradas digitales
void handleDigitalInputs() {
  if (!checkWebAuth()) return;
  
  String html = R"rawliteral(
    <!-- HTML de la página de entradas digitales -->
  )rawliteral";
  
  webServer.send(200, "text/html", html);
}

// API para obtener estado actual
void handleDigitalInputsStatus() {
  if (!checkWebAuth()) return;
  
  DynamicJsonDocument doc(256);
  doc["di1_state"] = di1State.currentState;
  doc["di2_state"] = di2State.currentState;
  doc["di1_enabled"] = digitalInputConfig.di1_enabled;
  doc["di2_enabled"] = digitalInputConfig.di2_enabled;
  doc["di1_waiting"] = di1State.waitingForLow;
  doc["di2_waiting"] = di2State.waitingForLow;
  
  String output;
  serializeJson(doc, output);
  webServer.send(200, "application/json", output);
}

// API para obtener configuración
void handleDigitalInputsConfig() {
  if (!checkWebAuth()) return;
  
  DynamicJsonDocument doc(256);
  doc["di1_enabled"] = digitalInputConfig.di1_enabled;
  doc["di1_relay"] = digitalInputConfig.di1_relay;
  doc["di1_duration"] = digitalInputConfig.di1_duration;
  doc["di2_enabled"] = digitalInputConfig.di2_enabled;
  doc["di2_relay"] = digitalInputConfig.di2_relay;
  doc["di2_duration"] = digitalInputConfig.di2_duration;
  
  String output;
  serializeJson(doc, output);
  webServer.send(200, "application/json", output);
}

// Handler para guardar configuración
void handleSaveDigitalInput() {
  if (!checkWebAuth()) return;
  
  int inputNumber = webServer.arg("input").toInt();
  
  if (inputNumber == 1) {
    digitalInputConfig.di1_enabled = webServer.hasArg("di1_enabled");
    digitalInputConfig.di1_relay = webServer.arg("di1_relay").toInt();
    digitalInputConfig.di1_duration = webServer.arg("di1_duration").toFloat();
  } else if (inputNumber == 2) {
    digitalInputConfig.di2_enabled = webServer.hasArg("di2_enabled");
    digitalInputConfig.di2_relay = webServer.arg("di2_relay").toInt();
    digitalInputConfig.di2_duration = webServer.arg("di2_duration").toFloat();
  }
  
  saveDigitalInputConfig();
  
  webServer.sendHeader("Location", "/digital_inputs");
  webServer.send(303);
}

// Registrar handlers en setup()
webServer.on("/digital_inputs", handleDigitalInputs);
webServer.on("/api/digital_inputs_status", handleDigitalInputsStatus);
webServer.on("/api/digital_inputs_config", handleDigitalInputsConfig);
webServer.on("/save_digital_input", HTTP_POST, handleSaveDigitalInput);
```

---

## 🔒 Análisis de Concurrencia y No Interferencia

### 1. Sistema Wiegand (Interrupciones)

**Mecanismo actual**:
- Usa interrupciones por hardware en GPIOs 33, 14, 4, 16
- ISRs extremadamente rápidas (solo modifican variables volátiles)
- Procesamiento en el loop principal

**Impacto de las entradas digitales**:
- ✅ **CERO IMPACTO**: Las entradas digitales usan polling simple
- ✅ No usan interrupciones
- ✅ No bloquean el procesador
- ✅ Lectura de `digitalRead()` es instantánea (~1-2µs)

### 2. Sistema de Relés

**Mecanismo actual**:
- Control mediante variables de estado globales
- Timer basado en `millis()`
- Función `checkRelayTimeout()` en el loop

**Compatibilidad**:
- ✅ **TOTALMENTE COMPATIBLE**: Usa la misma función `controlReleWithDuration()`
- ✅ Respeta el sistema de timeout existente
- ✅ No hay conflictos si dos fuentes activan el mismo relé (última gana)

### 3. Procesamiento MQTT

**Carga actual**:
- Procesamiento de mensajes en `mqttClient.loop()`
- Publicación de eventos

**Impacto**:
- ✅ **MÍNIMO**: Solo se publica al detectar flanco (evento poco frecuente)
- ✅ No bloquea el loop
- ✅ Tamaño de mensaje pequeño (~100 bytes)

### 4. Performance del Loop

**Tiempo de ejecución adicional**:
- 2x `digitalRead()`: ~2-4µs total
- Comparaciones lógicas: <1µs
- **Total por iteración: ~5µs**

**Frecuencia del loop actual**: ~10,000 iteraciones/segundo  
**Con entradas digitales**: ~9,998 iteraciones/segundo  
**Impacto**: < 0.02%

### 5. Uso de Memoria

**RAM adicional**:
```
DigitalInputConfig (global):    20 bytes
DigitalInputState x2 (global):  26 bytes
Variables locales:              ~50 bytes (temporales)
--------------------------------------------------
Total adicional:                ~100 bytes
```

**RAM actual**: 50,180 bytes (15.3%)  
**RAM con entradas**: 50,280 bytes (15.33%)  
**Impacto**: +0.03%

**EEPROM adicional**: 20 bytes (9700-9720)  
**EEPROM usado**: ~3500 bytes  
**Espacio disponible**: 6740 bytes  
**Impacto**: Despreciable

---

## ⚠️ Consideraciones Técnicas

### 1. Hardware

#### Resistencias Pull-Up Externas Recomendadas
Los pines 36 y 39 NO tienen resistencias pull-up/pull-down internas. Se recomienda:

```
               3.3V
                |
              [10kΩ]  (pull-up)
                |
     Señal ----+---- GPIO36 (o GPIO39)
                |
              [100nF]  (filtro anti-rebotes)
                |
               GND
```

#### Protección de Entrada
- **Diodo Schottky** a 3.3V (protección sobretensión)
- **Resistencia serie** 1kΩ (protección sobrecorriente)

### 2. Software

#### Anti-Rebotes
El diseño basado en flancos proporciona anti-rebotes natural:
- Solo se activa en transición LOW→HIGH
- El tiempo de activación del relé actúa como blanking
- No se reactiva hasta nuevo ciclo completo

#### Latencia
- **Detección de pulso**: <1ms (limitado por frecuencia del loop)
- **Activación del relé**: <5ms (desde detección hasta GPIO HIGH)

#### Prioridad
- Los teclados Wiegand tienen prioridad absoluta (interrupciones)
- Las entradas digitales se procesan en el loop (menor prioridad)
- No hay riesgo de pérdida de datos Wiegand

---

## 🧪 Plan de Pruebas

### Pruebas Unitarias

#### Test 1: Pulso Simple
```
Entrada: Pulso de 100ms en DI1
Configuración: DI1 → Relé 1, 2 segundos
Esperado:
- Relé 1 activa
- Permanece activo 2 segundos
- Desactiva automáticamente
```

#### Test 2: Pulso Largo
```
Entrada: Pulso de 5 segundos en DI1  
Configuración: DI1 → Relé 1, 2 segundos
Esperado:
- Relé 1 activa
- Permanece activo 2 segundos
- Desactiva a los 2 segundos (pulso aún activo)
- NO reactiva mientras pulso esté alto
```

#### Test 3: Pulsos Múltiples
```
Entrada: 3 pulsos de 100ms separados 500ms en DI1
Configuración: DI1 → Relé 1, 1 segundo
Esperado:
- Primer pulso → Activa relé 1s
- Segundo pulso → Activa relé 1s
- Tercer pulso → Activa relé 1s
```

#### Test 4: Dos Entradas Simultáneas
```
Entrada: Pulso en DI1 y DI2 simultáneos
Configuración: DI1 → Relé 1 (2s), DI2 → Relé 2 (3s)
Esperado:
- Ambos relés activan
- Relé 1 desactiva a los 2s
- Relé 2 desactiva a los 3s
```

### Pruebas de Integración

#### Test 5: Entrada Digital + Wiegand
```
Entrada: Pulso DI1 + código PIN simultáneos
Esperado:
- Código PIN se procesa correctamente
- DI1 activa su relé
- No hay interferencia mutua
```

#### Test 6: Entrada Digital + MQTT
```
Entrada: Pulso DI1 durante envío MQTT
Esperado:
- Mensaje MQTT se envía correctamente
- DI1 se detecta y procesa
- Evento de DI1 se publica a MQTT
```

### Pruebas de Estrés

#### Test 7: Pulsos Rápidos
```
Entrada: 100 pulsos de 50ms con 50ms entre ellos
Esperado:
- Todos los pulsos se detectan
- Relé activa en cada pulso
- No hay pérdida de eventos
```

#### Test 8: Operación Prolongada
```
Entrada: Sistema funcionando 24h con pulsos cada 5 minutos
Esperado:
- Todos los pulsos procesados correctamente
- Sin degradación de rendimiento
- Sin memory leaks
- Sin reinicios
```

---

## 📊 Métricas de Éxito

### Funcionalidad
- ✅ Detección de pulsos: 100% precisión
- ✅ Activación de relés: 100% confiabilidad
- ✅ Respeto de tiempos configurados: ±50ms tolerancia

### Performance
- ✅ Latencia de detección: <1ms
- ✅ Impacto en loop: <0.1%
- ✅ Uso de RAM: <0.5% adicional

### Integración
- ✅ Sin interferencia con Wiegand: 0 eventos perdidos
- ✅ Sin interferencia con MQTT: 0 mensajes perdidos
- ✅ Sin interferencia con relés: Operación simultánea correcta

---

## 🚀 Plan de Implementación

### Fase 1: Código Base (1-2 horas)
1. Añadir estructuras de datos
2. Implementar funciones de configuración EEPROM
3. Implementar lógica de detección de flancos
4. Integrar en setup() y loop()

### Fase 2: Interfaz Web (2-3 horas)
1. Crear página HTML de configuración
2. Implementar handlers de servidor
3. Crear API REST para estado y configuración
4. Implementar actualización en tiempo real

### Fase 3: Pruebas (2-3 horas)
1. Pruebas unitarias básicas
2. Pruebas de integración
3. Pruebas de estrés
4. Validación en hardware real

### Fase 4: Documentación (1 hora)
1. Actualizar documentación técnica
2. Crear guía de usuario
3. Documentar ejemplos de uso
4. Actualizar release notes

**Tiempo total estimado**: 6-9 horas

---

## 📝 Documentación Adicional Necesaria

1. **Manual de Usuario**
   - Cómo conectar dispositivos externos
   - Configuración de entradas digitales
   - Ejemplos de aplicación

2. **Guía de Hardware**
   - Esquema de conexión
   - Especificaciones eléctricas
   - Recomendaciones de cableado

3. **Troubleshooting**
   - Problemas comunes
   - Diagnóstico de fallos
   - Soluciones

---

## ✅ Checklist de Implementación

### Código
- [ ] Definir estructuras de datos
- [ ] Implementar funciones de EEPROM
- [ ] Implementar detección de flancos
- [ ] Integrar en setup()
- [ ] Integrar en loop()
- [ ] Implementar handlers web
- [ ] Implementar API REST

### Interfaz
- [ ] Diseñar página HTML
- [ ] Implementar formularios
- [ ] Añadir visualización en tiempo real
- [ ] Integrar con menú principal

### Pruebas
- [ ] Test de pulso simple
- [ ] Test de pulso largo
- [ ] Test de pulsos múltiples
- [ ] Test de entradas simultáneas
- [ ] Test con Wiegand
- [ ] Test con MQTT
- [ ] Test de estrés

### Documentación
- [ ] Actualizar README
- [ ] Crear guía de usuario
- [ ] Documentar API
- [ ] Actualizar release notes

---

## 🔗 Referencias

- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [GPIO Input-Only Pins](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
- Documentación actual del proyecto: `/docs/v3.0/README.md`

---

**Documento creado por**: Equipo SWAT ID  
**Fecha**: 11 de Diciembre, 2025  
**Versión**: 1.0  
**Estado**: Análisis completado - Listo para implementación

