# Verificación SIM / 4G — plan de trabajo v5.0.1

**Síntoma reportado:** no se detecta la SIM (o la UI/serial no refleja presencia útil del módem).  
**Módulo:** `src/gsm_modem.cpp` (FSM AT, NVS `a2acc_gsm`).  
**Fecha plan:** 18 Sep 2026.

---

## 1. Qué mirar primero en campo

1. Serial 115200: líneas `📡 [GSM] …` al arranque.
2. Web `/gsm` o `GET /api/gsm/status` → campos `state`, `model`, `imei`, `pin_set`, `pin_swap`.
3. Confirmar target compilado: **A2** (`esp32dev_4g`) vs **A2v3** (`esp32dev_s3`).

| Si `state` es… | Acción |
|----------------|--------|
| `disabled` | Activar módem en config / NVS `en=1` |
| `absent` | Problema **UART / alimentación / PWRKEY / pines**, no SIM |
| `probing` largo | Aumentar espera post-boot; revisar baud 115200 |
| `no_sim` | Módulo OK; SIM mal insertada, contacto, o respuesta CPIN no parseada |
| `pin_required` | SIM **sí** detectada → configurar PIN (un intento) |
| `registering` | SIM OK; antena / APN / cobertura |

---

## 2. Hipótesis (priorizadas)

### H1 — Tiempo insuficiente tras encendido del SIM7600E

El módulo puede tardar **10–15 s** en aceptar AT. Hoy: `GSM_PROBE_ROUNDS=6` × tick 500 ms + timeout 1 s ≈ pocos segundos → riesgo de `absent` falso si el probe arranca demasiado pronto.

**Fix previsto:** delay inicial / más rondas / timeout de probe más largo solo en fase Probing (sin bloquear Modbus: ticks en `loop`).

### H2 — Respuesta `AT+CPIN?` no parseada → `no_sim` falso

Hoy cualquier respuesta que no contenga `READY` / `SIM PIN` / `SIM PUK` cae en `GSM_NO_SIM`. Respuestas con ruido, `+CME ERROR: 10` (SIM not inserted) vs otros CME, o multilínea incompleta, se clasifican mal.

**Fix previsto:**

- Log de buffer crudo en fallo CPIN.
- Mapear `+CME ERROR: 10` / `SIM not inserted` → `no_sim` explícito.
- Reintentar `AT+CPIN?` 2–3 veces tras IMEI (SIM a veces tarda más que el módem).

### H3 — Pines / UART incorrectos en el target bajo prueba

| Target | TX | RX | UART | Swap |
|--------|----|----|------|------|
| A2v3 | 10 | 9 | 2 | auto |
| A2 | 13 | 34 | 1 | no |

Si se flashea S3 con flags A2 (o al revés), probe falla → `absent`.

### H4 — PIN / SIM bloqueada

`pin_required` / `pin_error` / `puk_required` **no** son “SIM no detectada”; documentar en UI el texto exacto.

### H5 — Alimentación / antena / SIM física

Fuera de firmware: SIM 3V, asiento, antena LTE. Si IMEI aparece y CPIN=READY pero no registra → cobertura/APN (fuera del síntoma “no SIM”).

---

## 3. Cambios de código previstos (orden)

1. **Instrumentación:** en cada transición CPIN y probe timeout, imprimir `s_rxBuf` (truncado) y CME si existe.
2. **Espera de arranque:** `GSM_BOOT_GRACE_MS` (p. ej. 8–12 s) antes del primer `AT`, o ampliar `GSM_PROBE_ROUNDS`.
3. **Reintentos CPIN?:** 3 intentos con 1–2 s entre ellos antes de `no_sim`.
4. **API status:** campo `last_at` / `last_error` (sin exponer PIN).
5. **Docs de prueba:** tabla de resultados en esta carpeta tras ensayo en placa.

No tocar política PIN (un intento, sin auto-retry, PIN nunca en JSON).

---

## 4. Checklist de prueba en placa

### Precondiciones

- [ ] Antena 4G conectada
- [ ] SIM conocida (con/sin PIN documentado)
- [ ] Alimentación 12/24 V estable
- [ ] Env correcto (`esp32dev_4g` o `esp32dev_s3`)

