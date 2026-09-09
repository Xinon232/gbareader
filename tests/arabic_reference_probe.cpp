#include "arabic_text.h"
#include <iostream>
#include <string>
int main(){std::string s;while(std::getline(std::cin,s)){const auto& a=arabic::shape(s.c_str(),[](const char*){return 8;});
 for(int i=0;i<a.count;++i){auto& t=a.items[a.order[i]];if(t.glyph)std::cout<<arabic::glyphs[t.glyph].source_id<<' ';}
 std::cout<<'\n';}}
