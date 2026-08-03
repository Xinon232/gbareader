#include "epub_document.h"

#include <cstring>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif

namespace reader {
namespace {

bool read_bytes(const ByteSource& source, uint32_t offset, unsigned char* out, uint32_t count)
{
    if(offset > source.size() || count > source.size() - offset) return false;
    for(uint32_t i = 0; i < count; ++i) if(! source.byte_at(offset + i, out[i])) return false;
    return true;
}

bool read16(const ByteSource& s, uint32_t o, uint16_t& v)
{
    unsigned char b[2]; if(! read_bytes(s, o, b, 2)) return false;
    v = uint16_t(b[0] | (uint16_t(b[1]) << 8)); return true;
}

bool read32(const ByteSource& s, uint32_t o, uint32_t& v)
{
    unsigned char b[4]; if(! read_bytes(s, o, b, 4)) return false;
    v = uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
    return true;
}

uint32_t crc32_bytes(const unsigned char* data, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFFu;
    for(uint32_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for(int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & uint32_t(0 - int32_t(crc & 1)));
    }
    return ~crc;
}

constexpr uint16_t ZIP_RELEVANT_FLAGS = 0x284Fu;
constexpr uint16_t ZIP_ENCRYPTION_FLAGS = 0x2041u;

char ascii_lower(char value)
{
    return value >= 'A' && value <= 'Z' ? char(value + ('a' - 'A')) : value;
}

bool ascii_ends_with(const char* value, const char* suffix)
{
    const uint32_t value_size = uint32_t(std::strlen(value));
    const uint32_t suffix_size = uint32_t(std::strlen(suffix));
    if(suffix_size > value_size) return false;
    const char* start = value + value_size - suffix_size;
    for(uint32_t index = 0; index < suffix_size; ++index)
        if(ascii_lower(start[index]) != suffix[index]) return false;
    return true;
}

bool image_path(const char* path)
{
    static const char* extensions[] = {
        ".jpg", ".jpeg", ".png", ".gif", ".webp", ".svg", ".svgz",
        ".bmp", ".avif", ".tif", ".tiff", ".ico", ".jxl", ".heic", ".heif"
    };
    for(const char* extension : extensions) if(ascii_ends_with(path, extension)) return true;
    return false;
}

bool image_media_type(const char* media)
{
    const char prefix[] = "image/";
    for(uint32_t index = 0; index < sizeof(prefix) - 1; ++index)
        if(!media[index] || ascii_lower(media[index]) != prefix[index]) return false;
    return true;
}

const char* bounded_find(const char* data, uint32_t size, const char* needle, uint32_t from = 0)
{
    const uint32_t n = uint32_t(std::strlen(needle));
    if(! n || n > size) return nullptr;
    for(uint32_t i = from; i <= size - n; ++i)
        if(std::memcmp(data + i, needle, n) == 0) return data + i;
    return nullptr;
}

bool xml_name_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ':';
}

bool xml_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

const char* next_open_tag(const char* data, uint32_t size, const char* local_name, uint32_t from = 0)
{
    const uint32_t wanted = uint32_t(std::strlen(local_name));
    for(uint32_t i = from; i < size; ++i) {
        if(i + 4 <= size && !std::memcmp(data + i, "<!--", 4)) {
            const char* marker = bounded_find(data, size, "-->", i + 4);
            if(!marker) return nullptr;
            i = uint32_t(marker - data) + 2;
            continue;
        }
        if(i + 9 <= size && !std::memcmp(data + i, "<![CDATA[", 9)) {
            const char* marker = bounded_find(data, size, "]]>", i + 9);
            if(!marker) return nullptr;
            i = uint32_t(marker - data) + 2;
            continue;
        }
        if(i + 2 <= size && data[i] == '<' && data[i + 1] == '?') {
            const char* marker = bounded_find(data, size, "?>", i + 2);
            if(!marker) return nullptr;
            i = uint32_t(marker - data) + 1;
            continue;
        }
        if(data[i] != '<' || i + 1 >= size || data[i + 1] == '/' || data[i + 1] == '!' || data[i + 1] == '?') continue;
        uint32_t start = i + 1, end = start;
        while(end < size && xml_name_char(data[end])) ++end;
        if(end == start || (end < size && data[end] != ' ' && data[end] != '\t' && data[end] != '\r' && data[end] != '\n' && data[end] != '/' && data[end] != '>')) continue;
        uint32_t local = start;
        for(uint32_t p = start; p < end; ++p) if(data[p] == ':') local = p + 1;
        if(end - local == wanted && !std::memcmp(data + local, local_name, wanted)) return data + i;
    }
    return nullptr;
}

const char* tag_end(const char* tag, const char* limit)
{
    char quote = 0;
    for(const char* p = tag; p < limit; ++p) {
        if(quote) { if(*p == quote) quote = 0; }
        else if(*p == '\'' || *p == '"') quote = *p;
        else if(*p == '>') return p;
    }
    return nullptr;
}

const char* next_close_tag(const char* data, uint32_t size, const char* local_name, uint32_t from)
{
    const uint32_t wanted = uint32_t(std::strlen(local_name));
    for(uint32_t i = from; i + 2 < size; ++i) {
        if(i + 4 <= size && !std::memcmp(data + i, "<!--", 4)) {
            const char* marker = bounded_find(data, size, "-->", i + 4);
            if(!marker) return nullptr;
            i = uint32_t(marker - data) + 2;
            continue;
        }
        if(i + 9 <= size && !std::memcmp(data + i, "<![CDATA[", 9)) {
            const char* marker = bounded_find(data, size, "]]>", i + 9);
            if(!marker) return nullptr;
            i = uint32_t(marker - data) + 2;
            continue;
        }
        if(i + 2 <= size && data[i] == '<' && data[i + 1] == '?') {
            const char* marker = bounded_find(data, size, "?>", i + 2);
            if(!marker) return nullptr;
            i = uint32_t(marker - data) + 1;
            continue;
        }
        if(data[i] != '<' || data[i + 1] != '/') continue;
        uint32_t start = i + 2, end = start;
        while(end < size && xml_name_char(data[end])) ++end;
        uint32_t local = start;
        for(uint32_t p = start; p < end; ++p) if(data[p] == ':') local = p + 1;
        if(end - local != wanted || std::memcmp(data + local, local_name, wanted)) continue;
        while(end < size && (data[end] == ' ' || data[end] == '\t' || data[end] == '\r' || data[end] == '\n')) ++end;
        if(end < size && data[end] == '>') return data + i;
    }
    return nullptr;
}

bool xml_section(const char* data, uint32_t size, const char* local_name,
                 const char*& content, const char*& close)
{
    const char* open = next_open_tag(data, size, local_name);
    if(!open) return false;
    const char* open_end = tag_end(open, data + size);
    if(!open_end) return false;
    close = next_close_tag(data, size, local_name, uint32_t(open_end + 1 - data));
    if(!close) return false;
    content = open_end + 1;
    return true;
}

enum class ExtraResult { OK, MALFORMED, ZIP64 };

ExtraResult check_extra(const ByteSource& source, uint32_t offset, uint32_t size)
{
    uint32_t used = 0;
    while(used < size) {
        if(size - used < 4) return ExtraResult::MALFORMED;
        uint16_t id, length;
        if(!read16(source, offset + used, id) || !read16(source, offset + used + 2, length))
            return ExtraResult::MALFORMED;
        if(id == 0x0001) return ExtraResult::ZIP64;
        used += 4;
        if(uint32_t(length) > size - used) return ExtraResult::MALFORMED;
        used += length;
    }
    return ExtraResult::OK;
}

bool attribute(const char* tag, const char* end, const char* name, char* out, int cap)
{
    const int name_len = int(std::strlen(name));
    for(const char* p = tag; p + name_len < end; ++p) {
        if(std::memcmp(p, name, name_len) || p == tag || !xml_space(p[-1])) continue;
        p += name_len;
        while(p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
        if(p == end || *p++ != '=') continue;
        while(p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
        if(p == end || (*p != '\'' && *p != '"')) return false;
        const char quote = *p++; int n = 0;
        while(p < end && *p != quote) { if(n + 1 >= cap) return false; out[n++] = *p++; }
        if(p == end) return false; out[n] = 0; return true;
    }
    return false;
}

bool normalize_path(const char* base_file, const char* relative, char* out)
{
    char joined[EPUB_MAX_PATH * 2]; int n = 0; int slash = -1;
    for(int i = 0; base_file[i]; ++i) if(base_file[i] == '/') slash = i;
    for(int i = 0; i <= slash; ++i) { if(n + 1 >= int(sizeof(joined))) return false; joined[n++] = base_file[i]; }
    for(int i = 0; relative[i]; ++i) {
        if(relative[i] == '?' || relative[i] == '#') break;
        if(relative[i] == '\\' || n + 1 >= int(sizeof(joined))) return false;
        joined[n++] = relative[i];
    }
    joined[n] = 0; if(relative[0] == '/') return false;
    int out_n = 0; int segment_starts[EPUB_MAX_PATH / 2]; int segments = 0;
    for(int i = 0; i <= n;) {
        while(joined[i] == '/') ++i;
        int start = i; while(joined[i] && joined[i] != '/') ++i;
        int len = i - start; if(! len) break;
        if(len == 1 && joined[start] == '.') continue;
        if(len == 2 && joined[start] == '.' && joined[start + 1] == '.') {
            if(! segments) return false; out_n = segment_starts[--segments]; continue;
        }
        if(segments >= int(sizeof(segment_starts) / sizeof(segment_starts[0]))) return false;
        segment_starts[segments++] = out_n;
        if(out_n) { if(out_n + 1 >= EPUB_MAX_PATH) return false; out[out_n++] = '/'; }
        if(out_n + len >= EPUB_MAX_PATH) return false;
        std::memcpy(out + out_n, joined + start, size_t(len)); out_n += len;
    }
    out[out_n] = 0; return out_n > 0;
}

bool is_block(const char* name)
{
    return !std::strcmp(name,"p") || !std::strcmp(name,"div") || !std::strcmp(name,"br") ||
           !std::strcmp(name,"li") || !std::strcmp(name,"blockquote") ||
           (name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && !name[2]);
}

int encode_utf8(uint32_t cp, unsigned char* out)
{
    if(cp <= 0x7F) { out[0] = uint8_t(cp); return 1; }
    if(cp <= 0x7FF) { out[0]=uint8_t(0xC0|(cp>>6)); out[1]=uint8_t(0x80|(cp&63)); return 2; }
    if(cp <= 0xFFFF && !(cp >= 0xD800 && cp <= 0xDFFF)) { out[0]=uint8_t(0xE0|(cp>>12)); out[1]=uint8_t(0x80|((cp>>6)&63)); out[2]=uint8_t(0x80|(cp&63)); return 3; }
    if(cp <= 0x10FFFF) { out[0]=uint8_t(0xF0|(cp>>18)); out[1]=uint8_t(0x80|((cp>>12)&63)); out[2]=uint8_t(0x80|((cp>>6)&63)); out[3]=uint8_t(0x80|(cp&63)); return 4; }
    out[0]='?'; return 1;
}

struct TextParser {
    enum Mode { TEXT, TAG, COMMENT } mode = TEXT;
    unsigned char* window;
    uint32_t window_start, window_size = 0, total = 0;
    int suppressed = 0, name_size = 0, tag_prefix_size = 0, entity_size = 0;
    char name[16]{}, tag_prefix[4]{}, entity[13]{}, quote = 0, comment_a = 0, comment_b = 0;
    bool closing = false, in_name = false, last_slash = false, pending_space = false, entity_active = false;
    unsigned char last = 0;

    void emit(unsigned char c) {
        if(total >= window_start && window_size < EPUB_TEXT_WINDOW_BYTES) window[window_size++] = c;
        ++total; last = c;
    }
    void ordinary(unsigned char c) {
        if(c=='\r'||c=='\n'||c=='\t'||c==' ') { if(total && last!='\n'&&last!=' ') pending_space=true; return; }
        if(pending_space) { emit(' '); pending_space=false; }
        emit(c);
    }
    void newline() { pending_space=false; if(total && last!='\n') emit('\n'); }
    bool hidden_name() const { return !std::strcmp(name,"head")||!std::strcmp(name,"style")||!std::strcmp(name,"script"); }
    void finish_tag() {
        name[name_size]=0;
        if(!closing&&!last_slash&&hidden_name()) ++suppressed;
        if(closing&&hidden_name()&&suppressed) --suppressed;
        if(!suppressed&&is_block(name)) newline();
        mode=TEXT; quote=0; name_size=0; tag_prefix_size=0; in_name=false;
    }
    void finish_entity(bool semicolon) {
        uint32_t cp='?'; bool valid=semicolon; const char* s=entity; uint32_t len=uint32_t(entity_size);
        if(valid) {
            if(len==3&&!std::memcmp(s,"amp",3))cp='&'; else if(len==2&&!std::memcmp(s,"lt",2))cp='<'; else if(len==2&&!std::memcmp(s,"gt",2))cp='>'; else if(len==4&&!std::memcmp(s,"quot",4))cp='"'; else if(len==4&&!std::memcmp(s,"apos",4))cp='\''; else if(len==4&&!std::memcmp(s,"nbsp",4))cp=' ';
            else if(len==5&&!std::memcmp(s,"mdash",5))cp=0x2014; else if(len==5&&!std::memcmp(s,"ndash",5))cp=0x2013; else if(len==6&&!std::memcmp(s,"hellip",6))cp=0x2026; else if(len==4&&!std::memcmp(s,"copy",4))cp=0x00A9;
            else if(len==5&&!std::memcmp(s,"lsquo",5))cp=0x2018; else if(len==5&&!std::memcmp(s,"rsquo",5))cp=0x2019; else if(len==5&&!std::memcmp(s,"ldquo",5))cp=0x201C; else if(len==5&&!std::memcmp(s,"rdquo",5))cp=0x201D;
            else if(len==3&&!std::memcmp(s,"reg",3))cp=0x00AE; else if(len==5&&!std::memcmp(s,"trade",5))cp=0x2122;
            else if(len>=2&&s[0]=='#') { cp=0;uint32_t i=1;int base=10;if(i<len&&(s[i]=='x'||s[i]=='X')){base=16;++i;}if(i==len)valid=false;for(;valid&&i<len;++i){int d=s[i]>='0'&&s[i]<='9'?s[i]-'0':s[i]>='a'&&s[i]<='f'?s[i]-'a'+10:s[i]>='A'&&s[i]<='F'?s[i]-'A'+10:-1;if(d<0||d>=base||cp>(0x10FFFF-uint32_t(d))/uint32_t(base))valid=false;else cp=cp*uint32_t(base)+uint32_t(d);}if(!valid||cp==0||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF))cp='?';}
        }
        unsigned char bytes[4];int count=encode_utf8(cp,bytes);if(pending_space){emit(' ');pending_space=false;}for(int i=0;i<count;++i)emit(bytes[i]);
        if(!semicolon)for(int i=0;i<entity_size;++i)ordinary(static_cast<unsigned char>(entity[i]));
        entity_size=0;entity_active=false;
    }
    void feed(unsigned char c) {
        if(mode==COMMENT){if(comment_a=='-'&&comment_b=='-'&&c=='>'){mode=TEXT;comment_a=comment_b=0;}else{comment_a=comment_b;comment_b=char(c);}return;}
        if(mode==TAG){
            if(tag_prefix_size<4){tag_prefix[tag_prefix_size++]=char(c);if(tag_prefix_size==3&&tag_prefix[0]=='!'&&tag_prefix[1]=='-'&&tag_prefix[2]=='-'){mode=COMMENT;return;}}
            if(quote){if(c==quote)quote=0;return;}if(c=='\''||c=='"'){quote=char(c);return;}if(c=='>'){finish_tag();return;}
            if(!in_name&&name_size){if(!xml_space(char(c)))last_slash=c=='/';return;}
            if(!in_name){if(xml_space(char(c)))return;if(c=='/'&&!name_size){closing=true;return;}in_name=true;}
            if(in_name&&xml_name_char(char(c))){char n=ascii_lower(char(c));if(n==':')name_size=0;else if(name_size<15)name[name_size++]=n;}else in_name=false;
            if(!xml_space(char(c)))last_slash=c=='/';return;
        }
        if(entity_active){if(c==';'){finish_entity(true);return;}if(c=='<'||c=='&'||entity_size>=12){finish_entity(false);feed(c);return;}entity[entity_size++]=char(c);return;}
        if(c=='<'){mode=TAG;closing=false;in_name=false;last_slash=false;name_size=tag_prefix_size=0;return;}
        if(suppressed)return;if(c=='&'){entity_active=true;entity_size=0;return;}ordinary(c);
    }
    bool finish() { if(mode!=TEXT)return false;if(entity_active)finish_entity(false);pending_space=false;if(total&&last!='\n')emit('\n');return true; }
};
}

const char* epub_error_string(EpubError e)
{
    switch(e) { case EpubError::NONE:return "EPUB ready"; case EpubError::READ_FAILED:return "EPUB read failed"; case EpubError::NOT_ZIP:return "Not an EPUB ZIP"; case EpubError::MULTI_DISK:return "Multi-disk EPUB"; case EpubError::ZIP64:return "ZIP64 EPUB unsupported"; case EpubError::ENCRYPTED:return "Encrypted EPUB"; case EpubError::UNSUPPORTED_COMPRESSION:return "EPUB compression unsupported"; case EpubError::TOO_LARGE:return "EPUB chapter too large"; case EpubError::MISSING_CONTAINER:return "EPUB container missing"; case EpubError::MISSING_ROOTFILE:return "EPUB package missing"; case EpubError::MISSING_MANIFEST_ITEM:return "EPUB manifest broken"; case EpubError::MISSING_SPINE:return "EPUB spine missing"; case EpubError::UNSAFE_PATH:return "Unsafe EPUB path"; case EpubError::INVALID_XHTML:return "EPUB text malformed"; default:return "Corrupt EPUB"; }
}

EpubDocument::EpubDocument() { close(); }
void EpubDocument::close() { _archive=nullptr;_central_offset=0;_central_size=0;_entry_count=0;_spine_count=0;_virtual_size=0;_error=EpubError::NONE;_cached_spine=-1;_window_start=0;_window_size=0;_buffer_size=0; }
bool EpubDocument::fail(EpubError e) const { _error=e; return false; }

bool EpubDocument::open(const ByteSource& archive)
{
    close(); _archive=&archive;
    if(archive.size()>EPUB_MAX_ARCHIVE_BYTES)return fail(EpubError::TOO_LARGE);
    if(!parse_zip()||!build_spine()){_virtual_size=0;return false;}
    _error=EpubError::NONE; return true;
}

bool EpubDocument::parse_zip()
{
    if(_archive->size()<22)return fail(EpubError::NOT_ZIP);
    uint32_t min=_archive->size()>65557?_archive->size()-65557:0,eocd=0,sig=0; bool found=false;
    for(uint32_t p=_archive->size()-22;;--p){
        if(!read32(*_archive,p,sig))return fail(EpubError::READ_FAILED);
        if(sig==0x06054b50){uint16_t candidate_comment;uint32_t candidate_size,candidate_offset;
            if(!read16(*_archive,p+20,candidate_comment)||!read32(*_archive,p+12,candidate_size)||!read32(*_archive,p+16,candidate_offset))return fail(EpubError::READ_FAILED);
            uint32_t locator_sig=0;const bool locator=p>=20&&read32(*_archive,p-20,locator_sig)&&locator_sig==0x07064b50;
            const uint32_t cd_end=locator?p-20:p;
            if(candidate_comment==_archive->size()-p-22u&&candidate_offset<=cd_end&&candidate_size==cd_end-candidate_offset){eocd=p;found=true;break;}}
        if(p==min)break;
    }
    if(!found)return fail(EpubError::NOT_ZIP);
    uint16_t disk,cd_disk,on_disk,count,comment; uint32_t cd_size,cd_offset;
    if(!read16(*_archive,eocd+4,disk)||!read16(*_archive,eocd+6,cd_disk)||!read16(*_archive,eocd+8,on_disk)||!read16(*_archive,eocd+10,count)||!read32(*_archive,eocd+12,cd_size)||!read32(*_archive,eocd+16,cd_offset)||!read16(*_archive,eocd+20,comment))return fail(EpubError::READ_FAILED);
    uint32_t locator_sig=0;if(eocd>=20&&!read32(*_archive,eocd-20,locator_sig))return fail(EpubError::READ_FAILED);if(eocd>=20&&locator_sig==0x07064b50)return fail(EpubError::ZIP64);
    if(disk||cd_disk||on_disk!=count)return fail(EpubError::MULTI_DISK); if(count==0xFFFF||cd_size==0xFFFFFFFF||cd_offset==0xFFFFFFFF)return fail(EpubError::ZIP64);
    if(comment!=_archive->size()-eocd-22u||cd_offset>_archive->size()||cd_size>_archive->size()-cd_offset||cd_offset+cd_size>eocd)return fail(EpubError::MALFORMED_ZIP);
    uint32_t p=cd_offset;
    for(int i=0;i<count;++i){uint32_t s,crc,cs,us,lo;uint16_t flags,method,nl,xl,cl,dstart;
        if(!read32(*_archive,p,s)||s!=0x02014b50)return fail(EpubError::MALFORMED_ZIP);
        if(!read16(*_archive,p+8,flags)||!read16(*_archive,p+10,method)||!read32(*_archive,p+16,crc)||!read32(*_archive,p+20,cs)||!read32(*_archive,p+24,us)||!read16(*_archive,p+28,nl)||!read16(*_archive,p+30,xl)||!read16(*_archive,p+32,cl)||!read16(*_archive,p+34,dstart)||!read32(*_archive,p+42,lo))return fail(EpubError::READ_FAILED);
        if(dstart)return fail(EpubError::MULTI_DISK); if(!nl||nl>=EPUB_MAX_PATH||p>_archive->size()||46u+nl+xl+cl>_archive->size()-p)return fail(EpubError::MALFORMED_ZIP);
        ExtraResult central_extra=check_extra(*_archive,p+46u+nl,xl);if(central_extra==ExtraResult::ZIP64)return fail(EpubError::ZIP64);if(central_extra==ExtraResult::MALFORMED)return fail(EpubError::MALFORMED_ZIP);
        char central_name[EPUB_MAX_PATH];if(!read_bytes(*_archive,p+46,(unsigned char*)central_name,nl))return fail(EpubError::READ_FAILED);central_name[nl]=0;
        if(image_path(central_name)){p+=46u+nl+xl+cl;continue;}
        if(flags&ZIP_ENCRYPTION_FLAGS)return fail(EpubError::ENCRYPTED);if(lo>_archive->size())return fail(EpubError::MALFORMED_ZIP);
        uint32_t local_sig,local_crc,local_cs,local_us;uint16_t local_flags,local_method,local_nl,local_xl;
        if(lo>_archive->size()||30u>_archive->size()-lo||!read32(*_archive,lo,local_sig)||!read16(*_archive,lo+6,local_flags)||!read16(*_archive,lo+8,local_method)||!read32(*_archive,lo+14,local_crc)||!read32(*_archive,lo+18,local_cs)||!read32(*_archive,lo+22,local_us)||!read16(*_archive,lo+26,local_nl)||!read16(*_archive,lo+28,local_xl))return fail(EpubError::READ_FAILED);
        if(local_sig!=0x04034b50||local_method!=method||local_nl!=nl||(local_flags&ZIP_RELEVANT_FLAGS)!=(flags&ZIP_RELEVANT_FLAGS))return fail(EpubError::MALFORMED_ZIP);if(local_flags&ZIP_ENCRYPTION_FLAGS)return fail(EpubError::ENCRYPTED);if(30u+uint32_t(local_nl)+uint32_t(local_xl)>_archive->size()-lo)return fail(EpubError::MALFORMED_ZIP);
        ExtraResult local_extra=check_extra(*_archive,lo+30u+local_nl,local_xl);if(local_extra==ExtraResult::ZIP64)return fail(EpubError::ZIP64);if(local_extra==ExtraResult::MALFORMED)return fail(EpubError::MALFORMED_ZIP);
        for(uint32_t n=0;n<nl;++n){unsigned char central_char,local_char;if(!read_bytes(*_archive,p+46u+n,&central_char,1)||!read_bytes(*_archive,lo+30u+n,&local_char,1))return fail(EpubError::READ_FAILED);if(central_char!=local_char)return fail(EpubError::MALFORMED_ZIP);}
        uint32_t data=lo+30u+local_nl+local_xl;if(cs>_archive->size()-data)return fail(EpubError::MALFORMED_ZIP);
        if(!(flags&0x0008u)){
            if(local_crc!=crc||local_cs!=cs||local_us!=us)return fail(EpubError::MALFORMED_ZIP);
        }else{
            if((local_crc&&local_crc!=crc)||(local_cs&&local_cs!=cs)||(local_us&&local_us!=us))return fail(EpubError::MALFORMED_ZIP);
            const uint32_t descriptor=data+cs;
            if(descriptor>cd_offset)return fail(EpubError::MALFORMED_ZIP);
            const uint32_t before_cd=cd_offset-descriptor;
            uint32_t first=0,second=0,third=0,fourth=0;bool unsigned_values=false,signed_values=false;
            if(before_cd>=12u){
                if(!read32(*_archive,descriptor,first)||!read32(*_archive,descriptor+4u,second)||!read32(*_archive,descriptor+8u,third))return fail(EpubError::READ_FAILED);
                unsigned_values=first==crc&&second==cs&&third==us;
                if(first==0x08074b50u&&before_cd>=16u){
                    if(!read32(*_archive,descriptor+12u,fourth))return fail(EpubError::READ_FAILED);
                    signed_values=second==crc&&third==cs&&fourth==us;
                }
            }
            bool endpoint_read_failed=false;
            auto legal_endpoint=[&](uint32_t length){
                const uint32_t end=descriptor+length;
                if(end==cd_offset)return true;
                uint32_t next_sig=0;
                if(end>cd_offset||cd_offset-end<4u)return false;
                if(!read32(*_archive,end,next_sig)){endpoint_read_failed=true;return false;}
                return next_sig==0x04034b50u;
            };
            const bool valid=(unsigned_values&&legal_endpoint(12u))||(signed_values&&legal_endpoint(16u));
            if(endpoint_read_failed)return fail(EpubError::READ_FAILED);
            if(!valid)return fail(EpubError::MALFORMED_ZIP);
        }
        p+=46u+nl+xl+cl;
    }
    if(p!=cd_offset+cd_size)return fail(EpubError::MALFORMED_ZIP);_central_offset=cd_offset;_central_size=cd_size;_entry_count=count;return true;
}

int EpubDocument::find_entry(const char* name, ZipEntry& entry) const
{
    if(!name||!_archive)return -1;
    const uint32_t wanted=uint32_t(std::strlen(name));
    uint32_t p=_central_offset;
    for(int i=0;i<_entry_count;++i){
        uint32_t sig,crc,cs,us,lo;uint16_t flags,method,nl,xl,cl;
        if(!read32(*_archive,p,sig)||!read16(*_archive,p+8,flags)||!read16(*_archive,p+10,method)||!read32(*_archive,p+16,crc)||!read32(*_archive,p+20,cs)||!read32(*_archive,p+24,us)||!read16(*_archive,p+28,nl)||!read16(*_archive,p+30,xl)||!read16(*_archive,p+32,cl)||!read32(*_archive,p+42,lo)){fail(EpubError::READ_FAILED);return -1;}
        if(sig!=0x02014b50||!nl||nl>=EPUB_MAX_PATH||p>_archive->size()||46u+nl+xl+cl>_archive->size()-p){fail(EpubError::MALFORMED_ZIP);return -1;}
        bool match=wanted==nl;
        for(uint32_t n=0;match&&n<nl;++n){unsigned char c;if(!read_bytes(*_archive,p+46u+n,&c,1)){fail(EpubError::READ_FAILED);return -1;}if(c!=static_cast<unsigned char>(name[n]))match=false;}
        if(match){
            if(!read_bytes(*_archive,p+46,(unsigned char*)entry.name,nl)){fail(EpubError::READ_FAILED);return -1;}
            entry.name[nl]=0;entry.compressed_size=cs;entry.uncompressed_size=us;entry.local_offset=lo;entry.crc32=crc;entry.method=method;entry.flags=flags;entry.name_length=nl;return 1;
        }
        p+=46u+nl+xl+cl;
    }
    if(p!=_central_offset+_central_size){fail(EpubError::MALFORMED_ZIP);return -1;}
    return 0;
}

bool EpubDocument::load_entry(const ZipEntry& z, uint32_t uncompressed_limit) const
{
    uint32_t sig;uint16_t flags,method,nl,xl;
    if(z.method!=0&&z.method!=8)return fail(EpubError::UNSUPPORTED_COMPRESSION);
    if(z.compressed_size>EPUB_MAX_COMPRESSED_BYTES||z.uncompressed_size>uncompressed_limit)return fail(EpubError::TOO_LARGE);
    if(!read32(*_archive,z.local_offset,sig)||!read16(*_archive,z.local_offset+6,flags)||!read16(*_archive,z.local_offset+8,method)||!read16(*_archive,z.local_offset+26,nl)||!read16(*_archive,z.local_offset+28,xl))return fail(EpubError::READ_FAILED);
    if(sig!=0x04034b50||method!=z.method||nl!=z.name_length||(flags&ZIP_RELEVANT_FLAGS)!=(z.flags&ZIP_RELEVANT_FLAGS))return fail(EpubError::MALFORMED_ZIP);
    if(flags&ZIP_ENCRYPTION_FLAGS)return fail(EpubError::ENCRYPTED);
    if(z.local_offset>_archive->size()||30u+uint32_t(nl)+uint32_t(xl)>_archive->size()-z.local_offset)return fail(EpubError::MALFORMED_ZIP);
    for(uint32_t n=0;n<nl;++n){unsigned char c;if(!read_bytes(*_archive,z.local_offset+30u+n,&c,1))return fail(EpubError::READ_FAILED);if(c!=static_cast<unsigned char>(z.name[n]))return fail(EpubError::MALFORMED_ZIP);}
    uint32_t data=z.local_offset+30u+nl+xl;if(z.compressed_size>_archive->size()-data)return fail(EpubError::MALFORMED_ZIP);
    if(z.method==0){if(z.compressed_size!=z.uncompressed_size)return fail(EpubError::MALFORMED_ZIP);if(!read_bytes(*_archive,data,_workspace.metadata,z.uncompressed_size))return fail(EpubError::READ_FAILED);if(crc32_bytes(_workspace.metadata,z.uncompressed_size)!=z.crc32)return fail(EpubError::MALFORMED_ZIP);_buffer_size=z.uncompressed_size;return true;}
    tinfl_init(&_inflator);uint32_t in_pos=0,out_pos=0;size_t avail=0,used=0;tinfl_status status=TINFL_STATUS_NEEDS_MORE_INPUT;
    while(status>0){if(used==avail){uint32_t left=z.compressed_size-in_pos;uint32_t take=left>sizeof(_input)?sizeof(_input):left;if(!take)return fail(EpubError::MALFORMED_ZIP);if(!read_bytes(*_archive,data+in_pos,_input,take))return fail(EpubError::READ_FAILED);in_pos+=take;avail=take;used=0;}
        size_t in_count=avail-used,out_count=z.uncompressed_size-out_pos;uint32_t f=TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;if(in_pos<z.compressed_size||used+in_count<avail)f|=TINFL_FLAG_HAS_MORE_INPUT;status=tinfl_decompress(&_inflator,_input+used,&in_count,_workspace.metadata,_workspace.metadata+out_pos,&out_count,f);used+=in_count;out_pos+=uint32_t(out_count);if(out_pos>z.uncompressed_size||(status==TINFL_STATUS_HAS_MORE_OUTPUT&&out_pos==z.uncompressed_size))return fail(EpubError::TOO_LARGE);}
    const uint32_t consumed=in_pos-uint32_t(avail-used);
    if(status!=TINFL_STATUS_DONE||out_pos!=z.uncompressed_size||consumed!=z.compressed_size||crc32_bytes(_workspace.metadata,out_pos)!=z.crc32)return fail(EpubError::MALFORMED_ZIP);_buffer_size=out_pos;return true;
}

bool EpubDocument::build_spine()
{
    ZipEntry container{};int container_status=find_entry("META-INF/container.xml",container);if(container_status<0)return false;if(!container_status)return fail(EpubError::MISSING_CONTAINER);if(!load_entry(container,EPUB_MAX_METADATA_BYTES))return false;
    const char* root=next_open_tag((char*)_workspace.metadata,_buffer_size,"rootfile");if(!root)return fail(EpubError::MISSING_ROOTFILE);const char* end=tag_end(root,(char*)_workspace.metadata+_buffer_size);char opf_path[EPUB_MAX_PATH];if(!end||!attribute(root,end,"full-path",opf_path,sizeof(opf_path)))return fail(EpubError::MISSING_ROOTFILE);
    char normalized[EPUB_MAX_PATH];if(!normalize_path("",opf_path,normalized))return fail(EpubError::UNSAFE_PATH);ZipEntry opf{};int opf_status=find_entry(normalized,opf);if(opf_status<0)return false;if(!opf_status)return fail(EpubError::MISSING_ROOTFILE);if(!load_entry(opf,EPUB_MAX_METADATA_BYTES))return false;
    const char* data=(char*)_workspace.metadata;const char *manifest,*manifest_close,*spine,*spine_close;
    if(!xml_section(data,_buffer_size,"manifest",manifest,manifest_close))return fail(EpubError::MISSING_MANIFEST_ITEM);
    if(!xml_section(data,_buffer_size,"spine",spine,spine_close))return fail(EpubError::MISSING_SPINE);
    int refs=0;uint32_t pos=0;const uint32_t spine_size=uint32_t(spine_close-spine);const uint32_t manifest_size=uint32_t(manifest_close-manifest);
    while(const char* itemref=next_open_tag(spine,spine_size,"itemref",pos)){
        const char* e=tag_end(itemref,spine_close);if(!e)return fail(EpubError::MISSING_SPINE);
        char idref[EPUB_MAX_PATH];if(!attribute(itemref,e,"idref",idref,sizeof(idref)))return fail(EpubError::MISSING_SPINE);
        bool matched=false,skip_image=false;uint32_t manifest_pos=0;
        while(const char* item=next_open_tag(manifest,manifest_size,"item",manifest_pos)){
            const char* item_end=tag_end(item,manifest_close);if(!item_end)return fail(EpubError::MISSING_MANIFEST_ITEM);
            char id[EPUB_MAX_PATH],href[EPUB_MAX_PATH],media[EPUB_MAX_PATH];
            if(attribute(item,item_end,"id",id,sizeof(id))&&!std::strcmp(id,idref)&&attribute(item,item_end,"href",href,sizeof(href))){
                const bool has_media=attribute(item,item_end,"media-type",media,sizeof(media));
                if(has_media&&image_media_type(media)){matched=true;skip_image=true;break;}
                if(has_media&&std::strcmp(media,"application/xhtml+xml")&&std::strcmp(media,"text/html"))return fail(EpubError::MISSING_MANIFEST_ITEM);
                if(refs>=EPUB_MAX_SPINE_ITEMS)return fail(EpubError::TOO_LARGE);
                char path[EPUB_MAX_PATH];if(!normalize_path(normalized,href,path))return fail(EpubError::UNSAFE_PATH);
                ZipEntry entry{};int entry_status=find_entry(path,entry);if(entry_status<0)return false;if(!entry_status)return fail(EpubError::MISSING_MANIFEST_ITEM);
                _spine[refs].entry=entry;matched=true;break;
            }
            manifest_pos=uint32_t(item_end-manifest)+1;
        }
        if(!matched)return fail(EpubError::MISSING_MANIFEST_ITEM);
        if(!skip_image)++refs;
        pos=uint32_t(e-spine)+1;
    }
    if(!refs)return fail(EpubError::MISSING_SPINE);
    _spine_count=refs;_virtual_size=0;_cached_spine=-1;
    for(int i=0;i<_spine_count;++i){if(!stream_chapter(i,0,true))return false;_spine[i].start=_virtual_size;_spine[i].size=_buffer_size;if(_virtual_size>0xFFFFFFFFu-_buffer_size)return fail(EpubError::TOO_LARGE);_virtual_size+=_buffer_size;}
    _cached_spine=-1;return true;
}

bool EpubDocument::stream_chapter(int i,uint32_t window_start,bool count_only) const
{
    const ZipEntry& z=_spine[i].entry;uint32_t sig,data;uint16_t flags,method,nl,xl;
    if(z.method!=0&&z.method!=8)return fail(EpubError::UNSUPPORTED_COMPRESSION);
    if(z.compressed_size>EPUB_MAX_COMPRESSED_BYTES||z.uncompressed_size>EPUB_MAX_XHTML_BYTES)return fail(EpubError::TOO_LARGE);
    if(!read32(*_archive,z.local_offset,sig)||!read16(*_archive,z.local_offset+6,flags)||!read16(*_archive,z.local_offset+8,method)||!read16(*_archive,z.local_offset+26,nl)||!read16(*_archive,z.local_offset+28,xl))return fail(EpubError::READ_FAILED);
    if(sig!=0x04034b50||method!=z.method||nl!=z.name_length||(flags&ZIP_RELEVANT_FLAGS)!=(z.flags&ZIP_RELEVANT_FLAGS))return fail(EpubError::MALFORMED_ZIP);
    if(flags&ZIP_ENCRYPTION_FLAGS)return fail(EpubError::ENCRYPTED);
    if(z.local_offset>_archive->size()||30u+uint32_t(nl)+uint32_t(xl)>_archive->size()-z.local_offset)return fail(EpubError::MALFORMED_ZIP);
    for(uint32_t n=0;n<nl;++n){unsigned char c;if(!read_bytes(*_archive,z.local_offset+30u+n,&c,1))return fail(EpubError::READ_FAILED);if(c!=static_cast<unsigned char>(z.name[n]))return fail(EpubError::MALFORMED_ZIP);}
    data=z.local_offset+30u+nl+xl;if(z.compressed_size>_archive->size()-data)return fail(EpubError::MALFORMED_ZIP);
    TextParser parser{};parser.window=_workspace.stream.text;parser.window_start=window_start;
    uint32_t crc=0xFFFFFFFFu,output=0;
    auto consume=[&](const unsigned char* bytes,uint32_t count){for(uint32_t n=0;n<count;++n){crc^=bytes[n];for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xEDB88320u&uint32_t(0-int32_t(crc&1)));parser.feed(bytes[n]);}output+=count;};
    uint32_t consumed=0;
    if(z.method==0){
        if(z.compressed_size!=z.uncompressed_size)return fail(EpubError::MALFORMED_ZIP);
        while(consumed<z.uncompressed_size){uint32_t take=z.uncompressed_size-consumed;if(take>sizeof(_input))take=sizeof(_input);if(!read_bytes(*_archive,data+consumed,_input,take))return fail(EpubError::READ_FAILED);consume(_input,take);consumed+=take;if(!count_only&&parser.window_size==EPUB_TEXT_WINDOW_BYTES){_cached_spine=i;_window_start=window_start;_window_size=parser.window_size;return true;}}
    }else{
        tinfl_init(&_inflator);uint32_t read_pos=0,dict_pos=0;size_t avail=0,used=0;tinfl_status status=TINFL_STATUS_NEEDS_MORE_INPUT;
        while(status>0){
            if(used==avail){uint32_t left=z.compressed_size-read_pos;uint32_t take=left>sizeof(_input)?sizeof(_input):left;if(!take)return fail(EpubError::MALFORMED_ZIP);if(!read_bytes(*_archive,data+read_pos,_input,take))return fail(EpubError::READ_FAILED);read_pos+=take;avail=take;used=0;}
            size_t in_count=avail-used,out_count=EPUB_INFLATE_DICTIONARY_BYTES-dict_pos;uint32_t f=0;if(read_pos<z.compressed_size||used+in_count<avail)f|=TINFL_FLAG_HAS_MORE_INPUT;
            status=tinfl_decompress(&_inflator,_input+used,&in_count,_workspace.stream.dictionary,_workspace.stream.dictionary+dict_pos,&out_count,f);
            used+=in_count;consume(_workspace.stream.dictionary+dict_pos,uint32_t(out_count));dict_pos+=uint32_t(out_count);
            if(dict_pos==EPUB_INFLATE_DICTIONARY_BYTES)dict_pos=0;
            if(output>z.uncompressed_size)return fail(EpubError::MALFORMED_ZIP);
            if(!count_only&&parser.window_size==EPUB_TEXT_WINDOW_BYTES){_cached_spine=i;_window_start=window_start;_window_size=parser.window_size;return true;}
        }
        consumed=read_pos-uint32_t(avail-used);
        if(status!=TINFL_STATUS_DONE)return fail(EpubError::MALFORMED_ZIP);
    }
    if(output!=z.uncompressed_size||consumed!=z.compressed_size||~crc!=z.crc32)return fail(EpubError::MALFORMED_ZIP);
    if(!parser.finish())return fail(EpubError::INVALID_XHTML);
    _buffer_size=parser.total;
    if(!count_only){_cached_spine=i;_window_start=window_start;_window_size=parser.window_size;}
    return true;
}

bool EpubDocument::byte_at(uint32_t offset,unsigned char& value) const
{
    if(offset>=_virtual_size)return false;int lo=0,hi=_spine_count-1;while(lo<=hi){int mid=(lo+hi)/2;const SpineItem&s=_spine[mid];if(offset<s.start)hi=mid-1;else if(offset>=s.start+s.size)lo=mid+1;else{uint32_t local=offset-s.start;if(_cached_spine!=mid||local<_window_start||local>=_window_start+_window_size){uint32_t start=(local/EPUB_TEXT_WINDOW_BYTES)*EPUB_TEXT_WINDOW_BYTES;if(!stream_chapter(mid,start,false))return false;}if(local<_window_start||local>=_window_start+_window_size)return fail(EpubError::MALFORMED_ZIP);value=_workspace.stream.text[local-_window_start];return true;}}return fail(EpubError::MALFORMED_ZIP);
}
}
