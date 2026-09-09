#!/usr/bin/env python3
"""Wiring contracts complement executable state and framebuffer tests."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
main = (root / 'src/main.cpp').read_text()
assert 'reader::handle_credits_input(' in main
assert 'reader::draw_credits(' in main
assert '"Start: Credits"' in main
assert '"Select: Controls"' in main
assert '"gbareader V1.1"' in main
assert '"files: /gbareader"' in main
assert 'reader::library_path(selected, path)' in main
assert 'reader::handle_controls_input(' in main
assert 'reader::draw_controls(' in main
assert 'No TXT/EPUB in root' not in main
initial = main[main.index('int main()'):main.index('bool storage_ok')]
assert 'painter.flip_page_later();\n    bn::core::update();' in initial, 'Initial flip must commit before first bitmap Home render'
assert 'Scene::CREDITS' in main
# Normal library controls must not execute on either modal transition/release tail.
assert 'if(credits_consumed)' in main
assert '} else if(scene == Scene::LIBRARY)' in main
# A snapshot made using older glyph widths is not a valid current-layout history.
assert 'history = footer.history;' not in main
assert 'history_rebuild = footer.history_rebuild;' not in main
assert 'reader::restore_saved_history(footer, history, history_rebuild);' in main
# Reading/settings Start retains its original action rather than opening credits.
reader_start = main[main.index('} else if(bn::keypad::start_pressed())'):main.index('} else if(bn::keypad::select_pressed())')]
assert 'file.save_footer(' in reader_start and 'Scene::CREDITS' not in reader_start
settings = main[main.index('if(bn::keypad::up_pressed() && settings_row'):main.index('if(scene == Scene::READER && reader::tick_save_message')]
assert 'bn::keypad::b_pressed() || bn::keypad::start_pressed()' in settings
assert 'scene = Scene::READER;' in settings
print('PASS: credits dispatcher, hint budget, unchanged reading Start, stale-history rejection wiring')
