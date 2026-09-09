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
from reportlab import rl_config

rl_config.invariant = 1  # Rebuilding unchanged controls produces identical bytes.

from pypdf import PdfReader
root=Path(__file__).resolve().parents[1]
text=(root/'README.md').read_text().split('## Controls\n',1)[1].split('## Hardware and files',1)[0]
styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyControls',fontName='Helvetica',fontSize=11,leading=16,spaceAfter=8))
styles['Heading1'].textColor=styles['Heading2'].textColor=HexColor('#254d83')
flow: list[Flowable]=[Paragraph('gbareader V1.3',styles['Title']),Paragraph('Full controls',styles['Heading1'])]
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
 'TXT bookmarks are stored in a trailing footer. EPUB bookmarks and normalized text caches stay inside the EPUB as ZIP members. No GBA .sav or permanent settings sidecar is required. Initial EPUB cache preparation and first-time Arabic activation save state automatically. Later position, back history and settings changes require a successful Reader Start save.',
 'An EPUB may show Preparing cache... on first opening. Cache preparation writes inside the EPUB automatically; it is not a text-editing or export feature. Wait for storage operations to finish before switching off. If a save fails, keep the book open and retry; do not assume the new place was stored.',
 'Supported files use UTF-8. EPUB support is text-only: no DRM, images, CSS presentation, scripts, embedded fonts, audio or video. Arabic uses native Ghoulam joining and limited per-line RTL; harakat are hidden only on screen. Full Persian/Urdu and full Unicode bidi are not supported. Long names are clipped on screen, not changed on disk. Missing storage or a missing/empty /gbareader folder never opens a demo or scans the card root.',
 'Requires compatible Supercard SD hardware. Emulator UI checks do not prove physical SD-card operation. Keep backups of your books.',
]:flow.append(Paragraph(escape(t),styles['BodyControls']))
flow += [PageBreak(), Paragraph('Credits and font licenses', styles['Heading1']),
         Paragraph('Made by Halim Jarrar<br/>(C) 2026<br/>halim-jarrar.de<br/>monday@halim-jarrar.de', styles['BodyControls'])]
for notice in [
 'Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, CC BY 4.0. Converted actual contextual GSUB forms to native 11px monochrome ROM glyphs; this modified representation is not author endorsement. Source: https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1 — License: https://creativecommons.org/licenses/by/4.0/.',
 'SuperFW fonts, renderer and SD foundation: David Guillen Fandos, © 2024–2025, GPL v3 or later. https://superfw.davidgf.net/. The original SuperFW and supplemental font packs are unchanged.',
 'UNSCII by Viznut: the included unscii-16-full-derived font pack is GPL, not wholly public domain. It incorporates GNU Unifont and public-domain Fixedsys Excelsior glyphs. https://viznut.fi/unscii/.',
 'GNU Unifont / Hangul: Roman Czyborra, Paul Hardy and Unifont contributors. GPL v2 or later with the GNU font embedding exception. https://www.unifoundry.com/unifont/. The inherited exact font snapshot version is not recorded; no claim of eligibility for a newer alternative OFL license is made.',
 'Butano engine and UI font: Gustavo Valiente, zlib license. Built with devkitARM / devkitPro. FatFs © 2022 ChaN, permissive redistribution terms in src/ff.c. Framework dependency notices remain in the Butano source distribution.',
 'miniz: MIT license; © 2010–2014 Rich Geldreich and Tenacious Software LLC; © 2013–2014 RAD Game Tools and Valve Software. Complete notice in third_party/miniz/LICENSE.',
 'Based on gba-vocab-trainer-CC v0.2.5. Project license: GPL v3 or later. Full source and notices: https://github.com/Xinon232/gbareader. In-ROM Credits keeps the personal block on page one and third-party credits on the following six pages. Left/Right changes pages; B or Start closes.'
]: flow.append(Paragraph(escape(notice), styles['BodyControls']))
out=root/'docs/gbareader-full-controls.pdf';out.parent.mkdir(exist_ok=True)
def footer(canvas,doc):
 canvas.setFont('Helvetica',9);canvas.setFillColor(HexColor('#526070'));canvas.drawString(42,25,'gbareader · Full controls · Halim Jarrar');canvas.drawRightString(553,25,str(doc.page))
SimpleDocTemplate(str(out),pagesize=(595,842),leftMargin=48,rightMargin=48,topMargin=38,bottomMargin=46,title='gbareader V1.3 — Full controls',author='Halim Jarrar').build(flow,onFirstPage=footer,onLaterPages=footer)
pdf=PdfReader(out); joined='\n'.join(p.extract_text() for p in pdf.pages)
for required in ['Read TXT and EPUB','/gbareader','Select','Start','Left','Right','Up','Down','previous page','shoulder','Settings','Save failed','Halim Jarrar','no text editing']:
 assert required.lower() in joined.lower(),required
print(f'{out}: {len(pdf.pages)} pages; all required descriptions and current controls extracted')
