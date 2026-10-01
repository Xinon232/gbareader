// Production boot, open-success and Settings code from main.cpp, compiled
// against the FatFS mock with keypad / screen doubles.
#include "global_fatfs_mock.h"
#include "reader_ui_state.h"
#include "reader_hold.h"
#include "reader_menu.h"
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
struct timer {int elapsed_ticks() const {return 1 << 30;}};
}
namespace reader {
static std::vector<std::string> names={"one.txt","two.epub","three.txt"};
bool storage_init(){return true;}int library_count(){return names.size();}
const char* library_name(int i){return names.at(i).c_str();}
constexpr int BROWSE_PATH_MAX=512;
}
// Screen double: records what Settings draws.
using Row=std::tuple<int,std::string,bool,std::string>;
static std::vector<Row> rows;static std::string title;
namespace screen {
constexpr int ROWS=8;
void header(uint8_t*,const char* t,const char* =nullptr){title=t;}
void row(uint8_t*,int slot,const char* text,bool selected,bool=false,const char* info=nullptr,unsigned=0)
{rows.emplace_back(slot,text,selected,info?info:"");}
}
using reader::Scene;
constexpr int BACKGROUND_WORK_TICKS=1,IMPORT_DEPTH=16,MESSAGE_FRAMES=120;
#include "reader_sampler.inc"
#include "list_keys.inc"
struct Sprites {void clear(){}};
static std::string overlay;
void show_overlay(int,Sprites&,const char* text,int=64){overlay=text;}
struct Source:reader::ByteSource {
    int saves=0;bool save_ok=true;bool open=true;
    uint32_t size()const override{return 12000;}
    bool byte_at(uint32_t at,unsigned char& c)const override{c=at%40==39?'\n':'a';return at<size();}
    bool save_footer(const reader::TxtSaveFooter&,const reader::ByteSource*){++saves;return save_ok;}
    void close(){open=false;}
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
    bool pending_back=false,redraw_ui=false,redraw_page=false,book_open=true;
    int page_turns=0,goto_percent=0,goto_before=0,save_ui=0,about_page=0,count_refresh_frames=0;
    int restarts=0,retargets=0;
    const char* open_name="one.txt";
    const char* message=nullptr;int message_frames=0;
    uint32_t saved_offset=0;bool confirm_leave=false;
    reader::ListNav home{},settings_nav{};
    reader::SettingsItem settings_items[reader::SETTINGS_MAX_ROWS];int settings_count=0;
    ListKeys list_keys;reader::KeyRepeat goto_left,goto_right;reader::Marquee marquee;
    Sprites sprites,save_sprites;
    App(){settings=reader::default_settings();settings_before=settings;reader::layout_page(file,1000,settings,nullptr,page);}
#include "settings_label.inc"
    void boot(){
#include "global_boot.inc"
        (void)settings_nav;(void)settings_items;(void)settings_count;(void)goto_percent;(void)goto_before;
        (void)count_refresh_frames;(void)about_page;(void)redraw_ui;(void)redraw_page;(void)open_name;
        (void)active_source;(void)message_frames;(void)save_message_timer;(void)pending_back;(void)list_keys;
        (void)goto_left;(void)goto_right;(void)marquee;(void)frame;(void)import_nav;(void)import_depth;
        (void)import_ok;(void)saved_offset;(void)confirm_leave;(void)confirm_nav;(void)import_source;(void)import_name;(void)storage_ok;
        this->home=home;this->reader_hold=reader_hold;this->scene=scene;this->message=message;
        this->settings_before=settings_before;
    }
    void flash(const char* text){message=text;message_frames=MESSAGE_FRAMES;redraw_ui=true;}
    void open_settings(bool open){
        settings_count=reader::settings_items(open,settings_items);settings_nav={};confirm_leave=false;
        if(!open){settings.line_spacing=global_settings.values.line_spacing;settings.paragraph_gap=global_settings.values.paragraph_gap;}
        settings_before=settings;list_keys.reset();scene=Scene::SETTINGS;redraw_ui=true;
    }
    void restart_page_count(){++restarts;}
    void retarget_page_count(){++retargets;}
    void opened(reader::OpenResult opened,const char* name){open_name=name;
#include "global_open_success.inc"
    }
    void frame(unsigned keys){bn::keypad::pressed=keys&~bn::keypad::held;bn::keypad::held=keys;
        book_open=open_name!=nullptr;
        if(scene==Scene::SETTINGS){
#include "global_settings_input.inc"
        }
    }
    void tap(unsigned keys){frame(0);frame(keys);frame(0);}
    void settings_draw(){rows.clear();uint8_t* pixels=nullptr;
#include "global_settings_draw.inc"
    }
    // Reader Select: the production entry path.
    void enter(){goto_before=goto_percent;book_open=true;open_name="one.txt";file.open=true;open_settings(true);}
    void row_to(reader::SettingsItem item){for(int i=0;i<settings_count;++i)if(settings_items[i]==item){settings_nav.selected=i;return;}assert(false);}
};
enum {UP=1,DOWN=2,LEFT=4,RIGHT=8,A=16,B=32,START=64,SELECT=128,L=256,R=512};
int main(){
    using reader::SettingsItem;
    // Boot: the last L/R choice and the last opened book come back.
    for(bool turns:{false,true}){
        files.clear();reset_fault();GlobalPreferences p{};p.shoulder_page_turns=turns;remember_global_book(p,"two.epub");files[paths[0]]=record(p,1);
        App a;a.boot();assert(a.scene==Scene::LIBRARY&&a.home.selected==1&&a.reader_hold.shoulder_page_turns==turns&&!a.message);
        a.opened(reader::OpenResult::FAILED,"three.txt");assert(!strcmp(a.global_settings.values.last_book,"two.epub"));
        writes=0;a.opened(reader::OpenResult::OPENED,"two.epub");assert(writes==0);
        a.opened(reader::OpenResult::OPENED,"three.txt");assert(writes==1&&a.file.saves==0);
        // Settings while reading: L/R page turns is a row; closing saves it.
        a.enter();a.row_to(SettingsItem::PAGE_TURN_KEYS);int before=writes,restarts=a.restarts;
        a.tap(A);assert(a.reader_hold.shoulder_page_turns!=turns&&writes==before);
        a.tap(B);assert(a.scene==Scene::READER&&writes==before+1&&a.restarts==restarts&&a.retargets==0);
        assert(a.global_settings.values.shoulder_page_turns!=turns&&a.file.saves==0);
        App cold;cold.boot();assert(cold.reader_hold.shoulder_page_turns!=turns&&cold.home.selected==2);
    }
    // Settings rows, as drawn: Back to Files, Go to (page info), spacing (lines info), gap, L/R, About.
    files.clear();reset_fault();
    {App a;a.boot();a.opened(reader::OpenResult::OPENED,"one.txt");a.enter();a.goto_percent=a.goto_before=0;a.settings_draw();
     assert(title=="Settings"&&rows.size()==7);
     assert(std::get<1>(rows[0])=="Back to Files"&&std::get<2>(rows[0])&&std::get<3>(rows[0]).empty());
     assert(std::get<1>(rows[1])=="Go to: 0%"&&std::get<3>(rows[1]).find("Page ")==0);
     assert(std::get<1>(rows[2])=="Line spacing: 1"&&std::get<3>(rows[2])==std::to_string(reader::lines_per_page(a.settings))+" lines");
     assert(std::get<1>(rows[3])=="Paragraph gap: Full"&&std::get<3>(rows[3]).empty());
     assert(std::get<1>(rows[4])=="L/R page turns: Off"&&std::get<1>(rows[5])=="Show file extensions: On"&&
            std::get<1>(rows[6])=="About");
     for(int i=1;i<7;++i)assert(!std::get<2>(rows[i]));
     // Up / Down move the cursor; a fresh press wraps around.
     a.tap(UP);assert(a.settings_nav.selected==6);a.tap(DOWN);assert(a.settings_nav.selected==0);
     for(int i=0;i<3;++i){a.tap(DOWN);}
     a.settings_draw();assert(std::get<2>(rows[3]));
     // Holding Down repeats and stops at the last row.
     a.frame(0);for(int i=0;i<200;++i)a.frame(DOWN);a.frame(0);assert(a.settings_nav.selected==6);}
    // Show file extensions: A flips, Left / Right set Off / On; closing saves it once.
    {files.clear();reset_fault();App a;a.boot();a.opened(reader::OpenResult::OPENED,"one.txt");
     a.enter();a.row_to(SettingsItem::FILE_EXTENSIONS);int w=writes,r0=a.restarts;
     a.tap(A);assert(!a.global_settings.values.show_extensions);
     a.settings_draw();assert(std::get<1>(rows[5])=="Show file extensions: Off");
     a.tap(RIGHT);assert(a.global_settings.values.show_extensions);a.tap(LEFT);a.tap(LEFT);
     assert(!a.global_settings.values.show_extensions);
     a.tap(B);assert(a.scene==Scene::READER&&writes==w+1&&a.restarts==r0);
     App cold;cold.boot();assert(!cold.global_settings.values.show_extensions);}
    // Go to: Left / Right 1% (held: repeats), L / R 10%; A jumps now; B applies too.
    for(unsigned exit:{unsigned(B),unsigned(START),unsigned(SELECT),unsigned(A)}){
        files.clear();reset_fault();App a;a.boot();a.opened(reader::OpenResult::OPENED,"one.txt");
        a.enter();a.row_to(SettingsItem::GOTO);int w=writes;a.tap(RIGHT);a.tap(RIGHT);assert(a.goto_percent==2);
        a.frame(0);for(int i=0;i<1+24+8*3;++i)a.frame(RIGHT);a.frame(0);assert(a.goto_percent==2+1+4);
        a.tap(L);assert(a.goto_percent==0);a.tap(R);a.tap(R);assert(a.goto_percent==20);
        int retarget=a.retargets,restart=a.restarts;a.tap(exit);
        assert(a.scene==Scene::READER&&a.retargets==retarget+1&&a.restarts==restart&&writes==w&&a.file.saves==0);
    }
    // Spacing / gap: Left / Right step, A steps and wraps; a change relayouts once and saves once.
    {files.clear();reset_fault();App a;a.boot();a.opened(reader::OpenResult::OPENED,"one.txt");
     a.enter();a.row_to(SettingsItem::LINE_SPACING);int w=writes,r0=a.restarts;
     a.tap(A);a.tap(A);a.tap(A);assert(a.settings.line_spacing==4);a.tap(A);assert(a.settings.line_spacing==0);
     a.tap(RIGHT);assert(a.settings.line_spacing==1);a.tap(LEFT);a.tap(LEFT);assert(a.settings.line_spacing==0);
     a.row_to(SettingsItem::PARAGRAPH_GAP);a.tap(A);assert(a.settings.paragraph_gap==reader::ParagraphGap::NONE);
     a.tap(RIGHT);assert(a.settings.paragraph_gap==reader::ParagraphGap::SMALL);
     a.tap(B);assert(a.restarts==r0+1&&writes==w+1&&a.global_settings.values.line_spacing==0&&
                     a.global_settings.values.paragraph_gap==reader::ParagraphGap::SMALL);
     // Unchanged values close without a write or relayout.
     a.enter();a.tap(B);assert(writes==w+1&&a.restarts==r0+1);
     // A failed save keeps the values pending and says so over the page.
     a.enter();a.row_to(SettingsItem::LINE_SPACING);a.tap(RIGHT);fault='w';ordinal=1;calls=0;a.tap(B);
     assert(a.scene==Scene::READER&&a.global_settings.dirty()&&overlay=="Settings save failed");
     reset_fault();a.enter();a.tap(B);assert(!a.global_settings.dirty());}
    // Back to Files on the saved page closes the book at once, keeps the settings and returns Home.
    {files.clear();reset_fault();App a;a.boot();a.opened(reader::OpenResult::OPENED,"one.txt");a.saved_offset=a.page.start_offset;
     a.enter();a.row_to(SettingsItem::LINE_SPACING);a.tap(RIGHT);a.row_to(SettingsItem::BACK_TO_FILES);int w=writes,r0=a.restarts;
     a.tap(A);assert(a.scene==Scene::LIBRARY&&!a.open_name&&!a.file.open&&!a.epub.open&&a.file.saves==0);
     assert(writes==w+1&&a.global_settings.values.line_spacing==2&&a.restarts==r0);
     // About opens from its row; B returns to Settings (main's ABOUT branch).
     a.enter();a.row_to(SettingsItem::ABOUT);a.tap(A);assert(a.scene==Scene::ABOUT&&a.about_page==0);}
    // Not on the saved page: the row asks "Continue without saving?"; B takes it back, A leaves.
    {files.clear();reset_fault();App a;a.boot();a.opened(reader::OpenResult::OPENED,"one.txt");a.saved_offset=a.page.start_offset+1;
     a.enter();assert(a.settings_items[a.settings_nav.selected]==SettingsItem::BACK_TO_FILES);
     a.tap(A);assert(a.scene==Scene::SETTINGS&&a.confirm_leave&&a.open_name);
     a.settings_draw();assert(std::get<1>(rows[0])=="Continue without saving?"&&std::get<2>(rows[0]));
     a.tap(B);assert(a.scene==Scene::SETTINGS&&!a.confirm_leave);a.settings_draw();assert(std::get<1>(rows[0])=="Back to Files");
     a.tap(A);assert(a.confirm_leave);a.tap(DOWN);assert(!a.confirm_leave&&a.scene==Scene::SETTINGS); // Moving away cancels too.
     a.tap(UP);a.tap(A);assert(a.confirm_leave);a.tap(A);
     assert(a.scene==Scene::LIBRARY&&!a.open_name&&!a.file.open&&a.file.saves==0);
     // Settings opens without the question.
     a.open_name="one.txt";a.enter();assert(!a.confirm_leave);}
    // Settings from Home: no Go to / Back to Files; values come from (and go to) the saved globals.
    {files.clear();reset_fault();GlobalPreferences p{};p.line_spacing=3;files[paths[0]]=record(p,1);
     App a;a.boot();a.open_name=nullptr;a.settings.line_spacing=1;a.open_settings(false);
     assert(a.settings_count==5&&a.settings_items[0]==SettingsItem::LINE_SPACING&&a.settings.line_spacing==3);
     a.settings_draw();assert(rows.size()==5&&std::get<1>(rows[0])=="Line spacing: 3");
     a.tap(RIGHT);int w=writes;a.row_to(SettingsItem::PAGE_TURN_KEYS);a.tap(RIGHT);assert(a.reader_hold.shoulder_page_turns);
     a.tap(SELECT);assert(a.scene==Scene::LIBRARY&&writes==w+1&&a.retargets==0);
     assert(a.global_settings.values.line_spacing==4&&a.global_settings.values.shoulder_page_turns);
     a.open_settings(false);a.tap(LEFT);fault='w';ordinal=1;calls=0;a.tap(B);
     assert(a.scene==Scene::LIBRARY&&a.message&&!strcmp(a.message,"Settings save failed"));}
    // Boot messages.
    {files.clear();reset_fault();files[paths[0]]={1,2,3};files[paths[1]]={4,5,6};App a;a.boot();
     assert(a.message&&!strcmp(a.message,"Settings load failed"));}
    // An open failure of the global save shows its overlay; the retry succeeds.
    {files.clear();reset_fault();App a;a.boot();fault='w';ordinal=1;
     a.opened(reader::OpenResult::OPENED,"two.epub");assert(a.global_settings.dirty()&&overlay=="Settings save failed"&&a.file.saves==0);
     reset_fault();a.opened(reader::OpenResult::OPENED,"two.epub");assert(!a.global_settings.dirty()&&a.file.saves==0);}
    puts("PASS: extracted main boot (last book, last L/R), open saves, Settings rows/draw/wrap/repeat, Go to, A wraps values, Back to Files, About, Settings from Home, failed saves");
}
