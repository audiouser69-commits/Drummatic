// Fuzz: valori estremi casuali su tutti i motori, filtri e distorsioni
#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
using namespace mp;
int main(){
  const int sr=44100; Rng r(99); int bad=0; float worst=0;
  for(int it=0;it<600;++it){
    VoiceShared sh; Voice v; v.shared=&sh; v.prepare(sr); VoiceSettings s;
    for(int t=0;t<kNumTargets;++t) s.base[t]= (it%3==0)? (r.uni()<0.5f?0.f:1.f) : r.uni();
    s.base[T_E1Decay]=0.5f; s.base[T_Gain]=norm(T_Gain,0);
    s.engine=it%kNumSynthEngines; s.filterMode=r.next()%kNumFilterModes; s.distMode=r.next()%kNumDistModes;
    s.stereo=r.uni()<0.5f; s.post=r.uni()<0.5f; s.variation=r.uni();
    for(int k=0;k<kNumSources;++k) sh.depth[k][r.next()%kNumTargets]=r.bi();
    TriggerInfo ti; ti.velocity=r.uni(); ti.pitchSt=r.bi()*24; ti.accent=r.uni()<0.3f;
    std::vector<float> L(sr/2),R(sr/2),a(sr/2),b(sr/2),c(sr/2),d(sr/2);
    v.trigger(s,ti); v.render(s,L.data(),R.data(),a.data(),b.data(),c.data(),d.data(),sr/4);
    v.trigger(s,ti); v.render(s,L.data()+sr/4,R.data()+sr/4,a.data(),b.data(),c.data(),d.data(),sr/4);
    for(float x:L){ if(!std::isfinite(x)){++bad;break;} worst=std::max(worst,std::abs(x)); }
  }
  std::printf("fuzz bad=%d worstPeak=%.2f\n",bad,worst); return bad?1:0;
}
