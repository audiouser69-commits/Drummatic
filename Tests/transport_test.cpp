#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
using namespace mp;
static float runBlocks (DrumMachine& dm, Transport& tp, int blocks, bool playing)
{
    std::vector<float> L (512), R (512); float pk = 0;
    for (int b = 0; b < blocks; ++b)
    {
        tp.hostPlaying = playing; tp.hostHasPosition = true;
        dm.process (L.data(), R.data(), 512, tp);
        for (float x : L) pk = std::max (pk, std::abs (x));
        if (playing) tp.ppq += 512.0 / 48000 * tp.bpm / 60;
    }
    return pk;
}
int main()
{
    DrumMachine dm; dm.prepare (48000, 512);
    for (int v = 0; v < 8; ++v) for (int t = 0; t < kNumTargets; ++t) dm.vs[v].base[t] = norm (t, targetMap (t).def);
    dm.ss[0].fill = 4;
    Transport tp; tp.bpm = 120;
    const float a = runBlocks (dm, tp, 200, true);
    dm.setTransportMode (DrumMachine::Stopped);          // es. STOP del plugin o nota MIDI ch10
    runBlocks (dm, tp, 300, true);
    const float b = runBlocks (dm, tp, 100, true);        // ancora fermo mentre il DAW suona
    runBlocks (dm, tp, 50, false);                        // stop nel DAW
    const float c = runBlocks (dm, tp, 200, true);        // play nel DAW → deve ripartire
    // blocco gigante oltre quello annunciato
    std::vector<float> L (8192), R (8192); tp.hostPlaying = true;
    dm.process (L.data(), R.data(), 8192, tp);
    std::printf ("play=%.3f fermo=%.4f dopo-riavvio-DAW=%.3f\n", a, b, c);
    return (a > 0.05f && b < 1e-4f && c > 0.05f) ? 0 : 1;
}
