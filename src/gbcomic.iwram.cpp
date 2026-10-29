// Unpacking .gbcomic chunks (IWRAM, ARM code): the GBA BIOS LZ77 format.
#include "gbcomic.h"

namespace reader::gbcomic {

bool Book::unpack(const uint8_t* in, uint32_t in_bytes, uint8_t* out, uint32_t out_bytes)
{
    if(in_bytes < 4 || in[0] != 0x10) return false;
    const uint32_t size = in[1] | (in[2] << 8) | (uint32_t(in[3]) << 16);
    if(size != out_bytes) return false;
    uint32_t i = 4, o = 0;
    while(o < size) {
        if(i >= in_bytes) return false;
        uint32_t flags = in[i++];
        for(int bit = 0; bit < 8 && o < size; ++bit, flags <<= 1) {
            if(flags & 0x80) {
                if(i + 1 >= in_bytes) return false;
                const uint32_t length = (in[i] >> 4) + 3u;
                const uint32_t distance = (((in[i] & 15u) << 8) | in[i + 1]) + 1u;
                i += 2;
                if(distance > o || length > size - o) return false;
                const uint8_t* from = out + o - distance;
                for(uint32_t k = 0; k < length; ++k) out[o + k] = from[k];
                o += length;
            } else {
                if(i >= in_bytes) return false;
                out[o++] = in[i++];
            }
        }
    }
    return true;
}

}
