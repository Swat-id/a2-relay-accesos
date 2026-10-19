# Respuesta a «Incidencias de compatibilidad App ↔ backend ↔ firmware»

**Fecha:** 19-Oct-2026 · **Ámbito:** firmware A2 / A2v3 (este repositorio)
**Firmware analizado:** v5.0.6 → ajustes entregados en **v5.0.7** (rama `v5.0.7_Bugs`)
**Banco de pruebas:** A2v3 `SWATID_CC641BEABF8C` (v5.0.6/v5.0.7-S3-BLE) + X1 `SWATID_A085E3CCAC98` presente en el mismo banco

Documento único de respuesta al informe de la App (19/10/2026, commit `a9a4e07`).
Toda la evidencia de este documento es de captura real (anexos A y B).

---

## 0. Veredicto por incidencia

| ID | Veredicto | Responsable principal | Acción |
|----|-----------|----------------------|--------|
| EQ-01 (WS-1) | Fuera del ámbito de este firmware | WS-1 / backend | Sin acción aquí; ver §4 para el patrón de correlación equivalente en A2 |
| **EQ-02 (A2v3 BLE)** | **Causa raíz identificada y REPRODUCIDA: la App conecta a un equipo X1 clasificándolo como A2.** El GATT del A2v3 está completo y validado | **App** (clasificación §6.5 de su propio informe) | Firmware v5.0.7 añade identificación de modelo en el anuncio + 2 endurecimientos; la App debe clasificar por manufacturer data |
| EQ-03 (X1) | Fuera del ámbito de este firmware | X1 | — |
| EQ-04 (A2/A2v3 MQTT) | Sin fallo conocido en firmware; **pregunta de contrato al backend** (§3) | Backend (a confirmar) | El A2 acepta tipo 0 legado, NO sobres firmados `swatid-mqtt/2` |
| EQ-05 (B16) | Fuera del ámbito de este firmware | B16 | — |

---

## 1. EQ-02 — Causa raíz reproducida en banco

### 1.1 La reproducción accidental que lo demuestra

Al ejecutar la «prueba mínima» (§3.5 del informe) desde un cliente BLE de
escritorio, el escáner conectó con **el primer `SWATID_*` visible**, que
resultó ser el **X1** (`SWATID_A085E3CCAC98`) y no el A2v3. Resultado:

```text
Servicio 0000ff00-… presente, pero SOLO con FF01–FF05
  → sin FF08 (devinfo) y sin FF0B (challenge)
  → la validación "A2" corta la sesión  ← EXACTAMENTE el síntoma de EQ-02
```

La secuencia observada por la App en el Pixel 9 (conexión OK → MTU 255 →
`discoverServices` OK con 3 servicios → corte antes de leer FF08) es la firma
de este escenario: 3 servicios = Generic Access + Generic Attribute + FF00,
y el corte llega porque **faltan FF0B/FF08… en el X1**. El nombre `SWATID_*`
y el servicio `FF00` son **comunes a varias familias**: no identifican el
modelo (carencia reconocida en el §6.5 del propio informe).

El segundo intento con `GATT_CONNECTION_TIMEOUT` 147 / status 133 es
coherente con reconectar contra el mismo X1 u otra pila ocupada — no se
reproduce contra el A2v3 (anexo A: segunda conexión OK tras desconectar).

### 1.2 El contrato del A2v3 está completo (evidencia)

Dump GATT real del A2v3 (anexo A): servicio `FF00` con **las 11
características FF01–FF0B**, propiedades correctas, `FF08` legible sin
autenticación, y **re-advertising verificado** tras desconexión (hipótesis 6
descartada). Además, el flujo funcional completo está validado desde cliente
real (anexo B): TOFU → challenge-response → token → FF07 → FF05 (status y
escaneo WiFi de 13 redes con el enlace estable).

Descarte del resto de hipótesis del §3.3: (1) la A2v3 con firmware <v5.0.6
**no anuncia BLE en absoluto** (el BLE en A2v3 existe desde v5.0.6) — no
puede producir este síntoma; (3)(4) falsas (anexo A); (6) falsa (anexo A).

### 1.3 Ajustes entregados en firmware v5.0.7 (defensa en profundidad)

