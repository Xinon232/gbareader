#include "reader_global_settings.h"
#include "reader_crc32.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>
#include "reader_ui_state.h"
using namespace reader;
static void seal(unsigned char* b) { auto c=crc32_bytes(b,28); for(int i=0;i<4;++i)b[28+i]=c>>(8*i); }
int main() {
    auto p=default_global_preferences();
    assert(p.line_spacing==1 && p.paragraph_gap==ParagraphGap::FULL && !p.shoulder_startup);
    unsigned char b[33]{}; GlobalPreferences out{}; uint32_t gen=0;
    for(int s=0;s<=4;++s)for(int g=0;g<4;++g)for(bool on:{false,true}) {
        p={uint8_t(s),ParagraphGap(g),on};
        assert(encode_global_settings(p,42,b));
        assert(!memcmp(b,"GBARCFG1",8) && b[8]==2 && b[12]==42);
        assert(decode_global_settings(b,32,out,gen) && gen==42 && same_global_preferences(p,out));
        for(int n=0;n<33;++n)if(n!=32)assert(!decode_global_settings(b,n,out,gen));
        for(int i=0;i<32;++i) {b[i]^=1;assert(!decode_global_settings(b,32,out,gen));b[i]^=1;}
    }
    assert(!encode_global_settings(p,0,b));
    p.line_spacing=5;assert(!encode_global_settings(p,1,b));p.line_spacing=1;
    p.paragraph_gap=ParagraphGap(4);assert(!encode_global_settings(p,1,b));p.paragraph_gap=ParagraphGap::FULL;
    for(int index:{8,12,16,17,18,19,27}) {
        assert(encode_global_settings(p,1,b));
        b[index]=index==12?0:255;seal(b);assert(!decode_global_settings(b,32,out,gen));
    }
    assert(encode_global_settings(p,0xffffffffu,b));assert(decode_global_settings(b,32,out,gen)&&gen==0xffffffffu);
    // V2 remembers the whole filename, not a list index or a lossy hash.
    unsigned char record[GLOBAL_SETTINGS_MAX_BYTES]{};
    GlobalPreferences named{};
    std::string name(240,'x'); name += u8"é.txt";
    assert(remember_global_book(named,name.c_str()));
    const auto n=global_settings_size(named);
    assert(n==32+name.size() && encode_global_settings(named,9,record));
    assert(record[8]==2 && decode_global_settings(record,n,out,gen));
    assert(!strcmp(out.last_book,name.c_str()));
    assert(!remember_global_book(named,std::string(256,'x').c_str()));
    assert(!remember_global_book(named,"bad/path.txt"));
    assert(!strcmp(named.last_book,name.c_str()));
    record[19]--; assert(!decode_global_settings(record,n,out,gen));
    // Previous 32-byte v1 stays readable and never invents a remembered book.
    assert(encode_global_settings(p,7,b)); b[8]=1;seal(b);
    assert(decode_global_settings(b,32,out,gen)&&!out.last_book[0]);
    std::vector<std::string> names={"Same.txt","Same.epub",u8"Thé.txt"};
    static std::vector<std::string>* list=&names;
    auto lookup=[](int i)->const char*{return (*list)[i].c_str();};
    assert(remembered_library_selection("Same.epub",3,lookup)==1);
    names.insert(names.begin(),"Added.txt");assert(remembered_library_selection("Same.epub",4,lookup)==2);
    std::swap(names[0],names[2]);assert(remembered_library_selection("Same.epub",4,lookup)==0);
    assert(remembered_library_selection("removed.txt",4,lookup)==0);
    assert(remembered_library_selection("",4,lookup)==0);
    assert(remembered_library_selection("Same.epub",0,lookup)==0);
    assert(remembered_library_selection("Same.epub",3,[](int)->const char*{return nullptr;})==0);
    names.resize(130,"other.txt");names[129]=name;assert(remembered_library_selection(name.c_str(),130,lookup)==129);
    assert(library_first_row(129,130)<=129&&library_first_row(129,130)+LIBRARY_VISIBLE_ROWS>129);
    assert(!strcmp(save_status_message(true,true),"Saved"));
    assert(!strcmp(save_status_message(false,true),"Position save failed"));
    assert(!strcmp(save_status_message(true,false),"Settings save failed"));
    assert(!strcmp(save_status_message(false,false),"Both saves failed"));
    puts("PASS: global codec defaults, values, CRC, strict fields, v1 migration and full v2 filename");
}
