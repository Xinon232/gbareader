// test_vocab_file_io_perf.cpp — bounded current-card cache regressions.
#include "vocab.h"
#include "vocab_file_io.h"

#include <cstdio>
#include <cstring>
#include <string>

static int fail(const char* message)
{
    std::printf("FAIL: %s\n", message);
    return 1;
}

int main()
{
    VocabFile vf;
    char source[VOCAB_FILE_BUFFER_LEN];
    int source_used = 0;
    if (!vocab_file_load("builtin.txt", vf, source, sizeof(source), source_used)) {
        return fail("could not load built-in source");
    }

    vocab_file_cache_reset_stats_for_tests();

    LineBuf first;
    if (!vocab_file_show(vf, source, source_used, 0, first)) {
        return fail("initial show failed");
    }
    if (vocab_file_cache_misses_for_tests() != 1) {
        return fail("initial show must parse exactly once");
    }

    LineBuf repeated;
    for (int frame = 0; frame < 120; ++frame) {
        if (!vocab_file_show(vf, source, source_used, 0, repeated)) {
            return fail("unchanged-frame show failed");
        }
    }
    if (vocab_file_cache_misses_for_tests() != 1) {
        return fail("unchanged frames reread/reparsed the card");
    }
    if (std::strcmp(first.a, repeated.a) || std::strcmp(first.b, repeated.b)) {
        return fail("cached text changed");
    }

    vocab_advance(vf, 0);
    if (!vocab_file_show(vf, source, source_used, 0, repeated)) {
        return fail("cached show after field update failed");
    }
    if (repeated.field != vf.field[0]) {
        return fail("cached result did not refresh out.field");
    }
    if (vocab_file_cache_misses_for_tests() != 1) {
        return fail("field-only update should not reread text");
    }

    // Reordering any card array invalidates the cache even if the currently
    // displayed index keeps the same source offset.
    int moved_idx = -1;
    if (!vocab_move_line_to_field_end(vf, 1, 1, moved_idx)) {
        return fail("test reorder failed");
    }
    if (!vocab_file_show(vf, source, source_used, 0, repeated)) {
        return fail("show after reorder failed");
    }
    if (vocab_file_cache_misses_for_tests() != 2) {
        return fail("array reorder did not invalidate cache");
    }

    // A source-offset change at the current numerical index must never return
    // the old card, even without relying on the array generation.
    uint32_t replacement_offset = vf.line_offsets[2];
    vf.line_offsets[0] = replacement_offset;
    if (!vocab_file_show(vf, source, source_used, 0, repeated)) {
        return fail("show after source-offset change failed");
    }
    if (vocab_file_cache_misses_for_tests() != 3) {
        return fail("source-offset change did not miss cache");
    }

    // Reloading the same filename is a new loaded-file generation.
    if (!vocab_file_load("builtin.txt", vf, source, sizeof(source), source_used)) {
        return fail("same-file reload failed");
    }
    if (!vocab_file_show(vf, source, source_used, 0, repeated)) {
        return fail("show after same-file reload failed");
    }
    if (vocab_file_cache_misses_for_tests() != 4) {
        return fail("same-file reload did not invalidate cache");
    }

    // Exercise the production scanner with tiny chunks so rows, CRLF pairs,
    // blank groups, malformed rows, and EOF all cross refill boundaries.
    const char* edge_data =
        "one\tuno\r\n"
        "malformed row\n"
        " \t \r\n"
        "two\tdos\n"
        "\n\r\n"
        "three\ttres";
    VocabFile scanned;
    int read_calls = 0;
    int scanned_count = vocab_file_scan_buffered_for_tests(
        edge_data, (int)std::strlen(edge_data), 7, scanned, read_calls);
    if (scanned_count != 3 || scanned.field_counts[0] != 1 ||
        scanned.field_counts[1] != 1 || scanned.field_counts[2] != 1) {
        return fail("chunked scan changed grouped/EOF semantics");
    }
    if (scanned.line_offsets[0] != 0 ||
        std::strncmp(edge_data + scanned.line_offsets[1], "two\tdos", 7) != 0 ||
        std::strncmp(edge_data + scanned.line_offsets[2], "three\ttres", 10) != 0) {
        return fail("chunked scan produced wrong absolute offsets");
    }
    if (read_calls < 2) {
        return fail("boundary test did not force multiple bulk reads");
    }

    std::string overlong(191, 'x');
    overlong[90] = '\t';
    overlong += "\nvalid\trow\n";
    scanned_count = vocab_file_scan_buffered_for_tests(
        overlong.data(), (int)overlong.size(), 17, scanned, read_calls);
    if (scanned_count != 1) {
        return fail("overlong row was not rejected without losing next row");
    }
    LineBuf valid;
    if (!vocab_show(scanned, overlong.data(), (int)overlong.size(), 0, valid) ||
        std::strcmp(valid.a, "valid") || std::strcmp(valid.b, "row")) {
        return fail("row after overlong row was not indexed correctly");
    }

    // A 150 KiB-class source must use hundreds of 512-byte refills, not one
    // FatFS call per byte. The 5,000-card cap remains intact.
    std::string large;
    large.reserve(150000);
    for (int i = 0; i < VOCAB_MAX_LINES; ++i) {
        char row[40];
        std::snprintf(row, sizeof(row), "word%04d-abcdefgh\ttarget%04d\n", i, i);
        large += row;
    }
    scanned_count = vocab_file_scan_buffered_for_tests(
        large.data(), (int)large.size(), 512, scanned, read_calls);
    if (scanned_count != VOCAB_MAX_LINES) {
        return fail("buffered scan broke the 5,000-card cap");
    }
    int expected_ceiling = ((int)large.size() + 511) / 512 + 1;
    if (read_calls > expected_ceiling) {
        return fail("150 KiB-class scan used more than chunk-count bulk reads");
    }
    std::printf("Buffered scan profile: %zu bytes, %d cards, %d bulk reads\n",
                large.size(), scanned_count, read_calls);

    std::puts("PASS: current-card cache and buffered scanner performance");
    return 0;
}
