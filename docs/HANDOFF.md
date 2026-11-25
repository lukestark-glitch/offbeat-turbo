# Handoff – Offbeat Turbo

**Fertig**
- Glue-Kompressor → Soft-Clipper → True-Peak-Limiter (4-fache Erkennung, 1,5 ms Look-ahead).
- Oberfläche mit Ladedruck-Regler und Lautheitsanzeige (LV2/VST3).
- LADSPA-Version für den LMMS-Master.
- `turbo-render` liest eigene WAVs; `make check` prüft Presets und Grenzfälle.

**Als Nächstes**
- An echten LMMS-Exporten des Nutzers prüfen (`turbo-render --in`) und die Boost-Werte der Presets anpassen.
