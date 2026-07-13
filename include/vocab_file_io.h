// vocab_file_io.h — SD/FAT-backed vocab file boundary.
#pragma once

#include "vocab.h"

constexpr int VOCAB_FILE_BUFFER_LEN = 2048;   // fallback/sample buffer only
constexpr int VOCAB_EXPORT_BUFFER_LEN = 4096; // host/stub export scratch only
constexpr int VOCAB_MAX_BROWSER_FILES = 32;
constexpr int VOCAB_FILENAME_MAX = 64;

// Mount/scan storage. On GBA this calls libfat and scans .txt files on SD.
// On host it exposes deterministic sample files for tests.
bool vocab_file_init();
bool vocab_file_sd_ready();
bool vocab_file_loaded_from_sd();

int vocab_file_count();
const char* vocab_file_name(int index);

// Load selected file. If SD is mounted on GBA, this streams the file once and
// fills vf.line_offsets/field[] without copying the file into RAM. If SD is not
// mounted or this is a host build, it falls back to small built-in samples.
bool vocab_file_load(const char* filename, VocabFile& vf,
                     char* fallback_buf, int fallback_len, int& fallback_used);

// Read one vocab row from the currently loaded source: fseek+read one line on
// SD, or memory-buffer lookup for fallback/host.
bool vocab_file_show(const VocabFile& vf, const char* fallback_buf, int fallback_used,
                     int line_idx, LineBuf& out);

// Host regression instrumentation for the bounded current-card cache. A miss
// means the source row had to be read and parsed; hits still refresh field.
void vocab_file_cache_reset_stats_for_tests();
int vocab_file_cache_misses_for_tests();

// Run the same chunked sequential scanner used by FatFS against a host memory
// source. This keeps buffer-boundary and call-count regressions testable.
int vocab_file_scan_buffered_for_tests(const char* data, int data_len, int chunk_size,
                                       VocabFile& vf, int& bulk_read_calls);

// Save/export current fields as dict.cc-style grouped TXT. On GBA+SD this full
// rewrites to a temporary file, then replaces the original. On fallback/host it
// writes grouped text into out_buf for testability.
bool vocab_file_save_grouped(VocabFile& vf, const char* fallback_buf, int fallback_used,
                             char* out_buf, int out_len, int& out_used);

// Backwards-compatible small-sample helpers used by older tests.
bool vocab_file_read_builtin_or_stub(const char* filename, char* out, int out_len, int& out_used);
bool vocab_file_export_grouped_stub(const VocabFile& vf, const char* source, int source_len,
                                    char* out, int out_len, int& out_used);
