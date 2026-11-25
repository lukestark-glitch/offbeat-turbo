// Messwerkzeuge der Offline-Renderer: WAV lesen/schreiben, Tonhöhe, Pegel, Spektrum, Korrelation.
// Quelle: offbeat-engine/tools/Measure.hpp – in die anderen Repos kopiert, Änderungen dort zurückspielen.
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace measure {

inline int parseNote(const char* s)
{
    if (std::isdigit((unsigned char)s[0]) || s[0] == '-')
        return std::atoi(s);
    static const int pcs[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A..G
    const int c = std::toupper((unsigned char)s[0]);
    if (c < 'A' || c > 'G')
        return -1;
    int pc = pcs[c - 'A'];
    ++s;
    if (*s == '#')
        ++pc, ++s;
    else if (*s == 'b')
        --pc, ++s;
    return (std::atoi(s) + 1) * 12 + pc; // F1 = 29, A4 = 69
}

inline bool writeWav(const char* path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = std::fopen(path, "wb");
    if (!f)
        return false;
    const uint32_t frames = (uint32_t)l.size(), dataBytes = frames * 8;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f), u32(36 + dataBytes), std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16), u16(3), u16(2), u32((uint32_t)sr), u32((uint32_t)sr * 8), u16(8), u16(32); // float32 stereo
    std::fwrite("data", 1, 4, f), u32(dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
        std::fwrite(&l[i], 4, 1, f), std::fwrite(&r[i], 4, 1, f);
    return std::fclose(f) == 0;
}

inline float toDb(float x) { return 20.0f * std::log10(x > 1e-12f ? x : 1e-12f); }

// Tonhöhe bei Zeit t: eine Periode aus den Nulldurchgängen rund um t.
inline float pitchAt(const std::vector<float>& x, int sr, float tMs)
{
    std::vector<double> zc;
    for (size_t i = 1; i < x.size(); ++i)
        if ((x[i - 1] < 0.0f) != (x[i] < 0.0f))
            zc.push_back((double)(i - 1) + x[i - 1] / (double)(x[i - 1] - x[i]));
    const double t = tMs * 0.001 * sr;
    for (size_t k = 1; k + 1 < zc.size(); ++k)
        if (zc[k] <= t && t < zc[k + 1])
            return (float)(sr / (zc[k + 1] - zc[k - 1]));
    return 0.0f;
}

inline float peakIn(const std::vector<float>& x, int sr, float fromMs, float toMs)
{
    float p = 0.0f;
    const size_t a = (size_t)(fromMs * 0.001f * sr), b = (size_t)(toMs * 0.001f * sr);
    for (size_t i = a; i < b && i < x.size(); ++i)
        p = std::fmax(p, std::fabs(x[i]));
    return p;
}

inline float rmsIn(const std::vector<float>& x, int sr, float fromMs, float toMs)
{
    double sum = 0.0;
    size_t a = (size_t)(fromMs * 0.001f * sr), b = (size_t)(toMs * 0.001f * sr), n = 0;
    for (size_t i = a; i < b && i < x.size(); ++i, ++n)
        sum += (double)x[i] * x[i];
    return n ? (float)std::sqrt(sum / n) : 0.0f;
}

// Zeit, nach der die Spitze in 1-ms-Fenstern zuletzt über `threshold` liegt.
inline float lengthTo(const std::vector<float>& x, int sr, float threshold)
{
    size_t last = 0;
    for (size_t i = 0; i < x.size(); ++i)
        if (std::fabs(x[i]) > threshold)
            last = i;
    return (float)last * 1000.0f / sr;
}

inline void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> w = std::polar(1.0, -2.0 * M_PI / (double)len);
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> wk = 1.0;
            for (size_t k = 0; k < len / 2; ++k, wk *= w) {
                const std::complex<double> u = a[i + k], v = a[i + k + len / 2] * wk;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
}

inline const float kBandHz[9] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

struct Spectrum {
    float bands[9];   // Oktavband-Energie in dB
    float centroid;   // Hz
    float above10k;   // Energieanteil über 10 kHz in dB
};

// Spektrum eines Abschnitts (Hann-Fenster, höchstens 2^17 Samples ab `from`).
inline Spectrum spectrum(const std::vector<float>& x, int sr, size_t from, size_t to)
{
    to = std::min(to, x.size());
    const size_t len = std::min<size_t>(to > from ? to - from : 0, 1 << 17);
    size_t n = 1024;
    while (n < len)
        n <<= 1;
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < len; ++i)
        a[i] = x[from + i] * (0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)len));
    fft(a);
    Spectrum s {};
    double band[9] = {}, total = 0.0, high = 0.0, weighted = 0.0;
    for (size_t k = 1; k < n / 2; ++k) {
        const double f = (double)k * sr / n, e = std::norm(a[k]);
        total += e;
        weighted += e * f;
        if (f > 10000.0)
            high += e;
        for (int b = 0; b < 9; ++b)
            if (f >= kBandHz[b] / M_SQRT2 && f < kBandHz[b] * M_SQRT2)
                band[b] += e;
    }
    for (int b = 0; b < 9; ++b)
        s.bands[b] = (float)(10.0 * std::log10(band[b] + 1e-30));
    s.centroid = total > 0.0 ? (float)(weighted / total) : 0.0f;
    s.above10k = (float)(10.0 * std::log10(high / (total + 1e-30) + 1e-30));
    return s;
}

