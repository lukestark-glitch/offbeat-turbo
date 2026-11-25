# Offbeat Turbo – Regler und Anleitung

Turbo macht den Mix laut. Der Weg geht über Glue-Kompressor, Soft-Clipper und einen True-Peak-sicheren Limiter. Ziel aus deinen Referenzen: im Drop **−6 bis −9 dBFS RMS**, Spitzen knapp unter 0 (Ceiling −0,3 dBFS).

## So benutzt du Turbo in LMMS (Master)

1. LMMS neu starten. Die LADSPA-Version liegt schon in deinem LMMS-LADSPA-Ordner (`~/Documents/lmms/plugins/ladspa/`).
2. **FX-Mixer** öffnen (F9), beim **Master** im Effekt-Bereich „Add effect“ wählen und „Offbeat Turbo“ aus der LADSPA-Liste nehmen.
3. Den Drop abspielen und **Boost** hochdrehen, bis die Lautheit passt.
   - In der LADSPA-Version gibt es nur einfache Regler und keine Anzeige.
   - Zum Messen exportierst du den Song als WAV und rufst `turbo-render --in song.wav` auf. Es zeigt die Lautheit vorher und nachher.

**In Carla (LV2/VST3 mit Oberfläche):**
- Dort siehst du den Ladedruck-Regler und die Lautheitsanzeige.
- Carla Rack ist in LMMS aber ein *Instrument*. Turbo darin wirkt nur auf Klänge aus **demselben** Carla, also nicht auf den ganzen Song.
- Für den Master nimmst du deshalb die LADSPA-Version.

## Regler

| Regler | Bereich | Standard | Wirkung |
|---|---|---|---|
| Boost (Ladedruck) | 0 … +24 dB | +13 dB | Wie stark in Kompressor, Clipper und Limiter gefahren wird; mehr = lauter |
| Glue | 0 … 100 % | 30 % | Leichter Bus-Kompressor (Schwelle −6 … −20 dBFS, Ratio bis 4:1), hält den Mix zusammen |
| Attack | 1 … 50 ms | 10 ms | Wie schnell Glue greift |
| Release | 30 … 600 ms | 150 ms | Wie schnell Glue loslässt |
| Clip | 0 … 100 % | 40 % | Soft-Clipper vor dem Limiter: mehr = mehr Lautheit durch Sättigung, weniger Limiter-Pumpen |
| Ceiling | −3 … 0 dB | −0,3 dB | Höchster Pegel am Ausgang (True-Peak) |
| Lim. Release | 10 … 500 ms | 60 ms | Wie schnell der Limiter loslässt; kurz = lauter, lang = ruhiger |
| Target | −14 … −4 dB | −7,5 dB | Nur die Zielmarke in der Anzeige, regelt nichts |

**Anzeige** (nur LV2/VST3):
- **Ausgang-RMS** über 3 s, mit grüner Zielzone −9 … −6 und dem Target-Strich.
- **Eingang-RMS.**
- **Gain-Reduction.**
- **True-Peak** am Ausgang mit dem Ceiling-Strich.

**Latenz:** Der Limiter schaut 1,5 ms voraus (bei 48 kHz 87 Samples Gesamtlatenz). LV2 und VST3 melden das dem Host.

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
- **Kurznamen:** `boost glue comp_attack comp_release clip ceiling lim_release target`
- %-Regler stehen als 0 … 1.
