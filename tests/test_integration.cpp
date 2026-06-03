// test_integration.cpp — end-to-end integration test of vocab + sav
// Build: g++ -std=c++17 -Iinclude src/vocab.cpp src/sav.cpp tests/test_integration.cpp -o test_integration
// Run: ./test_integration data/sample.txt
//
// Simulates what happens on the GBA on real boot:
//   1. Read savestate (first boot: empty)
//   2. Write default savestate
//   3. Read it back, verify
//   4. Open a vocab file
//   5. Advance a few words
//   6. Reset a word
//   7. Save savestate again
//   8. Round-trip: clear, reload, verify state matches

#include "vocab.h"
#include "sav.h"

#include <cstdio>
#include <fstream>
#include <vector>
#include <cstring>

int main(int argc, char* argv[])
{
    const char* path = (argc > 1) ? argv[1] : "data/sample.txt";
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return 1; }
    std::vector<char> data((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    f.close();

    printf("=== Integration test ===\n");

    // Step 1: clear savestate (simulating fresh cartridge).
    sav_clear();
    Savestate s1;
    bool ok = sav_load(s1);
    if (ok) { fprintf(stderr, "FAIL: load after clear returned true\n"); return 1; }
    printf("[1] Clear OK, no savestate\n");

    // Step 2: write default savestate.
    Savestate defaults;
    memset(&defaults, 0, sizeof(defaults));
    defaults.last_field = 1;
    defaults.last_line = 0;
    strncpy((char*)defaults.filename, "NL-DE-5000.txt", SAV_FILENAME_MAX - 1);
    sav_save(defaults);
    printf("[2] Wrote default savestate\n");

    // Step 3: reload and verify.
    Savestate s2;
    if (!sav_load(s2)) { fprintf(stderr, "FAIL: load after save\n"); return 1; }
    if (s2.last_field != 1 || s2.last_line != 0) {
        fprintf(stderr, "FAIL: default values wrong\n"); return 1;
    }
    if (strcmp((char*)s2.filename, "NL-DE-5000.txt") != 0) {
        fprintf(stderr, "FAIL: default filename wrong\n"); return 1;
    }
    printf("[3] Reloaded: last_field=%u last_line=%u file='%s'\n",
           s2.last_field, s2.last_line, s2.filename);

    // Step 4: open vocab file.
    VocabFile vf;
    int loaded = vocab_open(vf, data.data(), data.size());
    if (loaded <= 0) { fprintf(stderr, "FAIL: vocab_open\n"); return 1; }
    printf("[4] Loaded %d vocab pairs\n", loaded);

    // Step 5: advance 3 words through fields.
    vocab_advance(vf, 0);  // 1 -> 2
    vocab_advance(vf, 0);  // 2 -> 3
    vocab_advance(vf, 0);  // 3 -> 4
    vocab_advance(vf, 5);  // 1 -> 2
    vocab_advance(vf, 5);  // 2 -> 3
    vocab_advance(vf, 5);  // 3 -> 4
    vocab_advance(vf, 5);  // 4 -> 5  (graduated)
    printf("[5] Advanced 4 lines: line 0 to field 4, line 5 to field 5\n");
    printf("    field_counts: %u %u %u %u %u\n",
           vf.field_counts[0], vf.field_counts[1], vf.field_counts[2],
           vf.field_counts[3], vf.field_counts[4]);

    // Step 6: reset line 0 (back to field 1).
    vocab_reset(vf, 0);
    printf("[6] Reset line 0: now field=%u\n", vf.field[0]);

    // Step 7: update savestate with current line/field.
    s2.last_field = vf.field[0];
    s2.last_line = 0;
    sav_save(s2);
    printf("[7] Updated savestate: last_field=%u last_line=%u\n",
           s2.last_field, s2.last_line);

    // Step 8: simulate power cycle. Clear all RAM, reload savestate,
    // verify everything is consistent.
    // (We can't actually clear RAM in this test, but we can re-load
    // and verify the savestate round-trips.)
    Savestate s3;
    if (!sav_load(s3)) { fprintf(stderr, "FAIL: load after second save\n"); return 1; }
    if (s3.last_field != 1 || s3.last_line != 0) {
        fprintf(stderr, "FAIL: state not preserved across power cycle sim\n");
        return 1;
    }
    printf("[8] Power-cycle sim OK: last_field=%u last_line=%u file='%s'\n",
           s3.last_field, s3.last_line, s3.filename);

    // Bonus: show a line from each field via the streaming API.
    printf("\n[Bonus] Show first line of each field:\n");
    for (int target_field = 1; target_field <= 5; target_field++) {
        for (int i = 0; i < vf.line_count; i++) {
            if (vf.field[i] == target_field) {
                LineBuf lb;
                if (vocab_show(vf, data.data(), data.size(), i, lb)) {
                    printf("  Field %d, line %d: '%s' <-> '%s'\n",
                           target_field, i, lb.a, lb.b);
                    break;
                }
            }
        }
    }

    printf("\nPASS: full integration test\n");
    return 0;
}
