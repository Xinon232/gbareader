#!/usr/bin/env python3
"""Build the full-controls PDF from the current README controls section.
Requires reportlab and pypdf. Run from any directory.
"""
from pathlib import Path
import re
from xml.sax.saxutils import escape
from reportlab.platypus import SimpleDocTemplate,Paragraph,PageBreak,Flowable
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
flow: list[Flowable]=[Paragraph('gbareader V3.0',styles['Title']),Paragraph('Full controls',styles['Heading1'])]
def formatted(t):
 return re.sub(r'`([^`]+)`',r'<b>\1</b>',escape(t))
for block in text.strip().split('\n\n'):
 if block.startswith('See ['):continue
 if block.startswith('### '):
  if block in ('### Settings','### Migration and failed saves'):flow.append(PageBreak())
  flow.append(Paragraph(block[4:],styles['Heading2']))
 elif block.startswith('- '):
  for line in block.splitlines():flow.append(Paragraph(formatted(line[2:]),styles['BodyControls'],bulletText='\u2022'))
 else:flow.append(Paragraph(formatted(block.replace('\n',' ')),styles['BodyControls']))
flow += [PageBreak(), Paragraph('Credits and font licenses', styles['Heading1']),
         Paragraph('Made by Halim Jarrar<br/>(C) 2026<br/>halim-jarrar.de<br/>monday@halim-jarrar.de', styles['BodyControls'])]
notices = [
 'Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, CC BY 4.0. Converted actual contextual GSUB forms to native 11px monochrome ROM glyphs; this modified representation is not author endorsement. Source: https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1 — License: https://creativecommons.org/licenses/by/4.0/.',
 'SuperFW fonts, renderer and SD foundation: David Guillen Fandos, © 2024–2025, GPL v3 or later. https://superfw.davidgf.net/. The original SuperFW and supplemental font packs are unchanged.',
 'UNSCII by Viznut: the included unscii-16-full-derived font pack is GPL, not wholly public domain. It incorporates GNU Unifont and public-domain Fixedsys Excelsior glyphs. https://viznut.fi/unscii/.',
 'GNU Unifont / Hangul: Roman Czyborra, Paul Hardy and Unifont contributors. GPL v2 or later with the GNU font embedding exception. https://www.unifoundry.com/unifont/. The inherited exact font snapshot version is not recorded; no claim of eligibility for a newer alternative OFL license is made.',
 'UI font: the 5x7 font from gbamp3 v0.9.8 by Halim Jarrar, part of this project family (GPL v3 or later). https://github.com/Xinon232/gbamp3.',
 'Butano engine and cursor font: Gustavo Valiente, zlib license. Built with devkitARM / devkitPro. FatFs © 2022 ChaN, permissive redistribution terms in src/ff.c. Framework dependency notices remain in the Butano source distribution.',
 'miniz: MIT license; © 2010–2014 Rich Geldreich and Tenacious Software LLC; © 2013–2014 RAD Game Tools and Valve Software. Complete notice in third_party/miniz/LICENSE.',
 'Based on gba-vocab-trainer-CC v0.2.5. Project license: GPL v3 or later. Full source and notices: https://github.com/Xinon232/gbareader. In-ROM Credits keeps the personal block on page one and third-party credits on the following six pages. Left/Right changes pages; B or Start closes.'
]
for notice in notices: flow.append(Paragraph(escape(notice), styles['BodyControls']))
out=root/'docs/gbareader-full-controls.pdf';out.parent.mkdir(exist_ok=True)
def footer(canvas,doc):
 canvas.setFont('Helvetica',9);canvas.setFillColor(HexColor('#526070'));canvas.drawString(42,25,'gbareader · Full controls · Halim Jarrar');canvas.drawRightString(553,25,str(doc.page))
SimpleDocTemplate(str(out),pagesize=(595,842),leftMargin=48,rightMargin=48,topMargin=38,bottomMargin=46,title='gbareader V3.0 — Full controls',author='Halim Jarrar').build(flow,onFirstPage=footer,onLaterPages=footer)
pdf=PdfReader(out)
joined='\n'.join('\n'.join(line for line in page.extract_text().splitlines()
    if line not in ('gbareader · Full controls · Halim Jarrar',str(index)))
    for index,page in enumerate(pdf.pages,1))
for required in ['Read TXT and EPUB','/gbareader','Select','Start','Left','Right','Up','Down','previous page','shoulder','Settings','Save failed','Halim Jarrar','no text editing','L/R on startup','SETTINGS0.DAT','last successfully','Settings save failed']:
 assert required.lower() in joined.lower(),required
instructions='\n\n'.join(b for b in text.strip().split('\n\n') if not b.startswith('See ['))
markdown='# gbareader V3.0\n\n## Full controls\n\n'+instructions+'\n\n## Credits and font licenses\n\nMade by Halim Jarrar\n(C) 2026\nhalim-jarrar.de\nmonday@halim-jarrar.de\n\n'+'\n\n'.join(notices)+'\n'
(root/'docs/gbareader-full-controls.md').write_text(markdown,encoding='utf-8')
def normalized(s):
 return re.sub(r'\s+','',s.replace('`','').replace('\u2022',''))
visible=normalized(joined)
for block in markdown.strip().split('\n\n'):
 for line in (block.splitlines() if block.startswith('- ') else [block]):
  plain=re.sub(r'^#+\s*','',line)
  if plain.startswith('- '):plain=plain[2:]
  assert normalized(plain) in visible,('PDF text omitted',plain)
print(f'{out}: {len(pdf.pages)} pages; complete Markdown/PDF block parity verified')
