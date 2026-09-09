#include "reader_body.h"
#include "reader_file.h"
#include "epub_document.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
extern "C" { void* font_base_addr; void* reader_font_base_addr; }
static std::vector<unsigned char> load(const char* path) {
 FILE* f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); auto n=ftell(f); rewind(f);
 std::vector<unsigned char> b(n); assert(fread(b.data(),1,n,f)==size_t(n)); fclose(f); return b;
}
static bool sink(void* p,const unsigned char* b,uint32_t n) {
 static_cast<std::string*>(p)->append(reinterpret_cast<const char*>(b),n); return true;
}
static void exercise(const reader::ByteSource& source) {
 using namespace reader;
 auto settings=default_settings(); Page p{},next{},back{}; PageHistory h{};
 assert(open_first_page(source,settings,body_glyph_width,h,p));
 unsigned pages=0; uint32_t anchor=0;
 do {
  assert(p.next_offset>p.start_offset);
  for(int i=0;i<p.line_count;++i) {
   auto& line=shape_reader_line(p.lines[i].text,body_glyph_width);
   assert(line.valid && arabic_extent(line)<=224);
   if(arabic::contains(p.lines[i].text)) {
    assert(strchr(p.lines[i].text,'?')==nullptr);
    uint16_t pixels[240*18/2]; for(auto& v:pixels)v=0x7777;
    draw_body_line(p.lines[i].text,reinterpret_cast<uint8_t*>(pixels)+248);
    auto* bytes=reinterpret_cast<uint8_t*>(pixels);
    for(int y=0;y<18;++y)for(int x=0;x<240;++x)
     if(y==0||y==17||x<8||x>=232)assert(bytes[y*240+x]==0x77);
   }
  }
  if(++pages==3)anchor=p.start_offset;
  if(p.eof)break;
  assert(next_page(source,settings,body_glyph_width,h,p,next));
  assert(next.start_offset==p.next_offset);
  assert(previous_page(source,settings,body_glyph_width,h,back));
  assert(back.start_offset==p.start_offset && back.next_offset==p.next_offset);
  assert(next_page(source,settings,body_glyph_width,h,back,next)); p=next;
  assert(pages<10000);
 }while(true);
 assert(pages>3 && anchor>0);
 for(unsigned version: {0u,1u,unsigned(CURRENT_DISPLAY_LAYOUT),9u}) {
  TxtSaveFooter saved{}; saved.byte_offset=anchor; saved.settings=settings; saved.display_layout=version;
  saved.history.count=1; saved.history.offsets[0]=0;
  unsigned char bytes[TXT_SAVE_FOOTER_SIZE]; make_txt_save_footer(saved,bytes);
  TxtSaveFooter parsed{}; assert(parse_txt_save_footer(bytes,sizeof(bytes),parsed));
  assert(parsed.byte_offset==anchor);
  assert(open_page_at(source,anchor,settings,body_glyph_width,h,p));
  PageHistoryRebuild rebuild{}; restore_saved_history(parsed,h,rebuild);
  if(version==CURRENT_DISPLAY_LAYOUT) {assert(h.count==1 && h.offsets[0]==0);continue;}
  assert(h.count==0 && h.lazy && h.lazy_anchor==anchor);
  begin_history_rebuild(anchor,rebuild);
  while(rebuild.state==HistoryRebuildState::BUILDING)
   step_history_rebuild(source,settings,body_glyph_width,rebuild);
  assert(adopt_rebuilt_history(rebuild,h));
  assert(previous_page(source,settings,body_glyph_width,h,back));
  assert(back.start_offset<anchor && back.next_offset>=anchor);
  assert(next_page(source,settings,body_glyph_width,h,back,next));
  assert(next.start_offset==back.next_offset);
 }
 printf("%u pages: shaped bounds, contiguous next/back, resume, layouts 0/1/2/9 passed\n",pages);
}
int main(int argc,char** argv) {
 assert(argc==6); auto base=load(argv[1]),symbols=load(argv[2]); font_base_addr=base.data();reader_font_base_addr=symbols.data();
 auto txt=load(argv[3]); const auto original=txt;
 reader::MemorySource text(txt.data(),txt.size()); exercise(text); assert(txt==original);
 auto zip=load(argv[4]); const auto original_zip=zip;
 reader::MemorySource archive(zip.data(),zip.size()); reader::EpubDocument epub;
 assert(epub.open(archive)); std::string normalized;assert(epub.export_text(sink,&normalized));
 assert(normalized.find(u8"السَّلَام عليكم 123 {hello}")!=std::string::npos);
 assert(normalized.find(u8"ب\u08f0ب")!=std::string::npos);
 exercise(epub); assert(zip==original_zip);
 reader::MemorySource normalized_txt(reinterpret_cast<const unsigned char*>(normalized.data()),normalized.size());
 exercise(normalized_txt);
 // Cache is written ONLY to the explicitly generated QA path supplied by the runner.
 assert(reader::write_epub_cache_file_for_tests(argv[4],argv[5],epub));
 auto cached_bytes=load(argv[5]);
 reader::MemorySource cached(cached_bytes.data(),cached_bytes.size()); reader::EpubDocument reopened;
 assert(reopened.open(cached)); std::string cache_text;assert(reopened.export_text(sink,&cache_text));
 assert(cache_text==normalized); exercise(reopened);
 puts("TXT, original EPUB and reopened EPUB cache retain Arabic bytes; all navigation checks passed");
}
