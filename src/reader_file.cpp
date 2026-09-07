#include "reader_file.h"
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
constexpr uint32_t CACHE_HEADER_SIZE = 32;

bool extension_equal(const char* a, const char* b) {
    while(*a && *b) { char x=*a++, y=*b++; if(x>='A'&&x<='Z') x=char(x+'a'-'A'); if(x!=y) return false; }
    return !*a && !*b;
}
void put16(unsigned char* p,uint16_t v){p[0]=unsigned(v);p[1]=unsigned(v>>8);}
void put32(unsigned char* p,uint32_t v){p[0]=unsigned(v);p[1]=unsigned(v>>8);p[2]=unsigned(v>>16);p[3]=unsigned(v>>24);}
uint16_t get16(const unsigned char* p){return uint16_t(p[0]|uint16_t(p[1])<<8);}
uint32_t get32(const unsigned char* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc_update(uint32_t c,const unsigned char* p,uint32_t n){for(uint32_t i=0;i<n;++i){c^=p[i];for(int b=0;b<8;++b)c=(c>>1)^(0xEDB88320u&uint32_t(0-int32_t(c&1)));}return c;}
uint32_t crc(const unsigned char* p,uint32_t n){return ~crc_update(0xffffffffu,p,n);}
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
    uint32_t p=z.central,c=0xffffffffu;for(uint16_t i=0;i<z.count;++i){uint16_t nl;uint32_t r;if(!central_record(s,p,z.central+z.size,nl,r))return false;if(!owned_name(s,p+46,nl)){uint32_t left=r,at=p;unsigned char block[512];while(left){uint32_t take=left>sizeof(block)?sizeof(block):left;if(!source_read(s,at,block,take))return false;c=crc_update(c,block,take);at+=take;left-=take;}}p+=r;}if(p!=z.central+z.size)return false;result=~c;return true;
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

template<class Ops> bool append_state(Ops&o,const ByteSource& archive,const ZipLayout& old,const unsigned char footer[TXT_SAVE_FOOTER_SIZE],unsigned char* scratch,uint32_t cap){
    if(old.count==0xffff) return false;
    const uint32_t base=archive.size(), state_local=base;
    const uint32_t state_crc=crc(footer,TXT_SAVE_FOOTER_SIZE), after=state_local+30u+uint32_t(std::strlen(STATE_NAME))+TXT_SAVE_FOOTER_SIZE, central=after;
    const uint64_t end=uint64_t(central)+old.size+46u+std::strlen(STATE_NAME)+22u;if(end>0xffffffffu)return false;
    return local_header(o,state_local,STATE_NAME,TXT_SAVE_FOOTER_SIZE,state_crc)&&write_all(o,state_local+30u+uint32_t(std::strlen(STATE_NAME)),footer,TXT_SAVE_FOOTER_SIZE)&&copy_source(o,central,archive,old.central,old.size,scratch,cap)&&central_header(o,central+old.size,STATE_NAME,TXT_SAVE_FOOTER_SIZE,state_crc,state_local)&&final_eocd(o,central+old.size+46u+uint32_t(std::strlen(STATE_NAME)),central,old.size+46u+uint32_t(std::strlen(STATE_NAME)),uint16_t(old.count+1))&&o.truncate()&&o.sync();
}

template<class Ops> bool append_cache_and_state(Ops&o,const ByteSource&archive,const ZipLayout&old,const ByteSource&text,const unsigned char footer[TXT_SAVE_FOOTER_SIZE],unsigned char*scratch,uint32_t cap){
    if(!text.size()||old.count>0xfffd) return false;
    uint32_t fingerprint;if(!central_fingerprint(archive,old,fingerprint))return false;
    uint32_t text_crc_run=0xffffffffu;for(uint32_t at=0;at<text.size();){uint32_t take=text.size()-at>cap?cap:text.size()-at;if(!text.read_range(at,scratch,take))return false;text_crc_run=crc_update(text_crc_run,scratch,take);at+=take;}const uint32_t text_crc=~text_crc_run;
    unsigned char header[CACHE_HEADER_SIZE]{};std::memcpy(header,"GBAREPC5",8);put16(header+8,5);put16(header+10,1);put32(header+12,fingerprint);put32(header+16,text.size());put32(header+20,text_crc);put32(header+24,text_crc);put32(header+28,crc(header,28));
    const uint32_t cache_bytes=CACHE_HEADER_SIZE+text.size();
    uint32_t c=crc_update(0xffffffffu,header,sizeof(header));for(uint32_t at=0;at<text.size();){uint32_t take=text.size()-at>cap?cap:text.size()-at;if(!text.read_range(at,scratch,take))return false;c=crc_update(c,scratch,take);at+=take;}const uint32_t whole_crc=~c;
    const uint32_t base=archive.size(), cache_local=base, cache_data=cache_local+30u+uint32_t(std::strlen(CACHE_NAME)), state_local=cache_data+cache_bytes, state_data=state_local+30u+uint32_t(std::strlen(STATE_NAME));const uint32_t central=state_data+TXT_SAVE_FOOTER_SIZE;const uint32_t state_crc=crc(footer,TXT_SAVE_FOOTER_SIZE);const uint32_t added=46u+uint32_t(std::strlen(CACHE_NAME))+46u+uint32_t(std::strlen(STATE_NAME));if(uint64_t(central)+old.size+added+22u>0xffffffffu)return false;
    if(!local_header(o,cache_local,CACHE_NAME,cache_bytes,whole_crc)||!write_all(o,cache_data,header,sizeof(header))||!copy_source(o,cache_data+sizeof(header),text,0,text.size(),scratch,cap)||!local_header(o,state_local,STATE_NAME,TXT_SAVE_FOOTER_SIZE,state_crc)||!write_all(o,state_data,footer,TXT_SAVE_FOOTER_SIZE)||!copy_source(o,central,archive,old.central,old.size,scratch,cap)||!central_header(o,central+old.size,CACHE_NAME,cache_bytes,whole_crc,cache_local)||!central_header(o,central+old.size+46u+uint32_t(std::strlen(CACHE_NAME)),STATE_NAME,TXT_SAVE_FOOTER_SIZE,state_crc,state_local)||!final_eocd(o,central+old.size+added,central,old.size+added,uint16_t(old.count+2))||!o.truncate()||!o.sync()) return false;
    return true;
}

#ifdef __DEVKITARM__
struct FatOps { FIL& f; bool seek(uint32_t p){return f_lseek(&f,p)==FR_OK;} bool write(const unsigned char*p,uint32_t n){UINT w=0;return f_write(&f,p,n,&w)==FR_OK&&w==n;} bool truncate(){return f_truncate(&f)==FR_OK;} bool sync(){return f_sync(&f)==FR_OK;} };
bool same_history(const PageHistory&a,const PageHistory&b){if(a.count!=b.count)return false;for(int i=0;i<a.count;++i)if(a.offsets[(a.head+i)%PAGE_HISTORY_MAX]!=b.offsets[(b.head+i)%PAGE_HISTORY_MAX])return false;return true;}
#endif
}

