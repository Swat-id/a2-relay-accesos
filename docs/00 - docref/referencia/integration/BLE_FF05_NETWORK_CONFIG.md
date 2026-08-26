# BLE FF05: Configuración Ethernet

> **Fecha:** 3 Marzo 2026  
> **Versión Firmware:** v4.1.0-BLE  
> **Característica:** `0000FF05-0000-1000-8000-00805F9B34FB`

---

## Resumen

FF05 permite leer y escribir la configuración de red Ethernet del dispositivo.

| Operación | Requiere Auth | Permiso Requerido |
|-----------|---------------|-------------------|
| READ | ✅ Sí | Cualquier usuario autenticado |
| WRITE | ✅ Sí | `BLE_PERM_NETWORK_CONFIG` (0x02) |

---

## LECTURA (GET Network Config)

### Método

```typescript
// Suscribirse a NOTIFY, luego hacer READ para trigger
await BleClient.startNotifications(deviceId, SERVICE_UUID, CHAR_FF05, onNotify);
await BleClient.read(deviceId, SERVICE_UUID, CHAR_FF05);
// Esperar NOTIFY con datos (termina con EOT 0x04)
```

### Respuesta JSON (con EOT)

**Cuando DHCP está activo:**
```json
{"dhcp":true,"eth":true,"ip":"192.168.5.86","gw":"192.168.5.1","mask":"255.255.255.0","mac":"88:13:BF:B4:F7:DB"}\x04
```

**Cuando usa IP fija:**
```json
{"dhcp":false,"eth":true,"ip":"192.168.5.88","gw":"192.168.5.1","mask":"255.255.255.0","mac":"88:13:BF:B4:F7:DB","cfg_ip":"192.168.5.88","cfg_gw":"192.168.5.1","cfg_mask":"255.255.255.0"}\x04
```

### Campos de Respuesta

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `dhcp` | boolean | `true` = DHCP activo, `false` = IP fija |
| `eth` | boolean | `true` = Ethernet conectado |
| `ip` | string | IP actual asignada |
| `gw` | string | Gateway actual |
| `mask` | string | Máscara de subred actual |
| `mac` | string | Dirección MAC del dispositivo |
| `cfg_ip` | string | (Solo si dhcp=false) IP configurada |
| `cfg_gw` | string | (Solo si dhcp=false) Gateway configurado |
| `cfg_mask` | string | (Solo si dhcp=false) Máscara configurada |

### Detección de Fin de Transmisión (EOT)

Todas las respuestas JSON terminan con byte `0x04` (EOT - End Of Transmission).

```typescript
const EOT = 0x04;

function onNotify(data: DataView) {
  accumulated = concat(accumulated, data);
  
  // Verificar si termina con EOT
  if (accumulated[accumulated.length - 1] === EOT) {
    const jsonBytes = accumulated.slice(0, -1);  // Quitar EOT
    const json = new TextDecoder().decode(jsonBytes);
    const config = JSON.parse(json);
    // Procesar config...
  }
}
```

---

## ESCRITURA (SET Network Config)

### Formato Binario (17 bytes)

```
Byte 0:     DHCP flag (0x00 = IP fija, 0x01 = DHCP)
Bytes 1-4:  IP Address (4 bytes, big-endian)
Bytes 5-8:  Gateway (4 bytes, big-endian)
Bytes 9-12: Subnet Mask (4 bytes, big-endian)
Bytes 13-16: DNS Server (4 bytes, big-endian)
```

### Ejemplo: Configurar IP Fija

**Configuración deseada:**
- DHCP: false
- IP: 192.168.5.88
- Gateway: 192.168.5.1
- Subnet: 255.255.255.0
- DNS: 192.168.5.1

**Bytes a enviar (17 bytes):**
```
00 C0 A8 05 58 C0 A8 05 01 FF FF FF 00 C0 A8 05 01
│  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘
│       │          │          │          │
│       IP         GW         Mask       DNS
│    192.168.5.88  192.168.5.1  255.255.255.0  192.168.5.1
│
DHCP=false
```

