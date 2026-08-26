# Lectura de Configuración vía BLE - SWATID-A2 v4.0

## Resumen

Este documento detalla cómo obtener la configuración completa del dispositivo SWATID-A2 mediante BLE. Existen **3 características** para obtener información:

| UUID | Característica | Autenticación | Contenido |
|------|----------------|---------------|-----------|
| `FF05` | Config Red | **Sí** | Configuración de red (IP, gateway, DNS, etc.) |
| `FF09` | Info Completa | **Sí** | Toda la información del dispositivo |
| `FF0A` | Lista Códigos | **Sí** | Códigos locales configurados |

**IMPORTANTE:** Todas estas características devuelven datos en formato **JSON UTF-8**.

---

## 1. Configuración de Red (FF05)

### UUID Completo
```
0000FF05-0000-1000-8000-00805F9B34FB
```

### Operación
- **Leer (Read):** Obtiene la configuración de red actual

### Formato de Respuesta (JSON)

```json
{
  "dhcp": true,
  "connected": true,
  "ip": "192.168.5.86",
  "gateway": "192.168.5.1",
  "subnet": "255.255.255.0",
  "dns": "192.168.5.1",
  "mac": "88:13:BF:B4:F7:DB",
  "config_ip": "auto",
  "config_gateway": "auto",
  "config_subnet": "auto",
  "config_dns": "auto"
}
```

### Campos

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `dhcp` | boolean | `true` = DHCP activo, `false` = IP fija |
| `connected` | boolean | `true` = Ethernet conectado |
| `ip` | string | **IP REAL asignada actualmente** (ej: "192.168.5.86") |
| `gateway` | string | Gateway real actual |
| `subnet` | string | Máscara de subred real |
| `dns` | string | DNS real actual |
| `mac` | string | Dirección MAC del dispositivo |
| `config_ip` | string | IP configurada ("auto" si DHCP) |
| `config_gateway` | string | Gateway configurado ("auto" si DHCP) |
| `config_subnet` | string | Máscara configurada ("auto" si DHCP) |
| `config_dns` | string | DNS configurado ("auto" si DHCP) |

### Tamaño Típico
- ~230 bytes

### Ejemplo de Parseo - Android (Kotlin)

```kotlin
fun parseNetworkConfig(jsonString: String): NetworkConfig {
    val json = JSONObject(jsonString)
    
    return NetworkConfig(
        dhcp = json.getBoolean("dhcp"),
        connected = json.getBoolean("connected"),
        ip = json.getString("ip"),
        gateway = json.getString("gateway"),
        subnet = json.getString("subnet"),
        dns = json.getString("dns"),
        mac = json.getString("mac"),
        configIp = json.getString("config_ip"),
        configGateway = json.getString("config_gateway"),
        configSubnet = json.getString("config_subnet"),
        configDns = json.getString("config_dns")
    )
}

data class NetworkConfig(
    val dhcp: Boolean,
    val connected: Boolean,
    val ip: String,
    val gateway: String,
    val subnet: String,
    val dns: String,
    val mac: String,
    val configIp: String,
    val configGateway: String,
    val configSubnet: String,
    val configDns: String
)
```

### Ejemplo de Parseo - iOS (Swift)

```swift
struct NetworkConfig: Codable {
    let dhcp: Bool
    let connected: Bool
    let ip: String
    let gateway: String
    let subnet: String
    let dns: String
    let mac: String
    let configIp: String
    let configGateway: String
    let configSubnet: String
    let configDns: String
    
    enum CodingKeys: String, CodingKey {
        case dhcp, connected, ip, gateway, subnet, dns, mac
        case configIp = "config_ip"
        case configGateway = "config_gateway"
        case configSubnet = "config_subnet"
        case configDns = "config_dns"
    }
}

func parseNetworkConfig(data: Data) -> NetworkConfig? {
    let decoder = JSONDecoder()
    return try? decoder.decode(NetworkConfig.self, from: data)
}
```

---

## 2. Información Completa del Dispositivo (FF09)

### UUID Completo
```
0000FF09-0000-1000-8000-00805F9B34FB
```

### Operación
- **Leer (Read):** Obtiene toda la información del dispositivo

### Formato de Respuesta (JSON)

