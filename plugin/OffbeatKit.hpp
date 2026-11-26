// OffbeatKit: gemeinsamer Look und gemeinsame Bedienung aller Offbeat-Plugins (DPF + NanoVG).
// Quelle: offbeat-engine/plugin/OffbeatKit.hpp – in die anderen Repos kopiert, Änderungen dort zurückspielen.
//
// Das Plugin leitet von OffbeatKitUI ab und zeichnet in drawContent() mit den Bausteinen
// (Logo, Abschnitt, Regler, kompakte Regler, Knöpfe, Bild). Jeder Baustein meldet beim Zeichnen
// seine Klickfläche an; Maus und Tastatur arbeiten mit den Flächen des letzten Bildes.
//
// Bedienung: ziehen (hoch/runter), Shift = fein, Mausrad, Doppelklick = Standardwert,
// Schalter per Klick, Klick auf den Wert (oder Rechtsklick) = Zahl eintippen,
// KOPIEREN/EINFÜGEN = Einstellungen als JSON über die Zwischenablage.
#pragma once

#include "DistrhoPluginUtils.hpp"
#include "DistrhoStandaloneUtils.hpp"
#include "DistrhoUI.hpp"
#include "ParamText.hpp"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef OFFBEAT_VERSION
#define OFFBEAT_VERSION "dev" // kommt aus plugin/Makefile (git describe)
#endif

START_NAMESPACE_DISTRHO

namespace kit {
using namespace offbeat;

constexpr float kCellW = 114, kCellH = 112, kLabelH = 22, kPi = 3.14159265f;

inline Color withAlpha(Color c, float a)
{
    c.alpha = a;
    return c;
}

inline void upper(const char* in, char* out, size_t size)
{
    size_t i = 0;
    for (; in[i] && i + 1 < size; ++i)
        out[i] = (char)std::toupper((unsigned char)in[i]);
    out[i] = 0;
}

// Läuft das Plugin als eigene App (DPF-Standalone, Ton über CoreAudio/RtAudio ohne JACK)?
inline bool isStandaloneApp()
{
    return std::strstr(DISTRHO_NAMESPACE::getPluginFormatName(), "Standalone") != nullptr;
}

// Farben und Schriftzug eines Plugins.
struct Theme {
    const char* title;    // großer Schriftzug im Logo, z. B. "OFFBEAT"
    const char* subtitle; // klein darunter, z. B. "ENGINE"
    Color brand;          // Swoosh, Rahmen, globale Regler
    Color brandInk;       // Schrift auf der Chrom-Plakette
    Color night, night2;  // Hintergrund oben → unten
    Color light;          // Lochblech und Lichtstreifen
    Color plate = Color(10, 14, 46), track = Color(32, 40, 100);
    Color text = Color(232, 236, 255), dim = Color(150, 162, 225);
    Color chromeHi = Color(245, 247, 255), chromeLo = Color(120, 126, 155);
};
} // namespace kit

class OffbeatKitUI : public UI {
public:
    OffbeatKitUI(uint width, uint height, const kit::Theme& theme)
        : UI(width, height, true)
        , uiW((float)width)
        , uiH((float)height)
        , th(theme)
    {
        offbeat::applyDefaults(values);
        loadSharedResources();
    }

protected:
    enum { kButtonCopy = -1, kButtonPaste = -2,
           kButtonPreview = -10 /* -10 … -12: Ebene 1–3 */, kButtonPreviewAll = -13, kButtonPreviewLoop = -14,
           kButtonPreviewRange = -15, kButtonOctaveDown = -20, kButtonOctaveUp = -21,
           kButtonKey = -200 /* -200 … -224: Taste 0–24 der Bildschirm-Tastatur */ };

    // ---- vom Plugin zu füllen -----------------------------------------------------------------

    virtual void drawContent() = 0;
    virtual void onButton(int /*id*/) {}
    virtual const char* controlLabel(int p) const { return offbeat::kParams[p].name; }

    // ---- Vorschau (Wellenform + Vorhören), optional ------------------------------------------------
    // Ein Plugin mit Vorschau nennt seine Parameter und füllt in renderPreview() die Ebenen.
    struct PreviewParams {
        int play = -1;   // Vorhör-Wunsch: Bit 0–2 Ebene (1–3, 4 = alle, 0 = Stopp), Bit 3 wechselt je Klick
        int loop = -1;   // Schalter Loop
        int bar = -1;    // Schalter Ausschnitt: aus = 1 Schlag, an = 1 Takt
        int active = -1; // Anzeige vom DSP: welche Ebene gerade spielt
        int bpm = -1;    // Anzeige vom DSP: aktuelles Tempo (für die Schlag- und Offbeat-Linien)
        const char* barLabels[2] = { "1 Schlag", "1 Takt" }; // Beschriftung des Umschalters (aus / an)
        int beats[2] = { 1, 4 };   // gezeigte Schläge (aus / an), für die Schlag- und Offbeat-Linien
        bool allButton = true;     // „Alle“-Knopf bei mehreren Ebenen
        bool offbeatLines = true;  // Offbeat-Linien zeichnen
    };
    struct PreviewLayer {
        std::string name;
        std::vector<float> mono; // gerendert mit dem echten DSP
    };
    virtual PreviewParams previewParams() const { return PreviewParams(); }
    // Octave-Regler des Plugins (für die Bildschirm-Tastatur: Tasten klingen wie beschriftet), −1 = keiner
    virtual int octaveParam() const { return -1; }
    // Ebenen neu rendern (UI-Thread, gedrosselt, nie im Audio-Thread): previewLayers füllen und in previewSum
    // die Summe („alle zusammen“, eigens gerendert). Länge = Ausschnitt.
    virtual void renderPreview() {}
    std::vector<PreviewLayer> previewLayers;
    std::vector<float> previewSum;
    // Optional: Kurve 0..1 über der Wellenform (z. B. Pump-Kurve von Hydraulics; oben = 1), leer = keine
    std::vector<float> previewCurve;

