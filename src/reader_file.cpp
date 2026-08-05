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
 if(txt_book_name(_name)&&_physical_size>=TXT_SAVE_FOOTER_SIZE){unsigned char tail[TXT_SAVE_FOOTER_SIZE];UINT read=0;if(f_lseek(&_file,_physical_size-TXT_SAVE_FOOTER_SIZE)!=FR_OK||f_read(&_file,tail,sizeof(tail),&read)!=FR_OK||read!=sizeof(tail)){close();return false;}TxtSaveFooter footer{};if(parse_txt_save_footer(tail,footer)||looks_like_txt_save_footer(tail)){_size=_physical_size-TXT_SAVE_FOOTER_SIZE;_has_footer=parse_txt_save_footer(tail,footer);}_cache_size=0;}
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
bool ReaderFile::save_footer(const TxtSaveFooter& footer){if(!_open||!txt_book_name(_name))return false;
#ifdef __DEVKITARM__
 char filename[LIBRARY_NAME_MAX]{}; std::memcpy(filename, _name, sizeof(filename)); unsigned char bytes[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(footer,bytes);uint32_t at=_size;if(f_close(&_file)!=FR_OK){_open=false;return false;}if(f_open(&_file,filename,FA_WRITE|FA_OPEN_EXISTING)!=FR_OK){_open=false;return false;}if(f_lseek(&_file,at)!=FR_OK){f_close(&_file);_open=false;return false;}UINT written=0;bool ok=f_write(&_file,bytes,sizeof(bytes),&written)==FR_OK&&written==sizeof(bytes)&&f_sync(&_file)==FR_OK;f_close(&_file);_open=false;if(!ok)return false;return open_read_only(filename);
#else
 (void)footer;return false;
#endif
}
}
