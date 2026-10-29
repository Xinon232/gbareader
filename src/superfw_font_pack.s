    .section .rodata
    .align 2
    .global g_superfw_font_pack
g_superfw_font_pack:
    @ Butano assembles from build/, so the project root is one level up.
    .incbin "../references/superfw/res/fonts.pack"
    .global g_superfw_font_pack_end
g_superfw_font_pack_end:

    .align 2
    .global g_reader_font_pack
g_reader_font_pack:
    .incbin "../references/superfw/res/reader-symbols.pack"
    .global g_reader_font_pack_end
g_reader_font_pack_end:

    .section .data
    .align 2
    .global font_base_addr
font_base_addr:
    .word g_superfw_font_pack
    .global reader_font_base_addr
reader_font_base_addr:
    .word g_reader_font_pack
