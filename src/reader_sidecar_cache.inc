// Only used by the sidecar writer. Bounded streaming export, CRC table in 512 B.
// This header is included after side_read/side_write in reader_sidecar.inc.
namespace {
bool export_side_cache(FIL& file, const ByteSource& text, uint32_t fingerprint,
                       unsigned char* scratch, uint32_t& cache_bytes, uint32_t& cache_crc) {
    const uint32_t length=text.size();
    const uint32_t table_bytes=(length/4096u+(length%4096u!=0))*4u;
    if(!length || uint64_t(SAV_CACHE_START)+32+length+table_bytes>SAV_MAX_BYTES) return false;
    cache_bytes=32+length+table_bytes;
    struct Export {
        FIL& file;
        unsigned char* table;
        uint32_t length, at=0, used=0, block_used=0, block_crc=0xffffffffu;
        uint32_t text_crc=0, table_crc=0xffffffffu, table_at=0;
        bool flush() {
            if(!used) return true;
            if(!side_write(file,SAV_CACHE_START+32+length+table_at,table,used))return false;
            table_at+=used; used=0;return true;
        }
        static bool accept(void* opaque,const unsigned char* bytes,uint32_t count) {
            auto& s=*static_cast<Export*>(opaque);
            if(!count || count>s.length-s.at ||
               !side_write(s.file,SAV_CACHE_START+32+s.at,bytes,count))return false;
            s.at+=count;
            while(count) {
                uint32_t take=4096-s.block_used;if(take>count)take=count;
                s.block_crc=crc32_update(s.block_crc,bytes,take);
                s.block_used+=take;bytes+=take;count-=take;
                if(s.block_used==4096 || (!count && s.at==s.length)) {
                    s.text_crc=s.block_used==4096 ? crc32_combine_4096(s.text_crc,~s.block_crc) :
                        crc32_combine(s.text_crc,~s.block_crc,s.block_used);
                    put32(s.table+s.used,~s.block_crc);
                    s.table_crc=crc32_update(s.table_crc,s.table+s.used,4);
                    s.used+=4;s.block_used=0;s.block_crc=0xffffffffu;
                    if(s.used==512 && !s.flush())return false;
                }
            }
            return true;
        }
    } output{file,scratch,length};
    if(!text.export_text(Export::accept,&output) || output.at!=length || !output.flush())return false;
    unsigned char header[32]{};
    std::memcpy(header,"GBAREPC5",8);put16(header+8,6);put16(header+10,1);
    put32(header+12,fingerprint);put32(header+16,length);put32(header+20,output.text_crc);
    put32(header+24,~output.table_crc);put32(header+28,crc32_bytes(header,28));
    cache_crc=crc32_combine(crc32_bytes(header,32),output.text_crc,length);
    cache_crc=crc32_combine(cache_crc,~output.table_crc,table_bytes);
    return side_write(file,SAV_CACHE_START,header,32) &&
           f_lseek(&file,SAV_CACHE_START+cache_bytes)==FR_OK &&
           f_tell(&file)==SAV_CACHE_START+cache_bytes && f_truncate(&file)==FR_OK && f_sync(&file)==FR_OK;
}
}