| Cambio | Motivo |
|--------|--------|
| **Identificación de modelo en el anuncio**: manufacturer data `0xFFFF` + ASCII `"A2V3"` / `"A2"` | La App puede clasificar el modelo SIN conectar (resuelve de raíz la ambigüedad `SWATID_*`+`FF00`) |
| **Actualización de parámetros de conexión diferida 2 s** (antes se pedía en `onConnect`) | Pedirla en plena negociación MTU/discovery puede cortar algunas pilas Android; se mantienen los parámetros tolerantes (supervision 6 s) necesarios para convivir con el WiFi |
| **Watchdog de advertising** (cada 5 s) | Si el anuncio quedara parado tras una desconexión abrupta, se relanza solo — el equipo es siempre visible |

Presupuesto del anuncio (31 B por paquete), reparto explícito en v5.0.7:
**ADV = flags + nombre completo** (24 B — el nombre siempre visible, también
en escaneo pasivo) y **scan response = UUID FF00 + manufacturer data** (26 B).
**Criterio de clasificación recomendado para la App, en orden**: 1º
manufacturer data `0xFFFF`+modelo (fiable, emitido por A2/A2v3 v5.0.7); 2º si
no hay mfg: conectar y validar GATT **sin asumir modelo** (un `SWATID_*` sin
mfg puede ser X1, B16, WS-1…).

### 1.4 Lo que debe corregir la App (confirmando su propio §6)

1. No clasificar como A2 un `SWATID_*` sin manufacturer data (ítem 5 del §6).
2. Distinguir `service_missing` / `characteristics_missing` del error
   genérico, registrando los UUIDs descubiertos (ítems 3–4 del §6).
3. Tras conectar, si hay `FF08`, leerla ANTES de validar: su `device_type`
   (`"SWATID-A2"`) y `firmware_variant` identifican el equipo sin ambigüedad.

## 2. EQ-02 — Ficha de reproducción (§5 del informe, cumplimentada)

```text
ID: EQ-02-FW-01
fecha_hora_utc: 2026-10-19 (captura anexo A)
app_commit: n/a (cliente BLE de escritorio, bleak/macOS)
telefono_so: macOS 15 (CoreBluetooth)
cuenta_id: n/a (canal BLE puro)
rol_global: n/a
serial: SWATID_CC641BEABF8C (y contraste con SWATID_A085E3CCAC98, X1)
familia: A2v3
firmware_version: v5.0.6 (ajustes en v5.0.7)
firmware_variant: S3-BLE
link_role: n/a
modo_equipo: normal
online: sí (ETH); irrelevante para BLE
canal: BLE
accion: escaneo + conexión + descubrimiento + lectura FF08 + reconexión
request: prueba mínima §3.5 literal
firmware_recibio: conexión GATT
firmware_resultado: GATT completo FF01–FF0B; FF08 pública OK
ack_publicado: n/a
estado_despues: anuncio restablecido; segunda conexión OK
actuacion_fisica: n/a
```

## 3. EQ-04 — A2/A2v3 por MQTT: contrato real del firmware

No hay caso reproducible aportado; adelantamos el contrato para evitar el
fallo más probable. **El A2/A2v3 NO implementa el sobre firmado
`swatid-mqtt/2` con claves `plat-N` para órdenes de relé** (eso es WS-1/X1).
El contrato de orden remota del A2 es el tipo 0 legado (autenticación =
credenciales del broker):

```json
// → swatidhome/command/<serial>/cmd
{ "timestamp": "…", "message_id": 123, "device": "<serial>",
  "message_type": 0,
  "message_info": { "relay_number": 1, "duration": 3.5 } }
```

Respuesta (ACK) en `swatidhome/response/<serial>/rx`:
`{"message_id":123,"response_type":0,"response_info":"relay 1 activated","relay":1}` —
más evento de acceso (`source:"REMOTE_COMMAND"`) y estado en
`swatidhome/<serial>/relay_enable` (ON y OFF por timeout). Contrato completo:
[Manual de Integración MQTT v1.2](../v5.0.5_users/MANUAL-INTEGRACION-MQTT.md).

