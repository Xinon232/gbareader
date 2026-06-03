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

    void reset() {
        line_count = 0;
        loaded = false;
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

// Parse a single line of "source\ttarget\n" into a LineBuf.
// Returns true on success, false on empty/malformed.
// Used by both vocab_open (to build the offset table) and
// vocab_show (to format the current line for display).
bool parse_line_into(const char* line, int line_len, LineBuf& out);

// Phase 1 of file open: streaming pass to build line_offsets[] and
// field[] and field_counts[]. Reads the whole file but does not
// store its text — only the offsets and the field of each line.
// On GBA, this opens the .txt via libugba/FatFS and reads sector
// by sector. On host, it works on a byte buffer.
//
// For v1, this is a host-side test only. The GBA-side implementation
// will come in Step 6 when libugba is wired up.
int vocab_open(VocabFile& vf, const char* data, int data_len);

// Parse one physical TXT row into source/target columns. Exposed so the
// SD/FAT streaming layer can build offsets without loading the file.
bool parse_line_into(const char* line, int line_len, LineBuf& out);

// Phase 2 of file open: for a given line index, stream-read that
// line from the file and populate the LineBuf. On host, this is
// data + line_offsets[i]. On GBA, f_lseek + f_read.
bool vocab_show(VocabFile& vf, const char* data, int data_len,
                int line_idx, LineBuf& out);

// Mark a line's field as changed (advance or reset). Updates
// field_counts[]. Sets the dirty bit for that line. No file I/O.
void vocab_advance(VocabFile& vf, int line_idx);
void vocab_reset(VocabFile& vf, int line_idx);

// Dirty-bit queries / clearing.
bool vocab_is_dirty(const VocabFile& vf, int line_idx);
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
