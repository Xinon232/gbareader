#include "reader_turn.h"

namespace reader {

void turn_blit(const uint8_t* canvas, uint16_t* screen_page, Angle angle)
{
    volatile uint16_t* out = screen_page;
    switch(angle) {
    case Angle::RIGHT:
        // Screen row y is canvas column 159 - y, read downwards.
        for(int y = 0; y < 160; ++y) {
            const uint8_t* column = canvas + (159 - y);
            for(int x = 0; x < 240; x += 2, column += 320)
                *out++ = uint16_t(column[0] | (column[160] << 8));
        }
        break;
    case Angle::LEFT:
        // Screen row y is canvas column y, read upwards.
        for(int y = 0; y < 160; ++y) {
            const uint8_t* column = canvas + 239 * 160 + y;
            for(int x = 0; x < 240; x += 2, column -= 320)
                *out++ = uint16_t(column[0] | (column[-160] << 8));
        }
        break;
    case Angle::UPSIDE_DOWN:
        for(int y = 0; y < 160; ++y) {
            const uint8_t* row = canvas + (159 - y) * 240 + 239;
            for(int x = 0; x < 240; x += 2, row -= 2)
                *out++ = uint16_t(row[0] | (row[-1] << 8));
        }
        break;
    default:
        for(int i = 0; i < 240 * 160; i += 2)
            *out++ = uint16_t(canvas[i] | (canvas[i + 1] << 8));
        break;
    }
}

unsigned turn_keys(unsigned physical, Angle angle)
{
    const unsigned dpad = KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT;
    const bool up = physical & KEY_UP, down = physical & KEY_DOWN;
    const bool left = physical & KEY_LEFT, right = physical & KEY_RIGHT;
    unsigned seen = 0;
    switch(angle) {
    case Angle::RIGHT:  // D-pad on top: its Up points to the reader's right.
        if(up) seen |= KEY_RIGHT;
        if(down) seen |= KEY_LEFT;
        if(left) seen |= KEY_UP;
        if(right) seen |= KEY_DOWN;
        break;
    case Angle::LEFT:
        if(up) seen |= KEY_LEFT;
        if(down) seen |= KEY_RIGHT;
        if(left) seen |= KEY_DOWN;
        if(right) seen |= KEY_UP;
        break;
    case Angle::UPSIDE_DOWN:
        if(up) seen |= KEY_DOWN;
        if(down) seen |= KEY_UP;
        if(left) seen |= KEY_RIGHT;
        if(right) seen |= KEY_LEFT;
        break;
    default:
        return physical;
    }
    return (physical & ~dpad) | seen;
}

}
