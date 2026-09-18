#include "web_common.h"

#include <WebServer.h>
#include <pgmspace.h>

// Servicios de main.ino (pasarán a web_portal en su tranche)
extern WebServer server;
extern const char* admin_user;
extern char admin_password[32];

// Stylesheet único del portal: unión normalizada de los estilos que cada
// página definía por separado. Cubre todos los selectores usados por las
// páginas actuales (root, códigos, códigos remotos, DI, OTA, hora, BLE).
static const char WEB_CSS[] PROGMEM =
  "body{font-family:Arial,sans-serif;margin:0;background:#f4f6f8;color:#333}"
  "header{background:#35424a;color:#fff;padding:16px 0;text-align:center}"
  "main{padding:20px}"
  "nav{background:#2c3e50;padding:10px;text-align:center}"
  "nav a{color:#fff;margin:0 10px;text-decoration:none}"
  "nav a:hover{text-decoration:underline}"
  "h1,h2,h3{color:#2c3e50}"
  "p{line-height:1.4}"
  ".container{max-width:1000px;margin:20px auto;background:#fff;padding:20px;"
    "border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,.1)}"
  "form{background:#fff;padding:20px;border-radius:10px;margin-bottom:20px;"
    "box-shadow:0 2px 8px rgba(0,0,0,.1)}"
  "label{display:block;margin-top:12px;margin-bottom:5px;font-weight:bold}"
  "input,select{width:100%;padding:8px;margin-top:5px;border:1px solid #ccc;"
    "border-radius:5px;box-sizing:border-box}"
  "input[type='checkbox']{width:auto}"
  ".checkbox-label{display:inline;font-weight:normal}"
  "table{width:100%;border-collapse:collapse;margin-top:15px}"
  "th,td{border:1px solid #ddd;padding:8px;text-align:center}"
  "th{background:#f2f2f2}"
  "button,.btn{padding:10px 16px;border:none;border-radius:5px;cursor:pointer;"
    "background:#4CAF50;color:#fff;font-size:15px;margin:4px}"
  "button:hover,.btn:hover{opacity:.85}"
  ".btn-primary{background:#007bff}"
  ".btn-info{background:#17a2b8}"
  ".btn-secondary,.btn-back,button.back,button.home{background:#95a5a6}"
  ".btn-danger{background:#dc3545}"
  ".btn-warning{background:#ffc107;color:#212529}"
  ".btn-success,button.save{background:#28a745}"
  "button.sync{background:#f39c12}"
  ".button-group{text-align:center;margin:20px 0}"
  ".badge{padding:3px 8px;border-radius:4px;font-size:12px;color:#fff}"
  ".badge-success{background:#28a745}"
  ".badge-warning{background:#ffc107;color:#212529}"
  ".badge-danger{background:#dc3545}"
  ".card,.security-status,.dual-status,.export-section,.time-sync-section,"
  ".security-controls,.security-info,.dual-info,.remote-info,.info-box,"
  ".info-panel,.status-panel,.time-display,.time-info,.input-config,"
  ".confirm-box,.summary{background:#f8f9fa;padding:15px;margin:15px 0;"
    "border-radius:8px;border:1px solid #dee2e6}"
  ".security-status,.security-info{background:#fff3cd;border-left:4px solid #ffc107}"
  ".security-blocked{background:#f8d7da;border-left:4px solid #dc3545}"
  ".dual-status,.dual-info{background:#e8f5e8;border-left:4px solid #28a745}"
  ".time-sync-section,.info-box,.time-display{background:#e8f4fd;border:1px solid #bee5eb}"
  ".input-config.enabled{border-left:4px solid #28a745}"
  ".status{padding:10px;border-radius:5px;margin:10px 0}"
  ".status.synced,.success{background:#d5f4e6;color:#27ae60;border:1px solid #27ae60}"
  ".status.not-synced,.error{background:#fadbd8;color:#e74c3c;border:1px solid #e74c3c}"
  ".info{background:#e8f4fd;color:#31708f}"
  ".device-time{color:#e74c3c;font-weight:bold}"
  ".browser-time{color:#27ae60;font-weight:bold}"
  ".hidden{display:none}"
  ".readonly{background:#e9ecef;color:#6c757d}"
  ".actions a{margin-right:10px;text-decoration:none}"
  "a.delete{color:#dc3545}"
  "a.delete:hover{text-decoration:underline}"
  ".icon{margin-right:5px}"
  ".progress-container{background:#e9ecef;border-radius:5px;overflow:hidden;margin:10px 0}"
  ".progress-bar,.progress-fill{background:#28a745;height:20px;width:0;transition:width .3s}"
  ".status-indicator{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:6px}"
  ".status-high{background:#28a745}"
  ".status-low{background:#dc3545}"
  ".success-icon{color:#28a745;font-size:40px}"
  ".time-slot{background:#fff;border:1px solid #dee2e6;border-radius:5px;padding:10px;margin:8px 0}"
  ".permissions{font-size:13px;color:#555}"
  ".user-card{background:#fff;border:1px solid #dee2e6;border-left:4px solid #6c757d;"
    "border-radius:5px;padding:12px;margin:10px 0}"
  ".user-card.active{border-left-color:#28a745}"
  ".user-card.inactive{border-left-color:#dc3545;opacity:.7}"
  ".user-card.superadmin{border-left-color:#ffc107}"
  ".ble-status{background:#e8f4fd;padding:15px;border-radius:8px;margin:15px 0}"
  ".ble-enabled{border-left:4px solid #28a745}"
  ".ble-warning{background:#fff3cd;border-left:4px solid #ffc107;padding:15px;"
    "border-radius:8px;margin:15px 0}";

const char WEB_PAGE_END[] = "</body></html>";

bool webAuth() {
  if (!server.authenticate(admin_user, admin_password)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

String webPageBegin(const char* title) {
  String html;
  html.reserve(sizeof(WEB_CSS) + 220 + strlen(title));
  html += F("<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<title>");
  html += title;
  html += F("</title><style>");
  html += FPSTR(WEB_CSS);
  html += F("</style></head><body>");
  return html;
}
