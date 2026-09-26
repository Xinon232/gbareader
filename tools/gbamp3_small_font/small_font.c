/* 5x7 small font from gbamp3 v0.9.8 (github.com/Xinon232/gbamp3, player/src/text.c),
 * copied verbatim: glyph tables and small_bits(). Used only by
 * tools/make_ui_small_font.py to generate graphics/ui_small_font.bmp. */
#include <stdint.h>
#define SMALL_HEIGHT 7
#define SMALL_DESCENT 2   /* g j p q y reach two rows below the baseline */
#define SMALL_ADVANCE 6
/* ---- 5x7 small font (original gbamp3 glyphs; lowercase as in gbavocab v2.0) ---- */
static const unsigned char upper5[36][7]={{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
static const unsigned char lower5[26][7]={{0,0,14,1,15,17,15},{16,16,30,17,17,17,30},{0,0,14,17,16,17,14},{1,1,15,17,17,17,15},{0,0,14,17,31,16,14},{6,8,8,28,8,8,8},{0,15,17,17,15,1,14},{16,16,30,17,17,17,17},{4,0,12,4,4,4,14},{2,0,6,2,2,18,12},{16,16,18,20,24,20,18},{12,4,4,4,4,4,14},{0,0,26,21,21,21,21},
 {0,0,30,17,17,17,17},{0,0,14,17,17,17,14},{0,0,30,17,30,16,16},{0,0,15,17,15,1,1},{0,0,22,25,16,16,16},{0,0,15,16,14,1,30},{8,8,28,8,8,9,6},{0,0,17,17,17,19,13},{0,0,17,17,17,10,4},{0,0,17,17,21,21,10},{0,0,17,10,4,10,17},{0,0,17,17,15,1,14},{0,0,31,2,4,8,31}};
/* g j p q y: two extra rows below the baseline (rows 7-8), bowls on the x-height. */
static const unsigned char descend5[5][9]={{0,0,15,17,17,17,15,1,14},{2,0,6,2,2,2,2,18,12},{0,0,30,17,17,17,30,16,16},{0,0,15,17,17,17,15,1,1},{0,0,17,17,17,17,15,1,14}};
static unsigned small_bits(uint32_t c,int j){
 {const char *d="gjpqy";for(int k=0;k<5;k++)if(c==(uint32_t)d[k])return descend5[k][j];}
 if(j>=7)return 0;
 if(c>='a'&&c<='z')return lower5[c-'a'][j];
 if(c>='A'&&c<='Z')return upper5[c-'A'][j];
 if(c>='0'&&c<='9')return upper5[26+c-'0'][j];
 switch(c){
  case '-':return j==3?14:0;case '.':return j==6?4:0;case ',':return j==5?4:j==6?8:0;
  case ':':return (j==2||j==5)?4:0;case '/':return 1u<<(j<5?j:4);case '(':return (j==0||j==6)?2:4;
  case ')':return (j==0||j==6)?8:4;case '_':return j==6?31:0;case '\'':return j<2?4:0;
  case '!':return j==5?0:4;case '?':{static const unsigned char q[7]={14,17,1,2,4,0,4};return q[j];}
  case '&':{static const unsigned char a[7]={12,18,20,8,21,18,13};return a[j];}
  case '+':return j==3?31:(j>=1&&j<=5)?4:0;case '#':return (j==1||j==5)?31:10;
  case '"':return j<2?10:0;case '@':{static const unsigned char at[7]={14,17,23,21,23,16,14};return at[j];}
  case '%':{static const unsigned char pc[7]={24,25,2,4,8,19,3};return pc[j];}
  case '>':{static const unsigned char gt[7]={8,4,2,1,2,4,8};return gt[j];}
  case '*':{static const unsigned char st[7]={0,4,21,14,21,4,0};return st[j];}
  case '=':return (j==2||j==4)?31:0;case ';':return j==2||j==5?4:j==6?8:0;
  default:return 0;
 }
}
#include <stdio.h>
int main(void){
 for(uint32_t c=33;c<127;c++){printf("%u",c);for(int j=0;j<SMALL_HEIGHT+SMALL_DESCENT;j++)printf(" %u",small_bits(c,j));printf("\n");}
 return 0;
}
