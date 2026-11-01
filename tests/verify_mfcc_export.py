#!/usr/bin/env python3
"""Host parity for the reviewed PIUU v1 16 kHz/429-feature export.
Requires g++; uses existing PCM without regenerating datasets. Temporary build
outputs are cleaned automatically. Fail if any feature differs by >= 1e-5.
"""
from pathlib import Path
import re, subprocess, struct, math, json, tempfile, argparse, hashlib
parser=argparse.ArgumentParser(description="Compare PIUU v1 pristine/optimized MFCC on host; does not measure recognition")
parser.add_argument('export',type=Path)
parser.add_argument('--audio',type=Path,required=True,help='Existing PCM16 mono 16 kHz, at least one second')
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
base=Path(__file__).resolve().parents[1]
source=args.export.resolve(); live=base/'lib/wall_e_model'
workspace=tempfile.TemporaryDirectory(prefix='piuu-mfcc-')
work=Path(workspace.name)
config=re.search(r'ei_dsp_config_mfcc_t .*? = \{(.*?)\n\};', (source/'model-parameters/model_variables.h').read_text(), re.S)[1]
# Named axes are descriptive metadata, not used by extraction.
config=re.sub(r'ei_dsp_config_\d+_\d+_named_axes_size','1',config)
config=re.sub(r'ei_dsp_config_\d+_\d+_named_axes','nullptr',config)
cpp=r'''
#include "edge-impulse-sdk/classifier/ei_run_dsp.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
void *ei_malloc(size_t n){return malloc(n);} void *ei_calloc(size_t n,size_t s){return calloc(n,s);} void ei_free(void *p){free(p);}
void ei_printf(const char*,...){} uint64_t ei_read_timer_us(){return 0;}
static std::vector<int16_t> pcm(16000);
static int readAudio(size_t offset,size_t length,float *out){for(size_t i=0;i<length;++i)out[i]=pcm[offset+i];return 0;}
int main(int argc,char **argv){
 if(argc!=3)return 1;
 FILE *out=fopen(argv[1],"wb"),*in=fopen(argv[2],"rb");if(!out||!in)return 2;
 ei_dsp_config_mfcc_t config={CONFIG};
 for(int round=0;round<40;++round){
  if(round<8){for(int i=0;i<16000;++i){
   if(round==0)pcm[i]=0;
   else if(round==1)pcm[i]=1200;
   else if(round==2)pcm[i]=(i%2)?32767:-32768;
   else pcm[i]=int16_t(sin(i*(round*0.021))*12000 + cos(i*0.137)*1000);
  }}else if(fread(pcm.data(),2,16000,in)!=16000){rewind(in);if(fread(pcm.data(),2,16000,in)!=16000)return 3;}
  signal_t signal{};signal.total_length=16000;signal.get_data=readAudio;
  ei::matrix_t features(1,EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);
  int err=extract_mfcc_features(&signal,&features,&config,16000);
  if(err){fprintf(stderr,"MFCC error %d\n",err);return 4;}
  fwrite(features.buffer,sizeof(float),EI_CLASSIFIER_NN_INPUT_FRAME_SIZE,out);
 }
 fclose(in);fclose(out);return 0;
}
'''.replace('CONFIG',config+'\n')
(work/'mfcc.cpp').write_text(cpp)
audio=args.audio.resolve()
for name,root in [('pristine',source),('optimized',live)]:
 cmd=['g++','-std=c++11','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DEIDSP_USE_CMSIS_DSP=0','-DEIDSP_USE_ESP_DSP=0','-I'+str(root),'-I'+str(root/'edge-impulse-sdk'),str(work/'mfcc.cpp'),str(root/'edge-impulse-sdk/dsp/memory.cpp'),*[str(p) for p in (root/'edge-impulse-sdk/dsp/kissfft').glob('*.cpp')],*[str(p) for p in (root/'edge-impulse-sdk/dsp/kissfft').glob('*.c')],'-o',str(work/name)]
 r=subprocess.run(cmd,capture_output=True,text=True)
 if r.returncode:print(r.stderr[-6000:]);raise SystemExit(r.returncode)
 subprocess.run([str(work/name),str(work/(name+'.bin')),str(audio)],check=True)
a=(work/'pristine.bin').read_bytes();b=(work/'optimized.bin').read_bytes()
x=struct.unpack('<%df'%(len(a)//4),a);y=struct.unpack('<%df'%(len(b)//4),b)
assert len(a)==len(b) and all(math.isfinite(v) for v in x+y)
report={'windows':40,'features_per_window':429,'bit_identical':a==b,'max_abs_difference':max(abs(v-w) for v,w in zip(x,y)),'features_sha256':hashlib.sha256(a).hexdigest(),'pcm_sha256':hashlib.sha256(audio.read_bytes()).hexdigest(),'input':'8 synthetic windows + 32 windows of historical real PCM; repeated calls exercise caches; not recognition accuracy'}
assert report['max_abs_difference'] < 1e-5,report
print(json.dumps(report,indent=2));args.output.write_text(json.dumps(report,indent=2)+'\n')

workspace.cleanup()
