// Parameter-Liste von Offbeat Turbo. Neue Parameter immer hinten anhängen: die Nummer ist die
// Parameter-ID in LV2/VST3/LADSPA, gespeicherte Projekte hängen daran.
#pragma once

#include "ParamInfo.hpp"

namespace offbeat {

enum ParamId {
    kBoost,
    kGlue,
    kCompAttack,
    kCompRelease,
    kClip,
    kCeiling,
    kLimRelease,
    kTarget, // nur Anzeige: Zielmarke für den Ausgangspegel
    // Anzeige-Werte vom DSP an die Oberfläche
    kOutRms,
    kInRms,
    kGainReduction,
    kOutPeak,
    kAuto,     // An: Lautheit regelt sich langsam auf Target nach (zusätzlich zu Boost)
    kAutoGain, // Anzeige: aktuelle Auto-Verstärkung

    kParamCount
};

extern const ParamInfo kParams[kParamCount];

struct Preset {
    const char* name;
    struct Value { ParamId id; float value; };
    const Value* values;
    int count;
};

extern const Preset kPresets[];
extern const int kPresetCount;

void applyPreset(int index, float out[kParamCount]);
void applyDefaults(float out[kParamCount]);

} // namespace offbeat
