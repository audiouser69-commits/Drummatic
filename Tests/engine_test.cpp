#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
using namespace mp;
int main(){
  const int sr=48000;
  for(int e=0;e<kNumSynthEngines;++e){
    VoiceShared sh; Voice v; v.shared=&sh; v.prepare(sr); VoiceSettings s;
    for(int t=0;t<kNumTargets;++t) s.base[t]=norm(t,targetMap(t).def);
    s.engine=e; s.base[T_E1Decay]=norm(T_E1Decay,0.5f);
    TriggerInfo ti; v.trigger(s,ti);
    std::vector<float> L(sr),R(sr),a(sr),b(sr),c(sr),d(sr);
    v.render(s,L.data(),R.data(),a.data(),b.data(),c.data(),d.data(),sr);
    float pk=0,dc=0; double rms=0; for(int i=0;i<sr;++i){pk=std::max(pk,std::abs(L[i])); dc+=L[i]; rms+=L[i]*L[i];}
    // zero crossings in first 100ms -> rough pitch
    int zc=0; for(int i=1;i<4800;++i) if((L[i-1]<0)!=(L[i]<0)) ++zc;
    std::printf("%-10s peak=%.3f rms=%.4f dc=%.4f zc100ms=%d\n",engineInfo(e).name,pk,std::sqrt(rms/sr),dc/sr,zc);
  }
}
