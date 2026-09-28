# Rama v5.0.3_bugs

**Base:** `v5.0.2` (`388a32c`)  
**Enfoque:** corregir defectos de campo detectados tras v5.0.2 — sin ampliar alcance de producto.

## Bugs a verificar

| # | Síntoma | Hipótesis iniciales |
|---|---------|---------------------|
| **B1** | Códigos con **teclado 1** + relé 1 o 2 **no abren**; teclado 2 sí funciona | Lectura Wiegand1 (pines A2v3), matching `keyboard_id`, o camino `validateCode`/`controlRele` solo para KB1 |
| **B2** | Operaciones **lentas** con **baja cobertura** (4G/red) | Timeouts AT/PPP/MQTT bloqueando `loop()`; reintentos agresivos; `publish`/`EEPROM` en camino crítico |

## Documentos

| Documento | Descripción |
|-----------|-------------|
| [PLAN-BUGS.md](PLAN-BUGS.md) | Análisis, hipótesis priorizadas, plan de pruebas y cambios previstos |

## Criterio de hecho

1. Código PIN/TAG asignado a teclado 1 + relé 1 (y otro a relé 2) abre el relé correcto en placa.
2. Misma matriz para teclado 2 sin regresión.
3. Con CSQ bajo / MQTT inestable: acceso local sigue en tiempo razonable (&lt; ~300 ms tras `#`); UI/GSM no congelan el loop &gt; 1 s de forma sostenida.
4. Evidencia serial + checklist en este directorio.

## Estado

Rama y plan creados; correcciones pendientes de reproducción en placa.
