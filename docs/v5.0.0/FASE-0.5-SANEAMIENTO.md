# Fase 0.5 — Saneamiento de la base (implementada)

**Rama:** `v5.0.0` · **Fecha:** 18 Sep 2026
**Origen:** hallazgos C1, C2, A1, A2, M1, M2 de la [auditoría](AUDITORIA-FIRMWARE-Y-PLAN.md)
**Versiones resultantes:** `v4.1.1-BLE` (`esp32dev_ble`) · `v3.0.3-EEPROM` (`esp32dev`)
**Compilación verificada:** `esp32dev` (flash 92,3 %) y `esp32dev_ble` (flash 76,3 %)

---

## 1. Cambios implementados (`src/main.ino`)

### C1 — Códigos remotos: de EEPROM (rota) a NVS

**Problema real (más grave que el auditado):** `StoredRemoteCodes` (~2330 bytes) en offset 1800 terminaba en 4130 (> 4096) **y además solapaba** `DeviceKeyConfig` (3200) y `BLEAuthConfig` (3400) del build BLE. `EEPROM.get/put` fuera de rango es no-op silencioso → los códigos remotos **nunca se persistieron**; solo existían en RAM hasta el reinicio.

**Solución:** persistencia en **NVS** vía `Preferences`:

| Elemento | Valor |
|----------|-------|
| Namespace | `a2acc_rc` (convención `a2acc_*` de la rama v5) |
| Clave | `codes` (blob de `sizeof(StoredRemoteCodes)`) |
| Capacidad | **40 códigos** (sin cambio de contrato con el servidor) |
| Migración | No necesaria (nunca hubo datos persistidos en EEPROM) |
| Validación en carga | Tamaño de blob + `validMarker` + `version` + `count ≤ MAX` → si falla, reinicialización limpia |
| Fallo de escritura | Log explícito `❌ Error guardando códigos remotos en NVS` (antes: silencio) |

La zona EEPROM **1800–3199 queda reservada** (documentada en el código); no reutilizar sin revisar los offsets BLE.

### C1b — Mapa EEPROM verificado en compilación

`static_assert` sobre el mapa completo — cualquier solape futuro **no compila**:

```text
Config          0    → ≤ 256
DigitalInput    256  → ≤ 512
StoredCodes     512  → ≤ 1800
(reservado)     1800 → 3199
DeviceKey (BLE) 3200 → ≤ 3400
BLEAuth  (BLE)  3400 → ≤ 4090
Scratch test    4090 → 4092
Total           4096 (EEPROM_TOTAL_SIZE)
```

### C2 — Test de EEPROM no destructivo

- Antes: escribía `0x55` en la dirección **0** (primer byte de `config.deviceName` → nombre corrupto a `U…` en cada arranque) y `0xAA` en 4095.
- Ahora: test sobre la **zona scratch reservada** (`EEPROM_TEST_SCRATCH_OFFSET` = 4090, fuera de toda estructura), con **restauración del contenido original** tras el test.
- Eliminado el bucle de "probar tamaños decrecientes" (4096→512): un tamaño < 4096 rompería todo el mapa en silencio. Ahora `EEPROM.begin(4096)` fijo; si falla, error crítico en log y el equipo sigue arrancando (los accesos en RAM siguen operativos).

### A1 — Arranque no bloqueante (antes: hasta 30 s parado esperando ETH)

- `setup()` ya **no espera** a Ethernet: arranca el servidor web inmediatamente (escucha en todas las interfaces) y entra en `loop()` en cuanto termina la inicialización local.
- La decisión de AP de emergencia se **difiere al `loop()`**: si a los 30 s (`AP_FALLBACK_TIMEOUT_MS`) no hay ETH, se levanta el AP (`setupAPMode()`, que ya no re-arranca el web server y marca `apModeActive`).
- Efecto: códigos Wiegand, DI y relés operativos **desde el primer segundo**, con o sin cable.
- Nota: si ETH llega después de levantar el AP, el AP permanece hasta el reinicio (comportamiento aceptado en 0.5; el ciclo de vida completo del AP lo gestionará `wifi_manager` en Fase 1).

