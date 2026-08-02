#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text(encoding="utf-8")
core = (root / "src/reader_core.cpp").read_text(encoding="utf-8")
header = (root / "include/reader_core.h").read_text(encoding="utf-8")
file_source = (root / "src/reader_file.cpp").read_text(encoding="utf-8")
ffconf = (root / "include/ffconf.h").read_text(encoding="utf-8")
epub_header = (root / "include/epub_document.h").read_text(encoding="utf-8")

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
assert "offset - _cache_start >= uint32_t(_cache_size)" in file_source
assert file_source.index("_cache_size = 0;") < file_source.index("f_lseek(&_file, _cache_start)")
assert "#define FF_FS_READONLY\t1" in ffconf
assert "BN_DATA_EWRAM_BSS reader::EpubDocument epub;" in main
assert "GBA Reader v0.2.0" in main
assert "EPUB_MAX_ZIP_ENTRIES = 128" in epub_header
assert "EPUB_MAX_SPINE_ITEMS = 64" in epub_header
assert "EPUB_MAX_CHAPTER_BYTES = 64 * 1024" in epub_header
assert "idrefs[EPUB_MAX_SPINE_ITEMS]" not in (root / "src/epub_document.cpp").read_text()
print("PASS: source contracts")
