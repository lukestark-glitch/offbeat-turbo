#include "Preview.hpp"
#include "DemoLoop.h" // aus assets/demo-loop.raw erzeugt (xxd -i), siehe Makefile

#include <cmath>
#include <cstring>

namespace offbeat {

float demoSample(uint32_t frame, int channel)
{
    const size_t at = ((size_t)(frame % kDemoFrames) * 2 + channel) * 2;
    int16_t v;
    std::memcpy(&v, demo_loop_raw + at, 2); // 16 Bit little-endian
    return (float)v / 32767.0f;
}

void renderPreview(const float* params, int layer, double beats, std::vector<float>& l, std::vector<float>& r)
{
    const uint32_t frames = (uint32_t)std::lround(beats * kDemoBeat);
    if (layer == 0) {
        l.resize(frames), r.resize(frames);
        for (uint32_t i = 0; i < frames; ++i)
            l[i] = demoSample(i, 0), r[i] = demoSample(i, 1);
        return;
    }
    Turbo turbo;
    turbo.setSampleRate((float)kDemoRate);
    for (int i = 0; i < kParamCount; ++i)
        if (!(kParams[i].flags & (kFlagHidden | kFlagOutput)))
            turbo.setParam(i, params[i]);
    const uint32_t lat = turbo.latency(), total = frames + lat; // Latenz herausrechnen wie turbo-render
    std::vector<float> inL(total), inR(total), outL(total), outR(total);
    for (uint32_t i = 0; i < total; ++i)
        inL[i] = demoSample(i, 0), inR[i] = demoSample(i, 1);
    for (uint32_t pos = 0; pos < total; pos += 256)
        turbo.process(&inL[pos], &inR[pos], &outL[pos], &outR[pos], std::min<uint32_t>(256, total - pos));
    l.assign(outL.begin() + lat, outL.end());
    r.assign(outR.begin() + lat, outR.end());
}

} // namespace offbeat
