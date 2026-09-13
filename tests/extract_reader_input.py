#!/usr/bin/env python3
"""Compile the verbatim production reader dispatch against hardware/I/O doubles."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
s = (root/'src/main.cpp').read_text()
sampler = s[s.index('[[gnu::noinline]] unsigned sample_reader_hold'):s.index('// End reader hold sampler.')]
Path(sys.argv[1]).with_name('reader_sampler.inc').write_text(sampler)
a = s.index('            reader::Page next{};', s.index('} else if(scene == Scene::READER)'))
b = s.index('            const bool idle_frame', a)
sample = s.split('// Reader hold sampling:', 1)[1].split('\n', 1)[1].split('// End reader hold sampling.', 1)[0]
cleanup = s.split('// Reader mode overlay exit cleanup.', 1)[1].split('// End reader mode overlay exit cleanup.', 1)[0]
Path(sys.argv[1]).write_text(sample + '\nif(scene == Scene::READER) {\n' + s[a:b] + '\n}\n' + cleanup)
