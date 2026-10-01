#!/usr/bin/env python3
"""Build docs/gbareader-full-controls.pdf from the hand-written manual
docs/gbareader-full-controls.md (headings, paragraphs, lists, tables,
screenshots and links). Requires reportlab, pypdf and Pillow.
Checks afterwards that every line of the Markdown text is in the PDF.
"""
from pathlib import Path
import re
from xml.sax.saxutils import escape
from reportlab import rl_config
from reportlab.lib.colors import HexColor
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.platypus import (Image, KeepTogether, ListFlowable, ListItem, PageBreak, Paragraph,
                                SimpleDocTemplate, Spacer, Table, TableStyle)
from pypdf import PdfReader

rl_config.invariant = 1  # Rebuilding an unchanged manual gives identical bytes.

root = Path(__file__).resolve().parents[1]
source = root / 'docs/gbareader-full-controls.md'
out = root / 'docs/gbareader-full-controls.pdf'
text = source.read_text(encoding='utf-8')

BLUE = HexColor('#254d83')
BAR = HexColor('#aad2ff')   # the app's selection bar
GREY = HexColor('#526070')
styles = getSampleStyleSheet()
title = ParagraphStyle('ManualTitle', parent=styles['Title'], textColor=BLUE, fontSize=26, leading=30, spaceAfter=2)
subtitle = ParagraphStyle('ManualSubtitle', parent=styles['Normal'], textColor=GREY, fontSize=13, leading=16,
                          alignment=1, spaceAfter=14)
heading = ParagraphStyle('ManualHeading', parent=styles['Heading2'], textColor=BLUE, fontSize=15, leading=19,
                         spaceBefore=12, spaceAfter=6)
body = ParagraphStyle('ManualBody', parent=styles['Normal'], fontName='Helvetica', fontSize=10.5, leading=15,
                      spaceAfter=7)
cell = ParagraphStyle('ManualCell', parent=body, fontSize=9.5, leading=12.5, spaceAfter=0)
cell_head = ParagraphStyle('ManualCellHead', parent=cell, fontName='Helvetica-Bold')


def inline(t):
    """`keys` in bold, URLs as links."""
    parts = re.split(r'(`[^`]+`|https?://\S+)', t)
    html = []
    for p in parts:
        if p.startswith('`') and p.endswith('`'):
            html.append('<b>' + escape(p[1:-1]) + '</b>')
        elif p.startswith('http'):
            url = p.rstrip('.,')
            html.append(f'<link href="{escape(url)}" color="#254d83"><u>{escape(url)}</u></link>'
                        + escape(p[len(url):]))
        else:
            html.append(escape(p))
    return ''.join(html)


def table(lines):
    rows = [[c.strip() for c in l.strip().strip('|').split('|')] for l in lines if not re.match(r'^\|\s*-', l)]
    data = [[Paragraph(inline(c), cell_head if r == 0 else cell) for c in row] for r, row in enumerate(rows)]
    n = len(rows[0])
    widths = {3: [0.2, 0.22, 0.58], 2: [0.33, 0.67]}[n]
    t = Table(data, colWidths=[w * 499 for w in widths], repeatRows=1)
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BAR),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LINEBELOW', (0, 0), (-1, -1), 0.4, HexColor('#c8d3df')),
        ('TOPPADDING', (0, 0), (-1, -1), 4), ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
    ]))
    return [KeepTogether([t]) if len(rows) <= 12 else t, Spacer(1, 8)]


flow = []
for block in text.strip().split('\n\n'):
    lines = block.splitlines()
    if block.startswith('# '):
        flow.append(Paragraph(escape(block[2:]), title))
    elif block == '## Full controls':
        flow.append(Paragraph('Full controls', subtitle))
    elif block.startswith('## '):
        if block == '## Credits and licenses':
            flow.append(PageBreak())  # Credits get a page of their own.
        flow.append(Paragraph(escape(block[3:]), heading))
    elif block.startswith('!['):
        m = re.match(r'!\[([^\]]*)\]\(([^)]+)\)', block)
        img = Image(str(root / 'docs' / m.group(2)), width=240, height=160)
        # A thin frame like a GBA screen edge, so the white screen stands out.
        framed = Table([[img]], colWidths=[244], rowHeights=[164])
        framed.setStyle(TableStyle([('BOX', (0, 0), (-1, -1), 2, HexColor('#3a3f4a')),
                                    ('ALIGN', (0, 0), (-1, -1), 'CENTER'), ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
                                    ('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (-1, -1), 0),
                                    ('TOPPADDING', (0, 0), (-1, -1), 0), ('BOTTOMPADDING', (0, 0), (-1, -1), 0)]))
        flow.append(KeepTogether([Spacer(1, 2), framed, Spacer(1, 10)]))
    elif block.startswith('|'):
        flow += table(lines)
    elif block.startswith('- ') or re.match(r'^\d+\. ', block):
        numbered = not block.startswith('- ')
        items = [ListItem(Paragraph(inline(re.sub(r'^(- |\d+\. )', '', l)), body), leftIndent=14) for l in lines]
        flow.append(ListFlowable(items, bulletType='1' if numbered else 'bullet', start=1 if numbered else '•',
                                 leftIndent=14, bulletFontSize=10.5 if numbered else 9))
    else:
        flow.append(Paragraph(inline(' '.join(lines)), body))


# A heading always starts on the same page as what follows it.
kept = []
for f in flow:
    if kept and isinstance(kept[-1], Paragraph) and kept[-1].style is heading:
        kept[-1] = KeepTogether([kept[-1], f])
    else:
        kept.append(f)
flow = kept


def footer(canvas, doc):
    canvas.setFont('Helvetica', 9)
    canvas.setFillColor(GREY)
    canvas.drawString(48, 25, 'gbareader V3.1 · Full controls · github.com/Xinon232/gbareader')
    canvas.drawRightString(547, 25, str(doc.page))


SimpleDocTemplate(str(out), pagesize=(595, 842), leftMargin=48, rightMargin=48, topMargin=40, bottomMargin=46,
                  title='gbareader V3.1 - Full controls', author='Halim Jarrar').build(
    flow, onFirstPage=footer, onLaterPages=footer)

# Every line of the manual must be in the PDF.
pdf = PdfReader(out)
visible = re.sub(r'\s+', '', ''.join(page.extract_text() for page in pdf.pages))
for block in text.strip().split('\n\n'):
    if block.startswith('!['):
        continue
    for line in block.splitlines():
        if re.match(r'^\|\s*-', line):
            continue
        for piece in (line.strip('|').split('|') if line.startswith('|') else [line]):
            plain = re.sub(r'^(#+ |- |\d+\. )', '', piece.strip()).replace('`', '')
            assert re.sub(r'\s+', '', plain) in visible, ('PDF text omitted', plain)
for required in ['https://github.com/Xinon232/gbareader', 'Import into /gbareader?', 'Continue without saving?',
                 'Halim Jarrar', 'gba@halim-jarrar.de', 'halimj.itch.io', 'CC BY 4.0', 'GPL 3.0 or later']:
    assert re.sub(r'\s+', '', required) in visible, required
print(f'{out}: {len(pdf.pages)} pages; every manual line verified in the PDF')
