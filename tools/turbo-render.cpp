// turbo-render: rechnet eine WAV-Datei durch Offbeat Turbo und gibt die Lautheit vorher/nachher aus.
//
//   turbo-render --in export.wav [--out master.wav] [--preset NAME|NR] [--set boost=8] [--json datei]
//   turbo-render --check-all     (künstlicher Drop + Grenzfälle gegen die Ziele aus docs/PLAN.md, wie make check)
//   turbo-render --make-drop drop.wav   (Test-Drop der Abnahme als WAV)
//   turbo-render --list
#include "Measure.hpp"
#include "ParamText.hpp"
#include "Turbo.hpp"

#include <cfloat>
#include <string>
#include <vector>

using namespace offbeat;
using namespace measure;

namespace {

int findPreset(const char* s)
{
    for (int i = 0; i < kPresetCount; ++i)
        if (std::strcmp(kPresets[i].name, s) == 0)
            return i;
    char* end;
    const long i = std::strtol(s, &end, 10);
    return *end == 0 && i >= 0 && i < kPresetCount ? (int)i : -1;
}

int findParam(const std::string& symbol)
{
    for (int i = 0; i < kParamCount; ++i)
        if (symbol == kParams[i].symbol)
            return i;
    return -1;
}

struct Audio {
    std::vector<float> l, r;
    int sr = 48000;
};

// True-Peak unabhängig vom Plugin gemessen: 8-fach-Interpolation, 32 Taps pro Phase (Blackman-Sinc).
float truePeakDb(const Audio& a)
{
    const int up = 8, taps = 32;
    float h[up][taps];
    for (int k = 0; k < up; ++k) {
        float sum = 0.0f;
        for (int j = 0; j < taps; ++j) {
            const double x = j - (taps / 2 - 1) - (double)k / up;
            const double sinc = std::fabs(x) < 1e-9 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
            const double w = std::fabs(x) >= taps / 2 ? 0.0
                           : 0.42 + 0.5 * std::cos(M_PI * x / (taps / 2)) + 0.08 * std::cos(2 * M_PI * x / (taps / 2));
            h[k][j] = (float)(sinc * w);
            sum += h[k][j];
        }
        for (float& v : h[k])
            v /= sum;
    }
    float peak = 0.0f;
    for (const std::vector<float>* ch : { &a.l, &a.r }) {
        const std::vector<float>& x = *ch;
        for (size_t n = taps; n < x.size(); ++n)
            for (int k = 0; k < up; ++k) {
                float y = 0.0f;
                for (int j = 0; j < taps; ++j)
                    y += h[k][j] * x[n - taps + j];
                peak = std::fmax(peak, std::fabs(y));
            }
    }
    return toDb(peak);
}

float samplePeakDb(const Audio& a)
{
    float p = 0.0f;
    for (size_t i = 0; i < a.l.size(); ++i)
        p = std::fmax(p, std::fmax(std::fabs(a.l[i]), std::fabs(a.r[i])));
    return toDb(p);
}

float rmsDb(const Audio& a)
{
    double s = 0.0;
    for (size_t i = 0; i < a.l.size(); ++i)
        s += 0.5 * ((double)a.l[i] * a.l[i] + (double)a.r[i] * a.r[i]);
    return a.l.empty() ? -120.0f : (float)(10.0 * std::log10(s / a.l.size() + 1e-24));
}

// Lautestes 3-s-Fenster (Schritt 100 ms) – entspricht „RMS im Drop“.
float loudestDb(const Audio& a)
{
    const size_t win = (size_t)(3.0f * a.sr), hop = (size_t)(0.1f * a.sr);
    if (a.l.size() <= win)
        return rmsDb(a);
    std::vector<double> sq(a.l.size() + 1, 0.0);
    for (size_t i = 0; i < a.l.size(); ++i)
        sq[i + 1] = sq[i] + 0.5 * ((double)a.l[i] * a.l[i] + (double)a.r[i] * a.r[i]);
    double best = 0.0;
    for (size_t s = 0; s + win <= a.l.size(); s += hop)
        best = std::max(best, (sq[s + win] - sq[s]) / win);
    return (float)(10.0 * std::log10(best + 1e-24));
}

Audio process(const Audio& in, const float* values, std::vector<float>* autoTrace = nullptr)
{
    Turbo turbo;
    turbo.setSampleRate((float)in.sr);
    for (int i = 0; i < kParamCount; ++i)
        turbo.setParam(i, values[i]);
    Audio out { std::vector<float>(in.l.size()), std::vector<float>(in.l.size()), in.sr };
    for (size_t pos = 0; pos < in.l.size(); pos += 256) {
        const uint32_t n = (uint32_t)std::min<size_t>(256, in.l.size() - pos);
        turbo.process(&in.l[pos], &in.r[pos], &out.l[pos], &out.r[pos], n);
        if (autoTrace)
            autoTrace->push_back(turbo.getParam(kAutoGain)); // ein Wert pro 256 Samples
    }
    // Latenz herausrechnen, damit vorher/nachher übereinander liegen
    const size_t lat = turbo.latency();
    out.l.erase(out.l.begin(), out.l.begin() + std::min(lat, out.l.size()));
    out.r.erase(out.r.begin(), out.r.begin() + std::min(lat, out.r.size()));
    return out;
}

void report(const char* what, const Audio& a)
{
    std::printf("  %-8s RMS gesamt %6.2f dBFS | lautestes 3-s-Fenster %6.2f dBFS | Sample-Peak %6.2f dBFS | True-Peak %6.2f dBTP\n",
                what, rmsDb(a), loudestDb(a), samplePeakDb(a), truePeakDb(a));
}

// Künstlicher Hard-Bounce-Drop, 160 BPM, 8 Takte: Kick auf jedem Schlag, Offbeat-Bass, Hats, Akkorde.
Audio makeDrop(int sr, float rmsTarget)
{
    const float beatSec = 60.0f / 160.0f, len = 32 * beatSec;
    Audio a { std::vector<float>((size_t)(len * sr)), std::vector<float>((size_t)(len * sr)), sr };
    Noise noise;
    float lp = 0.0f, kickPhase = 0.0f, bassPhase = 0.0f;
    float chordPhase[3] = {};
    const float chordHz[3] = { midiToHz(65), midiToHz(68), midiToHz(72) }; // F4 As4 C5
    for (size_t i = 0; i < a.l.size(); ++i) {
        const float t = (float)i / sr, beatPos = t / beatSec, inBeat = (beatPos - std::floor(beatPos)) * beatSec;
        // Kick: Pitch 150 → 45 Hz, ~180 ms
        const float kHz = 45.0f + 105.0f * std::exp(-inBeat / 0.03f);
        kickPhase += kHz / sr;
        const float kick = inBeat < 0.18f ? 0.9f * std::sin(2 * kPi * kickPhase) * std::fmin(1.0f, inBeat / 0.001f) * (1.0f - inBeat / 0.18f) : 0.0f;
        // Offbeat-Bass F2
        const float off = inBeat - 0.5f * beatSec;
        bassPhase += 87.31f / sr;
        bassPhase -= std::floor(bassPhase);
        lp += 0.08f * ((2.0f * bassPhase - 1.0f) - lp);
        const float bass = (off >= 0.0f && off < 0.12f) ? 0.5f * lp * std::fmin(1.0f, off / 0.002f) * (1.0f - off / 0.12f) : 0.0f;
        // Hats auf jedem 16tel
        const float six = std::fmod(inBeat, 0.25f * beatSec);
        const float hat = 0.1f * noise.next() * std::exp(-six / 0.012f);
        // Akkord-Stabs auf der 2. und 4. Achtel
        float chord = 0.0f;
        for (int c = 0; c < 3; ++c) {
            chordPhase[c] += chordHz[c] / sr;
            chordPhase[c] -= std::floor(chordPhase[c]);
            chord += 2.0f * chordPhase[c] - 1.0f;
        }
        chord *= (off >= 0.0f && off < 0.15f) ? 0.06f * (1.0f - off / 0.15f) : 0.0f;
        a.l[i] = kick + bass + hat + chord;
        a.r[i] = kick + bass - 0.5f * hat + 0.8f * chord;
    }
    const float gain = std::pow(10.0f, (rmsTarget - rmsDb(a)) / 20.0f);
    for (size_t i = 0; i < a.l.size(); ++i)
        a.l[i] *= gain, a.r[i] *= gain;
    return a;
}

int checkAll()
{
    int failed = 0;
    auto expect = [&](const std::string& what, bool ok) {
        std::printf("  [%s] %s\n", ok ? "OK " : "FEHLER", what.c_str());
        failed += ok ? 0 : 1;
    };
    char buf[200];
    const Audio drop = makeDrop(48000, -16.0f);
    std::puts("== Künstlicher Drop (160 BPM, 8 Takte), Eingang:");
    report("vorher", drop);

    for (int p = 0; p < kPresetCount; ++p) {
        float values[kParamCount];
        applyPreset(p, values);
        const Audio out = process(drop, values);
        const float loud = loudestDb(out), tp = truePeakDb(out);
        std::printf("== Preset %s\n", kPresets[p].name);
        report("nachher", out);
        std::snprintf(buf, sizeof(buf), "True-Peak ≤ Ceiling %.1f dBTP (+0,05) (gemessen %.2f)", values[kCeiling], tp);
        expect(buf, tp <= values[kCeiling] + 0.05f);
        if (values[kBoost] > 0.0f) {
            std::snprintf(buf, sizeof(buf), "Drop-RMS −9 … −6 dBFS und ±1 dB um Target %.1f (gemessen %.2f)", values[kTarget], loud);
            expect(buf, loud >= -9.0f && loud <= -6.0f && std::fabs(loud - values[kTarget]) <= 1.0f);
            std::snprintf(buf, sizeof(buf), "Spitzen knapp unter 0: True-Peak ≥ −1,5 dBTP (gemessen %.2f)", tp);
            expect(buf, tp >= -1.5f);
        }
    }

    // Auto: leiser und lauter Mix regeln sich auf Target; im Breakdown friert Auto ein.
    auto scaled = [](const Audio& a, float db, int repeat) {
        Audio o { {}, {}, a.sr };
        const float g = std::pow(10.0f, db / 20.0f);
        for (int k = 0; k < repeat; ++k)
            for (size_t i = 0; i < a.l.size(); ++i)
                o.l.push_back(g * a.l[i]), o.r.push_back(g * a.r[i]);
        return o;
    };
    auto lastWindow = [](const Audio& a, float seconds) {
        const size_t n = (size_t)(seconds * a.sr);
        Audio o { std::vector<float>(a.l.end() - n, a.l.end()), std::vector<float>(a.r.end() - n, a.r.end()), a.sr };
        return o;
    };
    for (float inDb : { -6.0f, 4.0f }) {
        float values[kParamCount];
        applyPreset(0, values);
        values[kAuto] = 1.0f;
        std::vector<float> trace;
        const Audio out = process(scaled(drop, inDb, 3), values, &trace);
        const Audio tail = lastWindow(out, 6.0f);
        std::printf("== Auto, Mix %+.0f dB gegenüber dem Test-Drop (RMS %.0f dBFS), 36 s\n", inDb, -16.0f + inDb);
        report("Ende", tail);
        std::snprintf(buf, sizeof(buf), "Auto trifft Target %.1f ±1 dB in den letzten 6 s (gemessen %.2f, Auto %+.1f dB)",
                      values[kTarget], loudestDb(tail), trace.back());
        expect(buf, std::fabs(loudestDb(tail) - values[kTarget]) <= 1.0f);
        std::snprintf(buf, sizeof(buf), "True-Peak ≤ Ceiling (gemessen %.2f)", truePeakDb(out));
        expect(buf, truePeakDb(out) <= values[kCeiling] + 0.05f);
    }
    {
        float values[kParamCount];
        applyPreset(0, values);
        values[kAuto] = 1.0f;
        Audio song = scaled(drop, -4.0f, 2);                       // 24 s Drop
        const Audio breakdown = scaled(drop, -24.0f, 1);          // 12 s Breakdown, 20 dB leiser
        const Audio again = scaled(drop, -4.0f, 1);
        song.l.insert(song.l.end(), breakdown.l.begin(), breakdown.l.end());
        song.r.insert(song.r.end(), breakdown.r.begin(), breakdown.r.end());
        song.l.insert(song.l.end(), again.l.begin(), again.l.end());
        song.r.insert(song.r.end(), again.r.begin(), again.r.end());
        std::vector<float> trace;
        process(song, values, &trace);
        const size_t perSec = 48000 / 256;
        const float before = trace[24 * perSec - 1], during = trace[36 * perSec - 1];
        std::printf("== Auto, Drop 24 s → Breakdown −20 dB 12 s → Drop: Auto vor %.1f dB, am Ende des Breakdowns %.1f dB\n", before, during);
        std::snprintf(buf, sizeof(buf), "Auto zieht im Breakdown nicht hoch (Änderung ≤ 1 dB, gemessen %+.2f)", during - before);
        expect(buf, std::fabs(during - before) <= 1.0f);
    }

    // Grenzfall: Sinus bei sr/4 mit 45°-Phase → Samples bei ±0,707 der Spitze, True-Peak 3 dB über Sample-Peak.
    std::puts("== Grenzfall: Zwischen-Sample-Spitzen (sr/4, 45°), Sample-Peak 0 dBFS, Boost +12 dB, Nur Limiter");
    Audio isp { std::vector<float>(48000), std::vector<float>(48000), 48000 };
    for (size_t i = 0; i < isp.l.size(); ++i)
        isp.l[i] = isp.r[i] = 1.41421f * std::sin(kPi / 2 * (float)i + kPi / 4) * std::fmin(1.0f, (float)i / 480.0f);
    float values[kParamCount];
    applyPreset(3, values);
    values[kBoost] = 12.0f;
    const Audio ispOut = process(isp, values);
    report("vorher", isp);
    report("nachher", ispOut);
    std::snprintf(buf, sizeof(buf), "True-Peak ≤ −0,3 dBTP (+0,05) (gemessen %.2f)", truePeakDb(ispOut));
    expect(buf, truePeakDb(ispOut) <= -0.25f);

    std::puts("== Grenzfall: Stille");
    const Audio silence { std::vector<float>(48000, 0.0f), std::vector<float>(48000, 0.0f), 48000 };
    applyPreset(2, values);
    const Audio silOut = process(silence, values);
    float maxAbs = 0.0f;
    int bad = 0;
    for (size_t i = 0; i < silOut.l.size(); ++i) {
        maxAbs = std::fmax(maxAbs, std::fmax(std::fabs(silOut.l[i]), std::fabs(silOut.r[i])));
        bad += (!std::isfinite(silOut.l[i]) || (silOut.l[i] != 0.0f && std::fabs(silOut.l[i]) < FLT_MIN)) ? 1 : 0;
    }
    expect("Stille bleibt Stille, kein NaN/Denormal", maxAbs == 0.0f && bad == 0);

    std::puts("== Latenz");
    Turbo t;
    t.setSampleRate(48000.0f);
    std::vector<float> imp(2048, 0.0f), o1(2048), o2(2048);
    imp[100] = 0.1f;
    t.process(imp.data(), imp.data(), o1.data(), o2.data(), 2048);
    size_t at = 0;
    for (size_t i = 0; i < o1.size(); ++i)
        if (std::fabs(o1[i]) > std::fabs(o1[at]))
            at = i;
    std::snprintf(buf, sizeof(buf), "Gemeldete Latenz %u Samples = gemessene %d Samples", t.latency(), (int)at - 100);
    expect(buf, (int)at - 100 == (int)t.latency());

    std::printf("%s\n", failed ? "NICHT BESTANDEN" : "BESTANDEN");
    return failed ? 1 : 0;
}

void usage()
{
    std::puts("turbo-render --in datei.wav [--out datei.wav] [--preset NAME|NR] [--json datei] [--set symbol=wert]...\n"
              "turbo-render --check-all | --make-drop datei.wav | --list");
}

} // namespace

