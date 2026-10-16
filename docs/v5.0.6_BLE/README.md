# Rama v5.0.6_BLE — Configuración completa y segura por BLE

**Base:** `v5.0.5_users` (`afd9079`)  
**Objetivo:** que la App pueda **configurar el equipo al completo por BLE**, de forma robusta y segura, cubriendo (como mínimo) todo lo que hoy permite el portal web — y evitando conflictos como **configurar WiFi por BLE mientras el AP de gestión está activo**.

## Prioridades de producto

1. **WiFi por BLE con escaneo:** el equipo debe devolver las redes a su alcance antes/durante la configuración STA.
2. **Exclusión mutua AP ↔ BLE WiFi:** no configurar STA/AP “a la vez” por dos vías; política clara y segura.
3. **Paridad portal ↔ BLE:** checklist de cada pantalla/acción web mapeada a característica o comando BLE.
4. **Seguridad:** auth challenge-response (v4.1+), permisos por bitmask, sin exponer secretos (claves WiFi en claro solo en WRITE, nunca en READ/status).

## Documentos

| Documento | Descripción |
|-----------|-------------|
| [PLAN-BLE-CONFIG.md](PLAN-BLE-CONFIG.md) | Auditoría FF01–FF0B vs portal, gaps, diseño WiFi scan, fases |
| Referencia existente | [BLE_INTEGRATION.md](../v4.1/Firmware_description/BLE_INTEGRATION.md), [BLE_FF05](../00%20-%20docref/referencia/integration/BLE_FF05_NETWORK_CONFIG.md) |

## Criterio de hecho (MVP)

- [ ] App obtiene lista de SSIDs (RSSI/enc) por BLE y configura STA sin usar el portal AP.
- [ ] Con sesión BLE de red activa, el AP de boot_window no compite (o se apaga / se bloquea config web WiFi).
- [ ] Operaciones web críticas tienen equivalente BLE (o quedan explícitamente “solo web/MQTT” con justificación).
- [ ] Auth + permisos en todos los WRITE nuevos; respuestas con EOT/chunking estables.
- [ ] Manual App actualizado en esta carpeta.

## Estado

Rama y planificación creadas; implementación pendiente.
