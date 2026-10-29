/* Prints the gbamp3 5x7 small font (include/ui_small_glyphs.h) for
 * tools/make_ui_small_font.py, which generates graphics/ui_small_font.bmp. */
#include "../../include/ui_small_glyphs.h"
#include <stdio.h>
int main(void){
 for(uint32_t c=33;c<127;c++){printf("%u",c);for(int j=0;j<SMALL_HEIGHT+SMALL_DESCENT;j++)printf(" %u",small_bits(c,j));printf("\n");}
 return 0;
}
