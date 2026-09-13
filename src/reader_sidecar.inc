// Included in reader_file.cpp inside namespace reader. Uses its bounded codecs.
// Two sector-aligned state banks; immutable normalized cache starts at 2048.
#ifdef __DEVKITARM__
namespace {
constexpr uint32_t SAV_SLOT_BYTES = 1024;
constexpr uint32_t SAV_CACHE_START = 2048;
constexpr uint32_t SAV_MAX_BYTES = EPUB_MAX_ARCHIVE_BYTES;
bool valid_side_state(const TxtSaveFooter& state) {
    if(state.settings.line_spacing<1 || state.settings.line_spacing>4 ||
       state.settings.top_margin<1 || state.settings.top_margin>4 ||
       state.settings.bottom_margin<1 || state.settings.bottom_margin>4 ||
       state.history.count<0 || state.history.count>PAGE_HISTORY_MAX ||
       state.history.head<0 || state.history.head>=PAGE_HISTORY_MAX) return false;
    uint32_t previous=0;
    for(int i=0;i<state.history.count;++i) {
        const uint32_t at=state.history.offsets[(state.history.head+i)%PAGE_HISTORY_MAX];
        if(at>=state.byte_offset || (i && at<=previous))return false;
        previous=at;
    }
    return true;
}
bool valid_side_header(const unsigned char* bytes,uint32_t physical,uint32_t logical,uint32_t identity) {
    return !std::memcmp(bytes,"GBARSAV1",8) && get32(bytes+8)==1 && get32(bytes+12)!=0 &&
           get32(bytes+16)==physical && get32(bytes+20)==identity && get32(bytes+24)==logical &&
           get32(bytes+28)==1 && get32(bytes+32)<=SAV_MAX_BYTES-SAV_CACHE_START &&
           get32(bytes+40)==crc32_bytes(bytes,40);
}
bool side_read(FIL& f, uint32_t at, unsigned char* bytes, uint32_t count) {
    UINT got = 0;
    return at <= f_size(&f) && count <= f_size(&f) - at &&
           f_lseek(&f, at) == FR_OK && f_tell(&f) == at &&
           f_read(&f, bytes, count, &got) == FR_OK && got == count;
}
bool side_write(FIL& f, uint32_t at, const unsigned char* bytes, uint32_t count) {
    UINT written = 0;
    return at <= SAV_MAX_BYTES && count <= SAV_MAX_BYTES - at &&
           f_lseek(&f, at) == FR_OK && f_tell(&f) == at &&
           f_write(&f, bytes, count, &written) == FR_OK && written == count;
}
}
#include "reader_sidecar_cache.inc"
#endif

bool ReaderFile::companion_cache(uint32_t& bytes,uint32_t& crc) const {
    if(!ensure_source() || !_side_open || !_side_valid || !_side_cache_bytes)return false;
    bytes=_side_cache_bytes;crc=_side_cache_crc;return true;
}
bool ReaderFile::companion_read(uint32_t at,unsigned char* out,uint32_t count) const {
#ifdef __DEVKITARM__
    return out && ensure_source() && _side_open && _side_valid && at<=_side_cache_bytes &&
           count<=_side_cache_bytes-at && side_read(_side_file,SAV_CACHE_START+at,out,count);
#else
    (void)at;(void)out;(void)count;return false;
#endif
}

bool ReaderFile::side_name(char* output) const {
    const auto n = std::strlen(_name);
    // FAT LFN components cannot exceed 255 UTF-16 units. A conservative UTF-8
    // byte cap preserves the full source filename instead of truncating it.
    if(!n || std::strlen(library_basename(_name)) + 4 >= LIBRARY_NAME_MAX) return false;
    std::memcpy(output, _name, n);
    std::memcpy(output + n, ".sav", 5);
    return true;
}

bool ReaderFile::ensure_source() const {
#ifdef __DEVKITARM__
    if(!_open || _source_changed)return false;
    if(_source_checking)return _source_owned && !_file.err;
    if(_source_owned && !_file.err && !_source_verify)return true;
    _cache_size=0;
    _source_verify=true;
    // Close failure leaves ownership intact. Never open over a still-owned FIL.
    if(_source_owned) {
        if(f_close(&_file)!=FR_OK)return false;
        _source_owned=false;
    }
    if(f_open(&_file,_name,FA_READ|FA_OPEN_EXISTING)!=FR_OK)return false;
    _source_owned=true;
    if(f_size(&_file)!=_physical_size) {_source_changed=true;return false;}
    // Identity uses physical reads (and the original EPUB central directory).
    // Permit only those nested reads, not recursive recovery on another fault.
    _source_checking=true;
    uint32_t identity=0;
    const bool checked=source_identity(identity);
    _source_checking=false;
    _cache_size=0;
    if(!checked)return false;
    if(identity!=_source_identity) {_source_changed=true;return false;}
    _source_verify=false;
    return true;
#else
    return false;
#endif
}