```json
{
  "device_type": "SWATID-A2",
  "serial": "SWATID_8813BFB4F7DB",
  "name": "Controladora Principal",
  "firmware": "4.0.0-BLE",
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
    "relay_duration": 3.0,
    "mode": "normal",
    "local_access_blocked": false,
    "keyboard_reading": true,
    "mqtt_connected": true
  },
  "codes": {
    "local_count": 1,
    "local_max": 50,
    "remote_count": 0,
    "validation_mode": "local_first"
  },
  "uptime_seconds": 12345
}
```

### Campos Principales

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `device_type` | string | Tipo de dispositivo: "SWATID-A2" |
| `serial` | string | Número de serie único |
| `name` | string | Nombre configurado |
| `firmware` | string | Versión del firmware |

### Campos de Red (`network`)

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `network.ip` | string | **IP REAL actual** |
| `network.gateway` | string | **Gateway REAL actual** |
| `network.subnet` | string | **Máscara REAL actual** |
| `network.dns` | string | **DNS REAL actual** |
| `network.mac` | string | Dirección MAC |
| `network.dhcp` | boolean | `true` = DHCP, `false` = IP fija |
| `network.connected` | boolean | Estado de conexión Ethernet |

### Campos de Estado (`status`)

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `status.relay1` | boolean | Estado relé 1 (`true` = activo) |
| `status.relay2` | boolean | Estado relé 2 (`true` = activo) |
| `status.relay_duration` | float | Duración de activación en segundos |
| `status.mode` | string | "normal" o "torno" |
| `status.local_access_blocked` | boolean | Acceso local bloqueado |
| `status.keyboard_reading` | boolean | Lectura de teclados activa |
| `status.mqtt_connected` | boolean | Conexión MQTT activa |

### Campos de Códigos (`codes`)

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `codes.local_count` | int | Número de códigos locales |
| `codes.local_max` | int | Capacidad máxima (50) |
| `codes.remote_count` | int | Número de códigos remotos |
| `codes.validation_mode` | string | "local_first" o "remote_first" |

### Tamaño Típico
- ~450-550 bytes

### Ejemplo de Parseo - Android (Kotlin)

```kotlin
data class DeviceFullInfo(
    val deviceType: String,
    val serial: String,
    val name: String,
    val firmware: String,
    val network: NetworkInfo,
    val status: DeviceStatus,
    val codes: CodesInfo,
    val uptimeSeconds: Long
)

data class NetworkInfo(
    val ip: String,
    val gateway: String,
    val subnet: String,
    val dns: String,
    val mac: String,
    val dhcp: Boolean,
    val connected: Boolean
)

data class DeviceStatus(
    val relay1: Boolean,
    val relay2: Boolean,
    val relayDuration: Float,
    val mode: String,
    val localAccessBlocked: Boolean,
    val keyboardReading: Boolean,
    val mqttConnected: Boolean
)

data class CodesInfo(
    val localCount: Int,
    val localMax: Int,
    val remoteCount: Int,
    val validationMode: String
)

fun parseDeviceFullInfo(jsonString: String): DeviceFullInfo {
    val json = JSONObject(jsonString)
    val networkJson = json.getJSONObject("network")
    val statusJson = json.getJSONObject("status")
    val codesJson = json.getJSONObject("codes")
    
    return DeviceFullInfo(
        deviceType = json.getString("device_type"),
        serial = json.getString("serial"),
        name = json.getString("name"),
        firmware = json.getString("firmware"),
        network = NetworkInfo(
            ip = networkJson.getString("ip"),
            gateway = networkJson.getString("gateway"),
            subnet = networkJson.getString("subnet"),
            dns = networkJson.getString("dns"),
            mac = networkJson.getString("mac"),
            dhcp = networkJson.getBoolean("dhcp"),
            connected = networkJson.getBoolean("connected")
        ),
        status = DeviceStatus(
            relay1 = statusJson.getBoolean("relay1"),
            relay2 = statusJson.getBoolean("relay2"),
            relayDuration = statusJson.getDouble("relay_duration").toFloat(),
            mode = statusJson.getString("mode"),
            localAccessBlocked = statusJson.getBoolean("local_access_blocked"),
            keyboardReading = statusJson.getBoolean("keyboard_reading"),
            mqttConnected = statusJson.getBoolean("mqtt_connected")
        ),
        codes = CodesInfo(
            localCount = codesJson.getInt("local_count"),
            localMax = codesJson.getInt("local_max"),
            remoteCount = codesJson.getInt("remote_count"),
            validationMode = codesJson.getString("validation_mode")
        ),
        uptimeSeconds = json.getLong("uptime_seconds")
    )
}
```

