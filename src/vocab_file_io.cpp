// vocab_file_io.cpp — SD/FatFS streaming implementation.

#include "vocab_file_io.h"

#include <cstring>

#ifdef __DEVKITARM__
#include "bn_core.h"
#include "fatfs/ff.h"
#include "gbahw.h"
extern "C" {
#include "supercard_driver.h"
}
#endif

static char s_names[VOCAB_MAX_BROWSER_FILES][VOCAB_FILENAME_MAX];
static int s_name_count = 0;
static bool s_sd_ready = false;
static bool s_loaded_from_sd = false;
static char s_loaded_name[VOCAB_FILENAME_MAX];
static uint32_t s_loaded_generation = 0;
static VocabIoStats s_io_stats = {};

void vocab_file_io_reset_stats()
{
    s_io_stats = {};
}

VocabIoStats vocab_file_io_stats()
{
    return s_io_stats;
}

// One parsed card (~193 bytes), keyed by source generation + array generation
// + physical source offset. This keeps ordinary/feedback/animation frames off
// the SD card while remaining bounded regardless of vocabulary-file size.
static bool s_card_cache_valid = false;
static uint32_t s_card_cache_loaded_generation = 0;
static uint32_t s_card_cache_array_generation = 0;
static uint32_t s_card_cache_offset = 0;
#ifdef __DEVKITARM__
BN_DATA_EWRAM_BSS static LineBuf s_card_cache_line;
#else
static LineBuf s_card_cache_line;
#endif
static int s_card_cache_misses = 0;

static void invalidate_card_cache()
{
    s_card_cache_valid = false;
}

static void begin_loaded_file_generation()
{
    ++s_loaded_generation;
    if (s_loaded_generation == 0) {
        ++s_loaded_generation;
    }
    invalidate_card_cache();
}

void vocab_file_cache_reset_stats_for_tests()
{
    s_card_cache_misses = 0;
    invalidate_card_cache();
}

int vocab_file_cache_misses_for_tests()
{
    return s_card_cache_misses;
}

template<typename Ops>
static bool run_replacement_transaction(Ops& ops)
{
    if (!ops.rename_original_to_backup()) return false;
    if (!ops.rename_temporary_to_original()) {
        ops.restore_backup();
        return false;
    }
    if (!ops.reindex_replacement()) {
        if (ops.park_failed_replacement()) ops.restore_backup();
        return false;
    }
    return ops.remove_backup();
}

#ifndef __DEVKITARM__
class HostTransactionOps {
public:
    explicit HostTransactionOps(VocabIoFailurePoint failure) :
        failure_(failure), original_(true), backup_(false), temporary_(true),
        reindex_ok_(false), stats_{} {}

    bool rename_original_to_backup() {
        ++stats_.renames;
        if (failure_ == VOCAB_IO_FAIL_BACKUP_RENAME) return false;
        original_ = false; backup_ = true; return true;
    }
    bool rename_temporary_to_original() {
        ++stats_.renames;
        if (failure_ == VOCAB_IO_FAIL_REPLACEMENT_RENAME) return false;
        temporary_ = false; original_ = true; return true;
    }
    bool reindex_replacement() {
        ++stats_.index_scans;
        reindex_ok_ = failure_ != VOCAB_IO_FAIL_REINDEX;
        return reindex_ok_;
    }
    bool park_failed_replacement() {
        ++stats_.renames;
        if (!original_ || temporary_) return false;
        original_ = false; temporary_ = true; return true;
    }
    bool restore_backup() {
        ++stats_.renames;
        if (!backup_ || original_) return false;
        backup_ = false; original_ = true; return true;
    }
    bool remove_backup() {
        ++stats_.unlinks;
        if (!backup_ || failure_ == VOCAB_IO_FAIL_BACKUP_UNLINK) return false;
        backup_ = false; return true;
    }