### Casos

| # | Condición | Esperado |
|---|-----------|----------|
| 1 | Sin módulo (socket vacío) | `absent` &lt; 30 s |
| 2 | Módulo sin SIM | `model`/`imei` opcionales; `no_sim` |
| 3 | SIM sin PIN | `registering` → `attached` si hay red |
| 4 | SIM con PIN, NVS vacío | `pin_required` |
| 5 | SIM con PIN correcto (un set) | sale de PIN → registro |
| 6 | PIN incorrecto | `pin_error`, sin segundo intento |
| 7 | `POST /api/gsm/rescan` | re-probe sin reboot |

### Evidencia a capturar

- Fragmento serial del probe + CPIN
- JSON `/api/gsm/status`
- Target (A2 / A2v3) y versión firmware impresa al boot

---

## 5. Referencia rápida de comandos AT (SIM7600E)

| Comando | Uso en esta rama |
|---------|------------------|
| `AT` | Probe |
| `ATI` / `AT+GSN` | Modelo / IMEI |
| `AT+CPIN?` | Estado SIM |
| `AT+CPIN=` | Un intento si hay PIN en NVS |
| `AT+CEREG?` / `AT+CREG?` | Registro |
| `AT+CSQ` | Cobertura (poll ~10 s si attached) |

---

## 6. Resultados en placa A2v3 (18 Sep 2026)

### Cambios de firmware implementados (todos los de §3, y dos extra)

| # | Cambio | Detalle |
|---|--------|---------|
| 1 | Instrumentación | `AT+CMEE=2` (errores CME en texto), log del buffer crudo en cada fallo CPIN/probe, contador de rondas de probe |
| 2 | Gracia de arranque | `GSM_BOOT_GRACE_MS=8 s` antes del primer `AT` + `GSM_PROBE_ROUNDS` 6→12 (H1) |
| 3 | Reintentos CPIN | 8 intentos × 2 s antes de declarar `no_sim` (H2) |
| 4 | API/UI | Campo `last_error` en `/api/gsm/status` y fila "Último error AT" en `/gsm` |
| 5 | *(extra)* Chequeo CFUN | `AT+CFUN?` antes de CPIN; si no está en modo completo, `AT+CFUN=1` (timeout 10 s) y margen de 3 s |
| 6 | *(extra)* Rescan real | El rescan ahora envía **`AT+CRESET`** (reinicio del módulo, ~15 s): el SIM7600 se alimenta del socket y NO se reinicia con el ESP32, así que una SIM insertada en caliente solo se relee reiniciando el módulo |
| 7 | *(extra)* Modelo correcto | `AT+CGMM` en vez de `ATI` (que devolvía la línea de fabricante) |

### Evidencia capturada (serial + `/api/gsm/status` vía Ethernet)

```text
📡 [GSM] Módulo detectado
📡 [GSM] CPIN? respuesta cruda:  | +CME ERROR: SIM not inserted |
📡 [GSM] CPIN sin resultado (+CME ERROR: SIM not inserted) - reintento 1/8 en 2000 ms
… (8 intentos) …
📡 [GSM] SIM no disponible tras 8 intentos (último error: +CME ERROR: SIM not inserted)

STATUS: {"state":"no_sim","model":"…SIMCOM…","imei":"862499071607106",
         "last_error":"+CME ERROR: SIM not inserted", …}

>>> POST /api/gsm/rescan  (ciclo completo verificado)
📡 [GSM] Rescan: reiniciando módulo (AT+CRESET, ~15 s)…
📡 [GSM] Módulo detectado          ← módulo re-arrancado OK
📡 [GSM] CPIN? … SIM not inserted  ← persiste tras reinicio COMPLETO del módulo
```

### Conclusión

- **H1 y H2 quedan implementadas pero descartadas como causa**: el error es
  consistente, con texto CME claro, y persiste tras reiniciar el módulo entero.
- **Causa: física (H5) — el slot no detecta la tarjeta.** El módulo está
  perfecto (IMEI, AT estables); el porta-SIM no cierra el contacto de presencia.
