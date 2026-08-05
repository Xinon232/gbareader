#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text(encoding="utf-8")
core = (root / "src/reader_core.cpp").read_text(encoding="utf-8")
header = (root / "include/reader_core.h").read_text(encoding="utf-8")
file_source = (root / "src/reader_file.cpp").read_text(encoding="utf-8")
string_shims = (root / "src/string_shims.c").read_text(encoding="utf-8")
ffconf = (root / "include/ffconf.h").read_text(encoding="utf-8")
epub_header = (root / "include/epub_document.h").read_text(encoding="utf-8")
makefile = (root / "Makefile").read_text(encoding="utf-8")

assert "draw_text_idx8_bus16_range" in main
assert "bn::sprite_font ui_font(" in main
assert "bn::sprite_items::ui_variable_8x16_font" in main
assert "bn::sprite_text_generator ui(ui_font);" in main
assert "constexpr int FONT_HEIGHT = 16;" in header
assert "text size" not in main.lower()
assert "const bool arabic" in core
assert "UI_SPRITE_CAPACITY" in main
assert "LIBRARY_VISIBLE_ROWS" in main
assert "LIBRARY_DISPLAY_CHARACTERS" in main
assert "LIBRARY_WORST_CASE_SPRITES < 128" in main
assert "offset-_cache_start>=uint32_t(_cache_size)" in file_source
assert "char* strcpy(char* destination, const char* source)" in string_shims
assert file_source.index("_cache_size=0;") < file_source.index("f_lseek(&_file,_cache_start)")
assert "#define FF_FS_READONLY\t0" in ffconf
assert "#define FF_MAX_LFN\t\t255" in ffconf
assert "#define FF_LFN_BUF\t\t255" in ffconf
assert "BN_DATA_EWRAM_BSS reader::EpubDocument epub;" in main
assert "reader::Settings settings;" in main
assert "bn::keypad::l_pressed()" in main
assert "file.saved_footer(footer)" in main
assert "file.save_footer(footer)" in main
assert "GBA Reader v0.4.0" in main
assert "GBA Reader v0.4.0" in makefile
assert "EPUB_MAX_ZIP_ENTRIES" not in epub_header
assert "TOO_MANY_ENTRIES" not in epub_header
assert "uint32_t _central_offset" in epub_header
assert "EPUB_MAX_SPINE_ITEMS = 256" in epub_header
assert "struct SpineItem { uint32_t central_offset;" in epub_header
assert "EPUB_TEXT_WINDOW_BYTES = 16 * 1024" in epub_header
assert "EPUB_MAX_XHTML_BYTES = 4 * 1024 * 1024" in epub_header
assert "idrefs[EPUB_MAX_SPINE_ITEMS]" not in (root / "src/epub_document.cpp").read_text()
print("PASS: source contracts")