    // ---- Bausteine ------------------------------------------------------------------------------

    float values[offbeat::kParamCount];
    const float uiW, uiH;
    const kit::Theme th;

    // Verlauf, Lochblech-Raster und zwei schräge Lichtstreifen wie in einer Garage.
    void drawBackground()
    {
        beginPath();
        rect(0, 0, uiW, uiH);
        fillPaint(linearGradient(0, 0, 0, uiH, th.night, th.night2));
        fill();

        beginPath();
        for (int y = 9; y < uiH; y += 16)
            for (int x = 9 + (y / 16 % 2) * 8; x < uiW; x += 16)
                circle(x, y, 1.1f);
        fillColor(kit::withAlpha(th.light, 0.07f));
        fill();

        for (int i = 0; i < 2; ++i) {
            const float x0 = i == 0 ? 60.0f : uiW - 60.0f, dx = i == 0 ? 220.0f : -220.0f;
            beginPath();
            moveTo(x0, 0);
            lineTo(x0 + 26 * (i ? -1 : 1), 0);
            lineTo(x0 + dx + 60 * (i ? -1 : 1), uiH);
            lineTo(x0 + dx, uiH);
            closePath();
            fillPaint(linearGradient(0, 0, 0, uiH, kit::withAlpha(th.light, 0.10f), kit::withAlpha(th.light, 0.0f)));
            fill();
        }
    }

    // Logo: Chrom-Plakette mit kursivem Schriftzug, umkreist von einem Swoosh in der Markenfarbe.
    void drawLogo(float cx, float cy)
    {
        auto swoosh = [&](float a0, float a1) {
            save();
            translate(cx, cy);
            rotate(-0.12f);
            scale(1.0f, 0.3f);
            beginPath();
            arc(0, 0, 132, a0, a1, CW);
            restore();
            lineCap(ROUND);
            strokeColor(kit::withAlpha(th.brand, 0.35f));
            strokeWidth(13);
            stroke();
            strokeColor(th.brand);
            strokeWidth(6);
            stroke();
            lineCap(BUTT);
        };
        swoosh(kit::kPi * 1.08f, kit::kPi * 1.92f); // hinterer Bogen

        beginPath();
        roundedRect(cx - 105, cy - 25, 210, 50, 12);
        fillPaint(linearGradient(0, cy - 25, 0, cy + 25, th.chromeHi, th.chromeLo));
        fill();
        strokeColor(th.brand);
        strokeWidth(3);
        stroke();

        save();
        translate(cx - 4, cy + 1);
        transform(1.0f, 0.0f, std::tan(-0.28f), 1.0f, 0.0f, 0.0f); // kursiv; skewX() erlaubt in DPF nur positive Winkel
        fontSize(31);
        textLetterSpacing(3);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fontBlur(5);
        fillColor(kit::withAlpha(th.brand, 0.45f));
        text(0, 0, th.title, nullptr);
        fontBlur(0);
        fillColor(th.brandInk);
        text(0, 0, th.title, nullptr);
        restore();
        textLetterSpacing(0);

        swoosh(kit::kPi * 0.08f, kit::kPi * 0.92f); // vorderer Bogen

        fontSize(9);
        textLetterSpacing(2);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(th.text);
        text(cx + 105, cy + 36, th.subtitle, nullptr);
        textLetterSpacing(0);
        fontSize(8); // Versionsnummer klein links unter der Plakette
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(kit::withAlpha(th.dim, 0.7f));
        text(cx - 105, cy + 36, OFFBEAT_VERSION, nullptr);
    }

    // Abschnitt im Ticket-Stil: dunkle Plakette, dünner Neonrand, Kopfzeile mit Linie.
    void drawPanel(float x, float y, float w, float h, const char* label, const Color& c)
    {
        beginPath();
        roundedRect(x, y, w, h, 10);
        fillColor(kit::withAlpha(th.plate, 0.78f));
        fill();
        strokeColor(kit::withAlpha(c, 0.55f));
        strokeWidth(1);
        stroke();

        fontSize(11);
        textLetterSpacing(3);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(c);
        const float tw = text(x + 14, y + 13, label, nullptr) - (x + 14);
        textLetterSpacing(0);
        beginPath();
        moveTo(x + 24 + tw, y + 13);
        lineTo(x + w - 14, y + 13);
        strokeColor(kit::withAlpha(c, 0.35f));
        stroke();
    }

    // Kleines Etikett rechts in einer Kopfzeile (Ticket-Stil).
    void drawTag(float xRight, float y, const char* tag)
    {
        fontSize(9);
        textLetterSpacing(1.5f);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        Rectangle<float> bounds;
        textBounds(xRight, y, tag, nullptr, bounds);
        beginPath();
        rect(bounds.getX() - 10, y - 6, bounds.getWidth() + 10, 12);
        fillColor(th.plate);
        fill();
        fillColor(th.dim);
        text(xRight, y, tag, nullptr);
        textLetterSpacing(0);
    }

    void drawImage(const NanoImage& image, float x, float y, float w, float h)
    {
        if (!image.isValid())
            return;
        beginPath();
        rect(x, y, w, h);
        fillPaint(imagePattern(x, y, w, h, 0, image, 1.0f));
        fill();
    }

