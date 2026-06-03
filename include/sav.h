// sav.h — SRAM-backed "last opened list" pointer
// Step 3: persistence across power cycles.
//
// SRAM on GBA Supercard: 32KB at 0x0E000000. Battery-backed.
// We use the first 64 bytes to store a small struct with a magic
// number (to detect uninitialized SRAM) and the last-opened filename.

#pragma once

#include <cstdint>

constexpr int SAV_FILENAME_MAX = 56;  // null-terminated, 8.3 + null

// 64 bytes total. Trivially copyable so bn::sram::read/write work.
struct Savestate {
    uint32_t magic;        // 'VOC1' = 0x564F4331
    uint32_t version;      // 1
    uint32_t last_field;   // field of the most recently trained word (1..5)
    uint32_t last_line;    // line index of the most recently trained word
    uint8_t  filename[SAV_FILENAME_MAX];  // null-terminated
};
// sizeof(Savestate) = 4 + 4 + 4 + 4 + 56 = 72 bytes
static_assert(sizeof(Savestate) == 72, "Savestate must be 72 bytes");

constexpr uint32_t SAV_MAGIC = 0x564F4331u;  // 'VOC1'
constexpr int SAV_OFFSET = 0;                // write at start of SRAM

// Read savestate from SRAM. Returns true if a valid savestate was
// found (magic matches, version matches). Returns false if SRAM is
// uninitialized or the savestate is corrupt.
bool sav_load(Savestate& out);

// Write savestate to SRAM. Always succeeds.
void sav_save(const Savestate& s);

// Clear SRAM (used to reset).
void sav_clear();

// Test harness for host: verify the savestate struct layout.
int sav_test();