- Acción de campo: **apagar por completo el equipo**, extraer la SIM y
  verificar orientación (esquina biselada), tamaño correcto para el slot del
  A2v3 (adaptadores nano→micro suelen fallar el switch de presencia), limpiar
  contactos y reinsertar hasta el tope/clic. Al encender —o con el botón
  "Reiniciar módulo y re-detectar SIM" de `/gsm`— debe pasar a
  `registering`/`attached` (o `pin_required` si la SIM tiene PIN).

## 7. Segunda iteración: SIM detectada → `pin_required` (H4)

Tras re-asentar la SIM, el slot ya la detecta. Evidencia capturada:

```text
📡 [GSM] CPIN? respuesta cruda: AT+CPIN? | +CPIN: SIM PIN |  | OK |
📡 [GSM] Intentos restantes (PIN1,PIN2,PUK1,PUK2): 3,10,3,10
📡 [GSM] sim_check → pin_required
```

**La SIM M2M SÍ tiene PIN activado** (respuesta literal del módem, no es un
falso positivo del parser). Habitual en M2M: el operador entrega la tarjeta
con PIN por defecto o definido en su portal. Los **3 intentos de PIN1 están
intactos** (`AT+SPIC` ahora visible en `/gsm` y en el JSON como
`pin_attempts`).

### Mejoras añadidas en esta iteración

| Mejora | Detalle |
|--------|---------|
| Log crudo también en camino PIN/PUK | Antes solo se instrumentaba el camino de error |
| `AT+SPIC` → `pin_attempts` | Intentos restantes visibles en `/gsm` y API antes de arriesgar un intento |
| Reintento inmediato al guardar PIN | Antes `pin_required` era terminal hasta reinicio; ahora `gsmSetPin` re-entra en `sim_check` |
| **Quitar PIN permanentemente** | Nueva acción (web `/gsm` y MQTT `set_gsm.pin_disable`): desbloquea con `AT+CPIN=` y desactiva con `AT+CLCK="SC",0` — recomendado para M2M en campo; al completarse borra el PIN de NVS |

### Cómo resolverlo en campo (elegir una)

1. **Recomendado:** obtener el PIN (portal M2M del operador / soporte / tarjeta
   portadora) y usar en `/gsm` el botón **"Desbloquear y quitar PIN
   permanentemente"** — un solo intento, y la SIM queda sin PIN para siempre.
2. Alternativa: "Guardar PIN" — el equipo lo enviará en cada arranque
   (un intento por arranque y por valor).

⚠️ Con 3 intentos disponibles no hay riesgo inmediato, pero **no probar PINs
al azar**: cada fallo descuenta un intento y al tercero exige PUK.

## 8. Tercera iteración: PIN OK → `registering` sin avance (21 Sep 2026)

PIN `0987` aceptado (SIM desbloqueada; con `pin_set=true` el equipo lo enviará
solo en cada arranque en frío). El registro no avanzaba y no había visibilidad
del motivo. Instrumentación añadida y evidencia:

```text
📡 [GSM] CREG: stat=2 (buscando red)
📡 [GSM] Registrando… stat=2 (buscando red), CSQ=99
```

| Mejora | Detalle |
|--------|---------|
| `reg_stat` en API y `/gsm` | Código CEREG/CREG legible ("buscando red", "REGISTRO DENEGADO"…) con log en cada cambio + resumen cada 30 s |
| CSQ durante el registro | Antes solo se sondeaba en `attached`; ahora también registrando (distingue antena de aprovisionamiento) |
| **CSQ=99 visible** | Bug: 99 (sin señal) se mapeaba a -1 (no consultado); ahora se distingue y la web dice "Sin señal" |
| stat=3 → error explícito | "registro denegado por la red" en `last_error` (aprovisionamiento M2M) |
| Quitar PIN con SIM desbloqueada | El botón CLCK ahora también funciona en `registering`/`attached` (antes solo si pedía PIN) |

### Conclusión

**CSQ=99 mantenido: el módem no detecta ninguna señal de radio.** Con señal
cero el `stat=2` (buscando) no puede completarse nunca. Causa: **antena 4G
ausente/mal conectada** (conector MAIN del SIM7600E → SMA de la placa) o cero
cobertura en la ubicación. No es firmware, ni SIM, ni operador.

