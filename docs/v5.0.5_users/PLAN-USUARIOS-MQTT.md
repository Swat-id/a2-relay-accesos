# Plan — Gestión uniforme de usuarios A2 (alineación X1)

**Rama:** `v5.0.5_users` · **Fecha:** 01 Oct 2026  
**Referencia:** Relay X1 v1.2.0-rc1 — canal MQTT `/vinculo` + HMAC (doc integración, placa `SWATID_A085E3CCAC98`)  
**Baseline A2:** BLE v4.1 (challenge-response FF0B/FF01, HKDF), MQTT `message_type: 6`, EEPROM `BLEAuthConfig`

---

## 1. Objetivo de producto

Uniformar la **gestión de usuarios** de la controladora de accesos A2/A2v3 con el contrato ya validado en otras placas (X1), de modo que:

- Backend / App usen los **mismos topics y firmas** (`command/<serial>/vinculo` ↔ `<serial>/vinculo`).
- Quede **explícita** la diferencia entre:
  1. **Superadmin de plataforma** — acceso irrestricto de servicio.
  2. **Admin de placa** — dueño operativo del equipo (TOFU).
  3. **Usuarios autorizados** — slots con permisos.

---

## 2. Roles — definición operativa

### 2.1 Superadmin (plataforma)

| Atributo | Valor |
|----------|--------|
| Propósito | Soporte, fábrica, recuperación; **nunca** queda bloqueado por operaciones del admin de placa |
| Persistencia | Clave dedicada (p. ej. slot lógico `-1` / flag `platform_superadmin` **o** clave de fábrica en NVS separada de `user_keys`) |
| Alta | Provisioning de fábrica / procedimiento SWATID (no TOFU de instalador) |
| Baja | Solo procedimiento de fábrica / comando firmado por sí mismo; **no** la borra `unlink` de admin ni `clear_all` de usuarios |
| MQTT | Puede firmar cualquier comando autenticado del canal `/vinculo`; puede `devinfo` siempre |
| BLE | Si existe clave en dispositivo, autentica como hoy el “superadmin” de fábrica **o** slot especial |

**Regla de producto (usuario):** *el superadmin podrá acceder siempre.*

### 2.2 Admin de placa (slot 0 / role `admin`)

| Atributo | Valor (alineado X1 §5) |
|----------|-------------------------|
| Equivalente X1 | “Administrador” / `role: superadmin` en respuestas X1 *(renombrar en A2 a `admin` en JSON nuevo; alias `superadmin` en compat)* |
| Alta | TOFU: `link` + `key` solo si no hay admin **y** ventana remota abierta (o BLE presencial) |
| Permisos | Implícitos plenos (0xFF) |
| Puede | Alta usuarios 1–5, baja terceros, baja propia (limpia todos los usuarios), rotar su clave |
| No puede | Borrar la clave de **superadmin plataforma** |

### 2.3 Usuarios autorizados (slots 1–5)

| Atributo | Valor |
|----------|--------|
| Alta | Solo admin de placa (o superadmin plataforma), sobre firmado |
| Campos | `new_key` 128 hex, `name` ≤16, `permissions` bitmask |
| Permisos | `0x01` relé · `0x02` modo · `0x04` códigos · `0x08` red · (admin placa = 0xFF) |
| Baja | Propia (`unlink` sin slot) o por admin (`unlink` + `slot`) |
| Rotación | `update_vinculo` con su propia firma |

### 2.4 Mapa mental (evitar confusión con código actual A2)

```text
Hoy en A2 (BLEAuthConfig):
  superadmin_key     →  en la práctica = “dueño BLE” (mezcla fábrica + admin)
  user_keys[0..4]    →  usuarios 1..5

Objetivo v5.0.5:
  platform_superadmin_key  →  Superadmin plataforma (siempre acceso)
  admin_key (slot 0)       →  Admin de placa (TOFU instalador)
  user_keys[1..5]          →  Usuarios autorizados
```

Migración: si `superadmin_registered` y no hay “admin de placa” separado → tratar la clave actual como **admin de placa** y dejar plataforma vacía hasta provisioning (o mapear clave de fábrica conocida a plataforma). Documentar decisión en implementación.

