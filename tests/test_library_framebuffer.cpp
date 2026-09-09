// Compile the verbatim production Home render branch with real bitmap fonts.
// Only the Butano sprite adapter and library discovery are host doubles.
#include "reader_ui_state.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>
extern "C" {
#include "font_render.h"
void* font_base_addr;
void* reader_font_base_addr;
}
using Pixels = std::array<uint16_t, 240 * 160 / 2>;
struct Painter { Pixels pixels{}; Pixels& page() { return pixels; } };
struct UI { bool left=false; void set_left_alignment() { left=true; } };
using Label = std::tuple<int,int,std::string,bool>;
using Labels = std::vector<Label>;
static void add_text(UI& ui,int x,int y,const char* text,Labels& labels) {
    labels.emplace_back(x,y,text,ui.left);
}
static std::vector<std::string> names;
namespace reader {
int library_count() { return int(names.size()); }
const char* library_name(int i) { return names.at(i).c_str(); }
}
static void render(Painter& painter,Labels& sprites,int selected,bool storage_ok,const char* library_status=nullptr) {
    UI ui;
    constexpr int LIBRARY_VISIBLE_ROWS=reader::LIBRARY_VISIBLE_ROWS;
#include "library_branch.inc"
}
static std::vector<unsigned char> load(const char* path) {
    FILE* f=std::fopen(path,"rb"); assert(f);
    std::fseek(f,0,SEEK_END); long size=std::ftell(f); assert(size>0); std::rewind(f);
    std::vector<unsigned char> bytes(size);
    assert(std::fread(bytes.data(),1,bytes.size(),f)==bytes.size()); std::fclose(f); return bytes;
}
static void text(Pixels& p,const char* s,int x,int y,int w) {
    draw_text_idx8_bus16_range(s,reinterpret_cast<uint8_t*>(p.data())+y*240+x,0,w,240,1);
}
int main(int argc,char** argv) {
    assert(argc==3); auto base=load(argv[1]),symbols=load(argv[2]);
    font_base_addr=base.data(); reader_font_base_addr=symbols.data();
    const Labels labels={{0,-68,"gbareader V1.2",false},{0,-48,"files: /gbareader",false},
        {0,56,"UP/DOWN select   A open",false},{-104,72,"Select: Controls",true},{8,72,"Start: Credits",true}};
    for(int count : {0,1,2,3,4,5,6,64}) {
        names.clear();
        for(int i=0;i<count;++i) names.push_back(std::to_string(i+1)+u8" A very long book name é 漢字 that clips at the right edge.epub");
        for(int selected=0;selected<(count?count:1);++selected) {
            Painter painter; Labels actual_labels; Pixels expected{};
            render(painter,actual_labels,selected,true);
            if(count) {
                const int first=count<=5?0:std::min(std::max(selected-1,0),count-5);
                for(int i=first;i<count && i<first+5;++i) {
                    const int y=48+(i-first)*16;
                    text(expected,i==selected?">":" ",8,y,12);
                    text(expected,names[i].c_str(),22,y,210);
                }
            } else {
                text(expected,"No TXT/EPUB files found.",8,56,224);
                text(expected,"Put TXT/EPUB in /gbareader",8,78,224);
                text(expected,"on SD root, then restart.",8,94,224);
            }
            assert(painter.pixels==expected); // Entire framebuffer, including blank margins.
            assert(actual_labels==labels);
        }
    }
    names.clear(); Painter missing; Labels missing_labels; Pixels expected{};
    render(missing,missing_labels,0,false);
    text(expected,"SD or folder unavailable.",8,56,224);
    text(expected,"Put TXT/EPUB in /gbareader",8,78,224);
    text(expected,"on SD root, then restart.",8,94,224);
    assert(missing.pixels==expected && missing_labels==labels);
    names={"one.txt"}; Painter error; Labels error_labels; expected={};
    render(error,error_labels,0,true,"Read failed"); text(expected,"Read failed",8,64,224);
    assert(error.pixels==expected && error_labels==labels);
    std::puts("PASS: production Home framebuffer; five long names; every selection; empty/missing/error; unchanged title/path and footer placements");
}
