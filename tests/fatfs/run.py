#!/usr/bin/env python3
"""External correctness/fault harness. No timing or performance metrics."""
import pathlib, subprocess, shutil, json, hashlib, sys, zipfile, datetime, shlex, tempfile, os
ROOT=pathlib.Path(__file__).resolve().parent
SOURCE=pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT.parents[1]
for tool in ['gcc', 'g++', 'mkfs.fat', 'mcopy', 'fsck.fat']:
 if not shutil.which(tool): raise SystemExit('Required test dependency missing: '+tool)
RUN=pathlib.Path(tempfile.mkdtemp(prefix='gbareader-fatfs-', dir=os.environ.get('TMPDIR')))
SNAP=RUN/'snapshot';SNAP.mkdir();LOG=RUN/'commands.log'
def cmd(args,check=True):
 args=list(map(str,args));p=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
 with LOG.open('a') as f:f.write('$ '+shlex.join(args)+'\n'+p.stdout+'\nexit='+str(p.returncode)+'\n')
 if check and p.returncode:raise RuntimeError(p.stdout)
 return p
hashes={}
for folder in ['src','include','tests']:
 shutil.copytree(SOURCE/folder,SNAP/folder)
 for p in sorted((SNAP/folder).rglob('*')):
  if p.is_file():hashes[str(p.relative_to(SNAP))]=hashlib.sha256(p.read_bytes()).hexdigest()
(RUN/'source-hashes.json').write_text(json.dumps(hashes,indent=2))
# Only absent hardware-framework header is stubbed. Production sources unchanged.
stub=RUN/'stubs';stub.mkdir();(stub/'bn_core.h').write_text('#pragma once\n')
common=['-O1','-g','-ffunction-sections','-fdata-sections','-D__DEVKITARM__','-I'+str(stub),'-I'+str(SNAP/'include')]
objects=[]
for name in ['ff.c','ffunicode.c','miniz_tinfl.c','reader_core.cpp','reader_txt_save.cpp','epub_document.cpp','reader_file.cpp']:
 obj=RUN/(name+'.o');objects.append(obj)
 cmd(['gcc' if name.endswith('.c') else 'g++',*common,'-c',SNAP/'src'/name,'-o',obj])
cmd(['g++','-std=c++17',*common,ROOT/'harness.cpp',*objects,'-Wl,--gc-sections','-o',RUN/'harness'])
cmd(['python3',SNAP/'tests/generate_epub_fixtures.py',RUN/'fixtures'])
fixture=RUN/'fixtures'/os.environ.get('GBAREADER_FATFS_FIXTURE', 'window-cross.epub')
legacy=RUN/'legacy.txt';legacy.write_bytes(b'Original text body.\n'+b'\n[GBAR-SAVE:1;O= 0000123456;S=1;T=1;B=1;C=59AAEAA4                                             \n')
base=RUN/'base.img'
with base.open('wb') as f:f.truncate(32*1024*1024)
cmd(['mkfs.fat','-F','16',base]);cmd(['mcopy','-i',base,fixture,'::book.epub']);cmd(['mcopy','-i',base,legacy,'::legacy.txt'])
def invoke(image,book,action,version,prefix,kind='-',ordinal=0,policy='o'):
 p=cmd([RUN/'harness',image,book,action,version,kind,ordinal,policy,prefix],False)
 try:r=json.loads(p.stdout)
 except ValueError:r={'returncode':p.returncode,'output':p.stdout}
 return r
