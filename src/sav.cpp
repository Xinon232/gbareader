// sav.cpp — SRAM-backed "last opened list" pointer
// Step 3: persistence across power cycles.
//
// On GBA: real SRAM at 0x0E000000 via butano's bn::sram.
// On host: a static memory buffer that simulates SRAM. The semantics
// are the same — read returns what was previously written, magic +
// version detect uninitialized state.

#include "sav.h"

#ifdef __GBA__
    #include "bn_sram.h"
#endif

#include <cstdio>
#include <cstring>

#ifndef __GBA__

// Host simulation: 32KB static buffer mimics SRAM. Put it in EWRAM
// (not the default IWRAM) to leave IWRAM free for butano's hot code.
// We use the raw __attribute__ form because butano's BN_DATA_EWRAM_BSS
// macro lives in bn_hw_common.h, which isn't in the host include path.
// On GBA the macro is the same: section(".sbss") → EWRAM.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((section(".sbss")))
#endif
static uint8_t sram_sim[32 * 1024];
static bool sram_inited = false;

static void sram_init_if_needed() {
    if (!sram_inited) {
        memset(sram_sim, 0, sizeof(sram_sim));
        sram_inited = true;
    }
}

#endif

bool sav_load(Savestate& out)
{
#ifndef __GBA__
    sram_init_if_needed();
    memcpy(&out, sram_sim + SAV_OFFSET, sizeof(Savestate));
#else
    bn::sram::read_offset(out, SAV_OFFSET);
#endif

    if (out.magic != SAV_MAGIC) {
        return false;  // uninitialized or wrong version
    }
    if (out.version != 1) {
        return false;
    }
    // Ensure null termination on filename.
    out.filename[SAV_FILENAME_MAX - 1] = 0;
    return true;
}

void sav_save(const Savestate& s)
{
    // Force the magic and version to be correct.
    Savestate copy = s;
    copy.magic = SAV_MAGIC;
    copy.version = 1;
    copy.filename[SAV_FILENAME_MAX - 1] = 0;

#ifndef __GBA__
    sram_init_if_needed();
    memcpy(sram_sim + SAV_OFFSET, &copy, sizeof(Savestate));
#else
    bn::sram::write_offset(copy, SAV_OFFSET);
#endif
}

void sav_clear()
{
    Savestate zero;
    memset(&zero, 0, sizeof(zero));

#ifndef __GBA__
    sram_init_if_needed();
    memcpy(sram_sim + SAV_OFFSET, &zero, sizeof(Savestate));
#else
    bn::sram::write_offset(zero, SAV_OFFSET);
#endif
}

#ifndef __GBA__
int sav_test()
{
    printf("=== Savestate test ===\n");
    printf("sizeof(Savestate) = %zu bytes\n", sizeof(Savestate));

    // Clear first to start from known state.
    sav_clear();

    // 1. After clear, load should fail (no magic).
    Savestate s1;
    bool ok1 = sav_load(s1);
    printf("After clear, load() returns: %s (expected false)\n", ok1 ? "true" : "false");
    if (ok1) { fprintf(stderr, "FAIL\n"); return 1; }

    // 2. Save and reload.
    Savestate s2;
    s2.last_field = 3;
    s2.last_line = 42;
    strncpy((char*)s2.filename, "NL-DE-5000.txt", SAV_FILENAME_MAX - 1);
    sav_save(s2);

    Savestate s3;
    bool ok2 = sav_load(s3);
    if (!ok2) { fprintf(stderr, "FAIL: load after save\n"); return 1; }
    if (s3.last_field != 3) { fprintf(stderr, "FAIL: last_field\n"); return 1; }
    if (s3.last_line != 42) { fprintf(stderr, "FAIL: last_line\n"); return 1; }
    if (strcmp((char*)s3.filename, "NL-DE-5000.txt") != 0) {
        fprintf(stderr, "FAIL: filename='%s'\n", s3.filename);
        return 1;
    }
    printf("After save+load, last_field=%u last_line=%u file='%s'\n",
           s3.last_field, s3.last_line, s3.filename);

    // 3. Clear and verify load fails again.
    sav_clear();
    Savestate s4;
    bool ok3 = sav_load(s4);
    if (ok3) { fprintf(stderr, "FAIL: clear didn't reset magic\n"); return 1; }

    printf("PASS: savestate round-trip OK\n");
    return 0;
}
#endif
