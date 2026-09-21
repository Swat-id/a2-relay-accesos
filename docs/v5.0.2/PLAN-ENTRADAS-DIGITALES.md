# Plan — Gestión ampliada de entradas digitales (v5.0.2)

**Rama:** `v5.0.2` · **Fecha:** 21 Sep 2026  
**Baseline código:** `src/digital_inputs.*`, `DigitalInputConfig` en `eeprom_layout.h`, UI en `main.ino`  
**Histórico relacionado:** [docs/v3.0/mejora-modo-normal-inverso.md](../v3.0/mejora-modo-normal-inverso.md)

---

## 1. Baseline actual (qué hay hoy)

### 1.1 Persistencia (`DigitalInputConfig`, offset EEPROM 256, ~24 bytes packed)

Por cada DI (`di1_*` / `di2_*`):

| Campo | Uso actual |
|-------|------------|
| `enabled` | 0/1 |
| `relay` | 1 o 2 |
| `inverse` | 0 = Normal (flanco HIGH → pulso relé), 1 = Inverso (relé mantenido en LOW) |
| `reserved` | **siempre 0** (hueco libre) |
| `duration_ms` | Duración del pulso en modo Normal |

Marcador `0xD1D1D1D1` + `checksum`. Hueco hasta offset 512: **amplio** para crecer la struct con cuidado.

### 1.2 Comportamiento runtime

- Solo modo **actuador** (abre relé).
- MQTT al disparar / cambio en inverso:

```json
{
  "timestamp": "...",
  "event": "digital_input_trigger",
  "input": 1,
  "gpio": 36,
  "relay": 1,
  "duration_ms": 2000,
  "duration": 2.0,
  "message_id": "..."
}
```

Topic: `swatidhome/<serial>/digital_input`

### 1.3 Gap vs producto pedido

No existe tipo “sensor de puerta”: no hay estado lógico open/closed, no hay mensaje dedicado por cambio de puerta, y `inverse` hoy significa polaridad de **relé**, no de contacto magnético.

---

## 2. Modelo de tipos propuesto

```text
diN_type:
  0 = button   (pulsador)     ← default, compatible
  1 = door     (imán / contacto de puerta)
```

### 2.1 Tipo `button` (sin cambio semántico)

- Usa `enabled`, `relay`, `inverse`, `duration_ms` exactamente como ahora.
- Publica solo `digital_input_trigger` (mismo schema).
- UI: mismas opciones Normal/Inverso + relé + duración.

### 2.2 Tipo `door` (nuevo)

| Parámetro | Significado |
|-----------|-------------|
| `enabled` | Supervisión activa |
| `open_level` | Nivel eléctrico que significa **puerta abierta**: `0` = LOW, `1` = HIGH |
| `relay` / `duration_ms` / `inverse` | **Ignorados** en runtime (pueden quedar en EEPROM por compat UI; no deben disparar relé) |

Estado lógico:

```text
raw = digitalRead(pin)          // HIGH=1, LOW=0
door_open = (raw == open_level)
door_state = door_open ? "open" : "closed"
```

- Debounce recomendado: 30–50 ms (o reutilizar filtro simple por flanco estable) para imanes ruidosos.
- En **cada** cambio de `door_open` (tras debounce) → publicar mensaje de puerta.
- Al habilitar o al arranque: opcional **un** mensaje de estado inicial (recomendado: sí, con `"reason": "boot"` o `"sync"`) para que el backend no quede a ciegas; documentar como parte del contrato nuevo (no afecta a listeners de trigger).

---

## 3. Persistencia — compatibilidad EEPROM

### Opción A (preferida): reutilizar `diN_reserved` + un byte de polaridad en hueco

Hoy por DI hay 1 byte `reserved`. Propuesta packed:

```text
diN_enabled      u8
diN_relay        u8
diN_inverse      u8     // solo button
diN_type         u8     // ex-reserved: 0=button, 1=door  ★
diN_duration_ms  u32
diN_open_level   u8     // solo door: 0=LOW→open, 1=HIGH→open  ★ NUEVO
diN_pad[3]       u8     // alineación / futuro
```

