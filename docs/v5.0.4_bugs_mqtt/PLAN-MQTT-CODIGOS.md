# Plan — MQTT gestión de códigos locales y remotos (v5.0.4_bugs_mqtt)

**Rama:** `v5.0.4_bugs_mqtt` · **Fecha:** 29 Sep 2026  
**Baseline:** firmware v5.0.2+ / docs [MANUAL-INTEGRACION-MQTT.md](../v5.0.2/MANUAL-INTEGRACION-MQTT.md) §4.5  
**Código:** `src/main.ino` (`case 5`), `src/local_codes.*`, `src/remote_codes.*`

---

## 1. Objetivo

Dejar **operativo y documentado** el ciclo completo por MQTT:

| Operación | Locales | Remotos |
|-----------|---------|---------|
| Consultar / listar | Sí (mejorar respuesta) | **Falta** `list_remote_codes` |
| Crear | `add_local_code` | `add_remote_code` (+ franjas o siempre) |
| Borrar uno | `remove_local_code` | `remove_remote_code` |
| Borrar todos | `clear_local_codes` | `clear_remote_codes` |

Semántica de negocio:

- **Locales:** siempre activos; campos: `code_type`, `code_value`, `keyboard_id` (0/1/2), `relay` (1/2).
- **Remotos:** mismos campos + `time_slots[]` opcional:
  - `time_slots` ausente o `[]` / `count=0` → **siempre activo** (ya implementado en `isRemoteCodeStored`).
  - Con slots → válido solo si día (`days_of_week` bitmask) y hora actual entran en alguna franja (máx. 4).

---

## 2. Estado actual (auditoría)

### 2.1 Entrada de comandos

- Topic suscrito: `swatidhome/command/<serial>/#` (y variantes grant).
- `message_type: 5` + `message_info.action`.

### 2.2 Acciones implementadas hoy

| Acción | Estado | Notas / gaps |
|--------|--------|--------------|
| `add_remote_code` | OK parcial | Valida type/value/kb/relay; slots opcionales. No valida `isTimeSlotValid` en MQTT. Duplicado exacto type+value rechaza (no actualiza). No distingue “siempre” en la respuesta. |
| `remove_remote_code` | OK | Por type+value; no filtra keyboard. |
| `clear_remote_codes` | OK | |
| `add_local_code` | OK parcial | `keyboard_id` default 0. Respuesta mezcla texto + JSON en string. |
| `remove_local_code` | OK | `keyboard_id` opcional (-1 = cualquiera). |
| `clear_local_codes` | OK | |
| `list_local_codes` | OK parcial | Truncado a 30; payload en `response_info` como string JSON; campos `keyboard` vs `keyboard_id` inconsistentes con add. |
| **`list_remote_codes`** | **AUSENTE** | Solo aparecen remotos en el JSON grande de **get status / info** (tipo 2), no como comando de listado dedicado. |

### 2.3 Comportamiento horario remoto (runtime)

En `remote_codes.cpp` → `isRemoteCodeStored`:

```text
si time_slots_count == 0 → válido (siempre activo)
si time_slots_count > 0  → exige isCurrentTimeInTimeSlots(...)
```

Esto ya cumple el producto; falta **exponerlo** en listado/alta (`always_active: true/false`).

### 2.4 Locales y horarios

`CodeEntry` no tiene franjas. Si el backend envía `time_slots` en `add_local_code`, hoy se **ignoran**. Decisión v5.0.4: **rechazar** con error explícito si vienen slots (evita falsa sensación de restricción horaria).

### 2.5 Respuestas (`publishResponse`)

Hoy muchas respuestas son texto libre en un campo string. Para listados, el integrador debe parsear JSON embebido. Propuesta: respuesta estructurada con `result` / `data` (compat: mantener `response_info` texto + añadir objeto `data` si no rompe clientes).

---

## 3. Contrato MQTT propuesto (tipo 5)

Topic comando (igual que hoy):

`swatidhome/command/<serial>/...`  
Envelope mínimo:

```json
{
  "timestamp": "...",
  "message_id": 1001,
  "device": "<serial>",
  "message_type": 5,
  "message_info": { "action": "...", ... }
}
```

Topic respuesta (el ya usado por `publishResponse` — documentar path exacto en implementación).

