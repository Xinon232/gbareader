#include "reader_body.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
extern "C" { void* font_base_addr; void* reader_font_base_addr; }
static std::vector<unsigned char> load(const char* p) {
 FILE* f=fopen(p,"rb"); assert(f); fseek(f,0,SEEK_END); auto n=ftell(f); rewind(f);
 std::vector<unsigned char> b(n); assert(fread(b.data(),1,n,f)==size_t(n)); fclose(f); return b;
}
using Pixels=std::array<uint16_t,240*18/2>;
int main(int argc,char** argv) {
 assert(argc==3); auto a=load(argv[1]),b=load(argv[2]); font_base_addr=a.data(); reader_font_base_addr=b.data();
 Pixels actual{},expected{};
 reader::draw_body_line(u8"لا",reinterpret_cast<uint8_t*>(actual.data())+240+8, true);
 // One isolated lam-alef; right aligned by visible extent, real native bitmap.
 const auto& g=arabic::glyphs[arabic::ligatures[0].form[0]];
 const auto& shaped=arabic::shape(u8"لا",[](const char* s){return font_width(s);});
 assert(shaped.count==1 && shaped.items[0].glyph);
 const auto& real=arabic::glyphs[shaped.items[0].glyph]; (void)g;
 int left=8+224-reader::arabic_extent(shaped);
 auto* bytes=reinterpret_cast<uint8_t*>(expected.data());
 for(int y=0;y<real.height;++y)for(int x=0;x<real.width;++x)
  if(arabic::bitmap[real.offset+y]&(1u<<x))bytes[(1+real.top+y)*240+left+real.left+x]=1;
 assert(actual==expected && actual!=Pixels{});
 for(const char* s:{"Hello, 123!",u8"한글 日本語 Ω Ж € ™",u8"السَّلَام عليكم 123 {hello}",u8"abc العربية / 123!",u8"ببببببببببببببببببببببببببببببببببببببببببببب",u8"\u08f0\u064e", "\xff\xc0\xe2"}) {
  actual.fill(0x5555); expected=actual;
  reader::draw_body_line(s,reinterpret_cast<uint8_t*>(actual.data())+240+8, true);
  if(!arabic::contains(s)) {
   const char* expected_text = static_cast<unsigned char>(s[0]) == 0xff ? "???" : s;
   draw_text_idx8_bus16_range(expected_text,reinterpret_cast<uint8_t*>(expected.data())+240+8,0,224,240,1);
   assert(actual==expected); // Entire untouched legacy bitmap path, including full-width glyphs.
  }
  auto* px=reinterpret_cast<uint8_t*>(actual.data());
  for(int y=0;y<18;++y)for(int x=0;x<240;++x)
   if(y==0||y==17||x<8||x>=232)assert(px[y*240+x]==0x55);
 }
 puts("Arabic native pixels, RTL alignment, mixed/full-width bounds and legacy pixel parity passed");
 actual.fill(0); expected.fill(0);
 reader::draw_body_line(u8"لا", reinterpret_cast<uint8_t*>(actual.data())+240+8, false);
 draw_text_idx8_bus16_range(u8"لا", reinterpret_cast<uint8_t*>(expected.data())+240+8, 0, 224, 240, 1);
 assert(actual == expected); // OFF must not shape or scan for Arabic.
 for(const char* malformed : {"\xC2", "\xE2\x82", "\xED\xA0\x80", "\xF0\x80\x80\x80", "\xFF\xC0\xE2"}) {
  reader::MemorySource source(reinterpret_cast<const unsigned char*>(malformed), std::strlen(malformed));
  reader::Page page{};
  assert(reader::layout_page(source, 0, reader::default_settings(), reader::body_glyph_width, page));
  actual.fill(0x5555); expected = actual;
  arabic::scratch.count = 123;
  reader::draw_body_line(page.lines[0].text, reinterpret_cast<uint8_t*>(actual.data())+240+8, false);
  char replacement[5] = {}; std::memset(replacement, '?', std::strlen(malformed));
  draw_text_idx8_bus16_range(replacement, reinterpret_cast<uint8_t*>(expected.data())+240+8, 0, 224, 240, 1);
  assert(actual == expected && arabic::scratch.count == 123);
 }
 puts("PASS: core malformed canonical bytes through native OFF framebuffer, no shaping");

}
