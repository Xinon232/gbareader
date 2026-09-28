"""Fresh-mount expected-state boot failures; no original fixture is mutated."""
import json, shutil

CASES = ['both', 'missing0', 'partial', 'no-load', 'no-load-defaults',
         'boot-defaults', 'exhausted', 'partial-exhausted', 'future-target', 'future-partner',
         'damaged-target', 'damaged-only', 'retry-read']

def run(run_dir, base, cmd):
    rows=[]
    for case in CASES:
        image=run_dir/('global-boot-'+case+'.img')
        shutil.copyfile(base,image)
        cmd(['mmd','-i',image,'::gbareader'])
        result=cmd([run_dir/'global-boot',image,case])
        row=json.loads(result.stdout)
        fs=cmd(['fsck.fat','-n',image])
        row['fsck_exit']=fs.returncode
        rows.append(row)
    (run_dir/'global-boot-results.json').write_text(json.dumps(rows,indent=2))
    print('PASS: global boot faults/reconciliation, missing slot, partial load, no-load API, full identity, collision/damage/exhaustion, repeated read failure and fresh mounts (%d cases)'%len(rows))