### 3.1 Locales — siempre activos

#### `list_local_codes`

```json
"message_info": { "action": "list_local_codes", "offset": 0, "limit": 50 }
```

Respuesta `data`:

```json
{
  "scope": "local",
  "count": 12,
  "max": 50,
  "offset": 0,
  "limit": 50,
  "truncated": false,
  "validation_mode": "local_first",
  "always_active": true,
  "codes": [
    { "type": "PIN", "value": "1234", "keyboard_id": 1, "relay": 1 }
  ]
}
```

#### `add_local_code`

Obligatorios: `code_type`, `code_value`, `relay`.  
Opcional: `keyboard_id` (default 0).  
Si llega `time_slots` → error `local codes do not support time_slots`.

#### `remove_local_code` / `clear_local_codes`

Sin cambio de semántica; alinear nombres de campo en respuesta (`keyboard_id`).

### 3.2 Remotos — franjas o siempre

#### `list_remote_codes` (**nuevo**)

```json
"message_info": { "action": "list_remote_codes", "offset": 0, "limit": 40 }
```

Respuesta `data`:

```json
{
  "scope": "remote",
  "count": 3,
  "max": 40,
  "truncated": false,
  "codes": [
    {
      "type": "PIN",
      "value": "9999",
      "keyboard_id": 0,
      "relay": 2,
      "always_active": false,
      "time_slots_count": 1,
      "time_slots": [
        {
          "start_hour": 8, "start_minute": 0,
          "end_hour": 20, "end_minute": 0,
          "days_of_week": 31,
          "days_string": "Lun-Vie"
        }
      ]
    },
    {
      "type": "TAG",
      "value": "12345678",
      "keyboard_id": 2,
      "relay": 1,
      "always_active": true,
      "time_slots_count": 0,
      "time_slots": []
    }
  ]
}
```

#### `add_remote_code`

- Sin `time_slots` o array vacío → `always_active=true`.
- Con slots → validar cada uno con `isTimeSlotValid`; `days_of_week` 1–127.
- Opcional futuro: `replace: true` para actualizar en lugar de fallar por duplicado (fase 2).

Bitmask días (igual que hoy / web):

| Bit | Día |
|-----|-----|
| 1 | Lun |
| 2 | Mar |
| 4 | Mié |
| 8 | Jue |
| 16 | Vie |
| 32 | Sáb |
| 64 | Dom |

#### `remove_remote_code` / `clear_remote_codes`

Mantener; enriquecer ACK con eco de type/value.

### 3.3 Alias / descubribilidad (opcional)

| Alias | Equivale a |
|-------|------------|
| `list_codes` + `scope: local\|remote` | `list_local_codes` / `list_remote_codes` |
| `get_codes` | mismo |

Solo si no complica el parser; MVP = acciones explícitas.

---

## 4. Gaps y cambios de firmware

| # | Cambio | Prioridad |
|---|--------|-----------|
| C1 | Implementar `list_remote_codes` (JSON estructurado, paginación offset/limit) | P0 |
| C2 | Unificar respuesta de `list_local_codes` (mismos campos, `keyboard_id`, `scope`) | P0 |
| C3 | `add_local_code`: rechazar `time_slots` si presentes | P0 |
| C4 | `add_remote_code`: validar slots; echo `always_active` en ACK | P0 |
| C5 | `publishResponse` con objeto `data` (o topic `.../response` JSON puro) sin romper texto legacy | P1 |
| C6 | Paginación real si count > buffer MQTT (~2–4 KB) | P1 |
| C7 | `update_remote_code` / replace on duplicate | P2 |
| C8 | Eventos `events/.../codes` también en altas MQTT (hoy más orientado a BLE) | P2 |

### Ficheros

- `src/main.ino` — `case 5`
- `src/remote_codes.h/.cpp` — helpers list/serialize si se extraen
- `src/local_codes.h/.cpp` — opcional helper list
- `docs/v5.0.4_bugs_mqtt/` — contrato final + ejemplos
- `docs/v5.0.2/MANUAL-INTEGRACION-MQTT.md` o copia versionada en esta rama

---

## 5. Compatibilidad hacia atrás

