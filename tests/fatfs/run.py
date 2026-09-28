#!/usr/bin/env python3
"""Production ReaderFile/EPUB + real FAT16, readonly books and bounded companions."""
import pathlib, subprocess, shutil, json, hashlib, sys, zipfile, shlex, tempfile, os, struct, zlib
ROOT=pathlib.Path(__file__).resolve().parent
SOURCE=pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT.parents[1]
SEED=os.environ.get('GBAREADER_FATFS_SEED_IMAGE')
for tool in ['gcc','g++','mcopy','fsck.fat','mattrib','mren','mmd']+([] if SEED else ['mkfs.fat']):
    if not shutil.which(tool): raise SystemExit('Required test dependency missing: '+tool)
RUN=pathlib.Path(tempfile.mkdtemp(prefix='gbareader-sidecar-fatfs-',dir=os.environ.get('TMPDIR')))
SNAP=RUN/'snapshot'; SNAP.mkdir(); LOG=RUN/'commands.log'
def cmd(args,check=True):
    args=list(map(str,args));p=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
    with LOG.open('a') as f:f.write('$ '+shlex.join(args)+'\n'+p.stdout+'\nexit='+str(p.returncode)+'\n')
    if check and p.returncode:raise RuntimeError(f'{args}: {p.stdout}; evidence {RUN}')
    return p
hashes={}
for folder in ['src','include','tests']:
    shutil.copytree(SOURCE/folder,SNAP/folder)
    for p in sorted((SNAP/folder).rglob('*')):
        if p.is_file():hashes[str(p.relative_to(SNAP))]=hashlib.sha256(p.read_bytes()).hexdigest()
(RUN/'source-hashes.json').write_text(json.dumps(hashes,indent=2))
stub=RUN/'stubs';stub.mkdir();(stub/'bn_core.h').write_text('#pragma once\n')
common=['-O1','-g','-ffunction-sections','-fdata-sections','-D__DEVKITARM__','-I'+str(stub),'-I'+str(SNAP/'include')]
objects=[]
for name in ['ff.c','ffunicode.c','miniz_tinfl.c','reader_core.cpp','reader_txt_save.cpp','epub_document.cpp','reader_file.cpp','reader_global_settings.cpp']:
    obj=RUN/(name+'.o');objects.append(obj)
    cmd(['gcc' if name.endswith('.c') else 'g++',*common,'-c',SNAP/'src'/name,'-o',obj])
cmd(['g++','-std=c++17',*common,SNAP/'tests/fatfs/harness.cpp',*objects,'-Wl,--gc-sections','-o',RUN/'harness'])
cmd(['g++','-std=c++17',*common,SNAP/'tests/fatfs/global_boot.cpp',*objects,'-Wl,--gc-sections','-o',RUN/'global-boot'])
cmd(['python3',SNAP/'tests/generate_epub_fixtures.py',RUN/'fixtures'])
fixture=RUN/'fixtures'/os.environ.get('GBAREADER_FATFS_FIXTURE','window-cross.epub')
legacy=RUN/'legacy.txt';legacy.write_bytes(b'Original text body.\n'*80+b'\n[GBAR-SAVE:1;O= 0000123456;S=1;T=1;B=1;C=59AAEAA4                                             \n')
base=RUN/'base.img'
# Optional preformatted EMPTY FAT seed for environments that forbid formatting.
# Always operate on a copy; the supplied seed and fault semantics stay unchanged.
if SEED:
    shutil.copyfile(SEED,base)
    cmd(['fsck.fat','-n',base])
else:
    with base.open('wb') as f:f.truncate(32*1024*1024)
    cmd(['mkfs.fat','-F','16',base])
for src,name in [(fixture,'book.epub'),(legacy,'legacy.txt')]:
    cmd(['mcopy','-i',base,src,'::'+name]);cmd(['mattrib','-i',base,'+r','::'+name])
def invoke(image,book,action,version,prefix,kind='-',ordinal=0,policy='o'):
    p=cmd([RUN/'harness',image,book,action,version,kind,ordinal,policy,prefix],False)
    try:return json.loads(p.stdout)
    except ValueError:return {'returncode':p.returncode,'output':p.stdout}
def extract(image,book,target):
    p=cmd(['mcopy','-o','-i',image,'::'+book,target],False)
    if p.returncode:raise AssertionError(p.stdout)
    return target.read_bytes()
refs={};initial_footer={}
for book in ['book.epub','legacy.txt']:
    r=invoke(base,book,'probe',0,RUN/('reference-'+book));assert r['opened'] and r['document'],r
    refs[book]=(RUN/('reference-'+book+'.text')).read_bytes()
    fp=RUN/('reference-'+book+'.footer');initial_footer[book]=fp.read_bytes() if fp.exists() else None
