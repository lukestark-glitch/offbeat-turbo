# Offbeat Turbo – Regler und Anleitung

Turbo macht den Mix laut. Der Weg geht über Glue-Kompressor, Soft-Clipper und einen True-Peak-sicheren Limiter. Ziel aus deinen Referenzen: im Drop **−6 bis −9 dBFS RMS**, Spitzen knapp unter 0 (Ceiling −0,3 dBFS).

## So benutzt du Turbo in LMMS (Master)

1. LMMS neu starten. Die LADSPA-Version liegt als `offbeat-turbo-ladspa.so` in deinem LMMS-LADSPA-Ordner (`~/Documents/lmms/plugins/ladspa/`). LMMS 1.2 lädt dort nur Dateien mit der Endung `.so`.
2. **FX-Mixer** öffnen (F9), beim **Master** im Effekt-Bereich „Add effect“ wählen und „Offbeat Turbo“ aus der LADSPA-Liste nehmen.
3. Entweder **Auto** an: Turbo regelt den Drop von selbst auf Target (−7,5 dBFS RMS). Am besten den Song einmal laufen lassen, Auto braucht ein paar Sekunden.
   Oder **Auto** aus und **Boost** hochdrehen, bis die Lautheit passt.
   - In der LADSPA-Version gibt es nur einfache Regler und keine Anzeige.
   - Zum Messen exportierst du den Song als WAV und rufst `turbo-render --in song.wav` auf. Es zeigt die Lautheit vorher und nachher.

**In Carla (LV2/VST3 mit Oberfläche):**
- Dort siehst du den Ladedruck-Regler und die Lautheitsanzeige.
- Carla Rack ist in LMMS aber ein *Instrument*. Turbo darin wirkt nur auf Klänge aus **demselben** Carla, also nicht auf den ganzen Song.
- Für den Master nimmst du deshalb die LADSPA-Version.

## Regler

| Regler | Bereich | Standard | Wirkung |
|---|---|---|---|
| Boost (Ladedruck) | 0 … +26 dB | +13 dB | Wie stark in Kompressor, Clipper und Limiter gefahren wird; mehr = lauter |
| Glue | 0 … 60 % | 30 % | Leichter Bus-Kompressor (Schwelle −6 … −20 dBFS, Ratio bis 4:1), hält den Mix zusammen |
| Attack | 1 … 19 ms | 10 ms | Wie schnell Glue greift |
| Release | 20 … 280 ms | 150 ms | Wie schnell Glue loslässt |
| Clip | 0 … 80 % | 40 % | Soft-Clipper vor dem Limiter: mehr = mehr Lautheit durch Sättigung, weniger Limiter-Pumpen |
| Ceiling | −1,2 … 0 dB | −0,3 dB | Höchster Pegel am Ausgang (True-Peak) |
| Lim. Release | 10 … 110 ms | 60 ms | Wie schnell der Limiter loslässt; kurz = lauter, lang = ruhiger |
| Target | −11 … −4 dB | −7,5 dB | Ziellautheit: Marke in der Anzeige; mit **Auto** regelt Turbo darauf |
| Auto | Aus / An | Aus | **Aus (Manuell):** Du drehst Boost, bis die Anzeige im Ziel steht. **An:** Turbo regelt zusätzlich zum Boost langsam (höchstens 1,5 dB/s, ±12 dB) auf Target nach; in Stille und Breakdowns (Eingang > 10 dB leiser als zuvor) friert die Automatik ein |

**Anzeige** (nur LV2/VST3; mit Auto steht unten links die aktuelle Auto-Verstärkung):
- **Ausgang-RMS** über 3 s, mit grüner Zielzone −9 … −6 und dem Target-Strich.
- **Eingang-RMS.**
- **Gain-Reduction.**
- **True-Peak** am Ausgang mit dem Ceiling-Strich.

**Latenz:** Der Limiter schaut 1,5 ms voraus (bei 48 kHz 87 Samples Gesamtlatenz). LV2 und VST3 melden das dem Host.

Die Bereiche sind so gewählt, dass LMMS beim Einfügen der LADSPA-Version genau die Standardwerte einstellt. LADSPA kennt nur grobe Standard-Stufen; `make check` prüft das für jeden Regler.

## Presets

Die Presets sind auf einen Mix mit etwa **−16 dBFS RMS** eingestellt. Ist dein Mix leiser oder lauter, drehst du Boost entsprechend.

| Preset | Idee | Gemessen am Test-Drop |
|---|---|---|
| Drop −7.5 (Standard) | Boost 13, Glue 30 %, Clip 40 % | −7,5 dBFS RMS, True-Peak −0,29 dBTP |
| Sanft −9 | Weniger Boost, weniger Clip | −8,8 dBFS RMS |
| Hart −6 | Boost 15, mehr Clip | −6,3 dBFS RMS |
| Nur Limiter | Kein Boost, kein Glue, kein Clip | nur Spitzenschutz |

## Offline-Werkzeug

```
bin/turbo-render --in song.wav                    # Lautheit vorher/nachher (Standard-Preset)
bin/turbo-render --in song.wav --preset 2 --out master.wav
bin/turbo-render --in song.wav --set boost=10 --set clip=0.6
bin/turbo-render --check-all                      # Abnahme (= make check)
```

**JSON** wie bei den anderen Offbeat-Plugins.
- **Kurznamen:** `boost glue comp_attack comp_release clip ceiling lim_release target auto`
- %-Regler stehen als 0 … 1.
