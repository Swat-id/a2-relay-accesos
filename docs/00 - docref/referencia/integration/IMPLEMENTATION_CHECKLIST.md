# Checklist de Implementación v4.1 - App y Backend

> **Versión:** 4.1.0  
> **Fecha:** 5 Febrero 2026

---

## Resumen Ejecutivo

La versión 4.1 introduce mejoras críticas de seguridad que requieren cambios tanto en la **App móvil** como en el **Backend**. Este documento resume las mejoras a implementar.

---

## 🔐 Mejoras de Seguridad v4.1

| Área | Problema v4.0 | Solución v4.1 |
|------|---------------|---------------|
| **Auth BLE** | Clave de 64 bytes viaja en claro | Challenge-Response con SHA256 |
| **Sesión BLE** | Sin control de sesión | Token de 8 bytes con timeout |
| **Usuarios MQTT** | Clave enviada en el mensaje | Derivación HKDF (clave nunca viaja) |
| **Eventos** | Información básica | Eventos detallados de seguridad |

---

## 📱 Cambios Requeridos en App Móvil

### Alta Prioridad

#### 1. Autenticación Challenge-Response (FF0B + FF01)

```typescript
// ANTES (v4.0) - INSEGURO
await BLE.write(deviceId, 'FF01', userKey); // 64 bytes en claro

// AHORA (v4.1) - SEGURO
const nonce = await BLE.read(deviceId, 'FF0B');        // 16 bytes
const response = SHA256(userKey + nonce);              // 32 bytes
await BLE.write(deviceId, 'FF01', response);           // Solo hash
```

**Archivo de referencia:** `docs/00 - docref/referencia/integration/BLE_INTEGRATION_GUIDE.md` sección 4

#### 2. Manejo de Token de Sesión

```typescript
// Guardar token tras autenticación
const result = "OK:TOKEN:A1B2C3D4E5F6A7B8";
const token = hexToBytes(result.substring(9, 25)); // 8 bytes

// Incluir token en todas las operaciones
const command = new Uint8Array([...token, relay]);
await BLE.write(deviceId, 'FF02', command);
```

#### 3. Manejo de Expiración de Sesión

```typescript
// Si respuesta es ERROR:SESSION_EXPIRED o ERROR:INVALID_TOKEN
// → Re-autenticar automáticamente
if (response.includes('SESSION_EXPIRED') || response.includes('INVALID_TOKEN')) {
  await authenticateSecure(userKey);
}
```

### Media Prioridad

#### 4. Soporte para Derivación HKDF (Usuarios Invitados)

Cuando el Backend añade un usuario via MQTT (`add_user_derived`), la App recibe la clave derivada del Backend.

```typescript
// App recibe clave del Backend (vía HTTPS)
const userKey = await api.getUserKey(deviceSerial, userId);

// Usa esa clave para autenticarse
await authenticateSecure(hexToBytes(userKey));
```

#### 5. Eventos de Desvinculación

```typescript
// Cuando recibe evento USER_UNBOUND o SUPERADMIN_UNBOUND
// → Limpiar almacenamiento local
await SecureStorage.remove(`key_${deviceId}`);
await SecureStorage.remove(`token_${deviceId}`);
```

### Checklist App

```markdown
## App Móvil - Checklist v4.1

### Autenticación
- [ ] Implementar lectura de FF0B (Challenge)
- [ ] Implementar SHA256(key + nonce)
- [ ] Enviar response de 32 bytes a FF01
- [ ] Parsear token de respuesta
- [ ] Almacenar token en memoria de sesión
- [ ] Mantener compatibilidad con v4.0 (fallback a 64 bytes si falla)

### Operaciones con Token
- [ ] Modificar FF02 (Relay): prepend token
- [ ] Modificar FF03 (Mode): prepend token
- [ ] Modificar FF04 (AddCode): prepend token
- [ ] Modificar FF05 (Network): incluir token en JSON
- [ ] Modificar FF06 (Users): incluir token en JSON

### Sesiones
- [ ] Detectar ERROR:SESSION_EXPIRED
- [ ] Detectar ERROR:INVALID_TOKEN
- [ ] Auto re-autenticación
- [ ] Timeout de inactividad (5 min)

### Almacenamiento
- [ ] Guardar claves en almacenamiento seguro (Keychain/Keystore)
- [ ] Limpiar al desvincular
- [ ] No guardar tokens persistentemente (solo sesión)
```