Acción de campo: conectar/revisar la antena LTE (cable u.FL→SMA bien clipado
en MAIN) y repetir. Con antena, en `/gsm` debe verse CSQ 5-31 y el paso a
`attached`; si con buena señal quedara `stat=3 REGISTRO DENEGADO`, entonces sí
sería aprovisionamiento del perfil M2M (operador).

Recomendación al registrar: usar "Desbloquear y quitar PIN permanentemente"
(ahora funciona también con la SIM ya desbloqueada) para no depender del PIN.

## 9. Cuarta iteración: antena conectada, sigue sin señal (21 Sep 2026)

Con la antena instalada, tras reinicio completo del módulo (CRESET) el módem
arrancaba en `stat=0` (ni siquiera buscando). Mejoras añadidas:

| Mejora | Detalle |
|--------|---------|
| **Auto-kick `AT+COPS=0`** | Si `stat=0` (módem rendido o NVM en modo manual), fuerza selección automática; reintento cada 30 s |
| **`AT+CPSI?` en el ciclo** | Estado radio real ("NO SERVICE" / tecnología+banda) en log, API (`radio`) y fila "Radio" de `/gsm` |
| Ciclo de sondeo en registro | CEREG → CSQ → CPSI rotando cada 2 s |

### Evidencia (ciclo completo capturado)

```text
stat=0: forzando búsqueda (AT+COPS=0)… → OK
CREG: stat=2 (buscando red)            ← busca…
Radio (CPSI): NO SERVICE,Online        ← radio encendida, CERO señal
CREG: stat=0                           ← …se rinde (nada que encontrar)
(bucle: el firmware re-lanza la búsqueda cada 30 s)
```

### Conclusión

El módem busca cuando se le ordena y **no encuentra ninguna portadora en
ninguna banda** (`NO SERVICE`, CSQ=99 constante). El firmware queda validado y
auto-recuperable; la causa es física en la cadena RF:

1. **Conector equivocado en el módulo** (lo más común): el pigtail u.FL debe ir
   en **MAIN** del SIM7600 — no en AUX/DIV ni en GPS (los tres son idénticos).
2. Pigtail u.FL sin clipar del todo, o cable/SMA dañado.
3. **Antena no LTE** (una antena WiFi 2,4 GHz tiene la misma rosca SMA y no
   capta LTE).
4. Cobertura nula en la ubicación (comprobar con un móvil junto al equipo).

Tras corregir: botón "Reiniciar módulo y re-detectar SIM" en `/gsm` — con
señal se verá CSQ 5-31, CPSI con tecnología/banda y `attached` en segundos.

## 10. Registro conseguido + datos 4G implementados (21 Sep 2026)

Con la antena en el conector correcto: **`attached` — vodafone ES, LTE B20,
CSQ 20 (−73 dBm)**. La rama pasa de diagnóstico a funcionalidad: se adelanta
la **Fase 4 del plan (datos 4G / MQTT backup)** para el A2v3.

### Arquitectura de datos (solo A2v3 / core 3.x)

- La FSM cruda validada hace todo el bring-up (probe, PIN un intento, SPIC,
  registro, diagnóstico) hasta `attached`.
- Entonces cede la UART a la librería **PPP** del core (esp_modem) en modo
  **CMUX**: datos y comandos AT simultáneos (el estado sigue sondeándose vía
  `PPP.RSSI()`/`operatorName()` sin cortar la conexión).
- El netif PPP alimenta `net_manager` (`netOnGsmGotIp/Down`): prioridad
  **ETH > WiFi STA > 4G**, `PPP.setDefault()` cuando es la única viva, y el
  failover MQTT reutiliza el mecanismo de cambio de interfaz ya existente —
  cero cambios en la lógica MQTT.
- `data_mode=2` desactiva los datos; APN obligatorio (aviso en log/`/gsm` si
  falta). Rescan/APN nuevo reinician el ciclo limpiamente.
