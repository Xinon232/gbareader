#include "reader_file.h"
#include "epub_document.h"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using namespace reader;
namespace {
struct Handle { std::string name; BYTE mode; };
std::map<std::string,std::vector<unsigned char>> files;
std::map<FIL*,Handle> handles;
unsigned source_mutations=0;
char fault_kind='-';unsigned fault_ordinal=0;bool fault_hit=false;
std::map<char,unsigned> calls;
bool inject(char kind) {
    const unsigned ordinal=++calls[kind];
    if(kind!=fault_kind || ordinal!=fault_ordinal)return false;
    fault_hit=true;return true;
}
void arm(char kind='-',unsigned ordinal=0) {fault_kind=kind;fault_ordinal=ordinal;fault_hit=false;calls.clear();}
void disarm() {fault_kind='-';}
}
extern "C" {
FRESULT f_open(FIL* f,const TCHAR* name,BYTE mode) {
    assert(!handles.count(f));
    if(inject('o'))return FR_DISK_ERR;
    std::string path(name);
    if(path.size()<4 || path.substr(path.size()-4)!=".sav") {
        assert(mode==(FA_READ|FA_OPEN_EXISTING));
        if(mode & (FA_WRITE|FA_CREATE_ALWAYS|FA_CREATE_NEW|FA_OPEN_ALWAYS)) ++source_mutations;
    }
    if(!files.count(path)) {
        if(!(mode & (FA_CREATE_NEW|FA_CREATE_ALWAYS|FA_OPEN_ALWAYS))) return FR_NO_FILE;
        files[path]={};
    } else if(mode&FA_CREATE_NEW) return FR_EXIST;
    if(mode&FA_CREATE_ALWAYS) files[path].clear();
    *f={}; f->obj.objsize=files[path].size(); handles[f]={path,mode}; return FR_OK;
}
FRESULT f_close(FIL* f) { assert(handles.count(f));if(inject('c'))return FR_DISK_ERR; handles.erase(f); return FR_OK; }
FRESULT f_lseek(FIL* f,FSIZE_t at) {
    if(f->err)return FRESULT(f->err);
    if(inject('l')) {f->err=FR_DISK_ERR;return FR_DISK_ERR;}
    assert(handles.count(f)); auto& h=handles.at(f); auto& b=files[h.name];
    if(at>b.size() && (h.mode&FA_WRITE)) b.resize(at);
    f->fptr=std::min(at,FSIZE_t(b.size())); f->obj.objsize=b.size(); return FR_OK;
}
FRESULT f_read(FIL* f,void* out,UINT n,UINT* got) {
    *got=0;if(f->err)return FRESULT(f->err);
    if(inject('r')) {f->err=FR_DISK_ERR;return FR_DISK_ERR;}
    if(inject('q'))n/=2;
    auto& b=files[handles.at(f).name]; *got=std::min(n,UINT(b.size()-f->fptr));
    std::memcpy(out,b.data()+f->fptr,*got); f->fptr+=*got; return FR_OK;
}
FRESULT f_write(FIL* f,const void* in,UINT n,UINT* got) {
    *got=0;if(f->err)return FRESULT(f->err);
    if(inject('w')) {f->err=FR_DISK_ERR;return FR_DISK_ERR;}
    const bool torn=inject('p');if(torn)n/=2;
    auto& h=handles.at(f); assert(h.mode&FA_WRITE); auto& b=files[h.name];
    b.resize(std::max(b.size(),size_t(f->fptr+n))); std::memcpy(b.data()+f->fptr,in,n);
    f->fptr+=n; f->obj.objsize=b.size(); *got=n; return FR_OK;
}
FRESULT f_sync(FIL*) { return inject('s')?FR_DISK_ERR:FR_OK; }
FRESULT f_truncate(FIL* f) {if(inject('t'))return FR_DISK_ERR; auto& b=files[handles.at(f).name]; b.resize(f->fptr);f->obj.objsize=b.size();return FR_OK; }
}
void fault_cases(const std::string& name,bool cache,bool fresh) {
    const auto baseline=files;
    std::map<char,unsigned> totals;
    for(int stage=0;stage<2;++stage) {
        for(char kind : std::string(stage?"ocrqlwps t":"-")) {
            if(kind==' ')continue;
            const unsigned count=stage?totals[kind]:1;
            for(unsigned ordinal=1;ordinal<=count;++ordinal) {
                assert(handles.empty());files=baseline;if(fresh)files.erase(name+".sav");
                ReaderFile source;assert(source.open_read_only(name.c_str()));
                EpubDocument doc;if(cache)assert(doc.open(source));
                TxtSaveFooter state{};state.settings={3,ParagraphGap::HALF,true};state.byte_offset=411;
                arm(kind,ordinal);
                const bool saved=source.save_footer(state,cache?&doc:nullptr);
                const bool hit=fault_hit;
                if(!stage)totals=calls;
                disarm();
                if(stage && !hit) { std::fprintf(stderr,"unhit %s %c %u\n",name.c_str(),kind,ordinal);assert(false); }
                if(!stage)assert(saved);
                assert(files.at(name)==baseline.at(name) && source_mutations==0);
                if(!saved && !source.save_footer(state,cache?&doc:nullptr)) {
                    std::fprintf(stderr,"retry failed %s cache=%d fresh=%d fault=%c ordinal=%u\n",name.c_str(),cache,fresh,kind,ordinal);assert(false);
                }
                doc.close();source.close();assert(handles.empty());
                assert(source.open_read_only(name.c_str()));TxtSaveFooter actual{};
                assert(source.saved_footer(actual)&&actual.byte_offset==411&&actual.settings.arabic_shaping);
                if(cache) {assert(doc.open(source));std::vector<unsigned char> text(doc.size());assert(doc.read_range(0,text.data(),text.size()));}
                source.close();assert(handles.empty());
            }
        }
    }
    files=baseline;
    printf("PASS: all normal-path I/O fault ordinals and live retry %s cache=%d fresh=%d\n",name.c_str(),cache,fresh);
}
void source_recovery_cases(const std::string& name) {
    const auto baseline=files;
    for(char secondary : std::string("corl")) {
        ReaderFile source;assert(source.open_read_only(name.c_str()));
        EpubDocument doc;const bool epub=name=="Title.epub";
        if(epub)assert(doc.open(source));
        TxtSaveFooter pending{};pending.byte_offset=411;pending.settings={3,ParagraphGap::HALF,true};
        unsigned char expected[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(pending,expected);
        arm('r',1);assert(!source.save_footer(pending)&&fault_hit);disarm();
        // Fail inside recovery itself, not just the original normal-path sweep.
        arm(secondary,1);assert(!source.save_footer(pending)&&fault_hit);disarm();
        unsigned char byte=0;assert(source.byte_at(0,byte)&&byte==baseline.at(name)[0]);
        assert(source.save_footer(pending,epub?&doc:nullptr));
        TxtSaveFooter actual{};assert(source.saved_footer(actual));
        unsigned char encoded[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(actual,encoded);
        assert(!std::memcmp(encoded,expected,sizeof(encoded)));
        doc.close();source.close();assert(handles.empty());files=baseline;
    }
    for(bool resize : {false,true}) {
        ReaderFile source;assert(source.open_read_only(name.c_str()));
        EpubDocument doc;const bool epub=name=="Title.epub";
        unsigned char warmed=0;
        if(epub)assert(doc.open(source)&&doc.byte_at(0,warmed));
        TxtSaveFooter pending{};pending.byte_offset=411;pending.settings={3,ParagraphGap::HALF,true};
        arm('r',1);assert(!source.save_footer(pending)&&fault_hit);disarm();
        if(resize)files[name].push_back('x');else files[name][0]^=1;
        unsigned char byte=0;assert(!source.byte_at(0,byte));
        uint32_t count,crc;assert(!source.companion_cache(count,crc));
        assert(!source.companion_read(0,&byte,1));
        assert(!source.saved_footer(pending)&&!source.save_footer(pending));
        if(epub)assert(!doc.byte_at(0,warmed)); // No already-buffered stale text.
        assert(files.at(name+".sav")==baseline.at(name+".sav"));
        doc.close();source.close();assert(handles.empty());files=baseline;
    }
    printf("PASS: source recovery close/open/read/seek quarantine and changed size/identity refusal %s\n",name.c_str());
}
int main(int argc,char** argv) {
    assert(argc==3);
    files["Title.txt"]=std::vector<unsigned char>(12000,'a');
    const auto original=files.at("Title.txt");
    ReaderFile file; assert(file.open_read_only("Title.txt"));
    TxtSaveFooter state{};state.byte_offset=300;state.settings={2,ParagraphGap::SMALL,true};
    state.history.count=2;state.history.offsets[0]=0;state.history.offsets[1]=100;
    assert(file.save_footer(state));
    assert(source_mutations==0);
    assert(files.at("Title.txt")==original);
    assert(files.count("Title.txt.sav"));
    file.close(); assert(handles.empty()); assert(file.open_read_only("Title.txt"));
    TxtSaveFooter got{};assert(file.saved_footer(got));
    assert(got.byte_offset==300 && same_settings(got.settings,state.settings));
    assert(got.history.count==2 && got.history.offsets[1]==100);
    puts("PASS: actual ReaderFile TXT state companion, immutable source and reopen");
    file.close();
    std::ifstream input(argv[1],std::ios::binary);
    files["Title.epub"]={std::istreambuf_iterator<char>(input),{}};
    const auto epub_original=files.at("Title.epub");
    assert(!epub_original.empty());
    assert(file.open_read_only("Title.epub")); EpubDocument epub;assert(epub.open(file));
    std::vector<unsigned char> normalized(epub.size());
    assert(epub.read_range(0,normalized.data(),normalized.size()));
    assert(file.save_footer(state,&epub));
    assert(files.at("Title.epub")==epub_original && source_mutations==0);
    assert(files.count("Title.epub.sav") && files.count("Title.txt.sav"));
    const auto initial=files.at("Title.epub.sav");
    assert(initial.size()>2048+normalized.size());
    for(int i=0;i<4;++i) {
        state.byte_offset+=10;assert(file.save_footer(state,&epub));
        const auto& saved=files.at("Title.epub.sav");assert(saved.size()==initial.size());
        assert(std::equal(saved.begin()+2048,saved.end(),initial.begin()+2048));
    }
    epub.close();file.close();assert(file.open_read_only("Title.epub"));assert(epub.open(file));
    assert(epub.optimized_size()==normalized.size());
    std::vector<unsigned char> reread(epub.size());assert(epub.read_range(0,reread.data(),reread.size()));
    assert(reread==normalized && file.saved_footer(got) && got.byte_offset==state.byte_offset);
    assert(files.at("Title.epub")==epub_original);
    const auto before_invalid=files.at("Title.epub.sav");
    state.settings.line_spacing=MAX_LINE_SPACING+1;
    assert(!file.save_footer(state,&epub));
    assert(files.at("Title.epub.sav")==before_invalid);
    state.settings.line_spacing=2;
    puts("PASS: external EPUB cache, source immutability, live state-only saves and cached reopen");
    epub.close();file.close();
    fault_cases("Title.txt",false,false);fault_cases("Title.txt",false,true);
    fault_cases("Title.epub",true,false);fault_cases("Title.epub",true,true);
    source_recovery_cases("Title.txt");source_recovery_cases("Title.epub");
    files.erase("Title.txt.sav");assert(file.open_read_only("Title.txt"));
    arm('w',1);assert(!file.save_footer(state));disarm();file.close();
    assert(files.at("Title.txt.sav").empty());
    assert(file.open_read_only("Title.txt"));assert(!file.saved_footer(got));
    assert(!file.save_footer(state));file.close();
    assert(files.at("Title.txt.sav").empty());
    puts("PASS: empty interrupted first creation refuses ownership after reopen");
    // A true crash record: retain a checked identifying header but no full bank.
    // Returned write-error cleanup instead truncates first creation back to zero.
    files["Title.txt.sav"].clear();
    files.erase("Title.txt.sav");assert(file.open_read_only("Title.txt"));
    assert(file.save_footer(state));file.close();
    files["Title.txt.sav"].resize(64);
    assert(file.open_read_only("Title.txt"));assert(!file.saved_footer(got));
    assert(file.save_footer(state));file.close();
    puts("PASS: torn first state with durable identity header recovers after reopen");
    // Current embedded EPUB cache/state migrate through actual ReaderFile/EpubDocument.
    std::ifstream old_input(argv[2],std::ios::binary);
    files["Legacy.epub"]={std::istreambuf_iterator<char>(old_input),{}};
    const auto legacy_epub=files.at("Legacy.epub");
    assert(file.open_read_only("Legacy.epub") && epub.open(file));
    assert(epub.optimized_size() && epub.needs_cache_persistence());
    assert(file.saved_footer(got) && got.byte_offset==30 && got.settings.arabic_shaping);
    assert(file.save_footer(got,&epub));assert(!epub.needs_cache_persistence());
    const auto migrated=files.at("Legacy.epub.sav");
    got.byte_offset=80;assert(file.save_footer(got,&epub));
    assert(std::equal(migrated.begin()+2048,migrated.end(),files.at("Legacy.epub.sav").begin()+2048));
    assert(files.at("Legacy.epub")==legacy_epub);
    epub.close();file.close();assert(file.open_read_only("Legacy.epub")&&epub.open(file));
    assert(epub.optimized_size()&&!epub.needs_cache_persistence());
    assert(file.saved_footer(got)&&got.byte_offset==80);epub.close();file.close();
    puts("PASS: readonly embedded EPUB cache/state migration, precedence and live notification");
    // Current TXT footer stays hidden and byte-identical; newer companion wins.
    unsigned char legacy_footer[TXT_SAVE_FOOTER_SIZE];make_txt_save_footer(state,legacy_footer);
    files["Legacy.txt"]=original;
    files["Legacy.txt"].insert(files["Legacy.txt"].end(),legacy_footer,legacy_footer+sizeof(legacy_footer));
    const auto legacy_txt=files.at("Legacy.txt");
    for(const auto& name : {std::string("Legacy.txt"),std::string("Legacy.epub")}) {
        const auto source_before=files.at(name);
        for(unsigned length : {0u,1200u}) {
            files[name+".sav"]=std::vector<unsigned char>(length,'?');
            const auto collision=files.at(name+".sav");
            assert(file.open_read_only(name.c_str())&&!file.saved_footer(got));
            const bool is_epub=name=="Legacy.epub";
            if(is_epub)assert(epub.open(file));
            assert(!file.save_footer(state,is_epub?&epub:nullptr));
            epub.close();file.close();
            assert(files.at(name+".sav")==collision&&files.at(name)==source_before);
        }
        files.erase(name+".sav"); // Explicit test-fixture move-aside, never production.
    }
    puts("PASS: unknown empty/nonempty TXT and EPUB SAVs suppress embedded state and remain exact");
    assert(file.open_read_only("Legacy.txt")&&file.size()==original.size());
    assert(file.saved_footer(got)&&got.byte_offset==state.byte_offset);
    got.byte_offset=800;assert(file.save_footer(got));got.byte_offset=900;assert(file.save_footer(got));
    file.close();assert(files.at("Legacy.txt")==legacy_txt);
    auto good=files.at("Legacy.txt.sav");
    auto u32=[](const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);};
    const unsigned latest=u32(good.data()+12)>u32(good.data()+1036)?0:1024;
    files["Legacy.txt.sav"][latest+100]^=1;
    assert(file.open_read_only("Legacy.txt")&&file.saved_footer(got)&&got.byte_offset==800);file.close();
    files["Legacy.txt.sav"]=good;
    // A copied pair may be renamed; the identity is content-bound, not path-bound.
    files["Renamed.txt"]=legacy_txt;files["Renamed.txt.sav"]=good;
    assert(file.open_read_only("Renamed.txt")&&file.saved_footer(got)&&got.byte_offset==900);file.close();
    files["Legacy.txt"][0]^=1;
    assert(file.open_read_only("Legacy.txt")&&!file.saved_footer(got));
    assert(!file.save_footer(state));file.close();assert(files.at("Legacy.txt.sav")==good);
    files["Legacy.txt"]=legacy_txt;files["Legacy.txt.sav"]=std::vector<unsigned char>(1200,'?');
    const auto unknown=files.at("Legacy.txt.sav");
    assert(file.open_read_only("Legacy.txt")&&!file.saved_footer(got)&&!file.save_footer(state));
    file.close();assert(files.at("Legacy.txt.sav")==unknown&&files.at("Legacy.txt")==legacy_txt);
    puts("PASS: TXT legacy migration, torn-bank fallback, rename, sampled identity and unknown collision");
    // Bad external header/table reject at open; bad text is checked lazily, then
    // originals rebuild and one cache replacement suffices for the live object.
    const auto good_epub_sav=files.at("Title.epub.sav");
    for(unsigned damaged : {2048u, unsigned(good_epub_sav.size()-1), 2048u+32u}) {
        files["Title.epub.sav"]=good_epub_sav;files["Title.epub.sav"][damaged]^=1;
        assert(file.open_read_only("Title.epub")&&epub.open(file));
        if(damaged==2048u+32u)assert(epub.optimized_size());else assert(!epub.optimized_size());
        std::vector<unsigned char> text(epub.size());assert(epub.read_range(0,text.data(),text.size())&&text==normalized);
        assert(!epub.optimized_size()&&epub.needs_cache_persistence());
        assert(file.save_footer(state,&epub));const auto rebuilt=files.at("Title.epub.sav");
        assert(file.save_footer(state,&epub));assert(rebuilt.size()==files.at("Title.epub.sav").size());
        assert(std::equal(rebuilt.begin()+2048,rebuilt.end(),files.at("Title.epub.sav").begin()+2048));
        epub.close();file.close();assert(files.at("Title.epub")==epub_original);
    }
    puts("PASS: external header/table/block CRC and same-object fallback/rebuild saves");
    assert(handles.empty()&&source_mutations==0);
}
