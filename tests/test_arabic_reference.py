#!/usr/bin/env python3
"""Offline reference comparison: actual production glyph IDs vs HarfBuzz/TTF."""
from pathlib import Path
import itertools, subprocess, re, json, argparse
import uharfbuzz as hb
p=argparse.ArgumentParser();p.add_argument('font');p.add_argument('probe');a=p.parse_args()
f=hb.Font(hb.Face(Path(a.font).read_bytes()));f.scale=(100,100)
letters='ءآأؤإئابتثجحخدذرزسشصضطظعغـفقكلمنهويىة'
words=list(letters)+[''.join(x) for x in itertools.product(letters,repeat=2)]
words+=['ب'+x+'ب' for x in letters]+['لا','لأ','لإ','لآ','بلا','بَلَا','لالا','للا','بلاا','الله']
for file in (Path(__file__).parent/'fixtures/arabic').glob('sample*.txt'):
 words+=re.findall('[\u0621-\u064a]+',file.read_text())
# Marks are intentionally transparent/omitted by the display policy.
clean=[''.join(c for c in w if not '\u064b'<=c<='\u065f') for w in words]
result=subprocess.run([a.probe],input='\n'.join(words)+'\n',text=True,capture_output=True,check=True)
rows=result.stdout.splitlines();assert len(rows)==len(words)
failed=[]
for word,plain,row in zip(words,clean,rows):
 b=hb.Buffer();b.add_str(plain);b.guess_segment_properties();hb.shape(f,b)
 expected=[g.codepoint for g in b.glyph_infos];got=[int(x) for x in row.split()]
 if got!=expected:failed.append({'word':word,'expected':expected,'got':got})
print(json.dumps({'cases':len(words),'failures':failed},ensure_ascii=False,indent=2));assert not failed
