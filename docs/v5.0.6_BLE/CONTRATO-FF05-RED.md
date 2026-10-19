# Contrato BLE FF05 — Red unificada (ETH + WiFi) · v5.0.6 Fase 1

**Característica:** `0000FF05-0000-1000-8000-00805F9B34FB` (servicio FF00)
**Permiso requerido:** `BLE_PERM_NETWORK_CONFIG = 0x08`
⚠️ Corrección documental (B4): documentos v4.1 anteriores indicaban `0x02`
para red — el valor correcto **siempre fue 0x08** (0x02 es `PERM_MODE`).

Requisitos previos: sesión autenticada (FF0B challenge → FF01 response) con
permiso de red. Todas las respuestas (READ y WRITE) llegan por **notify con
fragmentación y marcador EOT (0x04)** — la App concatena hasta el EOT.

---

## READ — estado unificado

Campos legados **intactos** (Apps v4.1 no necesitan cambios) + aditivos v5.0.6:

```json
{
  "dhcp": true, "eth": true, "ip": "192.168.5.64", "gw": "192.168.5.1",
  "mask": "255.255.255.0", "mac": "A8:7F:B5:BF:13:88",
  "cfg_ip": "…", "cfg_gw": "…", "cfg_mask": "…",      // solo si dhcp=false

  "sta": { "connected": true, "ssid": "MiRed", "ip": "192.168.1.40", "rssi": -58 },
  "ap":  { "active": false, "clients": 0, "remaining_s": 0 },
  "ble_net_lock": true
}
```

Nunca incluye contraseñas.

## WRITE — dos formatos

1. **Binario 17 B (LEGADO Ethernet)**: `[dhcp][ip×4][gw×4][mask×4][dns×4]` —
   sin cambios; guarda y reinicia en 2 s.
2. **JSON** (v5.0.6): primer byte `{` → `{"cmd": "...", ...}`.

### Comandos JSON

| `cmd` | Parámetros | Respuesta OK | Notas |
|-------|------------|--------------|-------|
| `status` | — | estado unificado (= READ) | |
| `wifi_scan_start` | — | `{"ok":true,"scanning":true}` | Entra en **modo config** (apaga el AP); rate-limit 1/10 s (`scan_rate_limited`) |
| `wifi_scan_get` | — | `{"scanning":bool,"networks":[{"ssid","rssi","enc"}]}` | Poll cada ~2 s hasta `scanning:false` (máx. 20 redes) |
| `wifi_sta_set` | `ssid` (1–32), `pass` opc. | `{"ok":true,"connecting":true}` | Entra en modo config; guarda en NVS y conecta; comprobar con `status` |
| `wifi_sta_forget` | — | `{"ok":true,"sta":"forgotten"}` | Borra credenciales |
| `wifi_ap_set` | `mode` 0 off / 1 ventana / 2 siempre, `timeout_s` 10–3600, `pass` ≥8 — todos opc. | `{"ok":true}` | Errores: `invalid_ap_mode`, `ap_pass_min_8` |
| `wifi_ap_stop` | — | `{"ok":true,"ap":"off"}` | |
| `eth_set` | `dhcp` bool; si false: `ip`, `gw`, `mask`, `dns` opc. | `{"ok":true,"restarting":true}` | Equivalente JSON del binario; **reinicia en 2 s** |
| `exit_config` | — | `{"ok":true,"net_lock":false}` | Libera el lock y restaura la política del AP |

Errores comunes: `ERROR:NOT_AUTHENTICATED`, `ERROR:NO_PERMISSION` (texto
plano, pre-v5.0.6) · `invalid_json`, `unknown_cmd`, `invalid_ssid`,
`invalid_ip`, `scan_rate_limited` (JSON `{"ok":false,"error":…}`).

---

## Política AP ↔ BLE (B1, reglas R1–R5)

| Regla | Comportamiento implementado |
|-------|------------------------------|
| R1 | `wifi_scan_start` / `wifi_sta_set` apagan el SoftAP y toman el lock de red |
| R2/R4 | Con el lock activo, el portal web **rechaza** cambios de red con HTTP **409** |
| R3 | Al cerrar la sesión BLE (desconexión, timeout o `exit_config`): se restaura la política del AP de NVS — `always_on` reenciende; `boot_window` solo si la STA **no** quedó conectada |
| R5 | El escaneo es asíncrono (nunca bloquea accesos/relés) |

## Flujo recomendado para la App

```text
1. Connect → FF08 (devinfo) → FF0B (challenge) → FF01 (SHA256(key‖nonce)) → token
2. FF05 WRITE {"cmd":"wifi_scan_start"}
3. FF05 WRITE {"cmd":"wifi_scan_get"}  (poll ~2 s hasta scanning:false)
4. FF05 WRITE {"cmd":"wifi_sta_set","ssid":"…","pass":"…"}
5. FF05 WRITE {"cmd":"status"}  → sta.connected / sta.ip
6. FF05 WRITE {"cmd":"exit_config"}  (o simplemente desconectar)
```

## Pruebas del plan §9 cubiertas por diseño (pendiente validación con App)

W1–W5 implementadas; W6 garantizada por diseño (scan async + accesos primero
en el loop). S1/S2 reutilizan la auth existente; S3: el READ no expone
contraseñas.
