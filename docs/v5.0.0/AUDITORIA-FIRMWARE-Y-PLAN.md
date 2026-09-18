# Auditoría del firmware base y validación del plan v5.0.0

**Rama:** `v5.0.0` · **Baseline auditada:** `v4.1.0` (`src/main.ino`, 10.433 líneas)
**Fecha:** 18 Sep 2026
**Documentos relacionados:** [PLAN-DUAL-TARGET-WIFI-4G.md](PLAN-DUAL-TARGET-WIFI-4G.md), [MATRIZ-HARDWARE-A2-A2V3.md](MATRIZ-HARDWARE-A2-A2V3.md), [GUIA-PORTABLE-WIFI-GSM-RED.md](GUIA-PORTABLE-WIFI-GSM-RED.md)

---

## 1. Comprensión del producto (filosofía y funcionalidades)

Controladora de accesos sobre KinCony KC868-A2 (ESP32) para **hasta 2 puertas**:

| Bloque | Implementación actual |
|--------|----------------------|
| Lectores | 2 teclados/lectores **Wiegand** (GPIO 33/14 y 4/16) por interrupción + teclado RS485 legacy |
| Actuación | 2 relés (GPIO 15/2) con duración configurable y timeout |
| Validación | **Local primero** (códigos PIN/TAG en EEPROM, máx. 50) → **remota** vía MQTT (`swatidhome/command|events/<serial>`) con modo torno (pending request + timeout 5 s) |
| Códigos remotos | Caché local de validaciones remotas (máx. 40) con franjas horarias (`TimeSlot`, bitmask días) |
| Entradas digitales | DI1/DI2 (GPIO 36/39) → disparo de relé, modo Normal/Inverso |
| Seguridad | Bloqueo por intentos fallidos, bloqueo/desbloqueo remoto de acceso y teclados |
| Gestión | Web embebida (Basic auth), OTA (manual + auto-check + rollback), BLE (env `esp32dev_ble`: auth challenge-response + HKDF v4.1) |
| Red | Solo Ethernet LAN8720; AP WiFi "de emergencia" únicamente si falla ETH en el arranque |

**Filosofía de diseño (correcta y a preservar en v5):** la lógica de acceso local (Wiegand, relés, códigos en EEPROM) debe funcionar **sin red**; la red es transporte para validación remota, sincronización y gestión. El plan v5.0.0 formaliza esto ("relés/accesos no dependen de red"), pero la baseline la incumple en varios puntos (ver §3, A1–A3).

---

## 2. Validación del plan v5.0.0

### 2.1 Veredicto general

El plan es **sólido y se valida**: arquitectura de 3 capas (`net_manager` / `wifi_manager` / `gsm_modem`), feature flags por entorno, fases incrementales con criterios de hecho, política PIN estricta, y GSM datos (PPP) fuera del MVP. El orden de arranque propuesto (§7 del plan) corrige el principal defecto de la baseline (arranque bloqueado por red).

### 2.2 Correcciones y matizaciones al plan

| # | Documento | Corrección |
|---|-----------|------------|
| P1 | Plan §2 | El sketch no tiene "~5900+ líneas": son **10.433**. Refuerza el riesgo "monolito": la extracción a módulos `.cpp/.h` debería ser **prerequisito de Fase 1**, no opcional |
| P2 | Matriz (GSM A2v3) | GPIO9 se marca como "strapping S3": los strapping del ESP32-S3 son **GPIO0, 3, 45, 46**. GPIO9/10 no son strapping. Sí es relevante que en el **N16R8 (PSRAM octal) los GPIO 33–37 quedan reservados** — afecta a la re-asignación de pines Wiegand (los candidatos 4, 5, 6, 38 de la matriz siguen siendo válidos) |
| P3 | Plan Fase 1 | Existe ya un AP legacy (`setupAPMode()`, `main.ino:6104`): SSID `SWATID_CONFIG_*`, pass fija `12345678`, IP `192.168.4.1`, solo si ETH falla en boot. Al portar `wifi_manager` (AP `192.168.77.1`) hay que **eliminar/absorber este camino** — dos rutas de AP con IPs y contraseñas distintas es un riesgo de soporte en campo. Añadir tarea explícita 1.8: "retirar `setupAPMode()` y migrar su caso de uso a `wifi_manager` boot_window" |
| P4 | Plan Fase 1 | Añadir a la Fase 1 el **saneamiento previo de persistencia** (hallazgos C1/C2, §3): portar WiFi/GSM a NVS sobre una EEPROM con corrupción activa enmascarará regresiones |
| P5 | Plan §4 | La doble compatibilidad core 2.x (A2) / 3.x (S3) duplica los `#if` de `ETH.begin` ya existentes (`main.ino:9199-9213`). Valorar unificar A2 también en espressif32 6.8.1 / core 3.x (como A2-MODBUS) para mantener **una sola API de red**; el coste es re-validar BLE/OTA en A2, pero elimina la matriz de compatibilidad más peligrosa del proyecto |
| P6 | Plan pruebas | Añadir a las pruebas de Fase 1: **códigos Wiegand durante ventana de arranque** (hoy se pierden ~30 s, ver A1) y **caída de broker con ETH viva > 5 min** (hoy reinicia el equipo, ver A2) |

