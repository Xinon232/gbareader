#include "reader_file.h"
#include "epub_document.h"
#include "diskio.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
using namespace reader;
static int fd=-1, target=0, seen=0, hits=0;
static char kind='-', policy='o';
static bool armed=false;
static bool fault(char k) {
 if(!armed || kind!=k) return false;
 ++seen;
 if(seen<target || (policy!='p' && seen!=target)) return false;
 ++hits;
 if(policy=='c') { std::fprintf(stderr,"POWER_CUT %c ordinal=%d\n",k,seen); fsync(fd); _exit(86); }
 return true;
}
extern "C" {
DSTATUS disk_initialize(BYTE){return fd<0?STA_NOINIT:0;}
DSTATUS disk_status(BYTE){return fd<0?STA_NOINIT:0;}
DRESULT disk_read(BYTE,BYTE* b,LBA_t s,UINT n){if(fault('r'))return RES_ERROR;return pread(fd,b,size_t(n)*512,off_t(s)*512)==ssize_t(n)*512?RES_OK:RES_ERROR;}
DRESULT disk_write(BYTE,const BYTE* b,LBA_t s,UINT n){if(fault('w'))return RES_ERROR;return pwrite(fd,b,size_t(n)*512,off_t(s)*512)==ssize_t(n)*512?RES_OK:RES_ERROR;}
DRESULT disk_ioctl(BYTE,BYTE cmd,void* b){if(cmd==CTRL_SYNC){if(fault('s'))return RES_ERROR;return fsync(fd)==0?RES_OK:RES_ERROR;}if(cmd==GET_SECTOR_COUNT){*(LBA_t*)b=lseek(fd,0,SEEK_END)/512;return RES_OK;}if(cmd==GET_SECTOR_SIZE){*(WORD*)b=512;return RES_OK;}if(cmd==GET_BLOCK_SIZE){*(DWORD*)b=1;return RES_OK;}return RES_PARERR;}
}
static TxtSaveFooter state(int version){TxtSaveFooter f{};f.byte_offset=version*123;f.settings={uint8_t(version%4+1),2,3};f.history.count=3;f.history.head=62;f.history.offsets[62]=0;f.history.offsets[63]=version*17;f.history.offsets[0]=version*51;return f;}
static void dump(const char* path,const unsigned char*b,size_t n){FILE*f=fopen(path,"wb");if(!f||fwrite(b,1,n,f)!=n)exit(8);fclose(f);}
int main(int argc,char**argv){
 if(argc!=9)return 2; // image book action version fault ordinal policy out-prefix
 fd=::open(argv[1],O_RDWR);if(fd<0)return 3;
 FATFS fs{};if(f_mount(&fs,"0:",1)!=FR_OK)return 4;
 ReaderFile file;bool opened=file.open_read_only(argv[2]);if(!opened){puts("{\"opened\":false}");return 0;}
 bool txt=txt_book_name(argv[2]);EpubDocument doc;bool doc_ok=txt||doc.open(file);
 auto wanted=state(atoi(argv[4]));unsigned char expected[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(wanted,expected);
 char path[4096];snprintf(path,sizeof(path),"%s.expected",argv[8]);dump(path,expected,sizeof(expected));
 bool saved=false;if(strcmp(argv[3],"probe")==0){
  if(doc_ok){const ByteSource& src=txt?static_cast<const ByteSource&>(file):static_cast<const ByteSource&>(doc);std::vector<unsigned char> text(src.size());bool ok=src.read_range(0,text.data(),text.size());if(!ok)doc_ok=false;else{snprintf(path,sizeof(path),"%s.text",argv[8]);dump(path,text.data(),text.size());}}
 }else if(doc_ok){kind=argv[5][0];target=atoi(argv[6]);policy=argv[7][0];armed=true;saved=file.save_footer(wanted,strcmp(argv[3],"cache")==0?&doc:nullptr);armed=false;}
 TxtSaveFooter actual{};bool footer=file.saved_footer(actual);bool state_match=false;
 if(footer){unsigned char b[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(actual,b);state_match=memcmp(b,expected,sizeof(b))==0;snprintf(path,sizeof(path),"%s.footer",argv[8]);dump(path,b,sizeof(b));}
 printf("{\"opened\":true,\"document\":%s,\"optimized\":%u,\"saved\":%s,\"hits\":%d,\"footer\":%s,\"bookmark\":%u,\"state_match\":%s}\n",doc_ok?"true":"false",txt?0:doc.optimized_size(),saved?"true":"false",hits,footer?"true":"false",actual.byte_offset,state_match?"true":"false");
 file.close();f_mount(nullptr,"0:",0);fsync(fd);::close(fd);return 0;
}