---

## 🖥️ Cambios Requeridos en Backend

### Alta Prioridad

#### 1. Implementar HKDF para Usuarios

```python
# ANTES (v4.0) - INSEGURO
command = {
    "action": "add_user",
    "key": "A1B2C3D4..."  # Clave en claro ❌
}

# AHORA (v4.1) - SEGURO
user_key = HKDF(master_key, serial, f"user_key:{user_id}")
command = {
    "action": "add_user_derived",
    "user_id": user_id  # Clave NO viaja ✅
}
# Guardar user_key para dar a la App
```

**Archivo de referencia:** `docs/00 - docref/referencia/integration/MQTT_INTEGRATION_GUIDE.md` sección 6

#### 2. Almacenamiento Seguro de Claves Maestras

```python
# Cada dispositivo tiene una device_master_key de 32 bytes
# Almacenar encriptada en base de datos

# Ejemplo con AWS Secrets Manager / Azure Key Vault / HashiCorp Vault
class KeyVault:
    def store_master_key(self, serial: str, key: bytes):
        encrypted = self.encrypt(key)
        self.db.insert(serial, encrypted)
    
    def get_master_key(self, serial: str) -> bytes:
        encrypted = self.db.get(serial)
        return self.decrypt(encrypted)
```

#### 3. API para Entregar Claves a App

```python
# Endpoint seguro (HTTPS) para que la App obtenga su clave
@app.route('/api/v1/devices/<serial>/users/<user_id>/key')
@require_auth  # JWT o similar
def get_user_key(serial, user_id):
    # Verificar que el usuario autenticado es el propietario
    if not user_owns_device(current_user, serial, user_id):
        return {"error": "Unauthorized"}, 403
    
    # Derivar o recuperar clave
    key = backend.get_user_key_for_app(serial, user_id)
    return {"key": key.hex()}
```

### Media Prioridad

#### 4. Procesar Eventos de Usuarios

```python
def on_user_added_derived(event):
    """Actualiza BD cuando se añade usuario."""
    db.users.upsert({
        'device_serial': event['device'],
        'slot': event['slot'],
        'user_id': event['user_id'],
        'name': event['name'],
        'permissions': event['permissions'],
        'created_at': event['timestamp']
    })

def on_user_unbound(event):
    """Actualiza BD cuando se elimina usuario."""
    db.users.delete(
        device_serial=event['device'],
        slot=event['slot']
    )
    # Notificar a la App si está conectada
    push_notification(event['user_id'], "Acceso revocado")
```

#### 5. Configurar TLS para MQTT

```yaml
# Mosquitto config
listener 8883
certfile /etc/mosquitto/certs/server.crt
cafile /etc/mosquitto/certs/ca.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
tls_version tlsv1.2
```

```python
# Cliente Python
client.tls_set(
    ca_certs="ca.crt",
    certfile="client.crt",
    keyfile="client.key"
)
client.connect("mqtt.example.com", 8883)
```

### Checklist Backend

```markdown
## Backend - Checklist v4.1

### Seguridad Crítica
- [ ] Implementar funciones HKDF (extract + expand)
- [ ] Usar add_user_derived en lugar de add_user
- [ ] Almacenar device_master_key de forma segura (encriptada)
- [ ] Configurar TLS para conexiones MQTT
- [ ] Implementar ACL en broker MQTT

### API para App
- [ ] GET /devices/{serial}/users/{user_id}/key - Entregar clave derivada
- [ ] POST /devices/{serial}/users - Crear usuario (internamente usa add_user_derived)
- [ ] DELETE /devices/{serial}/users/{slot} - Eliminar usuario
- [ ] Autenticación JWT o similar en todos los endpoints

### Eventos MQTT
- [ ] Suscribirse a swatidhome/events/{serial}/#
- [ ] Procesar USER_ADDED_DERIVED → Actualizar BD
- [ ] Procesar USER_UNBOUND → Actualizar BD + notificar App
- [ ] Procesar SUPERADMIN_UNBOUND → Limpiar todos los usuarios
- [ ] Procesar ACCESS_GRANTED/DENIED → Log de accesos
- [ ] Procesar AUTH_SUCCESS/FAILED → Alertas de seguridad

### Base de Datos
- [ ] Tabla: devices (serial, master_key_encrypted, created_at)
- [ ] Tabla: users (id, device_serial, slot, user_id, name, permissions)
- [ ] Tabla: access_logs (timestamp, device_serial, event, details)
- [ ] Índices para búsquedas frecuentes

### Monitorización
- [ ] Dashboard de dispositivos online/offline
- [ ] Alertas de intentos de auth fallidos
- [ ] Métricas de uso por dispositivo
- [ ] Backup de claves maestras
```

