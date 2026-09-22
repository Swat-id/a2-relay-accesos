#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Genera el PDF del Manual de Integración MQTT desde MANUAL-INTEGRACION-MQTT.md
Uso: /tmp/pdfenv/bin/python build_mqtt_manual.py
"""
import os, re
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph,
                                Spacer, Image, Table, TableStyle, PageBreak,
                                NextPageTemplate, Preformatted, KeepTogether)
from reportlab.platypus.tableofcontents import TableOfContents

BASE = os.path.dirname(os.path.abspath(__file__))
MD = os.path.join(BASE, 'MANUAL-INTEGRACION-MQTT.md')
LOGO = os.path.join(BASE, '..', 'v5.0.1', 'manual', 'img', 'logo_swatid.png')
OUT = os.path.join(BASE, 'MANUAL-INTEGRACION-MQTT-v1.0.pdf')

AZUL = colors.HexColor('#1d5fa7')
AZUL_OSCURO = colors.HexColor('#35424a')
GRIS = colors.HexColor('#6c757d')

EMPRESA = "Smart World And Things SLU"
VERSION = "v1.0"
FECHA = "Septiembre 2026"
FIRMWARE = "v5.0.2"

ss = getSampleStyleSheet()
st_body = ParagraphStyle('Cuerpo', parent=ss['Normal'], fontSize=9.5, leading=13.5,
                         alignment=TA_JUSTIFY, spaceAfter=5)
st_li = ParagraphStyle('Li', parent=st_body, leftIndent=14, bulletIndent=4)
st_h1 = ParagraphStyle('H1x', parent=ss['Heading1'], fontSize=15, leading=19,
                       textColor=AZUL, spaceBefore=16, spaceAfter=7)
st_h2 = ParagraphStyle('H2x', parent=ss['Heading2'], fontSize=12, leading=15,
                       textColor=AZUL_OSCURO, spaceBefore=11, spaceAfter=5)
st_code = ParagraphStyle('Code', fontName='Courier', fontSize=7.6, leading=9.6,
                         backColor=colors.HexColor('#f4f4f4'),
                         borderColor=colors.HexColor('#d0d0d0'), borderWidth=0.6,
                         borderPadding=5, leftIndent=4, spaceBefore=4, spaceAfter=7)

TSTYLE = TableStyle([
    ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#f2f2f2')),
    ('TEXTCOLOR', (0, 0), (-1, 0), AZUL_OSCURO),
    ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
    ('FONTSIZE', (0, 0), (-1, -1), 8),
    ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#cccccc')),
    ('VALIGN', (0, 0), (-1, -1), 'TOP'),
    ('LEFTPADDING', (0, 0), (-1, -1), 4.5),
    ('RIGHTPADDING', (0, 0), (-1, -1), 4.5),
    ('TOPPADDING', (0, 0), (-1, -1), 3),
    ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
])


def inline(txt):
    """Markdown inline → markup reportlab (escapando XML)."""
    txt = txt.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    txt = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', txt)
    txt = re.sub(r'`([^`]+)`', r'<font face="Courier" size="8">\1</font>', txt)
    txt = re.sub(r'\[(.+?)\]\((.+?)\)', r'\1', txt)   # enlaces → texto plano
    return txt


class ManualDoc(BaseDocTemplate):
    def __init__(self, filename, **kw):
        super().__init__(filename, pagesize=A4, **kw)
        f_cover = Frame(15*mm, 15*mm, A4[0]-30*mm, A4[1]-30*mm, id='cover')
        f_body = Frame(17*mm, 19*mm, A4[0]-34*mm, A4[1]-38*mm, id='body')
        self.addPageTemplates([
            PageTemplate(id='Cover', frames=[f_cover]),
            PageTemplate(id='Body', frames=[f_body], onPage=self._decorate),
        ])

    def _decorate(self, canv, doc):
        canv.saveState()
        canv.setStrokeColor(AZUL); canv.setLineWidth(0.8)
        canv.line(17*mm, A4[1]-13*mm, A4[0]-17*mm, A4[1]-13*mm)
        canv.setFont('Helvetica', 8); canv.setFillColor(GRIS)
        canv.drawString(17*mm, A4[1]-11*mm, "SWATID A2/A2v3 — Manual de Integración MQTT")
        canv.drawRightString(A4[0]-17*mm, A4[1]-11*mm, EMPRESA)
        canv.line(17*mm, 13*mm, A4[0]-17*mm, 13*mm)
        canv.drawString(17*mm, 8.8*mm, f"Manual {VERSION} · {FECHA} · Firmware {FIRMWARE}")
        canv.drawCentredString(A4[0]/2, 8.8*mm, "www.swat-id.com")
        canv.drawRightString(A4[0]-17*mm, 8.8*mm, f"Página {doc.page}")
        canv.restoreState()

    def afterFlowable(self, fl):
        if isinstance(fl, Paragraph) and fl.style.name in ('H1x', 'H2x'):
            text = fl.getPlainText()
            level = 0 if fl.style.name == 'H1x' else 1
            key = f"h-{self.page}-{abs(hash(text)) % 99991}"
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(text, key, level=level, closed=False)
            self.notify('TOCEntry', (level, text, self.page, key))


def parse_md(path):
    """Convierte el subconjunto de Markdown del manual en flowables."""
    story = []
    lines = open(path, encoding='utf-8').read().splitlines()
    i = 0
    in_index = False
    while i < len(lines):
        ln = lines[i]

        # Saltar el índice manual (el PDF genera el suyo)
        if ln.startswith('## Índice'):
            in_index = True; i += 1; continue
        if in_index:
            if ln.startswith('## '):
                in_index = False
            else:
                i += 1; continue

        if ln.startswith('# '):
            i += 1; continue                       # título → portada
        if ln.startswith('### '):
            story.append(Paragraph(inline(ln[4:]), st_h2)); i += 1; continue
        if ln.startswith('## '):
            story.append(Paragraph(inline(ln[3:]), st_h1)); i += 1; continue
        if ln.startswith('```'):
            buf = []
            i += 1
            while i < len(lines) and not lines[i].startswith('```'):
                buf.append(lines[i]); i += 1
            i += 1
            story.append(Preformatted('\n'.join(buf), st_code))
            continue
        if ln.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].startswith('|'):
                cells = [c.strip() for c in lines[i].strip('|').split('|')]
                if not all(re.fullmatch(r':?-{2,}:?', c) for c in cells):
                    rows.append(cells)
                i += 1
            ncols = max(len(r) for r in rows)
            width = (A4[0] - 34*mm)
            # primera columna más estrecha en tablas de 2-3 columnas
            if ncols == 2: widths = [width*0.32, width*0.68]
            elif ncols == 3: widths = [width*0.26, width*0.30, width*0.44]
            else: widths = [width/ncols]*ncols
            cellstyle = ParagraphStyle('cell', parent=ss['Normal'], fontSize=8, leading=10.5)
            headstyle = ParagraphStyle('cellh', parent=cellstyle, fontName='Helvetica-Bold')
            data = []
            for ri, r in enumerate(rows):
                r = r + [''] * (ncols - len(r))
                stl = headstyle if ri == 0 else cellstyle
                data.append([Paragraph(inline(c), stl) for c in r])
            t = Table(data, colWidths=widths, repeatRows=1)
            t.setStyle(TSTYLE)
            story.append(t)
            story.append(Spacer(1, 4))
            continue
        if re.match(r'^\s*[-*] ', ln) or re.match(r'^\s*\d+\. ', ln):
            txt = re.sub(r'^\s*([-*]|\d+\.) ', '', ln)
            # continuación de línea envuelta
            while i + 1 < len(lines) and lines[i+1].startswith('  ') and \
                  not re.match(r'^\s*([-*]|\d+\.) ', lines[i+1]) and lines[i+1].strip():
                i += 1; txt += ' ' + lines[i].strip()
            story.append(Paragraph('• ' + inline(txt), st_li))
            i += 1; continue
        if ln.strip() in ('---', ''):
            i += 1; continue
        if ln.startswith('**') and ln.rstrip().endswith('**') and ln.count('**') == 2:
            story.append(Paragraph(inline(ln), st_body)); i += 1; continue

        # Párrafo (unir líneas envueltas)
        txt = ln
        while i + 1 < len(lines) and lines[i+1].strip() and \
              not re.match(r'^(#|\||```|\s*[-*] |\s*\d+\. |---)', lines[i+1]):
            i += 1; txt += ' ' + lines[i].strip()
        story.append(Paragraph(inline(txt), st_body))
        i += 1
    return story


doc = ManualDoc(OUT, title="Manual de Integración MQTT SWATID A2/A2v3", author=EMPRESA)
story = []

# Portada
story.append(Spacer(1, 16*mm))
logo = Image(LOGO, width=62*mm, height=62*mm); logo.hAlign = 'CENTER'
story.append(logo)
story.append(Spacer(1, 5*mm))
story.append(Paragraph("MANUAL DE INTEGRACIÓN MQTT",
    ParagraphStyle('t1', fontSize=23, leading=28, alignment=TA_CENTER,
                   fontName='Helvetica-Bold', textColor=AZUL_OSCURO)))
story.append(Spacer(1, 3*mm))
story.append(Paragraph("Controladora de Accesos <b>SWATID A2 / A2v3</b>",
    ParagraphStyle('t2', fontSize=15, leading=19, alignment=TA_CENTER, textColor=AZUL)))
story.append(Paragraph("Mensajería completa: eventos emitidos y comandos aceptados",
    ParagraphStyle('t3', fontSize=11, leading=15, alignment=TA_CENTER, textColor=GRIS)))
story.append(Spacer(1, 12*mm))
vt = Table([
    ['Versión del manual', VERSION],
    ['Fecha', FECHA],
    ['Firmware de referencia', f"{FIRMWARE} (base, -4G, -BLE, -S3)"],
    ['Audiencia', 'Integradores de plataforma / backend'],
], colWidths=[55*mm, 75*mm])
vt.setStyle(TableStyle([
    ('FONTNAME', (0, 0), (0, -1), 'Helvetica-Bold'),
    ('FONTSIZE', (0, 0), (-1, -1), 10),
    ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#bbbbbb')),
    ('BACKGROUND', (0, 0), (0, -1), colors.HexColor('#eef4fb')),
    ('TOPPADDING', (0, 0), (-1, -1), 5), ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
    ('LEFTPADDING', (0, 0), (-1, -1), 8),
]))
vt.hAlign = 'CENTER'
story.append(vt)
story.append(Spacer(1, 18*mm))
story.append(Paragraph(
    f"<b>{EMPRESA}</b> · CIF B40599813<br/>Tel. +34 633 448 427 · info@swat-id.com · www.swat-id.com",
    ParagraphStyle('emp', fontSize=10, leading=15, alignment=TA_CENTER, textColor=AZUL_OSCURO)))
story.append(NextPageTemplate('Body'))
story.append(PageBreak())

# Índice
story.append(Paragraph("Índice", ParagraphStyle('TOCTitle', parent=st_h1, spaceBefore=0)))
toc = TableOfContents()
toc.levelStyles = [
    ParagraphStyle('TOC1', fontSize=10.5, leading=15, fontName='Helvetica-Bold',
                   textColor=AZUL_OSCURO, leftIndent=2),
    ParagraphStyle('TOC2', fontSize=9, leading=12.5, leftIndent=14),
]
story.append(toc)
story.append(PageBreak())

story.extend(parse_md(MD))

doc.multiBuild(story)
print("PDF generado:", OUT)