    // Regler in einer Zelle (kCellW × kCellH, x/y = linke obere Ecke): Neon-Bogen mit Glühen um
    // eine Chrom-Kappe; Schalter als leuchtende Pille; darunter Name und Wert.
    void knob(int p, float x, float top, const Color& c)
    {
        addHit(p, -100, x, top, kit::kCellW, kit::kCellH, kKnob, top + 82);
        const offbeat::ParamInfo& info = offbeat::kParams[p];
        const float cx = x + kit::kCellW / 2, cy = top + 36;

        if (info.flags & offbeat::kFlagBool) {
            const bool on = values[p] > 0.5f;
            if (on) {
                beginPath();
                rect(cx - 36, cy - 24, 72, 48);
                fillPaint(boxGradient(cx - 24, cy - 12, 48, 24, 12, 10, kit::withAlpha(c, 0.6f), kit::withAlpha(c, 0.0f)));
                fill();
            }
            beginPath();
            roundedRect(cx - 24, cy - 12, 48, 24, 12);
            fillColor(on ? c : th.track);
            fill();
            beginPath();
            circle(on ? cx + 12 : cx - 12, cy, 9);
            fillPaint(linearGradient(0, cy - 9, 0, cy + 9, th.chromeHi, th.chromeLo));
            fill();
        } else {
            const float n = std::fmin(1.0f, std::fmax(0.0f, toNorm(p, values[p])));
            const float a0 = 0.75f * kit::kPi, a1 = a0 + 1.5f * kit::kPi * n, r = 26;
            lineCap(ROUND);
            beginPath();
            arc(cx, cy, r, a0, 2.25f * kit::kPi, CW);
            strokeWidth(5);
            strokeColor(th.track);
            stroke();
            if (n > 0.001f) {
                beginPath();
                arc(cx, cy, r, a0, a1, CW);
                strokeWidth(11);
                strokeColor(kit::withAlpha(c, 0.25f));
                stroke();
                strokeWidth(4);
                strokeColor(c);
                stroke();
            }

            beginPath(); // Chrom-Kappe mit Fase
            circle(cx, cy, 18);
            fillPaint(linearGradient(0, cy - 18, 0, cy + 18, th.chromeHi, Color(70, 76, 110)));
            fill();
            beginPath();
            circle(cx, cy, 14.5f);
            fillPaint(linearGradient(0, cy - 15, 0, cy + 15, Color(160, 166, 195), th.chromeHi));
            fill();

            beginPath();
            moveTo(cx + 5 * std::cos(a1), cy + 5 * std::sin(a1));
            lineTo(cx + 13 * std::cos(a1), cy + 13 * std::sin(a1));
            strokeWidth(3);
            strokeColor(th.night);
            stroke();
            lineCap(BUTT);
        }

        char label[40];
        kit::upper(controlLabel(p), label, sizeof(label));
        fontSize(10);
        textLetterSpacing(1);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(th.dim);
        text(cx, top + 76, label, nullptr);
        textLetterSpacing(0);
        drawValue(p, cx, top + 93, 88, c);
    }

    // Großer Rundregler im Anzeigen-Look (z. B. Ladedruck): Skala mit 5 beschrifteten Strichen,
    // leuchtender Bogen, Zeiger, Chrom-Nabe; Name und Wert groß in der Mitte unten.
    void bigKnob(int p, float cx, float cy, float r, const Color& c)
    {
        addHit(p, -100, cx - r, cy - r, 2 * r, 2 * r, kKnob, cy + 0.45f * r);
        const float n = std::fmin(1.0f, std::fmax(0.0f, toNorm(p, values[p])));
        const float a0 = 0.75f * kit::kPi, a1 = a0 + 1.5f * kit::kPi * n;

        beginPath(); // Zifferblatt
        circle(cx, cy, r);
        fillPaint(radialGradient(cx, cy - 0.2f * r, 0.1f * r, r, kit::withAlpha(c, 0.10f), th.night));
        fill();
        strokeColor(kit::withAlpha(c, 0.6f));
        strokeWidth(2);
        stroke();

        lineCap(ROUND);
        beginPath();
        arc(cx, cy, 0.82f * r, a0, 2.25f * kit::kPi, CW);
        strokeWidth(0.07f * r);
        strokeColor(th.track);
        stroke();
        if (n > 0.001f) {
            beginPath();
            arc(cx, cy, 0.82f * r, a0, a1, CW);
            strokeWidth(0.16f * r);
            strokeColor(kit::withAlpha(c, 0.22f));
            stroke();
            strokeWidth(0.07f * r);
            strokeColor(c);
            stroke();
        }
        lineCap(BUTT);

        fontSize(0.1f * r);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        for (int i = 0; i <= 20; ++i) { // Skala: 20 Teilstriche, jeder fünfte beschriftet
            const float a = a0 + 1.5f * kit::kPi * (float)i / 20.0f, major = i % 5 == 0;
            beginPath();
            moveTo(cx + (major ? 0.6f : 0.66f) * r * std::cos(a), cy + (major ? 0.6f : 0.66f) * r * std::sin(a));
            lineTo(cx + 0.72f * r * std::cos(a), cy + 0.72f * r * std::sin(a));
            strokeWidth(major ? 2.0f : 1.0f);
            strokeColor(kit::withAlpha(th.text, major ? 0.8f : 0.4f));
            stroke();
            if (major) {
                char tick[16];
                std::snprintf(tick, sizeof(tick), "%g", std::round(fromNorm(p, (float)i / 20.0f) * 10.0f) / 10.0f);
                fillColor(th.dim);
                text(cx + 0.48f * r * std::cos(a), cy + 0.48f * r * std::sin(a), tick, nullptr);
            }
        }

        beginPath(); // Zeiger
        moveTo(cx - 0.05f * r * std::cos(a1), cy - 0.05f * r * std::sin(a1));
        lineTo(cx + 0.7f * r * std::cos(a1), cy + 0.7f * r * std::sin(a1));
        strokeWidth(4);
        lineCap(ROUND);
        strokeColor(c);
        stroke();
        lineCap(BUTT);
        beginPath(); // Chrom-Nabe
        circle(cx, cy, 0.12f * r);
        fillPaint(linearGradient(0, cy - 0.12f * r, 0, cy + 0.12f * r, th.chromeHi, th.chromeLo));
        fill();

        char label[40];
        kit::upper(controlLabel(p), label, sizeof(label));
        fontSize(11);
        textLetterSpacing(2);
        fillColor(th.dim);
        text(cx, cy + 0.36f * r, label, nullptr);
        textLetterSpacing(0);
        if (p == editParam) {
            drawValue(p, cx, cy + 0.58f * r, 100, c);
            return;
        }
        char buf[32];
        formatValue(p, buf, sizeof(buf));
        fontSize(0.17f * r);
        fillColor(th.text);
        text(cx, cy + 0.58f * r, buf, nullptr);
    }

