#!/usr/bin/env python3
from pathlib import Path
import sys
s=(Path(__file__).resolve().parents[1]/'src/main.cpp').read_text()
out=Path(sys.argv[1]);out.mkdir(exist_ok=True)
a=s.index('    settings = reader::default_settings();');b=s.index('    while(true) {',a)
(out/'global_boot.inc').write_text(s[a:b])
a=s.index('            if(bn::keypad::up_pressed() && settings_row');b=s.index('\n        }\n\n        // Reader mode overlay exit cleanup.',a)
(out/'global_settings_input.inc').write_text(s[a:b])
a=s.index('                if(opened != reader::OpenResult::FAILED) {');b=s.index('                } else {',a)
(out/'global_open_success.inc').write_text(s[a:b]+'\n}\n')
a=s.index('                add_text(ui, 0, -62, "Reader settings"');b=s.index('\n            }\n            redraw_ui = false;',a)
(out/'global_settings_draw.inc').write_text(s[a:b])
