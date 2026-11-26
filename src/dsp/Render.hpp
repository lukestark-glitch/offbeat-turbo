// Noten offline rendern, genau wie im Plugin: Transport läuft ab Schlag 0, Blöcke von 256 Samples,
// an jedem Noten-Ereignis geteilt, Host-Tempo vor den Noten des Blocks. Gemeinsam genutzt vom
// Offline-Renderer und von der Vorschau in der Oberfläche – damit sind beide bitgleich.
// Quelle: offbeat-engine/src/dsp/Render.hpp – in die anderen Repos kopiert, Änderungen dort zurückspielen.
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace offbeat {

struct NoteEvent {
    uint32_t frame;
    int note, velocity;
    bool on;
};

// inst braucht setTransport(playing, beat, bpm), noteOn(note, vel), noteOff(note), process(l, r, n).
template <class Instrument>
void renderEvents(Instrument& inst, std::vector<NoteEvent> events, double bpm, int sr, float* l, float* r, uint32_t total)
{
    std::stable_sort(events.begin(), events.end(), [](const NoteEvent& a, const NoteEvent& b) { return a.frame < b.frame; });
    size_t next = 0;
    for (uint32_t pos = 0; pos < total;) {
        inst.setTransport(true, (double)pos / sr * bpm / 60.0, bpm);
        while (next < events.size() && events[next].frame <= pos) {
            const NoteEvent& e = events[next++];
            if (e.on)
                inst.noteOn(e.note, e.velocity);
            else
                inst.noteOff(e.note);
        }
        uint32_t n = total - pos < 256 ? total - pos : 256;
        if (next < events.size() && events[next].frame < pos + n)
            n = events[next].frame - pos;
        inst.process(l + pos, r + pos, n);
        pos += n;
    }
}

} // namespace offbeat