rows=[]
def verify(image,label,book,version,old=None):
    folder=RUN/label;folder.mkdir(exist_ok=True)
    r=invoke(image,book,'probe',version,folder/'probe')
    actual=extract(image,book,folder/book)
    original=fixture.read_bytes() if book.endswith('.epub') else legacy.read_bytes()
    assert actual==original,(label,'source bytes changed')
    assert r.get('opened') and r.get('document'),(label,r)
    assert (folder/'probe.text').read_bytes()==refs[book],(label,'normalized text changed')
    if book.endswith('.epub'):
        with zipfile.ZipFile(folder/book) as z:assert z.testzip() is None
    allowed=r.get('state_match',False)
    if old is not None:
        allowed |= invoke(image,book,'probe',old,folder/'old').get('state_match',False)
    else:
        fp=folder/'probe.footer'
        # No prior companion exists in initial/migration faults. An incomplete
        # companion deliberately suppresses legacy resurrection; source bytes
        # (including the legacy state) are still required to be exactly intact.
        allowed |= not r.get('footer') or (fp.read_bytes() if fp.exists() else None)==initial_footer[book]
    assert allowed,(label,'neither previous nor new complete state',r)
    fs=cmd(['fsck.fat','-n',image],False);(folder/'fsck.txt').write_text(fs.stdout)
    r.update(source_exact=True,normalized_equal=True,state_old_or_new=allowed,fsck_exit=fs.returncode)
    return r
success=RUN/'success.img';shutil.copyfile(base,success)
previous_cache=None
for version,action in [(1,'cache'),(2,'state'),(3,'state')]:
    label=f'success-{version}';save=invoke(success,'book.epub',action,version,RUN/(label+'-save'))
    v=verify(success,label,'book.epub',version);assert save.get('saved') and v['state_match'] and v['fsck_exit']==0,(save,v)
    payload=extract(success,'book.epub.sav',RUN/(label+'.sav'))
    assert len(payload)>2048 and len(payload)<=128*1024*1024
    if previous_cache is not None:assert payload[2048:]==previous_cache and len(payload)==2048+len(previous_cache)
    previous_cache=payload[2048:]
    rows.append(dict(case=label,save=save,verification=v))
    if version==1:shutil.copyfile(success,RUN/'cached.img')
# A companion cache hit opens without re-scanning the ZIP or rebuilding the spine:
# only the cache header/table are read, far fewer disk reads than a fresh open.
fresh_open=invoke(base,'book.epub','probe',0,RUN/'open-cost-fresh')
cached_open=invoke(RUN/'cached.img','book.epub','probe',1,RUN/'open-cost-cached')
assert cached_open['document'] and cached_open['optimized'] and fresh_open['document'],(fresh_open,cached_open)
assert cached_open['doc_open_reads']<=16,(fresh_open,cached_open)
# Books larger than the first read window (many-entries.epub in CI) need reads to open fresh.
if fresh_open['doc_open_reads']>16:assert cached_open['doc_open_reads']*4<fresh_open['doc_open_reads'],(fresh_open,cached_open)
print('OPEN_COST fresh=%d cached=%d disk reads'%(fresh_open['doc_open_reads'],cached_open['doc_open_reads']))
for mode in ['fresh','cached','fallback']:
    image=RUN/('repeat-'+mode+'.img');shutil.copyfile(base if mode=='fresh' else RUN/'cached.img',image)
    if mode=='fallback':
        payload=bytearray(extract(image,'book.epub.sav',RUN/'corrupt.sav'))
        n=struct.unpack_from('<I',payload,2048+16)[0];payload[2048+32+n-1]^=1
        (RUN/'corrupt.sav').write_bytes(payload);cmd(['mcopy','-o','-i',image,RUN/'corrupt.sav','::book.epub.sav'])
    label='success-repeat-'+mode
    save=invoke(image,'book.epub','repeat-fallback' if mode=='fallback' else 'repeat',2,RUN/(label+'-save'))
    v=verify(image,label,'book.epub',2);assert save.get('saved') and v['state_match'] and v['fsck_exit']==0,(save,v)
    rows.append(dict(case=label,save=save,verification=v))
# Same live ReaderFile/EpubDocument and pending state after a source FIL read latch.
for book in ['legacy.txt','book.epub']:
    for action in ['live-retry','live-retry-nav']:
        for existing in [False,True]:
            label=f'{action}-{book}-{existing}';image=RUN/(label+'.img');shutil.copyfile(base,image)
            if existing:assert invoke(image,book,'state',1,RUN/(label+'-seed'))['saved']
            save=invoke(image,book,action,2,RUN/(label+'-save'),'r',1)
            assert save.get('saved') and save['hits']==1 and save['state_match'],(label,save)
            assert (RUN/(label+'-save.text')).read_bytes()==refs[book]
            v=verify(image,label,book,2);assert v['state_match'] and v['fsck_exit']==0
            rows.append(dict(case=label,save=save,verification=v))
