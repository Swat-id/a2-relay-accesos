# Formato JSON de Características BLE - SWATID-A2 v4.0

> **IMPORTANTE:** Este documento define el formato EXACTO de los JSON que devuelve el dispositivo. 
> Todos los JSON son generados con ArduinoJson y están garantizados como válidos.

---

## FF09 - Configuración Completa del Dispositivo

### UUID
```
0000FF09-0000-1000-8000-00805F9B34FB
```

### Operación
- **Read:** Devuelve la configuración completa en JSON

### Autenticación
- **Requerida:** Sí (debe estar autenticado previamente via FF01)

---

### Respuesta OK - Ejemplo Real

```json
{
  "device_type": "SWATID-A2",
  "serial": "SWATID_8813BFB4F7DB",
  "name": "Controladora Principal",
  "firmware": "v4.0.0",
  "network": {
    "ip": "192.168.5.86",
    "gateway": "192.168.5.1",
    "subnet": "255.255.255.0",
    "dns": "192.168.5.1",
    "mac": "88:13:BF:B4:F7:DB",
    "dhcp": true,
    "connected": true
  },
  "status": {
    "relay1": false,
    "relay2": false,
    "relay_duration": 3,
    "mode": "normal",
    "local_access_blocked": false,
    "keyboard_reading": true,
    "mqtt_connected": true
  },
  "codes": {
    "local_count": 5,
    "local_max": 100,
    "remote_count": 0,
    "validation_mode": "local_first"
  },
  "uptime_seconds": 12345
}
```

### Respuesta ERROR - No Autenticado

```json
{"error":"NOT_AUTHORIZED"}
```

### Respuesta ERROR - Overflow

```json
{"error":"JSON_OVERFLOW"}
```

---

### Tabla de Campos FF09

| Campo | Tipo | Siempre Presente | Valores Posibles | Descripción |
|-------|------|------------------|------------------|-------------|
| `device_type` | string | ✅ | `"SWATID-A2"` | Tipo de dispositivo |
| `serial` | string | ✅ | `"SWATID_XXXXXXXXXXXX"` | Número de serie (19 chars) |
| `name` | string | ✅ | cualquier string | Nombre configurado |
| `firmware` | string | ✅ | `"v4.0.0"` | Versión firmware |
| `network` | object | ✅ | - | Objeto con datos de red |
| `network.ip` | string | ✅ | `"X.X.X.X"` | IP actual asignada |
| `network.gateway` | string | ✅ | `"X.X.X.X"` | Gateway actual |
| `network.subnet` | string | ✅ | `"X.X.X.X"` | Máscara actual |
| `network.dns` | string | ✅ | `"X.X.X.X"` | DNS actual |
| `network.mac` | string | ✅ | `"XX:XX:XX:XX:XX:XX"` | MAC address |
| `network.dhcp` | boolean | ✅ | `true` / `false` | Usa DHCP |
| `network.connected` | boolean | ✅ | `true` / `false` | Ethernet conectado |
| `status` | object | ✅ | - | Objeto con estado |
| `status.relay1` | boolean | ✅ | `true` / `false` | Relé 1 activo |
| `status.relay2` | boolean | ✅ | `true` / `false` | Relé 2 activo |
| `status.relay_duration` | number | ✅ | 0.1 - 999.9 | Duración en segundos |
| `status.mode` | string | ✅ | `"normal"` / `"torno"` | Modo operación |
| `status.local_access_blocked` | boolean | ✅ | `true` / `false` | Acceso local bloqueado |
| `status.keyboard_reading` | boolean | ✅ | `true` / `false` | Teclados activos |
| `status.mqtt_connected` | boolean | ✅ | `true` / `false` | MQTT conectado |
| `codes` | object | ✅ | - | Objeto con info códigos |
| `codes.local_count` | number | ✅ | 0 - 100 | Códigos locales |
| `codes.local_max` | number | ✅ | `100` | Capacidad máxima |
| `codes.remote_count` | number | ✅ | 0 - 100 | Códigos remotos |
| `codes.validation_mode` | string | ✅ | `"local_first"` / `"remote_first"` | Prioridad validación |
| `uptime_seconds` | number | ✅ | 0 - N | Segundos desde boot |

---

## FF0A - Lista de Códigos Locales

### UUID
```
0000FF0A-0000-1000-8000-00805F9B34FB
```

### Operación
- **Read:** Devuelve la primera página de códigos (página 0)
- **Write:** Solicita una página específica con `{"page": N}`

### Autenticación
- **Requerida:** Sí (debe estar autenticado previamente via FF01)

---

### Respuesta OK - Página 0 (Read)