- Struct crece ~4 bytes por DI → total ~32 bytes; sigue ≪ 256.
- Carga: si `validMarker` OK y `diN_type` no es 0/1 → forzar `0` (button).
- Equipos viejos: `reserved=0` → ya es `type=button`; bytes nuevos tras el layout antiguo…

**Problema:** si solo cambiamos el nombre de `reserved` → `type` **sin** crecer, equipos viejos OK. Si **añadimos** campos al final, `EEPROM.get` de struct más grande lee basura en los bytes nuevos → inicializar `open_level=0` si checksum falla **o** versionar.

### Opción B (más segura): versionado ligero

- Mantener marker; al cargar, si `checksum` no coincide con el nuevo algoritmo → migrar:
  - interpretar solo los 24 bytes antiguos;
  - `type=0`, `open_level=0`;
  - recalcular checksum y guardar.
- Incluir `type` y `open_level` en el checksum nuevo.

**Decisión de implementación:** Opción A + migración por checksum (B). No cambiar el marker para no invalidar configs válidas de button.

### Checksum

Ampliar `calculateDIChecksum` con `type` y `open_level`. Si el checksum antiguo no coincide pero el marker es válido y los campos “v1” son coherentes → migrar (no borrar enabled/relay).

---

## 4. Mensajería MQTT — contratos

### 4.1 Pulsador — **sin cambios** (compat)

- Topic: `swatidhome/<serial>/digital_input`
- `event`: `"digital_input_trigger"`
- Campos: los actuales (`input`, `gpio`, `relay`, `duration_ms`, `duration`, `message_id`, `timestamp`)

### 4.2 Imán / puerta — **contrato nuevo**

Mismo topic base (agrupa DI) **o** topic hermano; recomendación: **mismo topic** `.../digital_input` pero `event` distinto para que filtros actuales no rompan:

```json
{
  "timestamp": "2026-09-21T12:00:00Z",
  "event": "door_contact",
  "input": 2,
  "gpio": 39,
  "door_state": "open",
  "raw_level": "HIGH",
  "open_level": "HIGH",
  "message_id": "42"
}
```

| Campo | Notas |
|-------|--------|
| `event` | Siempre `"door_contact"` (nunca `digital_input_trigger`) |
| `door_state` | `"open"` \| `"closed"` |
| `raw_level` | `"HIGH"` \| `"LOW"` (eléctrico) |
| `open_level` | Configuración: qué nivel = abierto |
| **No** incluir | `relay`, `duration_ms` (evita que parsers de trigger confundan) |

Opcional futuro (no MVP): topic dedicado `.../door_contact` — se puede añadir como duplicado sin quitar el event en `digital_input`.

### 4.3 Qué no hacer

- No reutilizar `digital_input_trigger` con un flag `mode=door` como único discriminador si el backend actual no ignora campos desconocidos de forma segura: un `event` distinto es más claro y compatible.
- No enviar trigger al cambiar un imán.

---

## 5. API web y configuración

### 5.1 `GET /api/digital_inputs_config`

Añadir (por DI):

```json
{
  "di1_enabled": true,
  "di1_type": "button",
  "di1_relay": 1,
  "di1_duration": 2.0,
  "di1_inverse": 0,
  "di1_open_level": 0,
  "di2_type": "door",
  "di2_open_level": 1,
  ...
}
```

- `diN_type`: string `"button"` \| `"door"` (y aceptar `0`/`1` en POST).
- Clientes antiguos que solo leen enabled/relay/duration/inverse siguen funcionando.

### 5.2 `GET /api/digital_inputs_status`

Añadir campos **nuevos** sin quitar los actuales:

```json
{
  "di1_state": true,
  "di1_type": "button",
  "di1_waiting": false,
  "di2_state": false,
  "di2_type": "door",
  "di2_door_state": "closed"
}
```

### 5.3 UI `/digital_inputs`