### Ejemplo de Parseo - iOS (Swift)

```swift
struct DeviceFullInfo: Codable {
    let deviceType: String
    let serial: String
    let name: String
    let firmware: String
    let network: NetworkInfo
    let status: DeviceStatus
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

struct DeviceStatus: Codable {
    let relay1: Bool
    let relay2: Bool
    let relayDuration: Float
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

func parseDeviceFullInfo(data: Data) -> DeviceFullInfo? {
    let decoder = JSONDecoder()
    return try? decoder.decode(DeviceFullInfo.self, from: data)
}
```

---

## 3. Lista de Códigos Locales (FF0A)

### UUID Completo
```
0000FF0A-0000-1000-8000-00805F9B34FB
```

### Operación
- **Leer (Read):** Obtiene la primera página de códigos (15 códigos máximo)
- **Escribir (Write):** Solicita una página específica de códigos

> **NOTA:** Se usa ArduinoJson para garantizar JSON válido. La paginación usa **15 códigos por página** para respetar el MTU BLE (~512 bytes).

### Formato de Respuesta (JSON) - Lectura Inicial (Read)

```json
{
  "count": 25,
  "max": 100,
  "validation_mode": "local_first",
  "page": 0,
  "page_size": 15,
  "total_pages": 2,
  "has_more": true,
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
  ]
}
```

### Campos Principales

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `count` | int | **Total** de códigos almacenados |
| `max` | int | Capacidad máxima (100) |
| `validation_mode` | string | "local_first" o "remote_first" |
| `page` | int | Número de página actual (0-indexed) |
| `page_size` | int | Códigos por página (15) |
| `total_pages` | int | Número total de páginas |
| `has_more` | boolean | `true` si hay más páginas disponibles |
| `codes` | array | Array de códigos de esta página |

### Campos de Cada Código (`codes[]`)

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `id` | int | Índice del código (0-99) |
| `type` | string | **"PIN"** (código numérico) o **"TAG"** (tarjeta RFID) |
| `value` | string | Valor del código (ej: "123456" o "A1B2C3D4") |
| `keyboard` | int | Teclado asignado: **0**=ambos, **1**=teclado 1, **2**=teclado 2 |
| `relay` | int | Relé a activar: **1** o **2** |

### Tamaño Típico
- Página vacía: ~80 bytes
- Página con 15 códigos: ~900-1000 bytes

### Solicitar Página Específica (Write)

Para obtener más códigos, escribir en FF0A:

```json
{"page": 1}
```

**Respuesta:**
```json
{
  "count": 25,
  "page": 1,
  "page_size": 15,
  "total_pages": 2,
  "has_more": false,
  "codes": [
    {"id": 15, "type": "TAG", "value": "AABBCCDD", "keyboard": 0, "relay": 2},
    {"id": 16, "type": "PIN", "value": "9999", "keyboard": 2, "relay": 1}
  ]
}
```

### Códigos de Error

| Respuesta | Significado |
|-----------|-------------|
| `{"error":"NOT_AUTHORIZED"}` | Usuario no autenticado |
| `{"error":"EMPTY_REQUEST"}` | Solicitud vacía |
| `{"error":"INVALID_JSON"}` | JSON de solicitud mal formado |
| `{"error":"MISSING_PAGE"}` | Falta el campo `page` |
| `{"error":"INVALID_PAGE"}` | Página fuera de rango |
| `{"error":"JSON_OVERFLOW"}` | Error interno al generar JSON |

### Ejemplo de Parseo - Android (Kotlin)