int main(int argc, char** argv)
{
    int preset = 0;
    const char *in = nullptr, *out = nullptr, *jsonFile = nullptr;
    std::vector<std::pair<int, float>> overrides;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : nullptr;
        if (a == "--check-all")
            return checkAll();
        if (a == "--make-drop" && v) { // Test-Drop der Abnahme als WAV (RMS −16 dBFS), z. B. zum Vergleichen
            const Audio d = makeDrop(48000, -16.0f);
            return writeWav(v, d.l, d.r, d.sr) ? 0 : 1;
        }
        if (a == "--list") {
            std::puts("Presets:");
            for (int p = 0; p < kPresetCount; ++p)
                std::printf("  %d  %s\n", p, kPresets[p].name);
            std::puts("Parameter (symbol: min..max, default):");
            for (const ParamInfo& p : kParams)
                if (!(p.flags & kFlagOutput))
                    std::printf("  %-14s %g..%g %s, %g\n", p.symbol, p.min, p.max, p.unit, p.def);
            return 0;
        }
        if (!v) {
            usage();
            return 2;
        }
        ++i;
        if (a == "--in")
            in = v;
        else if (a == "--out")
            out = v;
        else if (a == "--json")
            jsonFile = v;
        else if (a == "--preset") {
            if ((preset = findPreset(v)) < 0) {
                std::fprintf(stderr, "Unbekanntes Preset: %s\n", v);
                return 2;
            }
        } else if (a == "--set") {
            const std::string kv = v;
            const size_t eq = kv.find('=');
            const int id = eq == std::string::npos ? -1 : findParam(kv.substr(0, eq));
            if (id < 0) {
                std::fprintf(stderr, "Unbekannter Parameter: %s\n", v);
                return 2;
            }
            overrides.emplace_back(id, (float)std::atof(kv.c_str() + eq + 1));
        } else {
            usage();
            return 2;
        }
    }
    if (!in) {
        usage();
        return 2;
    }
    Audio a;
    if (!readWav(in, a.l, a.r, a.sr)) {
        std::fprintf(stderr, "Konnte %s nicht lesen (WAV mit 16/24/32 Bit oder Float erwartet)\n", in);
        return 1;
    }
    float values[kParamCount];
    applyPreset(preset, values);
    if (jsonFile) {
        std::string json, errors;
        if (FILE* f = std::fopen(jsonFile, "rb")) {
            char buf[4096];
            for (size_t r; (r = std::fread(buf, 1, sizeof(buf), f)) > 0;)
                json.append(buf, r);
            std::fclose(f);
        }
        bool changed[kParamCount] = {};
        std::printf("JSON: %d Werte gesetzt%s%s\n", applyJson(json.c_str(), values, changed, errors),
                    errors.empty() ? "" : "; ", errors.c_str());
    }
    for (const auto& o : overrides)
        values[o.first] = o.second;

    std::printf("%s (%d Hz, %.1f s), Preset %s, Boost %+.1f dB, Ceiling %.1f dB\n", in, a.sr, (float)a.l.size() / a.sr,
                kPresets[preset].name, values[kBoost], values[kCeiling]);
    const Audio o = process(a, values);
    report("vorher", a);
    report("nachher", o);
    if (out) {
        if (!writeWav(out, o.l, o.r, o.sr)) {
            std::fprintf(stderr, "Konnte %s nicht schreiben\n", out);
            return 1;
        }
        std::printf("WAV: %s\n", out);
    }
    return 0;
}
