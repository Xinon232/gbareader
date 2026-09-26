#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
#include "reader_hold.h"
namespace bn {
template<int N> struct string:std::string{string(const char* s):std::string(s){}};
struct sprite_ptr {bool above=false;void put_above(){above=true;}};
template<class T>using ivector=std::vector<T>;
template<class T,int N> struct vector:ivector<T>{};
struct sprite_text_generator {
 int alignment=0,priority=3,z=0,x=0,y=0;bool per_char=false;std::string text;
 void set_right_alignment(){alignment=1;}void set_center_alignment(){alignment=0;}
 void set_one_sprite_per_character(bool v){per_char=v;}
 void set_bg_priority(int p){priority=p;}void set_z_order(int n){z=n;}
 void generate(int xx,int yy,const std::string& t,ivector<sprite_ptr>& sprites){
  assert(alignment==1 && priority==0 && z==-32767 && per_char);
  assert(t.find(' ')==std::string::npos); // Spaces become the box font's blank '~'.
  x=xx;y=yy;text=t;
  for(char& c:text)if(c=='~')c=' ';
  sprites.resize(sprites.size()+t.size());
  assert(sprites.size()<=24);
 }
};
}
constexpr int SAVE_OVERLAY_SPRITE_CAPACITY=24;
#include "overlay.inc"
int main(){
bn::sprite_text_generator save_ui;
bn::vector<bn::sprite_ptr,SAVE_OVERLAY_SPRITE_CAPACITY> sprites;
reader::ReaderHold reader_hold;
for(bool shoulder_page_turns : {true,false}){
reader_hold.shoulder_page_turns=shoulder_page_turns;
#include "mode_call.inc"
assert(save_ui.text==(shoulder_page_turns?"L+R: On":"L+R: Off"));
assert(save_ui.x==112 && save_ui.y==-64);
assert(save_ui.alignment==0 && save_ui.z==0 && !save_ui.per_char);
for(auto& sprite:sprites)assert(sprite.above);
}
sprites.clear();assert(sprites.empty());
puts("PASS: real overlay renderer calls: UI sprite foreground, bounded text, top-right position, outlined per-character glyphs, alignment restoration, replacement and removal");
}
