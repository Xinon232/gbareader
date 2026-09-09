#include "reader_file.h"
#include "reader_open.h"
#include "epub_document.h"
#include "diskio.h"
#include <cstdio>
#include <cassert>
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
static TxtSaveFooter state(int version){TxtSaveFooter f{};f.byte_offset=version*123;f.settings={uint8_t(version%4+1),2,3};f.settings.arabic_shaping=true;f.history.count=3;f.history.head=62;f.history.offsets[62]=0;f.history.offsets[63]=version*17;f.history.offsets[0]=version*51;return f;}
static void dump(const char* path,const unsigned char*b,size_t n){FILE*f=fopen(path,"wb");if(!f||fwrite(b,1,n,f)!=n)exit(8);fclose(f);}
int main(int argc,char**argv){
 if(argc!=9)return 2; // image book action version fault ordinal policy out-prefix
 fd=::open(argv[1],O_RDWR);if(fd<0)return 3;
 FATFS fs{};if(f_mount(&fs,"0:",1)!=FR_OK)return 4;
 if(std::strncmp(argv[2],"/gbareader/",11)==0) {
  assert(scan_library()); assert(library_count()==2);
  bool found=false;
  for(int i=0;i<library_count();++i) {
   char library_file[LIBRARY_PATH_MAX]; assert(library_path(i,library_file));
   if(std::strcmp(library_file,argv[2])==0)found=true;
   assert(std::strcmp(library_name(i),"outside.txt")!=0);
  }
  assert(found);
 }
 ReaderFile file;bool opened=file.open_read_only(argv[2]);if(!opened){puts("{\"opened\":false}");return 0;}
 bool txt=txt_book_name(argv[2]);EpubDocument doc;bool doc_ok=txt||doc.open(file);
 if(std::strncmp(argv[3],"mode",4)==0) {
  assert(doc_ok);
  const ByteSource& source = txt ? static_cast<const ByteSource&>(file) : static_cast<const ByteSource&>(doc);
  TxtSaveFooter before{}; const bool had_footer = file.saved_footer(before);
  Settings settings = default_settings(); Page page{}; PageHistory history{}; PageHistoryRebuild rebuild{};
  struct Context { ReaderFile& file; const ByteSource* cache; int calls; } context{file,txt?nullptr:&doc,0};
  kind=argv[5][0];target=atoi(argv[6]);policy=argv[7][0];
  auto persist=[](void* opaque,const TxtSaveFooter& state) {
   auto& c=*static_cast<Context*>(opaque); ++c.calls;
   armed=true; const bool result=c.file.save_footer(state,c.cache); armed=false; return result;
  };
  const auto result=open_document_page(source,had_footer?&before:nullptr,settings,nullptr,
                    history,page,rebuild,!txt&&!doc.optimized_size(),persist,&context);
  assert(result!=OpenResult::FAILED && settings.arabic_shaping==(atoi(argv[4])!=0));
  if(std::strcmp(argv[3],"mode-latin")==0) {
   // Fixture's final 100 bytes are strictly ASCII; bookmark it and change spacing.
   assert(source.size()>100); const uint32_t anchor=source.size()-100;
   assert(open_page_at(source,anchor,settings,nullptr,history,page));
   for(uint32_t at=anchor;at<source.size();++at) { unsigned char ch;assert(source.byte_at(at,ch)&&ch<128); }
   adjust_setting(settings,SettingField::LINE_SPACING,1);
   begin_history_rebuild(anchor,rebuild);
   assert(file.save_footer({anchor,settings,history,rebuild},txt?nullptr:&doc));
  }
  if(std::strcmp(argv[3],"mode-bookmark-arabic")==0) {
   uint32_t anchor=0;
   for(;anchor<source.size();++anchor) { unsigned char ch;assert(source.byte_at(anchor,ch));if(ch==0xD8||ch==0xD9)break; }
   assert(anchor<source.size() && !settings.arabic_shaping);
   assert(open_page_at(source,anchor,settings,nullptr,history,page));
   begin_history_rebuild(anchor,rebuild);
   assert(file.save_footer({anchor,settings,history,rebuild},txt?nullptr:&doc));
  }
  TxtSaveFooter actual{};const bool footer=file.saved_footer(actual);
  if(result==OpenResult::SAVED || had_footer) assert(footer);
  if(result==OpenResult::SAVED || (had_footer&&before.settings.arabic_shaping))
   assert(actual.settings.arabic_shaping==settings.arabic_shaping);
  printf("{\"result\":%d,\"calls\":%d,\"on\":%s,\"persisted_on\":%s,\"bookmark\":%u,\"page_anchor\":%u,\"hits\":%d}\n",
       int(result),context.calls,settings.arabic_shaping?"true":"false",
       footer&&actual.settings.arabic_shaping?"true":"false",actual.byte_offset,page.start_offset,hits);
  file.close(); f_mount(nullptr,"0:",0); fsync(fd);::close(fd);return 0;
 }
 auto wanted=state(atoi(argv[4]));unsigned char expected[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(wanted,expected);
 char path[4096];snprintf(path,sizeof(path),"%s.expected",argv[8]);dump(path,expected,sizeof(expected));
 bool saved=false;
 if(std::strncmp(argv[3],"repeat",6)==0) {
  assert(doc_ok && !txt);
  if(std::strcmp(argv[3],"repeat-fallback")==0) {
   unsigned char value; assert(doc.byte_at(doc.size()-1,value)); assert(!doc.optimized_size());
  }
  saved=file.save_footer(wanted,&doc); assert(saved);
  uint32_t central,bytes;uint16_t entries;
  assert(!doc.cache_archive_layout(central,bytes,entries));
  const uint32_t first=file.size();
  unsigned char eocd[22];assert(file.read_range(first-22,eocd,sizeof(eocd)));
  assert(eocd[0]==0x50 && eocd[1]==0x4b && eocd[2]==5 && eocd[3]==6);
  const uint32_t directory_bytes=uint32_t(eocd[12])|(uint32_t(eocd[13])<<8)|
          (uint32_t(eocd[14])<<16)|(uint32_t(eocd[15])<<24);
  // A state-only append copies the same-size directory, then replaces its
  // state record. Compare the exact transaction size, not the book length:
  // a valid tiny book can be much shorter than its 800-byte state record.
  const uint32_t state_append_bytes=30u+uint32_t(std::strlen("META-INF/gbareader/state-v5"))+
          TXT_SAVE_FOOTER_SIZE+directory_bytes+22u;
  saved=file.save_footer(wanted,&doc); assert(saved);
  assert(file.size()>first && file.size()-first==state_append_bytes);
  EpubDocument check;assert(check.open(file)&&check.optimized_size()==doc.size());
 }else if(strcmp(argv[3],"probe")==0){
  if(doc_ok){const ByteSource& src=txt?static_cast<const ByteSource&>(file):static_cast<const ByteSource&>(doc);std::vector<unsigned char> text(src.size());bool ok=src.read_range(0,text.data(),text.size());if(!ok)doc_ok=false;else{snprintf(path,sizeof(path),"%s.text",argv[8]);dump(path,text.data(),text.size());}}
 }else if(doc_ok){kind=argv[5][0];target=atoi(argv[6]);policy=argv[7][0];armed=true;saved=file.save_footer(wanted,strcmp(argv[3],"cache")==0?&doc:nullptr);armed=false;}
 TxtSaveFooter actual{};bool footer=file.saved_footer(actual);bool state_match=false;
 if(footer){unsigned char b[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(actual,b);state_match=memcmp(b,expected,sizeof(b))==0;snprintf(path,sizeof(path),"%s.footer",argv[8]);dump(path,b,sizeof(b));}
 printf("{\"opened\":true,\"document\":%s,\"optimized\":%u,\"saved\":%s,\"hits\":%d,\"footer\":%s,\"bookmark\":%u,\"state_match\":%s}\n",doc_ok?"true":"false",txt?0:doc.optimized_size(),saved?"true":"false",hits,footer?"true":"false",actual.byte_offset,state_match?"true":"false");
 file.close();f_mount(nullptr,"0:",0);fsync(fd);::close(fd);return 0;
}
