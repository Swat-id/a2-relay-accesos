# Banco de pruebas BLE — detección → acceso (A2 / A2v3)

**Versión:** v1.0 · **Fecha:** Octubre 2026 · **Firmware:** v5.0.7
**Suite automatizada:** `banco_ble.py` (cliente bleak; ejecutar desde
Terminal en macOS por el permiso de Bluetooth). Uso:
`banco_ble.py [SERIAL]` (por defecto `SWATID_CC641BEABF8C`).

Cubre exactamente el tramo que falla según la App: desde que el equipo se
anuncia hasta que la sesión queda autenticada y operativa.

## Casos

### Fase D — Detección y clasificación

| # | Caso | Criterio de PASS |
|---|------|------------------|
| D1 | El equipo anuncia su nombre completo | `SWATID_<serial>` visible en escaneo |
| D2 | El anuncio incluye el servicio FF00 | UUID en adv/scan response |
| D3 | Manufacturer data `0xFFFF` con el modelo | `"A2"` o `"A2V3"` |
| D4 | Clasificación posible SIN conectar | modelo no vacío (si falta, la App NO debe asumir A2: puede ser X1/B16/WS-1) |

### Fase C — Contrato GATT e identidad

| # | Caso | Criterio |
|---|------|----------|
| C1 | FF00 con FF01–FF0B completas | 11/11 características |
| C2 | FF08 legible sin autenticación | lectura pública OK |
| C3 | `device_type` identifica el MODELO | `SWATID-A2V3` en A2v3, `SWATID-A2` en A2 (v5.0.7) |
| C4 | Coherencia anuncio ↔ FF08 | mfg `A2V3` ⇔ `device_type SWATID-A2V3` |
| C5 | `firmware_variant` correcto | `S3-BLE` / `BLE` |

### Fase A — Acceso

| # | Caso | Criterio |
|---|------|----------|
| A1 | Clave INCORRECTA rechazada | `ERROR:INVALID_RESPONSE` (nunca abre sesión) |
| A2 | TOFU en equipo virgen | `OK:SUPERADMIN_REGISTERED:TOKEN:…` (si ya hay dueño, se omite y se usa la clave guardada) |
| A3 | Challenge-response con clave propia | `OK:*TOKEN:…` |
| A4 | FF07 accesible tras autenticar | `auth:true` + usuario |
| A5 | FF05 operativo (permiso red) | JSON de estado |

### Fase R — Reconexión

| # | Caso | Criterio |
|---|------|----------|
| R1 | Re-advertising tras desconexión | visible en ≤15 s |
| R2 | Segunda conexión aceptada | conexión GATT OK |

## Preparación del banco

1. **Equipo virgen para probar TOFU** (el caso de la App): si el equipo tiene
   dueño de pruebas anteriores, portal web → `/binding` → **«Restablecer
   vínculos (admin + usuarios)»** (borra admin+usuarios, conserva plataforma
   y abre la ventana TOFU 10 min).
2. Apagar/alejar otros equipos `SWATID_*` del banco o pasar el serial
   objetivo como argumento (los X1/B16 cercanos son la causa típica de
   "conecta pero corta": no tienen FF08/FF0B).
3. La clave de prueba se guarda en `/tmp/ble_key_<SERIAL>.hex` y se reutiliza
   entre ejecuciones (A2/A3 funcionan con dueño ya registrado).

## Interpretación rápida de fallos

| Falla | Diagnóstico |
|-------|-------------|
| D1 | No anuncia: ¿alimentada? ¿otro central ya conectado? (1 conexión simultánea) |
| D3/D4 | Firmware <v5.0.7 (sin mfg data) → la App no debe asumir modelo |
| C1 con 5/11 | **Estás conectado a un X1**, no a un A2/A2v3 |
| C3/C4 | Firmware <v5.0.7 (`device_type` siempre `SWATID-A2`) |
| A1 en verde pero A3 falla | El equipo tiene OTRO dueño → restablecer vínculos (web) |
| R1 | Pila sin re-anunciar → no debería ocurrir (watchdog v5.0.7); capturar serie |
