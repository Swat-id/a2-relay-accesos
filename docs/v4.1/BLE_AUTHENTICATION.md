# Autenticación BLE v4.1 - Challenge-Response

## Resumen

La versión 4.1 introduce un sistema de autenticación seguro basado en Challenge-Response que evita transmitir la clave del usuario por el canal BLE.

---

## Flujo de Autenticación Seguro

### Paso 1: Obtener Challenge

```
APP → Dispositivo: READ FF0B
Dispositivo genera: nonce = random(16 bytes)
Dispositivo → APP: nonce (16 bytes)
```

### Paso 2: Calcular Response

```javascript
// En la APP
const key = getUserKey();        // Clave de 64 bytes
const nonce = await ble.read('FF0B');  // 16 bytes del challenge

// Concatenar key + nonce
const data = new Uint8Array([...key, ...nonce]);

// Calcular SHA256
const response = await crypto.subtle.digest('SHA-256', data);
// response = 32 bytes
```

### Paso 3: Enviar Response

```
APP → Dispositivo: WRITE FF01 (32 bytes = SHA256 response)
```

### Paso 4: Verificación y Token

```
Dispositivo:
  for cada clave_almacenada:
    expected = SHA256(clave_almacenada + nonce)
    if response == expected:
      token = random(8 bytes)
      return "OK:TOKEN:" + hex(token)
  
  return "ERROR:INVALID_RESPONSE"
```

### Paso 5: Usar Token en Operaciones

```
APP → Dispositivo: WRITE FF02 (token + comando)
                         ↑      ↑
                    8 bytes  payload
```

---

## Código de Ejemplo (TypeScript)

```typescript
class BLEAuthenticator {
  private token: Uint8Array | null = null;

  async authenticate(deviceId: string, userKey: Uint8Array): Promise<boolean> {
    // 1. Obtener challenge
    const nonce = await BLE.read(deviceId, 'FF0B');
    console.log('Challenge recibido:', bytesToHex(nonce));

    // 2. Calcular response = SHA256(key + nonce)
    const data = new Uint8Array([...userKey, ...nonce]);
    const hashBuffer = await crypto.subtle.digest('SHA-256', data);
    const response = new Uint8Array(hashBuffer);
    console.log('Response calculado:', bytesToHex(response));

    // 3. Enviar response
    await BLE.write(deviceId, 'FF01', response);

    // 4. Leer resultado
    const result = await BLE.readNotify(deviceId, 'FF01');
    const resultStr = new TextDecoder().decode(result);

    if (resultStr.startsWith('OK:TOKEN:')) {
      // 5. Extraer token
      const tokenHex = resultStr.substring(9);
      this.token = hexToBytes(tokenHex);
      console.log('Autenticación exitosa, token:', tokenHex);
      return true;
    }

    console.error('Autenticación fallida:', resultStr);
    return false;
  }

  async activateRelay(deviceId: string, relay: number): Promise<void> {
    if (!this.token) {
      throw new Error('No autenticado');
    }

    // Construir comando con token
    const command = new Uint8Array([...this.token, relay]);
    await BLE.write(deviceId, 'FF02', command);
  }
}

// Funciones auxiliares
function bytesToHex(bytes: Uint8Array): string {
  return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('');
}

function hexToBytes(hex: string): Uint8Array {
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < bytes.length; i++) {
    bytes[i] = parseInt(hex.substr(i * 2, 2), 16);
  }
  return bytes;
}
```

---

## Modo Legado (Compatibilidad v4.0)

Las APPs que envían la clave de 64 bytes directamente a FF01 siguen funcionando:

```
APP → Dispositivo: WRITE FF01 (64 bytes = clave directa)
Dispositivo → APP: "OK:AUTHENTICATED:TOKEN:XXXXXXXX"
```

El dispositivo detecta automáticamente el método según la longitud:
- 32 bytes → Challenge-Response (v4.1)
- 64 bytes → Clave directa (v4.0 legado)

---

## Timeouts

| Timeout | Valor | Descripción |
|---------|-------|-------------|
| Challenge | 30s | Tiempo para responder al challenge |
| Sesión | 5 min | Inactividad máxima antes de invalidar sesión |
| Conexión | 30s | Tiempo para autenticarse tras conectar |

---

## Eventos MQTT

### Autenticación Exitosa

```json
{
  "event": "AUTH_SUCCESS",
  "method": "challenge_response",
  "user": "SUPERADMIN",
  "timestamp": "2026-02-05T10:30:00Z"
}
```

### Autenticación Fallida

```json
{
  "event": "AUTH_FAILED",
  "method": "challenge_response",
  "reason": "invalid_response",
  "timestamp": "2026-02-05T10:30:00Z"
}
```

---

## Seguridad

### Ventajas del Challenge-Response

1. **Clave no expuesta**: La clave nunca viaja por BLE
2. **Replay protection**: Cada nonce es único y temporal
3. **Forward secrecy**: Capturar un response no permite autenticaciones futuras

### Consideraciones

1. **Nonce único**: Cada lectura de FF0B genera un nuevo nonce
2. **Timeout de challenge**: El nonce expira en 30 segundos
3. **Un intento por challenge**: Tras enviar response, se invalida el challenge
