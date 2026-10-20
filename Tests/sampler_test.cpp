#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
using namespace mp;
int main(){
  const int sr=48000; SampleData sd; sd.sourceRate=44100;
  for(int i=0;i<44100;++i){ float e=std::exp(-i/8000.f); sd.l.push_back(std::sin(i*0.05f)*e); }
  for(int eng=0;eng<kNumSamplerEngines;++eng) for(int pv=0;pv<5;++pv){
    VoiceShared sh; Voice v; v.shared=&sh; v.prepare(sr); v.sample=&sd; VoiceSettings s;
    for(int t=0;t<kNumTargets;++t) s.base[t]=norm(t,targetMap(t).def);
    s.synthOn=false; s.sampOn=true; s.sampEngine=eng; s.base[T_Blend]=1; s.base[T_E2Decay]=norm(T_E2Decay,1.0f);
    for(int k=T_SP1;k<=T_SP4;++k) s.base[k]=pv*0.25f;
    TriggerInfo ti; v.trigger(s,ti);
    std::vector<float> L(sr),R(sr),a(sr),b(sr),c(sr),d(sr);
    v.render(s,L.data(),R.data(),a.data(),b.data(),c.data(),d.data(),sr);
    double rms=0; float pk=0; int nan=0; for(float x:L){ if(!std::isfinite(x)) ++nan; rms+=x*x; pk=std::max(pk,std::abs(x)); }
    std::printf("%-11s p=%.2f rms=%.4f peak=%.2f nan=%d\n",samplerInfo(eng).name,pv*0.25f,std::sqrt(rms/sr),pk,nan);
  }
}
