/* MIT, Halim Jarrar 2026. Exact-ROM UI runner: the gbareader ROM on mGBA with
 * the modeled Supercard SD card (scsd_model.h), scripted buttons and
 * screenshots. Not physical hardware proof.
 * Usage: ui_runner ROM FRAMES OUTDIR  (env SD_IMAGE, KEYS_FILE, SHOTS)
 * KEYS_FILE lines: "<frame> <key mask> <frames held>"; SHOTS: "f1,f2,...". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/timing.h>
#include <mgba/internal/gba/gba.h>
#include <mgba-util/vfs.h>
#include "scsd_model.h"
static void shot(const char*root,unsigned f,const color_t*p){
 char path[1024];snprintf(path,sizeof(path),"%s/frame-%05u.ppm",root,f);FILE*z=fopen(path,"wb");if(!z)exit(20);
 fprintf(z,"P6\n240 160\n255\n");
 for(int i=0;i<240*160;i++){unsigned char q[3]={p[i]&255,(p[i]>>8)&255,(p[i]>>16)&255};fwrite(q,1,3,z);}
 fclose(z);
}
int main(int argc,char**argv){
 if(argc!=4)return 2;
 struct mCore*c=mCoreFind(argv[1]);if(!c||!c->init(c))return 3;
 mCoreInitConfig(c,NULL);mCoreConfigSetIntValue(&c->config,"idleOptimization",0);mCoreConfigSetIntValue(&c->config,"logLevel",0);
 mCoreLoadForeignConfig(c,&c->config);
 color_t*p=calloc(240*160,sizeof(*p));c->setVideoBuffer(c,p,240);
 struct VFile*v=VFileOpen(argv[1],O_RDONLY);if(!v||!c->loadROM(c,v))return 4;
 c->reset(c);model_install(c);
 unsigned frames=(unsigned)atoi(argv[2]);
 unsigned ef[512],ek[512],el[512],n=0;
 const char*kf=getenv("KEYS_FILE");
 if(kf){FILE*k=fopen(kf,"r");if(!k)return 5;while(n<512&&fscanf(k,"%u %u %u",&ef[n],&ek[n],&el[n])==3)n++;fclose(k);}
 const char*shots=getenv("SHOTS");
 for(unsigned f=0;f<frames;f++){
  unsigned keys=0;for(unsigned i=0;i<n;i++)if(f>=ef[i]&&f<ef[i]+el[i])keys|=ek[i];
  c->setKeys(c,keys);c->runFrame(c);
  if(shots){char b[4096];snprintf(b,sizeof(b),",%s,",shots);char t[16];snprintf(t,sizeof(t),",%u,",f);if(strstr(b,t))shot(argv[3],f,p);}
 }
 shot(argv[3],frames,p);
 printf("SD reads=%u writes=%u\n",sd_reads,sd_writes);
 c->deinit(c);free(p);if(image_file)fclose(image_file);return 0;
}
