#include "reader_txt_save.h"

#include <cstring>

namespace reader {
namespace {
constexpr unsigned char V2_MAGIC[] = "\n[GBAR-SAVE:2]\n";
constexpr unsigned char V3_MAGIC[] = "\n[GBAR-SAVE:3]\n";
constexpr int V1_OFFSET_AT = 17, V1_SPACING_AT = 30, V1_TOP_AT = 34, V1_BOTTOM_AT = 38, V1_CHECKSUM_AT = 42;
constexpr int V2_SIZE_AT = 16, V2_OFFSET_AT = 20, V2_SPACING_AT = 24, V2_TOP_AT = 25, V2_BOTTOM_AT = 26, V2_HISTORY_COUNT_AT = 27, V2_CHECKSUM_AT = 28, V2_HISTORY_AT = 32;
constexpr int V3_FIELDS = 16, V3_OFFSET = 18, V3_SPACING = 31, V3_TOP = 34, V3_BOTTOM = 37, V3_STATE = 40, V3_INITIALIZED = 43, V3_ANCHOR = 46, V3_SCAN = 59, V3_COUNT = 72, V3_HISTORY = 75, V3_CHECKSUM = 717;

uint32_t fnv(const unsigned char* b, int n, int skip, int skipn) { uint32_t h=2166136261u; for(int i=0;i<n;++i) if(i<skip||i>=skip+skipn) h=(h^b[i])*16777619u; return h; }
uint32_t get32(const unsigned char*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
void dec(unsigned char* p,uint32_t v,int n){for(int i=n-1;i>=0;--i){p[i]=static_cast<unsigned char>('0'+v%10);v/=10;}}
bool readdec(const unsigned char*p,int n,uint32_t&v){v=0;for(int i=0;i<n;++i){if(p[i]<'0'||p[i]>'9'||v>(0xFFFFFFFFu-uint32_t(p[i]-'0'))/10)return false;v=v*10+uint32_t(p[i]-'0');}return true;}
void hex(unsigned char*p,uint32_t v){static const char d[]="0123456789ABCDEF";for(int i=7;i>=0;--i){p[i]=d[v&15];v>>=4;}}
bool readhex(const unsigned char*p,uint32_t&v){v=0;for(int i=0;i<8;++i){unsigned char c=p[i];uint32_t d=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:16;if(d>15)return false;v=(v<<4)|d;}return true;}
bool v1(const unsigned char*p){return !std::memcmp(p,"\n[GBAR-SAVE:",12);}
bool parse_v1(const unsigned char*p,TxtSaveFooter&f){uint32_t o,c;if(!v1(p)||p[95]!='\n'||std::memcmp(p+1,"[GBAR-SAVE:1;O=",15)||p[27]!=';'||p[28]!='S'||p[29]!='='||p[31]!=';'||p[32]!='T'||p[33]!='='||p[35]!=';'||p[36]!='B'||p[37]!='='||p[39]!=';'||p[40]!='C'||p[41]!='='||!readdec(p+V1_OFFSET_AT,10,o)||!readhex(p+V1_CHECKSUM_AT,c)||c!=fnv(p,96,V1_CHECKSUM_AT,8))return false; if(p[30]<'1'||p[30]>'4'||p[34]<'1'||p[34]>'4'||p[38]<'1'||p[38]>'4')return false;f={};f.byte_offset=o;f.settings={uint8_t(p[30]-'0'),uint8_t(p[34]-'0'),uint8_t(p[38]-'0')};f.history.lazy=o>0;f.history.lazy_anchor=o;return true;}
bool parse_v2(const unsigned char*p,TxtSaveFooter&f){if(std::memcmp(p,V2_MAGIC,sizeof(V2_MAGIC)-1)||p[383]!='\n'||get32(p+V2_SIZE_AT)!=384||get32(p+V2_CHECKSUM_AT)!=fnv(p,384,V2_CHECKSUM_AT,4))return false;int n=p[V2_HISTORY_COUNT_AT];if(n>PAGE_HISTORY_MAX||p[V2_SPACING_AT]<1||p[V2_SPACING_AT]>4||p[V2_TOP_AT]<1||p[V2_TOP_AT]>4||p[V2_BOTTOM_AT]<1||p[V2_BOTTOM_AT]>4)return false;f={};f.byte_offset=get32(p+V2_OFFSET_AT);f.settings={p[V2_SPACING_AT],p[V2_TOP_AT],p[V2_BOTTOM_AT]};f.history.count=n;uint32_t last=0;for(int i=0;i<n;++i){uint32_t x=get32(p+V2_HISTORY_AT+i*4);if(x>=f.byte_offset||(i&&x<=last))return false;f.history.offsets[i]=x;last=x;}f.history.lazy=!n&&f.byte_offset;f.history.lazy_anchor=f.byte_offset;return true;}
}

void make_txt_save_footer(const TxtSaveFooter& f,unsigned char out[TXT_SAVE_FOOTER_SIZE])
{
 std::memset(out,' ',TXT_SAVE_FOOTER_SIZE);std::memcpy(out,V3_MAGIC,sizeof(V3_MAGIC)-1);out[799]='\n';
 std::memcpy(out+V3_FIELDS,"O=0000000000;S=0;T=0;B=0;R=0;I=0;A=0000000000;Q=0000000000;N=00;H=",59);
 dec(out+V3_OFFSET,f.byte_offset,10);out[V3_SPACING]=static_cast<unsigned char>('0'+f.settings.line_spacing);out[V3_TOP]=static_cast<unsigned char>('0'+f.settings.top_margin);out[V3_BOTTOM]=static_cast<unsigned char>('0'+f.settings.bottom_margin);
 int state=int(f.history_rebuild.state);if(state<0||state>3)state=0;out[V3_STATE]=static_cast<unsigned char>('0'+state);out[V3_INITIALIZED]=f.history_rebuild.initialized?'1':'0';dec(out+V3_ANCHOR,f.history_rebuild.anchor,10);dec(out+V3_SCAN,f.history_rebuild.scan.start_offset,10);
 int n=f.history.count;if(n<0)n=0;if(n>PAGE_HISTORY_MAX)n=PAGE_HISTORY_MAX;dec(out+V3_COUNT,uint32_t(n),2);for(int i=0;i<PAGE_HISTORY_MAX;++i){uint32_t x=i<n?f.history.offsets[(f.history.head+i)%PAGE_HISTORY_MAX]:0;dec(out+V3_HISTORY+i*10,x,10);}std::memcpy(out+715,";C=",3);hex(out+V3_CHECKSUM,fnv(out,TXT_SAVE_FOOTER_SIZE,V3_CHECKSUM,8));
}
bool looks_like_txt_save_footer(const unsigned char*p,int n){return p&&((n==TXT_SAVE_FOOTER_SIZE&&!std::memcmp(p,V3_MAGIC,sizeof(V3_MAGIC)-1))||(n==TXT_SAVE_FOOTER_V2_SIZE&&!std::memcmp(p,V2_MAGIC,sizeof(V2_MAGIC)-1))||(n==TXT_SAVE_FOOTER_V1_SIZE&&v1(p)));}
bool parse_txt_save_footer(const unsigned char*p,int n,TxtSaveFooter&f)
{
 if(!p) return false;
 if(n==TXT_SAVE_FOOTER_V1_SIZE) return parse_v1(p,f);
 if(n==TXT_SAVE_FOOTER_V2_SIZE) return parse_v2(p,f);
 if(n!=TXT_SAVE_FOOTER_SIZE||!looks_like_txt_save_footer(p,n)||p[799]!='\n'||std::memcmp(p+V3_FIELDS,"O=",2)) return false;
 uint32_t o,a,q,c;if(!readdec(p+V3_OFFSET,10,o)||!readdec(p+V3_ANCHOR,10,a)||!readdec(p+V3_SCAN,10,q)||!readhex(p+V3_CHECKSUM,c)||c!=fnv(p,n,V3_CHECKSUM,8))return false;(void)q;int count=(p[V3_COUNT]-'0')*10+(p[V3_COUNT+1]-'0');if(count<0||count>PAGE_HISTORY_MAX||p[V3_SPACING]<'1'||p[V3_SPACING]>'4'||p[V3_TOP]<'1'||p[V3_TOP]>'4'||p[V3_BOTTOM]<'1'||p[V3_BOTTOM]>'4'||p[V3_STATE]<'0'||p[V3_STATE]>'3'||(p[V3_INITIALIZED]!='0'&&p[V3_INITIALIZED]!='1'))return false;
 f={};f.byte_offset=o;f.settings={uint8_t(p[V3_SPACING]-'0'),uint8_t(p[V3_TOP]-'0'),uint8_t(p[V3_BOTTOM]-'0')};f.history.count=count;for(int i=0;i<count;++i){uint32_t x;if(!readdec(p+V3_HISTORY+i*10,10,x)||(x>=o)||(i&&x<=f.history.offsets[i-1]))return false;f.history.offsets[i]=x;}const HistoryRebuildState persisted_state=HistoryRebuildState(p[V3_STATE]-'0');
 // A partial Page scan cannot safely resume from only start_offset: its next_offset,
 // EOF flag, and rebuilt ring are not serialized. Restart from zero at the anchor.
 if(persisted_state==HistoryRebuildState::BUILDING){f.history_rebuild={};f.history_rebuild.anchor=a;f.history_rebuild.state=HistoryRebuildState::BUILDING;}else{f.history_rebuild.state=persisted_state;}
 f.history.lazy=count==0&&o>0;f.history.lazy_anchor=o;return true;
}
}
