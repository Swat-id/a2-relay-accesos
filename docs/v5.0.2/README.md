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

## Código afectado (previsto)

- `src/eeprom_layout.h` — `DigitalInputConfig`
- `src/digital_inputs.h` / `.cpp` — FSM por tipo + publish
- `src/main.ino` — web `/digital_inputs`, APIs status/config/save; opcional comando MQTT set

## Criterio de hecho

- [ ] Pulsador: mismo JSON y misma lógica Normal/Inverso que hoy
- [ ] Imán: mensaje en **cada** transición open↔closed; HI/LOW mapeable a abierto/cerrado
- [ ] Firmware con EEPROM antigua arranca sin reconfigurar (tipo pulsador)
- [ ] Documentación de contrato MQTT en esta carpeta
- [ ] Compilan `esp32dev` y `esp32dev_ble` (y S3 si aplica)

## Estado

Rama y plan creados; implementación pendiente.
