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

## Teclado Wiegand 2 en A2v3: pines dedicados IO4/IO5 (25 Sep 2026)

**Resolución definitiva** (tras descartar dos vías): el segundo teclado se
conecta a los conectores **IO4 (D0)** e **IO5 (D1)** → chips GPIO4/GPIO5,
verificado con sniffer de flancos (13 D0 / 7 D1 al teclear `1111#`) y validado
end-to-end: lectura de PIN + petición MQTT `keyboard_id: 2`, `keyboard_pins: "4/5"`.

Hallazgos del camino (ver matriz de hardware v5.0.0 actualizada):

- Los bornes DI1/DI2 quedaron descartados como teclado 2: son necesarios para
  pulsadores de salida / contactos de puerta. El selector `bornes_mode`
  desarrollado para ello queda inerte (`WIEGAND2_SHARES_DI 0`) y se sanea a 0.
- El conector rotulado "SDA/SCL/GND/3V3" **no está conectado al MCU** en esta
  revisión de PCB (verificado eléctricamente). El modo experimental
  "teclado 2 en bus I2C" (bornes_mode=2, guardas i2c_guard.h) funcionó a nivel
  de firmware (cero tramas fantasma con LCD a 1 Hz) pero es inaccesible
  físicamente; el código queda disponible por si otra revisión expone el bus.
- Los lectores se alimentan **siempre a 12 V** (nunca del pin 3V3: un cruce
  destruyó el carril 3,3 V de una placa).

**Robustez I2C añadida** (subproducto valioso, activa en producción): si el
bus I2C queda retenido o falla, RTC y LCD se desactivan/suspenden con log
claro y reintento cada 60 s, sin afectar jamás a accesos, relés, teclados,
DI, web, MQTT ni red. Recuperación de bus en caliente (9 pulsos SCL + STOP)
con re-detección automática de la pantalla. Estado en `rtcStatusJson()`
(`"i2c_bus":"ok|error"`).