bool txt_book_name(const char* name){int n=0;while(name&&name[n]&&n<LIBRARY_NAME_MAX)++n;return n>4&&n<LIBRARY_NAME_MAX&&extension_equal(name+n-4,".txt");}
bool supported_book_name(const char* name){int n=0;while(name&&name[n]&&n<LIBRARY_NAME_MAX)++n;return n<LIBRARY_NAME_MAX&&(txt_book_name(name)||(n>5&&extension_equal(name+n-5,".epub")));}
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
REG_WAITCNT=0x40c0;set_supercard_mode(MAPPED_SDRAM,true,true);t_card_info info;if(sdcard_init(&info)||f_mount(&fatfs,"0:",1)!=FR_OK)return false;DIR d;FILINFO e;if(f_opendir(&d,"/")!=FR_OK)return false;while(name_count<LIBRARY_MAX_FILES&&f_readdir(&d,&e)==FR_OK&&e.fname[0])if(!(e.fattrib&AM_DIR)&&supported_book_name(e.fname)){std::strncpy(names[name_count],e.fname,LIBRARY_NAME_MAX-1);names[name_count][LIBRARY_NAME_MAX-1]=0;++name_count;}f_closedir(&d);return true;
#else
return false;
#endif
}
int library_count(){return name_count;}const char* library_name(int i){return i>=0&&i<name_count?names[i]:nullptr;}
ReaderFile::ReaderFile():_cache_start(0),_cache_size(0),_size(0),_physical_size(0),_footer_size(0),_footer_offset(0),_epub_cache_start(0),_epub_cache_size(0),_has_footer(false),_has_valid_cache(false),_open(false),_name{}{}
ReaderFile::~ReaderFile(){close();}
bool ReaderFile::open_read_only(const char*filename){close();
#ifdef __DEVKITARM__
if(!supported_book_name(filename)||f_open(&_file,filename,FA_READ|FA_OPEN_EXISTING)!=FR_OK)return false;_physical_size=uint32_t(f_size(&_file));_size=_physical_size;_footer_offset=_physical_size;_open=true;std::strncpy(_name,filename,LIBRARY_NAME_MAX-1);_name[LIBRARY_NAME_MAX-1]=0;if(txt_book_name(_name)){uint32_t n=_physical_size<TXT_SAVE_FOOTER_SIZE?_physical_size:TXT_SAVE_FOOTER_SIZE;unsigned char tail[TXT_SAVE_FOOTER_SIZE];UINT got=0;if(n&& (f_lseek(&_file,_physical_size-n)!=FR_OK||f_read(&_file,tail,n,&got)!=FR_OK||got!=n)){close();return false;}BookStorageLayout l{};if(!inspect_book_tail(_name,_physical_size,tail,n,l)){close();return false;}_size=l.book_size;_footer_offset=l.footer_offset;_footer_size=l.footer_size;_has_footer=l.has_valid_footer;}else{uint32_t n=_physical_size<uint32_t(TXT_SAVE_FOOTER_SIZE+LEGACY_CACHE_TRAILER_SIZE)?_physical_size:uint32_t(TXT_SAVE_FOOTER_SIZE+LEGACY_CACHE_TRAILER_SIZE);unsigned char tail[TXT_SAVE_FOOTER_SIZE+LEGACY_CACHE_TRAILER_SIZE];UINT got=0;if(n&&(f_lseek(&_file,_physical_size-n)!=FR_OK||f_read(&_file,tail,n,&got)!=FR_OK||got!=n)){close();return false;}BookStorageLayout l{};if(!inspect_book_tail(_name,_physical_size,tail,n,l)){close();return false;}_size=l.book_size;ZipLayout z{};if(zip_layout(*this,z)){if(l.book_size!=_physical_size&&l.has_valid_footer){_footer_offset=l.footer_offset;_footer_size=l.footer_size;_has_footer=true;}uint32_t data,size;if(find_state(*this,z,data,size)){_footer_offset=data;_footer_size=size;_has_footer=true;}}else{_size=_physical_size;}_cache_size=0;}return true;
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
bool ReaderFile::saved_footer(TxtSaveFooter& footer)const{if(!_open||!_has_footer||!_footer_size)return false;
#ifdef __DEVKITARM__
unsigned char b[TXT_SAVE_FOOTER_SIZE];UINT n=0;if(f_lseek(&_file,_footer_offset)!=FR_OK||f_read(&_file,b,_footer_size,&n)!=FR_OK||n!=_footer_size)return false;_cache_size=0;return parse_txt_save_footer(b,_footer_size,footer);
#else
(void)footer;return false;
#endif
}
bool ReaderFile::save_footer(const TxtSaveFooter& footer,const ByteSource* optimized){if(!_open||!supported_book_name(_name))return false;
#ifdef __DEVKITARM__
unsigned char replacement[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(footer,replacement);char filename[LIBRARY_NAME_MAX]{};std::memcpy(filename,_name,sizeof(filename));const uint32_t original=_physical_size, previous=_footer_size;if(previous){UINT n=0;if(f_lseek(&_file,_footer_offset)!=FR_OK||f_read(&_file,_previous_footer,previous,&n)!=FR_OK||n!=previous)return false;}_cache_size=0;if(f_close(&_file)!=FR_OK){_open=false;open_read_only(filename);return false;}_open=false;if(f_open(&_file,filename,FA_READ|FA_WRITE|FA_OPEN_EXISTING)!=FR_OK){open_read_only(filename);return false;}_open=true;FatOps ops{_file};bool written=false;if(txt_book_name(_name)){written=replace_txt_footer_transaction(ops,_footer_offset,original,_previous_footer,previous,replacement);}else{ZipLayout z{};const bool have_layout=zip_layout(*this,z);const bool make_cache=optimized&&optimized->size()&&have_layout&&optimized->cache_archive_layout(z.central,z.size,z.count);written=make_cache?append_cache_and_state(ops,*this,z,*optimized,replacement,_write_cache,sizeof(_write_cache)):(have_layout&&append_state(ops,*this,z,replacement,_write_cache,sizeof(_write_cache)));}if(!written){(void)ops.seek(original);(void)ops.truncate();(void)ops.sync();}const bool closed=f_close(&_file)==FR_OK;_open=false;const bool reopened=open_read_only(filename);if(!written||!closed||!reopened)return false;TxtSaveFooter verify{};return saved_footer(verify)&&verify.byte_offset==footer.byte_offset&&verify.settings.line_spacing==footer.settings.line_spacing&&verify.settings.top_margin==footer.settings.top_margin&&verify.settings.bottom_margin==footer.settings.bottom_margin&&same_history(verify.history,footer.history);
#else
(void)footer;(void)optimized;return false;
#endif
}

#ifndef __DEVKITARM__
bool write_epub_cache_file_for_tests(const char* input,const char* output,const EpubDocument& normalized){std::FILE*f=std::fopen(input,"rb");if(!f)return false;std::fseek(f,0,SEEK_END);long n=std::ftell(f);if(n<0){std::fclose(f);return false;}std::vector<unsigned char> raw(static_cast<size_t>(n), 0);std::rewind(f);if(std::fread(raw.data(),1,raw.size(),f)!=raw.size()){std::fclose(f);return false;}std::fclose(f);class V final:public ByteSource{public:std::vector<unsigned char>&d;V(std::vector<unsigned char>&x):d(x){}uint32_t size()const override{return d.size();}bool byte_at(uint32_t o,unsigned char&v)const override{if(o>=d.size())return false;v=d[o];return true;}} source(raw);uint32_t archive_size=source.size();if(raw.size()>=TXT_SAVE_FOOTER_V2_SIZE+LEGACY_CACHE_TRAILER_SIZE){BookStorageLayout legacy{};const uint32_t n=TXT_SAVE_FOOTER_V2_SIZE+LEGACY_CACHE_TRAILER_SIZE;if(inspect_book_tail("legacy.epub",source.size(),raw.data()+raw.size()-n,n,legacy)&&legacy.book_size<source.size())archive_size=legacy.book_size;}class P final:public ByteSource{public:const ByteSource&source;uint32_t n;P(const ByteSource&s,uint32_t x):source(s),n(x){}uint32_t size()const override{return n;}bool byte_at(uint32_t o,unsigned char&v)const override{return o<n&&source.byte_at(o,v);}} archive(source,archive_size);ZipLayout z{};if(!zip_layout(archive,z))return false;unsigned char footer[TXT_SAVE_FOOTER_SIZE]{};make_txt_save_footer(TxtSaveFooter{},footer);struct O{std::vector<unsigned char>&d;uint32_t p=0;bool seek(uint32_t x){p=x;if(p>d.size())d.resize(p);return true;}bool write(const unsigned char*x,uint32_t n){if(uint64_t(p)+n>0xffffffffu)return false;if(p+n>d.size())d.resize(p+n);std::memcpy(d.data()+p,x,n);p+=n;return true;}bool truncate(){d.resize(p);return true;}bool sync(){return true;}} ops{raw};unsigned char scratch[512];if(!append_cache_and_state(ops,archive,z,normalized,footer,scratch,sizeof(scratch)))return false;f=std::fopen(output,"wb");if(!f)return false;bool ok=std::fwrite(raw.data(),1,raw.size(),f)==raw.size()&&std::fclose(f)==0;return ok;}
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
