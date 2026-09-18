# Fase 1 — Gestión WiFi (AP/STA) + GSM Fase 1 (implementada)

**Rama:** `v5.0.0` · **Fecha:** 18 Sep 2026
**Módulos:** [wifi_manager](../../src/wifi_manager.cpp) · [gsm_modem](../../src/gsm_modem.cpp) · integración en [main.ino](../../src/main.ino)
**Compilación verificada:** `esp32dev` 92,4 % · `esp32dev_4g` 92,9 % · `esp32dev_ble` 76,2 %

---

## 1. WiFi — comportamiento

### AP de gestión

| Parámetro | Valor por defecto | Configurable |
|-----------|-------------------|--------------|
| SSID | **id del equipo** (serial fijo, p. ej. `SWATID_XXXXXXXX`) | No (identidad del equipo) |
| Contraseña | **`admin1234`** | Web y MQTT (mín. 8 caracteres) |
| Modo | `boot_window` | `disabled` / `boot_window` / `always_on` |
| Ventana | **60 s** al arrancar | 10–3600 s |
| IP del AP | 192.168.4.1 | — |

**Ventana de arranque:** el AP se levanta al arrancar; la cuenta atrás de 60 s
**se rearma mientras haya algún cliente conectado**. Sin clientes, al expirar
se apaga solo. Puede reactivarse en caliente desde la web (`Activar AP ahora`)
o por MQTT (`ap_start`), útil para mantenimiento sin reiniciar.

mDNS activo con el AP: `http://<nombre-equipo>.local`.

### STA (cliente WiFi)

- Credenciales persistidas en NVS (`a2acc_wifi`), nunca expuestas en JSON.
- `WiFi.setSleep(false)` (entorno industrial) y autoreconexión + reintento de
  fondo cada 30 s como red de seguridad.
- Escaneo de redes **asíncrono** con caché de resultados (no bloquea accesos).

### Prioridad y robustez

- `net_manager` agrega el estado: **ETH > WiFi STA** para MQTT; al cambiar la
  interfaz preferida (failover o failback) el loop fuerza reconexión MQTT
  automáticamente (cableado desde Fase 0.75).
- La caída de uno o todos los interfaces **no bloquea nada**: Wiegand, relés,
  DI y códigos locales corren igual; MQTT espera `netHasConnectivity()`.
- ⚠️ **Limitación conocida (core Arduino 2.x):** con ETH y STA levantadas *a la
  vez*, lwIP puede enrutar el tráfico saliente por STA (prioridad de netif del
  core). `net_manager` reporta ETH como preferida y todo funciona, pero la
  prioridad estricta de salida con ambas activas se completará en el target S3
  (core 3.x, `ETH.setDefault()`, Fase 2). En el caso operativo normal
  (failover: solo una interfaz viva) la prioridad es correcta.

## 2. WiFi — gestión web

- **`/wifi`** (enlazada desde la página de inicio): estado en vivo (ETH, STA,
  AP con clientes y segundos restantes, interfaz MQTT), escaneo con "Usar",
  conexión/olvido de red STA, modo/ventana del AP, activación y apagado
  inmediatos, cambio de contraseña del AP.
- APIs (Basic auth): `GET /api/wifi/status` · `GET /api/wifi/scan[?start=1]`
  · `POST /wifi` (`action=save_ap|set_ap_pass|ap_on|ap_off|connect_sta|forget_sta`).
- La página usa el CSS común de `web_common` (sin `<style>` propio, regla de
  la Fase 0.75).

## 3. WiFi/GSM — gestión MQTT (`message_type: 7`)

Envelope estándar (topic `swatidhome/command/<serial>/...`), respuestas por
`swatidhome/response/<serial>/rx` vía `publishResponse`:

```json
{ "device": "<serial>", "message_id": 123, "message_type": 7,
  "message_info": { "action": "get_wifi" } }
```

| `action` | `message_info` adicional | Efecto |
|----------|--------------------------|--------|
| `get_wifi` | — | Publica el JSON de estado (sin passwords) |
| `set_wifi` | `ap_mode` (0/1/2), `ap_timeout_s`, `ap_pass`, `sta_ssid`+`sta_pass`, `sta_enabled`, `forget_sta` | Configura y aplica en caliente; persiste en NVS |
| `ap_start` | `seconds` (0 = ventana configurada) | Activa el AP ya |
| `ap_stop` | — | Apaga el AP |
| `wifi_scan` | — | Lanza escaneo async |
| `get_wifi_scan` | — | Publica resultados del último escaneo |
| `get_gsm` | — | Publica estado GSM (`pin_set`, nunca el PIN) |
| `set_gsm` | `enabled`, `pin` (solo escritura), `apn`+`apn_mode`+`apn_user`+`apn_pass` | Configura GSM |
| `gsm_rescan` | — | Re-probe del módulo (hot-plug) |

## 4. GSM — Fase 1 (estado; sin datos PPP)

