# Rama v5.0.2 — Entradas digitales ampliadas (pulsador + imán de puerta)

**Base:** `v5.0.0` (`5b404ea`)  
**Enfoque:** ampliar DI1/DI2 con un segundo tipo de uso — **contactos de puerta (imán)** — sin romper el comportamiento ni el JSON de **pulsador** ya integrado.

## Objetivo de producto

| Tipo | Rol | Actuación | MQTT |
|------|-----|-----------|------|
| **Pulsador** (actual) | Botón / NO-NC que dispara acceso | Activa relé(s) configurados (Normal / Inverso) | `event: digital_input_trigger` (sin cambios de contrato) |
| **Imán / estado de puerta** (nuevo) | Sensor de puerta abierta/cerrada | **No** actúa relé (solo supervisión) | Evento **distinto** en cada cambio de estado lógico |

Cada DI se configura de forma independiente: DI1 puede ser pulsador y DI2 imán (o ambos del mismo tipo).

## Compatibilidad hacia atrás (obligatoria)

1. Config EEPROM existente → tipo por defecto **pulsador** (`type=0`); `di*_reserved` hoy es 0.
2. Topic MQTT de pulsador: `swatidhome/<serial>/digital_input` y campos actuales **inalterados**.
3. Integraciones que solo escuchan `digital_input_trigger` no ven mensajes de puerta.
4. API web/MQTT de config: campos nuevos **opcionales**; omitidos = comportamiento v5.0.0 / v3.x.

## Documentos

| Documento | Descripción |
|-----------|-------------|
| [PLAN-ENTRADAS-DIGITALES.md](PLAN-ENTRADAS-DIGITALES.md) | Análisis baseline, modelo de datos, mensajería, UI, fases y checklist |
| [CONTRATO-MQTT-DI.md](CONTRATO-MQTT-DI.md) | Contrato final trigger/door_contact + validación en placa |
| [MANUAL-INTEGRACION-MQTT.md](MANUAL-INTEGRACION-MQTT.md) | Manual de integración: TODA la mensajería MQTT (emitida y recibida) — fuente del PDF v1.0 |

## Código afectado (previsto)

- `src/eeprom_layout.h` — `DigitalInputConfig`
- `src/digital_inputs.h` / `.cpp` — FSM por tipo + publish
- `src/main.ino` — web `/digital_inputs`, APIs status/config/save; opcional comando MQTT set

## Criterio de hecho

- [x] Pulsador: mismo JSON y misma lógica Normal/Inverso que hoy
- [x] Imán: mensaje en **cada** transición open↔closed; HI/LOW mapeable a abierto/cerrado (validado en placa vía cambio de polaridad; imán físico pendiente de laboratorio)
- [x] Firmware con EEPROM antigua arranca sin reconfigurar (migración v1→v2 verificada)
- [x] Documentación de contrato MQTT en esta carpeta
- [x] Compilan `esp32dev`, `esp32dev_4g`, `esp32dev_ble` y `esp32dev_s3`

## Estado

**Implementada y validada en placa A2v3** (21 Sep 2026, firmware `v5.0.2-S3`):
migración EEPROM, tipo imán con debounce y sync boot/change/sync, UI web con
selector de tipo, APIs compatibles y LCD mostrando A/C para puertas. Ver
[CONTRATO-MQTT-DI.md](CONTRATO-MQTT-DI.md) con la evidencia.

## Corrección adicional en esta rama: recuperación del módem tras reinicio del ESP

**Síntoma:** tras flashear v5.0.2, el 4G quedaba en `absent` (módulo sin responder a AT).
**Causa raíz (no relacionada con las DI):** el SIM7600 se alimenta del socket y
NO se reinicia con el ESP32 — si el ESP se reinicia o reflashea con los datos
4G activos, el módulo permanece en modo **CMUX/PPP**, donde ignora los AT
"planos" de la FSM. Bug latente desde v5.0.1.

**Fix:** secuencia de recuperación intercalada en el probe (siempre por el
mapeo TX/RX aprendido): escape `+++` (modo DATA) en la ronda 3 y trama de
cierre CMUX **CLD** (3GPP TS 27.010) en las rondas 5 y 8.

**Validado en placa:** módulo atascado en CMUX real → `CLD` en ronda 5 →
`Módulo detectado` → registro → PPP → `Datos 4G conectados (IP 10.162.138.95)`.
Cualquier reinicio del ESP con datos 4G activos ahora se auto-recupera en ~10 s.