bool ReaderFile::source_identity(uint32_t& result) const {
#ifdef __DEVKITARM__
    // Bounded physical samples plus size/type and EPUB's existing central CRC.
    // Not a cryptographic full-file hash; documented unsampled-edit limitation.
    unsigned char bytes[512];
    put32(bytes, _physical_size); put32(bytes+4, _size);
    put32(bytes+8, txt_book_name(_name) ? 1 : 2);
    uint32_t crc = crc32_update(0xffffffffu, bytes, 12);
    const uint32_t take = _physical_size < sizeof(bytes) ? _physical_size : sizeof(bytes);
    const uint32_t positions[] = {0, (_physical_size - take)/2, _physical_size - take};
    for(uint32_t at : positions) {
        if(!side_read(_file, at, bytes, take)) return false;
        crc = crc32_update(crc, bytes, take);
    }
    if(!txt_book_name(_name)) {
        ZipLayout layout{}; uint32_t fingerprint;
        if(!zip_layout(*this,layout) || !central_fingerprint(*this,layout,fingerprint)) return false;
        put32(bytes,fingerprint); crc=crc32_update(crc,bytes,4);
    }
    result=~crc; return true;
#else
    (void)result; return false;
#endif
}

bool ReaderFile::read_side_slot(uint32_t slot) const {
#ifdef __DEVKITARM__
    _cache_size = 0; // The source read window is exclusively borrowed here.
    if(!_side_open || slot > 1 || !side_read(_side_file,slot*SAV_SLOT_BYTES,_cache,SAV_SLOT_BYTES)) return false;
    return valid_side_header(_cache,_physical_size,_size,_source_identity) &&
           get32(_cache+1020)==crc32_bytes(_cache,1020);
#else
    (void)slot; return false;
#endif
}

bool ReaderFile::load_sidecar() {
#ifdef __DEVKITARM__
    _side_valid=false; _side_generation=0; _side_cache_bytes=0; _side_cache_crc=0;
    char name[LIBRARY_PATH_MAX+4];
    if(!side_name(name)) { _side_present=true; return true; }
    if(!_side_open) {
        const auto opened=f_open(&_side_file,name,FA_READ|FA_OPEN_EXISTING);
        _side_present=opened!=FR_NO_FILE;
        if(opened!=FR_OK) return true; // Reading remains possible; no stale fallback on I/O errors.
        _side_open=true;
    }
    _side_present=true;
    // Existing empty files carry no ownership proof. Only our successful
    // CREATE_NEW (retained for live retry), or a matching header, permits writes.
    for(uint32_t slot=0;slot<2;++slot) if(read_side_slot(slot)) {
        const uint32_t generation=get32(_cache+12);
        if(!_side_valid || generation>_side_generation) {
            _side_valid=true; _side_generation=generation; _side_slot=slot;
            _side_cache_bytes=get32(_cache+32); _side_cache_crc=get32(_cache+36);
        }
    }
    if(!_side_valid) {
        for(uint32_t slot=0;slot<2;++slot)
            if(side_read(_side_file,slot*SAV_SLOT_BYTES,_cache,64) &&
               valid_side_header(_cache,_physical_size,_size,_source_identity))_side_created=true;
    }
    return true;
#else
    return false;
#endif
}

bool ReaderFile::recover_side_write(uint32_t original_size) {
#ifdef __DEVKITARM__
    if(!_side_open || original_size>=SAV_MAX_BYTES)return false;
    if(f_close(&_side_file)!=FR_OK)return false;
    _side_open=false;
    char name[LIBRARY_PATH_MAX+4];if(!side_name(name))return false;
    if(f_open(&_side_file,name,FA_READ|FA_WRITE|FA_OPEN_EXISTING)!=FR_OK)return false;
    _side_open=true;
    // FatFS can allocate a cluster before a failed data write updates objsize.
    // Its truncate() is a no-op at EOF. Extend the *owned companion* by one
    // logical byte first so truncation also retires such an attached tail chain.
    // No rename/unlink, no aliases, and never any source-book mutation.
    return f_lseek(&_side_file,original_size+1)==FR_OK && f_tell(&_side_file)==original_size+1 &&
           f_lseek(&_side_file,original_size)==FR_OK && f_tell(&_side_file)==original_size &&
           f_truncate(&_side_file)==FR_OK && f_sync(&_side_file)==FR_OK;
#else
    (void)original_size;return false;
#endif
}

