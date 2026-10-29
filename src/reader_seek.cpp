#include "reader_seek.h"

#ifdef __DEVKITARM__
namespace reader {

void SeekMap::build(FIL& file, unsigned char* scratch, size_t scratch_bytes)
{
    _mode = Mode::PLAIN;
    _points = 0;
    file.cltbl = nullptr;
    const FSIZE_t size = f_size(&file);
    FATFS* fs = file.obj.fs;
    if(!fs || !size) return;
    if(scan(file, scratch, scratch_bytes)) return;
    _mode = Mode::PLAIN;
    _points = 0;
    file.cltbl = nullptr;
    // The pieces, if they fit: FatFs fast seek.
    _table[0] = SIZE;
    file.cltbl = _table;
    if(f_lseek(&file, CREATE_LINKMAP) == FR_OK) {
        _mode = Mode::LINKED;
        return;
    }
    file.cltbl = nullptr;
#if FF_FS_EXFAT
    if(file.obj.stat == 2) return;   // exFAT in one piece: no chain to follow
#endif
    const FSIZE_t cluster = FSIZE_t(fs->csize) * FF_MIN_SS;
    const FSIZE_t clusters = (size + cluster - 1) / cluster;
    // The cluster before every step-th byte, found going forward once (one
    // walk of the chain; FatFs keeps the cluster of the byte before fptr).
    _step = ((clusters + SIZE - 1) / SIZE) * cluster;
    int n = 0;
    for(FSIZE_t at = _step; at < size && n < SIZE; at += _step) {
        if(f_lseek(&file, at) != FR_OK || file.fptr != at) break;
        _table[n++] = file.clust;
    }
    f_lseek(&file, 0);
    if(n) {
        _points = n;
        _mode = Mode::POINTS;
    }
}

FRESULT SeekMap::seek(FIL& file, FSIZE_t offset) const
{
    if(_mode == Mode::POINTS && offset >= _step) {
        // Point i is at (i + 1) * _step; the last one at or before the offset.
        FSIZE_t i = offset / _step - 1;
        if(i >= FSIZE_t(_points)) i = FSIZE_t(_points) - 1;
        const FSIZE_t at = (i + 1) * _step;
        // FatFs goes on from where it is when that is between the point and the offset.
        if(!(file.fptr >= at && file.fptr <= offset)) {
            file.fptr = at;
            file.clust = _table[i];
        }
    }
    return f_lseek(&file, offset);
}

}
#endif