    bool original() const { return original_; }
    bool backup() const { return backup_; }
    bool temporary() const { return temporary_; }
    const VocabIoStats& stats() const { return stats_; }

private:
    VocabIoFailurePoint failure_;
    bool original_;
    bool backup_;
    bool temporary_;
    bool reindex_ok_;
    VocabIoStats stats_;
};

VocabTransactionTestResult vocab_file_transaction_for_tests(VocabIoFailurePoint failure)
{
    VocabTransactionTestResult result = {};
    result.dirty = true;
    result.original_valid = true;
    if (failure == VOCAB_IO_FAIL_WRITE || failure == VOCAB_IO_FAIL_CLOSE) {
        result.stats.write_calls = 1;
        result.stats.closes = failure == VOCAB_IO_FAIL_CLOSE ? 1 : 0;
        result.temporary_valid = false;
        return result;
    }

    HostTransactionOps ops(failure);
    result.success = run_replacement_transaction(ops);
    result.dirty = !result.success;
    result.original_valid = ops.original();
    result.backup_valid = ops.backup();
    result.temporary_valid = ops.temporary();
    result.stats = ops.stats();
    return result;
}
#endif

#ifdef __DEVKITARM__
static FATFS s_fatfs;
// Save-only reindex scratch: bounded metadata (offsets/fields/dirty/counts), not
// vocabulary text. Keeping it static places it in normal EWRAM/BSS rather than
// on the small GBA stack and preserves the live dirty state if reindex fails.
BN_DATA_EWRAM_BSS static VocabFile s_reindex_scratch;
BN_DATA_EWRAM_BSS alignas(4) static char s_sequential_read_buffer[512];
BN_DATA_EWRAM_BSS alignas(4) static char s_save_write_buffer[512];

static FRESULT tracked_open(FIL* fp, const char* path, BYTE mode)
{
    ++s_io_stats.file_opens;
    return f_open(fp, path, mode);
}

static FRESULT tracked_close(FIL* fp)
{
    ++s_io_stats.closes;
    return f_close(fp);
}

static FRESULT tracked_seek(FIL* fp, FSIZE_t offset)
{
    ++s_io_stats.seeks;
    return f_lseek(fp, offset);
}

static FRESULT tracked_read(FIL* fp, void* buffer, UINT bytes, UINT* read)
{
    ++s_io_stats.read_calls;
    FRESULT result = f_read(fp, buffer, bytes, read);
    if (read) s_io_stats.bytes_read += *read;
    return result;
}

static FRESULT tracked_write(FIL* fp, const void* buffer, UINT bytes, UINT* written)
{
    ++s_io_stats.write_calls;
    FRESULT result = f_write(fp, buffer, bytes, written);
    if (written) s_io_stats.bytes_written += *written;
    return result;
}

static FRESULT tracked_sync(FIL* fp)
{
    return f_sync(fp);
}

static FRESULT tracked_rename(const char* from, const char* to)
{
    ++s_io_stats.renames;
    return f_rename(from, to);
}

static FRESULT tracked_unlink(const char* path)
{
    ++s_io_stats.unlinks;
    return f_unlink(path);
}
#endif