---

## 3. Hallazgos en el firmware base (robustez)

Orden: críticos → altos → seguridad → menores. Referencias sobre `src/main.ino`.

### C1 — CRÍTICO: `StoredRemoteCodes` excede la EEPROM → persistencia de códigos remotos rota en silencio

- `EEPROM_REMOTE_CODES_OFFSET = 1800` ([main.ino:134](../../src/main.ino:134)) y `sizeof(StoredRemoteCodes)` ≈ **2332 bytes** (4+4+2 + 40×58, alineado a 4) → fin en **4132 > 4096**.
- El propio diagnóstico de arranque lo detecta (imprime `❌ RemoteCodes excede EEPROM!`, [main.ino:9102](../../src/main.ino:9102)), pero el arranque continúa igualmente.
- `EEPROM.get/put` del core ESP32 hace **bounds-check y retorna sin leer/escribir** si `address + sizeof(T) > _size`: `loadStoredRemoteCodes()` ([main.ino](../../src/main.ino)) y `saveStoredRemoteCodes()` serían **no-op silenciosos** → los códigos remotos cacheados se pierden en cada reinicio.
- **Verificación inmediata:** revisar el log de boot de un equipo real buscando `RemoteCodes excede EEPROM`.
- **Agravante detectado en la implementación del fix:** en el build BLE existen además `DeviceKeyConfig` en offset **3200** y `BLEAuthConfig` en **3400** (hasta ~3883) — es decir, la zona de códigos remotos (1800–4130) también **solapa las claves BLE**. Entre 1800 y 3200 solo hay 1400 bytes: reducir `MAX_REMOTE_CODES` no permite mantener la capacidad de 40 (cabrían 23).
- **Corrección aplicada (Fase 0.5):** mover la persistencia de códigos remotos a **NVS** (`Preferences`, namespace `a2acc_rc`, convención de la rama v5): mantiene los 40 códigos, no toca los datos BLE de campo y libera la zona EEPROM 1800–3199 (queda reservada). Sin migración necesaria: la persistencia EEPROM nunca llegó a funcionar. Además, `static_assert` del mapa completo (Config/DI/StoredCodes/DeviceKey/BLEAuth/scratch) para que ningún solape vuelva a compilar.

### C2 — CRÍTICO: el test de EEPROM corrompe datos de producción en cada arranque

- [main.ino:9041-9042](../../src/main.ino:9041): `EEPROM.write(0, 0x55)` y `EEPROM.write(EEPROM_SIZE-1, 0xAA)` + `commit()`.
- La dirección **0 es `config.deviceName[0]`** ([main.ino:137](../../src/main.ino:137)): cada arranque persiste `0x55` ('U') como primer carácter del nombre del equipo, y `loadConfiguration()` ([main.ino:2479](../../src/main.ino:2479)) lo carga después ya corrupto (p. ej. `SWATID_…` → `UWATID_…`, afecta a hostname/mDNS/web).
- El byte `EEPROM_SIZE-1` (4095) cae en zona de `StoredRemoteCodes` (con el mapa corregido de C1).
- **Corrección:** hacer el test en direcciones reservadas (p. ej. definir un área de scratch de 4 bytes fuera de todas las estructuras) o hacer test no destructivo (leer→escribir→verificar→restaurar **sin** commit intermedio).