### Ejemplo: Configurar DHCP

**Bytes a enviar (17 bytes):**
```
01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
│  └──────────────────────────────────────────────┘
│                  (ignorados cuando DHCP=true)
│
DHCP=true
```

### Código TypeScript para la App

```typescript
interface NetworkConfig {
  dhcp: boolean;
  ip?: string;
  gateway?: string;
  subnet?: string;
  dns?: string;
}

function buildNetworkConfigBytes(config: NetworkConfig): Uint8Array {
  const bytes = new Uint8Array(17);
  
  // Byte 0: DHCP flag
  bytes[0] = config.dhcp ? 0x01 : 0x00;
  
  if (!config.dhcp) {
    // Parsear y copiar IP (bytes 1-4)
    const ipParts = config.ip!.split('.').map(Number);
    bytes[1] = ipParts[0];
    bytes[2] = ipParts[1];
    bytes[3] = ipParts[2];
    bytes[4] = ipParts[3];
    
    // Parsear y copiar Gateway (bytes 5-8)
    const gwParts = config.gateway!.split('.').map(Number);
    bytes[5] = gwParts[0];
    bytes[6] = gwParts[1];
    bytes[7] = gwParts[2];
    bytes[8] = gwParts[3];
    
    // Parsear y copiar Subnet (bytes 9-12)
    const subnetParts = config.subnet!.split('.').map(Number);
    bytes[9] = subnetParts[0];
    bytes[10] = subnetParts[1];
    bytes[11] = subnetParts[2];
    bytes[12] = subnetParts[3];
    
    // Parsear y copiar DNS (bytes 13-16)
    const dnsParts = (config.dns || config.gateway)!.split('.').map(Number);
    bytes[13] = dnsParts[0];
    bytes[14] = dnsParts[1];
    bytes[15] = dnsParts[2];
    bytes[16] = dnsParts[3];
  }
  
  return bytes;
}

// Uso:
const config: NetworkConfig = {
  dhcp: false,
  ip: '192.168.5.88',
  gateway: '192.168.5.1',
  subnet: '255.255.255.0',
  dns: '192.168.5.1'
};

const bytes = buildNetworkConfigBytes(config);
console.log('Hex:', Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join(''));
// Output: 00c0a80558c0a80501ffffff00c0a80501

await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF05, bytes.buffer);
```

---

## Respuestas de Escritura

| Situación | Respuesta |
|-----------|-----------|
| Éxito | `OK:NETWORK_CONFIGURED:RESTARTING` |
| No autenticado | `ERROR:NOT_AUTHENTICATED` |
| Sin permiso | `ERROR:NO_PERMISSION` |
| Datos insuficientes | `ERROR:INVALID_DATA` |

---

## Comportamiento del Firmware

Cuando recibe una configuración válida:

1. **Guarda en EEPROM** - La configuración persiste tras reinicio
2. **Responde OK** - Envía `OK:NETWORK_CONFIGURED:RESTARTING`
3. **Reinicia en 2 segundos** - El dispositivo se reinicia automáticamente para aplicar la nueva configuración

### ⚠️ IMPORTANTE: Reinicio Automático

El dispositivo **se reiniciará automáticamente 2 segundos después** de recibir una nueva configuración de red. La App debe:

1. Esperar la respuesta `OK:NETWORK_CONFIGURED:RESTARTING`
2. Mostrar mensaje al usuario: "Aplicando configuración, el dispositivo se reiniciará..."
3. Esperar 5-10 segundos
4. Reconectar al dispositivo (el BLE se desconectará durante el reinicio)
5. Verificar la nueva configuración leyendo FF05

### Log Esperado en Monitor Serial

