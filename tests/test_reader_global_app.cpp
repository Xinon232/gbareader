#include "global_fatfs_mock.h"
#include "reader_ui_state.h"
#include "reader_hold.h"
#include "reader_open.h"
#include <tuple>
namespace bn {
namespace keypad {unsigned held=0,pressed=0;
#define KEY(n,b) bool n##_pressed(){return pressed&(1u<<b);} bool n##_held(){return held&(1u<<b);}
KEY(up,0) KEY(down,1) KEY(left,2) KEY(right,3) KEY(a,4) KEY(b,5) KEY(start,6) KEY(select,7) KEY(l,8) KEY(r,9)
#undef KEY
} namespace core {void update(){}}
template<int N> using string=std::string;
template<int N> std::string to_string(int v){return std::to_string(v);}
}
namespace reader {
static std::vector<std::string> names={"one.txt","two.epub","three.txt"};
bool storage_init(){return true;}int library_count(){return names.size();}
const char* library_name(int i){return names.at(i).c_str();}
}
using reader::Scene;
#include "reader_sampler.inc"
struct Sprites {void clear(){}};
struct Generator {int palette;void set_left_alignment(){}void set_center_alignment(){}};
using Draw=std::tuple<int,int,int,std::string>;
static std::vector<Draw> draw;
void add_text(Generator g,int x,int y,const char* text,Sprites&){draw.emplace_back(g.palette,x,y,text);}
static std::string overlay;
void show_overlay(int,Sprites&,const char* text,int=64){overlay=text;}
void show_saving_overlay(int,Sprites&){}
struct Source:reader::ByteSource {
    int saves=0;bool save_ok=true;
    uint32_t size()const override{return 12000;}
    bool byte_at(uint32_t at,unsigned char& c)const override{c=at%40==39?'\n':'a';return at<size();}
    bool save_footer(const reader::TxtSaveFooter&,const reader::ByteSource*){++saves;return save_ok;}
    void close(){}
};
struct App {
    reader::GlobalSettingsStore global_settings;
    reader::Settings settings{},settings_before{};
    Scene scene=Scene::READER;
    reader::ReaderHold reader_hold{};
    reader::Page page{};reader::PageHistory history{};reader::PageHistoryRebuild history_rebuild{};
    reader::PageCount page_count{};
    reader::SaveMessageTimer save_message_timer{};
    Source file,epub;const reader::ByteSource* active_source=&file;
    reader::GlyphWidth glyph_width=nullptr;
    bool pending_back=false,redraw_ui=false,redraw_page=false;
    int page_turns=0,goto_percent=0,goto_before=0,settings_row=0,save_ui=0,selected=0;
    int restarts=0,retargets=0;
    const char* open_name="one.txt";
    static constexpr int STARTUP_ROW=reader::SETTING_FIELD_COUNT,GOTO_ROW=STARTUP_ROW+1,ROW_X=-60;
    Sprites sprites,save_sprites;Generator ui{1},hint_ui{2},cursor_ui{3};
    App(){settings=reader::default_settings();settings_before=settings;reader::layout_page(file,1000,settings,nullptr,page);}
    void boot(){
#include "global_boot.inc"
        (void)scene;(void)settings_row;(void)GOTO_ROW;(void)settings_before;(void)goto_percent;(void)goto_before;
        (void)count_refresh_frames;(void)redraw_ui;(void)redraw_page;(void)open_name;(void)active_source;
        (void)library_status;(void)save_message_timer;(void)pending_back;(void)credits_gate;(void)controls_page;
        this->selected=selected;this->reader_hold=reader_hold;this->scene=scene;
    }
    void restart_page_count(){++restarts;}
    void retarget_page_count(){++retargets;}
    void opened(reader::OpenResult opened,const char* name){open_name=name;
#include "global_open_success.inc"
    }
    void frame(unsigned keys){bn::keypad::pressed=keys&~bn::keypad::held;bn::keypad::held=keys;
#include "reader_input.inc"
        if(scene==Scene::SETTINGS){
#include "global_settings_input.inc"
        }
    }
    void tap(unsigned keys){frame(0);frame(keys);frame(0);}
    void settings_draw(){
#include "global_settings_draw.inc"
    }
    void enter(){scene=Scene::SETTINGS;settings_before=settings;goto_before=goto_percent;}
};
int main(){
    for(bool startup:{false,true}){
        files.clear();reset_fault();GlobalPreferences p{};p.shoulder_startup=startup;remember_global_book(p,"two.epub");files[paths[0]]=record(p,1);
        App a;a.boot();assert(a.scene==Scene::LIBRARY&&a.selected==1&&a.reader_hold.shoulder_page_turns==startup);
        a.opened(reader::OpenResult::FAILED,"three.txt");assert(!strcmp(a.global_settings.values.last_book,"two.epub"));
        writes=0;a.opened(reader::OpenResult::OPENED,"two.epub");assert(writes==0);
        a.opened(reader::OpenResult::OPENED,"three.txt");assert(writes==1&&a.file.saves==0);
        a.tap(0);a.frame(1);for(int i=0;i<48;++i)a.frame(1);a.frame(0);
        const bool live=a.reader_hold.shoulder_page_turns;assert(live!=startup&&writes==1);
        a.opened(reader::OpenResult::OPENED,"one.txt");assert(a.reader_hold.shoulder_page_turns==live&&writes==2);
        a.enter();a.settings_row=App::STARTUP_ROW;int restart=a.restarts;int before=writes;
        a.tap(startup?4:8);assert(a.reader_hold.shoulder_page_turns==live&&writes==before);
        a.tap(32);assert(a.scene==Scene::READER&&writes==before+1&&a.restarts==restart);
        assert(a.file.saves==0&&!strcmp(a.global_settings.values.last_book,"one.txt"));
        App cold;cold.boot();assert(cold.reader_hold.shoulder_page_turns!=startup&&cold.selected==0);
    }
    for(unsigned exit:{32u,64u,16u}) {
        files.clear();reset_fault();App a;a.boot();a.opened(reader::OpenResult::OPENED,"one.txt");
        a.enter();int w=writes;a.settings_row=0;a.tap(8);assert(writes==w);
        a.settings_row=App::GOTO_ROW;a.tap(exit);assert(writes==w+1&&a.file.saves==0&&!a.global_settings.dirty());
        a.enter();a.settings_row=0;a.tap(4);a.tap(8);a.settings_row=App::GOTO_ROW;a.tap(exit);assert(writes==w+1);
        a.enter();a.settings_row=App::GOTO_ROW;int retarget=a.retargets;a.tap(512);assert(a.goto_percent==10);a.tap(16);assert(a.retargets==retarget+1&&writes==w+1);
        a.enter();a.settings_row=0;a.tap(8);fault='w';ordinal=1;calls=0;a.tap(32);
        assert(a.global_settings.dirty()&&overlay=="Settings save failed"&&a.file.saves==0);
        reset_fault();a.enter();a.tap(32);assert(!a.global_settings.dirty()&&a.file.saves==0);
        a.enter();a.settings_row=App::STARTUP_ROW;a.tap(8);fault='w';ordinal=1;calls=0;a.tap(32);assert(a.global_settings.dirty());
        reset_fault();a.file.save_ok=false;a.tap(64);assert(!a.global_settings.dirty()&&overlay=="Position save failed");
        a.global_settings.values.line_spacing=0;fault='w';ordinal=1;calls=0;a.tap(64);assert(overlay=="Both saves failed"&&a.global_settings.dirty());
        reset_fault();a.file.save_ok=true;fault='w';ordinal=1;calls=0;a.tap(64);assert(overlay=="Settings save failed"&&a.global_settings.dirty());
        reset_fault();a.tap(64);assert(overlay=="Saved"&&!a.global_settings.dirty());
    }
    files.clear();reset_fault();App a;a.boot();fault='w';ordinal=1;
    a.opened(reader::OpenResult::OPENED,"two.epub");assert(a.global_settings.dirty()&&overlay=="Settings save failed"&&a.file.saves==0);
    reset_fault();a.opened(reader::OpenResult::OPENED,"two.epub");assert(!a.global_settings.dirty()&&a.file.saves==0);
    a.enter();a.settings_row=0;draw.clear();a.settings_draw();
    auto has=[](int palette,int y,const char* prefix){for(auto& d:draw)if(std::get<0>(d)==palette&&std::get<2>(d)==y&&std::get<3>(d).find(prefix)==0)return true;return false;};
    assert(has(1,-40,"Line spacing:")&&has(1,-24,"Paragraph gap:")&&has(2,-8,"Lines per page:")&&has(1,8,"L/R on startup:")&&has(1,24,"Go to:")&&has(2,40,"Page ")&&has(2,64,"LEFT/RIGHT"));
    for(int row=0;row<4;++row){a.settings_row=row;draw.clear();a.settings_draw();int ys[]={-40,-24,8,24};assert(has(3,ys[row],">"));}
    a.settings_row=0;for(int i=0;i<6;++i)a.tap(2);assert(a.settings_row==3);for(int i=0;i<6;++i)a.tap(1);assert(a.settings_row==0);
    puts("PASS: extracted main boot, successful/failed open, all Settings exits, independent save/retry, session mode, palette/coordinates/cursor and unchanged Go to");
}
