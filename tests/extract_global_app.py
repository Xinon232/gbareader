#!/usr/bin/env python3
"""Extract the production boot, open-success, Settings input/draw and list-key code."""
from pathlib import Path
import sys
s=(Path(__file__).resolve().parents[1]/'src/main.cpp').read_text()
out=Path(sys.argv[1]);out.mkdir(exist_ok=True)
def cut(start,end,name,after=''):
    a=s.index(start);b=s.index(end,a)
    (out/name).write_text(s[a:b]+after)
cut('    settings = reader::default_settings();','    auto flash = [&]','global_boot.inc')
cut('                if(opened != reader::OpenResult::FAILED) {','                } else {','global_open_success.inc','\n}\n')
a=s.index('        } else if(scene == Scene::SETTINGS) {');a=s.index('\n',a)+1
b=s.index('        } else if(scene == Scene::ABOUT) {',a)
(out/'global_settings_input.inc').write_text(s[a:b])
a=s.index('                screen::header(pixels, "Settings");');b=s.index('            if(message && message_frames',a)
(out/'global_settings_draw.inc').write_text(s[a:b].rsplit('            }',1)[0])
cut('struct ListKeys {','\nuint8_t* page_pixels','list_keys.inc')
cut('const char* settings_label(','\nstruct ImportProgress','settings_label.inc')
