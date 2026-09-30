# Contrato MQTT — gestión de códigos locales y remotos (v5.0.4)

**Validado en placa** A2v3 `SWATID_CC641BEABF8C` el 29-Sep-2026 (broker de
producción, mosquitto). Todas las capturas de este documento son reales.

- Topic de comando: `swatidhome/command/<serial>/...` — envelope con
  `message_type: 5` y `message_info.action`.
- Topic de respuesta: `swatidhome/response/<serial>/rx`.
- Toda respuesta lleva el texto legacy en `response_info` **y** (v5.0.4) un
  objeto estructurado `data`. Los clientes existentes no necesitan cambios.
- Buffer MQTT ampliado a 4 KB; los listados se paginan con `offset`/`limit`.

## 1. Acciones

| Acción | Ámbito | Novedad v5.0.4 |
|---|---|---|
| `list_local_codes` | local | paginación, `scope`, `keyboard_id` (+`keyboard` legacy), `data` |
| `add_local_code` | local | rechaza `time_slots`; ACK con `data` |
| `remove_local_code` | local | ACK con `data.remaining` |
| `clear_local_codes` | local | ACK con `data.cleared_count` |
| **`list_remote_codes`** | remoto | **NUEVA** — franjas completas + `always_active` |
| `add_remote_code` | remoto | validación completa (tipo/valor/teclado/relé/franjas); ACK con `data` |
| `remove_remote_code` | remoto | ACK con `data.remaining` |
| `clear_remote_codes` | remoto | ACK con `data.cleared_count` |

## 2. Reglas de negocio

- **Locales**: siempre activos. `time_slots` en `add_local_code` → error
  `local codes do not support time_slots (use add_remote_code)`.
- **Remotos**: `time_slots` ausente o `[]` → `always_active: true`. Con
  franjas (máx. 4): válido solo si día+hora actual caen en alguna.
- Franja: `start_hour/minute`, `end_hour/minute`, `days_of_week` (bitmask
  1=Lun … 32=Sáb, 64=Dom; 1–127) — **todos obligatorios**; una franja
  incompleta o inválida rechaza el alta entera (antes se ignoraba en silencio).
- Validaciones comunes: `code_type` ∈ {PIN, TAG}; `code_value` 1–16 chars;
  `keyboard_id` 0/1/2 (0 = ambos); `relay` 1/2.

## 3. Ejemplos reales (petición → respuesta)

### list_local_codes

```json
{"message_type":5,"message_info":{"action":"list_local_codes","offset":0,"limit":50}}
```

```json
"data": {
  "scope":"local","count":6,"max":50,"offset":0,"limit":50,
  "validation_mode":"local_first","always_active":true,
  "codes":[{"id":0,"type":"PIN","value":"123455","keyboard_id":0,"keyboard":0,"relay":2}, "..."],
  "showing":6,"truncated":false
}
```

`truncated: true` añade `next_offset` para pedir la página siguiente. En
páginas grandes `response_info` deja de duplicar el JSON (lleva un resumen).

### add_local_code

```json
{"message_type":5,"message_info":{"action":"add_local_code","code_type":"PIN","code_value":"7777","keyboard_id":1,"relay":1}}
```

```json
"data": {"scope":"local","code_type":"PIN","code_value":"7777","keyboard_id":1,"relay":1,"always_active":true,"total_codes":7}
```

### add_remote_code (con franja Lun–Vie 08–20)

```json
{"message_type":5,"message_info":{"action":"add_remote_code","code_type":"PIN","code_value":"6666","keyboard_id":0,"relay":2,
  "time_slots":[{"start_hour":8,"start_minute":0,"end_hour":20,"end_minute":0,"days_of_week":31}]}}
```

```json
"data": {"scope":"remote","code_type":"PIN","code_value":"6666","keyboard_id":0,"relay":2,"always_active":false,"time_slots_count":1,"total_codes":2}
```

### list_remote_codes (NUEVA)

```json
{"message_type":5,"message_info":{"action":"list_remote_codes","offset":0,"limit":8}}
```

```json
"data": {
  "scope":"remote","count":2,"max":40,"offset":0,"limit":8,
  "codes":[
    {"id":0,"type":"PIN","value":"5555","keyboard_id":0,"relay":1,"always_active":true,"time_slots_count":0,"time_slots":[]},
    {"id":1,"type":"PIN","value":"6666","keyboard_id":0,"relay":2,"always_active":false,"time_slots_count":1,
     "time_slots":[{"start_hour":8,"start_minute":0,"end_hour":20,"end_minute":0,"days_of_week":31,"days_string":"Lun-Vie"}]}
  ],
  "showing":2,"truncated":false
}
```

Límite por página: 8 por defecto, máximo 10 (una entrada con 4 franjas ronda
los 320 B y el buffer es de 4 KB).

### remove_local_code / remove_remote_code

```json
"data": {"scope":"local","code_type":"PIN","code_value":"7777","remaining":6}
```

### Errores validados en placa

| Petición | `response_info` |
|---|---|
| `add_local_code` con `time_slots` | `local codes do not support time_slots (use add_remote_code)` |
| Franja con `days_of_week: 0` | `time_slots[0] invalid (hours 0-23, minutes 0-59, days_of_week 1-127)` |
| Franja incompleta | `time_slots[i] incomplete (start_hour, ... required)` |
| Más de 4 franjas | `too many time_slots (max 4)` |
| `relay: 5` | `invalid relay (1 or 2)` |
| Acción desconocida | `unknown action: <acción>` |

Los errores llevan `response_type: 1`; los éxitos `response_type: 0`.

## 4. Compatibilidad

- Nombres de acciones sin cambios; campos nuevos solo aditivos.
- `list_local_codes`: el string JSON en `response_info` se mantiene (con
  `keyboard_id` añadido y `keyboard` conservado) mientras quepa en 1,5 KB.
- El `get status` (tipo 2) sigue incluyendo los arrays completos como antes.

## 5. Matriz de pruebas ejecutada (29-Sep-2026)

| # | Caso | Resultado |
|---|------|-----------|
| L1 | add_local PIN kb=1 relé=1 | ✅ ACK `data`, total_codes 7 |
| L2 | add_local con time_slots | ✅ error explícito |
| L3 | list_local_codes | ✅ 6 códigos, legacy + data |
| L4 | remove_local_code | ✅ remaining 6 |
| R1 | add_remote sin slots | ✅ `always_active: true` |
| R2 | add_remote Lun–Vie 08–20 | ✅ ACK; **runtime pendiente** (teclear 6666 dentro/fuera de franja) |
| R3 | list_remote_codes | ✅ slots + days_string |
| R4 | remove_remote_code | ✅ remaining 1 |
| R6 | slot days_of_week=0 | ✅ rechazado |
| — | relay inválido / acción desconocida | ✅ rechazados |
| L5/R5 | clear_* | ⏸ no ejecutados sobre la placa (tiene códigos reales); código revisado, ACK con `cleared_count` |

Pendiente en placa: teclear `6666#` dentro (abre relé 2) y fuera (rechaza) de
la franja Lun–Vie 08–20 para cerrar R2 end-to-end. El código remoto `6666`
queda cargado en el equipo para esa prueba.