Por cada DI:

1. Selector **Tipo:** Pulsador | Imán de puerta  
2. Si Pulsador → mostrar relé, duración, Normal/Inverso (como hoy)  
3. Si Imán → mostrar “Nivel = puerta abierta: LOW | HIGH”; ocultar relé/duración/inverso (o deshabilitarlos)

### 5.4 MQTT set config (si existe comando DI)

Si hay comando de configuración remota de DI, extender con `type` / `open_level` opcionales; default button.

---

## 6. Runtime — diseño de `processDigitalInput`

Pseudocódigo:

```text
processDigitalInput(n, ...):
  if !enabled: cleanup relay if button-inverse; return
  raw = digitalRead(pin)
  if type == DOOR:
     open = (raw == open_level)
     if debounced change of open:
        publishDoorContact(...)
     update last*
     return   // NUNCA controlRele*
  else:  // BUTTON
     // lógica actual Normal/Inverso intacta
```

Estado runtime: ampliar `DigitalInputState` con `bool doorOpen` / `bool doorOpenLast` (o reutilizar `lastState` solo como raw y derivar).

---

## 7. Fases de implementación

| Fase | Trabajo | Criterio |
|------|---------|----------|
| **0** | Este plan + README rama | Docs OK |
| **1** | EEPROM: `type` + `open_level`, checksum, migración | Load/save; button default |
| **2** | Runtime door + `publishDoorContact` | Mensajes en cada cambio; sin relé |
| **3** | Web UI + APIs status/config/save | Configurables ambos tipos |
| **4** | Doc contrato MQTT + prueba regresión button | Checklist firmada |
| **5** | (Opcional) comando MQTT set + estado boot sync | Backend al día al arrancar |

Bump de versión firmware sugerido al implementar: `v5.0.2` / `v5.0.2-BLE`.

---

## 8. Riesgos

| Riesgo | Mitigación |
|--------|------------|
| Parsers MQTT frágiles que asumen todo mensaje en el topic es trigger | `event` distinto; documentar filtro por `event` |
| Rebote del imán → ráfaga MQTT | Debounce 30–50 ms |
| Usuario deja `type=door` pero espera relé | UI clara; serial log “door: no relay” |
| Checksum rompe configs al ampliar | Migración marker+campos v1 → v2 |
| Confundir `inverse` (button) con `open_level` (door) | Nombres y UI separados; no reutilizar `inverse` para puerta |

---

## 9. Checklist de aceptación

### Regresión pulsador

- [ ] Normal: HIGH → relé + `digital_input_trigger` con `duration_ms`
- [ ] Inverso: LOW activa / HIGH desactiva + eventos actuales
- [ ] JSON idéntico en campos obligatorios a v5.0.0
- [ ] EEPROM sin reconfigurar tras flashear v5.0.2 (tipo button)

### Imán

- [ ] `open_level=LOW`: contacto a GND → `door_state=open` (o el mapeo elegido) y mensaje
- [ ] Cambio open→closed y closed→open → **dos** mensajes `door_contact`
- [ ] No se activa ningún relé
- [ ] Status API muestra `diN_door_state`

### Integración

- [ ] Backend que solo suscribe triggers: sin cambios
- [ ] Backend puerta: filtra `event == "door_contact"`

### Build

- [ ] `pio run -e esp32dev`
- [ ] `pio run -e esp32dev_ble`

---

## 10. Mapa de ficheros a tocar

| Fichero | Cambio |
|---------|--------|
| `src/eeprom_layout.h` | Campos `type`, `open_level`; asserts tamaño |
| `src/digital_inputs.h/.cpp` | Tipos, debounce, publish door, checksum |
| `src/main.ino` | Formularios, APIs, save validation |
| `docs/v5.0.2/` | Contrato MQTT final tras implementar |
| `docs/00 - docref/referencia/messaging/` | (opcional) enlace al contrato estable |

---

*Plan v5.0.2 — planificación únicamente; sin cambios de firmware en este paso.*
