#!/usr/bin/env python3
"""Scope contracts; executable extracted main paths are tested separately."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/main.cpp').read_text()
boot=s[s.index('    settings = reader::default_settings();'):s.index('    while(true) {')]
assert 'global_settings.load()' in boot
assert 'reader_hold.shoulder_page_turns = global_settings.values.shoulder_startup;' in boot
assert 'reader::remembered_library_selection(' in boot
assert s.count('reader_hold.shoulder_page_turns = global_settings.values.shoulder_startup;')==1
assert 'BN_DATA_EWRAM_BSS reader::GlobalSettingsStore global_settings;' in s
op=s[s.index('reader::TxtSaveFooter footer{};'):s.index('} else if(scene == Scene::READER)')]
assert op.index('settings.line_spacing = global_settings.values.line_spacing;')<op.index('reader::open_document_page(')
success=op[op.index('if(opened != reader::OpenResult::FAILED)'):op.index('} else {',op.index('if(opened != reader::OpenResult::FAILED)'))]
assert 'reader::remember_global_book(global_settings.values, open_name)' in success
assert 'global_settings.save()' in success
assert 'file.save_footer(' not in success
settings=s[s.index('if(bn::keypad::up_pressed() && settings_row'):s.index('// Reader mode overlay exit cleanup.')]
assert settings.count('global_settings.save()')==1
assert 'file.save_footer(' not in settings
assert 'global_settings.values.shoulder_startup = delta > 0;' in settings
assert 'reader_hold.shoulder_page_turns =' not in settings
assert 'const int row_y[GOTO_ROW + 1] = { -40, -24, 8, 24 };' in s
assert '"L/R on startup: "' in s
assert 'add_text(hint_ui, ROW_X, -8, lines.data(), sprites);' in s
assert 'add_text(hint_ui, ROW_X, 40, where.data(), sprites);' in s
assert 'constexpr int GOTO_ROW = STARTUP_ROW + 1;' in boot
start=s[s.index('} else if(bn::keypad::start_pressed())'):s.index('} else if(bn::keypad::select_pressed())')]
assert 'file.save_footer(' in start and 'global_settings.save()' in start
assert 'reader::save_status_message(saved, globals_saved)' in start
assert 'Settings save failed' in s
print('PASS: global boot/open/settings/manual-save wiring, exact rows/palettes, scoped I/O and session mode')
