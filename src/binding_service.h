#pragma once

#include <Arduino.h>

// ============================================================================
// binding_service — gestión uniforme de usuarios (v5.0.5, alineado X1)
//
// Canal MQTT de vinculación:
//   backend → equipo : swatidhome/command/<serial>/vinculo
//   equipo  → backend: swatidhome/<serial>/vinculo        (respuestas)
//   eventos          : swatidhome/events/<serial>/ble
//
// Modelo de roles (plan v5.0.5):
//   - Superadmin de PLATAFORMA: clave de servicio SWATID. Siempre accede;
//     ningún unlink/clear de admin o usuarios la borra. Alta/baja solo por
//     procedimiento de fábrica (web autenticada) o rotación firmada por ella.
//   - Admin de PLACA (slot 0): dueño operativo. Alta por TOFU (`link` con la
//     ventana remota abierta y sin admin previo). En JSON se reporta
//     role "admin" (alias X1 "superadmin" en superadmin_registered).
//   - Usuarios (slots 1..5): alta/baja por admin o plataforma, rotación
//     propia. Permisos bitmask: 1=relés, 2=modo, 4=códigos, 8=red.
//
// Autenticación: challenge GET → nonce 64 hex (60 s, uno vivo, se consume);
// sobre {nonce, hmac, payload} con hmac = HMAC-SHA256(key, unhex(nonce) ||
// utf8(payload)). El equipo prueba plataforma → admin → usuarios 1..5.
//
// Persistencia: blob NVS "a2acc_bind/store" (disponible también en builds
// sin BLE). En builds BLE se migra una vez desde BLEAuthConfig (la clave
// "superadmin" BLE histórica pasa a ser el ADMIN de placa) y los cambios se
// sincronizan de vuelta para que la app BLE siga autenticando.
// ============================================================================

#define BINDING_KEY_SIZE 64     // 64 bytes = 128 hex
#define BINDING_MAX_USERS 5     // slots 1..5 (slot 0 = admin de placa)
#define BINDING_WINDOW_BOOT_S 600
#define BINDING_NONCE_TTL_S 60

// Carga NVS (y migración BLE si procede). Llamar tras cargar la config BLE.
void bindingServiceBegin();

// Procesa un mensaje recibido en .../vinculo. Devuelve el JSON de respuesta
// a publicar en swatidhome/<serial>/vinculo.
String bindingHandleMessage(const String& message);

// Publicador de eventos BINDING_* (lo inyecta main: events/<serial>/ble)
void bindingSetEventPublisher(void (*fn)(const String& json));

// Ventana remota de vinculación (TOFU)
void bindingOpenWindow(uint32_t seconds);
void bindingCloseWindow();
bool bindingWindowOpen();
int bindingWindowRemainingS();

// Estado (web / devinfo)
bool bindingAdminSet();
bool bindingPlatformSet();
int bindingUsersCount();
String bindingStatusJson();

// Provisioning de fábrica (SOLO desde la web autenticada del equipo)
bool bindingSetPlatformKeyHex(const String& hex128);
bool bindingClearPlatformKey();

// Restablecimiento de vínculos desde la web (fábrica/banco): borra admin de
// placa y usuarios (la plataforma queda INTACTA), sincroniza con BLE y abre
// la ventana TOFU para que la App pueda vincularse de inmediato.
bool bindingResetOwner();

// U5 (BLE): acceso de solo lectura a la clave de plataforma para el
// challenge-response BLE (SHA256(key ‖ nonce)). nullptr si no provisionada.
// Mismo dominio de confianza (firmware); la clave nunca sale del equipo.
const uint8_t* bindingPlatformKeyPtr();

// Sincronización inversa BLE → binding (v5.0.6): las mutaciones hechas por
// el canal BLE (TOFU del primer FF01, tipo 6 MQTT) escriben BLEAuthConfig;
// esta función las refleja en el almacén de vinculación para que /vinculo,
// devinfo y la clave de plataforma vean SIEMPRE el mismo estado. main la
// invoca desde saveBLEAuthConfig(). No toca la clave de plataforma.
void bindingSyncFromBle();