rows=[]
def verify(image,label,version,old_version=None,txt=False):
 folder=RUN/label;folder.mkdir(exist_ok=True);book='legacy.txt' if txt else 'book.epub'
 r=invoke(image,book,'probe',version,folder/'probe')
 extracted=folder/book;p=cmd(['mcopy','-o','-i',image,'::'+book,extracted],False)
 valid=False;original=False;cache=False;state_allowed=False
 if p.returncode==0:
  if txt:
   raw=extracted.read_bytes();original=raw.startswith(b'Original text body.\n');valid=r.get('footer',False)
   r['exact_legacy_rollback']=raw==legacy.read_bytes()
   state_allowed=r.get('state_match',False) or r['exact_legacy_rollback']
  else:
   try:
    with zipfile.ZipFile(fixture) as z,zipfile.ZipFile(extracted) as actual:
     valid=actual.testzip() is None
     original=all(actual.read(i.filename)==z.read(i.filename) for i in z.infolist())
     # Also preserve exact original local header/name/extra/compressed payload bytes.
     import struct
     before=fixture.read_bytes();after=extracted.read_bytes()
     for i in z.infolist():
      start=i.header_offset;nl,el=struct.unpack_from('<HH',before,start+26);end=start+30+nl+el+i.compress_size
      original &= before[start:end]==after[start:end]
     eocd=after.rfind(b'PK\x05\x06');r['trailing_after_last_eocd']=len(after)-eocd-22 if eocd>=0 else None
     cache='META-INF/gbareader/cache-v5' in actual.namelist()
     state_allowed=bool(r.get('state_match'))
     if old_version is None:
      state_allowed |= extracted.read_bytes()==fixture.read_bytes()
     elif not state_allowed:
      old=invoke(image,book,'probe',old_version,folder/'old');state_allowed=bool(old.get('state_match'))
   except Exception as e:r['zip_error']=str(e)
 r.update(zip_or_footer_valid=bool(valid),original_preserved=bool(original),cache_present=cache,state_old_or_new=state_allowed)
 text=folder/'probe.text';reference=RUN/('baseline-txt/probe.text' if txt else 'baseline/probe.text')
 r['normalized_equal']=text.exists() and (not reference.exists() or text.read_bytes()==reference.read_bytes())
 fs=cmd(['fsck.fat','-n',image],False);(folder/'fsck.txt').write_text(fs.stdout);r['fsck_exit']=fs.returncode
 return r
verify(base,'baseline',1);verify(base,'baseline-txt',1,txt=True)
# Successful first cache and two later state saves, each inspected through fresh mount.
success=RUN/'success.img';shutil.copyfile(base,success)
for version,action in [(1,'cache'),(2,'state'),(3,'state')]:
 label='success-'+str(version);save=invoke(success,'book.epub',action,version,RUN/(label+'-save'));v=verify(success,label,version)
 rows.append(dict(case=label,save=save,verification=v))
 if version==1:shutil.copyfile(success,RUN/'cached.img')
# Bounded fixed fault ordinals are test selectors, not a performance survey.
faults=[('r',1,'o'),('r',2,'o'),('w',1,'o'),('w',2,'o'),('w',3,'o'),('s',1,'o'),('s',2,'o'),('w',2,'p'),('s',1,'p'),('w',3,'c'),('s',1,'c')]
for phase in ['cache','state','txt']:
 for kind,ordinal,policy in faults:
  label=f'{phase}-{kind}{ordinal}-{policy}';image=RUN/(label+'.img');shutil.copyfile(RUN/'cached.img' if phase=='state' else base,image)
  save=invoke(image,'legacy.txt' if phase=='txt' else 'book.epub','cache' if phase=='cache' else 'state',2,RUN/(label+'-save'),kind,ordinal,policy)
  v=verify(image,label,2,1 if phase=='state' else None,phase=='txt')
  rows.append(dict(case=label,save=save,verification=v))
(RUN/'results.json').write_text(json.dumps(rows,indent=2))
limitations=[r for r in rows if not all(r['verification'].get(k) for k in ['zip_or_footer_valid','original_preserved','state_old_or_new','normalized_equal']) or r['verification']['fsck_exit']!=0]
not_hit=[r['case'] for r in rows if not r['case'].startswith('success') and r['save'].get('hits')==0]
success_ok=all(r['save'].get('saved') and r['verification'].get('state_match') and r['verification']['cache_present'] for r in rows[:3])
lines=['# Real FatFS storage integration and fault-injection review','',f'Run directory: `{RUN}`',f'Source: `{SOURCE}`','', '## Reproduce','```sh',f'python3 {ROOT}/run.py {SOURCE}','```','Exact compiler, image, fixture, harness, extraction and fsck commands plus output: `'+str(LOG)+'`. Source snapshots and SHA-256 manifest: `snapshot/` and `source-hashes.json` in that run. Each rerun gets a fresh timestamped directory; old fault images remain isolated.','', '## Scope and model','Production `__DEVKITARM__` ReaderFile, EpubDocument, reader core/footer serialization, miniz, ff.c and ffunicode.c are compiled unchanged on the host. An empty bn_core.h satisfies the unused framework include; function-section linker GC discards storage_init and its Supercard initialization. Sector diskio uses pread/pwrite on a 32 MiB FAT16 image with 512-byte sectors. Every verification launches a separate process, creates a fresh FATFS mount, and reopens via ReaderFile/EpubDocument. mcopy independently extracts the result; Python zipfile checks all entry CRCs and exact decompressed bytes of every original book entry. Normalized text is compared byte-for-byte to the uncached baseline. Bookmark, settings and wrapped three-entry history are checked through canonical complete footer serialization. No speed benchmarks or comparative operation totals are collected.','', 'Faults arm immediately before save_footer, after normal open/EPUB parse. r/w/s select returned disk read/write/CTRL_SYNC errors. Ordinal is a fixed deterministic request selector; o is one-shot, p repeats matching failures from the selected ordinal until save returns. c exits the process immediately before that selected disk request without destructors/FatFS close, retaining all preceding sector writes. This is an abrupt-process-loss model, NOT torn-sector, device-cache, electrical or real SD power-loss emulation. No production recovery writes are added by the harness.','', '## Results',f'Successful cache creation plus two later saves passed: **{success_ok}**.',f'Cases: {len(rows)}. Cases with an integrity/state/text/fsck limitation: **{len(limitations)}**. Fault selectors not reached: {not_hit}.','', '| Case | save returned / cut | hit | archive/footer valid | original bytes | old/new state | normalized text | fsck |','|---|---|---|---|---|---|---|---|']
for r in rows:
 s=r['save'];v=r['verification'];lines.append('| '+ ' | '.join(map(str,[r['case'],s.get('saved',s.get('returncode')),s.get('hits','cut'),v['zip_or_footer_valid'],v['original_preserved'],v['state_old_or_new'],v['normalized_equal'],v['fsck_exit']]))+' |')
