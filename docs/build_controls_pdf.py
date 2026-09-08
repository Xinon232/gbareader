#!/usr/bin/env python3
"""Build the full-controls PDF from the current README controls section.
Requires reportlab and pypdf. Run from any directory.
"""
from pathlib import Path
import re
from xml.sax.saxutils import escape
from reportlab.platypus import SimpleDocTemplate,Paragraph,Spacer,PageBreak,Flowable
from reportlab.lib.styles import getSampleStyleSheet,ParagraphStyle
from reportlab.lib.colors import HexColor

from pypdf import PdfReader
root=Path(__file__).resolve().parents[1]
text=(root/'README.md').read_text().split('## Controls\n',1)[1].split('## Hardware and files',1)[0]
styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyControls',fontName='Helvetica',fontSize=11,leading=16,spaceAfter=8))
styles['Heading1'].textColor=styles['Heading2'].textColor=HexColor('#254d83')
flow: list[Flowable]=[Paragraph('gbareader V1.0',styles['Title']),Paragraph('Full controls',styles['Heading1'])]
def formatted(t):
 return re.sub(r'`([^`]+)`',r'<b>\1</b>',escape(t))
for block in text.strip().split('\n\n'):
 if block.startswith('See ['):continue
 if block.startswith('### '):
  if block=='### Settings':flow.append(PageBreak())
  flow.append(Paragraph(block[4:],styles['Heading2']))
 elif block.startswith('- '):
  for line in block.splitlines():flow.append(Paragraph(formatted(line[2:]),styles['BodyControls'],bulletText='\u2022'))
 else:flow.append(Paragraph(formatted(block.replace('\n',' ')),styles['BodyControls']))
flow += [Spacer(1,14),Paragraph('Files, saving and compatibility',styles['Heading2'])]
for t in [
 'TXT bookmarks are stored in a trailing footer. EPUB bookmarks and normalized text caches stay inside the EPUB as ZIP members. No GBA .sav or permanent settings sidecar is required. Initial EPUB cache preparation writes initial state automatically. Later changes to reading position, back history and settings require a successful Reader Start save.',
 'An EPUB may show Preparing cache... on first opening. Cache preparation writes inside the EPUB automatically; it is not a text-editing or export feature. Wait for storage operations to finish before switching off. If a save fails, keep the book open and retry; do not assume the new place was stored.',
 'Supported files use UTF-8. EPUB support is text-only: no DRM, images, CSS presentation, scripts, embedded fonts, audio or video. Arabic is not supported. Long names are clipped on screen, not changed on disk. Missing storage or a missing/empty /gbareader folder never opens a demo or scans the card root.',
 'Requires compatible Supercard SD hardware. Emulator UI checks do not prove physical SD-card operation. Keep backups of your books.',
]:flow.append(Paragraph(escape(t),styles['BodyControls']))
flow += [Spacer(1,10),Paragraph('Credits',styles['Heading2']),Paragraph('Made by Halim Jarrar · © 2026<br/>halim-jarrar.de · monday@halim-jarrar.de',styles['BodyControls']),Paragraph('Based on gba-vocab-trainer-CC v0.2.5, with SuperFW fonts and SD/FatFS integration. SuperFW font-rendering copyright © 2024 David Guillen Fandos. GPL-3.0-or-later; miniz is MIT-licensed. Full notices remain in the source repository.',styles['BodyControls'])]
out=root/'docs/gbareader-full-controls.pdf';out.parent.mkdir(exist_ok=True)
def footer(canvas,doc):
 canvas.setFont('Helvetica',9);canvas.setFillColor(HexColor('#526070'));canvas.drawString(42,25,'gbareader · Full controls · Halim Jarrar');canvas.drawRightString(553,25,str(doc.page))
SimpleDocTemplate(str(out),pagesize=(595,842),leftMargin=48,rightMargin=48,topMargin=38,bottomMargin=46,title='gbareader V1.0 — Full controls',author='Halim Jarrar').build(flow,onFirstPage=footer,onLaterPages=footer)
pdf=PdfReader(out); joined='\n'.join(p.extract_text() for p in pdf.pages)
for required in ['Read TXT and EPUB','/gbareader','Select','Start','Left','Right','Up','Down','previous page','shoulder','Settings','Save failed','Halim Jarrar','no text editing']:
 assert required.lower() in joined.lower(),required
print(f'{out}: {len(pdf.pages)} pages; all required descriptions and current controls extracted')