**Pregunta al backend (clave para cerrar EQ-04):** ¿qué publica
`POST /devices/{serial}/relay` para la familia A2 — tipo 0 legado o sobre
firmado? Si publica sobre firmado, esa es la causa de EQ-04: decidir entre
(a) backend publica tipo 0 para A2 (sin cambio de firmware) o (b) añadir al
A2 validación de sobres de plataforma (viable: la clave de plataforma existe
en el equipo desde v5.0.5 — estimación 1 fase de rama).

Para la matriz §4 del informe, el firmware A2 proporciona en cada orden:
recepción del `message_id` (log serie), validación, actuación física, ACK
correlado y estado posterior — los seis puntos del criterio de cierre §7.

## 4. EQ-01 / EQ-03 / EQ-05

WS-1, X1 y B16 no se gestionan desde este firmware. Sin perjuicio de ello,
el patrón de correlación que exige el informe (orden → publicación →
validación → actuación → ACK → estado) está soportado íntegramente por el
A2 vía tipo 0 + `response/rx` + `relay_enable`, y puede usarse como familia
de referencia al montar el banco de correlación.

## 5. Cambios entregados (rama `v5.0.7_Bugs`)

| Cambio | Fichero |
|--------|---------|
| Manufacturer data con modelo en el anuncio BLE | `src/main.ino` (initBLE) |
| Conn params diferidos 2 s (no en `onConnect`) | `src/main.ino` |
| Watchdog de advertising (5 s) | `src/main.ino` (loop) |
| Versión → v5.0.7 | `src/main.ino` |

4 entornos compilados; A2v3 del banco flasheada y verificada (anexo A).

---

## Anexo A — Dump GATT real (19-Oct-2026, prueba mínima §3.5)

```text
=== EQUIPOS SWATID VISIBLES ===
  SWATID_A085E3CCAC98  rssi=-57  uuids=['0000ff00-…']  mfg={}                         ← X1
  SWATID_CC641BEABF8C  rssi=-70  uuids=['0000ff00-…']  mfg={'0xFFFF': 'A2V3'}          ← A2v3 v5.0.7
  (nombre en el paquete ADV principal; UUID + modelo en scan response — reparto
   explícito del presupuesto de 31 B por paquete)

=== GATT de SWATID_CC641BEABF8C ===
Servicio 0000ff00-0000-1000-8000-00805f9b34fb
  FF01  notify,write
  FF02  notify,write,write-without-response
  FF03  notify,read,write,write-without-response
  FF04  notify,write,write-without-response
  FF05  notify,read,write,write-without-response
  FF06  notify,read,write,write-without-response
  FF07  notify,read
  FF08  notify,read
  FF09  notify,read
  FF0A  notify,read,write,write-without-response
  FF0B  notify,read

=== FF08 (pública) ===
{"device_type":"SWATID-A2","serial":"SWATID_CC641BEABF8C",
 "firmware_version":"v5.0.7","firmware_variant":"BLE","protocol_version":1,
 "capabilities":{"relays":2,"wiegand_inputs":2,"digital_inputs":2,"ble_users":5},
 "superadmin_registered":true,"active_users":0}

Desconectado. 3 s y segunda conexión…
✅ Segunda conexión OK (anuncio restablecido tras desconexión)
```

Contraste X1 (primer intento del mismo script, conectó al X1):
`FF00` presente pero solo `FF01–FF05` — sin `FF08` ni `FF0B` → una
validación "A2" corta la sesión en ese punto.

## Anexo B — Validación funcional previa (19-Oct, cliente BLE real)

```text
TOFU (FF01, 64 B)            → OK:SUPERADMIN_REGISTERED:TOKEN:…
Challenge-response (FF0B+FF01)→ OK:AUTHENTICATED:TOKEN:…
FF07 (auth)                  → estado JSON
FF05 {"cmd":"status"}        → eth/sta/ap/lock
FF05 wifi_scan_start/get     → 13 redes, enlace BLE ESTABLE durante el scan
FF05 exit_config             → ok
```

Referencia de integración App: [MANUAL-INTEGRACION-APP-BLE.md](../v5.0.6_BLE/MANUAL-INTEGRACION-APP-BLE.md) (v2.0).

---

*Smart World And Things SLU — equipo firmware A2/A2v3.*