- A2 clásico (core 2.x): sin datos (sin soporte PPP); GSM queda como estado.

### Panel "Estado de Red" en la página principal

Tabla en vivo (poll 3 s): Ethernet+IP, WiFi STA, AP+clientes, 4G
(registro/datos/IP PPP) y **MQTT con la interfaz por la que sale** — permite
validar visualmente el failover ETH → WiFi → 4G.

### Validación en placa (parcial)

```text
registering → attached  (COPS=0 auto-kick funcionó a la primera)
⚠️ Registrado pero SIN APN configurado - datos 4G en espera (configurar en /gsm)
```

Pendiente para cerrar: **APN del contrato M2M** (configurar en `/gsm`) →
`data_up` con IP PPP → prueba de failover: desconectar ETH (sin STA) y ver en
serial "Cambio de interfaz de red (→ 4g): reconectando MQTT… Conectado".

## 11. Validación FINAL del failover en placa (21 Sep 2026) ✅

### APN automático

En `apn_mode=auto` el firmware lee `AT+CGDCONT?`; el bearer cid 1 venía con
APN vacío (asignación por suscripción) → PPP arranca con APN vacío y la red
resuelve:

```text
📡 [GSM] Iniciando datos 4G (PPP/CMUX, APN '' [auto/red])…
📡 [GSM] ✅ Datos 4G conectados (PPP): IP 10.162.138.95
```

### Failover ETH → 4G (cable desconectado físicamente)

```text
🌐 ETH Desconectado
🔄 Reconexión MQTT (backoff: 5s)...
📡 Conectando a MQTT (timeout: 5s)... ✅ Conectado en 97ms   ← por 4G
📡 Suscrito a: swatidhome/command/SWATID_A0461CA0ED30/#
```

Doble confirmación: la web LAN sin respuesta (cable fuera de verdad) mientras
MQTT quedó conectado y suscrito — único camino posible: PPP/4G.

### Failback 4G → ETH (cable reconectado)

```text
🌐 ETH Dirección IP: 192.168.5.77
🔄 Cambio de interfaz de red (→ eth): reconectando MQTT
📡 Conectando a MQTT (timeout: 5s)... ✅ Conectado en 78ms   ← de vuelta por cable
```

### Estado final verificado (las TRES interfaces arriba a la vez)

```json
{"eth_up":true, "eth_ip":"192.168.5.77",
 "sta":{"connected":true,"ssid":"SWATID_REP","ip":"192.168.10.146"},
 "gsm_up":true, "mqtt_iface":"eth", "mqtt_connected":true}
```

Prioridad ETH > WiFi > 4G aplicada correctamente con el stack triple completo;
el PPP queda de reserva permanente sin cortar la sesión de datos.

## 12. Estado — RAMA VALIDADA

| Ítem | Estado |
|------|--------|
| Rama `v5.0.1` | Creada |
| Instrumentación CPIN/probe/PIN | ✅ Implementada y verificada en placa |
| Gracia arranque + reintentos (H1/H2) | ✅ Implementados |
| Rescan con reinicio de módulo (CRESET) | ✅ Verificado end-to-end |
| SIM no detectada | ✅ Resuelto re-asentando (causa física, H5) |
| SIM con PIN (H4) | ✅ Resuelto — PIN 0987 aceptado y almacenado (arranques en frío cubiertos) |
| Registro de red | ✅ `attached` — vodafone ES, LTE B20, CSQ 20 (causa era la antena) |
| Auto-kick COPS=0 + CPSI | ✅ Implementados y verificados |
| Datos 4G (PPP/CMUX, APN auto) | ✅ IP 10.162.138.95 — Fase 4 adelantada en A2v3 |
| Failover ETH→4G | ✅ MQTT reconectado por 4G en 97 ms (cable fuera, LAN muerta) |
| Failback 4G→ETH | ✅ "Cambio de interfaz (→ eth)" + reconexión en 78 ms |
| Triple stack simultáneo | ✅ ETH+STA+4G arriba, MQTT vía eth (prioridad correcta) |
| Cierre | ✅ **Objetivo de la rama cumplido y validado en hardware** (opcional: quitar PIN permanente de la SIM) |
