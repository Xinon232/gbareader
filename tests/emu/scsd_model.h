/* MIT, Halim Jarrar 2026. Copied from gbamp3 tests/scsd_model.h. TEST ONLY Supercard SD/SDHC register model.
 * gbareader: the mode register may also carry the write-access bit (mode 7),
 * CMD3 reports the ident state, and a small SDRAM window answers writes.
 * Actual ROM executes driver, FatFS, decoder and IRQ. No seeded guest buffers.
 * Serial CMD, 4-bit data latch including LDM bus reads. Explicit synthetic
 * costs: 5 cycles/halfword CMD/poll, 8 cycles/word data; not hardware proof.
 */
#include <mgba/internal/arm/arm.h>
#include <string.h>
static struct ARMMemory original_memory;
static struct mCore*model_core;static unsigned model_stats,io_irq_overlaps,io_audio_reads;
static uint64_t io_start,io_total,io_max;static unsigned io_sample_start;
static unsigned char *sdimage;static FILE *image_file;static uint64_t image_size,read_offset;
static unsigned cmd_bits,resp_bit,resp_count,sd_reads,sd_writes,sd_mode,mode_step,read_calls,delay_polls,configured_delay,fail_read;
static unsigned sdhc,read_active,cmd8_seen,high_reads,max_lba,read_cmds,stop_cmds,multi_cmds;
static uint64_t cmd_shift;static unsigned char response[20],sector_data[512];
static unsigned write_active,write_count,write_response,write_status,fail_write;
static uint64_t write_offset;
static unsigned char write_data[520];
static void finish_write(void){
 unsigned crc[4]={0};
 for(unsigned i=0;i<512;i++)for(int shift=4;shift>=0;shift-=4)for(unsigned lane=0;lane<4;lane++){
  unsigned bit=((write_data[i]>>(shift+lane))&1)^((crc[lane]>>15)&1);
  crc[lane]=((crc[lane]<<1)&65535)^(bit?0x1021:0);
 }
 for(unsigned i=0;i<8;i++){
  unsigned v=0;for(unsigned half=0;half<2;half++)for(unsigned lane=0;lane<4;lane++)v|=((crc[lane]>>(15-i*2-half))&1)<<(4*(1-half)+lane);
  if(write_data[512+i]!=v){fprintf(stderr,"BAD DATA CRC\n");exit(43);}
 }
 sd_writes++;write_status=sd_writes==fail_write?11:5;write_response=0;
 if(write_status==5){
  if(write_offset>image_size||image_size-write_offset<512)exit(44);
  if(fseeko(image_file,(off_t)write_offset,SEEK_SET)||fwrite(write_data,1,512,image_file)!=512||fflush(image_file))exit(45);
 }
 write_offset+=512;
}
static unsigned crc7_test(const unsigned char*p,unsigned n){unsigned crc=0;for(unsigned j=0;j<n;j++)for(int bit=7;bit>=0;bit--){unsigned feedback=((crc>>6)^((p[j]>>bit)&1))&1;crc=((crc<<1)&127)^(feedback?9:0);}return (crc<<1)|1;}
static void start_sector(void){
 sd_reads++;read_calls=0;delay_polls=configured_delay;
 unsigned lba=read_offset/512;if(lba>max_lba)max_lba=lba;if(lba>=0x400000)high_reads++;
 if(read_offset>image_size||image_size-read_offset<512||sd_reads==fail_read){delay_polls=0xffffffff;return;}
 if(fseeko(image_file,(off_t)read_offset,SEEK_SET)||fread(sector_data,1,512,image_file)!=512)exit(38);
 io_start=mTimingGlobalTime(model_core->timing);io_sample_start=model_stats?model_core->busRead32(model_core,model_stats+32):0;
 if(model_core->busRead16(model_core,0x04000208))io_audio_reads++;
}
static void sd_command(void){
 unsigned char p[6];uint64_t v=cmd_shift;for(int i=5;i>=0;i--){p[i]=v;v>>=8;}
 unsigned cmd=p[0]&63,arg=((unsigned)p[1]<<24)|((unsigned)p[2]<<16)|((unsigned)p[3]<<8)|p[4];
 if(getenv("SD_TRACE"))fprintf(stderr,"CMD%u arg=%08x\n",cmd,arg);
 if(crc7_test(p,5)!=p[5]){fprintf(stderr,"BAD CMD CRC\n");exit(30);}
 memset(response,0xff,sizeof(response));memset(response,0,6);response[0]=cmd;response[3]=9;resp_bit=0;resp_count=48;
 switch(cmd){
 case 0:resp_count=0;break;
 case 8:cmd8_seen++;response[3]=1;response[4]=0xaa;if(getenv("SDV1"))resp_count=0;break;
 case 55:break;
 case 41:if(sdhc&&(!(arg&0x40000000)||!cmd8_seen)){fprintf(stderr,"SDHC missing HCS/CMD8\n");resp_count=0;break;}response[0]=63;response[1]=sdhc?192:128;break;
 case 2:response[0]=63;resp_count=136;break;
 case 3:response[1]=0x12;response[2]=0x34;response[3]=4;break;   /* R6: state ident (2), as gbareader checks */
 case 9:
  memset(response,0,sizeof(response));response[0]=63;resp_count=136;
  if(getenv("SD_BAD_CSD")){response[1]=128;break;}
  if(sdhc){response[1]=64;unsigned cs=8191;response[8]=(cs>>16)&63;response[9]=cs>>8;response[10]=cs;}
  else{/* CSD v1: 2 GiB, READ_BL_LEN=10,C_SIZE=4095,C_SIZE_MULT=7. */response[6]=10;response[7]=3;response[8]=255;response[9]=192;response[10]=3;response[11]=128;}
  break;
 case 7:case 6:case 16:case 13:break;
 case 18:case 17:
  /* CMD17 and CMD18 both use block addressing on SDHC. */
  read_cmds++;if(cmd==18)multi_cmds++;read_active=1;read_offset=sdhc?(uint64_t)arg*512:arg;
  if(!sdhc&&arg%512)exit(40);
  start_sector();break;
 case 12:read_active=write_active=0;stop_cmds++;break;
 case 24:case 25:
  if(!getenv("SD_ALLOW_INDEX_WRITES")||getenv("SD_READONLY")){response[1]=128;break;}
  write_active=1;write_offset=sdhc?(uint64_t)arg*512:arg;write_count=write_response=0;
  if(write_offset%512)exit(40);
  break;
 default:fprintf(stderr,"UNMODELED COMMAND %u\n",cmd);exit(32);
 }
}
static void sd_store32(struct ARMCore*c,uint32_t a,int32_t val,int*cycles){
 if(a==0x09000000&&sdimage&&write_active){
  if(cycles)*cycles+=8;
  if(write_count){write_data[write_count-1]=(unsigned char)val;if(++write_count==521){finish_write();write_count=0;}}
  else if((uint32_t)val!=0xffffffffu)exit(46);
  return;
 }
 original_memory.store32(c,a,val,cycles);
}
static uint32_t sd_load32(struct ARMCore*c,uint32_t a,int*cycles){
 if(a==0x09000000&&sdimage&&write_active){if(cycles)*cycles+=8;return 0xffffffffu;}
 if(a>=0x09100000&&a<0x09100020&&sdimage){
  if(cycles)*cycles+=8;
  if(!read_active||delay_polls)exit(41);
  unsigned i=read_calls++;
  if(i<512){/* each word clocks four nibbles: only odd latch is stable */
   unsigned off=(i/2)*2;
   return i&1?((unsigned)sector_data[off]|((unsigned)sector_data[off+1]<<8))<<16:(unsigned)sector_data[off]<<24;
  }
  /* Upstream consumes but does not validate CRC16. Deliberately nonzero. */
  return 0xa5a5a5a5;
 }
 return original_memory.load32(c,a,cycles);
}
static uint32_t sd_load_multiple(struct ARMCore*c,uint32_t a,int mask,enum LSMDirection direction,int*cycles){
 if(a==0x09100000&&sdimage){if(direction!=LSM_IA)exit(42);uint32_t at=a;for(int i=0;i<16;i++)if(mask&(1<<i)){c->gprs[i]=sd_load32(c,at,cycles);at+=4;}return at;}
 return original_memory.loadMultiple(c,a,mask,direction,cycles);
}
/* gbareader: a little writable Supercard SDRAM beyond the ROM image (the
 * "Flashcart compatible" probe at 0x08F00000); present with a card image. */