---

## 🔄 Flujo de Vinculación Completo (v4.1)

### Escenario: Nuevo Usuario Invitado

```
┌────────────────────────────────────────────────────────────────────────────┐
│                    FLUJO DE VINCULACIÓN DE USUARIO                          │
└────────────────────────────────────────────────────────────────────────────┘

1. SUPERADMIN solicita añadir usuario via Web/App
   │
   ▼
2. BACKEND recibe solicitud con:
   - device_serial
   - user_id (único)
   - name
   - permissions
   │
   ▼
3. BACKEND deriva clave:
   user_key = HKDF(device_master_key, serial, "user_key:" + user_id)
   │
   ▼
4. BACKEND guarda en BD:
   - user_id
   - device_serial
   - slot asignado
   - user_key (para entregar a App)
   │
   ▼
5. BACKEND publica MQTT (SIN clave):
   {
     "action": "add_user_derived",
     "slot": 2,
     "user_id": "user_123",
     "name": "Juan"
   }
   │
   ▼
6. DISPOSITIVO recibe mensaje:
   - Deriva la MISMA clave usando user_id
   - Guarda en EEPROM slot 2
   - Publica evento USER_ADDED_DERIVED
   │
   ▼
7. BACKEND recibe evento:
   - Confirma creación exitosa
   - Notifica al nuevo usuario
   │
   ▼
8. NUEVO USUARIO recibe notificación:
   - Descarga App
   - App solicita clave a Backend (HTTPS)
   │
   ▼
9. BACKEND entrega clave derivada:
   GET /devices/{serial}/users/{user_id}/key
   Response: {"key": "a1b2c3d4..."}
   │
   ▼
10. APP guarda clave en Keychain/Keystore
    │
    ▼
11. USUARIO se conecta por BLE:
    - Lee FF0B (challenge)
    - Calcula SHA256(key + nonce)
    - Envía a FF01
    │
    ▼
12. DISPOSITIVO verifica:
    - Encuentra match en slot 2
    - Genera token de sesión
    - Responde "OK:TOKEN:..."
    │
    ▼
13. APP usa token para operaciones
```

---

## 📊 Matriz de Compatibilidad

| Componente | v4.0 | v4.1 | Notas |
|------------|------|------|-------|
| Firmware | ✅ | ✅ | v4.1 soporta ambos métodos |
| App con auth legacy | ✅ | ✅ | Sigue funcionando (64 bytes) |
| App con Challenge-Response | ❌ | ✅ | Nuevo en v4.1 |
| Backend con add_user | ✅ | ✅ | Funciona pero inseguro |
| Backend con add_user_derived | ❌ | ✅ | Recomendado |

### Recomendación de Migración

1. **Fase 1:** Actualizar firmware a v4.1
2. **Fase 2:** Actualizar Backend para usar HKDF y add_user_derived
3. **Fase 3:** Actualizar App para usar Challenge-Response
4. **Fase 4:** Deprecar métodos legacy (opcional)

---

## 📁 Documentación de Referencia

| Documento | Contenido |
|-----------|-----------|
| `docs/00 - docref/referencia/integration/BLE_INTEGRATION_GUIDE.md` | Guía completa BLE para App |
| `docs/00 - docref/referencia/integration/MQTT_INTEGRATION_GUIDE.md` | Guía completa MQTT para Backend |
| `docs/v4.1/BLE_AUTHENTICATION.md` | Detalles de Challenge-Response |
| `docs/v4.1/MQTT_SECURE_USERS.md` | Detalles de HKDF |
| `docs/v4.1/SECURITY_ANALYSIS.md` | Análisis de seguridad |
| `docs/00 - docref/referencia/messaging/code-management.md` | Gestión de códigos PIN/TAG |
| `docs/00 - docref/referencia/messaging/user-management.md` | Gestión de usuarios |

---

**Documento creado:** 5 Febrero 2026  
**Autor:** Equipo SWATID  
**Versión:** 1.0