    // Kompakter Regler als Pille (Name links, Wert rechts) für Kopfzeilen: ziehen, Mausrad,
    // Klick ohne Ziehen = Zahl eintippen; Schalter per Klick.
    void compact(int p, float x, float y, float w, const Color& c)
    {
        addHit(p, -100, x, y, w, 20, kCompact, y + 20);
        const bool isBool = offbeat::kParams[p].flags & offbeat::kFlagBool;
        const bool on = isBool && values[p] > 0.5f;
        beginPath();
        roundedRect(x, y, w, 20, 10);
        fillColor(on ? kit::withAlpha(c, 0.85f) : th.plate);
        fill();
        strokeColor(kit::withAlpha(c, 0.7f));
        strokeWidth(1.2f);
        stroke();

        char label[40];
        kit::upper(controlLabel(p), label, sizeof(label));
        fontSize(9);
        textLetterSpacing(1);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(on ? th.night : th.dim);
        text(x + 10, y + 10.5f, label, nullptr);
        textLetterSpacing(0);
        if (p == editParam) {
            drawValue(p, x + w - 34, y + 10, 56, c);
            return;
        }
        char buf[32];
        formatValue(p, buf, sizeof(buf));
        fontSize(11);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(on ? th.night : th.text);
        text(x + w - 10, y + 10.5f, buf, nullptr);
    }

    // Pillen-Knopf; id wird an onButton() gemeldet.
    void pill(int id, float x, float y, float w, float h, const char* label, const Color& c, bool active)
    {
        addHit(-1, id, x, y, w, h, kButton, y + h);
        if (active) {
            beginPath();
            rect(x - 12, y - 12, w + 24, h + 24);
            fillPaint(boxGradient(x, y, w, h, h / 2, 14, kit::withAlpha(c, 0.55f), kit::withAlpha(c, 0.0f)));
            fill();
        }
        beginPath();
        roundedRect(x, y, w, h, h / 2);
        fillColor(active ? c : kit::withAlpha(th.plate, 0.85f));
        fill();
        strokeColor(kit::withAlpha(c, active ? 1.0f : 0.6f));
        strokeWidth(1.5f);
        stroke();

        char text_[24];
        kit::upper(label, text_, sizeof(text_));
        float size = h > 24 ? 13.0f : 10.0f;
        fontSize(size);
        textLetterSpacing(1.5f);
        Rectangle<float> bounds; // zu lange Beschriftung verkleinern, bis sie in den Knopf passt
        textBounds(0, 0, text_, nullptr, bounds);
        if (bounds.getWidth() > w - 10.0f)
            fontSize(size * (w - 10.0f) / bounds.getWidth());
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(active ? th.night : th.text);
        text(x + w / 2, y + h / 2 + 1, text_, nullptr);
        textLetterSpacing(0);
    }