```json
{
  "count": 25,
  "max": 100,
  "validation_mode": "local_first",
  "page": 0,
  "page_size": 15,
  "total_pages": 2,
  "codes": [
    {
      "id": 0,
      "type": "PIN",
      "value": "123456",
      "keyboard": 1,
      "relay": 1
    },
    {
      "id": 1,
      "type": "TAG",
      "value": "A1B2C3D4",
      "keyboard": 0,
      "relay": 2
    }
  ],
  "has_more": true
}
```

### Solicitud de Página (Write)

**Escribir en FF0A:**
```json
{"page": 1}
```

**Respuesta Página 1:**
```json
{
  "count": 25,
  "page": 1,
  "page_size": 15,
  "total_pages": 2,
  "codes": [
    {
      "id": 15,
      "type": "TAG",
      "value": "DEADBEEF",
      "keyboard": 2,
      "relay": 1
    }
  ],
  "has_more": false
}
```

### Respuestas ERROR

```json
{"error":"NOT_AUTHORIZED"}
```

```json
{"error":"EMPTY_REQUEST"}
```

```json
{"error":"INVALID_JSON"}
```

```json
{"error":"MISSING_PAGE"}
```

```json
{"error":"INVALID_PAGE"}
```

```json
{"error":"JSON_OVERFLOW"}
```

---

### Tabla de Campos FF0A

| Campo | Tipo | Siempre Presente | Valores Posibles | Descripción |
|-------|------|------------------|------------------|-------------|
| `count` | number | ✅ | 0 - 100 | Total de códigos almacenados |
| `max` | number | ✅ (página 0) | `100` | Capacidad máxima |
| `validation_mode` | string | ✅ (página 0) | `"local_first"` / `"remote_first"` | Prioridad validación |
| `page` | number | ✅ | 0 - N | Número de página actual |
| `page_size` | number | ✅ | `15` | Códigos por página |
| `total_pages` | number | ✅ | 1 - 7 | Total de páginas |
| `codes` | array | ✅ | [] | Array de códigos |
| `codes[].id` | number | ✅ | 0 - 99 | Índice del código |
| `codes[].type` | string | ✅ | `"PIN"` / `"TAG"` | Tipo de código |
| `codes[].value` | string | ✅ | cualquier string | Valor del código |
| `codes[].keyboard` | number | ✅ | 0, 1, 2 | 0=ambos, 1=teclado1, 2=teclado2 |
| `codes[].relay` | number | ✅ | 1, 2 | Relé a activar |
| `has_more` | boolean | ✅ | `true` / `false` | Hay más páginas |

---

## Ejemplo de Código - Android (Kotlin)

```kotlin
// Parsear FF09
fun parseFF09(json: String): DeviceConfig? {
    return try {
        val obj = JSONObject(json)
        
        // Verificar si es error
        if (obj.has("error")) {
            Log.e("BLE", "Error FF09: ${obj.getString("error")}")
            return null
        }
        
        val network = obj.getJSONObject("network")
        val status = obj.getJSONObject("status")
        val codes = obj.getJSONObject("codes")
        
        DeviceConfig(
            deviceType = obj.getString("device_type"),
            serial = obj.getString("serial"),
            name = obj.getString("name"),
            firmware = obj.getString("firmware"),
            ip = network.getString("ip"),
            gateway = network.getString("gateway"),
            subnet = network.getString("subnet"),
            dns = network.getString("dns"),
            mac = network.getString("mac"),
            dhcp = network.getBoolean("dhcp"),
            connected = network.getBoolean("connected"),
            relay1 = status.getBoolean("relay1"),
            relay2 = status.getBoolean("relay2"),
            relayDuration = status.getDouble("relay_duration"),
            mode = status.getString("mode"),
            localAccessBlocked = status.getBoolean("local_access_blocked"),
            keyboardReading = status.getBoolean("keyboard_reading"),
            mqttConnected = status.getBoolean("mqtt_connected"),
            localCount = codes.getInt("local_count"),
            localMax = codes.getInt("local_max"),
            remoteCount = codes.getInt("remote_count"),
            validationMode = codes.getString("validation_mode"),
            uptimeSeconds = obj.getLong("uptime_seconds")
        )
    } catch (e: Exception) {
        Log.e("BLE", "Error parsing FF09: ${e.message}")
        null
    }
}

// Parsear FF0A
fun parseFF0A(json: String): CodesPage? {
    return try {
        val obj = JSONObject(json)
        
        // Verificar si es error
        if (obj.has("error")) {
            Log.e("BLE", "Error FF0A: ${obj.getString("error")}")
            return null
        }
        
        val codesArray = obj.getJSONArray("codes")
        val codes = mutableListOf<LocalCode>()
        
        for (i in 0 until codesArray.length()) {
            val code = codesArray.getJSONObject(i)
            codes.add(LocalCode(
                id = code.getInt("id"),
                type = code.getString("type"),
                value = code.getString("value"),
                keyboard = code.getInt("keyboard"),
                relay = code.getInt("relay")
            ))
        }
        
        CodesPage(
            count = obj.getInt("count"),
            max = obj.optInt("max", 100),
            validationMode = obj.optString("validation_mode", "local_first"),
            page = obj.getInt("page"),
            pageSize = obj.getInt("page_size"),
            totalPages = obj.getInt("total_pages"),
            hasMore = obj.getBoolean("has_more"),
            codes = codes
        )
    } catch (e: Exception) {
        Log.e("BLE", "Error parsing FF0A: ${e.message}")
        null
    }
}
```

