# Rama v5.0.5_users — Gestión uniforme de usuarios

**Base:** `v5.0.4_bugs_mqtt` (`81f9e38`)  
**Referencia de producto:** contrato MQTT del **SWATID Relay X1** v1.2.0-rc1 (canal `/vinculo`, validado 01/10/2026).  
**Objetivo:** alinear la gestión de usuarios de la controladora A2/A2v3 con el modelo ya desplegado en otras placas, sin romper BLE v4.1 ni el tipo 6 legado.

## Modelo de roles (obligatorio diferenciar)

| Rol | Quién | Acceso | Gestión |
|-----|-------|--------|---------|
| **Superadmin (plataforma)** | Clave de servicio / fábrica SWATID | **Siempre** puede autenticarse y operar (no queda fuera por bajas de admin de placa) | No la crea el instalador; provisionada o reservada |
| **Admin de placa** | Primer vínculo TOFU (slot administrador) | Pleno sobre la placa; alta/baja de usuarios; baja de sí mismo limpia usuarios | Canal `/vinculo` + BLE |
| **Usuario autorizado** | Slots 1–5 | Según `permissions` (relé, modo, códigos, red) | Solo el admin de placa (o superadmin) |

> En X1 el “superadmin” del documento de vinculación es el **admin de placa** (slot 0). En A2 v5.0.5 hay que **nombrar y separar** explícitamente el superadmin de plataforma del admin de placa para evitar ambigüedad con el `superadmin_*` actual del EEPROM BLE.

## Alcance

- Analizar gaps A2 (tipo 6 MQTT solo con credenciales de broker; sin canal `/vinculo` firmado).
- Planificar canal de vinculación uniforme (topics, challenge, HMAC, ventana remota).
- Planificar persistencia de 3 roles y compat con `BLEAuthConfig` / HKDF v4.1.
- Mantener message_type 6 como **legado** (como en X1 §4.7).

## Documentos

| Documento | Descripción |
|-----------|-------------|
| [PLAN-USUARIOS-MQTT.md](PLAN-USUARIOS-MQTT.md) | Análisis A2 vs X1, roles, gaps, fases, riesgos |
| [REF-X1-VINCULO.md](REF-X1-VINCULO.md) | Resumen del contrato X1 (canal vinculación) como referencia portable |

## Criterio de hecho (MVP)

1. `devinfo` / `challenge` / `link` (admin placa) / alta usuario firmada operan por MQTT en A2 como en X1 (topics `.../vinculo`).
2. Superadmin plataforma documentado y con política “siempre acceso” (no eliminable por `unlink` de usuario ni por clear parcial).
3. BLE existente sigue autenticando con las mismas claves de slots.
4. Tipo 6 sigue respondiendo (compat); nuevas gestiones van por `/vinculo`.
5. Manual MQTT A2 actualizado (o anexo en esta rama).

## Estado

**Planificación cerrada** (01 Oct 2026).  
Siguiente paso: implementación fases 1–4 del [plan](PLAN-USUARIOS-MQTT.md) (persistencia 3 roles → canal `/vinculo` → BLE compartido → manual).
