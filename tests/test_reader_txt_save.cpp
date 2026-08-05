#include "reader_txt_save.h"
#include <cassert>
#include <cstdio>
using namespace reader;
int main(){ TxtSaveFooter in{123456,{1,1,1}}; unsigned char bytes[TXT_SAVE_FOOTER_SIZE]{}; make_txt_save_footer(in,bytes); TxtSaveFooter out{}; assert(looks_like_txt_save_footer(bytes)); assert(parse_txt_save_footer(bytes,out)); assert(out.byte_offset==123456 && out.settings.line_spacing==1 && out.settings.top_margin==1 && out.settings.bottom_margin==1); bytes[42]^=1; assert(!parse_txt_save_footer(bytes,out)); std::puts("PASS: TXT save footer"); }
