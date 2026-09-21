# Contrato MQTT — Entradas digitales (v5.0.2, implementado)

**Topic común:** `swatidhome/<serial>/digital_input`
**Discriminador:** campo `event` — los consumidores deben filtrar por él.

---

## 1. Pulsador — `digital_input_trigger` (SIN CAMBIOS desde v3.x)

```json
{
  "timestamp": "2026-09-21T13:05:00+01:00",
  "event": "digital_input_trigger",
  "input": 1,
  "gpio": 16,
  "relay": 1,
  "duration_ms": 2000,
  "duration": 2.0,
  "message_id": "17"
}
```

Casos: disparo Normal (`duration_ms` configurado), Inverso ON
(`duration_ms: 4294967295`) e Inverso OFF (`duration_ms: 0`).

## 2. Imán / puerta — `door_contact` (NUEVO en v5.0.2)

Se emite en **cada cambio** del estado lógico abierta↔cerrada (debounce
50 ms) y como **sincronización** al arrancar o al recuperar el broker.

```json
{
  "timestamp": "2026-09-21T13:05:00+01:00",
  "event": "door_contact",
  "input": 2,
  "gpio": 17,
  "door_state": "open",
  "raw_level": "HIGH",
  "open_level": "HIGH",
  "reason": "change",
  "message_id": "18"
}
```

| Campo | Valores | Notas |
|-------|---------|-------|
| `event` | `door_contact` | Nunca `digital_input_trigger` |
| `door_state` | `open` \| `closed` | Estado lógico según polaridad configurada |
| `raw_level` | `HIGH` \| `LOW` | Nivel eléctrico real del pin |
| `open_level` | `HIGH` \| `LOW` | Configuración: qué nivel significa abierta |
| `reason` | `boot` \| `change` \| `sync` | boot = primer estado tras arrancar; sync = estado tras reconectar broker (pudo perderse algún cambio); change = transición en vivo |
| **Nunca incluye** | `relay`, `duration_ms`, `duration` | Los parsers de trigger no pueden confundirlo |

### Reglas de compatibilidad

1. Backend que solo procesa `event == "digital_input_trigger"`: **cero cambios** (los door_contact simplemente se ignoran si filtra por event; si NO filtraba por event, debe empezar a hacerlo — el campo existe desde v3.x en todos los mensajes).
2. Un imán **jamás** dispara relé ni genera trigger.
3. `reason=sync` implica que el backend debe tratar el mensaje como estado
   absoluto actual (no como transición).

## 3. APIs web (campos añadidos, ninguno eliminado)

- `GET /api/digital_inputs_config` → añade `diN_type` (`"button"|"door"`),
  `diN_open_level` (0/1).
- `GET /api/digital_inputs_status` → añade `diN_type` y, para tipo door,
  `diN_door_state` (`open|closed|unknown`).
- `POST /save_digital_input` → acepta `diN_type` y `diN_open_level`
  **opcionales**; si se omiten se conserva lo configurado (clientes v5.0.0
  siguen funcionando).

## 4. Persistencia y migración

`DigitalInputConfig` v2 (28 bytes, offset EEPROM 256): los 24 bytes v1 quedan
intactos (`diN_type` ocupa el antiguo `reserved`, que valía 0 = pulsador);
el bloque nuevo (`open_level` + `v2_marker 0xD2`) va detrás y se migra
automáticamente al primer arranque (checksum ampliado y auto-reparable).

## 5. Validación en placa A2v3 (21 Sep 2026)

```text
🔧 [DI] Migrando configuración v1 → v2 (tipos pulsador, open_level=LOW)     ← migración
⚙️ [DI2] Configuración: HABILITADA, IMAN/PUERTA (abierta=HIGH)              ← alta por web
🚪 [DI2] Estado inicial de puerta: ABIERTA (nivel HIGH)
✅ [DI2] door_contact publicado: open (boot)                                 ← sync arranque
⚙️ [DI2] Configuración: HABILITADA, IMAN/PUERTA (abierta=LOW)               ← cambio polaridad
🚪 [DI2] Estado inicial de puerta: CERRADA (nivel HIGH)
✅ [DI2] door_contact publicado: closed (sync)                               ← transición lógica
(reinicio)
💾 Configuración DI cargada: DI1=OFF(PULSADOR), DI2=ON(IMAN/PUERTA)         ← persistencia
✅ [DI2] door_contact publicado: closed (boot)
```

Status API verificada: `"di2_type":"door","di2_door_state":"open|closed"` con
los campos v1 intactos. Ningún relé activado en modo imán.

Pendiente de laboratorio: transición física con imán real (abrir/cerrar
puerta) — la lógica de flancos+debounce ya quedó ejercitada vía cambio de
polaridad.
