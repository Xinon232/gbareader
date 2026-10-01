// Production globals + actual bundled ff.c, with disk-sector boot faults.
#include "reader_global_settings.h"
#include "diskio.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <fcntl.h>
#include <unistd.h>
using namespace reader;
static int fd=-1, failures=0, writes=0;
static bool fail_reads=false, all_sectors=true;
static LBA_t bad_sector=0;
extern "C" {
DSTATUS disk_initialize(BYTE){return fd<0?STA_NOINIT:0;}
DSTATUS disk_status(BYTE){return fd<0?STA_NOINIT:0;}
DRESULT disk_read(BYTE,BYTE* b,LBA_t s,UINT n){
    if(fail_reads&&(all_sectors||(bad_sector>=s&&bad_sector-s<n))){++failures;return RES_ERROR;}
    return pread(fd,b,size_t(n)*512,off_t(s)*512)==ssize_t(n)*512?RES_OK:RES_ERROR;
}
DRESULT disk_write(BYTE,const BYTE* b,LBA_t s,UINT n){++writes;return pwrite(fd,b,size_t(n)*512,off_t(s)*512)==ssize_t(n)*512?RES_OK:RES_ERROR;}
DRESULT disk_ioctl(BYTE,BYTE cmd,void* b){if(cmd==CTRL_SYNC)return fsync(fd)==0?RES_OK:RES_ERROR;if(cmd==GET_SECTOR_COUNT){*(LBA_t*)b=lseek(fd,0,SEEK_END)/512;return RES_OK;}if(cmd==GET_SECTOR_SIZE){*(WORD*)b=512;return RES_OK;}if(cmd==GET_BLOCK_SIZE){*(DWORD*)b=1;return RES_OK;}return RES_PARERR;}
}
static const char* paths[]={"/gbareader/SETTINGS0.DAT","/gbareader/SETTINGS1.DAT"};
static void seed_slot(int slot,const GlobalPreferences& p,uint32_t gen,bool future,bool damaged) {
    FIL f{};UINT written=0;unsigned char bytes[GLOBAL_SETTINGS_MAX_BYTES];
    assert(encode_global_settings(p,gen,bytes));const auto n=global_settings_size(p);
    if(future)bytes[8]=99;
    if(damaged)bytes[n-1]^=1;
    assert(f_open(&f,paths[slot],FA_WRITE|FA_CREATE_NEW)==FR_OK);
    assert(f_write(&f,bytes,n,&written)==FR_OK&&written==n);
    assert(f_sync(&f)==FR_OK&&f_close(&f)==FR_OK);
}
static void remount(FATFS& fs) {
    assert(f_mount(nullptr,"0:",0)==FR_OK);assert(f_mount(&fs,"0:",1)==FR_OK);
}
int main(int argc,char** argv){
    assert(argc==3);const std::string which=argv[2];
    fd=::open(argv[1],O_RDWR);assert(fd>=0);FATFS fs{};assert(f_mount(&fs,"0:",1)==FR_OK);
    GlobalPreferences old;old.line_spacing=2;remember_global_book(old,"older.txt");
    if(which!="missing0"&&which!="future-partner")
        seed_slot(0,old,1,which=="future-target",which=="damaged-target"||which=="damaged-only");
    old.line_spacing=3;remember_global_book(old,"previous.txt");
    if(which!="damaged-only")seed_slot(1,old,(which=="exhausted"||which=="partial-exhausted")?0xffffffffu:2,which=="future-partner",false);
    if((which=="partial"||which=="partial-exhausted")||which=="missing0"||which=="retry-read") {
        FIL f{};assert(f_open(&f,paths[1],FA_READ)==FR_OK);
        bad_sector=fs.database+(f.obj.sclust-2)*fs.csize;all_sectors=false;
        assert(f_close(&f)==FR_OK);
    }
    remount(fs);
    const bool blocked=(which=="exhausted"||which=="partial-exhausted")||which=="future-target"||which=="damaged-only";
    GlobalPreferences pending;
    {
        GlobalSettingsStore app;
        if(which!="no-load"&&which!="no-load-defaults") {
            fail_reads=true;const auto loaded=app.load();fail_reads=false;
            assert(failures>0);
            assert(loaded==(((which=="partial"||which=="partial-exhausted")||which=="retry-read")?GlobalLoadResult::RECOVERED:GlobalLoadResult::ERROR));
        }
        if(which!="boot-defaults"&&which!="no-load-defaults") {
            app.values.line_spacing=4;app.values.paragraph_gap=ParagraphGap::NONE;app.values.shoulder_page_turns=true;
            const std::string name=std::string(245,'x')+u8"ééé.txt";
            assert(name.size()==255&&remember_global_book(app.values,name.c_str()));
        }
        pending=app.values;const int before=writes;
        if(which=="retry-read") {
            fail_reads=true;
            for(int n=0;n<3;++n)assert(!app.save()&&app.dirty()&&writes==before&&same_global_preferences(app.values,pending));
            fail_reads=false;
        }
        const bool saved=app.save();assert(saved!=blocked&&same_global_preferences(app.values,pending));
        if(blocked)assert(app.dirty()&&writes==before);
        else assert(!app.dirty());
    }
    remount(fs);
    {
        GlobalSettingsStore cold;const auto loaded=cold.load();
        if(!blocked) {
            assert(loaded==(which=="future-partner"?GlobalLoadResult::RECOVERED:GlobalLoadResult::LOADED));
            assert(same_global_preferences(cold.values,pending));
        } else if(which!="damaged-only")assert(same_global_preferences(cold.values,old));
        printf("{\"case\":\"%s\",\"blocked\":%s,\"failed_reads\":%d,\"generation\":%u,\"fresh_mount\":true}\n",argv[2],blocked?"true":"false",failures,cold.generation());
    }
    assert(f_mount(nullptr,"0:",0)==FR_OK);assert(fsync(fd)==0);assert(::close(fd)==0);
}
