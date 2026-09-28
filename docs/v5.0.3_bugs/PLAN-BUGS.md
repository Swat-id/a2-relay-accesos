# Plan — Bugs v5.0.3 (teclado 1 / lentitud con baja cobertura)

**Rama:** `v5.0.3_bugs` · **Fecha:** 28 Sep 2026  
**Baseline:** `v5.0.2` — Wiegand A2v3 mapeado (KB1: GPIO18/8, KB2: GPIO4/5), DI pulsador/imán, GSM/PPP.

---

## Bug B1 — Códigos de teclado 1 no abren relé

### Síntoma (reportado)

Al crear códigos con **teclado = 1** y **relé = 1 o 2**, el acceso **no abre**.  
Con **teclado = 2** el comportamiento es correcto (mismo tipo de código / mismo relé).

### Flujo actual relevante

```text
Wiegand ISR → processWiegand* → processKey / TAG
  → validateCode(code, type, keyboardId)
      → isCodeStored(..., keyboardId, &relay)   // local
      → isRemoteCodeStored(...)                 // caché remota
      → controlReleWithDuration(releDuration, relay)
```

Matching local (`local_codes.cpp`): válido si `keyboard_id == 0` (ambos) **o** `keyboard_id == keyboardId` del lector.

Modo torno: ignora el relé del código y usa `getRelayForKeyboard(keyboardId)` — documentar si el fallo ocurre con torno ON.

### Hipótesis priorizadas

| Pri | Hipótesis | Cómo verificar |
|-----|-----------|----------------|
| H1 | **Teclado 1 no genera trama** en A2v3 (pines 18/8 / cableado GPIO1–GPIO2) → nunca llega `validateCode(..., 1)` | Serial: ausentes logs `🔐 [TECLADO 1]`; sniffer flancos en IO GPIO1/2 |
| H2 | Llega validación KB1 pero **código no hace match** (UI guarda mal `keyboard_id`, tipo PIN/TAG, espacios) | Log `addCode` + listado web; al teclear: `Código válido LOCAL` vs envío remoto |
| H3 | Match OK pero **`controlReleWithDuration` falla** solo en algún relé desde KB1 (poco probable si KB2+relé1 funciona) | Log pin/relé; probar KB2→relé1 y KB1→relé1 |
| H4 | **Modo torno** o `localValidationFirst=false` + MQTT caído: KB1 queda esperando remoto; KB2 tiene código en caché distinta | Revisar config torno / orden validación |
| H5 | Bug en UI/API: al elegir “Teclado 1” se persiste otro valor | Inspeccionar POST web / MQTT type add_code |
| H6 | Confusión de prueba: código KB1 probado en teclado 2 (match rechazado) | Matriz de prueba cruzada §B1.3 |

### Cambios previstos (tras confirmar H*)

1. **Instrumentación temporal** (o flags debug): en `validateCode` loguear `keyboardId`, resultado `localFound`/`remoteFound`, `relayToActivate`, y si se llama `controlRele*`.
2. Si H1: revisar `hw_config.h` Wiegand1, `INPUT_PULLUP`, ISR attach, alimentación 12 V del lector; contrastar con matriz v5.0.0/v5.0.2.
3. Si H2/H5: corregir formulario `/codes` o parser MQTT; migración de códigos mal guardados si aplica.
4. Si H4: garantizar fallback local inmediato cuando el código existe (ya hay ramas de emergencia; auditar que KB1 las use igual que KB2).
5. Test de regresión documentado; quitar logs ruidosos o dejarlos tras `#ifdef`.

### Matriz de prueba B1

| # | Código (kb / relé) | Lector usado | Esperado |
|---|--------------------|--------------|----------|
| 1 | kb=1, relé=1 | Teclado 1 | Abre relé 1 |
| 2 | kb=1, relé=2 | Teclado 1 | Abre relé 2 |
| 3 | kb=2, relé=1 | Teclado 2 | Abre relé 1 |
| 4 | kb=2, relé=2 | Teclado 2 | Abre relé 2 |
| 5 | kb=0 (ambos), relé=1 | Teclado 1 y 2 | Abre en ambos |
| 6 | kb=1, relé=1 | Teclado 2 | **No** abre (rechazo) |
| 7 | Igual con TAG | según | Misma lógica |

Capturar por cada caso: fragmento serial + (opcional) evento MQTT `access`.

### Ficheros a tocar (candidatos)

- `src/main.ino` — `validateCode`, process Wiegand1, UI códigos  
- `src/local_codes.cpp` — matching / logs  
- `src/hw_config.h` — pines Wiegand1 si H1 confirmada  
- `docs/v5.0.3_bugs/` — evidencia y cierre del bug  