# Fixed real-sector fault selectors: transient, persistent, and abrupt process loss.
# Host FIL tests additionally enumerate every normal-path API ordinal, short I/O,
# partial record writes, reopen failures, and pointer-keyed close quarantine.
faults=[('r',1,'o'),('r',2,'o'),('w',1,'o'),('w',2,'o'),('w',3,'o'),('s',1,'o'),('s',2,'o'),('w',2,'p'),('s',1,'p'),('w',3,'c'),('s',1,'c')]
for phase,book in [('cache','book.epub'),('state','book.epub'),('txt','legacy.txt')]:
    for kind,ordinal,policy in faults:
        label=f'{phase}-{kind}{ordinal}-{policy}';image=RUN/(label+'.img')
        shutil.copyfile(RUN/'cached.img' if phase=='state' else base,image)
        save=invoke(image,book,'cache' if phase=='cache' else 'state',2,RUN/(label+'-save'),kind,ordinal,policy)
        v=verify(image,label,book,2,1 if phase=='state' else None)
        if policy=='o' and save.get('hits',0):assert v['fsck_exit']==0,(label,v)
        rows.append(dict(case=label,save=save,verification=v))
# The same source identity must reject another book's SAV, without reading stale
# legacy Arabic/bookmarks and without overwriting the collision.
image=RUN/'stale.img';shutil.copyfile(success,image)
wrong=RUN/'wrong.sav';wrong.write_bytes(extract(image,'book.epub.sav',RUN/'copied.sav'))
cmd(['mcopy','-i',image,wrong,'::legacy.txt.sav'])
r=invoke(image,'legacy.txt','probe',2,RUN/'stale-probe');assert not r['footer']
s=invoke(image,'legacy.txt','state',2,RUN/'stale-save');assert not s['saved']
assert extract(image,'legacy.txt.sav',RUN/'stale-after.sav')==wrong.read_bytes()
assert extract(image,'legacy.txt',RUN/'stale-after.txt')==legacy.read_bytes()
# Unknown empty/nonempty collisions stay exact, including automatic cache saves.
# Seed genuine partial-header crash records separately from returned-error cleanup.
for book in ['legacy.txt','book.epub']:
    for length in [0,1200,64]:
        label=f'ownership-{book}-{length}';image=RUN/(label+'.img');shutil.copyfile(base,image)
        payload=RUN/(label+'.sav')
        if length==64:
            assert invoke(image,book,'state',1,RUN/(label+'-seed'))['saved']
            header=extract(image,book+'.sav',RUN/(label+'-complete.sav'))[:64]
            assert zlib.crc32(header[:40])==struct.unpack_from('<I',header,40)[0]
            payload.write_bytes(header)
        else:payload.write_bytes(b'?'*length)
        cmd(['mcopy','-o','-i',image,payload,'::'+book+'.sav'])
        assert not invoke(image,book,'probe',2,RUN/(label+'-probe'))['footer']
        save=invoke(image,book,'cache' if book.endswith('.epub') else 'state',2,RUN/(label+'-save'))
        assert save.get('saved')==(length==64),(label,save)
        if length!=64:assert extract(image,book+'.sav',RUN/(label+'-after.sav'))==payload.read_bytes()
        v=verify(image,label,book,2);assert v['fsck_exit']==0
        if length==64:assert v['state_match']
        rows.append(dict(case=label,save=save,verification=v))
(RUN/'results.json').write_text(json.dumps(rows,indent=2))
print(json.dumps({'run':str(RUN),'cases':len(rows),'unhit_selectors':[r['case'] for r in rows if r['case'].startswith(('cache-','state-','txt-')) and r['save'].get('hits')==0], 'filesystem_diagnostics':[r['case'] for r in rows if r['verification']['fsck_exit']!=0]},indent=2))
print('PASS: real FatFS read-only TXT/EPUB, bounded cache/state saves, exact source preservation, fresh-mount recovery, collision rejection')
# Fresh mode fixtures replace readonly files; preserve their exact bytes too.
for name in ['book.epub','legacy.txt']:cmd(['mattrib','-i',base,'-r','::'+name])
sys.dont_write_bytecode=True
from mode_cases import run as run_mode_cases
run_mode_cases(RUN,base,invoke,cmd)
from global_cases import run as run_global_cases
run_global_cases(RUN,success,invoke,cmd,extract)
from global_boot_cases import run as run_global_boot_cases
run_global_boot_cases(RUN,base,cmd)
