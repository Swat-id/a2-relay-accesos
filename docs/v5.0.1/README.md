# Rama v5.0.1 — Verificación conectividad 4G / detección SIM

**Base:** `v5.0.0` (`5b404ea`)  
**Enfoque:** diagnosticar y corregir por qué el firmware **no detecta la SIM** (o se queda en `no_sim` / `absent` / `pin_required`) y validar el camino hasta registro de red en SIM7600E.

## Alcance

| Incluye | No incluye (aún) |
|---------|------------------|
| Logs AT más claros (CPIN, errores, timeouts) | PPP / MQTT por 4G (Fase 4 del plan v5) |
| Ajuste tiempos de arranque módulo (PWRKEY / espera AT) | Cambios WiFi AP/STA salvo regresiones |
| Revisión pines UART A2 vs A2v3 y `pin_swap` | BLE en S3 |
| Distinguir **módulo ausente** vs **SIM ausente** vs **PIN** | Rediseño UI completa |
| Pruebas en `esp32dev_4g` y/o `esp32dev_s3` | |

## Contexto del problema

En v5.0.0 la FSM GSM (Fase 1) ya implementa:

```text
Probing (AT) → ATI → GSN → CPIN? → Registering → Attached
```

Estados relevantes si “no hay SIM”:

| Estado API | Significado probable |
|------------|----------------------|
| `absent` | Sin respuesta AT (UART / módulo apagado / pines) |
| `no_sim` | Módulo responde pero `AT+CPIN?` no da READY/PIN/PUK |
| `pin_required` | SIM presente; falta PIN en NVS `a2acc_gsm` |
| `pin_error` | PIN rechazado (un intento; sin auto-retry) |
| `registering` | SIM OK; sin registro de red aún |

Hipótesis a verificar (orden sugerido): ver [VERIFICACION-SIM-4G.md](VERIFICACION-SIM-4G.md).

## Hardware / envs

| Env | Placa | UART GSM | Notas |
|-----|-------|----------|-------|
| `esp32dev_4g` | KC868-A2 | TX **13**, RX **34**, UART1 | Sin swap posible (GPIO34 input-only) |
| `esp32dev_s3` | KC868-A2v3 | TX **10**, RX **9**, UART2 | Swap auto + NVS `pin_swap` |

Referencias fabricante: [A2](https://www.kincony.com/esp32-4g-gps-arduino-relay.html) · [A2v3](https://www.kincony.com/kincony-kc868-a2v3-esp32-s3-2-channel-relay-module-released.html)

## Documentos

| Documento | Descripción |
|-----------|-------------|
| [VERIFICACION-SIM-4G.md](VERIFICACION-SIM-4G.md) | Checklist de campo, hipótesis, cambios previstos en `gsm_modem` |
| Código | `src/gsm_modem.*`, `src/hw_config.h`, integración en `main.ino` |
| Guía diseño | [../v5.0.0/GUIA-PORTABLE-WIFI-GSM-RED.md](../v5.0.0/GUIA-PORTABLE-WIFI-GSM-RED.md) §5 |
| Fase 1 (baseline) | [../v5.0.0/FASE-1-WIFI-GSM.md](../v5.0.0/FASE-1-WIFI-GSM.md) |

## Criterio de hecho

1. Con módulo + SIM insertada (PIN correcto o sin PIN): estado llega al menos a `registering` / `attached` (según cobertura).
2. Sin SIM: estado estable `no_sim` (no confundir con `absent`).
3. Sin módulo: `absent` en &lt; ~30 s, sin spam UART.
4. `/api/gsm/status` y serial muestran respuesta cruda o motivo de `CPIN` cuando falle.
5. Compilan los envs GSM activos (`esp32dev_4g`, `esp32dev_s3`).

## Estado

Rama creada; pendiente instrumentación y pruebas en placa.
