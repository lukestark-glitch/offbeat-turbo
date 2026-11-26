#include "Turbo.hpp"
#include "Preview.hpp"

#include <algorithm>


namespace offbeat {

// ---- TruePeak ---------------------------------------------------------------------------------

TruePeak::TruePeak()
{
    const float center = (float)(kTaps / 2 - 1); // Zwischenwerte zwischen hist[15] und hist[16]
    for (int k = 1; k <= 3; ++k) {
        float sum = 0.0f;
        for (int j = 0; j < kTaps; ++j) {
            const float x = (float)j - center - (float)k / 4.0f;
            const float sinc = std::fabs(x) < 1e-6f ? 1.0f : std::sin(kPi * x) / (kPi * x);
            const float half = (float)(kTaps / 2);
            const float w = std::fabs(x) >= half ? 0.0f
                          : 0.42f + 0.5f * std::cos(kPi * x / half) + 0.08f * std::cos(2.0f * kPi * x / half);
            h[k - 1][j] = sinc * w;
            sum += h[k - 1][j];
        }
        for (float& v : h[k - 1])
            v /= sum;
    }
}

void TruePeak::reset()
{
    for (float& v : hist)
        v = 0.0f;
    pos = 0;
}

float TruePeak::process(float x)
{
    hist[pos] = x;
    pos = (pos + 1) % kTaps; // pos zeigt jetzt auf das älteste Sample
    float peak = std::fabs(hist[(pos + kTaps / 2 - 1) % kTaps]);
    for (int k = 0; k < 3; ++k) {
        float y = 0.0f;
        for (int j = 0; j < kTaps; ++j)
            y += h[k][j] * hist[(pos + j) % kTaps];
        peak = std::fmax(peak, std::fabs(y));
    }
    return peak;
}

// ---- RmsMeter ---------------------------------------------------------------------------------

void RmsMeter::setSampleRate(float sr)
{
    blockLen = (int)(0.01f * sr);
    blocks.assign(300, 0.0);
    reset();
}

void RmsMeter::reset()
{
    std::fill(blocks.begin(), blocks.end(), 0.0);
    sum = current = 0.0;
    inBlock = index = 0;
}

void RmsMeter::add(float l, float r)
{
    current += 0.5 * ((double)l * l + (double)r * r);
    if (++inBlock < blockLen)
        return;
    sum += current - blocks[index];
    blocks[index] = current;
    index = (index + 1) % (int)blocks.size();
    current = 0.0;
    inBlock = 0;
}

float RmsMeter::db() const
{
    const double ms = std::max(sum, 0.0) / ((double)blockLen * blocks.size());
    return ms > 1e-12 ? (float)(10.0 * std::log10(ms)) : -120.0f;
}

// ---- Turbo ------------------------------------------------------------------------------------

Turbo::Turbo()
{
    applyDefaults(params);
    setSampleRate(48000.0f);
}

void Turbo::setSampleRate(float newSr)
{
    sr = newSr;
    lookahead = std::max(2, (int)std::lround(0.0015f * sr));
    const int delay = (int)latency() + 1;
    delayL.assign(delay, 0.0f);
    delayR.assign(delay, 0.0f);
    need.assign(lookahead, 1.0f);
    avg.assign(lookahead, 1.0f);
    outRms.setSampleRate(sr);
    inRms.setSampleRate(sr);
    peakBlocks.assign(300, 0.0f);
    reset();
}

void Turbo::reset()
{
    compEnv = 0.0f;
    autoDb = autoOutMs = autoInMs = 0.0f;
    autoInPeakDb = -120.0f;
    tpL.reset(), tpR.reset(), tpOutL.reset(), tpOutR.reset();
    std::fill(delayL.begin(), delayL.end(), 0.0f);
    std::fill(delayR.begin(), delayR.end(), 0.0f);
    std::fill(need.begin(), need.end(), 1.0f);
    std::fill(avg.begin(), avg.end(), 1.0f);
    delayPos = needPos = avgPos = 0;
    limEnv = 1.0f;
    outRms.reset();
    inRms.reset();
    std::fill(peakBlocks.begin(), peakBlocks.end(), 0.0f);
    blockPeak = 0.0f;
    blockGr = 1.0f;
    peakBlock = inPeakBlock = 0;
}

void Turbo::setParam(int id, float value)
{
    if (id < 0 || id >= kParamCount || (kParams[id].flags & kFlagOutput))
        return;
    params[id] = clampf(value, kParams[id].min, kParams[id].max);
}

void Turbo::processChain(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
{
    const DenormalGuard denormals; // Flush-to-Zero für diesen Block (Intel und Apple Silicon)
    const bool autoOn = params[kAuto] >= 0.5f;
    if (!autoOn)
        autoDb = 0.0f;
    const float outCoef = tauCoef(1000.0f, sr), inCoef = tauCoef(400.0f, sr);
    const float peakFall = 0.2f / sr;          // jüngstes Eingangs-Maximum sinkt um 0,2 dB/s: Breakdowns bis ~50 s bleiben eingefroren
    const float maxStep = 1.5f / sr;           // Auto ändert sich höchstens um 1,5 dB/s
    const float target = params[kTarget];
    const float boostFixed = std::pow(10.0f, params[kBoost] / 20.0f);
    const float ceiling = std::pow(10.0f, params[kCeiling] / 20.0f);

    // Glue: Schwelle −6 … −20 dBFS, Ratio 1:1 … 4:1, weiches Knie 6 dB. Make-up gleicht die Hälfte der
    // Absenkung aus, die ein Signal mit Spitzen bei 0 dBFS erfährt (Glue macht sonst nur leiser).
    const float glue = params[kGlue];
    const float threshold = -6.0f - 14.0f * glue, slope = 1.0f / (1.0f + 3.0f * glue) - 1.0f, knee = 6.0f;
    const float makeup = std::pow(10.0f, 0.5f * slope * threshold / 20.0f);
    const float atk = tauCoef(params[kCompAttack], sr), rel = tauCoef(params[kCompRelease], sr);

    // Clip: Einsatz 0 … 6 dB unter dem Ceiling, darüber tanh-Knie bis zum Ceiling.
    const float clipStart = ceiling * std::pow(10.0f, -6.0f * params[kClip] / 20.0f);
    const float clipRange = ceiling - clipStart;

    const float limRel = tauCoef(params[kLimRelease], sr);
    // 0,05 dB Reserve: die 4-fach-Schätzung liest echte Zwischen-Spitzen minimal zu niedrig
    const float detect = ceiling * 0.9943f;
    const int delayLen = (int)delayL.size();
    float worstGr = 1.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        inRms.add(inL[i], inR[i]);
        if (autoOn) {
            const float inMs = 0.5f * (inL[i] * inL[i] + inR[i] * inR[i]);
            autoInMs = inMs + inCoef * (autoInMs - inMs);
            const float inDb = 10.0f * std::log10(autoInMs + 1e-12f);
            autoInPeakDb = std::fmax(inDb, autoInPeakDb - peakFall);
            if (inDb > -50.0f && inDb > autoInPeakDb - 10.0f) {
                const float err = target - 10.0f * std::log10(autoOutMs + 1e-12f);
                autoDb = clampf(autoDb + clampf(0.5f * err / sr, -maxStep, maxStep), -12.0f, 12.0f);
            }
        }
        const float boost = autoOn ? std::pow(10.0f, (params[kBoost] + autoDb) / 20.0f) : boostFixed;
        float l = boost * inL[i], r = boost * inR[i];

        float compGain = 1.0f;
        if (glue > 0.0f) {
            const float level = std::fmax(std::fabs(l), std::fabs(r));
            compEnv = level + (level > compEnv ? atk : rel) * (compEnv - level);
            const float over = 20.0f * std::log10(compEnv + 1e-9f) - threshold;
            float gr = 0.0f;
            if (over >= 0.5f * knee)
                gr = slope * over;
            else if (over > -0.5f * knee)
                gr = slope * (over + 0.5f * knee) * (over + 0.5f * knee) / (2.0f * knee);
            compGain = std::pow(10.0f, gr / 20.0f);
            l *= compGain * makeup, r *= compGain * makeup;
        }

        if (clipRange > 1e-6f) {
            for (float* x : { &l, &r }) {
                const float a = std::fabs(*x);
                if (a > clipStart) {
                    const float y = clipStart + clipRange * std::tanh((a - clipStart) / clipRange);
                    *x = *x < 0.0f ? -y : y;
                }
            }
        }

        // Limiter: benötigte Absenkung aus der True-Peak-Schätzung (kommt TruePeak::kDelay Samples später),
        // Min-Hold über die Vorschau, Release-Glättung (bleibt immer ≤ Hold), Mittelung über die Vorschau.
        const float peak = std::fmax(tpL.process(l), tpR.process(r));
        need[needPos] = peak > detect ? detect / peak : 1.0f;
        needPos = (needPos + 1) % lookahead;
        float hold = 1.0f;
        for (float v : need)
            hold = std::fmin(hold, v);
        limEnv = hold < limEnv ? hold : hold + limRel * (limEnv - hold);
        avg[avgPos] = limEnv;
        avgPos = (avgPos + 1) % lookahead;
        float gain = 0.0f;
        for (float v : avg)
            gain += v;
        gain /= (float)lookahead;

        delayL[delayPos] = l;
        delayR[delayPos] = r;
        delayPos = (delayPos + 1) % delayLen;
        float ol = gain * delayL[delayPos], or_ = gain * delayR[delayPos]; // ältestes Sample
        ol = clampf(ol, -ceiling, ceiling); // Sicherheitsnetz: kein Sample über das Ceiling
        or_ = clampf(or_, -ceiling, ceiling);
        outL[i] = ol;
        outR[i] = or_;

        outRms.add(ol, or_);
        const float outMs = 0.5f * (ol * ol + or_ * or_);
        autoOutMs = outMs + outCoef * (autoOutMs - outMs);
        blockPeak = std::fmax(blockPeak, std::fmax(tpOutL.process(ol), tpOutR.process(or_)));
        worstGr = std::fmin(worstGr, compGain * gain);
        if (++inPeakBlock >= (int)(0.01f * sr)) {
            peakBlocks[peakBlock] = blockPeak;
            peakBlock = (peakBlock + 1) % (int)peakBlocks.size();
            blockPeak = 0.0f;
            inPeakBlock = 0;
        }
    }

    float maxPeak = blockPeak;
    for (float p : peakBlocks)
        maxPeak = std::fmax(maxPeak, p);
    params[kOutRms] = clampf(outRms.db(), -60.0f, 0.0f);
    params[kInRms] = clampf(inRms.db(), -60.0f, 0.0f);
    // Gain-Reduction: fällt sofort, steigt über ~300 ms zurück (lesbar in der Anzeige)
    const float grDb = 20.0f * std::log10(worstGr + 1e-9f);
    grShown = grDb < grShown ? grDb : grShown + (grDb - grShown) * std::fmin(1.0f, (float)frames / (0.3f * sr));
    params[kGainReduction] = clampf(grShown, -30.0f, 0.0f);
    params[kOutPeak] = clampf(20.0f * std::log10(maxPeak + 1e-9f), -60.0f, 6.0f);
    params[kAutoGain] = autoDb;
}

// ---- Vorhören ---------------------------------------------------------------------------------

void Turbo::startPreview(int code)
{
    if (code == 2)
        reset(); // „nachher“ startet mit frischer Kette (wie ein neu eingefügtes Turbo)
    previewActive = code;
    previewPending = 0;
    previewPos = 0;
    previewLen = params[kPreviewBar] >= 0.5f ? kDemoFrames : kDemoBeat;
    previewFade = 0.0f;
    previewFadeStep = 1.0f / (0.003f * sr);
}

void Turbo::updatePreview(const float* inL, const float* inR, uint32_t frames)
{
    const int request = (int)params[kPreview];
    if (!previewSeen) { // Wert beim Laden eines Projekts: nur merken, nicht abspielen
        previewSeen = true;
        lastPreview = request;
    } else if (request != lastPreview) {
        lastPreview = request;
        const int code = request & 7;
        if (code == 1 || code == 2) {
            if (previewActive) { // läuft schon: erst ausblenden, dann umschalten (kein Pegelsprung)
                previewPending = code;
                previewFadeStep = -1.0f / (0.003f * sr);
            } else {
                startPreview(code);
            }
        } else if (previewActive) {
            previewPending = 0;
            previewFadeStep = -1.0f / (0.003f * sr); // ausblenden, dann aus
        }
    }
    if (!previewActive)
        return;
    for (uint32_t i = 0; i < frames; ++i) // Host-Audio hat Vorrang
        if (std::fabs(inL[i]) > 1e-4f || std::fabs(inR[i]) > 1e-4f) {
            previewActive = 0;
            return;
        }
}

void Turbo::process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
{
    updatePreview(inL, inR, frames);
    if (!previewActive) {
        processChain(inL, inR, outL, outR, frames);
        params[kPreviewActive] = 0.0f;
        return;
    }
    // Demo-Loop statt Eingang (nachher: durch die Kette; vorher: unbearbeitet), in Stücken von 256
    for (uint32_t done = 0; done < frames;) {
        const uint32_t n = std::min<uint32_t>(256, frames - done);
        for (uint32_t i = 0; i < n; ++i) {
            const bool playing = previewPos < previewLen;
            demoL[i] = playing ? demoSample(previewPos, 0) : 0.0f;
            demoR[i] = playing ? demoSample(previewPos, 1) : 0.0f;
            if (++previewPos >= previewLen && params[kPreviewLoop] >= 0.5f)
                previewPos = 0;
        }
        if (previewActive == 2)
            processChain(demoL, demoR, outL + done, outR + done, n);
        else
            std::copy(demoL, demoL + n, outL + done), std::copy(demoR, demoR + n, outR + done);
        for (uint32_t i = 0; i < n; ++i) {
            previewFade = clampf(previewFade + previewFadeStep, 0.0f, 1.0f);
            outL[done + i] *= previewFade;
            outR[done + i] *= previewFade;
        }
        done += n;
        if (previewPending && previewFade <= 0.0f)
            startPreview(previewPending); // ausgeblendet: jetzt umschalten und wieder einblenden
    }
    // Ende: ausgeblendet, oder Demo ohne Loop fertig (plus Latenz zum Ausklingen)
    if ((previewFadeStep < 0.0f && previewFade <= 0.0f && !previewPending) || (params[kPreviewLoop] < 0.5f && previewPos >= previewLen + latency())) {
        previewActive = 0;
        reset(); // sonst käme der Demo-Rest aus der Limiter-Verzögerung ungedämpft heraus
    }
    params[kPreviewActive] = (float)previewActive;
}

} // namespace offbeat
