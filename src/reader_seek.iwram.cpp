#include "reader_seek.h"

// SeekMap::scan in IWRAM (ARM code): it runs for every cluster of a book when
// it opens, about 100,000 times for a 400 MB comic on 4 KiB clusters.
#ifdef __DEVKITARM__
#include "diskio.h"
#include <cstring>

namespace reader {

bool SeekMap::scan(FIL& file, unsigned char* scratch, size_t scratch_bytes)
{
    FATFS* fs = file.obj.fs;
    const bool fat16 = fs->fs_type == FS_FAT16;
#if FF_FS_EXFAT
    // exFAT: a file in one piece has no FAT chain (its FAT entries mean nothing);
    // FatFs's own fast seek map of it needs no reads (SeekMap::build).
    const bool exfat = fs->fs_type == FS_EXFAT;
    if(exfat && file.obj.stat != 0) return false;
#else
    const bool exfat = false;
#endif
    // FAT16 / FAT32 / exFAT (32-bit entries, all bits used), and nothing of the
    // FAT waiting in FatFs's window to be written.
    if((!fat16 && !exfat && fs->fs_type != FS_FAT32) || fs->wflag || !scratch) return false;
    DWORD* points = reinterpret_cast<DWORD*>(scratch);
    unsigned char* fat = scratch + SIZE * sizeof(DWORD);
    if(reinterpret_cast<uintptr_t>(scratch) & 3 || scratch_bytes < SIZE * sizeof(DWORD) + 2 * FF_MIN_SS) return false;
    UINT batch = UINT((scratch_bytes - SIZE * sizeof(DWORD)) / FF_MIN_SS);
    if(batch > 16) batch = 16;
    const FSIZE_t size = f_size(&file);
    const FSIZE_t cluster_bytes = FSIZE_t(fs->csize) * FF_MIN_SS;
    const DWORD clusters = DWORD((size + cluster_bytes - 1) / cluster_bytes);
    const int shift = fat16 ? 8 : 7;   // FAT entries per sector: 1 << shift
    const DWORD mask = (DWORD(1) << shift) - 1;
    const DWORD entry = exfat ? 0xFFFFFFFF : 0x0FFFFFFF;   // the bits of a FAT32 / exFAT entry
    // Points (when the pieces do not fit): the cluster before every step-th cluster.
    const DWORD step = clusters > 1 ? (clusters + SIZE - 1) / SIZE : 0;
    constexpr int MAX_RUNS = (SIZE - 2) / 2;
    int runs = 0, n_points = 0;
    bool fits = true;
    DWORD first = 0, loaded = 0;   // FAT sectors in memory: first .. first + loaded - 1
    DWORD run_start = 0, run_len = 0, to_point = step;
    // Cluster i of the file is c: a new piece starts with it.
    auto start = [&](DWORD c, DWORD i) {
        if(c < 2 || c >= fs->n_fatent) return false;
        if(run_len) {
            if(runs < MAX_RUNS) { _table[1 + 2 * runs] = run_len; _table[2 + 2 * runs] = run_start; }
            if(++runs > MAX_RUNS) fits = false;
        }
        run_start = c;
        run_len = 1;
        if(step && !--to_point) {
            to_point = step;
            if(n_points < SIZE && i + 1 < clusters) points[n_points++] = c;
        }
        return true;
    };
    DWORD i = 0, c = file.obj.sclust;
    if(!start(c, 0)) return false;
    // (No division in here: it runs for every cluster of the file.)
    while(i + 1 < clusters) {
        // The FAT entry of c (its next cluster), a few FAT sectors read at once.
        const DWORD sector = c >> shift;
        if(sector - first >= loaded) {
            if(sector >= fs->fsize) return false;
            first = sector;
            loaded = fs->fsize - sector < batch ? fs->fsize - sector : batch;
            if(disk_read(fs->pdrv, fat, fs->fatbase + first, loaded) != RES_OK) return false;
        }
        const DWORD at = ((sector - first) << shift) + (c & mask);
        // Usually the next cluster is the one after: all such entries in memory at once.
        DWORD avail = (loaded << shift) - at, j = 0;
        if(avail > clusters - 1 - i) avail = clusters - 1 - i;
        if(fat16) {
            const WORD* e = reinterpret_cast<const WORD*>(fat) + at;
            while(j < avail && e[j] == c + 1 + j) ++j;
        } else {
            const DWORD* e = reinterpret_cast<const DWORD*>(fat) + at;
            while(j < avail && (e[j] & entry) == c + 1 + j) ++j;
        }
        if(j) {
            // Clusters i + 1 .. i + j are c + 1 .. c + j: the same piece.
            if(c + j >= fs->n_fatent) return false;
            run_len += j;
            if(step) {
                DWORD done = 0;
                while(to_point <= j - done) {
                    done += to_point;
                    to_point = step;
                    if(n_points < SIZE && i + done + 1 < clusters) points[n_points++] = c + done;
                }
                to_point -= j - done;
            }
            c += j;
            i += j;
            continue;
        }
        c = fat16 ? DWORD(reinterpret_cast<const WORD*>(fat)[at]) : reinterpret_cast<const DWORD*>(fat)[at] & entry;
        ++i;
        if(!start(c, i)) return false;
    }
    if(run_len) {
        if(runs < MAX_RUNS) { _table[1 + 2 * runs] = run_len; _table[2 + 2 * runs] = run_start; }
        if(++runs > MAX_RUNS) fits = false;
    }
    if(fits) {
        // FatFs fast seek's table: its size, then (length, first cluster) for each piece, then 0.
        _table[0] = SIZE;
        _table[1 + 2 * runs] = 0;
        file.cltbl = _table;
        _mode = Mode::LINKED;
        return true;
    }
    if(n_points) {
        std::memcpy(_table, points, sizeof(DWORD) * DWORD(n_points));
        _step = FSIZE_t(step) * cluster_bytes;
        _points = n_points;
        _mode = Mode::POINTS;
    }
    return true;
}

}
#endif
