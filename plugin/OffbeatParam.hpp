// ParamInfo → DPF-Parameter; gemeinsam für alle Offbeat-Plugins.
// Quelle: offbeat-engine/plugin/OffbeatParam.hpp – in die anderen Repos kopiert, Änderungen dort zurückspielen.
#pragma once

#include "DistrhoPlugin.hpp"
#include "ParamInfo.hpp"

START_NAMESPACE_DISTRHO

inline void fillParameter(const offbeat::ParamInfo& info, Parameter& parameter)
{
    using namespace offbeat;
    parameter.hints = (info.flags & kFlagOutput) ? kParameterIsOutput : kParameterIsAutomatable;
    if (info.flags & kFlagHidden)
        parameter.hints = (parameter.hints & kParameterIsOutput) | kParameterIsHidden;
    if (info.flags & kFlagLog)
        parameter.hints |= kParameterIsLogarithmic;
    if (info.flags & kFlagBool)
        parameter.hints |= kParameterIsBoolean | kParameterIsInteger;
    if (info.flags & kFlagInt)
        parameter.hints |= kParameterIsInteger;
    parameter.name = info.name;
    parameter.symbol = info.symbol;
    parameter.unit = info.unit;
    parameter.ranges.min = info.min;
    parameter.ranges.max = info.max;
    parameter.ranges.def = info.def;

    if (info.labels) {
        const int count = (int)(info.max - info.min) + 1;
        ParameterEnumerationValue* values = new ParameterEnumerationValue[count];
        for (int i = 0; i < count; ++i) {
            values[i].value = info.min + (float)i;
            values[i].label = info.labels[i];
        }
        parameter.enumValues.count = (uint8_t)count;
        parameter.enumValues.restrictedMode = true;
        parameter.enumValues.values = values; // DPF gibt den Speicher frei
    }
}

END_NAMESPACE_DISTRHO
