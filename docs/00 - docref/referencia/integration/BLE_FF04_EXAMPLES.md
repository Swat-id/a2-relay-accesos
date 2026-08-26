# FF04 AddCode - Ejemplos de Mensajes BLE

> **Versión:** v4.1.0-BLE  
> **Última actualización:** 5 Febrero 2026  
> **Estado:** ✅ FUNCIONANDO

---

## Formato del Comando FF04

```
Byte 0: type     (0=PIN, 1=TAG)
Byte 1: keyboard (0=ambos, 1=teclado1, 2=teclado2)
Byte 2: relay    (1 o 2)
Byte 3+: code    (ASCII, 1-16 caracteres)
```

---

## Ejemplos de Comandos Válidos

### Ejemplo 1: PIN "123456" para teclado 1, relé 1

```javascript
// Datos a enviar
const command = new Uint8Array([
  0x00,  // type = PIN (0)
  0x01,  // keyboard = teclado 1 (1)
  0x01,  // relay = relé 1 (1)
  0x31, 0x32, 0x33, 0x34, 0x35, 0x36  // "123456" en ASCII
]);

// Bytes hexadecimales: 00 01 01 31 32 33 34 35 36
// Total: 9 bytes
```

**Respuesta esperada:** `OK:CODE_ADDED:N` (donde N es el nuevo total de códigos)

---

### Ejemplo 2: PIN "9999" para ambos teclados, relé 2

```javascript
const command = new Uint8Array([
  0x00,  // type = PIN (0)
  0x00,  // keyboard = ambos (0)
  0x02,  // relay = relé 2 (2)
  0x39, 0x39, 0x39, 0x39  // "9999" en ASCII
]);

// Bytes hexadecimales: 00 00 02 39 39 39 39
// Total: 7 bytes
```

---

### Ejemplo 3: TAG "ABC123" para teclado 2, relé 1

```javascript
const command = new Uint8Array([
  0x01,  // type = TAG (1)
  0x02,  // keyboard = teclado 2 (2)
  0x01,  // relay = relé 1 (1)
  0x41, 0x42, 0x43, 0x31, 0x32, 0x33  // "ABC123" en ASCII
]);

// Bytes hexadecimales: 01 02 01 41 42 43 31 32 33
// Total: 9 bytes
```

---

### Ejemplo 4: PIN "262524" (del log de pruebas)

```javascript
const command = new Uint8Array([
  0x00,  // type = PIN (0)
  0x01,  // keyboard = teclado 1 (1)
  0x01,  // relay = relé 1 (1)
  0x32, 0x36, 0x32, 0x35, 0x32, 0x34  // "262524" en ASCII
]);

// Bytes hexadecimales: 00 01 01 32 36 32 35 32 34
// Total: 9 bytes
```

---

## Función Helper para la App (TypeScript)

```typescript
/**
 * Construye el comando BLE para añadir un código
 */
function buildAddCodeCommand(
  type: 'pin' | 'tag',
  code: string,
  keyboard: 0 | 1 | 2 = 1,
  relay: 1 | 2 = 1
): Uint8Array {
  const typeByte = type === 'pin' ? 0x00 : 0x01;
  const codeBytes = new TextEncoder().encode(code);
  
  const command = new Uint8Array(3 + codeBytes.length);
  command[0] = typeByte;
  command[1] = keyboard;
  command[2] = relay;
  command.set(codeBytes, 3);
  
  return command;
}

// Uso:
const cmd = buildAddCodeCommand('pin', '123456', 1, 1);
// Resultado: Uint8Array [0, 1, 1, 49, 50, 51, 52, 53, 54]
```

---

## Tabla de Conversión ASCII

| Carácter | Hex | Decimal |
|----------|-----|---------|
| '0' | 0x30 | 48 |
| '1' | 0x31 | 49 |
| '2' | 0x32 | 50 |
| '3' | 0x33 | 51 |
| '4' | 0x34 | 52 |
| '5' | 0x35 | 53 |
| '6' | 0x36 | 54 |
| '7' | 0x37 | 55 |
| '8' | 0x38 | 56 |
| '9' | 0x39 | 57 |
| 'A' | 0x41 | 65 |
| 'B' | 0x42 | 66 |
| 'C' | 0x43 | 67 |