lines += ['', '## Actual limitations and retained evidence','A failed save is not assumed to have rolled back: the table accepts either the complete old or complete new state, and records corruption separately. fsck exit nonzero is retained even when ZIP bytes remain readable; no automatic repair is run. Persistent failures may prevent rollback. TXT is the existing embedded legacy bookmark/footer writer, not a plain-text editor replacement path; original body preservation and legacy-footer rollback are evaluated separately. Physical archive compaction, permanent sidecars, hardware initialization/flashcard execution, out-of-space, torn sectors and exhaustive fault positions are outside scope.','', 'Full per-case outputs, parsed footer bytes, expected footer bytes, normalized text, extracted archives, read-only fsck diagnostics, and all images are retained under the run directory. `results.json` is machine-readable. Successful image is final version 3; cached.img preserves version 1 for later-save faults.']
for r in rows:
 if r['case'].startswith(('cache-w','state-w')) and r['save'].get('hits',0) and not r['verification'].get('document'):
  lines += ['', f"**Returned-error recovery failure: {r['case']}**: save_footer returned false after an injected disk_write error; a fresh FAT mount and EpubDocument reopen failed, although Python ZIP validation preserved the original entry data. Last EOCD has {r['verification'].get('trailing_after_last_eocd')} trailing bytes. This is an application readability/rollback failure, not a passing recovery test. The production same-FIL seek/truncate cleanup can be blocked by a latched FatFS FIL.err; no production fix is made here."]
lines += ['', '## Exact key snapshot hashes', '```json', json.dumps({k:hashes[k] for k in ['src/reader_file.cpp','include/reader_file.h','src/epub_document.cpp','src/ff.c','include/ffconf.h']},indent=2),'```']
for r in limitations:lines += ['',f'### {r["case"]}', '```json',json.dumps(r,indent=2),'```']
report=RUN/'fatfs-review.md';report.write_text('\n'.join(lines)+'\n')
print(json.dumps({'run':str(RUN),'report':str(report),'success_ok':success_ok,'cases':len(rows),'limitations':len(limitations),'not_hit':not_hit},indent=2))

# Only successful transactions and exercised transient returned errors are gates.
# Persistent-device-error and abrupt-loss cases remain explicit diagnostics;
# they do not imply guaranteed electrical power-loss recovery.
required = [r for r in rows if r['case'].startswith('success-') or
            (r['case'].endswith('-o') and r['save'].get('hits', 0) > 0)]
failed = []
for row in required:
 v = row['verification']
 keys = ['zip_or_footer_valid', 'original_preserved', 'state_old_or_new', 'normalized_equal']
 ok = all(v.get(k) for k in keys) and v.get('fsck_exit') == 0
 if not row['case'].startswith('txt-'): ok = ok and v.get('document', False)
 if row['case'].startswith('success-'):
  ok = ok and row['save'].get('saved') and v.get('state_match')
 if not ok: failed.append(row['case'])
if failed:
 raise SystemExit('FAIL: real FatFS recovery gate: '+', '.join(failed))
print('PASS: real FatFS successful saves and exercised transient-error recovery')
