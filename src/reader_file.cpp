#include "reader_file.h"
#include "reader_crc32.h"
#include "epub_document.h"

#include <cstring>
#ifndef __DEVKITARM__
#include <cstdio>
#include <vector>
#endif
#ifdef __DEVKITARM__
#include "bn_core.h"
#include "gbahw.h"
extern "C" {
#include "supercard_driver.h"
}
#endif

namespace reader {
namespace {
#ifdef __DEVKITARM__
__attribute__((section(".sbss")))
#endif
char names[LIBRARY_MAX_FILES][LIBRARY_NAME_MAX];
int name_count;
#ifdef __DEVKITARM__
FATFS fatfs;
#endif
constexpr char CACHE_NAME[] = "META-INF/gbareader/cache-v5";
constexpr char STATE_NAME[] = "META-INF/gbareader/state-v5";
// Keep the existing owned ZIP path/magic; the internal version field selects
// v5 (whole-text checks) or v6 (4096-byte blocks, trailing CRC table).
constexpr uint32_t CACHE_HEADER_SIZE = 32;

bool extension_equal(const char* a, const char* b) {
    while(*a && *b) { char x=*a++, y=*b++; if(x>='A'&&x<='Z') x=char(x+'a'-'A'); if(x!=y) return false; }
    return !*a && !*b;
}
// Avoid hosted strncpy: Butano's final ROM link intentionally provides only its
// small string shim set. This always terminates the fixed library-name buffers.
template<unsigned N> void copy_book_name(char (&destination)[N], const char* source) {
    int i = 0;
    while(source[i] && i < int(N) - 1) { destination[i] = source[i]; ++i; }
    destination[i] = 0;
}
void put16(unsigned char* p,uint16_t v){p[0]=unsigned(v);p[1]=unsigned(v>>8);}
void put32(unsigned char* p,uint32_t v){p[0]=unsigned(v);p[1]=unsigned(v>>8);p[2]=unsigned(v>>16);p[3]=unsigned(v>>24);}
uint16_t get16(const unsigned char* p){return uint16_t(p[0]|uint16_t(p[1])<<8);}
uint32_t get32(const unsigned char* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
constexpr unsigned char LEGACY_CACHE_MAGIC[]={'G','B','A','R','C','H','E','1'};
constexpr uint32_t LEGACY_CACHE_TRAILER_SIZE=32;
uint32_t legacy_hash(const unsigned char* b){uint32_t h=2166136261u;for(uint32_t i=0;i<LEGACY_CACHE_TRAILER_SIZE;++i)if(i<28||i>=32)h=(h^b[i])*16777619u;return h;}

struct ZipLayout { uint32_t central, size, eocd; uint16_t count; };
bool source_read(const ByteSource& s,uint32_t at,unsigned char* out,uint32_t n){return s.read_range(at,out,n);}
bool zip_layout(const ByteSource& s, ZipLayout& z) {
    if(s.size()<22) return false;
    const uint32_t min=s.size()>65557?s.size()-65557:0;
    for(uint32_t p=s.size()-22;;--p) { unsigned char h[22];
        if(!source_read(s,p,h,22)) return false;
        if(get32(h)==0x06054b50u && get16(h+20)==s.size()-p-22u &&
           !get16(h+4) && !get16(h+6) && get16(h+8)==get16(h+10) &&
           get16(h+10)!=0xffff && get32(h+12)!=0xffffffffu && get32(h+16)!=0xffffffffu &&
           get32(h+16)<=p && get32(h+12)==p-get32(h+16)) {
            z={get32(h+16),get32(h+12),p,get16(h+10)}; return true;
        }
        if(p==min) break;
    }
    return false;
}
bool central_record(const ByteSource& s,uint32_t at,uint32_t end,uint16_t& name_len,uint32_t& record){
    unsigned char h[46]; if(at>end||end-at<46||!source_read(s,at,h,46)||get32(h)!=0x02014b50u) return false;
    name_len=get16(h+28); const uint16_t extra=get16(h+30),comment=get16(h+32);
    record=46u+name_len+extra+comment; return name_len && record<=end-at;
}
bool name_at(const ByteSource&s,uint32_t at,uint16_t n,const char* name){
    uint32_t i=0;while(name[i])++i;if(i!=n)return false;unsigned char b[64];
    while(i){uint32_t take=i>sizeof(b)?sizeof(b):i;if(!source_read(s,at,b,take)||std::memcmp(b,name,take))return false;at+=take;name+=take;i-=take;}return true;
}
bool owned_name(const ByteSource&s,uint32_t at,uint16_t n){
    static const char prefix[]="META-INF/gbareader/"; if(n<sizeof(prefix)-1)return false;
    unsigned char p[sizeof(prefix)-1];return source_read(s,at,p,sizeof(p))&&!std::memcmp(p,prefix,sizeof(p));
}
bool central_fingerprint(const ByteSource&s,const ZipLayout& z,uint32_t& result){
    uint32_t p=z.central,c=0xffffffffu;for(uint16_t i=0;i<z.count;++i){uint16_t nl;uint32_t r;if(!central_record(s,p,z.central+z.size,nl,r))return false;if(!owned_name(s,p+46,nl)){uint32_t left=r,at=p;unsigned char block[512];while(left){uint32_t take=left>sizeof(block)?sizeof(block):left;if(!source_read(s,at,block,take))return false;c=crc32_update(c,block,take);at+=take;left-=take;}}p+=r;}if(p!=z.central+z.size)return false;result=~c;return true;
}
[[maybe_unused]] bool find_state(const ByteSource&s,const ZipLayout& z,uint32_t& data,uint32_t& size){
    bool found=false;uint32_t p=z.central;for(uint16_t i=0;i<z.count;++i){uint16_t nl;uint32_t r;if(!central_record(s,p,z.central+z.size,nl,r))return false;if(name_at(s,p+46,nl,STATE_NAME)){unsigned char h[46],local[30];if(!source_read(s,p,h,46)||get16(h+10)||get32(h+20)!=TXT_SAVE_FOOTER_SIZE||get32(h+24)!=TXT_SAVE_FOOTER_SIZE||!source_read(s,get32(h+42),local,30)||get32(local)!=0x04034b50u||get16(local+6)||get16(local+8)||get32(local+18)!=TXT_SAVE_FOOTER_SIZE||get32(local+22)!=TXT_SAVE_FOOTER_SIZE)return false;uint32_t d=get32(h+42)+30u+get16(local+26)+get16(local+28);if(d>s.size()||TXT_SAVE_FOOTER_SIZE>s.size()-d)return false;data=d;size=TXT_SAVE_FOOTER_SIZE;found=true;}p+=r;}return found;
}

template<class Ops> bool write_all(Ops& o,uint32_t at,const unsigned char* p,uint32_t n){return o.seek(at)&&o.write(p,n);}
// A TXT footer replacement can overwrite a shorter legacy footer before a write,
// truncate, or sync failure. Put the captured footer back before restoring length.
template<class Ops> bool replace_txt_footer_transaction(Ops& o,uint32_t footer_offset,
                                                         uint32_t original_size,
                                                         const unsigned char* previous,
                                                         uint32_t previous_size,
                                                         const unsigned char* replacement){
    if(write_all(o,footer_offset,replacement,TXT_SAVE_FOOTER_SIZE)&&o.truncate()&&o.sync()) return true;
    (void)(write_all(o,footer_offset,previous,previous_size)&&o.seek(original_size)&&o.truncate()&&o.sync());
    return false;
}
template<class Ops> bool copy_source(Ops&o,uint32_t dst,const ByteSource&s,uint32_t src,uint32_t n,unsigned char* scratch,uint32_t cap){while(n){uint32_t take=n>cap?cap:n;if(!source_read(s,src,scratch,take)||!write_all(o,dst,scratch,take))return false;src+=take;dst+=take;n-=take;}return true;}
template<class Ops> bool local_header(Ops&o,uint32_t at,const char* name,uint32_t bytes,uint32_t checksum){unsigned char h[30]{};uint16_t n=uint16_t(std::strlen(name));put32(h,0x04034b50u);put16(h+4,20);put32(h+14,checksum);put32(h+18,bytes);put32(h+22,bytes);put16(h+26,n);return write_all(o,at,h,30)&&write_all(o,at+30,reinterpret_cast<const unsigned char*>(name),n);}
template<class Ops> bool central_header(Ops&o,uint32_t at,const char* name,uint32_t bytes,uint32_t checksum,uint32_t local){unsigned char h[46]{};uint16_t n=uint16_t(std::strlen(name));put32(h,0x02014b50u);put16(h+4,20);put16(h+6,20);put32(h+16,checksum);put32(h+20,bytes);put32(h+24,bytes);put16(h+28,n);put32(h+42,local);return write_all(o,at,h,46)&&write_all(o,at+46,reinterpret_cast<const unsigned char*>(name),n);}
template<class Ops> bool final_eocd(Ops&o,uint32_t at,uint32_t central,uint32_t size,uint16_t count){unsigned char h[22]{};put32(h,0x06054b50u);put16(h+8,count);put16(h+10,count);put32(h+12,size);put32(h+16,central);return write_all(o,at,h,22);}

bool superseded_entry(const ByteSource& source, uint32_t at, uint16_t length, bool replace_cache, bool& skip)
{
    skip = false;
    if(length != sizeof(STATE_NAME) - 1) return true;
    unsigned char name[sizeof(STATE_NAME) - 1];
    if(!source_read(source, at, name, sizeof(name))) return false;
    static_assert(sizeof(CACHE_NAME) == sizeof(STATE_NAME));
    skip = !std::memcmp(name, STATE_NAME, sizeof(name)) ||
           (replace_cache && !std::memcmp(name, CACHE_NAME, sizeof(name)));
    return true;
}

// Select only exact superseded names; unknown application and source members
// remain byte-for-byte in the live directory. Old local records stay on disk.
template<class Ops> bool retained_directory(
        Ops& o, const ByteSource& archive, const ZipLayout& old, uint32_t destination,
        unsigned char* scratch, uint32_t cap, uint32_t& bytes, uint16_t& count, bool copy, bool replace_cache = false)
{
    bytes = 0; count = 0;
    uint32_t at = old.central;
    for(uint16_t i = 0; i < old.count; ++i) {
        uint16_t length;
        uint32_t record;
        bool skip;
        if(!central_record(archive, at, old.central + old.size, length, record) ||
           !superseded_entry(archive, at + 46, length, replace_cache, skip)) return false;
        if(!skip) {
            if(copy && !copy_source(o, destination + bytes, archive, at, record, scratch, cap))
                return false;
            bytes += record;
            ++count;
        }
        at += record;
    }
    return at == old.central + old.size;
}

template<class Ops> bool append_state(
        Ops& o, const ByteSource& archive, const ZipLayout& old,
        const unsigned char footer[TXT_SAVE_FOOTER_SIZE], unsigned char* scratch, uint32_t cap,
        uint32_t append_offset = 0)
{
    uint32_t retained;
    uint16_t count;
    if(!retained_directory(o, archive, old, 0, scratch, cap, retained, count, false) ||
       count >= 0xfffe) return false;
    const uint32_t name_size = uint32_t(std::strlen(STATE_NAME));
    if(!append_offset) append_offset = archive.size();
    const uint64_t end = uint64_t(append_offset) + 30u + name_size + TXT_SAVE_FOOTER_SIZE +
                         retained + 46u + name_size + 22u;
    if(end > EPUB_MAX_ARCHIVE_BYTES) return false;
    const uint32_t local = append_offset, data = local + 30u + name_size;
    const uint32_t central = data + TXT_SAVE_FOOTER_SIZE;
    const uint32_t checksum = crc32_bytes(footer, TXT_SAVE_FOOTER_SIZE);
    return local_header(o, local, STATE_NAME, TXT_SAVE_FOOTER_SIZE, checksum) &&
           write_all(o, data, footer, TXT_SAVE_FOOTER_SIZE) &&
           retained_directory(o, archive, old, central, scratch, cap, retained, count, true) &&
           central_header(o, central + retained, STATE_NAME, TXT_SAVE_FOOTER_SIZE, checksum, local) &&
           final_eocd(o, central + retained + 46u + name_size, central,
                      retained + 46u + name_size, uint16_t(count + 1)) && o.truncate() && o.sync();
}

template<class Ops> bool append_cache_and_state(
        Ops& o, const ByteSource& archive, const ZipLayout& old, const ByteSource& text,
        const unsigned char footer[TXT_SAVE_FOOTER_SIZE], unsigned char* scratch, uint32_t cap,
        uint32_t append_offset = 0)
{
    uint32_t retained;
    uint16_t count;
    if(!text.size() ||
       !retained_directory(o, archive, old, 0, scratch, cap, retained, count, false, true) ||
       count > 0xfffc) return false;
    uint32_t fingerprint;
    if(!central_fingerprint(archive, old, fingerprint)) return false;
    const uint32_t cache_name_size = uint32_t(std::strlen(CACHE_NAME));
    const uint32_t state_name_size = uint32_t(std::strlen(STATE_NAME));
    const uint32_t added = 46u + cache_name_size + 46u + state_name_size;
    if(!append_offset) append_offset = archive.size();
    const uint32_t table_bytes = (text.size() / 4096u + (text.size() % 4096u != 0)) * 4u;
    const uint64_t end = uint64_t(append_offset) + 30u + cache_name_size +
            CACHE_HEADER_SIZE + text.size() + table_bytes + 30u + state_name_size +
            TXT_SAVE_FOOTER_SIZE + retained + added + 22u;
    if(end > EPUB_MAX_ARCHIVE_BYTES) return false;
    const uint32_t cache_local = append_offset;
    const uint32_t cache_data = cache_local + 30u + cache_name_size;
    const uint32_t cache_bytes = CACHE_HEADER_SIZE + text.size() + table_bytes;
    const uint32_t state_local = cache_data + cache_bytes;
    const uint32_t state_data = state_local + 30u + state_name_size;
    const uint32_t central = state_data + TXT_SAVE_FOOTER_SIZE;
    const uint32_t state_crc = crc32_bytes(footer, TXT_SAVE_FOOTER_SIZE);
    // Nothing before the original EOF is written. Headers are finalized before
    // publishing the new directory/EOCD, after the single payload export.
    struct Export {
        Ops& ops;
        uint32_t at, remaining;
        unsigned char* table;
        uint32_t table_at, capacity, used = 0;
        uint32_t checksum = 0, block_crc = 0xffffffffu, block_used = 0, table_crc = 0xffffffffu;
        bool flush() {
            if(!used) return true;
            if(!write_all(ops, table_at, table, used)) return false;
            table_at += used; used = 0; return true;
        }
        static bool accept(void* context, const unsigned char* bytes, uint32_t count) {
            auto& self = *static_cast<Export*>(context);
            if(!count || count > self.remaining || !write_all(self.ops, self.at, bytes, count))
                return false;
            self.at += count;
            self.remaining -= count;
            while(count) {
                uint32_t take = 4096 - self.block_used;
                if(take > count) take = count;
                self.block_crc = crc32_update(self.block_crc, bytes, take);
                self.block_used += take; bytes += take; count -= take;
                if(self.block_used == 4096 || (!count && !self.remaining)) {
                    // Reuse finalized block CRCs rather than hash text twice.
                    self.checksum = self.block_used == 4096 ?
                            crc32_combine_4096(self.checksum, ~self.block_crc) :
                            crc32_combine(self.checksum, ~self.block_crc, self.block_used);
                    put32(self.table + self.used, ~self.block_crc);
                    self.table_crc = crc32_update(self.table_crc, self.table + self.used, 4);
                    self.used += 4; self.block_used = 0; self.block_crc = 0xffffffffu;
                    if(self.used + 4 > self.capacity && !self.flush()) return false;
                }
            }
            return true;
        }
    } output{o, cache_data + CACHE_HEADER_SIZE, text.size(), scratch,
             cache_data + CACHE_HEADER_SIZE + text.size(), cap};
    if(cap < 4 || !text.export_text(Export::accept, &output) || output.remaining || !output.flush()) return false;
    const uint32_t text_crc = output.checksum;
    unsigned char header[CACHE_HEADER_SIZE]{};
    std::memcpy(header, "GBAREPC5", 8);
    put16(header + 8, 6); put16(header + 10, 1);
    put32(header + 12, fingerprint); put32(header + 16, text.size());
    put32(header + 20, text_crc); put32(header + 24, ~output.table_crc);
    put32(header + 28, crc32_bytes(header, 28));
    uint32_t whole_crc = crc32_combine(crc32_bytes(header, sizeof(header)), text_crc, text.size());
    whole_crc = crc32_combine(whole_crc, ~output.table_crc, table_bytes);
    return local_header(o, cache_local, CACHE_NAME, cache_bytes, whole_crc) &&
           write_all(o, cache_data, header, sizeof(header)) &&
           local_header(o, state_local, STATE_NAME, TXT_SAVE_FOOTER_SIZE, state_crc) &&
           write_all(o, state_data, footer, TXT_SAVE_FOOTER_SIZE) &&
           retained_directory(o, archive, old, central, scratch, cap, retained, count, true, true) &&
           central_header(o, central + retained, CACHE_NAME, cache_bytes, whole_crc, cache_local) &&
           central_header(o, central + retained + 46u + cache_name_size, STATE_NAME,
                          TXT_SAVE_FOOTER_SIZE, state_crc, state_local) &&
           final_eocd(o, central + retained + added, central, retained + added,
                      uint16_t(count + 2)) && o.truncate() && o.sync();
}

template<class Ops> bool finish_epub_append(Ops& ops, bool written, uint32_t original)
{
    if(!written) {
        // A returned I/O error may latch the handle. Reacquire it before seeking;
        // never truncate at a stale position when recovery or seek fails.
        (void)(ops.recover() && ops.seek(original) && ops.truncate() && ops.sync());
    }
    return written;
}

#ifdef __DEVKITARM__
struct FatOps {
    FIL& f;
    const char* name;
    bool seek(uint32_t at) { return f_lseek(&f, at) == FR_OK; }
    bool write(const unsigned char* bytes, uint32_t count) {
        UINT written = 0;
        return f_write(&f, bytes, count, &written) == FR_OK && written == count;
    }
    bool truncate() { return f_truncate(&f) == FR_OK; }
    bool sync() { return f_sync(&f) == FR_OK; }
    bool recover() {
        (void)f_close(&f);
        return f_open(&f, name, FA_READ | FA_WRITE | FA_OPEN_EXISTING) == FR_OK;
    }
};
bool same_history(const PageHistory&a,const PageHistory&b){if(a.count!=b.count)return false;for(int i=0;i<a.count;++i)if(a.offsets[(a.head+i)%PAGE_HISTORY_MAX]!=b.offsets[(b.head+i)%PAGE_HISTORY_MAX])return false;return true;}
#endif
}

const char* library_basename(const char* name) {
    if(!name) return name;
    const char prefix[] = "/gbareader/";
    unsigned i = 0;
    while(prefix[i] && name[i] == prefix[i]) ++i;
    return prefix[i] ? name : name + i;
}
bool txt_book_name(const char* name){name=library_basename(name);int n=0;while(name&&name[n]&&n<LIBRARY_NAME_MAX)++n;return n>4&&n<LIBRARY_NAME_MAX&&extension_equal(name+n-4,".txt");}
bool supported_book_name(const char* name){name=library_basename(name);int n=0;while(name&&name[n]&&n<LIBRARY_NAME_MAX)++n;return n<LIBRARY_NAME_MAX&&(txt_book_name(name)||(n>5&&extension_equal(name+n-5,".epub")));}
const char* save_result_string(bool saved){return saved?"Saved":"Save failed";}
bool book_size_without_footer(const char* name,uint32_t physical,const unsigned char*tail,uint32_t tail_size,uint32_t&logical,bool&valid,uint32_t&footer){logical=physical;valid=false;footer=0;if(!txt_book_name(name)||!tail)return false;const int sizes[]={TXT_SAVE_FOOTER_SIZE,TXT_SAVE_FOOTER_V2_SIZE,TXT_SAVE_FOOTER_V1_SIZE};for(int s:sizes)if(physical>=uint32_t(s)&&tail_size>=uint32_t(s)){const unsigned char*p=tail+tail_size-s;if(looks_like_txt_save_footer(p,s)){TxtSaveFooter f{};valid=parse_txt_save_footer(p,s,f);footer=s;logical=physical-footer;return true;}}return false;}
bool inspect_book_tail(const char*name,uint32_t physical,const unsigned char*tail,uint32_t tail_size,BookStorageLayout&l){
 l={physical,physical,0,0,0,0,false,false};if(!supported_book_name(name)||!tail)return false;
 const int sizes[]={TXT_SAVE_FOOTER_SIZE,TXT_SAVE_FOOTER_V2_SIZE,TXT_SAVE_FOOTER_V1_SIZE};
 for(int s:sizes)if(physical>=uint32_t(s)&&tail_size>=uint32_t(s)){const unsigned char*p=tail+tail_size-s;if(looks_like_txt_save_footer(p,s)){TxtSaveFooter footer{};l.has_valid_footer=parse_txt_save_footer(p,s,footer);l.footer_size=s;l.footer_offset=physical-s;l.book_size=txt_book_name(name)?physical-s:physical;break;}}
 if(txt_book_name(name)||!l.has_valid_footer||l.footer_size==0||physical<uint32_t(l.footer_size)+LEGACY_CACHE_TRAILER_SIZE||tail_size<uint32_t(l.footer_size)+LEGACY_CACHE_TRAILER_SIZE)return true;
 const unsigned char* trailer=tail+tail_size-l.footer_size-LEGACY_CACHE_TRAILER_SIZE;
 if(std::memcmp(trailer,LEGACY_CACHE_MAGIC,sizeof(LEGACY_CACHE_MAGIC))||get32(trailer+8)!=LEGACY_CACHE_TRAILER_SIZE||get32(trailer+28)!=legacy_hash(trailer))return true;
 const uint32_t cache_start=get32(trailer+12),text_size=get32(trailer+16),archive_size=get32(trailer+20);
 if(archive_size!=physical-uint32_t(l.footer_size)-LEGACY_CACHE_TRAILER_SIZE||!text_size||cache_start>archive_size||text_size>archive_size-cache_start)return true;
 l.book_size=archive_size;l.cache_start=cache_start;l.cache_size=text_size;l.cache_crc32=get32(trailer+24);return true;
}

bool storage_init(){name_count=0;
#ifdef __DEVKITARM__
REG_WAITCNT=0x40c0;set_supercard_mode(MAPPED_SDRAM,true,true);t_card_info info;if(sdcard_init(&info)||f_mount(&fatfs,"0:",1)!=FR_OK)return false;return scan_library();
#else
return false;
#endif
}
bool scan_library() {
    name_count = 0;
#ifdef __DEVKITARM__
    DIR directory; FILINFO entry;
    if(f_opendir(&directory, "/gbareader") != FR_OK) return false;
    bool ok = true;
    while(name_count < LIBRARY_MAX_FILES) {
        if(f_readdir(&directory, &entry) != FR_OK) { ok = false; break; }
        if(!entry.fname[0]) break;
        if(!(entry.fattrib & AM_DIR) && supported_book_name(entry.fname))
            copy_book_name(names[name_count++], entry.fname);
    }
    if(f_closedir(&directory) != FR_OK) ok = false;
    if(!ok) name_count = 0;
    return ok;
#else
    return false;
#endif
}
int library_count(){return name_count;}const char* library_name(int i){return i>=0&&i<name_count?names[i]:nullptr;}
bool library_path(int index, char (&path)[LIBRARY_PATH_MAX]) {
    path[0] = 0;
    const char* name = library_name(index);
    if(!name) return false;
    const char prefix[] = "/gbareader/";
    std::memcpy(path, prefix, sizeof(prefix) - 1);
    int at = sizeof(prefix) - 1;
    while(*name) path[at++] = *name++;
    path[at] = 0;
    return true;
}
ReaderFile::ReaderFile():_cache_start(0),_cache_size(0),_size(0),_physical_size(0),_footer_size(0),_footer_offset(0),_epub_cache_start(0),_epub_cache_size(0),_has_footer(false),_has_valid_cache(false),_open(false),_name{}{}
ReaderFile::~ReaderFile(){close();}
bool ReaderFile::open_read_only(const char*filename){close();
#ifdef __DEVKITARM__
if(!supported_book_name(filename)||f_open(&_file,filename,FA_READ|FA_OPEN_EXISTING)!=FR_OK)return false;_physical_size=uint32_t(f_size(&_file));_size=_physical_size;_footer_offset=_physical_size;_open=true;copy_book_name(_name,filename);if(txt_book_name(_name)){uint32_t n=_physical_size<TXT_SAVE_FOOTER_SIZE?_physical_size:TXT_SAVE_FOOTER_SIZE;unsigned char tail[TXT_SAVE_FOOTER_SIZE];UINT got=0;if(n&& (f_lseek(&_file,_physical_size-n)!=FR_OK||f_read(&_file,tail,n,&got)!=FR_OK||got!=n)){close();return false;}BookStorageLayout l{};if(!inspect_book_tail(_name,_physical_size,tail,n,l)){close();return false;}_size=l.book_size;_footer_offset=l.footer_offset;_footer_size=l.footer_size;_has_footer=l.has_valid_footer;}else{uint32_t n=_physical_size<uint32_t(TXT_SAVE_FOOTER_SIZE+LEGACY_CACHE_TRAILER_SIZE)?_physical_size:uint32_t(TXT_SAVE_FOOTER_SIZE+LEGACY_CACHE_TRAILER_SIZE);unsigned char tail[TXT_SAVE_FOOTER_SIZE+LEGACY_CACHE_TRAILER_SIZE];UINT got=0;if(n&&(f_lseek(&_file,_physical_size-n)!=FR_OK||f_read(&_file,tail,n,&got)!=FR_OK||got!=n)){close();return false;}BookStorageLayout l{};if(!inspect_book_tail(_name,_physical_size,tail,n,l)){close();return false;}_size=l.book_size;ZipLayout z{};if(zip_layout(*this,z)){if(l.book_size!=_physical_size&&l.has_valid_footer){_footer_offset=l.footer_offset;_footer_size=l.footer_size;_has_footer=true;}uint32_t data,size;if(find_state(*this,z,data,size)){_footer_offset=data;_footer_size=size;_has_footer=true;}}else{_size=_physical_size;}_cache_size=0;}return true;
#else
(void)filename;return false;
#endif
}
void ReaderFile::close(){
#ifdef __DEVKITARM__
if(_open)f_close(&_file);
#endif
_open=false;_size=_physical_size=_footer_size=_footer_offset=_epub_cache_start=_epub_cache_size=0;_cache_size=0;_has_footer=_has_valid_cache=false;_name[0]=0;}
bool ReaderFile::physical_byte_at(uint32_t offset,unsigned char&value)const{if(!_open||offset>=_physical_size)return false;if(offset<_cache_start||offset>=_cache_start+uint32_t(_cache_size)){
#ifdef __DEVKITARM__
_cache_start=offset&~uint32_t(FILE_WINDOW_BYTES-1);_cache_size=0;UINT n=0;if(f_lseek(&_file,_cache_start)!=FR_OK||f_read(&_file,_cache,sizeof(_cache),&n)!=FR_OK)return false;_cache_size=n;
#else
return false;
#endif
}if(offset-_cache_start>=uint32_t(_cache_size))return false;value=_cache[offset-_cache_start];return true;}
bool ReaderFile::byte_at(uint32_t o,unsigned char&v)const{return o<_size&&physical_byte_at(o,v);}bool ReaderFile::optimized_byte_at(uint32_t o,unsigned char&v)const{return _has_valid_cache&&o<_epub_cache_size&&physical_byte_at(_epub_cache_start+o,v);}
bool ReaderFile::read_range(uint32_t offset, unsigned char* output, uint32_t count) const
{
    if(!output || offset > _size || count > _size - offset) return false;
    while(count) {
        // Reuse the physical window refill/error handling once per block, not
        // the virtual byte-at fallback once per byte. Copy only valid bytes.
        unsigned char first;
        if(!physical_byte_at(offset, first)) return false;
        uint32_t take = uint32_t(_cache_size) - (offset - _cache_start);
        if(take > count) take = count;
        std::memcpy(output, _cache + offset - _cache_start, take);
        output += take; offset += take; count -= take;
    }
    return true;
}
bool ReaderFile::saved_footer(TxtSaveFooter& footer)const{if(!_open||!_has_footer||!_footer_size)return false;
#ifdef __DEVKITARM__
unsigned char b[TXT_SAVE_FOOTER_SIZE];UINT n=0;if(f_lseek(&_file,_footer_offset)!=FR_OK||f_read(&_file,b,_footer_size,&n)!=FR_OK||n!=_footer_size)return false;_cache_size=0;return parse_txt_save_footer(b,_footer_size,footer);
#else
(void)footer;return false;
#endif
}
bool ReaderFile::save_footer(const TxtSaveFooter& footer, const ByteSource* optimized)
{
    if(!_open || !supported_book_name(_name)) return false;
#ifdef __DEVKITARM__
    unsigned char replacement[TXT_SAVE_FOOTER_SIZE];
    make_txt_save_footer(footer, replacement);
    char filename[LIBRARY_PATH_MAX]{};
    std::memcpy(filename, _name, sizeof(filename));
    const uint32_t original = _physical_size, previous = _footer_size;
    if(previous) {
        UINT count = 0;
        if(f_lseek(&_file, _footer_offset) != FR_OK ||
           f_read(&_file, _previous_footer, previous, &count) != FR_OK || count != previous)
            return false;
    }
    _cache_size = 0;
    if(f_close(&_file) != FR_OK) {
        _open = false;
        open_read_only(filename);
        return false;
    }
    _open = false;
    if(f_open(&_file, filename, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK) {
        open_read_only(filename);
        return false;
    }
    _open = true;
    FatOps ops{_file, filename};
    bool written = false, made_cache = false;
    if(txt_book_name(filename)) {
        written = replace_txt_footer_transaction(ops, _footer_offset, original,
                                                 _previous_footer, previous, replacement);
        if(!written) (void)(ops.seek(original) && ops.truncate() && ops.sync());
    } else {
        ZipLayout layout{};
        const bool have_layout = zip_layout(*this, layout);
        const bool make_cache = optimized && optimized->size() && have_layout &&
                optimized->cache_archive_layout(layout.central, layout.size, layout.count);
        made_cache = make_cache;
        written = make_cache ?
                append_cache_and_state(ops, *this, layout, *optimized, replacement,
                                       _write_cache, sizeof(_write_cache), original) :
                (have_layout && append_state(ops, *this, layout, replacement,
                                             _write_cache, sizeof(_write_cache), original));
        written = finish_epub_append(ops, written, original);
    }
    const bool closed = f_close(&_file) == FR_OK;
    _open = false;
    const bool reopened = open_read_only(filename);
    if(!written || !closed || !reopened) return false;
    TxtSaveFooter verify{};
    const bool verified = saved_footer(verify) && verify.byte_offset == footer.byte_offset &&
           same_settings(verify.settings, footer.settings) && same_history(verify.history, footer.history);
    if(verified && made_cache) optimized->cache_persisted();
    return verified;
#else
    (void)footer;
    (void)optimized;
    return false;
#endif
}

#ifndef __DEVKITARM__
EpubAppendTestResult append_epub_transaction_for_tests(
        std::vector<unsigned char>& raw, const ByteSource* text, const TxtSaveFooter& state,
        EpubTestFault fault, int nth, uint32_t append_offset)
{
    EpubAppendTestResult result{};
    struct Faults {
        EpubAppendTestResult& result;
        EpubTestFault fault;
        int nth;
        bool latched = false;
        bool hit(EpubTestFault kind) {
            const int call = ++result.calls[int(kind)];
            if(fault == kind && call == nth) {
                result.fault_hit = true;
                latched = true;
                return true;
            }
            return false;
        }
    } faults{result, fault, nth};
    class Archive final : public ByteSource {
    public:
        const std::vector<unsigned char>& bytes;
        uint32_t length;
        Faults& faults;
        Archive(const std::vector<unsigned char>& data, Faults& f) :
                bytes(data), length(data.size()), faults(f) {}
        uint32_t size() const override { return length; }
        bool byte_at(uint32_t at, unsigned char& value) const override {
            return read_range(at, &value, 1);
        }
        bool read_range(uint32_t at, unsigned char* out, uint32_t count) const override {
            if(faults.hit(EpubTestFault::READ) || at > length || count > length - at) return false;
            std::memcpy(out, bytes.data() + at, count); return true;
        }
    } archive(raw, faults);
    const uint32_t physical_size = uint32_t(raw.size());
    const uint32_t tail_size = physical_size < TXT_SAVE_FOOTER_V2_SIZE + LEGACY_CACHE_TRAILER_SIZE ?
            physical_size : TXT_SAVE_FOOTER_V2_SIZE + LEGACY_CACHE_TRAILER_SIZE;
    BookStorageLayout legacy{};
    if(inspect_book_tail("legacy.epub", physical_size, raw.data() + physical_size - tail_size,
                         tail_size, legacy)) archive.length = legacy.book_size;
    if(!append_offset) append_offset = physical_size;
    struct Ops {
        std::vector<unsigned char>& bytes;
        Faults& faults;
        uint32_t pos = 0;
        bool seek(uint32_t at) {
            if(faults.hit(EpubTestFault::SEEK) || faults.latched) return false;
            pos = at; return true;
        }
        bool write(const unsigned char* data, uint32_t count) {
            const bool bad = faults.hit(EpubTestFault::WRITE);
            if(faults.latched && !bad) return false;
            const uint32_t take = bad ? count / 2 : count;
            if(uint64_t(pos) + take > 0xffffffffu) return false;
            if(pos + take > bytes.size()) bytes.resize(pos + take);
            std::memcpy(bytes.data() + pos, data, take); pos += take; return !bad;
        }
        bool truncate() {
            if(faults.hit(EpubTestFault::TRUNCATE) || faults.latched) return false;
            bytes.resize(pos); return true;
        }
        bool sync() { return !faults.hit(EpubTestFault::SYNC) && !faults.latched; }
        bool recover() { faults.latched = false; return true; }
    } ops{raw, faults};
    ZipLayout layout{};
    if(!zip_layout(archive, layout)) return result;
    unsigned char footer[TXT_SAVE_FOOTER_SIZE], scratch[512];
    make_txt_save_footer(state, footer);
    const bool written = text ?
            append_cache_and_state(ops, archive, layout, *text, footer, scratch, sizeof(scratch), append_offset) :
            append_state(ops, archive, layout, footer, scratch, sizeof(scratch), append_offset);
    result.success = finish_epub_append(ops, written, physical_size);
    return result;
}

bool write_epub_cache_file_for_tests(const char* input,const char* output,const EpubDocument& normalized){std::FILE*f=std::fopen(input,"rb");if(!f)return false;std::fseek(f,0,SEEK_END);long n=std::ftell(f);if(n<0){std::fclose(f);return false;}std::vector<unsigned char> raw(static_cast<size_t>(n), 0);std::rewind(f);if(std::fread(raw.data(),1,raw.size(),f)!=raw.size()){std::fclose(f);return false;}std::fclose(f);class V final:public ByteSource{public:std::vector<unsigned char>&d;V(std::vector<unsigned char>&x):d(x){}uint32_t size()const override{return d.size();}bool byte_at(uint32_t o,unsigned char&v)const override{if(o>=d.size())return false;v=d[o];return true;}} source(raw);uint32_t archive_size=source.size();if(raw.size()>=TXT_SAVE_FOOTER_V2_SIZE+LEGACY_CACHE_TRAILER_SIZE){BookStorageLayout legacy{};const uint32_t n=TXT_SAVE_FOOTER_V2_SIZE+LEGACY_CACHE_TRAILER_SIZE;if(inspect_book_tail("legacy.epub",source.size(),raw.data()+raw.size()-n,n,legacy)&&legacy.book_size<source.size())archive_size=legacy.book_size;}class P final:public ByteSource{public:const ByteSource&source;uint32_t n;P(const ByteSource&s,uint32_t x):source(s),n(x){}uint32_t size()const override{return n;}bool byte_at(uint32_t o,unsigned char&v)const override{return o<n&&source.byte_at(o,v);}} archive(source,archive_size);ZipLayout z{};if(!zip_layout(archive,z))return false;unsigned char footer[TXT_SAVE_FOOTER_SIZE]{};make_txt_save_footer(TxtSaveFooter{},footer);struct O{std::vector<unsigned char>&d;uint32_t p=0;bool seek(uint32_t x){p=x;if(p>d.size())d.resize(p);return true;}bool write(const unsigned char*x,uint32_t n){if(uint64_t(p)+n>0xffffffffu)return false;if(p+n>d.size())d.resize(p+n);std::memcpy(d.data()+p,x,n);p+=n;return true;}bool truncate(){d.resize(p);return true;}bool sync(){return true;}} ops{raw};unsigned char scratch[512];if(!append_cache_and_state(ops,archive,z,normalized,footer,scratch,sizeof(scratch),uint32_t(raw.size())))return false;f=std::fopen(output,"wb");if(!f)return false;bool ok=std::fwrite(raw.data(),1,raw.size(),f)==raw.size()&&std::fclose(f)==0;return ok;}
bool corrupt_epub_cache_file_for_tests(const char*path){std::FILE*f=std::fopen(path,"r+b");if(!f)return false;for(long p=0;;++p){if(std::fseek(f,p,SEEK_SET)||std::fgetc(f)==EOF)break;if(std::fseek(f,p,SEEK_SET))break;unsigned char b[8];if(std::fread(b,1,8,f)!=8)break;if(!std::memcmp(b,"GBAREPC5",8)){std::fseek(f,p+32,SEEK_SET);int x=std::fgetc(f);std::fseek(f,p+32,SEEK_SET);std::fputc(x^1,f);return std::fclose(f)==0;}}std::fclose(f);return false;}
FooterWriteTestResult footer_write_transaction_for_tests(uint32_t old_size,int first_limit,bool fail_sync){
    unsigned char old[TXT_SAVE_FOOTER_SIZE]{}, replacement[TXT_SAVE_FOOTER_SIZE];
    static const unsigned char v1[]="\n[GBAR-SAVE:1;O= 0000123456;S=1;T=1;B=1;C=59AAEAA4                                             \n";
    if(old_size==TXT_SAVE_FOOTER_V1_SIZE) std::memcpy(old,v1,old_size);
    else if(old_size==TXT_SAVE_FOOTER_V2_SIZE){static const unsigned char v2_magic[]="\n[GBAR-SAVE:2]\n";std::memcpy(old,v2_magic,sizeof(v2_magic)-1);put32(old+16,TXT_SAVE_FOOTER_V2_SIZE);put32(old+20,123456);old[24]=old[25]=old[26]=1;old[383]='\n';uint32_t h=2166136261u;for(int i=0;i<TXT_SAVE_FOOTER_V2_SIZE;++i)if(i<28||i>=32)h=(h^old[i])*16777619u;put32(old+28,h);}
    else { TxtSaveFooter saved{};saved.byte_offset=123456;saved.settings={1,1,1};make_txt_save_footer(saved,old); }
    std::memset(replacement,0x5a,sizeof(replacement));
    struct Ops { unsigned char bytes[1024]{}; uint32_t size=8,pos=0; int limit; bool bad_sync; int writes=0; bool seek(uint32_t p){if(p>size)return false;pos=p;return true;} bool write(const unsigned char* p,uint32_t n){uint32_t take=writes++?n:(n>uint32_t(limit)?uint32_t(limit):n);if(pos+take>sizeof(bytes))return false;std::memcpy(bytes+pos,p,take);pos+=take;if(pos>size)size=pos;return take==n;} bool truncate(){size=pos;return true;} bool sync(){bool bad=bad_sync;bad_sync=false;return !bad;} } ops{{},8,0,first_limit,fail_sync};
    std::memcpy(ops.bytes+8,old,old_size);ops.size=8+old_size;
    const bool ok=replace_txt_footer_transaction(ops,8,8+old_size,old,old_size,replacement);
    TxtSaveFooter parsed{};const bool restored=ops.size==8+old_size&&!std::memcmp(ops.bytes+8,old,old_size);
    return {ops.size,ok,restored,restored&&parse_txt_save_footer(ops.bytes+8,old_size,parsed)};
}
#endif
}
