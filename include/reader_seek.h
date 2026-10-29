#pragma once
// Seeking in a book file stored in many pieces on the card. FatFs finds an
// offset by following the file's cluster chain from its start (or from where
// it is, going forward): in a large file that is hundreds of FAT sectors per
// read, and the whole app slows down. Fast seek (a map of the pieces) fixes
// that while the map fits; otherwise the cluster at regular points of the
// file is noted once when it opens, and each seek starts from the nearest
// point before it.
#ifdef __DEVKITARM__
#include "ff.h"
#include <cstddef>

namespace reader {

class SeekMap {
public:
    static constexpr int SIZE = 256;   // DWORDs: fast seek up to 127 pieces, or 256 points

    // After f_open (read only). Never fails: at worst seeks stay as FatFs does them.
    // With scratch memory (at least 1.5 KiB) the FAT is read a few sectors at a
    // time and scanned here, instead of one FatFs call for every cluster.
    void build(FIL& file, unsigned char* scratch = nullptr, size_t scratch_bytes = 0);
    void clear() { _mode = Mode::PLAIN; }
    // f_lseek, starting from the nearest noted point when that saves following the chain.
    FRESULT seek(FIL& file, FSIZE_t offset) const;

    enum class Mode : unsigned char { PLAIN, LINKED, POINTS };
    Mode mode() const { return _mode; }
    int points() const { return _points; }

private:
    bool scan(FIL& file, unsigned char* scratch, size_t scratch_bytes);

    DWORD _table[SIZE];
    Mode _mode = Mode::PLAIN;
    int _points = 0;
    FSIZE_t _step = 0;      // bytes between points (whole clusters)
};

}
#endif
