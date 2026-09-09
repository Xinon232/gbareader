#!/usr/bin/env python3
"""Extract the actual Home branch, not a duplicate implementation."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
source = (root / 'src/main.cpp').read_text()
start = source.index('if(scene == Scene::LIBRARY) {', source.index('if(redraw_ui) {'))
branch = source[start:].split('} else if(scene == Scene::CONTROLS)', 1)[0].split('{', 1)[1]
Path(sys.argv[1]).write_text(branch)