static bool str_eq_local(const char* a, const char* b)
{
    if (!a || !b) return false;
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static bool has_txt_ext(const char* name)
{
    if (!name) return false;
    int len = 0;
    while (name[len]) len++;
    if (len < 5 || len >= VOCAB_FILENAME_MAX) return false;
    const char* e = name + len - 4;
    return (e[0] == '.') &&
           (e[1] == 't' || e[1] == 'T') &&
           (e[2] == 'x' || e[2] == 'X') &&
           (e[3] == 't' || e[3] == 'T');
}

static bool make_sidecar_name(const char* original, const char* suffix,
                              char out[VOCAB_FILENAME_MAX])
{
    if (!original || !suffix || !has_txt_ext(original)) return false;
    int len = 0;
    while (original[len]) {
        if (len >= VOCAB_FILENAME_MAX - 1) return false;
        out[len] = original[len];
        ++len;
    }
    if (len < 4 || suffix[0] != '.' || !suffix[1] || !suffix[2] || !suffix[3] || suffix[4]) {
        return false;
    }
    out[len - 4] = suffix[0];
    out[len - 3] = suffix[1];
    out[len - 2] = suffix[2];
    out[len - 1] = suffix[3];
    out[len] = 0;
    return !str_eq_local(original, out);
}

bool vocab_file_sidecar_name_for_tests(const char* original, const char* suffix,
                                       char out[VOCAB_FILENAME_MAX])
{
    return make_sidecar_name(original, suffix, out);
}

static void add_name(const char* name)
{
    if (!name || s_name_count >= VOCAB_MAX_BROWSER_FILES) return;
    for (int i = 0; i < s_name_count; i++) {
        if (str_eq_local(s_names[i], name)) return;
    }
    int j = 0;
    while (name[j] && j < VOCAB_FILENAME_MAX - 1) {
        s_names[s_name_count][j] = name[j];
        j++;
    }
    s_names[s_name_count][j] = 0;
    s_name_count++;
}

static void add_builtin_names()
{
    add_name("builtin.txt");
    add_name("NL-DE-5000.txt");
    add_name("ES-DE-vocab.txt");
}

static void ensure_names_for_tests_or_fallback()
{
    if (s_name_count == 0) {
        add_builtin_names();
    }
}

static bool copy_text(const char* text, char* out, int out_len, int& out_used)
{
    if (!text || !out || out_len <= 0) return false;
    int len = 0;
    while (text[len] && len < out_len - 1) {
        out[len] = text[len];
        len++;
    }
    out[len] = 0;
    out_used = len;
    return text[len] == 0;
}

bool vocab_file_read_builtin_or_stub(const char* filename, char* out, int out_len, int& out_used)
{
    static const char* builtin =
        "English\tEnglish\n"
        "français\tFrench\n"
        "Deutsch\tGerman\n"
        "español\tSpanish\n"
        "português\tPortuguese\n"
        "italiano\tItalian\n"
        "svenska\tSwedish\n"
        "dansk\tDanish\n"
        "norsk\tNorwegian\n"
        "suomi\tFinnish\n"
        "íslenska\tIcelandic\n"
        "føroyskt\tFaroese\n"
        "Nederlands\tDutch\n"
        "polski\tPolish\n"
        "čeština\tCzech\n"
        "Türkçe\tTurkish\n"
        "Ελληνικά\tGreek\n"
        "русский\tRussian\n"
        "українська\tUkrainian\n"
        "日本語\tJapanese\n"
        "中文\tChinese\n"
        "한국어\tKorean\n";

    static const char* nl_de =
        "hond\tHund\r\n"
        "kat\tKatze\r\n"
        "\r\n"
        "boom\tBaum\r\n"
        "huis\tHaus\r\n"
        "\r\n"
        "boek\tBuch\r\n";

    static const char* es_de =
        "perro\tHund\r\n"
        "gato\tKatze\r\n"
        "agua\tWasser\r\n"
        "\r\n"
        "pan\tBrot\r\n"
        "escuela\tSchule\r\n";

    if (str_eq_local(filename, "builtin.txt")) return copy_text(builtin, out, out_len, out_used);
    if (str_eq_local(filename, "NL-DE-5000.txt")) return copy_text(nl_de, out, out_len, out_used);
    if (str_eq_local(filename, "ES-DE-vocab.txt")) return copy_text(es_de, out, out_len, out_used);
    return false;
}

static void set_loaded_name(const char* filename)
{
    int i = 0;
    while (filename && filename[i] && i < VOCAB_FILENAME_MAX - 1) {
        s_loaded_name[i] = filename[i];
        i++;
    }
    s_loaded_name[i] = 0;
}

#ifdef __DEVKITARM__
static void scan_sd_root()
{
    DIR dir;
    FILINFO info;
    if (f_opendir(&dir, "/") != FR_OK) return;
    while (s_name_count < VOCAB_MAX_BROWSER_FILES) {
        if (f_readdir(&dir, &info) != FR_OK) break;
        if (!info.fname[0]) break;
        if ((info.fattrib & AM_DIR) == 0 && has_txt_ext(info.fname)) {
            add_name(info.fname);
        }
    }
    f_closedir(&dir);
}
#endif

bool vocab_file_init()
{
    s_name_count = 0;
    s_loaded_from_sd = false;
    s_loaded_name[0] = 0;
#ifdef __DEVKITARM__
    // SuperFW initializes the SuperCard hardware before mounting FatFS:
    // faster WAITCNT, map SDRAM, enable SD interface, then run sdcard_init.
    // Without this the hardware probe stalls for a few seconds and mount
    // falls back to the ROM sample list.
    REG_WAITCNT = 0x40c0;
    set_supercard_mode(MAPPED_SDRAM, true, true);
    t_card_info sd_info;
    unsigned sd_ret = sdcard_init(&sd_info);
    s_sd_ready = (sd_ret == 0) && (f_mount(&s_fatfs, "0:", 1) == FR_OK);
    if (s_sd_ready) {
        scan_sd_root();
    }
#else
    s_sd_ready = false;
#endif
    if (s_name_count == 0) {
        add_builtin_names();
    }
    return s_sd_ready;
}

bool vocab_file_sd_ready() { return s_sd_ready; }
bool vocab_file_loaded_from_sd() { return s_loaded_from_sd; }

int vocab_file_count()
{
    ensure_names_for_tests_or_fallback();
    return s_name_count;
}

const char* vocab_file_name(int index)
{
    ensure_names_for_tests_or_fallback();
    if (index < 0 || index >= s_name_count) return nullptr;
    return s_names[index];
}

static bool line_is_blank(const char* line, int line_len)
{
    for (int i = 0; i < line_len; i++) {
        char ch = line[i];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') return false;
    }
    return true;
}

template<typename Source>
static int scan_sequential_source(Source& source, VocabFile& vf)
{
    vf.reset();
    int loaded = 0;
    int current_field = 1;
    bool had_valid_in_current_group = false;
    bool pending_group_advance = false;
    char line[VOCAB_RAW_LINE_MAX];

    while (loaded < VOCAB_MAX_LINES) {
        uint32_t line_offset = 0;
        int line_len = 0;
        bool got_any = false;
        bool ended_with_newline = false;
        bool overflow = false;
        char ch = 0;
        uint32_t absolute_offset = 0;

        while (source.next(ch, absolute_offset)) {
            if (!got_any) {
                got_any = true;
                line_offset = absolute_offset;
            }
            if (ch == '\n') {
                ended_with_newline = true;
                break;
            }
            if (line_len < VOCAB_RAW_LINE_MAX - 1) {
                line[line_len++] = ch;
            } else {
                overflow = true;
            }
        }
        if (!got_any) break;

        // A newline-terminated row needs one byte of headroom for the newline
        // in the bounded one-row read path. A final 191-byte row without a
        // newline remains representable and valid.
        if (ended_with_newline && line_len == VOCAB_RAW_LINE_MAX - 1) {
            overflow = true;
        }
        line[line_len] = 0;
        if (overflow) continue;

        if (line_is_blank(line, line_len)) {
            if (had_valid_in_current_group) pending_group_advance = true;
            continue;
        }

        if (vocab_validate_raw_row(line, line_len)) {
            if (pending_group_advance) {
                if (current_field < 5) current_field++;
                pending_group_advance = false;
                had_valid_in_current_group = false;
            }
            vf.line_offsets[loaded] = line_offset;
            vf.field[loaded] = (uint8_t)current_field;
            vf.field_counts[current_field - 1]++;
            had_valid_in_current_group = true;
            ++loaded;
        }
    }

    vf.line_count = loaded;
    vf.loaded = loaded > 0;
    return loaded;
}

#ifndef __DEVKITARM__
class MemorySequentialSource {
public:
    MemorySequentialSource(const char* data, int data_len, int chunk_size) :
        data_(data), data_len_(data_len), chunk_size_(chunk_size),
        source_pos_(0), buffer_pos_(0), buffer_len_(0), read_calls_(0)
    {
        if (chunk_size_ < 1) chunk_size_ = 1;
        if (chunk_size_ > 1024) chunk_size_ = 1024;
    }

    bool next(char& out, uint32_t& absolute_offset)
    {
        if (buffer_pos_ >= buffer_len_) {
            if (source_pos_ >= data_len_) return false;
            int remaining = data_len_ - source_pos_;
            buffer_len_ = remaining < chunk_size_ ? remaining : chunk_size_;
            std::memcpy(buffer_, data_ + source_pos_, (size_t)buffer_len_);
            buffer_start_ = source_pos_;
            source_pos_ += buffer_len_;
            buffer_pos_ = 0;
            ++read_calls_;
        }
        absolute_offset = (uint32_t)(buffer_start_ + buffer_pos_);
        out = buffer_[buffer_pos_++];
        return true;
    }

    int read_calls() const { return read_calls_; }

private:
    const char* data_;
    int data_len_;
    int chunk_size_;
    int source_pos_;
    int buffer_start_ = 0;
    int buffer_pos_;
    int buffer_len_;
    int read_calls_;
    alignas(4) char buffer_[1024];
};

int vocab_file_scan_buffered_for_tests(const char* data, int data_len, int chunk_size,
                                       VocabFile& vf, int& bulk_read_calls)
{
    if (!data || data_len < 0) {
        vf.reset();
        bulk_read_calls = 0;
        return 0;
    }
    MemorySequentialSource source(data, data_len, chunk_size);
    int loaded = scan_sequential_source(source, vf);
    bulk_read_calls = source.read_calls();
    return loaded;
}
#endif

#ifdef __DEVKITARM__
class FatFsSequentialSource {
public:
    explicit FatFsSequentialSource(FIL& fp) :
        fp_(fp), buffer_start_(0), buffer_pos_(0), buffer_len_(0), failed_(false)
    {
    }

    bool next(char& out, uint32_t& absolute_offset)
    {
        if (buffer_pos_ >= buffer_len_) {
            buffer_start_ = (uint32_t)f_tell(&fp_);
            UINT bytes_read = 0;
            FRESULT result = tracked_read(&fp_, s_sequential_read_buffer,
                                          sizeof(s_sequential_read_buffer), &bytes_read);
            if (result != FR_OK) {
                failed_ = true;
                return false;
            }
            buffer_pos_ = 0;
            buffer_len_ = (int)bytes_read;
            if (buffer_len_ == 0) return false;
        }
        absolute_offset = buffer_start_ + (uint32_t)buffer_pos_;
        out = s_sequential_read_buffer[buffer_pos_++];
        return true;
    }

    bool failed() const { return failed_; }

private:
    FIL& fp_;
    uint32_t buffer_start_;
    int buffer_pos_;
    int buffer_len_;
    bool failed_;
};

static bool scan_sd_index(const char* filename, VocabFile& vf)
{
    FIL fp;
    if (tracked_open(&fp, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;

    FatFsSequentialSource source(fp);
    ++s_io_stats.index_scans;
    int loaded = scan_sequential_source(source, vf);
    bool close_ok = tracked_close(&fp) == FR_OK;
    if (source.failed() || !close_ok || loaded <= 0) {
        vf.reset();
        return false;
    }
    return true;
}

static bool open_sd_streaming(const char* filename, VocabFile& vf)
{
    if (!scan_sd_index(filename, vf)) return false;
    set_loaded_name(filename);
    s_loaded_from_sd = true;
    return true;
}

static bool path_exists(const char* path)
{
    FILINFO info;
    return f_stat(path, &info) == FR_OK;
}

static bool recover_sd_sidecars(const char* filename)
{
    char tmp[VOCAB_FILENAME_MAX];
    char bak[VOCAB_FILENAME_MAX];
    if (!make_sidecar_name(filename, ".tmp", tmp) ||
        !make_sidecar_name(filename, ".bak", bak)) return false;

    bool original_exists = path_exists(filename);
    bool backup_exists = path_exists(bak);
    bool temp_exists = path_exists(tmp);
    VocabFile& probe = s_reindex_scratch;

    if (!original_exists && backup_exists) {
        if (!scan_sd_index(bak, probe)) return false;
        if (tracked_rename(bak, filename) != FR_OK) return false;
        original_exists = true;
        backup_exists = false;
    }

    if (original_exists && backup_exists) {
        if (scan_sd_index(filename, probe)) {
            if (tracked_unlink(bak) != FR_OK) return false;
            backup_exists = false;
        } else {
            if (!scan_sd_index(bak, probe) || temp_exists) return false;
            if (tracked_rename(filename, tmp) != FR_OK) return false;
            if (tracked_rename(bak, filename) != FR_OK) {
                tracked_rename(tmp, filename);
                return false;
            }
            temp_exists = true;
            backup_exists = false;
        }
    }

    // A temporary is stale only after a structurally valid original is proven.
    if (original_exists && !backup_exists && temp_exists) {
        if (!scan_sd_index(filename, probe)) return false;
        if (tracked_unlink(tmp) != FR_OK) return false;
    }
    return original_exists;
}
#endif

bool vocab_file_load(const char* filename, VocabFile& vf,
                     char* fallback_buf, int fallback_len, int& fallback_used)
{
    // Every load attempt, including a same-name reload, starts a new source
    // identity so no parsed card can leak across file generations.
    begin_loaded_file_generation();
#ifdef __DEVKITARM__
    if (s_sd_ready && filename && has_txt_ext(filename)) {
        if (recover_sd_sidecars(filename) && open_sd_streaming(filename, vf)) {
            fallback_used = 0;
            return true;
        }
    }
#endif
    s_loaded_from_sd = false;
    if (!vocab_file_read_builtin_or_stub(filename, fallback_buf, fallback_len, fallback_used)) {
        return false;
    }
    set_loaded_name(filename);
    ++s_io_stats.index_scans;
    return vocab_open(vf, fallback_buf, fallback_used) > 0;
}

#ifdef __DEVKITARM__
static bool read_bounded_raw_line(FIL& fp, uint32_t offset,
                                  char* line, int line_cap, int& line_len)
{
    if (!line || line_cap < 2) return false;
    if (tracked_seek(&fp, (FSIZE_t)offset) != FR_OK) return false;

    UINT bytes_read = 0;
    // Read one byte beyond the maximum accepted content length so a delimiter
    // at index VOCAB_RAW_LINE_MAX - 1 is observable. The buffer itself is
    // exactly VOCAB_RAW_LINE_MAX bytes; the delimiter is replaced by NUL.
    UINT request = (UINT)line_cap;
    if (tracked_read(&fp, line, request, &bytes_read) != FR_OK) return false;

    for (UINT i = 0; i < bytes_read; ++i) {
        if (line[i] == '\r' || line[i] == '\n') {
            line_len = (int)i;
            line[line_len] = 0;
            return line_len > 0;
        }
    }

    // Without CR/LF, only a short read at EOF is a valid final row. Filling
    // the entire buffer proves that content exceeds the 191-byte limit and
    // leaves no room for a terminator.
    if (bytes_read >= (UINT)line_cap || !f_eof(&fp)) return false;
    line_len = (int)bytes_read;
    line[line_len] = 0;
    return line_len > 0;
}
#endif

bool vocab_file_show(const VocabFile& vf, const char* fallback_buf, int fallback_used,
                     int line_idx, LineBuf& out)
{
    if (line_idx < 0 || line_idx >= vf.line_count) {
        invalidate_card_cache();
        return false;
    }

    uint32_t source_offset = vf.line_offsets[line_idx];
    if (s_card_cache_valid &&
        s_card_cache_loaded_generation == s_loaded_generation &&
        s_card_cache_array_generation == vf.array_generation &&
        s_card_cache_offset == source_offset) {
        out = s_card_cache_line;
        out.field = vf.field[line_idx];
        return true;
    }

    ++s_card_cache_misses;
    ++s_io_stats.full_display_parses;
    LineBuf parsed;
    bool ok = false;
#ifdef __DEVKITARM__
    if (s_loaded_from_sd) {
        FIL fp;
        if (tracked_open(&fp, s_loaded_name, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
            invalidate_card_cache();
            return false;
        }
        char line[VOCAB_RAW_LINE_MAX];
        int line_len = 0;
        if (read_bounded_raw_line(fp, source_offset, line, sizeof(line), line_len)) {
            ok = parse_line_into(line, line_len, parsed);
        }
        tracked_close(&fp);
    } else
#endif
    {
        // Host/fallback source access is counted as one bounded adapter read so
        // cache tests can mechanically assert that unchanged frames add none.
        int start = (int)source_offset;
        int end = start;
        int limit = start + VOCAB_RAW_LINE_MAX - 1;
        if (limit > fallback_used) limit = fallback_used;
        while (end < limit && fallback_buf[end] != '\r' && fallback_buf[end] != '\n') ++end;
        ++s_io_stats.read_calls;
        s_io_stats.bytes_read += (uint32_t)(end - start);
        ok = vocab_show(const_cast<VocabFile&>(vf), fallback_buf, fallback_used,
                        line_idx, parsed);
    }

    if (!ok) {
        invalidate_card_cache();
        return false;
    }

    parsed.field = vf.field[line_idx];
    s_card_cache_line = parsed;
    s_card_cache_loaded_generation = s_loaded_generation;
    s_card_cache_array_generation = vf.array_generation;
    s_card_cache_offset = source_offset;
    s_card_cache_valid = true;
    out = parsed;
    return true;
}

#ifdef __DEVKITARM__
class FatFsBufferedWriter {
public:
    explicit FatFsBufferedWriter(FIL& fp) : fp_(fp), used_(0), ok_(true) {}

    bool append(const char* data, int len)
    {
        while (len > 0 && ok_) {
            int room = (int)sizeof(s_save_write_buffer) - used_;
            if (room == 0 && !flush()) return false;
            room = (int)sizeof(s_save_write_buffer) - used_;
            int take = len < room ? len : room;
            std::memcpy(s_save_write_buffer + used_, data, (size_t)take);
            used_ += take;
            data += take;
            len -= take;
        }
        return ok_;
    }

    bool flush()
    {
        if (!ok_ || used_ == 0) return ok_;
        UINT written = 0;
        ok_ = tracked_write(&fp_, s_save_write_buffer, (UINT)used_, &written) == FR_OK &&
              written == (UINT)used_;
        used_ = 0;
        return ok_;
    }

private:
    FIL& fp_;
    int used_;
    bool ok_;
};

static bool write_sd_grouped_temp(const VocabFile& vf, const char* tmp_name)
{
    FIL in;
    if (tracked_open(&in, s_loaded_name, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;

    FIL out;
    if (tracked_open(&out, tmp_name, FA_WRITE | FA_CREATE_NEW) != FR_OK) {
        tracked_close(&in);
        return false;
    }

    char line[VOCAB_RAW_LINE_MAX];
    FatFsBufferedWriter writer(out);
    bool ok = true;
    for (int field = 1; field <= 5 && ok; ++field) {
        for (int i = 0; i < vf.line_count && ok; ++i) {
            if (vf.field[i] != field) continue;
            int line_len = 0;
            if (!read_bounded_raw_line(in, vf.line_offsets[i], line,
                                       VOCAB_RAW_LINE_MAX, line_len) ||
                !writer.append(line, line_len) || !writer.append("\r\n", 2)) {
                ok = false;
            }
        }
        if (field < 5 && ok && !writer.append("\r\n", 2)) ok = false;
    }
    if (ok && !writer.flush()) ok = false;
    if (ok && tracked_sync(&out) != FR_OK) ok = false;
    if (tracked_close(&in) != FR_OK) ok = false;
    if (tracked_close(&out) != FR_OK) ok = false;
    return ok;
}

class FatFsReplacementOps {
public:
    FatFsReplacementOps(const char* original, const char* temporary, const char* backup) :
        original_(original), temporary_(temporary), backup_(backup) {}

    bool rename_original_to_backup() {
        return tracked_rename(original_, backup_) == FR_OK;
    }
    bool rename_temporary_to_original() {
        return tracked_rename(temporary_, original_) == FR_OK;
    }
    bool reindex_replacement() {
        return scan_sd_index(original_, s_reindex_scratch);
    }
    bool park_failed_replacement() {
        return tracked_rename(original_, temporary_) == FR_OK;
    }
    bool restore_backup() {
        return tracked_rename(backup_, original_) == FR_OK;
    }
    bool remove_backup() {
        return tracked_unlink(backup_) == FR_OK;
    }

private:
    const char* original_;
    const char* temporary_;
    const char* backup_;
};

static bool save_sd_grouped(VocabFile& vf)
{
    char tmp_name[VOCAB_FILENAME_MAX];
    char bak_name[VOCAB_FILENAME_MAX];
    if (!make_sidecar_name(s_loaded_name, ".tmp", tmp_name) ||
        !make_sidecar_name(s_loaded_name, ".bak", bak_name)) return false;

    if (!recover_sd_sidecars(s_loaded_name)) return false;

    // Recovery removes sidecars only after proving a structurally valid original.
    if (path_exists(tmp_name) || path_exists(bak_name)) return false;

    if (!write_sd_grouped_temp(vf, tmp_name)) {
        // The original is still present, so this known-incomplete temp is safe
        // to remove. Failure to remove it is conservative and blocks retry.
        tracked_unlink(tmp_name);
        return false;
    }

    FatFsReplacementOps replacement(s_loaded_name, tmp_name, bak_name);
    if (!run_replacement_transaction(replacement)) {
        invalidate_card_cache();
        return false;
    }

    vf = s_reindex_scratch;
    s_loaded_from_sd = true;
    vocab_clear_dirty(vf);
    vf.array_generation = 0;
    begin_loaded_file_generation();
    return true;
}
#endif

bool vocab_file_save_grouped(VocabFile& vf, const char* fallback_buf, int fallback_used,
                             char* out_buf, int out_len, int& out_used)
{
    // Dirty bits cover field movement; array_generation covers reorder/shuffle.
    // A clean unchanged file is an immediate success with no I/O or reindex.
    if (!vocab_any_dirty(vf) && vf.array_generation == 0) {
        out_used = 0;
        return true;
    }
#ifdef __DEVKITARM__
    if (s_loaded_from_sd) {
        out_used = 0;
        return save_sd_grouped(vf);
    }
#endif
    int written = vocab_export_grouped(vf, fallback_buf, fallback_used, out_buf, out_len);
    if (written < 0) return false;
    out_used = written;
    ++s_io_stats.write_calls;
    s_io_stats.bytes_written += (uint32_t)written;
    vocab_clear_dirty(vf);
    vf.array_generation = 0;
    begin_loaded_file_generation();
    return true;
}

bool vocab_file_export_grouped_stub(const VocabFile& vf, const char* source, int source_len,
                                    char* out, int out_len, int& out_used)
{
    int written = vocab_export_grouped(vf, source, source_len, out, out_len);
    if (written < 0) return false;
    out_used = written;
    return true;
}
