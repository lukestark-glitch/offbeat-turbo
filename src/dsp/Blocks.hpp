// DSP-Bausteine, die alle Modi gemeinsam nutzen. Kein DPF, kein Heap, echtzeitsicher.
#pragma once

#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace offbeat {

constexpr float kPi = 3.14159265358979f;

// Schaltet für die Lebensdauer des Objekts Flush-to-Zero ein (Intel: SSE-MXCSR, ARM/Apple Silicon:
// FPCR-Bit FZ). Sehr kleine Zahlen (Denormals) in ausklingenden Filtern und Hall kosten sonst viel CPU.
// Am Anfang jedes process() anlegen; beim Verlassen wird der alte Zustand wiederhergestellt.
class DenormalGuard {
public:
    DenormalGuard()
    {
#if defined(__SSE__) || defined(__x86_64__)
        old = _mm_getcsr();
        _mm_setcsr(old | 0x8040); // Flush-to-Zero + Denormals-are-Zero
#elif defined(__aarch64__)
        __asm__ __volatile__("mrs %0, fpcr" : "=r"(old));
        __asm__ __volatile__("msr fpcr, %0" : : "r"(old | (1ull << 24)));
#endif
    }
    ~DenormalGuard()
    {
#if defined(__SSE__) || defined(__x86_64__)
        _mm_setcsr(old);
#elif defined(__aarch64__)
        __asm__ __volatile__("msr fpcr, %0" : : "r"(old));
#endif
    }
    DenormalGuard(const DenormalGuard&) = delete;
    DenormalGuard& operator=(const DenormalGuard&) = delete;

private:
#if defined(__aarch64__)
    uint64_t old = 0;
#else
    unsigned int old = 0;
#endif
};

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

inline float midiToHz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }

// Koeffizient für exponentiellen Abfall, der nach `ms` bei −40 dB (Faktor 0,01) ankommt.
inline float decayCoef(float ms, float sr) { return std::exp(-4.605f / (ms * 0.001f * sr)); }

// Koeffizient für exponentiellen Abfall mit Zeitkonstante `ms`.
inline float tauCoef(float ms, float sr) { return std::exp(-1.0f / (ms * 0.001f * sr)); }

// Weicher Ausgangsbegrenzer: linear bis 0,8, darüber sanft gegen 0,99 (−0,1 dBFS).
inline float softLimit(float x)
{
    const float a = std::fabs(x);
    if (a <= 0.8f)
        return x;
    const float y = 0.8f + 0.19f * std::tanh((a - 0.8f) / 0.19f);
    return x < 0.0f ? -y : y;
}

// tanh-Sättigung, Vollaussteuerung bleibt bei 1. amount 0..1.
inline float saturate(float x, float amount)
{
    if (amount <= 0.0f)
        return x;
    const float g = 1.0f + 9.0f * amount;
    return std::tanh(g * x) / std::tanh(g);
}

// Weißes Rauschen (xorshift32), −1..1.
struct Noise {
    uint32_t state = 0x12345678u;
    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float)(int32_t)state * (1.0f / 2147483648.0f);
    }
};

inline float polyBlep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Bandbegrenzter Oszillator mit stufenloser Überblendung Saw (0) ↔ Square (1).
struct Osc {
    float phase = 0.0f;
    float process(float hz, float sr, float squareMix)
    {
        const float dt = clampf(hz / sr, 0.0f, 0.5f);
        const float saw = 2.0f * phase - 1.0f - polyBlep(phase, dt);
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        const float sq = (phase < 0.5f ? 1.0f : -1.0f) + polyBlep(phase, dt) - polyBlep(p2, dt);
        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;
        return saw + squareMix * (sq - saw);
    }
};

struct Sine {
    float phase = 0.0f;
    float process(float hz, float sr)
    {
        const float y = std::sin(2.0f * kPi * phase);
        phase += hz / sr;
        if (phase >= 1.0f)
            phase -= std::floor(phase);
        return y;
    }
};

