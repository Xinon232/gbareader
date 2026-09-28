#pragma once
#include "reader_global_settings.h"
#include "reader_crc32.h"
#include <map>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace reader;
using Bytes=std::vector<unsigned char>;
static std::map<std::string,Bytes> files;
static std::map<FIL*,std::string> handles;
static int writes=0;
static char fault=0; static int ordinal=0, calls=0; static bool persistent=false, short_io=false, corrupt=false;
static std::map<char,int> counts;
static bool hit(char op) {++counts[op];return op==fault && (++calls==ordinal || (persistent&&calls>ordinal));}
static void reset_fault(){fault=0;ordinal=calls=0;persistent=short_io=corrupt=false;counts.clear();}
static const char* paths[]={"/gbareader/SETTINGS0.DAT","/gbareader/SETTINGS1.DAT"};
extern "C" {
FRESULT f_open(FIL* f,const TCHAR* path,BYTE mode) {
    assert(!handles.count(f)); assert(path==std::string(paths[0])||path==std::string(paths[1]));
    if(hit('o'))return FR_DISK_ERR;
    if(mode&FA_CREATE_NEW){if(files.count(path))return FR_EXIST;files[path]={};}
    if(!files.count(path))return FR_NO_FILE;
    if(mode&FA_CREATE_ALWAYS)files[path].clear();
    *f={};f->obj.objsize=files[path].size();handles[f]=path;return FR_OK;
}
FRESULT f_close(FIL* f){assert(handles.count(f));if(hit('c'))return FR_DISK_ERR;handles.erase(f);return FR_OK;}
FRESULT f_read(FIL* f,void* b,UINT n,UINT* got){assert(handles.count(f));bool fail=hit('r');*got=0;if(fail&&!short_io&&!corrupt)return FR_DISK_ERR;auto& v=files[handles[f]];*got=std::min<size_t>(n,v.size()-f->fptr);if(fail&&short_io&&*got)--*got;if(*got)memcpy(b,v.data()+f->fptr,*got);if(fail&&corrupt&&*got)static_cast<unsigned char*>(b)[0]^=1;f->fptr+=*got;return FR_OK;}
FRESULT f_write(FIL* f,const void* b,UINT n,UINT* got){assert(handles.count(f));++writes;bool fail=hit('w');*got=0;if(fail&&!short_io)return FR_DISK_ERR;if(fail&&short_io)n/=2;auto& v=files[handles[f]];v.resize(f->fptr+n);memcpy(v.data()+f->fptr,b,n);f->fptr+=n;f->obj.objsize=v.size();*got=n;return FR_OK;}
FRESULT f_lseek(FIL* f,FSIZE_t pos){assert(handles.count(f));if(hit('l'))return FR_DISK_ERR;f->fptr=pos;return FR_OK;}
FRESULT f_truncate(FIL* f){assert(handles.count(f));if(hit('t'))return FR_DISK_ERR;files[handles[f]].resize(f->fptr);f->obj.objsize=f->fptr;return FR_OK;}
FRESULT f_sync(FIL* f){assert(handles.count(f));return hit('s')?FR_DISK_ERR:FR_OK;}
}
static Bytes record(GlobalPreferences p,uint32_t gen){Bytes b(global_settings_size(p));assert(encode_global_settings(p,gen,b.data()));return b;}
