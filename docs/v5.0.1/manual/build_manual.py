#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generador del Manual de Usuario SWATID A2v3 (PDF).
Uso: /tmp/pdfenv/bin/python build_manual.py
"""
import os
from PIL import Image as PILImage
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph,
                                Spacer, Image, Table, TableStyle, PageBreak,
                                NextPageTemplate, KeepTogether)
from reportlab.platypus.tableofcontents import TableOfContents

BASE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(BASE, 'img')
OUT = os.path.join(BASE, 'MANUAL-USUARIO-SWATID-A2V3-v1.0.pdf')

AZUL = colors.HexColor('#1d5fa7')
AZUL_OSCURO = colors.HexColor('#35424a')
GRIS = colors.HexColor('#6c757d')

EMPRESA = "Smart World And Things SLU"
CIF = "B40599813"
TEL = "+34 633 448 427"
MAIL = "info@swat-id.com"
WEB = "www.swat-id.com"
VERSION_MANUAL = "v1.0"
FECHA = "Septiembre 2026"
FIRMWARE = "v5.0.0-S3"
PRODUCTO = "SWATID A2v3"

# ----------------------------------------------------------------------------
# Estilos
# ----------------------------------------------------------------------------
ss = getSampleStyleSheet()
st_body = ParagraphStyle('Cuerpo', parent=ss['Normal'], fontSize=10, leading=14,
                         alignment=TA_JUSTIFY, spaceAfter=6)
st_h1 = ParagraphStyle('H1x', parent=ss['Heading1'], fontSize=16, leading=20,
                       textColor=AZUL, spaceBefore=18, spaceAfter=8)
st_h2 = ParagraphStyle('H2x', parent=ss['Heading2'], fontSize=13, leading=16,
                       textColor=AZUL_OSCURO, spaceBefore=12, spaceAfter=6)
st_cap = ParagraphStyle('Caption', parent=ss['Normal'], fontSize=8.5,
                        textColor=GRIS, alignment=TA_CENTER, spaceBefore=3,
                        spaceAfter=10)
st_nota = ParagraphStyle('Nota', parent=st_body, backColor=colors.HexColor('#fff3cd'),
                         borderPadding=6, borderColor=colors.HexColor('#ffc107'),
                         borderWidth=1, spaceBefore=6, spaceAfter=8)
st_ok = ParagraphStyle('NotaOk', parent=st_nota, backColor=colors.HexColor('#e8f5e8'),
                       borderColor=colors.HexColor('#28a745'))
st_toc_t = ParagraphStyle('TOCTitle', parent=st_h1, spaceBefore=0)

TSTYLE = TableStyle([
    ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#f2f2f2')),
    ('TEXTCOLOR', (0, 0), (-1, 0), AZUL_OSCURO),
    ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
    ('FONTSIZE', (0, 0), (-1, -1), 8.5),
    ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#cccccc')),
    ('VALIGN', (0, 0), (-1, -1), 'TOP'),
    ('LEFTPADDING', (0, 0), (-1, -1), 5),
    ('RIGHTPADDING', (0, 0), (-1, -1), 5),
    ('TOPPADDING', (0, 0), (-1, -1), 3.5),
    ('BOTTOMPADDING', (0, 0), (-1, -1), 3.5),
])


def tcell(txt, bold=False):
    stl = ParagraphStyle('c', parent=ss['Normal'], fontSize=8.5, leading=11,
                         fontName='Helvetica-Bold' if bold else 'Helvetica')
    return Paragraph(txt, stl)


def tabla(headers, rows, widths):
    data = [[tcell(h, True) for h in headers]] + [[tcell(c) for c in r] for r in rows]
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1)
    t.setStyle(TSTYLE)
    return t


def figura(nombre, ancho_mm, caption):
    path = os.path.join(IMG, nombre)
    with PILImage.open(path) as im:
        w, h = im.size
    ancho = ancho_mm * mm
    alto = ancho * h / w
    max_alto = 225 * mm
    if alto > max_alto:
        alto = max_alto
        ancho = alto * w / h
    return [Image(path, width=ancho, height=alto), Paragraph(caption, st_cap)]


# ----------------------------------------------------------------------------
# Documento con TOC + marcadores
# ----------------------------------------------------------------------------
class ManualDoc(BaseDocTemplate):
    def __init__(self, filename, **kw):
        super().__init__(filename, pagesize=A4, **kw)
        self._h1 = 0
        frame_cover = Frame(15*mm, 15*mm, A4[0]-30*mm, A4[1]-30*mm, id='cover')
        frame_body = Frame(18*mm, 20*mm, A4[0]-36*mm, A4[1]-40*mm, id='body')
        self.addPageTemplates([
            PageTemplate(id='Cover', frames=[frame_cover]),
            PageTemplate(id='Body', frames=[frame_body], onPage=self._decorate),
        ])

    def _decorate(self, canv, doc):
        canv.saveState()
        # Cabecera
        canv.setStrokeColor(AZUL)
        canv.setLineWidth(0.8)
        canv.line(18*mm, A4[1]-14*mm, A4[0]-18*mm, A4[1]-14*mm)
        canv.setFont('Helvetica', 8)
        canv.setFillColor(GRIS)
        canv.drawString(18*mm, A4[1]-12*mm, f"{PRODUCTO} — Manual de Usuario")
        canv.drawRightString(A4[0]-18*mm, A4[1]-12*mm, EMPRESA)
        # Pie
        canv.line(18*mm, 14*mm, A4[0]-18*mm, 14*mm)
        canv.drawString(18*mm, 9.5*mm, f"Manual {VERSION_MANUAL} · {FECHA} · Firmware {FIRMWARE}")
        canv.drawCentredString(A4[0]/2, 9.5*mm, WEB)
        canv.drawRightString(A4[0]-18*mm, 9.5*mm, f"Página {doc.page}")
        canv.restoreState()

    def afterFlowable(self, fl):
        if isinstance(fl, Paragraph):
            stl = fl.style.name
            if stl in ('H1x', 'H2x'):
                text = fl.getPlainText()
                level = 0 if stl == 'H1x' else 1
                key = f"h-{self.page}-{abs(hash(text)) % 99991}"
                self.canv.bookmarkPage(key)
                self.canv.addOutlineEntry(text, key, level=level, closed=False)
                self.notify('TOCEntry', (level, text, self.page, key))


doc = ManualDoc(OUT, title=f"Manual de Usuario {PRODUCTO}", author=EMPRESA)
story = []

# ----------------------------------------------------------------------------
# PORTADA
# ----------------------------------------------------------------------------
story.append(Spacer(1, 18*mm))
logo = Image(os.path.join(IMG, 'logo_swatid.png'), width=70*mm, height=70*mm)
logo.hAlign = 'CENTER'
story.append(logo)
story.append(Spacer(1, 6*mm))
story.append(Paragraph("MANUAL DE USUARIO",
    ParagraphStyle('t1', fontSize=26, leading=30, alignment=TA_CENTER,
                   fontName='Helvetica-Bold', textColor=AZUL_OSCURO)))
story.append(Spacer(1, 4*mm))
story.append(Paragraph("Controladora de Accesos <b>SWATID A2v3</b>",
    ParagraphStyle('t2', fontSize=17, leading=22, alignment=TA_CENTER,
                   textColor=AZUL)))
story.append(Paragraph("Ethernet · WiFi · 4G · Pantalla LCD · RTC",
    ParagraphStyle('t3', fontSize=12, leading=16, alignment=TA_CENTER,
                   textColor=GRIS)))
story.append(Spacer(1, 14*mm))
vt = Table([
    ['Versión del manual', VERSION_MANUAL],
    ['Fecha', FECHA],
    ['Firmware de referencia', FIRMWARE],
    ['Hardware', 'KinCony KC868-A2v3 (ESP32-S3)'],
], colWidths=[55*mm, 65*mm])
vt.setStyle(TableStyle([
    ('FONTNAME', (0, 0), (0, -1), 'Helvetica-Bold'),
    ('FONTSIZE', (0, 0), (-1, -1), 10),
    ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#bbbbbb')),
    ('BACKGROUND', (0, 0), (0, -1), colors.HexColor('#eef4fb')),
    ('TOPPADDING', (0, 0), (-1, -1), 5),
    ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
    ('LEFTPADDING', (0, 0), (-1, -1), 8),
]))
vt.hAlign = 'CENTER'
story.append(vt)
story.append(Spacer(1, 22*mm))
story.append(Paragraph(
    f"<b>{EMPRESA}</b> · CIF {CIF}<br/>Tel. {TEL} · {MAIL} · {WEB}",
    ParagraphStyle('emp', fontSize=10, leading=15, alignment=TA_CENTER,
                   textColor=AZUL_OSCURO)))
story.append(NextPageTemplate('Body'))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# ÍNDICE
# ----------------------------------------------------------------------------
story.append(Paragraph("Índice", st_toc_t))
toc = TableOfContents()
toc.levelStyles = [
    ParagraphStyle('TOC1', fontSize=11, leading=16, fontName='Helvetica-Bold',
                   textColor=AZUL_OSCURO, leftIndent=2),
    ParagraphStyle('TOC2', fontSize=9.5, leading=13, leftIndent=14),
]
toc.dotsMinLevel = 0
story.append(toc)
story.append(PageBreak())


def H1(txt):
    story.append(Paragraph(txt, st_h1))


def H2(txt):
    story.append(Paragraph(txt, st_h2))


def P(txt):
    story.append(Paragraph(txt, st_body))


def NOTA(txt):
    story.append(Paragraph("<b>Atención:</b> " + txt, st_nota))


def OK(txt):
    story.append(Paragraph(txt, st_ok))


def FIG(nombre, ancho, caption):
    for el in figura(nombre, ancho, caption):
        el.hAlign = 'CENTER'
        story.append(el)


# ============================ 1. INTRODUCCIÓN ==============================
H1("1. Introducción")
P("El <b>SWATID A2v3</b> es una controladora de accesos profesional para la "
  "gestión de hasta <b>dos puertas</b> mediante lectores Wiegand (teclados de "
  "PIN y/o lectores de tarjeta RFID). Valida credenciales de forma <b>local</b> "
  "— sin depender de conectividad — y de forma <b>remota</b> contra la "
  "plataforma SWATID a través de MQTT.")
P("Esta versión A2v3 incorpora triple conectividad WAN con conmutación "
  "automática — <b>Ethernet, WiFi y 4G</b> —, pantalla <b>LCD OLED</b> de "
  "estado, y reloj de tiempo real (<b>RTC</b>) con batería para mantener la "
  "hora sin alimentación ni red.")
P("Principios de diseño del equipo:")
P("• La apertura de puertas con credenciales locales <b>funciona siempre</b>, "
  "aunque no exista ninguna conectividad.<br/>"
  "• La red se gestiona con prioridad <b>Ethernet &gt; WiFi &gt; 4G</b> y "
  "conmutación automática en segundos, sin reiniciar el equipo.<br/>"
  "• Toda la configuración se realiza desde el navegador web, sin software "
  "adicional.")

# ============================ 2. SEGURIDAD =================================
H1("2. Advertencias de seguridad")
P("• La instalación debe realizarla personal cualificado, con el equipo "
  "desconectado de la alimentación.<br/>"
  "• Alimentar únicamente a <b>12 V DC</b> con una fuente dimensionada para "
  "los picos de consumo del módem 4G.<br/>"
  "• No manipular la tarjeta SIM ni los conectores de antena con el equipo "
  "encendido.<br/>"
  "• Los relés conmutan cargas de baja tensión de seguridad (cerraderos, "
  "tornos). No conectar directamente cargas de red eléctrica.<br/>"
  "• Instalar el equipo en el interior de una envolvente adecuada, protegido "
  "de humedad y polvo.")

# ============================ 3. CARACTERÍSTICAS ===========================
H1("3. Características técnicas")
story.append(tabla(
    ['Característica', 'Descripción'],
    [
        ['Microcontrolador', 'ESP32-S3 (N16R8: 16 MB flash, 8 MB PSRAM)'],
        ['Ethernet', 'W5500 10/100 Mbps (RJ45), DHCP o IP estática'],
        ['WiFi', '2,4 GHz — Punto de acceso de gestión y cliente (STA)'],
        ['Módem celular', 'SIMCOM SIM7600E (LTE cat. 1, bandas europeas), SIM estándar'],
        ['Datos 4G', 'PPP con APN automático o manual; respaldo de MQTT'],
        ['Pantalla', 'OLED 0,96" 128×64 (estado de red, relés, entradas, hora)'],
        ['Reloj (RTC)', 'DS3231 con batería de respaldo, sincronización automática'],
        ['Lectores', '2 × Wiegand (teclado PIN / RFID), uso simultáneo'],
        ['Salidas', '2 relés para cerradero/torno, tiempo de apertura configurable'],
        ['Entradas', '2 entradas digitales (pulsador de salida, contacto de puerta)'],
        ['Bus auxiliar', 'RS485 (teclados compatibles)'],
        ['Capacidad local', '50 códigos locales + 40 códigos remotos con franjas horarias'],
        ['Gestión', 'Web embebida, MQTT, actualización OTA con rollback'],
        ['Alimentación', '12 V DC'],
    ],
    [45, 125]))

# ============================ 4. INSTALACIÓN ===============================
H1("4. Instalación y conexionado")
H2("4.1 Alimentación")
P("Conectar 12 V DC en el borne de alimentación respetando la polaridad. "
  "Emplear una fuente con margen suficiente: el módem 4G genera picos de "
  "consumo durante la transmisión.")
H2("4.2 Antena 4G")
NOTA("El módem no funcionará sin antena. Conectar una antena <b>LTE/4G</b> "
     "(no una antena WiFi, aunque la rosca SMA sea idéntica) y comprobar que "
     "el latiguillo interno está clipado en el conector <b>MAIN</b> del "
     "módulo SIM7600 — no en AUX ni en GPS. Sin antena, la página 4G "
     "mostrará «Sin señal» (CSQ 99) de forma permanente.")
H2("4.3 Tarjeta SIM")
P("Insertar la SIM (tamaño estándar del zócalo; evitar adaptadores, que no "
  "suelen accionar el detector de presencia) con el equipo <b>apagado</b>, "
  "hasta su tope. Si la SIM se inserta en caliente, usar el botón «Reiniciar "
  "módulo y re-detectar SIM» de la página 4G.")
P("Si la SIM tiene PIN, se configura una única vez desde la página 4G "
  "(ver sección 8.6). Para SIM M2M se recomienda retirar el PIN de forma "
  "permanente desde esa misma página.")
H2("4.4 Ethernet")
P("Conectar el cable de red al RJ45. Por defecto el equipo obtiene IP por "
  "DHCP; puede fijarse IP estática desde la página de inicio.")
H2("4.5 Lectores Wiegand, entradas y relés")
story.append(tabla(
    ['Conexión', 'Señales', 'Notas'],
    [
        ['Lector 1 (puerta 1)', 'D0, D1, GND, 12V', 'Teclado PIN o lector RFID Wiegand'],
        ['Lector 2 (puerta 2)', 'D0, D1, GND, 12V', 'Funcionamiento simultáneo con el lector 1'],
        ['Relé 1 / Relé 2', 'C, NO, NC', 'Cerradero o torno; tiempo configurable'],
        ['Entrada DI1 / DI2', 'Contacto seco', 'Pulsador de salida o automatismo (modo normal/inverso)'],
        ['RS485', 'A, B', 'Teclados RS485 compatibles (opcional)'],
    ],
    [42, 40, 88]))
P("La asignación lector→relé se define en la web (modo torno o asignación "
  "por código).")

# ============================ 5. PANTALLA LCD ==============================
H1("5. Pantalla LCD de estado")
P("La pantalla OLED muestra en tiempo real (refresco 1 s) el estado completo "
  "del equipo:")
FIG('lcd_pantalla.png', 105, "Figura 5.1 — Pantalla de estado (contenido real de ejemplo)")
story.append(tabla(
    ['Fila', 'Contenido', 'Ejemplo'],
    [
        ['1', 'Fecha y hora (RTC / sistema)', '21/09/26  12:35:04'],
        ['2', 'IP Ethernet («sin enlace» si no hay cable)', 'E:192.168.5.77'],
        ['3', 'IP WiFi cliente («--» si no configurada)', 'W:192.168.10.146'],
        ['4', 'Punto de acceso: clientes y ventana restante', 'AP:ON 2cli 45s / AP:off'],
        ['5', 'Estado 4G y cobertura CSQ (0-31)', '4G:attached CSQ19'],
        ['6', 'Estado real de los relés', 'R1:off  R2:ON'],
        ['7', 'Entradas digitales (H/L) y MQTT (ok/--)', 'D1:H D2:L  M:ok'],
        ['8', 'Identificador del equipo', 'SWATID_A0461CA0ED30'],
    ],
    [12, 92, 66]))

# ============================ 6. PUESTA EN MARCHA ==========================
H1("6. Puesta en marcha")
H2("6.1 Primer arranque")
P("Al alimentar el equipo, la lógica de accesos queda operativa de inmediato "
  "(no espera a la red). La pantalla muestra el identificador del equipo "
  "(«SWATID_…»), que es también el nombre de su red WiFi de gestión.")
H2("6.2 Punto de acceso WiFi de gestión")
story.append(tabla(
    ['Parámetro', 'Valor de fábrica'],
    [
        ['Nombre de red (SSID)', 'Identificador del equipo (p. ej. SWATID_A0461CA0ED30)'],
        ['Contraseña', 'admin1234'],
        ['Dirección de la web', 'http://192.168.4.1'],
        ['Duración', '60 s tras el arranque; permanece activo mientras haya alguien conectado'],
    ],
    [50, 120]))
P("El AP se apaga solo transcurrida la ventana sin clientes. Puede "
  "reactivarse desde la web (página Red WiFi → «Activar AP ahora»), por MQTT, "
  "o reiniciando el equipo. También puede configurarse como «siempre activo».")
H2("6.3 Acceso a la web de gestión")
P("Desde el AP: navegar a <b>http://192.168.4.1</b>. Desde la red cableada: "
  "navegar a la IP mostrada en la pantalla LCD (fila 2). "
  "Usuario <b>admin</b>, contraseña <b>admin</b>.")
NOTA("Cambiar la contraseña web en la primera puesta en marcha "
     "(Inicio → Configuración del Dispositivo → Cambiar contraseña) y la "
     "contraseña del punto de acceso (Red WiFi → Cambiar contraseña AP).")

# ============================ 7. WEB: INICIO ===============================
H1("7. Interfaz web — página de inicio")
P("La página de inicio concentra el estado del equipo y su configuración "
  "básica.")
H2("7.1 Estado de red y seguridad")
FIG('web_inicio_1.png', 150, "Figura 7.1 — Cabecera y panel «Estado de Red» en vivo")
P("El panel <b>Estado de Red</b> se actualiza cada 3 segundos y muestra cada "
  "interfaz (Ethernet, WiFi cliente, AP, 4G) y, en la fila MQTT, <b>por qué "
  "interfaz está saliendo la comunicación</b> en ese momento — útil para "
  "validar la conmutación automática.")
FIG('web_inicio_2.png', 150, "Figura 7.2 — Estado de seguridad, teclados y configuración del dispositivo")
P("Desde <b>Estado de Seguridad</b> puede bloquearse temporalmente el acceso "
  "local o deshabilitar la lectura de teclados. En <b>Configuración del "
  "Dispositivo</b> se ajustan nombre, duración del relé, DHCP/IP estática "
  "(con DHCP se muestran la IP y puerta de enlace asignadas) y el modo de "
  "validación (local primero / remoto primero).")
H2("7.2 Acciones y estado")
FIG('web_inicio_3.png', 150, "Figura 7.3 — Modo torno, acciones rápidas y último acceso")
P("Las <b>acciones rápidas</b> permiten activar manualmente los relés, "
  "acceder a las páginas de gestión y reiniciar el equipo. El <b>modo "
  "torno</b> asigna cada teclado a un relé fijo con validación remota.")

# ============================ 8. WEB: PÁGINAS ==============================
H1("8. Interfaz web — páginas de gestión")

H2("8.1 Gestión de códigos locales")
FIG('web_codes.png', 145, "Figura 8.1 — Gestión de códigos (alta, búsqueda, importación CSV, lectura de tags)")
P("Alta de códigos PIN o TAG (hasta 50), asignables a un teclado concreto y "
  "a un relé. Incluye importación/exportación CSV y un modo de <b>lectura de "
  "tags en tiempo real</b> para dar de alta tarjetas pasándolas por el "
  "lector.")

H2("8.2 Códigos remotos")
FIG('web_remote_codes.png', 145, "Figura 8.2 — Códigos remotos con franjas horarias")
P("Caché local de credenciales gestionadas desde la plataforma (hasta 40), "
  "con <b>franjas horarias</b> (hasta 4 por código, con días de la semana). "
  "Se sincronizan por MQTT y persisten ante cortes de alimentación.")

H2("8.3 Entradas digitales")
FIG('web_digital_inputs.png', 145, "Figura 8.3 — Configuración de entradas digitales")
P("Cada entrada puede activar un relé en modo <b>Normal</b> (un pulso activa "
  "el relé el tiempo configurado — pulsador de salida) o <b>Inverso</b> (relé "
  "mantenido mientras la entrada esté activa — automatismos).")

H2("8.4 Red WiFi")
FIG('web_wifi.png', 145, "Figura 8.4 — Página Red WiFi: estado, AP y cliente")
P("Gestión completa del WiFi: estado en vivo, configuración del punto de "
  "acceso (modo, ventana, contraseña, activación inmediata) y conexión como "
  "<b>cliente</b> a la red del edificio, con <b>escaneo de redes</b> "
  "integrado (botón «Usar» para rellenar el SSID).")

H2("8.5 Módem 4G")
FIG('web_gsm.png', 140, "Figura 8.5 — Página 4G: estado real del módem, APN y PIN")
P("Estado completo del módem: registro de red, operador, cobertura con "
  "calidad, tecnología/banda, estado de los <b>datos 4G (PPP)</b> con su IP, "
  "y el último error AT para diagnóstico. El <b>APN</b> funciona en "
  "automático (lo asigna la red) o manual.")
P("<b>PIN de la SIM:</b> el equipo lo envía una sola vez por arranque y por "
  "valor — si es rechazado <b>no reintenta</b> (evita el bloqueo por PUK) y "
  "muestra los intentos restantes. Para SIM M2M se recomienda el botón "
  "«Desbloquear y quitar PIN permanentemente».")

H2("8.6 Actualización OTA")
FIG('web_ota.png', 145, "Figura 8.6 — Actualización de firmware")
P("Actualización de firmware desde el navegador (fichero .bin) o desde "
  "servidor de actualizaciones, con verificación y <b>rollback automático</b> "
  "si el nuevo firmware no arranca.")

H2("8.7 Sincronización de hora")
FIG('web_time_sync.png', 130, "Figura 8.7 — Sincronización de hora con el navegador")
P("Permite poner en hora el equipo desde el navegador. En el A2v3 la hora "
  "se guarda además en el <b>RTC</b> con batería (ver sección 10).")

# ============================ 9. CONECTIVIDAD ==============================
H1("9. Conectividad: prioridad y conmutación automática")
P("El equipo mantiene sus tres interfaces WAN preparadas y encamina la "
  "comunicación MQTT según la prioridad:")
story.append(tabla(
    ['Prioridad', 'Interfaz', 'Uso'],
    [
        ['1', 'Ethernet', 'Preferente siempre que el cable tenga enlace'],
        ['2', 'WiFi cliente', 'Respaldo si cae Ethernet'],
        ['3', '4G (datos PPP)', 'Respaldo si caen Ethernet y WiFi'],
    ],
    [22, 40, 108]))
P("La conmutación es automática en ambos sentidos (avería y recuperación) y "
  "tarda segundos; la web y la pantalla LCD muestran en todo momento la "
  "interfaz en uso. La apertura local de puertas <b>no depende de la red</b> "
  "y no se ve afectada por las conmutaciones.")
OK("Validado en fábrica: desconexión del cable con MQTT activo → reconexión "
   "por 4G en menos de 1 segundo tras detectar la caída; retorno a Ethernet "
   "automático al reconectar el cable.")
H2("9.1 Cómo validar la conmutación")
P("1. En Inicio → Estado de Red, comprobar «MQTT: conectado vía eth».<br/>"
  "2. Desconectar el cable Ethernet (sin WiFi configurada): en la pantalla "
  "LCD la fila E: pasará a «sin enlace» y M: seguirá «ok» — la comunicación "
  "sale por 4G.<br/>"
  "3. Reconectar el cable: el panel volverá a mostrar «vía eth».")

# ============================ 10. HORA Y RTC ===============================
H1("10. Fecha y hora (RTC)")
P("El A2v3 mantiene su propia hora gracias al RTC DS3231 con batería:")
P("• Al arrancar, el equipo toma la hora del RTC — las franjas horarias de "
  "los códigos remotos funcionan aunque no haya ninguna red.<br/>"
  "• Cuando hay Internet, la hora se sincroniza automáticamente (NTP) y el "
  "RTC se corrige periódicamente.<br/>"
  "• También puede sincronizarse desde la plataforma (MQTT) o manualmente "
  "desde el navegador (sección 8.7).")
NOTA("Si la batería del RTC se agota, la página 4G/estado lo indicará y la "
     "hora se recuperará de la red en cada arranque. Sustituir la pila "
     "(CR1220/CR2032 según placa) con el equipo apagado.")

# ============================ 11. MQTT =====================================
H1("11. Integración MQTT (resumen)")
P("El equipo se comunica con la plataforma SWATID mediante MQTT usando su "
  "identificador único. Resumen de temas:")
story.append(tabla(
    ['Tema (topic)', 'Sentido', 'Uso'],
    [
        ['swatidhome/command/&lt;id&gt;/#', 'Plataforma → equipo', 'Comandos y validación remota'],
        ['swatidhome/response/&lt;id&gt;/rx', 'Equipo → plataforma', 'Respuestas a comandos'],
        ['swatidhome/events/&lt;id&gt;/…', 'Equipo → plataforma', 'Eventos de acceso y sistema'],
        ['swatidhome/keepalive/&lt;id&gt;', 'Equipo → plataforma', 'Latido cada 60 s'],
    ],
    [62, 40, 68]))
P("Los comandos (message_type) cubren: relés (0), información (1), sistema "
  "(2), seguridad (3), hora (4), códigos (5), BLE (6) y <b>red (7)</b>: "
  "estado y configuración de WiFi, AP, escaneo, 4G, APN, PIN y RTC. La "
  "referencia completa de la API se facilita a integradores bajo demanda "
  f"({MAIL}).")

# ============================ 12. MANTENIMIENTO ============================
H1("12. Mantenimiento")
story.append(tabla(
    ['Operación', 'Dónde', 'Descripción'],
    [
        ['Actualizar firmware', 'Web → OTA', 'Subir .bin oficial; rollback automático si falla'],
        ['Reiniciar equipo', 'Web → Inicio → Reiniciar', 'Reinicio ordenado (los accesos se recuperan en segundos)'],
        ['Reset de fábrica', 'Web → Inicio → Resetear', 'Borra configuración y códigos (¡irreversible!)'],
        ['Re-detectar SIM/módem', 'Web → 4G', 'Reinicia el módulo 4G sin cortar la alimentación'],
        ['Cambio de contraseñas', 'Inicio / Red WiFi', 'Contraseña web y contraseña del AP'],
    ],
    [40, 42, 88]))

# ============================ 13. PROBLEMAS ================================
H1("13. Resolución de problemas")
story.append(tabla(
    ['Síntoma', 'Causa probable', 'Solución'],
    [
        ['No veo la red WiFi del equipo',
         'La ventana del AP (60 s) expiró sin conexiones',
         'Reiniciar el equipo y conectar en el primer minuto, o activar el AP desde la web/plataforma; puede configurarse «siempre activo»'],
        ['4G: «Sin módulo»',
         'Módulo mal insertado o alimentación insuficiente',
         'Revisar el módulo en su zócalo y la fuente de alimentación; botón re-detectar'],
        ['4G: «Sin SIM» con SIM insertada',
         'SIM sin llegar al tope o con adaptador',
         'Apagar, reinsertar la SIM sin adaptador hasta el tope; después «Reiniciar módulo y re-detectar SIM»'],
        ['4G: «PIN requerido»',
         'La SIM tiene PIN activado (habitual en M2M)',
         'Introducir el PIN en la página 4G (un solo intento por valor); recomendado «quitar PIN permanentemente»'],
        ['4G: cobertura «Sin señal» (CSQ 99)',
         'Antena ausente, en conector AUX/GPS, o antena WiFi',
         'Conectar antena LTE en el conector MAIN del módulo; comprobar cobertura con un móvil'],
        ['4G: «REGISTRO DENEGADO»',
         'Perfil M2M sin aprovisionar o roaming no permitido',
         'Contactar con el operador de la SIM'],
        ['MQTT sin conexión con red operativa',
         'Broker inaccesible',
         'El equipo reintenta automáticamente; los accesos locales no se ven afectados'],
        ['Hora incorrecta sin red',
         'Batería del RTC agotada',
         'Sustituir la pila; la hora se resincroniza sola al volver la red'],
        ['Olvidé la contraseña web',
         '—',
         'Contactar con soporte técnico'],
    ],
    [38, 52, 80]))

# ============================ 14. SOPORTE ==================================
H1("14. Soporte y contacto")
story.append(Spacer(1, 4*mm))
logo2 = Image(os.path.join(IMG, 'logo_swatid.png'), width=45*mm, height=45*mm)
logo2.hAlign = 'CENTER'
story.append(logo2)
story.append(Spacer(1, 4*mm))
ct = Table([
    ['Empresa', EMPRESA],
    ['CIF', CIF],
    ['Teléfono', TEL],
    ['Correo electrónico', MAIL],
    ['Web', WEB],
], colWidths=[50*mm, 90*mm])
ct.setStyle(TableStyle([
    ('FONTNAME', (0, 0), (0, -1), 'Helvetica-Bold'),
    ('FONTSIZE', (0, 0), (-1, -1), 10.5),
    ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#bbbbbb')),
    ('BACKGROUND', (0, 0), (0, -1), colors.HexColor('#eef4fb')),
    ('TOPPADDING', (0, 0), (-1, -1), 6),
    ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
    ('LEFTPADDING', (0, 0), (-1, -1), 8),
]))
ct.hAlign = 'CENTER'
story.append(ct)
story.append(Spacer(1, 8*mm))
P("© 2026 Smart World And Things SLU. Todos los derechos reservados. "
  "Las características descritas corresponden al firmware indicado en "
  "portada y pueden ampliarse en versiones posteriores. Este documento no "
  "podrá reproducirse sin autorización expresa de Smart World And Things SLU.")

doc.multiBuild(story)
print("PDF generado:", OUT)
