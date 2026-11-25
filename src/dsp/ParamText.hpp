// Werte als Text: Eingabe einer Zahl am Regler und Einstellungen als JSON (Kopieren/Einfügen).
// Ohne DPF, damit offbeat-render dasselbe JSON lesen kann. Gemeinsam für alle Offbeat-Plugins:
// braucht nur kParams/kParamCount aus dem Params.hpp des jeweiligen Plugins.
// Quelle: offbeat-engine/src/dsp/ParamText.* – in die anderen Repos kopiert, Änderungen dort zurückspielen.
#pragma once

#include "Params.hpp"

#include <string>

namespace offbeat {

// Text aus dem Eingabefeld eines Reglers in den Parameterwert umrechnen, in der Einheit der Anzeige:
// Prozent bei 0–1-Reglern, "1.5k" bei Hz, Namen bei Auswahl-Reglern, An/Aus bei Schaltern.
// Ergebnis ist auf den Bereich begrenzt; false bei unlesbarem Text.
bool parseParamText(int p, const char* text, float& out);

// JSON-Objekt {"symbol": wert, ...} lesen. Werte wie im Plugin (0–1-Regler als 0..1, Hz, ms, dB),
// Auswahl-Regler auch als Name ("mode": "Bass"), Schalter auch als true/false.
// Nur genannte Parameter ändern sich; changed[p] wird für jeden gesetzten Parameter true.
// Rückgabe: Anzahl gesetzter Werte; unbekannte oder ungültige Einträge landen in errors.
int applyJson(const char* json, float values[kParamCount], bool changed[kParamCount], std::string& errors);

// Die genannten Parameter als JSON (Auswahl als Name, Schalter als true/false).
std::string toJson(const float values[kParamCount], const int* list, int count);

} // namespace offbeat
