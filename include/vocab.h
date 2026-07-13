// vocab.h — Streaming vocab data layer
// Step 2b: redesigned for 5000-word scale with bounded RAM (~26KB).
//
// The full text of the .txt file does NOT live in RAM. We keep:
//   - line_offsets[N]  : byte offset of each line in the file
//   - field[N]         : current field 1..5 for each word
//   - dirty[N/8]       : bitset, which lines changed since last save
//   - field_counts[5]  : how many words in each field
//   - current_line_buf : 256B scratch for stream-read one line
//
// Showing a word: f_lseek(line_offsets[i]) + f_read into current_line_buf
// Saving: rewrite the .txt, updating each line with its current field
//         (or the new field for changed lines).
//
// v1.0 save strategy: full rewrite. For a 150KB file, ~50ms. Acceptable.
// v1.x: in-place patches at dirty offsets. Defer.

#pragma once

#include <cstdint>
#include <cstring>

// Hard cap on lines in a .txt file. 5000 is the design target per
// user (2026-06-02). At 272B/pair × 5000 = 1.36MB which doesn't fit
// in EWRAM; we use the streaming layout below instead (~26KB total).
constexpr int VOCAB_MAX_LINES = 5000;

// Max length of a single word (source or target). Real dict.cc samples
// reached 67/77 bytes per side, so keep 96 bytes per side.
constexpr int VOCAB_LINE_MAX = 96;

// Max physical line length accepted by the importer/exporter scratch path.
constexpr int VOCAB_RAW_LINE_MAX = 192;

// One line's worth of parsed data. Held in a small buffer while being
// displayed, not in a big array.
struct LineBuf {
    char a[VOCAB_LINE_MAX];
    char b[VOCAB_LINE_MAX];
    uint8_t field;       // current field 1..5
};

// The full streaming state for one open .txt file. Sized for the
// worst case (5000 lines). At 5000 lines this struct is ~26KB, all
// of which fits in EWRAM.
//
// All arrays are static-sized (not heap-allocated) so the size is
// known at compile time and there's no risk of malloc failure on
// the GBA.
struct VocabFile {
    // 5000 entries × 4 bytes = 20KB
    uint32_t line_offsets[VOCAB_MAX_LINES];

    // 5000 entries × 1 byte = 5KB
    uint8_t field[VOCAB_MAX_LINES];

    // 5000/8 = 625 bytes
    uint8_t dirty[VOCAB_MAX_LINES / 8];

    // 5 × 2 bytes = 10 bytes
    uint16_t field_counts[5];

    // 256 bytes — scratch for stream-read of one line
    char current_line_buf[256];

    int line_count;       // number of valid lines (≤ VOCAB_MAX_LINES)
    bool loaded;          // true after a successful vocab_open

    // Incremented whenever line records are reordered. The bounded
    // current-card cache uses this to reject stale text even when the
    // displayed numerical index and source offset happen to stay unchanged.
    uint32_t array_generation;

    void reset() {
        line_count = 0;
        loaded = false;
        array_generation = 0;
        for (int i = 0; i < VOCAB_MAX_LINES; i++) {
            line_offsets[i] = 0;
            field[i] = 1;
        }
        for (int i = 0; i < VOCAB_MAX_LINES / 8; i++) {
            dirty[i] = 0;
        }
        for (int i = 0; i < 5; i++) {
            field_counts[i] = 0;
        }
        current_line_buf[0] = 0;
    }
};

// Lightweight structural validation used by indexing. It applies the same raw
// length, first-tab split, and whitespace rules as parse_line_into(), without
// any UTF-8/font conversion or Arabic shaping.
bool vocab_validate_raw_row(const char* line, int line_len);

// Fully parse one "source\ttarget" row for display. Returns false on malformed
// input or when converted display text does not fit LineBuf.
bool parse_line_into(const char* line, int line_len, LineBuf& out);

// Phase 1 of file open: a buffered streaming pass builds line_offsets[],
// field[], and field_counts[]. It validates raw row structure only and does not
// store text, perform font mapping, convert display bytes, or shape Arabic.
// The SD adapter uses 512-byte FatFS reads; host tests use the same scanner.
int vocab_open(VocabFile& vf, const char* data, int data_len);

// Phase 2: read and fully parse one physical row only when it must be shown.
// Host uses data + line_offsets[i]; GBA uses one seek and one bounded bulk read.
bool vocab_show(VocabFile& vf, const char* data, int data_len,
                int line_idx, LineBuf& out);

// Mark a line's field as changed (advance or reset). Updates
// field_counts[]. Sets the dirty bit for that line. No file I/O.
void vocab_advance(VocabFile& vf, int line_idx);
void vocab_reset(VocabFile& vf, int line_idx);

// Reorder helpers. These change the review order while keeping each
// line's offset/field/dirty state attached to the same word.
bool vocab_move_line_to_field_end(VocabFile& vf, int line_idx, int field,
                                  int& new_idx);
bool vocab_shuffle_field(VocabFile& vf, int field, uint32_t seed);

// Dirty-bit queries / clearing.
bool vocab_is_dirty(const VocabFile& vf, int line_idx);
bool vocab_any_dirty(const VocabFile& vf);
bool vocab_field_counts_valid(const VocabFile& vf);
void vocab_clear_dirty(VocabFile& vf);

// Serialize a LineBuf back into "a\tb\n" form. Used by save.
// Returns bytes written (always a_len + 1 + b_len + 1), or -1
// if the buffer is too small.
int format_line(char* out_buf, int out_buf_len, const LineBuf& in);

// Export rows grouped by current field, preserving the original row text
// from the source buffer. Writes CRLF line endings and one blank separator
// between fields. Returns bytes written, or -1 if out_buf is too small.
int vocab_export_grouped(const VocabFile& vf, const char* data, int data_len,
                         char* out_buf, int out_buf_len);

// Test harness: run open → show → advance → reset → save round-trip
// on a synthesized in-memory file. Verifies RAM budget and that
// the streaming model produces identical output to a naive
// load-all model. Host-only.
int vocab_streaming_test(const char* sample_data, int sample_len);