// Korrelation L/R aus dem Spektrum, nur Anteile unter `maxHz` (0 = alle).
// Gibt 2 zurück, wenn dort kaum Energie liegt (unter −40 dB des Gesamtsignals).
inline float correlation(const std::vector<float>& l, const std::vector<float>& r, float maxHz, int sr)
{
    const size_t len = std::min<size_t>(l.size(), 1 << 20);
    size_t n = 1024;
    while (n < len)
        n <<= 1;
    std::vector<std::complex<double>> a(n), b(n);
    for (size_t i = 0; i < len; ++i)
        a[i] = l[i], b[i] = r[i];
    fft(a);
    fft(b);
    double lr = 0, ll = 0, rr = 0, total = 0;
    for (size_t k = 1; k < n / 2; ++k) {
        total += std::norm(a[k]) + std::norm(b[k]);
        if (maxHz > 0.0f && (double)k * sr / n >= maxHz)
            continue;
        lr += std::real(a[k] * std::conj(b[k]));
        ll += std::norm(a[k]);
        rr += std::norm(b[k]);
    }
    if (ll + rr < 1e-4 * total || ll <= 0.0 || rr <= 0.0)
        return 2.0f;
    return (float)(lr / std::sqrt(ll * rr));
}

// WAV lesen (PCM 16/24/32 Bit oder Float 32, mono oder stereo). Mono wird auf beide Seiten gelegt.
inline bool readWav(const char* path, std::vector<float>& l, std::vector<float>& r, int& sr)
{
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return false;
    std::vector<unsigned char> d;
    unsigned char buf[65536];
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;)
        d.insert(d.end(), buf, buf + n);
    std::fclose(f);
    auto u16 = [&](size_t i) { return (uint32_t)d[i] | (uint32_t)d[i + 1] << 8; };
    auto u32 = [&](size_t i) { return u16(i) | u16(i + 2) << 16; };
    if (d.size() < 12 || std::memcmp(&d[0], "RIFF", 4) || std::memcmp(&d[8], "WAVE", 4))
        return false;
    int format = 0, channels = 0, bits = 0;
    for (size_t i = 12; i + 8 <= d.size();) {
        const uint32_t size = u32(i + 4);
        const size_t body = i + 8;
        if (!std::memcmp(&d[i], "fmt ", 4) && body + 16 <= d.size()) {
            format = (int)u16(body);
            channels = (int)u16(body + 2);
            sr = (int)u32(body + 4);
            bits = (int)u16(body + 14);
            if (format == 0xFFFE && body + 26 <= d.size())
                format = (int)u16(body + 24); // WAVE_FORMAT_EXTENSIBLE: Untertyp
        } else if (!std::memcmp(&d[i], "data", 4) && channels > 0 && bits > 0) {
            const size_t bytes = bits / 8, frame = bytes * channels;
            const size_t frames = std::min<size_t>(size, d.size() - body) / frame;
            l.resize(frames), r.resize(frames);
            for (size_t k = 0; k < frames; ++k)
                for (int c = 0; c < 2; ++c) {
                    const size_t at = body + k * frame + (c < channels ? c : 0) * bytes;
                    float v;
                    if (format == 3 && bits == 32)
                        std::memcpy(&v, &d[at], 4);
                    else if (bits == 16)
                        v = (float)(int16_t)u16(at) / 32768.0f;
                    else if (bits == 24)
                        v = (float)((int32_t)(u32(at) << 8) >> 8) / 8388608.0f;
                    else if (bits == 32)
                        v = (float)(int32_t)u32(at) / 2147483648.0f;
                    else
                        return false;
                    (c == 0 ? l : r)[k] = v;
                }
            return true;
        }
        i = body + size + (size & 1);
    }
    return false;
}

} // namespace measure
