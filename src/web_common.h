#pragma once

#include <Arduino.h>

// ============================================================================
// web_common — recursos compartidos del portal web embebido
//
// Un único stylesheet (unión normalizada de los 13 bloques <style> que había
// repartidos por las páginas) y helpers de cabecera/pie y autenticación.
// Reduce flash (CSS una sola vez) y da aspecto homogéneo a todas las páginas.
// ============================================================================

// true si la petición está autenticada; si no, envía requestAuthentication()
// y devuelve false (el handler debe hacer return inmediatamente).
bool webAuth();

// Cabecera completa de página: <!DOCTYPE...><head> con meta, título y el CSS
// común </head><body>. El cuerpo de la página se concatena a continuación.
String webPageBegin(const char* title);

// Cierre de página
extern const char WEB_PAGE_END[];
