#pragma once
#include "ghoulam_data.h"
#include <cstring>
// Bounded display-only Arabic. One shared, non-reentrant EWRAM scratch line.
// Source offsets are logical UTF-8 bytes; no document/index/persistence writes.
namespace arabic {
constexpr int capacity=256; // Reader lines hold at most 255 UTF-8 bytes.
inline bool script(unsigned c) {return (c>=0x600&&c<=0x6ff)||(c>=0x750&&c<=0x77f)||(c>=0x870&&c<=0x8ff)||c==0x200c||c==0x200d;}
inline bool mark(unsigned c) {return (c>=0x610&&c<=0x61a)||(c>=0x64b&&c<=0x65f)||c==0x670||(c>=0x6d6&&c<=0x6dc)||(c>=0x6df&&c<=0x6e4)||(c>=0x6e7&&c<=0x6e8)||(c>=0x6ea&&c<=0x6ed)||(c>=0x898&&c<=0x89f)||(c>=0x8ca&&c<=0x8e1)||(c>=0x8e3&&c<=0x8ff);}
inline unsigned decode(const char* s,int n,int& p) {
 unsigned c=static_cast<unsigned char>(s[p++]);if(c<128)return c;
 int extra=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;
 if(!extra||p+extra>n)return '?';
 unsigned v=c&((1u<<(6-extra))-1);
 for(int j=0;j<extra;++j){unsigned b=static_cast<unsigned char>(s[p+j]);if((b&0xc0)!=0x80)return '?';v=(v<<6)|(b&63);}
 if(v<(extra==1?0x80u:extra==2?0x800u:0x10000u)||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return '?';
 p+=extra;return v;
}
inline void encode(unsigned c,char s[5]) {
 int n=0;if(c<128)s[n++]=char(c);
 else if(c<0x800){s[n++]=char(0xc0|(c>>6));s[n++]=char(0x80|(c&63));}
 else if(c<0x10000){s[n++]=char(0xe0|(c>>12));s[n++]=char(0x80|((c>>6)&63));s[n++]=char(0x80|(c&63));}
 else{s[n++]=char(0xf0|(c>>18));s[n++]=char(0x80|((c>>12)&63));s[n++]=char(0x80|((c>>6)&63));s[n++]=char(0x80|(c&63));}s[n]=0;
}
inline bool contains(const char* s){int n=int(std::strlen(s));for(int p=0;p<n;)if(script(decode(s,n,p)))return true;return false;}
inline const Letter* letter(unsigned c){for(const auto& l:letters)if(l.code==c)return &l;return nullptr;}
struct Item {unsigned code;uint16_t begin,end,glyph;int16_t x;uint8_t advance,level;};
struct Line {Item items[capacity];uint16_t order[capacity];int count,width;bool valid;};
#ifdef __DEVKITARM__
extern Line scratch;
#else
inline Line scratch;
#endif
inline const Letter* joining(unsigned c){static constexpr Letter causing={0,{0,0,0,0},1,1};return c==0x200d?&causing:letter(c);}
inline int direction(unsigned c) {
 if((c>='0'&&c<='9')||(c>=0x660&&c<=0x669))return 2;
 if(script(c)&&!mark(c)&&c!=0x60c&&c!=0x61b&&c!=0x61f&&c!=0x200c&&c!=0x200d)return 1;
 if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>=0x80&&!script(c)&&c!=0x200c&&c!=0x200d))return 0;
 return -1;
}
inline unsigned mirror(unsigned c){switch(c){case '(':return ')';case ')':return '(';case '[':return ']';case ']':return '[';case '{':return '}';case '}':return '{';default:return c;}}
// Measure receives only the non-Arabic fallback character, never shaped bytes.
template<class Measure> const Line& shape(const char* s,Measure measure,int length=-1) {
 Line& a=scratch;a.count=0;a.width=0;a.valid=true;
 int n=length<0?int(std::strlen(s)):length;
 if(n>=576){a.valid=false;return a;}
 for(int p=0;p<n;){int begin=p;unsigned c=decode(s,n,p);
  if(mark(c)){if(a.count)a.items[a.count-1].end=uint16_t(p);continue;}
  if(a.count==capacity){a.valid=false;return a;}
  auto& t=a.items[a.count++];t={c,uint16_t(begin),uint16_t(p),0,0,0,0};
 }
 int base=0;for(int i=0;i<a.count;++i){int d=direction(a.items[i].code);if(d==0||d==1){base=d;break;}}
 // Required ligatures are actual rlig substitutions after init/medi+fina.
 for(int i=0;i+1<a.count;++i)if(a.items[i].code==0x644){
  for(const auto& lig:ligatures)if(a.items[i+1].code==lig.code){
   const auto* prev=i?joining(a.items[i-1].code):nullptr;
   bool joined=prev&&prev->next&&!(i&&a.items[i-1].glyph>=ligatures[0].form[0]);
   a.items[i].glyph=lig.form[joined?1:0];a.items[i].end=a.items[i+1].end;
   for(int j=i+1;j+1<a.count;++j)a.items[j]=a.items[j+1];
   --a.count;break;
  }
 }
 for(int i=0;i<a.count;++i){auto& t=a.items[i];int d=direction(t.code);t.level=uint8_t(d<0?3:d);
  const auto* l=letter(t.code);
  if(t.glyph)t.advance=glyphs[t.glyph].advance;
  else if(l){const auto* prev=i?joining(a.items[i-1].code):nullptr;const auto* next=i+1<a.count?joining(a.items[i+1].code):nullptr;
   bool before=prev&&prev->next&&l->prev;
   // A preceding lam-alef cannot connect to the next character.
   if(i&&a.items[i-1].code==0x644&&a.items[i-1].end-a.items[i-1].begin>=4&&a.items[i-1].glyph>=ligatures[0].form[0])before=false;
   bool after=next&&l->next&&next->prev;
   t.glyph=l->form[(before?1:0)|(after?2:0)];t.advance=glyphs[t.glyph].advance;
  }else if(t.code==0x200c||t.code==0x200d){t.advance=0;}
  else{unsigned c=t.code;if(c==0x60c)c=',';else if(c>=0x660&&c<=0x669)c='0'+c-0x660;else if(script(c))c='?';char ch[5];encode(c,ch);int w=measure(ch);t.advance=uint8_t(w>0&&w<=32?w:0);t.code=c;}
  a.width+=t.advance;a.order[i]=uint16_t(i);
 }
 // Paired brackets take their enclosed first strong direction. This keeps
 // brace transliteration/digits as an LTR island in an Arabic paragraph.
 for(int i=0;i<a.count;++i)if(a.items[i].code=='('||a.items[i].code=='['||a.items[i].code=='{'){
  unsigned close=mirror(a.items[i].code);int depth=0;
  for(int j=i+1;j<a.count;++j){if(a.items[j].code==a.items[i].code)++depth;
   if(a.items[j].code!=close)continue;
   if(depth){--depth;continue;}
   int d=base;for(int k=i+1;k<j;++k)if(a.items[k].level!=3){d=a.items[k].level==1?1:0;break;}
   a.items[i].level=a.items[j].level=uint8_t(d);break;
  }
 }
 for(int i=0;i<a.count;){if(a.items[i].level!=3){++i;continue;}int j=i;while(j<a.count&&a.items[j].level==3)++j;
  int prev=i?(a.items[i-1].level==1?1:0):base,next=j<a.count?(a.items[j].level==1?1:0):base;
  int d=prev==next?prev:base;for(;i<j;++i)a.items[i].level=uint8_t(d);
 }
 for(int i=0;i<a.count;++i){auto& t=a.items[i];if(t.level==0&&base)t.level=2;if(t.level==1)t.code=mirror(t.code);}
 // Bidi level reversal is per *already wrapped line*, never UTF-8 bytes.
 for(int level=2;level>=1;--level)for(int i=0;i<a.count;){if(a.items[a.order[i]].level<level){++i;continue;}
  int j=i;while(j<a.count&&a.items[a.order[j]].level>=level)++j;
  for(int l=i,r=j-1;l<r;++l,--r){auto tmp=a.order[l];a.order[l]=a.order[r];a.order[r]=tmp;}i=j;
 }
 int x=0;for(int i=0;i<a.count;++i){auto& t=a.items[a.order[i]];t.x=int16_t(x);x+=t.advance;}
 return a;
}
// Logical byte boundary to visual caret. Harakat share their base's edge;
// lam-alef has an interior caret without splitting its connected artwork.
inline int caret_x(const Line& line,const char* source,int byte) {
 if(!line.count)return 0;
 const Item* chosen=&line.items[line.count-1];
 for(int i=0;i<line.count;++i)if(byte<line.items[i].end){chosen=&line.items[i];break;}
 const auto& t=*chosen;int total=0,before=0;
 for(int p=t.begin;p<t.end;){int start=p;auto c=decode(source,t.end,p);if(!mark(c)){++total;if(start<byte)++before;}}
 if(byte<=t.begin)before=0;
 if(byte>=t.end)before=total;
 int d=total?t.advance*before/total:0;return t.x+(t.level==1?t.advance-d:d);
}
// Shared real compositor for full/reduced cards and bitmap entry previews.
// The Latin painter retains SuperFW artwork/palette; Arabic is foreground 1.
template<class Latin> void compose(const Line& a,int scale,uint32_t* pixels,Latin latin) {
 if(!a.valid||scale<4||scale>8)return;
 for(int i=0;i<a.count;++i){const auto& t=a.items[i];
  if(!t.glyph){if(t.advance)latin(t,scale,pixels);continue;}
  const auto& g=glyphs[t.glyph];
  for(int y=0;y<g.height;++y)for(int x=0;x<g.width;++x)if(bitmap[g.offset+y]&(1u<<x)){
   int sx=t.x+g.left+x,sy=g.top+y;if(sx<0||sy<0)continue;
   int dx=sx*scale/8,dy=sy*scale/8;if(dx>=224||dy>=16)continue;
   auto& word=pixels[dy*28+dx/8];int shift=(dx%8)*4;word=(word&~(15u<<shift))|(1u<<shift);
  }
 }
}
}
