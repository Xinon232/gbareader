#ifndef VOCAB_UNSCII_16_SPRITE_FONT_H
#define VOCAB_UNSCII_16_SPRITE_FONT_H

#include "bn_sprite_font.h"
#include "bn_utf8_characters_map.h"
#include "bn_sprite_items_vocab_unscii_16_font.h"

namespace vocab_font
{

constexpr bn::utf8_character unscii_16_sprite_font_utf8_characters[] = {
    "Á", "É", "Í", "Ó", "Ú", "Ü", "Ñ", "á", "é", "í", "ó", "ú", "ü", "ñ", "¡", "¿"
};

constexpr bn::span<const bn::utf8_character> unscii_16_sprite_font_utf8_characters_span(
        unscii_16_sprite_font_utf8_characters);

constexpr auto unscii_16_sprite_font_utf8_characters_map =
        bn::utf8_characters_map<unscii_16_sprite_font_utf8_characters_span>();

constexpr bn::sprite_font unscii_16_sprite_font(
        bn::sprite_items::vocab_unscii_16_font,
        unscii_16_sprite_font_utf8_characters_map.reference());

}

#endif
