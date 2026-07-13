// vocab_file_io.cpp — SD/FatFS streaming implementation.

#include "vocab_file_io.h"

#include <cstring>

#ifdef __DEVKITARM__
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

// One parsed card (~193 bytes), keyed by source generation + array generation
// + physical source offset. This keeps ordinary/feedback/animation frames off
// the SD card while remaining bounded regardless of vocabulary-file size.
static bool s_card_cache_valid = false;
static uint32_t s_card_cache_loaded_generation = 0;
static uint32_t s_card_cache_array_generation = 0;
static uint32_t s_card_cache_offset = 0;
static LineBuf s_card_cache_line;
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

#ifdef __DEVKITARM__
static FATFS s_fatfs;
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

        LineBuf parsed;
        if (parse_line_into(line, line_len, parsed)) {
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
            FRESULT result = f_read(&fp_, buffer_, sizeof(buffer_), &bytes_read);
            if (result != FR_OK) {
                failed_ = true;
                return false;
            }
            buffer_pos_ = 0;
            buffer_len_ = (int)bytes_read;
            if (buffer_len_ == 0) return false;
        }
        absolute_offset = buffer_start_ + (uint32_t)buffer_pos_;
        out = buffer_[buffer_pos_++];
        return true;
    }

    bool failed() const { return failed_; }

private:
    FIL& fp_;
    uint32_t buffer_start_;
    int buffer_pos_;
    int buffer_len_;
    bool failed_;
    alignas(4) char buffer_[512];
};

static bool open_sd_streaming(const char* filename, VocabFile& vf)
{
    FIL fp;
    if (f_open(&fp, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;

    FatFsSequentialSource source(fp);
    int loaded = scan_sequential_source(source, vf);
    bool close_ok = f_close(&fp) == FR_OK;
    if (source.failed() || !close_ok || loaded <= 0) {
        vf.reset();
        return false;
    }
    set_loaded_name(filename);
    s_loaded_from_sd = true;
    return true;
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
        if (open_sd_streaming(filename, vf)) {
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
    return vocab_open(vf, fallback_buf, fallback_used) > 0;
}

#ifdef __DEVKITARM__
static bool read_bounded_raw_line(FIL& fp, uint32_t offset,
                                  char* line, int line_cap, int& line_len)
{
    if (!line || line_cap < 2) return false;
    if (f_lseek(&fp, (FSIZE_t)offset) != FR_OK) return false;

    UINT bytes_read = 0;
    UINT request = (UINT)(line_cap - 1);
    if (f_read(&fp, line, request, &bytes_read) != FR_OK) return false;

    for (UINT i = 0; i < bytes_read; ++i) {
        if (line[i] == '\r' || line[i] == '\n') {
            line_len = (int)i;
            line[line_len] = 0;
            return line_len > 0;
        }
    }

    // A full buffer without CR/LF is only valid when it is exactly the final
    // row at EOF. Otherwise the source row was truncated/overlong.
    if (bytes_read == request && !f_eof(&fp)) return false;
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
    LineBuf parsed;
    bool ok = false;
#ifdef __DEVKITARM__
    if (s_loaded_from_sd) {
        FIL fp;
        if (f_open(&fp, s_loaded_name, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
            invalidate_card_cache();
            return false;
        }
        char line[VOCAB_RAW_LINE_MAX];
        int line_len = 0;
        if (read_bounded_raw_line(fp, source_offset, line, sizeof(line), line_len)) {
            ok = parse_line_into(line, line_len, parsed);
        }
        f_close(&fp);
    } else
#endif
    {
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
            int room = (int)sizeof(buffer_) - used_;
            if (room == 0 && !flush()) return false;
            room = (int)sizeof(buffer_) - used_;
            int take = len < room ? len : room;
            std::memcpy(buffer_ + used_, data, (size_t)take);
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
        ok_ = f_write(&fp_, buffer_, (UINT)used_, &written) == FR_OK &&
              written == (UINT)used_;
        used_ = 0;
        return ok_;
    }

private:
    FIL& fp_;
    int used_;
    bool ok_;
    alignas(4) char buffer_[512];
};

static bool save_sd_grouped(VocabFile& vf)
{
    FIL in;
    if (f_open(&in, s_loaded_name, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;

    char tmp_name[VOCAB_FILENAME_MAX + 5];
    int n = 0;
    while (s_loaded_name[n] && n < VOCAB_FILENAME_MAX - 5) {
        tmp_name[n] = s_loaded_name[n];
        n++;
    }
    tmp_name[n++] = '.';
    tmp_name[n++] = 't';
    tmp_name[n++] = 'm';
    tmp_name[n++] = 'p';
    tmp_name[n] = 0;

    FIL out;
    if (f_open(&out, tmp_name, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        f_close(&in);
        return false;
    }

    char line[VOCAB_RAW_LINE_MAX];
    FatFsBufferedWriter writer(out);
    bool ok = true;
    for (int field = 1; field <= 5 && ok; field++) {
        for (int i = 0; i < vf.line_count && ok; i++) {
            if (vf.field[i] != field) continue;
            int line_len = 0;
            if (!read_bounded_raw_line(in, vf.line_offsets[i], line,
                                       VOCAB_RAW_LINE_MAX, line_len)) {
                ok = false;
                break;
            }
            if (!writer.append(line, line_len)) ok = false;
            if (ok && !writer.append("\r\n", 2)) ok = false;
        }
        if (field < 5 && ok && !writer.append("\r\n", 2)) {
            ok = false;
        }
    }
    if (ok && !writer.flush()) ok = false;

    if (f_close(&in) != FR_OK) ok = false;
    if (f_close(&out) != FR_OK) ok = false;
    if (!ok) {
        f_unlink(tmp_name);
        return false;
    }

    f_unlink(s_loaded_name);
    if (f_rename(tmp_name, s_loaded_name) != FR_OK) {
        return false;
    }
    if (!open_sd_streaming(s_loaded_name, vf)) {
        invalidate_card_cache();
        return false;
    }
    vocab_clear_dirty(vf);
    begin_loaded_file_generation();
    return true;
}
#endif

bool vocab_file_save_grouped(VocabFile& vf, const char* fallback_buf, int fallback_used,
                             char* out_buf, int out_len, int& out_used)
{
#ifdef __DEVKITARM__
    if (s_loaded_from_sd) {
        out_used = 0;
        return save_sd_grouped(vf);
    }
#endif
    int written = vocab_export_grouped(vf, fallback_buf, fallback_used, out_buf, out_len);
    if (written < 0) return false;
    out_used = written;
    vocab_clear_dirty(vf);
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
