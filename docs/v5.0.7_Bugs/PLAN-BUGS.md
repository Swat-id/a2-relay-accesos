# Plan — Bugs v5.0.7

**Rama:** `v5.0.7_Bugs` · **Fecha apertura:** 19 Oct 2026  
**Baseline:** `v5.0.6_BLE` (`96fe781`) — FF05 WiFi/BLE validado en A2v3; binding sync; manuals App/MQTT.

---

## 1. Objetivo

Centralizar y cerrar defectos de campo / laboratorio posteriores a v5.0.6, con evidencia serial/App y sin romper contratos BLE/MQTT ya publicados.

---

## 2. Inventario (a completar)

| ID | Síntoma | Severidad | Estado |
|----|---------|-----------|--------|
| *(pendiente priorizar con el equipo)* | | | Abierto |

> Añadir filas conforme se reporten bugs. Cada bug cerrado debe enlazar commit y notas de prueba.

### Plantilla por bug

```text
### Bxxx — título corto
- Síntoma:
- Repro:
- Hipótesis:
- Fix previsto (ficheros):
- Criterio de hecho:
- Evidencia:
```

---

## 3. Áreas calientes (contexto post-v5.0.6)

| Área | Notas |
|------|--------|
| BLE + WiFi radio | Conn params + scan 120 ms/canal ya mitigaron caídas; vigilar iOS/Android reales |
| Sync BLE ↔ binding | `bindingSyncFromBle` nuevo; vigilar ecos / doble TOFU |
| Lock AP ↔ BLE config | R1–R5; casos edge al desconectar App a mitad de scan |
| A2 clásico vs A2v3 | Validación BLE completa aún pendiente en A2 clásico |
| Códigos / DI / GSM | Fuera del foco BLE Fase 1; abrir IDs si aparecen regresiones |

---

## 4. Orden de trabajo

```text
1. Registrar bug con repro mínimo
2. Clasificar severidad / target (A2 / A2v3 / ambos)
3. Fix mínimo + prueba de regresión del área
4. Actualizar esta tabla + docs de contrato si el comportamiento visible cambia
5. Compilar envs afectados
```

---

## 5. Estado

| Ítem | Estado |
|------|--------|
| Rama creada desde v5.0.6_BLE | Sí |
| Inventario inicial | Vacío — a rellenar |
| Push | Tras commit de apertura documental |