    // Vorschau: Wellenform (Summe gefüllt, Ebenen als Linien), Schlag- und Offbeat-Linien, Dichte (Crest) und
    // Spitze, rechts die Vorhör-Knöpfe (je Ebene, ALLE, LOOP, Ausschnitt). Farben aus dem Theme bzw. c.
    void drawPreview(float x, float y, float w, float h, const Color& c)
    {
        const PreviewParams pp = previewParams();
        if (pp.play < 0)
            return;
        const float bw = 92, wx = x, ww = w - 2 * bw - 22, wy = y, wh = h;
        beginPath();
        roundedRect(wx, wy, ww, wh, 6);
        fillColor(th.night);
        fill();
        strokeColor(kit::withAlpha(c, 0.35f));
        strokeWidth(1);
        stroke();

        const bool bar = pp.bar >= 0 && values[pp.bar] >= 0.5f;
        const int beats = pp.beats[bar ? 1 : 0];
        const float mid = wy + wh / 2, amp = wh / 2 - 4;
        for (int b = 0; b < beats; ++b) { // Schlag- (schwach) und Offbeat-Linien (hell)
            for (int half = 0; half < (pp.offbeatLines ? 2 : 1); ++half) {
                const float lx = wx + ww * ((float)b + 0.5f * half) / beats;
                if (lx <= wx + 1)
                    continue;
                beginPath();
                moveTo(lx, wy + 3);
                lineTo(lx, wy + wh - 3);
                strokeColor(kit::withAlpha(half ? c : th.text, half ? 0.55f : 0.15f));
                strokeWidth(half ? 1.5f : 1.0f);
                stroke();
            }
        }
        fontSize(8);
        textLetterSpacing(1);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(kit::withAlpha(c, 0.8f));
        if (pp.offbeatLines)
            text(wx + ww / beats * 0.5f + 4, wy + 4, "OFFBEAT", nullptr);
        textLetterSpacing(0);

        auto envelope = [&](const std::vector<float>& x, std::vector<float>& lo, std::vector<float>& hi) {
            const int cols = (int)ww;
            lo.assign(cols, 0.0f), hi.assign(cols, 0.0f);
            if (x.empty())
                return;
            for (int k = 0; k < cols; ++k) {
                const size_t a = x.size() * k / cols, e = std::max(a + 1, x.size() * (k + 1) / cols);
                float mn = 0.0f, mx = 0.0f;
                for (size_t i = a; i < e && i < x.size(); ++i)
                    mn = std::fmin(mn, x[i]), mx = std::fmax(mx, x[i]);
                lo[k] = mn, hi[k] = mx;
            }
        };
        std::vector<float> lo, hi;
        envelope(previewSum, lo, hi); // Summe: gefülltes Band mit Neon-Rand
        if (!lo.empty()) {
            beginPath();
            moveTo(wx, mid - amp * hi[0]);
            for (size_t k = 1; k < hi.size(); ++k)
                lineTo(wx + k, mid - amp * hi[k]);
            for (size_t k = lo.size(); k-- > 0;)
                lineTo(wx + k, mid - amp * lo[k]);
            closePath();
            fillColor(kit::withAlpha(c, 0.35f));
            fill();
            strokeColor(c);
            strokeWidth(1.2f);
            stroke();
        }
        for (size_t i = 0; i < previewLayers.size() && previewLayers.size() > 1; ++i) { // Ebenen als Linien
            envelope(previewLayers[i].mono, lo, hi);
            const Color lc = layerColor(c, (int)i);
            beginPath();
            moveTo(wx, mid - amp * hi[0]);
            for (size_t k = 1; k < hi.size(); ++k)
                lineTo(wx + k, mid - amp * hi[k]);
            strokeColor(kit::withAlpha(lc, 0.9f));
            strokeWidth(1.0f);
            stroke();
        }

        if (!previewCurve.empty()) { // Überlagerungs-Kurve (oben = 1, unten = 0)
            beginPath();
            const int cols = (int)ww;
            for (int k = 0; k < cols; ++k) {
                const float v = previewCurve[std::min(previewCurve.size() - 1, previewCurve.size() * k / cols)];
                const float cy = wy + 4 + (wh - 8) * (1.0f - std::fmin(1.0f, std::fmax(0.0f, v)));
                k ? lineTo(wx + k, cy) : moveTo(wx, cy);
            }
            strokeColor(th.text);
            strokeWidth(1.6f);
            stroke();
        }
        // Dichte (Crest) und Spitze der Summe
        double sq = 0.0;
        float peak = 0.0f;
        for (float v : previewSum)
            sq += (double)v * v, peak = std::fmax(peak, std::fabs(v));
        const float rms = previewSum.empty() ? 0.0f : (float)std::sqrt(sq / previewSum.size());
        char info[64];
        if (peak > 1e-6f && rms > 1e-9f)
            std::snprintf(info, sizeof(info), "DICHTE %.1f dB  \xc2\xb7  SPITZE %.1f dBFS", 20.0f * std::log10(peak / rms), 20.0f * std::log10(peak));
        else
            std::snprintf(info, sizeof(info), "STILLE");
        fontSize(9);
        textLetterSpacing(1);
        textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
        fillColor(th.text);
        text(wx + ww - 6, wy + wh - 4, info, nullptr);
        textLetterSpacing(0);

        // Vorhör-Knöpfe: Ebenen, ALLE (bei mehreren Ebenen), LOOP, Ausschnitt
        const int playing = pp.active >= 0 ? (int)values[pp.active] : 0;
        const float bx = wx + ww + 12, bh = 22, gap = 6;
        const int n = (int)previewLayers.size();
        for (int i = 0; i < n; ++i)
            pill(kButtonPreview - i, bx + (i % 2) * (bw + gap), y + (i / 2) * (bh + gap), bw, bh, previewLayers[i].name.c_str(),
                 layerColor(c, i), playing == i + 1);
        if (n > 1 && pp.allButton)
            pill(kButtonPreviewAll, bx + (n % 2) * (bw + gap), y + (n / 2) * (bh + gap), bw, bh, "Alle", c, playing == 4);
        const float rowY = y + h - bh;
        pill(kButtonPreviewLoop, bx, rowY, bw, bh, "Loop", c, pp.loop >= 0 && values[pp.loop] >= 0.5f);
        pill(kButtonPreviewRange, bx + bw + gap, rowY, bw, bh, pp.barLabels[bar ? 1 : 0], c, bar);
    }

    // Bildschirm-Tastatur (nur in der Standalone-App sinnvoll): zwei Oktaven ab C der Tastatur-Oktave,
    // links OKT−/OKT+. Spielbar mit der Maus und der Computer-Tastatur (A W S E D F T G Z/Y H U J K O L P,
    // „-“/„+“ = Oktave). Gedrückte Tasten leuchten in c.
    void drawKeyboard(float x, float y, float w, float h, const Color& c)
    {
        const float bw = 56;
        pill(kButtonOctaveDown, x, y + 4, bw, 22, "Okt -", c, false);
        pill(kButtonOctaveUp, x, y + 32, bw, 22, "Okt +", c, false);
        char oct[16]; // Oktave steht auf den C-Tasten
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);

