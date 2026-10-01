#!/usr/bin/env python3
from pathlib import Path
import sys
s=(Path(__file__).resolve().parents[1]/'src/main.cpp').read_text()
a=s.index('void show_overlay(');b=s.index('void show_saving_overlay(',a)
Path(sys.argv[1]).write_text(s[a:b])
a=s.index('show_overlay(save_ui, sprites,');b=s.index(';',a)+1
Path(sys.argv[1]).with_name('mode_call.inc').write_text(s[a:b])