---

## Bug B2 — Lentitud con baja cobertura

### Síntoma (reportado)

Algunas operaciones se perciben **lentas** cuando hay **baja cobertura** (típico 4G / CSQ bajo o registro inestable). No debe degradar el camino de acceso local.

### Zonas sensibles (baseline)

| Zona | Riesgo |
|------|--------|
| `gsm_modem` FSM | Timeouts AT (`GSM_CMD_TIMEOUT_MS`, `CFUN` 10 s); `AT+CRESET` ~15 s en rescan |
| PPP / CMUX | `PPP.begin`, reconexión, eventos WiFi/PPP |
| MQTT | `mqttClient.publish` / reconnect sobre enlace 4G malo; keepalive |
| Web | `handleClient` + páginas grandes mientras el stack IP retransmite |
| EEPROM | `commit` en alta de códigos (independiente de cobertura, pero suma latencia percibida) |

Regla de producto (ya en plan v5): **relés / Wiegand / códigos locales no dependen de red**.

### Hipótesis priorizadas

| Pri | Hipótesis | Mitigación prevista |
|-----|-----------|---------------------|
| S1 | `gsmModemLoop` / PPP hace trabajo largo en el mismo `loop` que Wiegand | Acotar trabajo por tick; no `CRESET` automático en CSQ bajo; backoff |
| S2 | MQTT reconnect/publish bloquea decenas–cientos de ms en 4G malo | Timeout corto; no reintentar en ráfaga; acceso local **antes** de cualquier red |
| S3 | Validación “primero remoto” con broker inalcanzable → espera perceptible | Preferir local-first en campo; o timeout duro + fallback local si el código existe |
| S4 | LCD/RTC/I2C recovery (60 s) o render compitiendo | Ya hay `i2c_guard`; verificar que no bloquee en error de bus |
| S5 | UI web “cuelga” al guardar con MQTT/GSM ocupados | `yield` / responder HTTP antes de operaciones largas |

### Plan de medición

1. Definir operaciones a cronometrar (serial timestamps):
   - PIN local OK → relé ON  
   - Apertura web `/` y `/codes`  
   - Alta de código (web)  
   - Cambio página GSM / status  
2. Escenarios de red:
   - ETH OK, 4G off  
   - Solo 4G CSQ bueno (≥20)  
   - Solo 4G CSQ malo (≤7) o sin registro  
   - MQTT broker inalcanzable  
3. Objetivo acceso local: **p95 &lt; 300 ms** desde fin de trama Wiegand hasta GPIO relé HIGH, en todos los escenarios.

### Cambios previstos (orientativos)

1. Auditoría de `loop()`: orden `processWiegand` / `digitalInputs` **antes** que `gsm`/`mqtt` pesado.
2. GSM: en CSQ bajo no disparar rescan/CRESET periódico; alargar periodo de poll; logs rate-limited.
3. MQTT: backoff exponencial; `publish` fallido no debe `delay()`; revisar `delay(1000)` residuales en `main.ino` ligados a red.
4. Documentar configuración recomendada: `localValidationFirst = true` en instalaciones 4G.
5. (Opcional) métrica `loop_max_ms` en status para campo.

### Ficheros a tocar (candidatos)

- `src/main.ino` — orden loop, delays, mqtt reconnect  
- `src/gsm_modem.cpp` — política rescan / timeouts en baja cobertura  
- `src/net_manager.*` / `wifi_manager.*` — si el failover dispara trabajo extra  
- `docs/v5.0.3_bugs/` — tabla de mediciones  

---

## Orden de trabajo recomendado

```text
1. Reproducir B1 en placa (matriz §B1) con log serial completo
2. Clasificar H1–H6 → parche mínimo + prueba cruzada
3. Medir B2 (acceso local + web) en CSQ bajo
4. Aplicar mitigaciones S* sin romper registro 4G
5. Compilar esp32dev / esp32dev_ble / esp32dev_s3 según target de prueba
6. Cerrar docs con evidencia y bump notas si se publica firmware
```

## Fuera de alcance

- Nuevas features DI / WiFi / PPP  
- Cambio de contrato MQTT salvo bugfix  
- Refactor grande del monolito  

## Estado

| Ítem | Estado |
|------|--------|
| Rama `v5.0.3_bugs` | Creada |
| Plan | Este documento |
| Fix B1 | Pendiente reproducción |
| Fix B2 | Pendiente medición |
| Push / release | Cuando el usuario lo indique |
