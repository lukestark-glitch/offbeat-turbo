# Plan – Offbeat Turbo

Master-Lautheit für die Offbeat-Familie: Bus-Kompressor (leicht) → Soft-Clipper → True-Peak-sicherer Limiter. Ziel aus den Referenzen des Nutzers: Drops bei **−6 bis −9 dBFS RMS**, Spitzen knapp unter 0 (Ceiling −0,3 dBFS).

Formate:
- **LV2 und VST3** mit eigener Oberfläche (Ladedruck-Regler, Lautheitsanzeige).
- **LADSPA** für den LMMS-Master. LMMS lädt LADSPA direkt als Effekt auf jeder Spur und auf dem Master. Es gibt dort nur generische Regler; Tempo braucht Turbo nicht.

## Signalweg

1. **Boost** (0 … +24 dB, „Ladedruck“): Eingangsverstärkung, der große Regler.
2. **Glue:** Bus-Kompressor, stereo gekoppelt.
   - Feed-forward mit Spitzen-Hüllkurve und weichem Knie (6 dB).
   - Glue 0 … 1 stellt gemeinsam Schwelle (−6 bis −20 dBFS) und Ratio (1:1 bis 4:1).
   - Attack und Release sind eigene Regler.
3. **Clip:** Soft-Clipper (tanh-Knie).
   - Clip 0 … 1 legt den Einsatz von 0 bis 6 dB unter dem Ceiling.
   - Kein Sample überschreitet das Ceiling.
4. **Limiter:** True-Peak-sicher.
   - **Erkennung:** Die Spitzen werden auf 4-fach überabgetastetem Signal gesucht (Polyphasen-Sinc, 32 Taps pro Phase; 12 Taps lasen die Spitzen des Clippers nahe Nyquist bis 0,4 dB zu niedrig).
   - **Vorschau:** 1,5 ms Look-ahead. Die Absenkung setzt schon vor der Spitze ein: Min-Hold über die Vorschau, dann Release-Glättung, dann Mittelung über die Vorschau.
   - **Sicherheit:** Am Ende sorgt ein harter Sample-Clip am Ceiling dafür, dass nichts darüber geht.
   - **Latenz:** Look-ahead plus Interpolator, etwa 1,6 ms. Sie wird an den Host gemeldet (LV2/VST3).
5. **Ceiling** (−3 … 0 dB, Standard −0,3 dBFS).

**Anzeige** (Ausgabe-Parameter, in der Oberfläche):
- RMS am Ausgang und am Eingang (gleitend über 3 s),
- Gain-Reduction,
- True-Peak am Ausgang (Maximum der letzten 3 s).

Der Regler **Target** (Standard −7,5 dBFS) setzt nur die Zielmarke der Anzeige; er regelt nichts.

**Offline-Werkzeug** `turbo-render`:
- **Eigene Datei:** Eine aus LMMS exportierte WAV einlesen, durch Turbo rechnen und die Lautheit vorher und nachher ausgeben (RMS gesamt, lautestes 3-s-Fenster, Sample-Peak, True-Peak). Auf Wunsch schreibt es die bearbeitete WAV.
- **`--check-all`:** Erzeugt einen künstlichen Drop (Kick, Offbeat-Bass, Hats, Akkorde bei 160 BPM, RMS −16 dBFS) und prüft jedes Preset.
- **Prüfungen:**
  - True-Peak ≤ Ceiling (+0,05 dB Toleranz),
  - die Drop-Presets landen bei −6 … −9 dBFS RMS,
  - Stille bleibt Stille,
  - kein NaN, keine Denormals.

**Gemeinsamer Code** aus offbeat-engine (bewusste Kopie): `ParamInfo.hpp`, `ParamText.*`, `Blocks.hpp`, `OffbeatKit.hpp` (mit `bigKnob`), `OffbeatParam.hpp`, `tools/Measure.hpp`.

**IDs** (nie mehr ändern):
- URI `https://github.com/lukestark-glitch/offbeat-turbo`
- VST3 `Ofbt`/`OfbU`
- Bundle `offbeat-turbo`, LADSPA-Datei `offbeat-turbo-ladspa.dylib`, LADSPA-ID von DPF aus der Unique-ID

## Oberfläche

- **Kopf:** Logo „OFFBEAT / TURBO“ (Giftgrün) und der Turbolader.
- **Links „LADEDRUCK“:** großer Boost-Regler im Manometer-Look.
- **Rechts „LAUTHEIT“:**
  - Balken für Ausgang-RMS mit Zielzone −9 … −6 dBFS und Target-Marke,
  - Eingang-RMS,
  - Gain-Reduction,
  - True-Peak mit Ceiling-Marke.
- **Unten:** Glue, Attack, Release, Clip, Ceiling, Limiter Release und Target, dazu KOPIEREN/EINFÜGEN.

## Stand (PR #1)

`make check` (= `turbo-render --check-all`): **BESTANDEN**.

| Fall | Messwert |
|---|---|
| Test-Drop vorher | RMS −16,0 dBFS, True-Peak −4,6 dBTP |
| Drop −7.5 | −7,52 dBFS RMS, True-Peak −0,29 dBTP |
| Sanft −9 | −8,79 dBFS RMS, True-Peak −0,33 dBTP |
| Hart −6 | −6,30 dBFS RMS, True-Peak −0,30 dBTP |
| Zwischen-Sample-Spitzen (+3 dB ISP, Boost +12) | True-Peak −0,35 dBTP |
| Stille | bleibt Stille |
| Latenz | 87 Samples gemeldet = gemessen |

- **Glue:** Er gleicht die Hälfte seiner Absenkung automatisch aus (Make-up). Sonst wäre „Hart“ leiser als „Sanft“.
- **Rechenlast:** etwa 30-fach schneller als Echtzeit.

## Offene Fragen

- **LADSPA-Installation (vom Nutzer freigegeben):**
  - `make install` legt die LADSPA-Datei nach `~/Library/Audio/Plug-Ins/LADSPA/`.
  - Zusätzlich kommt sie in den LMMS-LADSPA-Ordner aus `~/.lmmsrc.xml` (`laddir`, hier `~/Documents/lmms/plugins/ladspa/`).
  - Andere Dateien dort werden nicht angefasst.
- **Turbo in Carla:** Ein Carla Rack ist in LMMS ein Instrument. Turbo darin wirkt nur auf Klänge aus demselben Carla. Für den Master ist darum LADSPA gedacht.
- **Kein Auto-Gain:** Target ist nur eine Anzeige. Der Nutzer dreht Boost, bis der Balken in der Zielzone steht; das ist vorsichtiger als eine Automatik.
- **Clipper ohne Oversampling:** Er läuft auf einfacher Abtastrate (minimal Aliasing bei viel Clip). Der Limiter danach ist dennoch True-Peak-sicher.
