#include "reader_file.h"
#include <cstring>
#ifdef __DEVKITARM__
#include "bn_core.h"
#include "gbahw.h"
extern "C" {
#include "supercard_driver.h"
}
#endif
namespace reader { namespace {
#ifdef __DEVKITARM__
__attribute__((section(".sbss")))
#endif
char names[LIBRARY_MAX_FILES][LIBRARY_NAME_MAX];
int name_count;
#ifdef __DEVKITARM__
FATFS fatfs;
#endif
bool extension_equal(const char* value,const char* extension){while(*value&&*extension){char a=*value++,b=*extension++;if(a>='A'&&a<='Z')a=char(a+('a'-'A'));if(a!=b)return false;}return !*value&&! *extension;}
}
bool txt_book_name(const char* name){int length=0;while(name&&name[length]&&length<LIBRARY_NAME_MAX)++length;return length<LIBRARY_NAME_MAX&&length>4&&extension_equal(name+length-4,".txt");}
bool supported_book_name(const char* name){int length=0;while(name&&name[length]&&length<LIBRARY_NAME_MAX)++length;if(length>=LIBRARY_NAME_MAX)return false;return txt_book_name(name)||(length>5&&extension_equal(name+length-5,".epub"));}
bool book_size_without_footer(const char* name,uint32_t physical_size,const unsigned char tail[TXT_SAVE_FOOTER_SIZE],uint32_t& logical_size,bool& has_valid_footer){logical_size=physical_size;has_valid_footer=false;if(!supported_book_name(name)||physical_size<TXT_SAVE_FOOTER_SIZE||!tail)return false;TxtSaveFooter footer{};has_valid_footer=parse_txt_save_footer(tail,footer);if(!has_valid_footer&&!looks_like_txt_save_footer(tail))return false;logical_size=physical_size-TXT_SAVE_FOOTER_SIZE;return true;}
const char* save_result_string(bool saved){return saved?"Saved":"Save failed";}
namespace {
template<typename Ops>
bool write_footer_transaction(Ops& ops,uint32_t logical_size,const unsigned char replacement[TXT_SAVE_FOOTER_SIZE],const unsigned char* previous,bool had_previous){
 if(!ops.seek(logical_size))return false;
 if(ops.write(replacement,TXT_SAVE_FOOTER_SIZE)&&ops.sync())return true;
 // A failed first append must not leave trailing bytes after an EPUB EOCD.
 // A failed replacement must put the prior recognizable footer back.
 bool recovered=ops.seek(logical_size);
 if(recovered)recovered=had_previous?ops.write(previous,TXT_SAVE_FOOTER_SIZE):ops.truncate();
 if(recovered)recovered=ops.sync();
 (void)recovered;
 return false;
}
#ifdef __DEVKITARM__
struct FatFooterOps {
 FIL& file;
 bool seek(uint32_t offset){return f_lseek(&file,offset)==FR_OK;}
 bool write(const unsigned char* data,uint32_t size){UINT written=0;return f_write(&file,data,size,&written)==FR_OK&&written==size;}
 bool sync(){return f_sync(&file)==FR_OK;}
 bool truncate(){return f_truncate(&file)==FR_OK;}
};
#else
struct MemoryFooterOps {
 unsigned char data[8+TXT_SAVE_FOOTER_SIZE]{};
 uint32_t size=8;
 uint32_t position=0;
 int first_write_limit=TXT_SAVE_FOOTER_SIZE;
 bool first_sync_fails=false;
 int writes=0;
 int syncs=0;
 bool seek(uint32_t offset){if(offset>size)return false;position=offset;return true;}
 bool write(const unsigned char* input,uint32_t count){uint32_t amount=count;if(writes++==0&&first_write_limit<int(count))amount=uint32_t(first_write_limit);if(position+amount>sizeof(data))return false;std::memcpy(data+position,input,amount);position+=amount;if(position>size)size=position;return amount==count;}
 bool sync(){return !(syncs++==0&&first_sync_fails);}
 bool truncate(){size=position;return true;}
};
#endif
}
#ifndef __DEVKITARM__
FooterWriteTestResult footer_write_transaction_for_tests(bool existing_footer,int first_write_limit,bool first_sync_fails){
 unsigned char old_footer[TXT_SAVE_FOOTER_SIZE];unsigned char replacement[TXT_SAVE_FOOTER_SIZE];
 std::memset(old_footer,0xA5,sizeof(old_footer));std::memset(replacement,0x5A,sizeof(replacement));
 MemoryFooterOps ops{};ops.first_write_limit=first_write_limit;ops.first_sync_fails=first_sync_fails;
 if(existing_footer){std::memcpy(ops.data+8,old_footer,sizeof(old_footer));ops.size=8+TXT_SAVE_FOOTER_SIZE;}
 bool success=write_footer_transaction(ops,8,replacement,old_footer,existing_footer);
 bool restored=existing_footer&&ops.size==8+TXT_SAVE_FOOTER_SIZE&&std::memcmp(ops.data+8,old_footer,sizeof(old_footer))==0;
 return {ops.size,success,restored};
}
#endif
bool storage_init(){name_count=0;
#ifdef __DEVKITARM__
 REG_WAITCNT=0x40c0;set_supercard_mode(MAPPED_SDRAM,true,true);t_card_info info;if(sdcard_init(&info)!=0||f_mount(&fatfs,"0:",1)!=FR_OK)return false;DIR directory;FILINFO entry;if(f_opendir(&directory,"/")!=FR_OK)return false;while(name_count<LIBRARY_MAX_FILES&&f_readdir(&directory,&entry)==FR_OK&&entry.fname[0])if(!(entry.fattrib&AM_DIR)&&supported_book_name(entry.fname)){int i=0;while(entry.fname[i]&&i<LIBRARY_NAME_MAX-1){names[name_count][i]=entry.fname[i];++i;}names[name_count][i]=0;++name_count;}f_closedir(&directory);return true;
#else
 return false;
#endif
}
int library_count(){return name_count;}const char* library_name(int index){return index>=0&&index<name_count?names[index]:nullptr;}
ReaderFile::ReaderFile():_cache_start(0),_cache_size(0),_size(0),_physical_size(0),_has_footer(false),_open(false),_name{}{} ReaderFile::~ReaderFile(){close();}
bool ReaderFile::open_read_only(const char* filename){close();
#ifdef __DEVKITARM__
 if(!supported_book_name(filename) || f_open(&_file, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;
 _physical_size = uint32_t(f_size(&_file));
 _size = _physical_size;
 _open = true;
 int i = 0;
 while(filename[i] && i < LIBRARY_NAME_MAX - 1) { _name[i] = filename[i]; ++i; }
 _name[i] = 0;
 if(_physical_size>=TXT_SAVE_FOOTER_SIZE){unsigned char tail[TXT_SAVE_FOOTER_SIZE];UINT read=0;if(f_lseek(&_file,_physical_size-TXT_SAVE_FOOTER_SIZE)!=FR_OK||f_read(&_file,tail,sizeof(tail),&read)!=FR_OK||read!=sizeof(tail)){close();return false;}book_size_without_footer(_name,_physical_size,tail,_size,_has_footer);_cache_size=0;}
 return true;
#else
 (void)filename;return false;
#endif
}
void ReaderFile::close(){
#ifdef __DEVKITARM__
 if(_open)f_close(&_file);
#endif
 _open=false;_size=0;_physical_size=0;_cache_size=0;_has_footer=false;_name[0]=0;
}
bool ReaderFile::byte_at(uint32_t offset,unsigned char& value) const{if(!_open||offset>=_size)return false;if(offset<_cache_start||offset>=_cache_start+uint32_t(_cache_size)){
#ifdef __DEVKITARM__
 _cache_start=offset&~uint32_t(511);_cache_size=0;if(f_lseek(&_file,_cache_start)!=FR_OK)return false;UINT read=0;if(f_read(&_file,_cache,sizeof(_cache),&read)!=FR_OK)return false;_cache_size=int(read);
#else
 return false;
#endif
 }if(offset-_cache_start>=uint32_t(_cache_size))return false;value=_cache[offset-_cache_start];return true;}
bool ReaderFile::saved_footer(TxtSaveFooter& footer) const{if(!_open||!_has_footer)return false;
#ifdef __DEVKITARM__
 unsigned char tail[TXT_SAVE_FOOTER_SIZE];UINT read=0;if(f_lseek(&_file,_physical_size-TXT_SAVE_FOOTER_SIZE)!=FR_OK||f_read(&_file,tail,sizeof(tail),&read)!=FR_OK||read!=sizeof(tail))return false;_cache_size=0;return parse_txt_save_footer(tail,footer);
#else
 (void)footer;return false;
#endif
}
bool ReaderFile::save_footer(const TxtSaveFooter& footer){if(!_open||!supported_book_name(_name))return false;
#ifdef __DEVKITARM__
 char filename[LIBRARY_NAME_MAX]{};std::memcpy(filename,_name,sizeof(filename));
 unsigned char replacement[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(footer,replacement);
 const uint32_t logical_size=_size;const bool had_previous=_physical_size==_size+TXT_SAVE_FOOTER_SIZE;
 unsigned char previous[TXT_SAVE_FOOTER_SIZE]{};
 if(had_previous){UINT read=0;if(f_lseek(&_file,logical_size)!=FR_OK||f_read(&_file,previous,sizeof(previous),&read)!=FR_OK||read!=sizeof(previous)){_cache_size=0;return false;}}
 _cache_size=0;
 if(f_close(&_file)!=FR_OK){_open=false;open_read_only(filename);return false;}
 _open=false;
 if(f_open(&_file,filename,FA_WRITE|FA_OPEN_EXISTING)!=FR_OK){open_read_only(filename);return false;}
 _open=true;
 FatFooterOps ops{_file};const bool written=write_footer_transaction(ops,logical_size,replacement,previous,had_previous);
 const bool closed=f_close(&_file)==FR_OK;_open=false;
 const bool reopened=open_read_only(filename);
 if(!written||!closed||!reopened)return false;
 TxtSaveFooter verified{};
 return saved_footer(verified)&&verified.byte_offset==footer.byte_offset&&
        verified.settings.line_spacing==footer.settings.line_spacing&&
        verified.settings.top_margin==footer.settings.top_margin&&
        verified.settings.bottom_margin==footer.settings.bottom_margin;
#else
 (void)footer;return false;
#endif
}
}
