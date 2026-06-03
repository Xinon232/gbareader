#ifndef VOCAB_LATIN_OLD_EXT_SPRITE_FONT_H
#define VOCAB_LATIN_OLD_EXT_SPRITE_FONT_H

#include "bn_sprite_font.h"
#include "bn_utf8_characters_map.h"
#include "bn_sprite_items_vocab_latin_old_ext_font.h"

namespace vocab_font
{

constexpr bn::utf8_character latin_old_ext_sprite_font_utf8_characters[] = {
    "Á", "É", "Í", "Ó", "Ú", "Ü", "Ñ", "á", "é", "í", "ó", "ú", "ü", "ñ", "¡", "¿",
    "Ä", "Ö", "ä", "ö", "ß"
};

constexpr bn::span<const bn::utf8_character> latin_old_ext_sprite_font_utf8_characters_span(
        latin_old_ext_sprite_font_utf8_characters);

constexpr auto latin_old_ext_sprite_font_utf8_characters_map =
        bn::utf8_characters_map<latin_old_ext_sprite_font_utf8_characters_span>();

constexpr bn::sprite_font latin_old_ext_sprite_font(
        bn::sprite_items::vocab_latin_old_ext_font,
        latin_old_ext_sprite_font_utf8_characters_map.reference());

}

#endif
