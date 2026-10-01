#!/usr/bin/env python3
"""Scope contracts; executable extracted main paths are tested separately."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/main.cpp').read_text()
boot=s[s.index('    settings = reader::default_settings();'):s.index('    while(true) {')]
assert 'global_settings.load()' in boot
# The last L/R choice is restored once at launch.
assert s.count('reader_hold.shoulder_page_turns = global_settings.values.shoulder_page_turns;')==1
assert 'reader_hold.shoulder_page_turns = global_settings.values.shoulder_page_turns;' in boot
assert 'reader::remembered_library_selection(' in boot
assert 'BN_DATA_EWRAM_BSS reader::GlobalSettingsStore global_settings;' in s
op=s[s.index('reader::TxtSaveFooter footer{};'):s.index('} else if(scene == Scene::READER)')]
assert op.index('settings.line_spacing = global_settings.values.line_spacing;')<op.index('reader::open_document_page(')
success=op[op.index('if(opened != reader::OpenResult::FAILED)'):op.index('} else {',op.index('if(opened != reader::OpenResult::FAILED)'))]
assert 'reader::remember_global_book(global_settings.values, open_name)' in success
assert 'global_settings.save()' in success
assert 'file.save_footer(' not in success
reader=s[s.index('} else if(scene == Scene::READER)'):s.index('} else if(scene == Scene::SETTINGS)')]
# Hold Up toggles and saves the choice; Select opens Settings; nothing else saves globals.
toggle=reader[reader.index('if(reader_action == 1)'):reader.index('} else if(forward_pressed)')]
assert 'global_settings.values.shoulder_page_turns = reader_hold.shoulder_page_turns;' in toggle
assert toggle.count('global_settings.save()')==1 and 'file.save_footer(' not in toggle
assert 'reader_action == 2' not in s
select=reader[reader.index('} else if(bn::keypad::select_pressed())'):reader.index('const bool idle_frame')]
assert 'open_settings(true);' in select and 'file.close()' not in select
settings=s[s.index('} else if(scene == Scene::SETTINGS)'):s.index('} else if(scene == Scene::ABOUT)')]
assert settings.count('global_settings.save()')==1
assert 'file.save_footer(' not in settings
assert 'global_settings.values.shoulder_page_turns = reader_hold.shoulder_page_turns;' in settings
assert 'shoulder_startup' not in s
start=reader[reader.index('} else if(bn::keypad::start_pressed())'):reader.index('} else if(bn::keypad::select_pressed())')]
assert 'file.save_footer(' in start and 'global_settings.save()' in start
assert 'reader::save_status_message(saved, globals_saved)' in start
assert 'Settings save failed' in s
# Home: Select opens Settings, Start the import browser.
home=s[s.index('if(scene == Scene::LIBRARY) {'):s.index('} else if(scene == Scene::READER)')]
assert home.index('open_settings(false);')<home.index('enter_import("/");')
print('PASS: global boot/open/settings/manual-save wiring, saved L/R choice, Select/Start routes')
