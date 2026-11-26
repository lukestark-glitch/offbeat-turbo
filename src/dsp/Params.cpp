#include "Params.hpp"

namespace offbeat {

// Bereiche so gewählt, dass LMMS beim Einfügen der LADSPA-Version genau die Standardwerte einstellt:
// LADSPA kennt nur grobe Standard-Stufen (Minimum, 25 %, Mitte, 75 %, Maximum), siehe ladspaDefault()
// in tools/turbo-render.cpp. Deshalb sind die Regler linear und die Standards liegen auf diesen Stufen.
// clang-format off
const ParamInfo kParams[kParamCount] = {
    // name               symbol          unit   min     max     default  flags
    { "Boost",            "boost",        "dB",  0,      26,     13,      0 },
    { "Glue",             "glue",         "",    0,      0.6f,   0.3f,    0 },
    { "Comp Attack",      "comp_attack",  "ms",  1,      19,     10,      0 },
    { "Comp Release",     "comp_release", "ms",  20,     280,    150,     0 },
    { "Clip",             "clip",         "",    0,      0.8f,   0.4f,    0 },
    { "Ceiling",          "ceiling",      "dB",  -1.2f,  0,      -0.3f,   0 },
    { "Limiter Release",  "lim_release",  "ms",  10,     110,    60,      0 },
    { "Target",           "target",       "dB",  -11,    -4,     -7.5f,   0 },
    { "Output RMS",       "out_rms",      "dB",  -60,    0,      -60,     kFlagOutput },
    { "Input RMS",        "in_rms",       "dB",  -60,    0,      -60,     kFlagOutput },
    { "Gain Reduction",   "gain_red",     "dB",  -30,    0,      0,       kFlagOutput },
    { "Output True Peak", "out_peak",     "dB",  -60,    6,      -60,     kFlagOutput },
    { "Auto Loudness",    "auto",         "",    0,      1,      0,       kFlagBool },
    { "Auto Gain",        "auto_gain",    "dB",  -12,    12,     0,       kFlagOutput },
    { "Preview",          "preview",      "",    0,      15,     0,       kFlagInt | kFlagHidden },
    { "Preview Loop",     "preview_loop", "",    0,      1,      0,       kFlagBool | kFlagHidden },
    { "Preview Bar",      "preview_bar",  "",    0,      1,      0,       kFlagBool | kFlagHidden },
    { "Preview Active",   "preview_active", "",  0,      4,      0,       kFlagOutput | kFlagHidden },
};

namespace {
using V = Preset::Value;
const V drop[]   = { { kBoost, 13 } }; // Boost-Werte passen zu einem Mix mit ~−16 dBFS RMS
const V soft[]   = { { kBoost, 8.5f }, { kGlue, 0.2f }, { kClip, 0.25f }, { kTarget, -9 } };
const V hard[]   = { { kBoost, 15 }, { kGlue, 0.2f }, { kClip, 0.6f }, { kTarget, -6 } };
const V limOnly[] = { { kBoost, 0 }, { kGlue, 0 }, { kClip, 0 } };

#define PRESET(name, arr) { name, arr, (int)(sizeof(arr) / sizeof(arr[0])) }
} // namespace

const Preset kPresets[] = {
    PRESET("Drop -7.5 (Standard)", drop),
    PRESET("Sanft -9", soft),
    PRESET("Hart -6", hard),
    PRESET("Nur Limiter", limOnly),
};
// clang-format on

const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

void applyDefaults(float out[kParamCount])
{
    for (int i = 0; i < kParamCount; ++i)
        out[i] = kParams[i].def;
}

void applyPreset(int index, float out[kParamCount])
{
    applyDefaults(out);
    if (index < 0 || index >= kPresetCount)
        return;
    const Preset& p = kPresets[index];
    for (int i = 0; i < p.count; ++i)
        out[p.values[i].id] = p.values[i].value;
}

int jsonParams(const float[kParamCount], int* out)
{
    // alle einstellbaren Regler (ohne Anzeige-Werte)
    int n = 0;
    for (int p = 0; p < kParamCount; ++p)
        if (!(kParams[p].flags & (kFlagOutput | kFlagHidden)))
            out[n++] = p;
    return n;
}

} // namespace offbeat
