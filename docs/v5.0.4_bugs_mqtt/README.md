# Rama v5.0.4_bugs_mqtt

**Base:** `v5.0.3_bugs` (`5586de5`)  
**Enfoque:** revisar y dejar **operativa** la mensajería MQTT de gestión de códigos — consulta, alta y baja de **locales** y **remotos**, con semántica clara de horarios.

## Diferencia de producto (obligatoria)

| Tipo | Persistencia | Horario | Uso |
|------|--------------|---------|-----|
| **Local** | EEPROM (`StoredCodes`, máx. 50) | **Siempre activo** (sin franjas) | Acceso offline inmediato |
| **Remoto (caché)** | NVS `a2acc_rc` (máx. 40) | **Franjas + días** o **siempre activo** si `time_slots` vacío / count=0 | Caché de autorizaciones con ventana temporal |

## Alcance

- Auditar comandos `message_type: 5` (y respuestas).
- Completar huecos (p. ej. **listar remotos**).
- Unificar contratos JSON para que el backend pueda CRUD + listado sin ambigüedad.
- Actualizar manual de integración MQTT (derivado de v5.0.2).

## Documentos

| Documento | Descripción |
|-----------|-------------|
| [PLAN-MQTT-CODIGOS.md](PLAN-MQTT-CODIGOS.md) | Estado actual, gaps, contrato propuesto, fases y checklist |

## Criterio de hecho

1. MQTT: listar / crear / borrar códigos **locales** y **remotos** con respuestas JSON parseables.
2. Remoto con `time_slots` → solo válido en franja/día; sin slots → siempre activo.
3. Local nunca acepta ni persiste franjas (si llegan, se ignoran o se rechazan con error claro).
4. Manual MQTT actualizado; ejemplos listos para prueba con Mosquitto/backend.
5. Sin romper acciones existentes ya usadas en producción (compat hacia atrás).

## Estado

Rama y plan creados; implementación pendiente.
