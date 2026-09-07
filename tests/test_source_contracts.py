#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text(encoding="utf-8")
core = (root / "src/reader_core.cpp").read_text(encoding="utf-8")
header = (root / "include/reader_core.h").read_text(encoding="utf-8")
file_source = (root / "src/reader_file.cpp").read_text(encoding="utf-8")
save_header = (root / "include/reader_txt_save.h").read_text(encoding="utf-8")
save_source = (root / "src/reader_txt_save.cpp").read_text(encoding="utf-8")
ui_source = (root / "src/reader_ui_state.cpp").read_text(encoding="utf-8")
string_shims = (root / "src/string_shims.c").read_text(encoding="utf-8")
ffconf = (root / "include/ffconf.h").read_text(encoding="utf-8")
epub_header = (root / "include/epub_document.h").read_text(encoding="utf-8")
makefile = (root / "Makefile").read_text(encoding="utf-8")
workflow = (root / ".github/workflows/build-rom.yml").read_text(encoding="utf-8")

assert "draw_text_idx8_bus16_range" in main
assert "bn::sprite_font ui_font(" in main
assert "bn::sprite_items::ui_variable_8x16_font" in main
assert "constexpr int FONT_HEIGHT = 16;" in header
assert "const bool arabic" in core
assert "UI_SPRITE_CAPACITY" in main
assert "LIBRARY_WORST_CASE_SPRITES < 128" in main
assert "char* strcpy(char* destination, const char* source)" in string_shims
assert "#define FF_FS_READONLY\t0" in ffconf
assert "#define FF_MAX_LFN\t\t255" in ffconf
assert "#define FF_LFN_BUF\t\t255" in ffconf
assert "BN_DATA_EWRAM_BSS reader::EpubDocument epub;" in main
assert "BN_DATA_EWRAM_BSS reader::PageHistoryRebuild history_rebuild;" in main
assert "bool shoulder_page_turns = false;" in main
assert "shoulder_page_turns = ! shoulder_page_turns;" in main
assert "shoulder_page_turns && bn::keypad::r_pressed()" in main
assert "shoulder_page_turns && bn::keypad::l_pressed()" in main

# Save results are timed without blocking input or delaying the main loop.
assert "reader::start_save_message(save_message_timer);" in main
assert "reader::tick_save_message(save_message_timer)" in main
assert "SAVE_MESSAGE_FRAMES = 90" in (root / "include/reader_ui_state.h").read_text()
assert "--timer.frames_remaining" in ui_source
assert "bn::core::update();\n                const bool saved = file.save_footer(" in main
assert "active_source == &epub ? active_source : nullptr" in main
assert "reader::TxtSaveFooter footer{page.start_offset, settings, history, history_rebuild};" in main

# Version-2 saves carry the circular page history; version 1 remains readable.
assert "TXT_SAVE_FOOTER_V1_SIZE = 96" in save_header
assert "TXT_SAVE_FOOTER_SIZE = 800" in save_header
assert "PageHistory history;" in save_header
assert "V2_HISTORY_AT" in save_source
assert "parse_v1" in save_source
# EPUB caches are standard ZIP members; no private trailer or post-EOCD bytes remain.
assert "META-INF/gbareader/cache-v5" in file_source
assert "META-INF/gbareader/state-v5" in file_source
assert "GBAREPC5" in file_source
assert "append_cache_and_state" in file_source
assert "local_header" in file_source
assert "final_eocd" in file_source
assert "EPUB_CACHE_TRAILER" not in file_source
assert "write_epub_cache_transaction" not in file_source
# Butano's ROM link omits these hosted C-string routines; use bounded local comparisons/copies.
assert "std::strncpy" not in file_source
assert "std::strncmp" not in (root / "src/epub_document.cpp").read_text(encoding="utf-8")

# Back navigation never performs a synchronous scan from byte zero.
previous = core[core.index("bool previous_page"):core.index("void begin_history_rebuild")]
assert "while" not in previous
assert "layout_page(source, 0" not in previous
assert "step_history_rebuild" in core
assert "reader::step_history_rebuild" in main
assert 'show_overlay(save_ui, save_sprites, "Loading back...")' in main

assert "GBA Reader v0.8.0" in main
assert "GBA Reader v0.8.0" in makefile
assert "release/v0.8.0" in workflow
assert "pull_request:" in workflow and "      - main" in workflow
assert "contents: read" in workflow
assert "tests/fatfs/run.py" in workflow
assert "GBAReader-${{ github.sha }}" in workflow
assert "Publish v0.5.0" not in workflow

assert "EPUB_MAX_ZIP_ENTRIES" not in epub_header
assert "TOO_MANY_ENTRIES" not in epub_header
assert "uint32_t _central_offset" in epub_header
assert "EPUB_MAX_SPINE_ITEMS = 256" in epub_header
assert "struct SpineItem { uint32_t central_offset;" in epub_header
assert "EPUB_TEXT_WINDOW_BYTES = 16 * 1024" in epub_header
assert "EPUB_MAX_XHTML_BYTES = 32 * 1024 * 1024" in epub_header
assert "COMPRESSED_ENTRY_TOO_LARGE" in epub_header
assert 'reader_font_base_addr' in (root / "references/superfw/src/fonts/font_render.c").read_text()
assert 'ientry == 0xFFFF' in (root / "references/superfw/src/fonts/font_render.c").read_text()
assert 'reader-symbols.pack' in (root / "src/superfw_font_pack.s").read_text()
assert "idrefs[EPUB_MAX_SPINE_ITEMS]" not in (root / "src/epub_document.cpp").read_text()
# ZIP local-name validation reuses the already buffered central bytes.
epub_source = (root / "src/epub_document.cpp").read_text()
parse_zip = epub_source[epub_source.index("bool EpubDocument::parse_zip()"):epub_source.index("int EpubDocument::find_entry")]
assert "central_char" not in parse_zip, "local comparison must not reread central names"
assert "std::memcmp(central_name,local_name,nl)" in parse_zip
assert "read_bytes(*_archive,lo+30,local_name,nl)" in parse_zip
# Settings have no live preview: pause reconstruction and defer layout until exit.
settings_entry = main[main.index("} else if(bn::keypad::down_pressed())"):main.index("} else if(bn::keypad::start_pressed())")]
assert "history_rebuild = {}" not in settings_entry, "settings entry must preserve reconstruction"
assert "settings_before = settings" in settings_entry
settings_ui = main[main.index("if(bn::keypad::up_pressed() && settings_row"):main.index("if(scene == Scene::READER && reader::tick_save_message")]
assert "reader::layout_page" not in settings_ui, "no intermediate settings layout"
assert "if(!reader::same_settings(settings_before, settings))" in settings_ui
# Cached text is hashed once; the ZIP-member CRC combines finalized header/text CRCs.
cache_load = epub_source[epub_source.index("bool EpubDocument::load_owned_cache()"):epub_source.index("bool EpubDocument::parse_zip()")]
assert "whole_crc = crc32_update(whole_crc, block, take)" not in cache_load, "cache text must not be hashed twice"
assert "crc32_combine(" in cache_load
# Both readers use the same bounded read-only CRC primitive (no per-bit hot loops).
assert '#include "reader_crc32.h"' in epub_source and '#include "reader_crc32.h"' in file_source
assert "0xEDB88320" not in epub_source and "0xEDB88320" not in file_source
print("PASS: source contracts")