```kotlin
data class LocalCode(
    val id: Int,
    val type: String,      // "PIN" o "TAG"
    val value: String,
    val keyboard: Int,     // 0=ambos, 1=teclado1, 2=teclado2
    val relay: Int         // 1 o 2
)

data class CodesResponse(
    val count: Int,              // Total de códigos
    val max: Int,                // Capacidad máxima (100)
    val validationMode: String,  // "local_first" o "remote_first"
    val page: Int,               // Página actual (0-indexed)
    val pageSize: Int,           // Códigos por página (15)
    val totalPages: Int,         // Total de páginas
    val hasMore: Boolean,        // Hay más páginas
    val codes: List<LocalCode>
)

fun parseCodesResponse(jsonString: String): CodesResponse {
    val json = JSONObject(jsonString)
    val codesArray = json.getJSONArray("codes")
    
    val codesList = mutableListOf<LocalCode>()
    for (i in 0 until codesArray.length()) {
        val codeJson = codesArray.getJSONObject(i)
        codesList.add(LocalCode(
            id = codeJson.getInt("id"),
            type = codeJson.getString("type"),
            value = codeJson.getString("value"),
            keyboard = codeJson.getInt("keyboard"),
            relay = codeJson.getInt("relay")
        ))
    }
    
    return CodesResponse(
        count = json.getInt("count"),
        max = json.optInt("max", 100),
        validationMode = json.optString("validation_mode", "local_first"),
        page = json.getInt("page"),
        pageSize = json.getInt("page_size"),
        totalPages = json.getInt("total_pages"),
        hasMore = json.getBoolean("has_more"),
        codes = codesList
    )
}

// Función para obtener todos los códigos paginados
suspend fun getAllCodes(characteristic: BluetoothGattCharacteristic): List<LocalCode> {
    val allCodes = mutableListOf<LocalCode>()
    var currentPage = 0
    var hasMore = true
    
    while (hasMore) {
        if (currentPage == 0) {
            // Primera página: leer característica
            val response = readCharacteristic(characteristic)
            val parsed = parseCodesResponse(response)
            allCodes.addAll(parsed.codes)
            hasMore = parsed.hasMore
        } else {
            // Siguientes páginas: escribir solicitud
            writeCharacteristic(characteristic, """{"page": $currentPage}""")
            val response = readCharacteristic(characteristic)
            val parsed = parseCodesResponse(response)
            allCodes.addAll(parsed.codes)
            hasMore = parsed.hasMore
        }
        currentPage++
    }
    
    return allCodes
}

// Función para obtener el nombre legible del teclado
fun getKeyboardName(keyboard: Int): String {
    return when (keyboard) {
        0 -> "Ambos teclados"
        1 -> "Teclado 1"
        2 -> "Teclado 2"
        else -> "Desconocido"
    }
}
```

### Ejemplo de Parseo - iOS (Swift)

```swift
struct LocalCode: Codable {
    let id: Int
    let type: String       // "PIN" o "TAG"
    let value: String
    let keyboard: Int      // 0=ambos, 1=teclado1, 2=teclado2
    let relay: Int         // 1 o 2
    
    var keyboardDescription: String {
        switch keyboard {
        case 0: return "Ambos teclados"
        case 1: return "Teclado 1"
        case 2: return "Teclado 2"
        default: return "Desconocido"
        }
    }
}

struct CodesResponse: Codable {
    let count: Int              // Total de códigos
    let max: Int?               // Capacidad máxima (100) - solo en página 0
    let validationMode: String? // "local_first" o "remote_first" - solo en página 0
    let page: Int               // Página actual (0-indexed)
    let pageSize: Int           // Códigos por página (15)
    let totalPages: Int         // Total de páginas
    let hasMore: Bool           // Hay más páginas
    let codes: [LocalCode]
    
    enum CodingKeys: String, CodingKey {
        case count, max, codes, page
        case validationMode = "validation_mode"
        case pageSize = "page_size"
        case totalPages = "total_pages"
        case hasMore = "has_more"
    }
}

func parseCodesResponse(data: Data) -> CodesResponse? {
    let decoder = JSONDecoder()
    return try? decoder.decode(CodesResponse.self, from: data)
}
```

---

## Flujo Recomendado para la APP

