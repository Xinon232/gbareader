#pragma once
#include <cstdint>

namespace reader {
namespace crc32_detail {
// Two nibble lookups per byte: 64 bytes in ROM, no mutable table or init cost.
inline constexpr uint32_t table[16] = {
    0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
    0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
    0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
    0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu
};
inline constexpr uint32_t matrix_times(const uint32_t* matrix, uint32_t vector)
{
    uint32_t sum = 0;
    while(vector) {
        if(vector & 1) sum ^= *matrix;
        vector >>= 1;
        ++matrix;
    }
    return sum;
}
inline constexpr void matrix_square(uint32_t* square, const uint32_t* matrix)
{
    for(int i = 0; i < 32; ++i) square[i] = matrix_times(matrix, matrix[i]);
}
// 4096 bytes are 2^15 bits. Build the shift operator at compile time:
// 128 bytes of read-only ROM, no runtime squaring or additional RAM per block.
struct BlockShift { uint32_t rows[32]{}; };
inline constexpr BlockShift make_block_shift()
{
    BlockShift matrix{};
    matrix.rows[0] = 0xEDB88320u;
    for(int i = 1; i < 32; ++i) matrix.rows[i] = uint32_t(1) << (i - 1);
    for(int power = 0; power < 15; ++power) {
        BlockShift next{};
        matrix_square(next.rows, matrix.rows);
        matrix = next;
    }
    return matrix;
}
inline constexpr BlockShift block_shift = make_block_shift();
}

inline uint32_t crc32_combine_4096(uint32_t first, uint32_t second)
{
    return crc32_detail::matrix_times(crc32_detail::block_shift.rows, first) ^ second;
}

// Updates take/return an unfinalized state; bytes/combine use finalized CRCs.
inline uint32_t crc32_update(uint32_t crc, const unsigned char* data, uint32_t size)
{
    for(uint32_t i = 0; i < size; ++i) {
        crc ^= data[i];
        crc = (crc >> 4) ^ crc32_detail::table[crc & 15];
        crc = (crc >> 4) ^ crc32_detail::table[crc & 15];
    }
    return crc;
}
inline uint32_t crc32_bytes(const unsigned char* data, uint32_t size)
{
    return ~crc32_update(0xffffffffu, data, size);
}

// Existing save-side GF(2) combination shared with cache loading. Bounded
// 256-byte matrix workspace; length is the second input's byte length.
inline uint32_t crc32_combine(uint32_t first, uint32_t second, uint32_t length)
{
    using namespace crc32_detail;
    if(!length) return first;
    uint32_t odd[32], even[32];
    odd[0] = 0xEDB88320u;
    uint32_t row = 1;
    for(int i = 1; i < 32; ++i) { odd[i] = row; row <<= 1; }
    matrix_square(even, odd);
    matrix_square(odd, even);
    do {
        matrix_square(even, odd);
        if(length & 1) first = matrix_times(even, first);
        length >>= 1;
        if(!length) break;
        matrix_square(odd, even);
        if(length & 1) first = matrix_times(odd, first);
        length >>= 1;
    } while(length);
    return first ^ second;
}
}