// Naives Dreieck (Obertöne fallen mit 1/n², Aliasing unkritisch).
struct Tri {
    float phase = 0.0f;
    float process(float hz, float sr)
    {
        const float y = 4.0f * std::fabs(phase - 0.5f) - 1.0f;
        phase += hz / sr;
        if (phase >= 1.0f)
            phase -= std::floor(phase);
        return y;
    }
};

// TPT State-Variable-Filter (Zavalishin/Simper), 12 dB/Okt.
struct Svf {
    float ic1 = 0.0f, ic2 = 0.0f;
    float g = 0.0f, k = 1.414f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float lp = 0.0f, bp = 0.0f, hp = 0.0f;

    // resonance 0..1 → Dämpfung k von 2 (keine Resonanz) bis 0,05.
    void set(float hz, float sr, float resonance = 0.29f)
    {
        g = std::tan(kPi * clampf(hz, 10.0f, 0.45f * sr) / sr);
        k = 2.0f - 1.95f * clampf(resonance, 0.0f, 1.0f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void process(float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
};

// Einpoliger Glätter für Parameter und Gates.
struct Smoother {
    float value = 0.0f, coef = 0.0f;
    void setTime(float ms, float sr) { coef = tauCoef(ms, sr); }
    float process(float target) { return value = target + coef * (value - target); }
};

// ADSR: linearer Anstieg vom aktuellen Pegel, exponentieller Abfall (Zeiten bis −40 dB).
struct Adsr {
    enum Stage { kIdle, kAttack, kDecay, kRelease };
    Stage stage = kIdle;
    float level = 0.0f;

    void gateOn() { stage = kAttack; }
    void gateOff()
    {
        if (stage != kIdle)
            stage = kRelease;
    }
    bool active() const { return stage != kIdle; }
    float process(float aMs, float dMs, float sustain, float rMs, float sr)
    {
        switch (stage) {
        case kAttack:
            level += 1.0f / (aMs * 0.001f * sr + 1.0f);
            if (level >= 1.0f) {
                level = 1.0f;
                stage = kDecay;
            }
            break;
        case kDecay:
            level = sustain + decayCoef(dMs, sr) * (level - sustain);
            break;
        case kRelease:
            level *= decayCoef(rMs, sr);
            if (level < 1e-4f) {
                level = 0.0f;
                stage = kIdle;
            }
            break;
        case kIdle:
            break;
        }
        return level;
    }
};

// Stereo-Chorus: zwei modulierte Verzögerungen (12 ± 5 ms), links/rechts gegenphasig.
struct Chorus {
    static constexpr int kSize = 8192; // reicht für 17 ms bis 192 kHz
    float buf[2][kSize] = {};
    int write = 0;
    float phase = 0.0f;

    void reset()
    {
        for (auto& ch : buf)
            for (float& v : ch)
                v = 0.0f;
    }
    float read(const float* b, float delaySamples) const
    {
        float pos = (float)write - delaySamples;
        while (pos < 0.0f)
            pos += kSize;
        const int i = (int)pos;
        const float frac = pos - (float)i;
        return b[i % kSize] + frac * (b[(i + 1) % kSize] - b[i % kSize]);
    }
    void process(float& l, float& r, float mix, float sr)
    {
        buf[0][write] = l;
        buf[1][write] = r;
        const float m = std::sin(2.0f * kPi * phase);
        phase += 0.35f / sr;
        if (phase >= 1.0f)
            phase -= 1.0f;
        const float wl = read(buf[0], (0.012f + 0.005f * m) * sr);
        const float wr = read(buf[1], (0.012f - 0.005f * m) * sr);
        write = (write + 1) % kSize;
        l += mix * (wl - l) * 0.5f;
        r += mix * (wr - r) * 0.5f;
    }
};

} // namespace offbeat
