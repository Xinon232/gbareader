"""Independent fresh-mount oracle for global records; books/SAVs stay exact."""
import json, shutil, zlib, struct

def run(RUN, success, invoke, cmd, extract):
    image=RUN/'globals.img';shutil.copyfile(success,image)
    cmd(['mmd','-i',image,'::gbareader'])
    for name in ['one.txt','one.epub']:
        source=RUN/('global-'+name);source.write_bytes(b'Latin body\n'*400 if name.endswith('.txt') else (RUN/'fixtures/stored.epub').read_bytes())
        cmd(['mcopy','-i',image,source,'::gbareader/'+name])
    # Separate unrelated state must remain byte-identical, not merely parseable.
    unrelated=RUN/'unrelated.sav';unrelated.write_bytes(b'not an owned record')
    cmd(['mcopy','-i',image,unrelated,'::unrelated.sav'])
    protected={name:extract(image,name,RUN/('before-'+name)) for name in ['book.epub','book.epub.sav','legacy.txt','unrelated.sav']}
    def call(action,version=0,name='one.txt',kind='-',ordinal=0,policy='o',img=image):
        return invoke(img,name,action,version,RUN/'global',kind,ordinal,policy)
    r=call('global-probe');assert r['load']==0 and r['selected']==0 and not r['name_match'],r
    rows=[]
    for v in [1,2,3,4,5]:
        name='one.epub' if v%2 else 'one.txt'
        r=call('global-save',v,name);assert r['saved'] and not r['dirty'] and r['name_match'],r
        p=call('global-probe',name=name);assert p['name_match'] and p['spacing']==v%5 and p['gap']==v%4 and p['startup']==bool(v%2),p
        assert p['selected']==(1 if name.endswith('.epub') else 0),p
        rows.append(p)
    # Actual TXT/EPUB ReaderFile + EpubDocument opening merges globals and
    # saved Arabic/anchor, without changing either companion or EPUB payload.
    txt_seed=invoke(image,'legacy.txt','state',2,RUN/'global-txt-seed');assert txt_seed['saved']
    protected['legacy.txt.sav']=extract(image,'legacy.txt.sav',RUN/'before-legacy.sav')
    for book,anchor in [('book.epub',369),('legacy.txt',246)]:
        # Some inherited fixtures normalize to fewer bytes than their synthetic
        # bookmark. Match the production valid-anchor rule, not a fixed number.
        if anchor >= len((RUN/('reference-'+book+'.text')).read_bytes()):anchor=0
        p=call('global-book',name=book)
        assert p['spacing']==0 and p['gap']==1 and p['arabic'] and p['anchor']==anchor,p
        if book.endswith('.epub'):assert p['cache']>0
    # Same remembered book with unchanged preferences is a true no-op.
    before=[extract(image,'gbareader/SETTINGS%d.DAT'%i,RUN/('slot%d-before'%i)) for i in [0,1]]
    r=call('global-remember',name='one.epub');assert r['saved'] and not r['dirty']
    after=[extract(image,'gbareader/SETTINGS%d.DAT'%i,RUN/('slot%d-after'%i)) for i in [0,1]]
    assert before==after
    # Explicit byte-format/CRC oracle does not call the production decoder.
    for b in after:
        assert b[:8]==b'GBARCFG1' and struct.unpack_from('<I',b,8)[0]==2
        assert len(b)==32+b[19] and not any(b[20:28])
        assert zlib.crc32(b[:-4])==struct.unpack_from('<I',b,len(b)-4)[0]
    # 255 UTF-8 bytes: selection survives the maximum supported exact identity.
    longname='x'*245+'éé.txt' # 253 bytes, then two bytes below
    longname='xx'+longname
    assert len(longname.encode())==255
    cmd(['mcopy','-i',image,RUN/'global-one.txt','::gbareader/'+longname])
    r=call('global-remember',name=longname);assert r['saved'] and r['selected']==2,r
    r=call('global-probe',name=longname);assert r['name_match'] and r['selected']==2 and r['spacing']==0,r
    r=call('global-prefs',3,name=longname);assert r['saved'] and r['name_match'] and r['spacing']==3,r
    cmd(['mren','-i',image,'::gbareader/'+longname,'::gbareader/renamed.txt'])
    r=call('global-probe',name=longname);assert r['selected']==0 and r['name_match'],r
    # Corrupt newest copy => older valid complete settings and name.
    active=r['active'];b=bytearray(extract(image,'gbareader/SETTINGS%d.DAT'%active,RUN/'latest.dat'));b[-1]^=1
    (RUN/'latest.dat').write_bytes(b);cmd(['mcopy','-o','-i',image,RUN/'latest.dat','::gbareader/SETTINGS%d.DAT'%active])
    r=call('global-probe',name=longname);assert r['load']==2 and r['name_match'] and r['spacing']==0,r
    # Real-sector errors retry in the same bounded production object.
    fault_base=RUN/'globals-fault-base.img';shutil.copyfile(image,fault_base)
    for kind in ['r','w','s']:
        for ordinal in [1,2,3]:
            fault_image=RUN/f'global-{kind}{ordinal}.img';shutil.copyfile(fault_base,fault_image)
            r=call('global-retry',4,'one.txt',kind,ordinal,img=fault_image)
            assert r['saved'] and not r['dirty'],r
            p=call('global-probe',name='one.txt',img=fault_image);assert p['name_match'] and p['spacing']==4,p
            fs=cmd(['fsck.fat','-n',fault_image],False);assert fs.returncode==0,fs.stdout
            for name,b in protected.items():assert extract(fault_image,name,RUN/('fault-check-'+name))==b,name
            rows.append(r)
    for name,b in protected.items():assert extract(image,name,RUN/('after-'+name))==b,name
    assert cmd(['fsck.fat','-n',image],False).returncode==0
    (RUN/'global-results.json').write_text(json.dumps(rows,indent=2))
    print('PASS: global native FAT defaults, checked alternating slots, exact CRC/name, maximum UTF-8, selection, no-op, preferences/name isolation, corrupt fallback, live sector retry and untouched books/SAV/cache')
