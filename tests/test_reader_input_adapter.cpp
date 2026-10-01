#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include "reader_ui_state.h"
#include "reader_hold.h"
namespace bn { namespace keypad {
unsigned held=0, pressed=0;
#define KEY(n,b) bool n##_pressed(){return pressed&(1u<<b);} bool n##_held(){return held&(1u<<b);}
KEY(up,0) KEY(down,1) KEY(left,2) KEY(right,3) KEY(a,4) KEY(b,5) KEY(start,6) KEY(select,7) KEY(l,8) KEY(r,9)
#undef KEY
} namespace core {void update(){}} }
namespace reader {
struct ByteSource {unsigned size() const {return 100;}};
struct Page {unsigned start_offset=0;};
struct Settings {};
struct PageHistory {};
enum class HistoryRebuildState {BUILDING,READY,FAILED};
struct PageHistoryRebuild {HistoryRebuildState state=HistoryRebuildState::FAILED;};
struct TxtSaveFooter {unsigned anchor; Settings settings;PageHistory history;PageHistoryRebuild rebuild;};
int forwards=0,backs=0,saves=0;
const char* save_status_message(bool,bool);
int page_percent(const Page&,unsigned){return 37;}
bool next_page(const ByteSource&,Settings,int,PageHistory&,Page&,Page&){++forwards;return true;}
bool previous_page(const ByteSource&,Settings,int,PageHistory&,Page&){++backs;return true;}
struct File:ByteSource {void close(){} bool save_footer(TxtSaveFooter,const ByteSource*){++saves;return true;}};
struct Epub:ByteSource {void close(){}};
}
using reader::Scene;
#include "reader_sampler.inc"
struct Sprites {bool visible=false;void clear(){visible=false;}};
std::string last_overlay;
void show_overlay(int,Sprites& sprites,const char* text,int=64){last_overlay=text;sprites.visible=true;}
void show_saving_overlay(int,Sprites&){}
void show_save_result(int,Sprites&,bool){}
struct GlobalValues {bool shoulder_page_turns=false;};
struct GlobalStub {GlobalValues values;int saves=0;bool ok=true;bool save(){++saves;return ok;}};
GlobalStub global_settings;
struct App {
reader::ReaderHold reader_hold{};

Scene scene=Scene::READER;reader::File file;reader::Epub epub;reader::Page page;
reader::Settings settings,settings_before;reader::PageHistory history;
reader::PageHistoryRebuild history_rebuild;reader::SaveMessageTimer save_message_timer{};
reader::ByteSource* active_source=&file;int glyph_width=0,save_ui=0;Sprites save_sprites;
Sprites sprites;
bool pending_back=false,redraw_page=false,redraw_ui=false;
int page_turns=0,goto_percent=0,goto_before=0;
const char* open_name="synthetic.txt";
unsigned saved_offset=99;
int settings_opens=0;
void open_settings(bool book_open){assert(book_open);++settings_opens;scene=Scene::SETTINGS;}
void frame(unsigned keys){bn::keypad::pressed=keys&~bn::keypad::held;bn::keypad::held=keys;
#include "reader_input.inc"
}
};
int main(){
App a;a.frame(1);assert(!a.reader_hold.shoulder_page_turns);a.frame(0);assert(!a.reader_hold.shoulder_page_turns);
// The Writer press sample is frame zero, followed by 48 held updates.
global_settings.saves=0;
a.frame(1);for(int i=1;i<48;++i){a.frame(1);assert(!a.reader_hold.shoulder_page_turns);}
assert(global_settings.saves==0);
a.frame(1);assert(a.reader_hold.shoulder_page_turns);
// The new mode is the remembered one: stored and saved at once.
assert(global_settings.values.shoulder_page_turns&&global_settings.saves==1);
assert(last_overlay=="L+R: On");
for(int i=0;i<59;++i){a.frame(1);assert(a.sprites.visible);}
a.frame(1);assert(!a.sprites.visible);
for(int i=0;i<1000;++i)a.frame(1);
assert(a.reader_hold.shoulder_page_turns&&global_settings.saves==1);a.frame(0);a.frame(1);
for(int i=0;i<48;++i)a.frame(1);
assert(!a.reader_hold.shoulder_page_turns);a.frame(0);
assert(last_overlay=="L+R: Off"&&!global_settings.values.shoulder_page_turns&&global_settings.saves==2);
// A failed save still switches the mode and says so.
global_settings.ok=false;a.frame(1);for(int i=0;i<48;++i)a.frame(1);a.frame(0);
assert(a.reader_hold.shoulder_page_turns&&last_overlay=="Settings save failed");
global_settings.ok=true;a.frame(1);for(int i=0;i<48;++i)a.frame(1);a.frame(0);assert(!a.reader_hold.shoulder_page_turns);
// A second toggle resets the whole lifetime, including while the old text is visible.
a.frame(1);for(int i=0;i<48;++i)a.frame(1);
a.frame(0);a.frame(1);for(int i=0;i<48;++i)a.frame(1);
assert(last_overlay=="L+R: Off" && a.reader_hold.mode_message_frames==60);
// Select opens Settings (Go to starts at the current percentage).
a.frame(0);a.frame(128);assert(a.scene==Scene::SETTINGS&&a.settings_opens==1&&a.goto_percent==37&&a.goto_before==37);
assert(!a.sprites.visible && a.reader_hold.mode_message_frames==0);
// Holding Down does nothing any more, however long.
{App d;d.frame(0);int saves=global_settings.saves;
 for(int i=0;i<2000;++i){d.frame(2);assert(d.scene==Scene::READER&&!d.reader_hold.shoulder_page_turns);}
 d.frame(0);assert(d.settings_opens==0&&global_settings.saves==saves&&reader::forwards==0&&reader::backs==0);}
// Every subthreshold release is silent; every companion cancels the whole hold.
for(int duration=0;duration<48;++duration){
 App t;t.frame(0);t.frame(1);for(int i=0;i<duration;++i)t.frame(1);
 t.frame(0);assert(!t.reader_hold.shoulder_page_turns && t.scene==Scene::READER);
}
for(unsigned companion=2;companion<1024;companion<<=1){
 for(int order=0;order<2;++order){
  App t;t.frame(0);if(order==0){t.frame(1);for(int i=0;i<47;++i)t.frame(1);}
  t.frame(1|companion);t.frame(1);
  // Select may legitimately open Settings. Other pre-existing actions remain eligible.
  const auto scene_after_chord=t.scene;
  for(int i=0;i<100;++i)t.frame(1);
  assert(!t.reader_hold.shoulder_page_turns && t.scene==scene_after_chord);
  t.frame(0);t.scene=Scene::READER;t.frame(1);for(int i=0;i<48;++i)t.frame(1);
  assert(t.reader_hold.shoulder_page_turns);
 }
}
for(Scene other : {Scene::LIBRARY,Scene::SETTINGS,Scene::ABOUT,Scene::IMPORT,Scene::IMPORT_CONFIRM}){
 App t;t.frame(0);t.frame(1);for(int i=0;i<47;++i)t.frame(1);
 t.scene=other;t.frame(1);t.scene=Scene::READER;
 for(int i=0;i<100;++i)t.frame(1);
 assert(!t.reader_hold.shoulder_page_turns && t.scene==Scene::READER);
}
// Page-turn keys and Start remain immediate; shoulder toggling does not turn a page.
App p;p.frame(0);reader::forwards=reader::backs=reader::saves=0;
for(unsigned key : {8u,16u,4u,32u,64u}){p.frame(key);p.frame(0);}
assert(reader::forwards==2 && reader::backs==2 && reader::saves==1);
assert(p.saved_offset==p.page.start_offset); // A successful Start save marks the page as saved.
assert(p.page_turns==0); // Two forward and two back turns cancel out.
p.frame(256|512);p.frame(0);assert(reader::forwards==2 && reader::backs==2);
p.frame(1);for(int i=0;i<48;++i)p.frame(1);p.frame(0);
assert(reader::forwards==2 && reader::backs==2);
p.frame(512);p.frame(0);p.frame(256);p.frame(0);
assert(reader::forwards==3 && reader::backs==3);
assert(p.page_turns==0);
puts("PASS: production controls: hold Up 0..47 releases, 48 boundary, saved once per toggle, Down hold inert, Select opens Settings, every companion/order, scene tails, unchanged navigation/save; overlay 59/60 expiry/reset/exit");
}