```
┌─────────────────────────────────────────────────────────────────┐
│              FLUJO DE LECTURA DE CONFIGURACIÓN                  │
└─────────────────────────────────────────────────────────────────┘

   ┌─────────────┐                    ┌─────────────┐
   │  APP Móvil  │                    │   ESP32     │
   └──────┬──────┘                    └──────┬──────┘
          │                                  │
          │  1. Conectar BLE                 │
          │─────────────────────────────────>│
          │                                  │
          │  2. Autenticarse (FF01)          │
          │─────────────────────────────────>│
          │                                  │
          │     OK:AUTHENTICATED             │
          │<─────────────────────────────────│
          │                                  │
          │  3. Leer Info Completa (FF09)    │
          │─────────────────────────────────>│
          │                                  │
          │     JSON con toda la info        │
          │<─────────────────────────────────│
          │                                  │
          │  4. Leer Códigos (FF0A)          │
          │─────────────────────────────────>│
          │                                  │
          │     JSON con lista de códigos    │
          │<─────────────────────────────────│
          │                                  │
          │  5. Si hay más códigos (more=true)│
          │     Escribir {"page": 1} en FF0A │
          │─────────────────────────────────>│
          │                                  │
          │     JSON con siguiente página    │
          │<─────────────────────────────────│
          │                                  │
```

---

## Código Completo de Ejemplo - Android

```kotlin
class BLEConfigReader(private val gatt: BluetoothGatt) {
    
    companion object {
        val SERVICE_UUID = UUID.fromString("0000FF00-0000-1000-8000-00805F9B34FB")
        val CHAR_FULLINFO_UUID = UUID.fromString("0000FF09-0000-1000-8000-00805F9B34FB")
        val CHAR_CODES_UUID = UUID.fromString("0000FF0A-0000-1000-8000-00805F9B34FB")
        val CHAR_NETWORK_UUID = UUID.fromString("0000FF05-0000-1000-8000-00805F9B34FB")
    }
    
    // Leer información completa
    fun readFullInfo() {
        val char = gatt.getService(SERVICE_UUID)?.getCharacteristic(CHAR_FULLINFO_UUID)
        char?.let { gatt.readCharacteristic(it) }
    }
    
    // Leer códigos
    fun readCodes() {
        val char = gatt.getService(SERVICE_UUID)?.getCharacteristic(CHAR_CODES_UUID)
        char?.let { gatt.readCharacteristic(it) }
    }
    
    // Solicitar página de códigos
    fun requestCodesPage(page: Int) {
        val char = gatt.getService(SERVICE_UUID)?.getCharacteristic(CHAR_CODES_UUID)
        char?.let {
            it.value = """{"page": $page}""".toByteArray(Charsets.UTF_8)
            gatt.writeCharacteristic(it)
        }
    }
    
    // Callback para procesar respuestas
    fun onCharacteristicRead(characteristic: BluetoothGattCharacteristic, status: Int) {
        if (status != BluetoothGatt.GATT_SUCCESS) return
        
        val jsonString = String(characteristic.value, Charsets.UTF_8)
        Log.d("BLE", "Recibido: $jsonString")
        
        when (characteristic.uuid) {
            CHAR_FULLINFO_UUID -> {
                val info = parseDeviceFullInfo(jsonString)
                // Actualizar UI con info
            }
            CHAR_CODES_UUID -> {
                val codes = parseCodesResponse(jsonString)
                // Actualizar UI con códigos
            }
            CHAR_NETWORK_UUID -> {
                val network = parseNetworkConfig(jsonString)
                // Actualizar UI con red
            }
        }
    }
}
```

---

## Troubleshooting

### Problema: Recibo datos corruptos o caracteres extraños

**Causa:** No se está decodificando como UTF-8

**Solución:**
```kotlin
// Kotlin
val jsonString = String(characteristic.value, Charsets.UTF_8)

// Swift
let jsonString = String(data: data, encoding: .utf8)
```

### Problema: Campos undefined o null

**Causa:** El JSON no contiene ese campo

**Solución:** Usar métodos `opt*` en lugar de `get*`:
```kotlin
// Kotlin
val more = json.optBoolean("more", false)
val showing = json.optInt("showing", 0)
```

### Problema: No recibo la respuesta de códigos

**Causa:** La característica FF0A envía `notify()` después de `setValue()`

**Solución:** Suscribirse a notificaciones en FF0A:
```kotlin
// Habilitar notificaciones en FF0A
val descriptor = characteristic.getDescriptor(CLIENT_CHARACTERISTIC_CONFIG)
descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
gatt.writeDescriptor(descriptor)
gatt.setCharacteristicNotification(characteristic, true)
```

---

*Documento generado para SWATID-A2 v4.0*
*Última actualización: 5 Febrero 2026*