### A2 — Sin reinicio del MCU por broker MQTT caído

- Eliminado el `ESP.restart()` tras 5 min de MQTT desconectado con ETH viva (una parada de broker de 1 h provocaba ~12 reinicios de la controladora).
- Ahora: aviso periódico en log (`⚠️ MQTT sin conexión desde hace N min`) y la reconexión con backoff sigue trabajando. Los accesos locales no se interrumpen.

### M1 — Versión unificada

- El banner de arranque imprimía `v4.0.0 (BLE)` con `firmwareVersion = v4.1.0`. Ahora el banner usa `firmwareFullVersion` (única fuente, también para OTA/web/MQTT).
- Bump de mantenimiento: **4.1.0 → 4.1.1** (BLE) y **3.0.2 → 3.0.3** (sin BLE).

### M2 — Parser JSON del callback MQTT

- `DynamicJsonDocument` de 1024 → **2048** (igual al buffer de PubSubClient): un `sync` de códigos remotos con 4 franjas horarias podía descartarse en silencio.

---

## 2. Qué NO cambia en esta fase

- `setupAPMode()` se mantiene (SSID `SWATID_CONFIG_*`, pass `12345678`, IP 192.168.4.1): su retirada/absorción corresponde a `wifi_manager` en **Fase 1** (tarea P3 de la auditoría).
- Conexión MQTT síncrona (hasta 5 s por intento, hallazgo A3): se aborda con `net_manager` en Fase 1.
- Credenciales embebidas (S1): la provisión por NVS/AP va en Fase 1; **pendiente rotar la credencial MQTT** del histórico git (acción de backend, no de firmware).

---

## 3. Plan de pruebas en equipo real

| # | Prueba | Resultado esperado |
|---|--------|--------------------|
| 1 | Boot con cable ETH | Log `EEPROM configurada: 4096 bytes ✅`; mapa sin errores; IP por evento; web accesible |
| 2 | Boot **sin** cable + código local a los ~5 s | Relé abre (validación local) **antes** de cualquier timeout de red |
| 3 | Boot sin cable, esperar 30 s | AP `SWATID_CONFIG_*` visible; web en 192.168.4.1; log `MODO AUTÓNOMO` |
| 4 | Nombre del equipo | Ya **no** aparece con `U` inicial espuria; si un equipo quedó con nombre corrupto de versiones previas, renombrarlo una vez desde la web |
| 5 | Sync de códigos remotos por MQTT → reiniciar | Los códigos **sobreviven al reinicio** (log `Códigos remotos guardados en NVS: N`) — primera vez que esto funciona |
| 6 | Parar el broker 10 min con ETH viva | Cero reinicios; aviso periódico en log; reconexión sola al volver el broker |
| 7 | OTA desde v4.1.0-BLE a v4.1.1-BLE | Claves BLE y códigos locales intactos (offsets EEPROM sin cambio) |
| 8 | Regresión | Wiegand 1/2, DI1/DI2, modo torno, BLE (env BLE) y OTA operativos |

---

## 4. Registro de decisiones

| Decisión | Alternativas descartadas | Motivo |
|----------|--------------------------|--------|
| Códigos remotos → NVS | Reducir a 23 códigos (cabría en 1800–3200); re-empaquetar structs (30 códigos máx.) | Mantener capacidad 40 del contrato MQTT; cero riesgo para datos BLE; alineado con la dirección NVS de v5 |
| `EEPROM.begin(4096)` fijo | Mantener bucle de tamaños decrecientes | Un tamaño menor rompía silenciosamente toda la persistencia; mejor fallo explícito |
| AP diferido en `loop()` | Mantener espera bloqueante reducida | Regla v5: la red no bloquea la lógica local |
| Versiones 4.1.1 / 3.0.3 | Saltar a 5.0.0-dev | v5.0.0 se reserva para el dual-target WiFi/4G; esto es mantenimiento de la base |

---

*Fase 0.5 completada a nivel de código — pendiente validación en hardware (plan §3).*