FSM AT no bloqueante según la guía portable §5:

```text
Probing ─AT OK→ SimCheck ─READY→ Registering ─CEREG/CREG 1|5→ Attached
   │                │                                        (poll CSQ/COPS 10 s)
   ▼                ├─ PinRequired / PinError / PukRequired / NoSim
 Absent (reposo; rescan manual)
```

- **Política PIN estricta:** `AT+CPIN=` **una vez** por arranque y por valor;
  rechazo → `pin_error` sin reintentos (riesgo PUK). El PIN es de solo
  escritura (web/MQTT lo setean, nunca se lee).
- **UART:** `UART1` con pines remapeados — la **UART2 la ocupa el RS485** del
  teclado (difiere de la guía MODBUS, documentado en el código).

| Target | TX | RX | Swap auto |
|--------|----|----|-----------|
| A2 clásico (socket 4G) | GPIO13 | GPIO34 | **No** (GPIO34 es solo entrada) |
| A2v3 (ESP32-S3) | GPIO10 | GPIO9 | Sí (mapeo aprendido → NVS `pin_swap`) |

- Config NVS `a2acc_gsm`: `en`, `pin_swap`, `pin`, `apn_mode`, `apn`,
  `apn_user`, `apn_pass`, `data_mode` (reservado Fase 4).
- **Página `/gsm` dedicada** (enlazada desde inicio en targets con GSM):
  estado en vivo (FSM con etiquetas, módulo, IMEI, operador, cobertura con
  calidad y dBm), re-detección, config APN (auto/manual, el password nunca se
  relee), y gestión del PIN con aviso de intento único (escribir/borrar; nunca
  se muestra).
- APIs web: `GET /api/gsm/status` · `POST /api/gsm/rescan` · `POST /gsm`.
- Sin módulo: `absent` tras 6 intentos (~3 s), sin spam de UART, cero impacto
  en el resto del firmware.

### Entornos

| Env | Flags | Uso |
|-----|-------|-----|
| `esp32dev` | WIFI=1, GSM=0 | A2 producción |
| `esp32dev_4g` *(nuevo)* | WIFI=1, GSM=1 | A2 clásico con SIM7600 en el socket — permite validar toda la FSM GSM en hardware actual |
| `esp32dev_ble` | WIFI=1, GSM=0, BLE | A2 + BLE |
| `esp32dev_s3` | *(pendiente Fase 2)* | A2v3: mismo código GSM con pines S3 (`A2_BOARD_A2V3=1`); falta board/core 3.x + W5500 |

## 5. Presupuesto flash

| Env | Flash | Libre |
|-----|-------|-------|
| `esp32dev` | 1.210.609 B (92,4 %) | 100 KB |
| `esp32dev_4g` | 1.217.677 B (92,9 %) | 93 KB |
| `esp32dev_ble` | 1.498.361 B (76,2 %) | — |

El coste del WiFi completo fue ~16 KB (dentro de lo estimado en Fase 0.75
gracias a la reformulación HTML previa).

## 6. Plan de pruebas en hardware

| # | Prueba | Esperado |
|---|--------|----------|
| 1 | Arrancar equipo | AP `SWATID_…`/`admin1234` visible; se apaga solo a los 60 s sin clientes |
| 2 | Conectarse al AP antes de 60 s | El AP **no** se apaga mientras el móvil siga conectado; al desconectar, se apaga a los 60 s |
| 3 | `/wifi` desde el AP (192.168.4.1) | Estado en vivo; escaneo lista redes; conectar STA guarda y conecta |
| 4 | STA configurada + reinicio | Conecta sola; web accesible por IP STA |
| 5 | ETH + STA, quitar cable ETH | MQTT reconecta por WiFi (log "cambio interfaz"); accesos locales sin cortes |
| 6 | Reponer cable ETH | Failback: MQTT reconecta; `mqtt_iface=eth` en `/api/wifi/status` |
| 7 | Sin ETH ni STA | Accesos locales OK; AP reactivable por ventana de arranque (reinicio) |
| 8 | MQTT `set_wifi` (ap siempre activo) y `ap_stop` | Cambios en caliente, persistentes tras reinicio |
| 9 | `esp32dev_4g` sin módulo SIM | `absent` en <5 s, arranque normal |
| 10 | `esp32dev_4g` con SIM7600 + SIM sin PIN | `attached`, IMEI/CSQ/operador en `/api/gsm/status` |
| 11 | SIM con PIN correcto configurado | Un intento, `attached` |
| 12 | SIM con PIN erróneo | `pin_error`, **sin** reintentos (contador PIN de la SIM baja solo 1) |
| 13 | Escaneo WiFi durante paso de tarjetas | Cero pérdida de lecturas Wiegand |

---

*Fase 1 implementada — pendiente validación en hardware. GSM datos (PPP/MQTT backup) queda para Fase 4 / v5.1 según plan.*
