// Beschreibung eines Parameters; gemeinsam für alle Offbeat-Plugins.
// Quelle: offbeat-engine/src/dsp/ParamInfo.hpp – in die anderen Repos kopiert, Änderungen dort zurückspielen.
#pragma once

#include <cstdint>

namespace offbeat {

enum ParamFlags : uint32_t {
    kFlagLog = 1,    // logarithmischer Regler (Frequenzen, Zeiten)
    kFlagBool = 2,   // An/Aus-Schalter
    kFlagInt = 4,    // ganzzahlig (mit labels: Auswahl)
    kFlagOutput = 8, // Anzeige-Wert vom DSP an die Oberfläche (Pegel), nicht einstellbar
    kFlagHidden = 16, // Bedienelement der Oberfläche (z. B. Vorhören): im Host versteckt, nicht automatisierbar,
                      // nicht in Presets und JSON
};

struct ParamInfo {
    const char* name;
    const char* symbol;
    const char* unit;
    float min, max, def;
    uint32_t flags;
    const char* const* labels = nullptr; // Namen für ganzzahlige Auswahl-Parameter, sonst nullptr
};

} // namespace offbeat
