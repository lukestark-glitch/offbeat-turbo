// Oberfläche von Offbeat Turbo: großer Ladedruck-Regler (Boost), Lautheitsanzeige mit Zielzone,
// Turbolader und die übrigen Regler. Look und Bedienung: OffbeatKit.hpp (aus offbeat-engine).
#include "OffbeatKit.hpp"
#include "TurboImage.h" // aus assets/turbo.png erzeugt (xxd -i), siehe plugin/Makefile

START_NAMESPACE_DISTRHO

using namespace offbeat;
using kit::kCellH;
using kit::kCellW;
using kit::kLabelH;

namespace {
constexpr float kW = 860, kH = 560, kMargin = 20, kGridX = 31;
constexpr float kTopY = 104, kTopH = 300, kKnobsY = 416;
constexpr float kMeterX = 452, kBarX = 560, kBarW = 200;

const int kKnobs[7] = { kGlue, kCompAttack, kCompRelease, kClip, kCeiling, kLimRelease, kTarget };

kit::Theme turboTheme()
{
    kit::Theme t;
    t.title = "OFFBEAT";
    t.subtitle = "TURBO";
    t.brand = Color(90, 255, 40);
    t.brandInk = Color(20, 95, 0);
    t.night = Color(2, 6, 2);
    t.night2 = Color(8, 42, 10);
    t.light = Color(120, 255, 90);
    t.plate = Color(6, 18, 6);
    t.track = Color(25, 70, 20);
    t.text = Color(235, 255, 230);
    t.dim = Color(160, 220, 150);
    return t;
}
} // namespace

class TurboUI : public OffbeatKitUI {
public:
    TurboUI()
        : OffbeatKitUI(kW, kH, turboTheme())
    {
        turbo = createImageFromMemory(turbo_png, turbo_png_len, IMAGE_GENERATE_MIPMAPS);
    }

protected:
    void drawContent() override
    {
        drawLogo(145, 50);

        drawPanel(kMargin, kTopY, 420, kTopH, "LADEDRUCK", th.brand);
        bigKnob(kBoost, kMargin + 210, kTopY + kLabelH + 140, 125, th.brand);
        compact(kAuto, 150, kTopY + 3, 130, th.brand); // Manuell ↔ Auto (regelt zusätzlich auf Target)
        if (values[kAuto] >= 0.5f) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "AUTO %+.1f dB", values[kAutoGain]);
            fontSize(12);
            textLetterSpacing(1.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(th.brand);
            text(kMargin + 14, kTopY + kTopH - 14, buf, nullptr);
            textLetterSpacing(0);
        }

        drawPanel(kMeterX, kTopY, kW - kMargin - kMeterX, kTopH, "LAUTHEIT", th.brand);
        drawImage(turbo, kW - 300, 6, 260, 130); // ragt aus dem Kopf in die Anzeige
        drawMeters();

        drawPanel(kMargin, kKnobsY, kW - 2 * kMargin, kLabelH + kCellH, "REGLER", th.brand);
        for (int i = 0; i < 7; ++i)
            knob(kKnobs[i], kGridX + i * kCellW, kKnobsY + kLabelH, th.brand);
        copyPaste(kW - kMargin - 12, kKnobsY + 3);
    }

    const char* controlLabel(int p) const override
    {
        switch (p) {
        case kCompAttack: return "Attack";
        case kCompRelease: return "Release";
        case kLimRelease: return "Lim. Release";
        case kBoost: return "Boost";
        case kAuto: return "Auto";
        default: return kParams[p].name;
        }
    }

private:
    NanoImage turbo;

    // Ein Balken: Wert von lo bis hi (dB), optional Zielzone und Marke.
    void bar(float y, float h, const char* label, float value, float lo, float hi, const Color& c,
             float zoneLo = 1, float zoneHi = 0, float mark = 1e9f, bool fromRight = false)
    {
        auto xOf = [&](float db) { return kBarX + kBarW * std::fmin(1.0f, std::fmax(0.0f, (db - lo) / (hi - lo))); };
        fontSize(10);
        textLetterSpacing(1);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(th.dim);
        text(kMeterX + 14, y + h / 2, label, nullptr);
        textLetterSpacing(0);

        beginPath();
        roundedRect(kBarX, y, kBarW, h, 3);
        fillColor(th.track);
        fill();
        if (zoneLo < zoneHi) { // Zielzone
            beginPath();
            rect(xOf(zoneLo), y, xOf(zoneHi) - xOf(zoneLo), h);
            fillColor(kit::withAlpha(c, 0.25f));
            fill();
        }
        const float x0 = fromRight ? xOf(value) : kBarX, x1 = fromRight ? kBarX + kBarW : xOf(value);
        if (x1 > x0) {
            beginPath();
            roundedRect(x0, y + 2, x1 - x0, h - 4, 2);
            fillPaint(linearGradient(kBarX, 0, kBarX + kBarW, 0, kit::withAlpha(c, 0.55f), c));
            fill();
        }
        if (mark < 1e8f) {
            beginPath();
            moveTo(xOf(mark), y - 3);
            lineTo(xOf(mark), y + h + 3);
            strokeWidth(2);
            strokeColor(th.text);
            stroke();
        }
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.1f dB", value);
        fontSize(h > 20 ? 15 : 12);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(th.text);
        text(kW - kMargin - 12, y + h / 2, buf, nullptr);
    }

    // Ist-Lautheit (Ausgang-RMS über 3 s) gegen Zielzone −9 … −6 dBFS und Target-Marke.
    void drawMeters()
    {
        const float out = values[kOutRms], target = values[kTarget];
        const bool inZone = out >= -9.0f && out <= -6.0f;
        const bool autoOn = values[kAuto] >= 0.5f;
        const char* state = out < -40.0f ? "KEIN SIGNAL"
                          : out < -9.0f  ? (autoOn ? "AUTO REGELT HOCH" : "ZU LEISE: BOOST HOCH")
                          : out > -6.0f  ? (autoOn ? "AUTO REGELT RUNTER" : "SEHR LAUT")
                                         : "IM ZIEL";
        fontSize(12);
        textLetterSpacing(2);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(inZone ? th.brand : th.text);
        text(kMeterX + 14, kTopY + 74, state, nullptr);
        textLetterSpacing(0);

        bar(kTopY + 96, 34, "AUSGANG RMS", out, -20.0f, 0.0f, th.brand, -9.0f, -6.0f, target);
        bar(kTopY + 150, 20, "EINGANG RMS", values[kInRms], -20.0f, 0.0f, th.dim);
        bar(kTopY + 190, 20, "GAIN RED.", values[kGainReduction], -12.0f, 0.0f, Color(255, 200, 40), 1, 0, 1e9f, true);
        bar(kTopY + 230, 20, "TRUE PEAK", values[kOutPeak], -20.0f, 1.0f, th.brand, 1, 0, values[kCeiling]);
        fontSize(9);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(th.dim);
        text(kMeterX + 14, kTopY + 276, "Zielzone -9 bis -6 dBFS RMS (3 s)  \xc2\xb7  Strich = Target", nullptr);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TurboUI)
};

UI* createUI()
{
    return new TurboUI();
}

END_NAMESPACE_DISTRHO