bool ReaderFile::write_side_cache(const ByteSource& text,uint32_t fingerprint) {
#ifdef __DEVKITARM__
    const uint32_t original_size=f_size(&_side_file);
    uint32_t bytes=0,expected=0;
    const bool written=export_side_cache(_side_file,text,fingerprint,_write_cache,bytes,expected);
    if(!written)(void)recover_side_write(original_size);
    if(!_side_open)return false;
    if(f_close(&_side_file)!=FR_OK)return false;
    _side_open=false;
    char name[LIBRARY_PATH_MAX+4];if(!side_name(name))return false;
    if(f_open(&_side_file,name,FA_READ|FA_OPEN_EXISTING)!=FR_OK)return false;
    _side_open=true;
    if(!written)return false;
    // One full sequential readback only on cache creation/rebuild; normal cached
    // opens retain lazy table/block validation, and state saves never read cache.
    uint32_t crc=0xffffffffu;
    _cache_size=0;
    for(uint32_t pos=0;pos<bytes;) {
        const uint32_t take=bytes-pos>sizeof(_cache)?sizeof(_cache):bytes-pos;
        if(!side_read(_side_file,SAV_CACHE_START+pos,_cache,take))return false;
        crc=crc32_update(crc,_cache,take);pos+=take;
    }
    if(~crc!=expected)return false;
    if(f_close(&_side_file)!=FR_OK)return false;
    _side_open=false;
    if(f_open(&_side_file,name,FA_READ|FA_WRITE|FA_OPEN_EXISTING)!=FR_OK)return false;
    _side_open=true;_side_cache_bytes=bytes;_side_cache_crc=expected;
    return true;
#else
    (void)text;(void)fingerprint;return false;
#endif
}

bool ReaderFile::save_footer(const TxtSaveFooter& footer, const ByteSource* optimized) {
#ifdef __DEVKITARM__
    if(!valid_side_state(footer) || !ensure_source())return false;
    // A failed installed-file reopen must not permanently block a live retry.
    // Reacquire a possibly latched handle before probing the bounded state banks.
    if(_side_present && !_side_valid) {
        if(_side_open) { if(f_close(&_side_file)!=FR_OK)return false; _side_open=false; }
        load_sidecar();
    }
    ZipLayout layout{};
    const bool make_cache=optimized && !txt_book_name(_name) && optimized->size() &&
        optimized->cache_archive_layout(layout.central,layout.size,layout.count);
    // Publish a recoverable state bank before the first large cache export.
    if(make_cache && !_side_valid && !save_footer(footer,nullptr))return false;
    uint32_t fingerprint=0;
    if(make_cache && !central_fingerprint(*this,layout,fingerprint))return false;
    if(!_open || !supported_book_name(_name) || (_side_present&&!_side_valid&&!_side_created) ||
       _side_generation==0xffffffffu) return false;
    char name[LIBRARY_PATH_MAX+4]; uint32_t identity;
    _cache_size=0;
    if(!side_name(name) || !source_identity(identity)) return false;
    if(identity!=_source_identity) {_source_changed=true;_cache_size=0;return false;}
    if(_side_open) {
        if(f_close(&_side_file)!=FR_OK) return false; // quarantine, never reuse a live failed-close FIL
        _side_open=false;
    }
    const BYTE mode=FA_READ|FA_WRITE|(_side_present ? FA_OPEN_EXISTING : FA_CREATE_NEW);
    if(f_open(&_side_file,name,mode)!=FR_OK) return false;
    _side_open=true;
    if(!_side_present) _side_created=true;
    _side_present=true;
    if(make_cache && !write_side_cache(*optimized,fingerprint))return false;
    const uint32_t generation=_side_generation+1;
    const uint32_t slot=_side_valid ? 1-_side_slot : 0;
    _cache_size=0;
    std::memset(_cache,0,SAV_SLOT_BYTES);
    std::memcpy(_cache,"GBARSAV1",8); put32(_cache+8,1);put32(_cache+12,generation);
    put32(_cache+16,_physical_size);put32(_cache+20,_source_identity);put32(_cache+24,_size);
    put32(_cache+28,1);put32(_cache+32,_side_cache_bytes);put32(_cache+36,_side_cache_crc);
    put32(_cache+40,crc32_bytes(_cache,40));
    make_txt_save_footer(footer,_cache+64);
    const uint32_t expected=crc32_bytes(_cache,1020);put32(_cache+1020,expected);
    const uint32_t original_size=f_size(&_side_file);
    const bool written=side_write(_side_file,slot*SAV_SLOT_BYTES,_cache,SAV_SLOT_BYTES) && f_sync(&_side_file)==FR_OK;
    if(!written)(void)recover_side_write(original_size);
    if(!_side_open)return false;
    const bool closed=f_close(&_side_file)==FR_OK;
    if(!closed) return false;
    _side_open=false;
    load_sidecar();
    if(!written || !_side_valid || _side_generation!=generation || _side_slot!=slot ||
       !read_side_slot(slot) || get32(_cache+1020)!=expected) return false;
    _side_created=false;
    if(make_cache)optimized->cache_persisted();
    return true;
#else
    (void)footer; (void)optimized; return false;
#endif
}
