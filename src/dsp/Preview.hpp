// Vorschau von Offbeat Turbo: ein eingebauter Demo-Loop (1 Takt Kick+Bass und Hats bei 160 BPM, mit
// Offbeat Engine erzeugt, assets/demo-loop.raw) vorher und nachher. Genutzt von der Oberfläche
// (Wellenform), vom DSP (Vorhören über den Plugin-Ausgang) und von turbo-render (--preview-check).
#pragma once

#include "Turbo.hpp"

#include <vector>

namespace offbeat {

constexpr int kDemoRate = 48000;
constexpr uint32_t kDemoFrames = 72000; // 4 Schläge bei 160 BPM
constexpr uint32_t kDemoBeat = kDemoFrames / 4;

// Sample des Demo-Loops (−16 dBFS RMS), channel 0 = links, 1 = rechts.
float demoSample(uint32_t frame, int channel);

// Ebene 0 = vorher (Demo-Ausschnitt), 1 = nachher (durch Turbo, Latenz herausgerechnet, wie turbo-render).
void renderPreview(const float* params, int layer, double beats, std::vector<float>& l, std::vector<float>& r);

} // namespace offbeat
