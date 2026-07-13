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

static int test_transaction_failures()
{
    const VocabIoFailurePoint failures[] = {
        VOCAB_IO_FAIL_WRITE, VOCAB_IO_FAIL_CLOSE, VOCAB_IO_FAIL_BACKUP_RENAME,
        VOCAB_IO_FAIL_REPLACEMENT_RENAME, VOCAB_IO_FAIL_REINDEX,
        VOCAB_IO_FAIL_BACKUP_UNLINK
    };
    for (VocabIoFailurePoint failure : failures) {
        VocabTransactionTestResult result = vocab_file_transaction_for_tests(failure);
        if (result.success || !result.dirty || !result.original_valid) {
            return fail("failure injection lost dirty state/original");
        }
    }
    VocabTransactionTestResult success =
        vocab_file_transaction_for_tests(VOCAB_IO_FAIL_NONE);
    if (!success.success || success.dirty || !success.original_valid ||
        success.backup_valid || success.temporary_valid || success.stats.renames != 2) {
        return fail("successful transaction invariant failed");
    }

    char near_max[VOCAB_FILENAME_MAX];
    for (int i = 0; i < VOCAB_FILENAME_MAX - 5; ++i) near_max[i] = 'a';
    near_max[VOCAB_FILENAME_MAX - 5] = '.';
    near_max[VOCAB_FILENAME_MAX - 4] = 't';
    near_max[VOCAB_FILENAME_MAX - 3] = 'x';
    near_max[VOCAB_FILENAME_MAX - 2] = 't';
    near_max[VOCAB_FILENAME_MAX - 1] = 0;
    char sidecar[VOCAB_FILENAME_MAX];
    if (!vocab_file_sidecar_name_for_tests(near_max, ".bak", sidecar) ||
        std::strlen(sidecar) != std::strlen(near_max) ||
        std::strcmp(sidecar + std::strlen(sidecar) - 4, ".bak") != 0) {
        return fail("near-limit sidecar path truncated or malformed");
    }
    return 0;
}

static int test_structural_validator()
{
    const char* accepted[] = {
        "hello\tworld", "français\tüber", "дом\tслово", "بيت\tدار",
        "日本語\tかな", "中文\t词", "한국어\t단어", "left\tright\r",
        "left\tright\n"
    };
    for (const char* row : accepted) {
        int len = (int)std::strlen(row);
        LineBuf parsed;
        if (!vocab_validate_raw_row(row, len) || !parse_line_into(row, len, parsed)) {
            return fail("validator rejected a supported structural row");
        }
    }

    const char* rejected[] = {
        "malformed", "\tright", "left\t", " \t right", "left\t \r\n", "\t"
    };
    for (const char* row : rejected) {
        int len = (int)std::strlen(row);
        LineBuf parsed;
        if (vocab_validate_raw_row(row, len) || parse_line_into(row, len, parsed)) {
            return fail("validator accepted malformed/empty structural row");
        }
    }

    std::string max_row(95, 'a');
    max_row += '\t';
    max_row += std::string(95, 'b');
    LineBuf max_parsed;
    if ((int)max_row.size() != VOCAB_RAW_LINE_MAX - 1 ||
        !vocab_validate_raw_row(max_row.data(), (int)max_row.size()) ||
        !parse_line_into(max_row.data(), (int)max_row.size(), max_parsed)) {
        return fail("validator rejected maximum final row");
    }
    std::string too_long = max_row + "x";
    if (vocab_validate_raw_row(too_long.data(), (int)too_long.size())) {
        return fail("validator accepted overlong row");
    }
    return 0;
}

int main()
{
    if (test_transaction_failures()) return 1;
    if (test_structural_validator()) return 1;

    VocabFile vf;
    char source[VOCAB_FILE_BUFFER_LEN];
    int source_used = 0;
    if (!vocab_file_load("builtin.txt", vf, source, sizeof(source), source_used)) {
        return fail("could not load built-in source");
    }
    if (vocab_any_dirty(vf) || !vocab_field_counts_valid(vf)) {
        return fail("fresh-load dirty/count invariant failed");
    }

    char save_output[VOCAB_EXPORT_BUFFER_LEN];
    int save_used = -1;
    vocab_file_io_reset_stats();
    if (!vocab_file_save_grouped(vf, source, source_used,
                                 save_output, sizeof(save_output), save_used)) {
        return fail("no-change save did not return success");
    }
    VocabIoStats no_change = vocab_file_io_stats();
    if (save_used != 0 || no_change.write_calls != 0 || no_change.index_scans != 0) {
        return fail("no-change save performed rewrite/reindex work");
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
    VocabIoStats unchanged_frames = vocab_file_io_stats();
    if (unchanged_frames.read_calls != 1 || unchanged_frames.full_display_parses != 1) {
        return fail("unchanged frames performed additional adapter reads/parses");
    }
    if (std::strcmp(first.a, repeated.a) || std::strcmp(first.b, repeated.b)) {
        return fail("cached text changed");
    }

    vocab_advance(vf, 0);
    if (!vocab_any_dirty(vf) || !vocab_field_counts_valid(vf)) {
        return fail("advance dirty/count invariant failed");
    }
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
    if (vocab_file_scan_buffered_for_tests("", 0, 7, scanned, read_calls) != 0 ||
        scanned.line_count != 0) {
        return fail("zero-card scan failed");
    }
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

    const char* exact_boundaries[] = {
        "abcdefg\tvalue\n",       // tab begins the second 7-byte chunk
        "aaaaaaé\tx\n",           // UTF-8 sequence crosses a chunk boundary
        "a\tbbbbb\n"              // newline begins the second chunk
    };
    for (const char* boundary_row : exact_boundaries) {
        scanned_count = vocab_file_scan_buffered_for_tests(
            boundary_row, (int)std::strlen(boundary_row), 7, scanned, read_calls);
        if (scanned_count != 1) {
            return fail("tab/newline/UTF-8 chunk boundary scan failed");
        }
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
    std::string large_output(160000, '\0');
    int large_output_bytes = vocab_export_grouped(
        scanned, large.data(), (int)large.size(), large_output.data(),
        (int)large_output.size());
    if (large_output_bytes != 150008) {
        return fail("grouped output byte profile changed unexpectedly");
    }
    int buffered_write_calls = (large_output_bytes + 511) / 512;

    std::printf("Render cache profile: first frame %u read/%u parse; 120 repeats +0/+0\n",
                unchanged_frames.read_calls, unchanged_frames.full_display_parses);
    std::printf("Buffered scan profile: %zu bytes, %d cards, %d bulk reads\n",
                large.size(), scanned_count, read_calls);
    std::printf("Buffered save profile: %d bytes, %d writes (legacy small-write path: 10004)\n",
                large_output_bytes, buffered_write_calls);

    if (!vocab_file_load("builtin.txt", vf, source, sizeof(source), source_used)) {
        return fail("reload before changed-save test failed");
    }
    vocab_advance(vf, 0);
    vocab_file_io_reset_stats();
    save_used = 0;
    if (!vocab_file_save_grouped(vf, source, source_used,
                                 save_output, sizeof(save_output), save_used)) {
        return fail("changed fallback save failed");
    }
    VocabIoStats changed = vocab_file_io_stats();
    if (vocab_any_dirty(vf) || save_used <= 0 || changed.write_calls == 0 ||
        changed.bytes_written != (uint32_t)save_used) {
        return fail("successful changed save did not clear dirty/count output");
    }

    std::puts("PASS: current-card cache and buffered scanner performance");
    return 0;
}