### A1 — ALTO: `setup()` bloquea hasta 30 s esperando Ethernet

- [main.ino:9219-9227](../../src/main.ino:9221): bucle `while (!ethConnected …30000)` con `delay(500)`. Las ISR Wiegand ya están armadas pero `loop()` no corre: los códigos presentados durante el arranque **no se procesan** (y el web/AP tampoco existe aún).
- Contradice la regla de campo del plan ("red no bloquea lógica local"). El orden de arranque §7 del plan lo corrige — mantenerlo como **criterio de aceptación de Fase 1**: arranque a `loop()` en < 3 s, red en background.

### A2 — ALTO: reinicio del MCU si MQTT lleva 5 min caído (con ETH viva)

- [main.ino:9461-9469](../../src/main.ino:9464): `ESP.restart()` si `!mqttClient.connected() && ethConnected` durante > 5 min.
- Una parada de broker de 1 h ⇒ **~12 reinicios** de una controladora de accesos que funcionaría perfectamente en local. Cada reinicio además re-bloquea el arranque (A1).
- **Corrección:** eliminar el restart (o restringirlo a fallo real de pila TCP detectado, no a broker inalcanzable). Con `net_manager` en v5, la condición "sin broker" es un estado normal de operación, no un fallo del MCU.

### A3 — ALTO: conexión MQTT bloqueante en `loop()` (hasta 5 s por intento)

- `connectToMqtt()` ([main.ino:747](../../src/main.ino:747)) usa `mqttClient.connect()` síncrono con `MQTT_CONNECT_TIMEOUT_SEC = 5`. Con broker inalcanzable y backoff 5→30 s, el `loop()` se congela hasta 5 s por intento: pulsaciones de teclado, DI y relés se procesan con esa latencia (el buffer Wiegand por ISR amortigua, pero el timeout de relé y el modo torno sufren jitter).
- **Corrección v5:** integrar el patrón `mqttEnsureTransport()` + `netHasConnectivity()` de la guía y valorar reducir el socket timeout a 2 s; ideal a medio plazo: cliente MQTT asíncrono o conexión en tarea FreeRTOS separada del core de accesos.

### S1 — SEGURIDAD: credenciales embebidas en el fuente (y en git)

- [main.ino:74-79](../../src/main.ino:74): IP del broker, usuario y contraseña MQTT (`Swatid2025!`) y `admin/admin` web en claro en el repositorio.
- AP de emergencia con pass fija `12345678` ([main.ino:6108](../../src/main.ino:6108)).
- **Corrección propuesta (encaja en Fase 1):** mover broker/credenciales a NVS con provisión por AP/BLE; contraseña de AP **derivada por equipo** (p. ej. del serial fijo) impresa en etiqueta; forzar cambio de `admin` en primer login. Mínimo si no da tiempo: rotar la credencial MQTT actual, que ya debe considerarse comprometida al estar en el histórico de git.

### S2 — SEGURIDAD: Basic auth sobre HTTP plano

Toda la web (incl. cambio de contraseña e importación de códigos) viaja en claro. En ETH de instalación es riesgo acotado; **con WiFi STA/AP en v5 la superficie crece** (radio). Mitigación razonable para MVP: documentarlo, restringir gestión sensible al AP directo, y valorar HTTPS o al menos digest en v5.1.

### M1 — MENOR: incoherencia de versiones

- Banner de arranque dice `v4.0.0 (BLE)` / `v3.0.2` ([main.ino:9008-9011](../../src/main.ino:9008)) mientras `firmwareVersion` es `v4.1.0` / `v3.0.2` ([main.ino:91-97](../../src/main.ino:91)). Unificar en una única constante usada en banner, web, OTA y MQTT (importante para el auto-update OTA que compara versiones).

### M2 — MENOR: `DynamicJsonDocument doc(1024)` en `mqttCallback`