---

## 3. Estado actual A2 vs X1

| Capacidad | A2 hoy | X1 v1.2 | Gap |
|-----------|--------|---------|-----|
| Topics `/vinculo` | No | Sí | **P0** implementar |
| Challenge + HMAC MQTT | No (solo BLE SHA256 key‖nonce) | Sí HMAC(nonce‖payload) | **P0** |
| message_type 6 add/list/clear | Sí (auth = broker) | Sí legado | Mantener; no es el canal nuevo |
| Ventana remota 10 min / web | No documentada igual | Sí | **P0** |
| TOFU primer admin | `set_superadmin` / BLE | `link` POST | Unificar en `/vinculo` |
| Eventos `BINDING_*` | Parcial vía BLE/MQTT 6 | Completos | Alinear nombres |
| Cola offline eventos | Limitada | queued/age_ms | Fuera de MVP usuarios (nota) |
| 1 admin + 5 users | Sí (1+5) | Sí (slot 0+5) | Alinear numeración slot 0 |
| Superadmin plataforma distinto | **No** | Implícito fábrica en otros productos | **P0** modelo A2 |

---

## 4. Contrato MQTT objetivo (canal vinculación)

Misma geometría que X1 (detalle en [REF-X1-VINCULO.md](REF-X1-VINCULO.md)):

| Dirección | Topic |
|-----------|--------|
| Backend → equipo | `swatidhome/command/<serial>/vinculo` |
| Equipo → backend | `swatidhome/<serial>/vinculo` |
| Eventos | `swatidhome/events/<serial>/ble` (o `/binding` si se prefiere; X1 usa `/ble`) |

### 4.1 Públicos

- `{"command":"devinfo","type":"GET"}` → capabilities A2: `relays:2`, `keyboards:2`, `slots:6` (admin+5) o `7` si se expone plataforma como slot extra en contadores — **decidir:** X1 cuenta 6 = admin+5; plataforma no entra en `slots` de capabilities o se informa aparte (`platform_superadmin: true/false`).
- `{"command":"challenge","type":"GET"}` → nonce 64 hex, 60 s, un solo vivo.

### 4.2 TOFU admin de placa

- `{"command":"link","type":"POST","key":"<128 hex>"}`  
- Condiciones: sin admin de placa + ventana abierta.  
- **No** usa la clave de superadmin plataforma.

### 4.3 Autenticados (sobre)

```json
{"nonce":"<64 hex>","hmac":"<64 hex>","payload":"<string exacto del comando>"}
```

`hmac = HMAC-SHA256(clave_firmante, bytes(nonce) ‖ bytes(payload))`  
El equipo prueba: **plataforma → admin → users 1..5**; el primero que verifique es el firmante.

Comandos: alta usuario, `update_vinculo`, `unlink` (propia / tercero / admin→limpia users).

### 4.4 message_type 6 (legado)

Igual que X1 §4.7: sigue; única auth = broker. Documentar “solo compat; gestión nueva por `/vinculo`”.

---

## 5. Ajustes de firmware previstos

| ID | Cambio | Pri |
|----|--------|-----|
| U1 | Módulo `binding_service` (o equivalente): parse topic `.../vinculo`, devinfo/challenge/link/envelope | P0 |
| U2 | Separar persistencia **platform_superadmin** vs **admin slot 0** vs users; migración desde `BLEAuthConfig` | P0 |
| U3 | Ventana remota (boot 10 min + botón/web “abrir vinculación”) | P0 |
| U4 | Eventos `BINDING_ADDED/UPDATED/CLEARED` alineados X1 | P1 |
| U5 | BLE: autenticar con las mismas claves (admin + users + plataforma) | P0 |
| U6 | `devinfo` capabilities A2 (2 relés, DI, GSM flags, slots) | P1 |
| U7 | Actualizar manual MQTT A2 + ejemplos mosquitto/Python | P1 |
| U8 | Deprecar en docs el uso de tipo 6 para altas nuevas | P2 |
| U9 | (Opcional) cola queued/age_ms estilo X1 | P3 |

