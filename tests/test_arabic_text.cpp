#include "arabic_text.h"
#include <cassert>
#include <cstdio>
#include <cstring>
static int latin(const char* s){return *s==' '?4:8;}
int main(){
 const auto& a=arabic::shape(u8"بَبب",latin);
 assert(a.valid&&a.count==3);
 const auto* b=arabic::letter(0x628);
 assert(a.items[0].glyph==b->form[2]);
 assert(a.items[1].glyph==b->form[3]);
 assert(a.items[2].glyph==b->form[1]);
 assert(a.items[0].x>a.items[1].x&&a.items[1].x>a.items[2].x);
 assert(a.items[0].end==4);
 const auto& lam=arabic::shape(u8"بلا",latin);
 assert(lam.count==2&&lam.items[1].glyph==arabic::ligatures[0].form[1]);
 const auto& repeated_lam=arabic::shape(u8"لالا",latin);
 assert(repeated_lam.count==2&&repeated_lam.items[0].glyph==arabic::ligatures[0].form[0]&&repeated_lam.items[1].glyph==arabic::ligatures[0].form[0]);
 const auto& mixed=arabic::shape(u8"باب {baab 12} / بيت،",latin);
 assert(mixed.items[0].x>mixed.items[4].x); // transliteration left of Arabic
 assert(mixed.items[5].x<mixed.items[6].x); // Latin remains LTR
 assert(mixed.items[4].code=='{'&&mixed.items[12].code=='}');
 const auto& digits=arabic::shape(u8"باب 123 بيت",latin);
 assert(digits.items[4].x<digits.items[5].x&&digits.items[5].x<digits.items[6].x);
 const auto& separate=arabic::shape(u8"ب ب",latin);
 assert(separate.items[0].glyph==b->form[0]&&separate.items[2].glyph==b->form[0]);
 const auto& controls=arabic::shape(u8"ب‌ ب",latin);
 assert(controls.items[0].glyph==b->form[0]&&controls.items[1].advance==0);
 const auto& extended_marks=arabic::shape(u8"بࣰب",latin);
 assert(extended_marks.count==2&&extended_marks.items[0].glyph==b->form[2]&&extended_marks.items[1].glyph==b->form[1]);
 const auto& marks=arabic::shape(u8"َُ",latin);assert(marks.count==0&&marks.width==0);
 const auto& unknown=arabic::shape(u8"پ",latin);assert(unknown.count==1&&unknown.items[0].advance==8);
 const char broken[]={char(0xf0),char(0x80),0};
 const auto& malformed=arabic::shape(broken,latin);assert(malformed.count==2&&malformed.width==16);
 std::puts("PASS joining, actual lam-alef, mixed RTL/LTR, brackets, digits, controls, malformed input");
}
