#include "reader_core.h"
#include "reader_txt_save.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
 const char* text=u8"السَّلَام عليكم 123 {hello}";
 reader::MemorySource src(reinterpret_cast<const unsigned char*>(text),std::strlen(text));
 reader::Page p{};
 auto settings = reader::default_settings(); settings.arabic_shaping = true;
 assert(reader::layout_page(src,0,settings,nullptr,p));
 assert(std::strcmp(p.lines[0].text,text)==0); // Logical display bytes survive decode.
 assert(p.next_offset==std::strlen(text));
 puts("Arabic logical TXT decode passed");
 const char* joined=u8"لالالالالالالالالالالالالالالالالالالالالا";
 reader::MemorySource lig(reinterpret_cast<const unsigned char*>(joined),std::strlen(joined));
 assert(reader::layout_page(lig,0,settings,nullptr,p));
 assert(p.line_count==1); // Native Ghoulam lam-alef is much narrower than two fallback cells.
 assert(std::strcmp(p.lines[0].text,joined)==0);
 puts("Arabic joining-aware wrapping passed");
 reader::TxtSaveFooter old{};
 old.byte_offset=1000; old.settings=reader::default_settings();
 old.display_layout=1; old.history.count=1; old.history.offsets[0]=400;
 unsigned char bytes[reader::TXT_SAVE_FOOTER_SIZE];
 reader::make_txt_save_footer(old,bytes);
 reader::TxtSaveFooter restored{};
 assert(reader::parse_txt_save_footer(bytes,sizeof(bytes),restored));
 reader::PageHistory history{}; reader::PageHistoryRebuild rebuild{};
 reader::restore_saved_history(restored,history,rebuild);
 assert(restored.byte_offset==1000);
 assert(history.count==0 && history.lazy && history.lazy_anchor==1000);
 puts("V1.1 bookmark anchor preserved; incompatible history invalidated");
}