        static const int kWhite[7] = { 0, 2, 4, 5, 7, 9, 11 };
        const float kx = x + bw + 10, kw = w - bw - 10, whiteW = kw / 15.0f;
        for (int i = 0; i < 15; ++i) { // weiße Tasten (15 = zwei Oktaven + C)
            const int semi = 12 * (i / 7) + kWhite[i % 7];
            const float tx = kx + i * whiteW;
            addHit(-1, kButtonKey - semi, tx, y, whiteW, h, kButton, y + h);
            beginPath();
            roundedRect(tx + 1, y, whiteW - 2, h, 3);
            fillPaint(linearGradient(0, y, 0, y + h, keyDown[semi] ? c : th.chromeHi, keyDown[semi] ? kit::withAlpha(c, 0.7f) : th.chromeLo));
            fill();
            if (semi % 12 == 0) {
                std::snprintf(oct, sizeof(oct), "C%d", keyboardOctave + semi / 12);
                fontSize(8);
                fillColor(th.night);
                text(tx + whiteW / 2, y + h - 8, oct, nullptr);
            }
        }
        for (int i = 0; i < 14; ++i) { // schwarze Tasten darüber (Klickflächen zuletzt = oben)
            const int pos = i % 7;
            if (pos == 2 || pos == 6)
                continue;
            const int semi = 12 * (i / 7) + kWhite[pos] + 1;
            const float tx = kx + (i + 1) * whiteW - whiteW * 0.32f;
            addHit(-1, kButtonKey - semi, tx, y, whiteW * 0.64f, h * 0.6f, kButton, y + h);
            beginPath();
            roundedRect(tx, y, whiteW * 0.64f, h * 0.6f, 2);
            fillColor(keyDown[semi] ? c : th.night);
            fill();
            strokeColor(kit::withAlpha(c, 0.5f));
            strokeWidth(1);
            stroke();
        }
    }

    // Farbe einer Ebene: Ebene 0 in der Akzentfarbe, weitere heller bzw. wärmer
    static Color layerColor(const Color& c, int i)
    {
        if (i == 0)
            return c;
        const float t = i == 1 ? 0.45f : 0.75f;
        return Color(c.red + (1.0f - c.red) * t, c.green + (0.9f - c.green) * t * 0.6f, c.blue * (1.0f - t * 0.5f), 1.0f);
    }

    // KOPIEREN / EINFÜGEN, rechtsbündig bis xRight; Statusmeldung links daneben.
    void copyPaste(float xRight, float y)
    {
        const float w = 92, h = 20;
        pill(kButtonCopy, xRight - 2 * w - 6, y, w, h, "Kopieren", th.brand, false);
        pill(kButtonPaste, xRight - w, y, w, h, "EINF\xc3\x9cGEN", th.brand, false); // Großbuchstaben: upper() kennt kein ü
        if (!status.empty() && Clock::now() < statusUntil) {
            fontSize(11);
            textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
            fillColor(th.text);
            text(xRight - 2 * w - 18, y + h / 2, status.c_str(), nullptr);
        }
    }

    void setValue(int p, float v, bool gesture)
    {
        const offbeat::ParamInfo& info = offbeat::kParams[p];
        if (info.flags & offbeat::kFlagOutput)
            return;
        if (info.flags & (offbeat::kFlagInt | offbeat::kFlagBool))
            v = std::round(v);
        if (gesture)
            editParameter(p, true);
        values[p] = v;
        setParameterValue(p, v);
        if (gesture)
            editParameter(p, false);
        if (!(info.flags & offbeat::kFlagHidden) || p == previewParams().bar)
            previewDirty = true;
        redraw();
    }

    void redraw()
    {
        dirty = true;
        repaint();
    }

    void formatValue(int p, char* buf, size_t size) const
    {
        const offbeat::ParamInfo& i = offbeat::kParams[p];
        const float v = values[p];
        if (i.labels)
            std::snprintf(buf, size, "%s", i.labels[(int)std::lround(v - i.min)]);
        else if (i.flags & offbeat::kFlagBool)
            std::snprintf(buf, size, "%s", v > 0.5f ? "An" : "Aus");
        else if (i.flags & offbeat::kFlagInt)
            std::snprintf(buf, size, i.min < 0 ? "%+d" : "%d", (int)std::lround(v));
        else if (!std::strcmp(i.unit, "Hz") && v >= 1000.0f)
            std::snprintf(buf, size, "%.1f kHz", v / 1000.0f);
        else if (!std::strcmp(i.unit, "Hz") || !std::strcmp(i.unit, "ct") || !std::strcmp(i.unit, "ms")
                 || !std::strcmp(i.unit, "BPM"))
            std::snprintf(buf, size, v < 10.0f ? "%.1f %s" : "%.0f %s", v, i.unit);
        else if (i.unit[0])
            std::snprintf(buf, size, "%.1f %s", v, i.unit);
        else
            std::snprintf(buf, size, "%.0f %%", 100.0f * v);
    }

    static float toNorm(int p, float v)
    {
        const offbeat::ParamInfo& i = offbeat::kParams[p];
        if (i.flags & offbeat::kFlagLog)
            return std::log(v / i.min) / std::log(i.max / i.min);
        return (v - i.min) / (i.max - i.min);
    }

    static float fromNorm(int p, float n)
    {
        const offbeat::ParamInfo& i = offbeat::kParams[p];
        float v = (i.flags & offbeat::kFlagLog) ? i.min * std::pow(i.max / i.min, n) : i.min + n * (i.max - i.min);
        if (i.flags & (offbeat::kFlagInt | offbeat::kFlagBool))
            v = std::round(v);
        return v;
    }

    // ---- DPF ------------------------------------------------------------------------------------

    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= (uint32_t)offbeat::kParamCount)
            return;
        const float old = values[index];
        values[index] = value;
        const PreviewParams pp = previewParams();
        if (!(offbeat::kParams[index].flags & (offbeat::kFlagOutput | offbeat::kFlagHidden))
            || (int)index == pp.bar || ((int)index == pp.bpm && std::fabs(old - value) > 0.01f))
            previewDirty = true;
        redraw();
    }

#if DISTRHO_PLUGIN_WANT_PROGRAMS
    void programLoaded(uint32_t index) override
    {
        offbeat::applyPreset((int)index, values);
        previewDirty = true;
        redraw();
    }