static uint16_t sdram_probe[128];
static int sdram_at(uint32_t a){return sdimage&&a>=0x08F00000&&a<0x08F00100?(int)((a-0x08F00000)>>1):-1;}
static uint32_t sd_load16(struct ARMCore*c,uint32_t a,int*cycles){
 if(sdram_at(a)>=0)return sdram_probe[sdram_at(a)];
 if(a==0x09000000&&sdimage){if(cycles)*cycles+=5;return write_active&&write_response<4?((write_status>>(3-write_response++))&1)*256:256;}
 if(a==0x09800000){if(cycles)*cycles+=5;if(!sdimage)return 0;if(resp_bit<resp_count){unsigned i=resp_bit++;return (response[i/8]>>(7-i%8))&1;}return 1;}
 if(a==0x09100000&&sdimage){
  if(cycles)*cycles+=5;
  if(read_calls==520){uint64_t elapsed=mTimingGlobalTime(model_core->timing)-io_start;io_total+=elapsed;if(elapsed>io_max)io_max=elapsed;if(model_stats&&model_core->busRead32(model_core,model_stats+32)>io_sample_start)io_irq_overlaps++;read_calls++;return 0xf00;}
  if(read_calls==521&&read_active){read_offset+=512;start_sector();}
  if(delay_polls){if(delay_polls!=0xffffffff)delay_polls--;return 256;}return 0;
 }
 return original_memory.load16(c,a,cycles);
}
static void sd_store16(struct ARMCore*c,uint32_t a,int16_t val,int*cycles){
 if(sdram_at(a)>=0){sdram_probe[sdram_at(a)]=(uint16_t)val;return;}
 if(a==0x09000000&&sdimage&&write_active){if(cycles)*cycles+=5;if(!val){if(write_count)exit(47);write_count=1;}return;}
 if(a==0x09fffffe){if(cycles)*cycles+=5;unsigned v=(uint16_t)val;if(mode_step<2){if(v!=0xa55a)exit(34);mode_step++;}else{if((v&3)!=3)exit(35);sd_mode=v;if(++mode_step==4)mode_step=0;}return;}
 if(a==0x09440000){if(cycles)*cycles+=5;return;}
 if(a==0x09800000&&sdimage){if(cycles)*cycles+=5;if((sd_mode&3)!=3)exit(36);cmd_shift=(cmd_shift<<1)|((val>>7)&1);if(++cmd_bits==48){cmd_bits=0;sd_command();}return;}
 original_memory.store16(c,a,val,cycles);
}
static void model_install(struct mCore*c){
 model_core=c;sdhc=getenv("SDHC")!=NULL;
 const char*p=getenv("SD_IMAGE");if(p&&*p){image_file=fopen(p,getenv("SD_ALLOW_INDEX_WRITES")?"r+b":"rb");if(!image_file)exit(37);fseeko(image_file,0,SEEK_END);image_size=ftello(image_file);rewind(image_file);sdimage=malloc(1);}
 const char*d=getenv("SD_DELAY_POLLS");configured_delay=d?strtoul(d,0,0):0;d=getenv("SD_FAIL_READ");fail_read=d?strtoul(d,0,0):0;
 d=getenv("SD_FAIL_WRITE");fail_write=d?strtoul(d,0,0):0;
 struct ARMCore*cpu=c->cpu;original_memory=cpu->memory;cpu->memory.store32=sd_store32;cpu->memory.load32=sd_load32;cpu->memory.load16=sd_load16;cpu->memory.store16=sd_store16;cpu->memory.loadMultiple=sd_load_multiple;
}