```
🔵 [BLE] FF05 - onWrite llamado
🔵 [BLE] FF05 - Auth: 1, Permisos: 0xFF, NETWORK_CONFIG: 0x02
🔵 [BLE] FF05 - Recibidos 17 bytes
🔵 [BLE] FF05 - Byte DHCP: 0x00 -> IP Fija
🔵 [BLE] FF05 - Nueva config: DHCP=false, IP=192.168.5.88
🔵 [BLE] FF05 - GW=192.168.5.1, Mask=255.255.255.0
🔵 [BLE] FF05 - Configuración guardada en EEPROM
🔵 [BLE] Red configurada: DHCP=No por SUPERADMIN
🔵 [BLE] FF05 - Reinicio programado en 2 segundos para aplicar configuración de red
🔄 [LOOP] Ejecutando reinicio programado para aplicar configuración de red...
```

---

## Flujo Completo de la App

```typescript
async function setNetworkConfig(config: NetworkConfig): Promise<boolean> {
  // 1. Verificar autenticación
  if (!isAuthenticated) {
    throw new Error('No autenticado');
  }
  
  // 2. Construir bytes
  const bytes = buildNetworkConfigBytes(config);
  console.log('[BLE-A2] FF05 setNetwork:', {
    dhcp: config.dhcp,
    ip: config.ip,
    bytes: Array.from(bytes),
    hex: Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('')
  });
  
  // 3. Suscribirse a NOTIFY para recibir respuesta
  await BleClient.startNotifications(deviceId, SERVICE_UUID, CHAR_FF05, (data) => {
    const response = new TextDecoder().decode(data);
    console.log('[BLE-A2] FF05 response:', response);
    
    if (response.includes('RESTARTING')) {
      // El dispositivo se reiniciará en 2 segundos
      showToast('Configuración guardada. El dispositivo se reiniciará...');
    }
  });
  
  // 4. Enviar con WRITE (con ACK)
  try {
    await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF05, bytes.buffer);
    console.log('[BLE-A2] FF05 setNetwork - Write ACK successful');
    
    // 5. Esperar respuesta y desconexión por reinicio
    await delay(3000);  // El dispositivo se reinicia en ~2s
    
    // 6. Reconectar automáticamente
    await delay(5000);  // Esperar a que el dispositivo arranque
    await reconnectToDevice();
    
    // 7. Verificar la nueva configuración
    const newConfig = await readNetworkConfig();
    console.log('Nueva IP:', newConfig.ip);
    
    return true;
  } catch (error) {
    console.error('[BLE-A2] FF05 setNetwork error:', error);
    return false;
  } finally {
    try {
      await BleClient.stopNotifications(deviceId, SERVICE_UUID, CHAR_FF05);
    } catch (e) {}
  }
}

// Uso con manejo de reconexión:
async function handleSaveEthernet(config: NetworkConfig) {
  showLoading('Guardando configuración de red...');
  
  const success = await setNetworkConfig(config);
  
  if (success) {
    showToast('Configuración de red aplicada correctamente');
  } else {
    showError('Error al configurar la red');
  }
  
  hideLoading();
}
```

---

## Troubleshooting

### El Write retorna OK pero no cambia la configuración

1. **Verificar permisos**: El usuario debe tener `BLE_PERM_NETWORK_CONFIG`
2. **Verificar autenticación**: Debe estar autenticado como SUPERADMIN o usuario con permisos
3. **Revisar monitor serial**: Buscar logs de `🔵 [BLE] FF05`

### La IP no cambia después del Write

1. **Esperar 1-2 segundos**: La reconexión Ethernet puede tardar
2. **Verificar cable Ethernet**: Debe estar conectado
3. **Leer FF05 de nuevo**: Verificar que `dhcp` ahora es `false`

### Error 201 (GATT_WRITE_NOT_PERMITTED)

1. **Limpiar caché BLE en Android**: Settings → Bluetooth → Olvidar dispositivo
2. **Usar `write` en lugar de `writeWithoutResponse`**
3. **Verificar que el firmware está actualizado** (v4.1.0+)

---

## UUIDs de Referencia

```typescript
const SERVICE_UUID = '0000FF00-0000-1000-8000-00805F9B34FB';
const CHAR_FF05_NETWORK = '0000FF05-0000-1000-8000-00805F9B34FB';
```

---

**Estado:** ✅ DOCUMENTACIÓN ACTUALIZADA - FIRMWARE v4.1.0