- [main.ino:818](../../src/main.ino:818): el buffer MQTT es 2048 pero el JSON se parsea con 1024; un `sync` de códigos remotos con 4 franjas horarias puede desbordar (deserialización falla y el mensaje se descarta con solo un log). Subir a 2048 o filtrar por tamaño con aviso MQTT.

### M3 — MENOR: dependencia CDN en la web embebida

- [main.ino:6212](../../src/main.ino:6212): Font Awesome desde cdnjs. En modo AP/offline (el caso de uso principal del AP) los iconos no cargan y la página tarda por el timeout del fetch. Eliminar o incrustar los 4–5 iconos usados como SVG inline.

---

## 4. Propuestas de mejora para la rama v5.0.0

### 4.1 Insertar una "Fase 0.5 — Saneamiento de base" antes de portar red

| # | Tarea | Origen |
|---|-------|--------|
| 0.5.1 | Corregir mapa EEPROM (C1) + `static_assert` de layout | C1 |
| 0.5.2 | Test EEPROM no destructivo | C2 |
| 0.5.3 | Eliminar espera bloqueante de ETH en `setup()` (adelanto del orden §7 del plan) | A1 |
| 0.5.4 | Eliminar `ESP.restart()` por MQTT caído | A2 |
| 0.5.5 | Unificar constante de versión | M1 |
| 0.5.6 | `DynamicJsonDocument` 1024→2048 en callback | M2 |

Son cambios pequeños, testeables en `esp32dev` actual, y dejan la base limpia para el diff grande de Fase 1. Publicables como `v4.1.1` de mantenimiento si se quiere desacoplar del ciclo v5.

### 4.2 Refuerzos al plan de red (Fases 1–3)

1. **Retirar `setupAPMode()`** al introducir `wifi_manager` (P3) y documentar la migración para instaladores (cambia SSID, IP y contraseña del AP).
2. **Provisión de credenciales MQTT vía AP/NVS** (S1) — encaja de forma natural en el bloque `/system` del `wifi_server` que ya se va a portar.
3. **Criterio de aceptación adicional Fase 1:** tiempo de `setup()` → primer ciclo de `loop()` **< 3 s** con y sin cable; código Wiegand presentado a los 5 s del encendido debe abrir puerta con validación local.
4. **Criterio adicional failover:** broker caído 30 min con ETH viva → cero reinicios, accesos locales OK, reconexión sola al volver el broker.
5. **Watchdog real:** en vez del restart por MQTT (A2), habilitar `esp_task_wdt` sobre el `loop()` — reinicia solo si la lógica de accesos deja de ejecutar, que es el fallo que sí justifica un reboot.
6. **A2v3 / S3:** fijar en laboratorio la re-asignación Wiegand2 (conflicto GPIO16/17 con DI, ya recogido en la matriz) **antes** de encargar preseries; añadir a la matriz el resultado como tabla definitiva de pines v5.

### 4.3 Deuda estructural (durante Fase 1, no después)

Extraer del monolito, como mínimo, en ficheros propios: `eeprom_layout.h` (offsets + static_asserts), `mqtt_service.*`, `web_portal.*`, `access_logic.*`. La portación de `net_manager`/`wifi_manager` ya obliga a tocar todos esos puntos; hacerlo sin trocear duplicará el riesgo de regresión en las 10.433 líneas actuales.

---

## 5. Resumen ejecutivo

- **El plan v5.0.0 es correcto** en arquitectura, fases y alcance del MVP; se proponen 6 matizaciones (P1–P6), ninguna estructural.
- **Dos bugs críticos de persistencia** en la baseline (C1: códigos remotos fuera de EEPROM con fallo silencioso; C2: test EEPROM que corrompe el nombre del equipo) deben corregirse **antes** de portar la capa de red — se propone una Fase 0.5 de saneamiento, publicable como v4.1.1.
- **Tres debilidades de robustez de campo** (arranque bloqueado 30 s, reboot por broker caído, MQTT bloqueante) que la propia arquitectura v5 resuelve si se aplican como criterios de aceptación medibles.
- **Seguridad:** rotar la credencial MQTT del repositorio y planificar provisión por NVS/AP en Fase 1.

---

*Auditoría v1 — 2026-09-18 — rama v5.0.0*