#endif

    // Manche Hosts (Carla-Bridge auf macOS) zeichnen ein einzelnes repaint() aus einem Klick
    // erst beim nächsten Ereignis; hier wird im Idle-Takt sicher nachgezeichnet.
    void uiIdle() override
    {
        // Bild der Oberfläche für die Doku: Standalone mit OFFBEAT_SCREENSHOT=datei.ppm starten,
        // die Oberfläche speichert sich nach ein paar Bildern selbst und beendet das Programm.
        // OFFBEAT_SCREENSHOT_PRESET=n lädt vorher das Preset n.
        if (const char* shot = std::getenv("OFFBEAT_SCREENSHOT")) {
            if (++screenshotTicks == 2)
                if (const char* preset = std::getenv("OFFBEAT_SCREENSHOT_PRESET")) {
                    float v[offbeat::kParamCount];
                    offbeat::applyPreset(std::atoi(preset), v);
                    for (int p = 0; p < offbeat::kParamCount; ++p)
                        if (!(offbeat::kParams[p].flags & offbeat::kFlagOutput))
                            setValue(p, v[p], false);
                }
            if (screenshotTicks == 20)
                getWindow().renderToPicture(shot);
            else if (screenshotTicks == 40)
                std::exit(0);
            dirty = true;
        }
        // Vorschau höchstens alle 120 ms neu rendern (Reglerbewegungen gedrosselt)
        if (previewDirty && previewParams().play >= 0 && Clock::now() - previewRendered > std::chrono::milliseconds(120)) {
            previewDirty = false;
            previewRendered = Clock::now();
            renderPreview();
            dirty = true;
        }
        if (dirty)
            repaint();
    }

    void onNanoDisplay() override
    {
        dirty = false;
        const float s = getWidth() / uiW; // DPF skaliert das Fenster, wir die Zeichnung
        scale(s, s);
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        hits.clear();
        drawBackground();
        drawContent();
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float s = getWidth() / uiW;
        const float x = ev.pos.getX() / s, y = ev.pos.getY() / s;

        if (ev.press && editParam >= 0) { // Klick irgendwohin übernimmt die Eingabe
            commitEdit();
            return true;
        }
        const Hit* h = hitAt(x, y);
        if (ev.press && ev.button == kMouseButtonRight) {
            if (h && h->param >= 0)
                startEdit(h->param);
            return h && h->param >= 0;
        }
        if (ev.button != kMouseButtonLeft)
            return false;

        if (!ev.press && mouseKey >= 0) { // Taste der Bildschirm-Tastatur losgelassen
            playKey(mouseKey, false);
            mouseKey = -1;
            return true;
        }
        if (!ev.press) {
            if (dragParam >= 0) {
                editParameter(dragParam, false);
                if (dragKind == kCompact && !dragMoved)
                    startEdit(dragParam);
            }
            dragParam = -1;
            return true;
        }
        if (!h)
            return false;
        if (h->kind == kButton) {
            if (h->button == kButtonCopy)
                copySettings();
            else if (h->button == kButtonPaste)
                pasteSettings();
            else if (h->button <= kButtonPreview && h->button >= kButtonPreviewRange)
                previewButton(h->button);
            else if (h->button == kButtonOctaveDown || h->button == kButtonOctaveUp) {
                keyboardOctave = std::max(0, std::min(7, keyboardOctave + (h->button == kButtonOctaveUp ? 1 : -1)));
                redraw();
            } else if (h->button <= kButtonKey && h->button > kButtonKey - 25) {
                mouseKey = kButtonKey - h->button;
                playKey(mouseKey, true);
            }
            else
                onButton(h->button);
            return true;
        }
        const int p = h->param;
        const offbeat::ParamInfo& info = offbeat::kParams[p];
        if (info.flags & offbeat::kFlagOutput)
            return true;
        if (info.flags & offbeat::kFlagBool) {
            setValue(p, values[p] > 0.5f ? 0.0f : 1.0f, true);
            return true;
        }
        if (h->kind == kKnob && y >= h->valueTop) { // Klick auf den Wert unter dem Regler
            startEdit(p);
            return true;
        }
        if (p == lastClickParam && ev.time - lastClickTime < 300) { // Doppelklick
            setValue(p, info.def, true);
            lastClickParam = -1;
            return true;
        }
        lastClickParam = p;
        lastClickTime = ev.time;
        dragParam = p;
        dragKind = h->kind;
        dragMoved = false;
        dragY = y;
        dragNorm = toNorm(p, values[p]);
        editParameter(p, true);
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (dragParam < 0)
            return false;
        const float s = getWidth() / uiW;
        const float y = ev.pos.getY() / s;
        if (std::fabs(y - dragY) < 0.5f)
            return true;
        dragMoved = true;
        const float range = (ev.mod & kModifierShift) ? 1000.0f : 200.0f;
        dragNorm = std::fmin(1.0f, std::fmax(0.0f, dragNorm + (dragY - y) / range));
        dragY = y;
        setValue(dragParam, fromNorm(dragParam, dragNorm), false);
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = getWidth() / uiW;
        const Hit* h = hitAt(ev.pos.getX() / s, ev.pos.getY() / s);
        if (!h || h->param < 0 || ev.delta.getY() == 0.0)
            return false;
        const int p = h->param;
        const offbeat::ParamInfo& info = offbeat::kParams[p];
        if (info.flags & offbeat::kFlagOutput)
            return false;
        const float dir = ev.delta.getY() > 0.0 ? 1.0f : -1.0f;
        float v;
        if (info.flags & (offbeat::kFlagInt | offbeat::kFlagBool))
            v = std::fmin(info.max, std::fmax(info.min, values[p] + dir));
        else
            v = fromNorm(p, std::fmin(1.0f, std::fmax(0.0f, toNorm(p, values[p]) + dir * 0.02f)));
        setValue(p, v, true);
        return true;
    }

    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (editParam < 0 && kit::isStandaloneApp()) { // Standalone: Computer-Tastatur spielt Noten
            static const char kKeys[] = "awsedftgzhujkolp"; // C … D# der nächsten Oktave
            const uint key = ev.key >= 'A' && ev.key <= 'Z' ? ev.key + 32 : ev.key;
            if (ev.press && (key == '-' || key == '+')) {
                keyboardOctave = std::max(0, std::min(7, keyboardOctave + (key == '+' ? 1 : -1)));
                redraw();
                return true;
            }
            const char* at = key ? std::strchr(kKeys, (int)(key == 'y' ? 'z' : key)) : nullptr; // y = z (QWERTZ)
            if (at && *at) {
                const int semi = (int)(at - kKeys);
                if (ev.press != keyDown[semi]) // Tasten-Wiederholung ignorieren
                    playKey(semi, ev.press);
                return true;
            }
        }
        if (editParam < 0 || !ev.press)
            return false;
        if (ev.key == kKeyEnter)
            commitEdit();
        else if (ev.key == kKeyEscape)
            editParam = -1;
        else if ((ev.key == kKeyBackspace || ev.key == kKeyDelete) && !editText.empty())
            editText.pop_back();
        redraw();
        return true;
    }

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (editParam < 0)
            return false;
        if (ev.character >= 32 && ev.character != 127 && editText.size() < 20)
            editText += ev.string;
        redraw();
        return true;
    }