| Regla | Detalle |
|-------|---------|
| Acciones existentes | Siguen aceptándose con los mismos nombres |
| Campos nuevos | Solo aditivos en respuestas (`always_active`, `scope`, `data`) |
| `list_local_codes` | Si un cliente parsea el string en `response_info`, mantener el JSON ahí **y** duplicar en `data` si se amplía el envelope |
| Status tipo 2 | `stored_codes` / `stored_remote_codes` se mantienen; list_* es el camino CRUD recomendado |

---

## 6. Fases de implementación

| Fase | Trabajo | Criterio |
|------|---------|----------|
| **0** | Este plan | Docs OK |
| **1** | C1–C4 en firmware | Mosquitto: list/add/remove local y remoto |
| **2** | C5–C6 respuestas + paginación | Listados >30/40 sin truncar a ciegas |
| **3** | Manual + ejemplos JSON en `docs/v5.0.4_bugs_mqtt/ejemplos/` | Integrador puede copiar/pegar |
| **4** | (Opcional) C7–C8 | Update + eventos |

---

## 7. Plan de pruebas (MQTT)

### Locales

| # | Caso | Esperado |
|---|------|----------|
| L1 | `add_local_code` PIN kb=1 relé=1 | ACK ok; list lo muestra; acceso siempre |
| L2 | `add_local_code` con `time_slots` | Error explícito |
| L3 | `list_local_codes` | JSON con todos (o página) |
| L4 | `remove_local_code` | Desaparece; acceso falla |
| L5 | `clear_local_codes` | count=0 |

### Remotos

| # | Caso | Esperado |
|---|------|----------|
| R1 | `add_remote_code` sin slots | `always_active=true`; acceso cualquier día/hora |
| R2 | `add_remote_code` Lun–Vie 08–20 | Fuera de franja → no abre; dentro → abre |
| R3 | `list_remote_codes` | Incluye slots + `always_active` |
| R4 | `remove_remote_code` | Eliminado |
| R5 | `clear_remote_codes` | Vacío |
| R6 | Slot con `days_of_week=0` | Rechazado en add |

### Regresión

- Validación acceso local/remoto sin MQTT de gestión.
- Tipo 2 get status sigue incluyendo arrays.
- No romper tipo 0/1/4/7.

---

## 8. Riesgos

| Riesgo | Mitigación |
|--------|------------|
| Payload MQTT > MTU / buffer PubSubClient | Paginación; documentar `limit` máx. |
| Clientes que solo leen `response_info` texto | Mantener string JSON ahí en list_* |
| Reloj desfasado → franjas mal aplicadas | Depende RTC/NTP (doc); no bloquear locales |
| Confundir “remoto MQTT” con “validación remota online” | Glosario en manual: caché NVS vs request tipo 0 |

---

## 9. Glosario (para el manual)

- **Código local:** persistido en equipo; siempre válido si coincide type/value/teclado.
- **Código remoto (caché):** persistido tras alta MQTT/web; puede tener calendario; **no** es la respuesta online a `message_type 0`.
- **Validación remota online:** solicitud `.../access` + grant; independiente del CRUD tipo 5.

---

## 10. Estado

| Ítem | Estado |
|------|--------|
| Rama `v5.0.4_bugs_mqtt` | Creada |
| Plan | Este documento |
| C1 `list_remote_codes` | **Implementado y validado en placa** (29-Sep) |
| C2 unificación `list_local_codes` | **Implementado y validado** (paginación, `keyboard_id`+legacy) |
| C3 rechazo `time_slots` en local | **Implementado y validado** |
| C4 validación completa `add_remote_code` | **Implementado y validado** (tipo/valor/kb/relé/franjas) |
| C5 respuestas con `data` | **Implementado** (`publishDataResponse`, buffer 4 KB) |
| C6 paginación | **Implementado** (`offset`/`limit`/`next_offset`) |
| C7/C8 (update / eventos) | Pendiente (fase 4 opcional) |
| Matriz de pruebas | Ver [CONTRATO-MQTT-CODIGOS.md](CONTRATO-MQTT-CODIGOS.md) §5 — falta R2 runtime (teclear 6666 en/fuera de franja) y clears |
| Compilación | 4 entornos OK; flasheado en A2v3 CC64 |
| Push | Cuando el usuario lo indique |

---

*Planificación únicamente — sin cambios de firmware en este paso.*