### Ficheros candidatos

- Nuevos: `src/binding_service.h/.cpp` (espejo conceptual X1 BindingService)
- `src/eeprom_layout.h` / NVS namespace `a2acc_bind`
- `src/main.ino` — ruta MQTT `vinculo`, web “abrir ventana”
- BLE paths existentes — reutilizar keys
- `docs/v5.0.5_users/` + actualización manual integración

---

## 6. Semántica “superadmin siempre accede”

Checklist de implementación:

- [ ] `clear_all` / `unlink slot 0` / wipe de usuarios **no** borran `platform_superadmin`.
- [ ] Firma MQTT: si la clave plataforma verifica, se acepta aunque no haya admin de placa.
- [ ] BLE: autenticación con clave plataforma válida tras wipe de admin.
- [ ] `devinfo` expone `platform_superadmin_registered` (bool) **sin** filtrar la clave.
- [ ] Admin de placa no puede `unlink` la plataforma.
- [ ] Documentar recuperación: solo SWATID / procedimiento fábrica rota o limpia plataforma.

---

## 7. Fases

| Fase | Trabajo | Criterio |
|------|---------|----------|
| **0** | Este plan + REF X1 | Docs OK |
| **1** | Persistencia 3 roles + migración | Compila; datos viejos → admin placa |
| **2** | Canal `/vinculo` público + TOFU + HMAC | Prueba mosquitto como X1 §8 |
| **3** | BLE usa mismas claves; regresión app v4.1 | Auth FF0B/FF01 OK |
| **4** | Eventos + manual + deprecación tipo 6 en docs | Integrador único |
| **5** | (Opc) cola offline / LWT keepalive al estilo X1 | Uniformidad sesión |

---

## 8. Plan de pruebas

| # | Caso | Esperado |
|---|------|----------|
| P1 | `devinfo` sin vínculos | `available_for_linking`, ventana |
| P2 | TOFU admin en ventana | `linked` slot admin; evento BINDING_ADDED |
| P3 | TOFU fuera de ventana | `remote_linking_window_closed` |
| P4 | Challenge + alta usuario firmada por admin | slot N linked |
| P5 | Alta firmada por usuario normal | `superadmin_required` / admin_required |
| P6 | Unlink admin | users cleared; **plataforma intacta** |
| P7 | Firma con clave plataforma tras P6 | OK (siempre acceso) |
| P8 | Tipo 6 `list_users` | Sigue respondiendo (legado) |
| P9 | BLE auth admin y user | Token OK |
| P10 | Capacidades A2 en devinfo | relays=2, etc. |

---

## 9. Riesgos

| Riesgo | Mitigación |
|--------|------------|
| Nombre `superadmin` en JSON X1 vs rol plataforma | En A2: `role: "admin"` para slot 0; `role: "platform_superadmin"`; aceptar alias en lectura |
| EEPROM BLE lleno / tamaño struct | Valorar NVS `a2acc_bind` para plataforma + admin |
| App móvil solo habla BLE tipo 6 | Mantener tipo 6; App nueva → vinculo o BLE actualizado |
| Dos nonces (BLE 16 B vs MQTT 32 B) | Canales independientes; documentar |
| Builds sin `ENABLE_BLE` | Canal `/vinculo` debe funcionar **también sin BLE** (claves en NVS) |

---

## 10. Fuera de alcance (esta rama)

- Replicar todo el contrato X1 (relé pulse/on/off, cola 24 eventos, LWT) — solo **usuarios/vinculación**.
- Cambiar CRUD de códigos (v5.0.4).
- PPP/4G.

---

## 11. Estado

| Ítem | Estado |
|------|--------|
| Rama `v5.0.5_users` | Creada y publicada |
| Plan roles + gaps + REF X1 | **Cerrado** (01 Oct 2026) |
| Implementación U1–U7 | Pendiente (siguiente trabajo en esta rama) |
| Push planificación | Hecho |

---

*Fin de la fase de planificación. No se requiere más análisis documental para arrancar U1.*
