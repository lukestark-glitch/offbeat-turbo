// DSP-Kern von Offbeat Turbo: Boost → Bus-Kompressor (Glue) → Soft-Clipper → True-Peak-Limiter.
// Ohne DPF, damit turbo-render denselben Code nutzt. Puffer entstehen in setSampleRate().
#pragma once

#include "Blocks.hpp"
#include "Params.hpp"

#include <vector>

namespace offbeat {

// Schätzt Spitzen zwischen den Samples: 4-fach-Interpolation (Polyphasen-Sinc, 32 Taps pro Phase;
// kürzere Filter lesen Spitzen nahe der Nyquist-Frequenz – wie sie der Clipper erzeugt – zu niedrig).
// Liefert für das Sample, das kDelay Samples zurückliegt, das Maximum aus ihm und den drei
// Zwischenwerten bis zum nächsten Sample.
class TruePeak {
public:
    static constexpr int kTaps = 32, kDelay = kTaps / 2;
    TruePeak();
    void reset();
    float process(float x);

private:
    float h[3][kTaps];
    float hist[kTaps] = {};
    int pos = 0;
};

// Gleitender Mittelwert der Leistung über ~3 s (in 10-ms-Blöcken).
class RmsMeter {
public:
    void setSampleRate(float sr);
    void reset();
    void add(float l, float r);
    float db() const;

private:
    std::vector<double> blocks;
    double sum = 0.0, current = 0.0;
    int blockLen = 480, inBlock = 0, index = 0;
};

class Turbo {
public:
    Turbo();

    void setSampleRate(float sr);
    void setParam(int id, float value);
    float getParam(int id) const { return params[id]; }

    // Host-Audio durch die Kette. Läuft das Vorhören (Parameter preview), kommt stattdessen der Demo-Loop
    // heraus (vorher bzw. nachher); Host-Audio am Eingang hat Vorrang und beendet das Vorhören.
    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames);

    // Einmal pro Block: läuft der Host-Transport? Play beendet ein laufendes Vorhören, und solange der
    // Host spielt, startet keins.
    void setTransport(bool playing);

    // Verzögerung durch Look-ahead und Interpolator, in Samples.
    uint32_t latency() const { return (uint32_t)(lookahead - 1 + TruePeak::kDelay); }

private:
    void processChain(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames);
    void updatePreview(const float* inL, const float* inR, uint32_t frames);
    void startPreview(int code);
    void reset();

    float params[kParamCount];
    float sr = 48000.0f;

    // Vorhören
    bool previewSeen = false;
    bool transportRunning = false;
    int lastPreview = 0, previewActive = 0, previewPending = 0; // Pending: nach dem Ausblenden umschalten
    uint32_t previewPos = 0, previewLen = 0;
    float previewFade = 0.0f, previewFadeStep = 0.0f; // weiche Ein-/Ausblendung (3 ms)
    float demoL[256], demoR[256];

    // Kompressor
    float compEnv = 0.0f;

    // Auto-Lautheit: zusätzliche Verstärkung in dB, regelt den Ausgang (τ 1 s) langsam auf Target.
    // Friert ein bei Stille und in Breakdowns (Eingang > 10 dB unter seinem jüngsten Maximum).
    float autoDb = 0.0f, autoOutMs = 0.0f, autoInMs = 0.0f, autoInPeakDb = -120.0f;

    // Limiter
    int lookahead = 72;
    TruePeak tpL, tpR, tpOutL, tpOutR;
    std::vector<float> delayL, delayR, need, avg;
    int delayPos = 0, needPos = 0, avgPos = 0;
    float limEnv = 1.0f;

    // Anzeige
    RmsMeter outRms, inRms;
    std::vector<float> peakBlocks;
    float blockPeak = 0.0f, blockGr = 1.0f, grShown = 0.0f;
    int peakBlock = 0, inPeakBlock = 0;
};

} // namespace offbeat
