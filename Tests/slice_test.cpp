// MACRO 2 (lane) -> START: ogni colpo deve partire da un punto diverso del campione
#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
using namespace mp;
int main(){
  const int sr=48000; SampleData sd; sd.sourceRate=48000;
  for(int i=0;i<48000;++i) sd.l.push_back((0.1f+0.8f*i/48000.f)*std::sin(6.2831853f*2000.f*i/48000.f)); // ampiezza = posizione
  VoiceShared sh; Voice v; v.shared=&sh; v.prepare(sr); v.sample=&sd; VoiceSettings s;
  for(int t=0;t<kNumTargets;++t) s.base[t]=norm(t,targetMap(t).def);
  s.filterMode=DarkLP; s.synthOn=false; s.sampOn=true; s.sampEnvSrc=0; s.base[T_Blend]=1; s.base[T_Start]=0;
  sh.depth[S_Macro2][T_Start]=1.0f;
  int ok=0;
  for(float m: {0.0f,0.25f,0.5f,0.75f}){
    TriggerInfo ti; ti.macro2=m; v.trigger(s,ti);
    std::vector<float> L(64),R(64),a(64),b(64),c(64),d(64);
    v.render(s,L.data(),R.data(),a.data(),b.data(),c.data(),d.data(),64);
    const float expect=0.1f+0.8f*m; float got=0; for(int i=8;i<64;++i) got=std::max(got,std::abs(L[i]));
    std::printf("macro2=%.2f primo campione=%.3f atteso~%.3f\n",m,got,expect);
    ok += std::abs(got/ (dbToGain(denorm(T_Gain,s.base[T_Gain]))) - expect) < 0.05f;
  }
  return ok==4?0:1;
}