---

## Respuestas del Firmware

### Respuestas de Éxito

| Respuesta | Significado |
|-----------|-------------|
| `OK:CODE_ADDED:1` | Código añadido, ahora hay 1 código total |
| `OK:CODE_ADDED:2` | Código añadido, ahora hay 2 códigos total |
| `OK:CODE_ADDED:N` | Código añadido, ahora hay N códigos total |

### Respuestas de Error

| Respuesta | Causa |
|-----------|-------|
| `ERROR:NOT_AUTHENTICATED` | No se ha autenticado primero en FF01 |
| `ERROR:NO_PERMISSION` | Usuario no tiene permiso ADD_CODES |
| `ERROR:STORAGE_NOT_READY` | Error interno del firmware |
| `ERROR:INVALID_LENGTH` | Comando tiene menos de 4 bytes |
| `ERROR:INVALID_TYPE` | Byte 0 no es 0 ni 1 |
| `ERROR:INVALID_KEYBOARD` | Byte 1 no es 0, 1 ni 2 |
| `ERROR:INVALID_RELAY` | Byte 2 no es 1 ni 2 |
| `ERROR:INVALID_CODE_LENGTH` | Código vacío o mayor a 16 caracteres |
| `ERROR:CODE_EXISTS_OR_FULL` | Código duplicado o memoria llena |

---

## Flujo Completo de la App

```typescript
async function addPinCode(deviceId: string, code: string): Promise<boolean> {
  const SERVICE_UUID = '0000FF00-0000-1000-8000-00805F9B34FB';
  const CHAR_FF04 = '0000FF04-0000-1000-8000-00805F9B34FB';
  
  // 1. Construir comando
  const command = buildAddCodeCommand('pin', code, 1, 1);
  console.log('[BLE] FF04 command bytes:', Array.from(command).map(b => b.toString(16).padStart(2, '0')).join(' '));
  
  // 2. Suscribirse a NOTIFY antes de escribir
  await BleClient.startNotifications(deviceId, SERVICE_UUID, CHAR_FF04, (value) => {
    const response = new TextDecoder().decode(value);
    console.log('[BLE] FF04 NOTIFY response:', response);
    
    if (response.startsWith('OK:CODE_ADDED:')) {
      console.log('[BLE] ✅ Código añadido correctamente');
    } else if (response.startsWith('ERROR:')) {
      console.log('[BLE] ❌ Error:', response);
    }
  });
  
  // 3. Enviar comando
  try {
    await BleClient.write(deviceId, SERVICE_UUID, CHAR_FF04, command);
    console.log('[BLE] FF04 write completed');
  } catch (error) {
    // El write timeout NO es necesariamente un error
    // La respuesta real viene por NOTIFY
    console.log('[BLE] FF04 write timeout (normal, check NOTIFY for response)');
  }
  
  // 4. Esperar un momento para el NOTIFY
  await new Promise(resolve => setTimeout(resolve, 500));
  
  // 5. Desuscribirse
  await BleClient.stopNotifications(deviceId, SERVICE_UUID, CHAR_FF04);
  
  return true;
}
```

---

## Validación de la Persistencia

Para verificar que el código se guardó correctamente:

1. **Añadir código por BLE**
2. **Reiniciar el ESP32** (desconectar/conectar alimentación)
3. **Leer FF0A** (lista de códigos) y verificar que el código aparece

```typescript
// Leer FF0A para ver los códigos guardados
const codesData = await BleClient.read(deviceId, SERVICE_UUID, '0000FF0A-...');
const codesJson = new TextDecoder().decode(codesData);
console.log('Códigos guardados:', JSON.parse(codesJson));
```

---

## Notas Importantes

1. **El "Write timeout" de la App es un falso positivo** - La respuesta real viene por NOTIFY
2. **El firmware ahora usa `addCode()`** - La misma función que usa la web, garantizando persistencia
3. **Reiniciar para verificar** - Los códigos deben persistir después de un reinicio del ESP32