---

## Ejemplo de Código - iOS (Swift)

```swift
// Modelos
struct DeviceConfig: Codable {
    let deviceType: String
    let serial: String
    let name: String
    let firmware: String
    let network: NetworkInfo
    let status: StatusInfo
    let codes: CodesInfo
    let uptimeSeconds: Int
    
    enum CodingKeys: String, CodingKey {
        case deviceType = "device_type"
        case serial, name, firmware, network, status, codes
        case uptimeSeconds = "uptime_seconds"
    }
}

struct NetworkInfo: Codable {
    let ip: String
    let gateway: String
    let subnet: String
    let dns: String
    let mac: String
    let dhcp: Bool
    let connected: Bool
}

struct StatusInfo: Codable {
    let relay1: Bool
    let relay2: Bool
    let relayDuration: Double
    let mode: String
    let localAccessBlocked: Bool
    let keyboardReading: Bool
    let mqttConnected: Bool
    
    enum CodingKeys: String, CodingKey {
        case relay1, relay2, mode
        case relayDuration = "relay_duration"
        case localAccessBlocked = "local_access_blocked"
        case keyboardReading = "keyboard_reading"
        case mqttConnected = "mqtt_connected"
    }
}

struct CodesInfo: Codable {
    let localCount: Int
    let localMax: Int
    let remoteCount: Int
    let validationMode: String
    
    enum CodingKeys: String, CodingKey {
        case localCount = "local_count"
        case localMax = "local_max"
        case remoteCount = "remote_count"
        case validationMode = "validation_mode"
    }
}

struct CodesPage: Codable {
    let count: Int
    let max: Int?
    let validationMode: String?
    let page: Int
    let pageSize: Int
    let totalPages: Int
    let hasMore: Bool
    let codes: [LocalCode]
    
    enum CodingKeys: String, CodingKey {
        case count, max, page, codes
        case validationMode = "validation_mode"
        case pageSize = "page_size"
        case totalPages = "total_pages"
        case hasMore = "has_more"
    }
}

struct LocalCode: Codable {
    let id: Int
    let type: String
    let value: String
    let keyboard: Int
    let relay: Int
}

// Parseo
func parseFF09(data: Data) -> DeviceConfig? {
    // Primero verificar si es error
    if let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
       let error = json["error"] as? String {
        print("Error FF09: \(error)")
        return nil
    }
    
    let decoder = JSONDecoder()
    return try? decoder.decode(DeviceConfig.self, from: data)
}

func parseFF0A(data: Data) -> CodesPage? {
    // Primero verificar si es error
    if let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
       let error = json["error"] as? String {
        print("Error FF0A: \(error)")
        return nil
    }
    
    let decoder = JSONDecoder()
    return try? decoder.decode(CodesPage.self, from: data)
}
```

---

## Flujo Recomendado para la APP

```
1. Conectar al dispositivo BLE (nombre = serial)
2. Autenticar via FF01 con clave de 64 bytes
3. Esperar respuesta "OK:..." en FF01
4. Leer FF09 para obtener configuración completa
5. Verificar que no hay error en respuesta
6. Leer FF0A para obtener primera página de códigos
7. Si has_more == true, solicitar más páginas con {"page": N}
8. Repetir hasta has_more == false
```

---

## Notas Importantes

1. **Todos los campos son UTF-8**
2. **Todos los booleanos son `true` o `false` (minúsculas)**
3. **Los números no llevan comillas**
4. **Los strings siempre llevan comillas dobles**
5. **El array `codes` puede estar vacío `[]`**
6. **Si hay error, solo existe el campo `error`**
7. **Tamaño máximo de respuesta: ~1500 bytes**

---

*Última actualización: 26 Febrero 2026*
