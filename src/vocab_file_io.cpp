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
        "hond\tHund\n"
        "kat\tKatze\n"
        "boom\tBaum\n"
        "huis\tHaus\n"
        "boek\tBuch\n"
        "water\tWasser\n"
        "brood\tBrot\n"
        "school\tSchule\n"
        "leraar\tLehrer\n"
        "meisje\tMaedchen\n"
        "jongen\tJunge\n"
        "auto\tAuto\n"
        "trein\tZug\n"
        "fiets\tFahrrad\n"
        "appel\tApfel\n"
        "peer\tBirne\n"
        "tafel\tTisch\n"
        "stoel\tStuhl\n"
        "raam\tFenster\n"
        "deur\tTuer\n";

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

#ifdef __DEVKITARM__
static bool ff_read_char(FIL& fp, char& out)
{
    UINT br = 0;
    FRESULT fr = f_read(&fp, &out, 1, &br);
    return fr == FR_OK && br == 1;
}

static bool open_sd_streaming(const char* filename, VocabFile& vf)
{
    FIL fp;
    if (f_open(&fp, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;

    vf.reset();
    int loaded = 0;
    int current_field = 1;
    bool had_valid_in_current_group = false;
    bool pending_group_advance = false;
    char line[VOCAB_RAW_LINE_MAX];

    while (loaded < VOCAB_MAX_LINES && !f_eof(&fp)) {
        FSIZE_t offset = f_tell(&fp);
        int line_len = 0;
        bool got_any = false;
        bool overflow = false;
        char ch = 0;
        while (ff_read_char(fp, ch)) {
            got_any = true;
            if (line_len < VOCAB_RAW_LINE_MAX - 1) {
                line[line_len++] = ch;
            } else {
                overflow = true;
            }
            if (ch == '\n') break;
        }
        if (!got_any) break;
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
            vf.line_offsets[loaded] = (uint32_t)offset;
            vf.field[loaded] = (uint8_t)current_field;
            vf.field_counts[current_field - 1]++;
            had_valid_in_current_group = true;
            loaded++;
        }
    }

    f_close(&fp);
    vf.line_count = loaded;
    vf.loaded = (loaded > 0);
    if (loaded <= 0) return false;
    set_loaded_name(filename);
    s_loaded_from_sd = true;
    return true;
}
#endif

bool vocab_file_load(const char* filename, VocabFile& vf,
                     char* fallback_buf, int fallback_len, int& fallback_used)
{
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

bool vocab_file_show(const VocabFile& vf, const char* fallback_buf, int fallback_used,
                     int line_idx, LineBuf& out)
{
#ifdef __DEVKITARM__
    if (s_loaded_from_sd) {
        if (line_idx < 0 || line_idx >= vf.line_count) return false;
        FIL fp;
        if (f_open(&fp, s_loaded_name, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;
        if (f_lseek(&fp, (FSIZE_t)vf.line_offsets[line_idx]) != FR_OK) {
            f_close(&fp);
            return false;
        }
        char line[VOCAB_RAW_LINE_MAX];
        int line_len = 0;
        char ch = 0;
        while (line_len < VOCAB_RAW_LINE_MAX - 1 && ff_read_char(fp, ch)) {
            line[line_len++] = ch;
            if (ch == '\n') break;
        }
        f_close(&fp);
        line[line_len] = 0;
        if (!parse_line_into(line, line_len, out)) return false;
        out.field = vf.field[line_idx];
        return true;
    }
#endif
    return vocab_show(const_cast<VocabFile&>(vf), fallback_buf, fallback_used, line_idx, out);
}

#ifdef __DEVKITARM__
static bool read_raw_line(FIL& fp, uint32_t offset, char* line, int line_cap, int& line_len)
{
    if (f_lseek(&fp, (FSIZE_t)offset) != FR_OK) return false;
    line_len = 0;
    char ch = 0;
    while (line_len < line_cap - 1 && ff_read_char(fp, ch)) {
        if (ch == '\n') break;
        if (ch != '\r') {
            line[line_len++] = ch;
        }
    }
    line[line_len] = 0;
    return line_len > 0;
}

static bool write_all(FIL& fp, const char* data, int len)
{
    UINT written = 0;
    return f_write(&fp, data, (UINT)len, &written) == FR_OK && written == (UINT)len;
}

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
    bool ok = true;
    for (int field = 1; field <= 5 && ok; field++) {
        for (int i = 0; i < vf.line_count && ok; i++) {
            if (vf.field[i] != field) continue;
            int line_len = 0;
            if (!read_raw_line(in, vf.line_offsets[i], line, VOCAB_RAW_LINE_MAX, line_len)) {
                ok = false;
                break;
            }
            if (!write_all(out, line, line_len)) ok = false;
            if (ok && !write_all(out, "\r\n", 2)) ok = false;
        }
        if (field < 5 && ok) {
            if (!write_all(out, "\r\n", 2)) ok = false;
        }
    }

    f_close(&in);
    if (f_close(&out) != FR_OK) ok = false;
    if (!ok) {
        f_unlink(tmp_name);
        return false;
    }

    f_unlink(s_loaded_name);
    if (f_rename(tmp_name, s_loaded_name) != FR_OK) {
        return false;
    }
    vocab_clear_dirty(vf);
    return open_sd_streaming(s_loaded_name, vf);
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