protected: // auch für eigene Bedienflächen abgeleiteter Oberflächen (z. B. Raster von Gearbox)
    using Clock = std::chrono::steady_clock;
    enum Kind { kKnob, kCompact, kButton };
    struct Hit {
        float x, y, w, h;
        int param, button;
        Kind kind;
        float valueTop;
    };

    std::vector<Hit> hits;
    int dragParam = -1, lastClickParam = -1, editParam = -1;
    Kind dragKind = kKnob;
    bool dragMoved = false, dirty = true;
    uint lastClickTime = 0;
    int screenshotTicks = 0;
    bool previewDirty = true, previewToggle = false;
    int keyboardOctave = 3, mouseKey = -1;
    bool keyDown[25] = {};
    std::chrono::steady_clock::time_point previewRendered;
    float dragY = 0, dragNorm = 0;
    std::string editText, status;
    Clock::time_point statusUntil;

    void addHit(int param, int button, float x, float y, float w, float h, Kind kind, float valueTop)
    {
        hits.push_back({ x, y, w, h, param, button, kind, valueTop });
    }

    // Oberste Fläche zuerst (zuletzt gezeichnet).
    const Hit* hitAt(float x, float y) const
    {
        for (auto it = hits.rbegin(); it != hits.rend(); ++it)
            if (x >= it->x && x < it->x + it->w && y >= it->y && y < it->y + it->h)
                return &*it;
        return nullptr;
    }

    // Wert unter dem Regler, im Eingabemodus als Feld mit Cursor.
    void drawValue(int p, float cx, float cy, float w, const Color& c)
    {
        fontSize(13);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        if (p == editParam) {
            beginPath();
            roundedRect(cx - w / 2, cy - 9, w, 19, 4);
            fillColor(th.night);
            fill();
            strokeColor(c);
            strokeWidth(1.5f);
            stroke();
            fillColor(th.text);
            text(cx, cy, (editText + "|").c_str(), nullptr);
            return;
        }
        char buf[32];
        formatValue(p, buf, sizeof(buf));
        fillColor(th.text);
        text(cx, cy, buf, nullptr);
    }

    void startEdit(int p)
    {
        if (dragParam >= 0)
            editParameter(dragParam, false);
        dragParam = -1;
        editParam = p;
        editText.clear();
        redraw();
    }

    void commitEdit()
    {
        const int p = editParam;
        editParam = -1;
        float v;
        if (!editText.empty() && offbeat::parseParamText(p, editText.c_str(), v))
            setValue(p, v, true);
        else if (!editText.empty())
            showStatus("Ung\xc3\xbcltige Eingabe");
        redraw();
    }

    // Taste 0–24 der Bildschirm-Tastatur spielen: klingt wie beschriftet (Octave-Regler herausgerechnet)
    void playKey(int semi, bool down)
    {
        if (semi < 0 || semi > 24)
            return;
        const int shift = octaveParam() >= 0 ? 12 * (int)std::lround(values[octaveParam()]) : 0;
        const int note = std::max(0, std::min(127, 12 * (keyboardOctave + 1) + semi - shift));
        if (down && keyDown[semi])
            return;
        keyDown[semi] = down;
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
        sendNote(0, (uint8_t)note, down ? 100 : 0);
#else
        (void)note; // Effekte (Turbo) haben keinen MIDI-Eingang
#endif
        redraw();
    }

    void previewButton(int id)
    {
        const PreviewParams pp = previewParams();
        if (id == kButtonPreviewLoop && pp.loop >= 0) {
            setValue(pp.loop, values[pp.loop] >= 0.5f ? 0.0f : 1.0f, true);
            return;
        }
        if (id == kButtonPreviewRange && pp.bar >= 0) {
            setValue(pp.bar, values[pp.bar] >= 0.5f ? 0.0f : 1.0f, true);
            return;
        }
        int code = id == kButtonPreviewAll ? 4 : kButtonPreview - id + 1;
        if (pp.active >= 0 && (int)values[pp.active] == code)
            code = 0; // laufende Ebene noch einmal anklicken = Stopp
        previewToggle = !previewToggle; // jede Änderung löst im DSP aus, auch zweimal dieselbe Ebene
        setValue(pp.play, (float)(code | (previewToggle ? 8 : 0)), true);
    }

    void showStatus(const std::string& text)
    {
        status = text;
        statusUntil = Clock::now() + std::chrono::seconds(4);
        redraw();
    }

    void copySettings()
    {
        int list[offbeat::kParamCount];
        const std::string json = offbeat::toJson(values, list, offbeat::jsonParams(values, list));
        if (getWindow().setClipboard("text/plain", json.c_str(), json.size()))
            showStatus("Einstellungen kopiert");
        else
            showStatus("Kopieren ging nicht");
    }

    void pasteSettings()
    {
        size_t size = 0;
        const char* data = (const char*)getWindow().getClipboard(size);
        if (!data || !size) {
            showStatus("Zwischenablage ist leer");
            return;
        }
        const std::string json(data, size);
        float next[offbeat::kParamCount];
        std::memcpy(next, values, sizeof(next));
        bool changed[offbeat::kParamCount] = {};
        std::string errors;
        const int n = offbeat::applyJson(json.c_str(), next, changed, errors);
        for (int p = 0; p < offbeat::kParamCount; ++p)
            if (changed[p])
                setValue(p, next[p], true);
        char msg[64];
        std::snprintf(msg, sizeof(msg), "%d Werte gesetzt", n);
        showStatus(n == 0 ? "Kein g\xc3\xbcltiges JSON" : errors.empty() ? msg : std::string(msg) + " (" + errors + ")");
    }
};

END_NAMESPACE_DISTRHO
