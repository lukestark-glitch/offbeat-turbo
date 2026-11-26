# Offbeat Turbo – Code-Doku

## Überblick

```
offbeat-turbo/
├── src/dsp/
│   ├── Params.hpp/.cpp   Regler, Anzeige-Werte (kFlagOutput), Presets, jsonParams()
│   ├── Turbo.hpp/.cpp    TruePeak (4-fach-Interpolator), RmsMeter (3 s), Turbo (die ganze Kette)
│   └── Blocks.hpp, ParamInfo.hpp, ParamText.*   gemeinsame Bausteine
├── plugin/
│   ├── DistrhoPluginInfo.h  Name, URI, IDs, Latenz an, DPF_VST_SHOW_PARAMETER_OUTPUTS (siehe unten)
│   ├── TurboPlugin.cpp      DPF-Hülle: Parameter, Presets, Latenz, Anzeige-Werte
│   ├── TurboUI.cpp          Oberfläche: Ladedruck (bigKnob), Lautheits-Balken, Regler
│   └── OffbeatKit.hpp, OffbeatParam.hpp
├── tools/turbo-render.cpp   WAV durch Turbo rechnen, Lautheit vorher/nachher, --check-all
└── assets/turbo.png
```

## Signalfluss (`Turbo::process`, pro Sample)

```
in ─► Eingang-RMS
   ─► × Boost (+ Auto-Verstärkung, wenn Auto an)
   ─► Glue: Pegel = max(|L|,|R|) → Hüllkurve (Attack/Release) → Gain-Computer mit 6-dB-Knie
            Schwelle = −6 − 14·Glue dBFS, Ratio = 1 + 3·Glue, Make-up = halbe Absenkung bei 0 dBFS
   ─► Clip: |x| über clipStart = Ceiling · 10^(−6·Clip/20) läuft weich (tanh) gegen das Ceiling
   ─► Limiter:
        1. TruePeak (L, R): Maximum aus Sample und 3 interpolierten Zwischenwerten (32 Taps/Phase)
        2. nötige Absenkung = Ceiling·0,9943 / Spitze   (0,05 dB Reserve)
        3. Min-Hold über die Vorschau (1,5 ms)
        4. Release-Glättung (bleibt immer ≤ Hold, darum sicher)
        5. Mittelwert über die Vorschau → weicher Anstieg vor der Spitze
        6. Audio um die Vorschau (+ Interpolator) verzögert × Absenkung
   ─► harter Sample-Clip am Ceiling (Sicherheitsnetz)
   ─► Ausgang-RMS, True-Peak-Anzeige, Auto-Regelung
```

`latency()` = Vorschau − 1 + Interpolator-Verzögerung (87 Samples bei 48 kHz). Das Plugin meldet sie dem Host mit `setLatency()`.

**Auto:** Eine Ausgangs-Leistung mit 1 s Zeitkonstante wird mit Target verglichen. Die Verstärkung ändert sich um 0,5 dB/s je dB Abstand, höchstens um 1,5 dB/s. Sie friert ein, wenn der Eingang unter −50 dBFS liegt oder mehr als 10 dB unter seinem jüngsten Maximum. Dieses Maximum sinkt nur mit 0,2 dB/s, darum bleibt die Regelung in Breakdowns bis etwa 50 s eingefroren.

## Anzeige-Werte

`kOutRms`, `kInRms`, `kGainReduction`, `kOutPeak` und `kAutoGain` tragen `kFlagOutput`.
- **DSP:** `process()` schreibt sie am Blockende in `params[]`.
- **Plugin:** `getParameterValue()` reicht sie an DPF weiter, das sie an die Oberfläche schickt.
- **Speichern:** In Presets und JSON tauchen sie nicht auf.

**VST2** kennt keine Anzeige-Werte. DPF zeigt dort normalerweise nur die Regler vor dem ersten Anzeige-Wert. `Auto` liegt aber dahinter, weil es nachträglich angehängt wurde. Darum steht in `DistrhoPluginInfo.h` `DPF_VST_SHOW_PARAMETER_OUTPUTS`; in VST2 sind die Anzeigen damit als Regler sichtbar.

## LADSPA und LMMS

- **LADSPA-Version:** DPF baut sie mit (`TARGETS += ladspa`). Sie hat keine Oberfläche und kein Tempo; Turbo braucht keins.
- **Dateiendung:** LMMS 1.2 lädt nur `*.so`. `make install` kopiert die Datei deshalb als `offbeat-turbo-ladspa.so` in den LMMS-Ordner (`laddir` aus `~/.lmmsrc.xml`).
- **Standardwerte:** LADSPA kennt nur grobe Standard-Stufen (Minimum, 25 %, Mitte, 75 %, Maximum, 0, 1, 100, 440). DPF wählt die Stufe nach linearen Schwellen, LMMS rechnet sie bei logarithmischen Reglern geometrisch zurück. Darum sind alle Regler linear, und jeder Standard liegt genau auf einer Stufe.
- **Prüfung:** `ladspaDefault()` in `turbo-render.cpp` bildet das nach; `make check` meldet, wenn ein neuer Regler das bricht.

## Anpassen

- **Klang der Kette:** Die Formeln stehen oben in `Turbo::process()`: Schwelle und Ratio des Glue, Einsatz des Clippers, Reserve des Limiters. Danach immer `make check`; es prüft True-Peak, Ziel-RMS, Grenzfälle und Latenz.
- **Neuer Regler:** `ParamId`/`kParams` **hinten** anhängen und in `process()` benutzen. Für LADSPA den Bereich so wählen, dass `ladspaDefault()` den Standard trifft, dann in `TurboUI::drawContent()` platzieren.
- **Neuer Anzeige-Wert:** mit `kFlagOutput` anlegen, am Ende von `process()` setzen und in `drawMeters()` (TurboUI.cpp) zeichnen.
- **Presets:** in `Params.cpp`, ans Ende. `--check-all` prüft jedes Preset mit Boost am Test-Drop (−16 dBFS RMS): Lautheit −9 … −6 dBFS und ±1 dB um sein Target.

## turbo-render

```sh
bin/turbo-render --in song.wav [--out master.wav] [--preset N] [--set boost=10] [--json datei]
bin/turbo-render --check-all          # = make check
bin/turbo-render --make-drop drop.wav # den Test-Drop als WAV schreiben
bin/turbo-render --preset-json 0      # Preset als JSON
```

Die True-Peak-Messung im Werkzeug ist unabhängig vom Plugin (8-fach, 32 Taps). Damit prüft sie den Limiter ehrlich.

## Gemeinsame Bausteine der Offbeat-Familie

Diese Dateien sind in allen vier Repos **gleich**. Sie sind eine bewusste Kopie; die Quelle ist [offbeat-engine](https://github.com/lukestark-glitch/offbeat-engine). Änderst du sie, dann zuerst dort und danach in die anderen Repos kopieren.

| Datei | Inhalt |
|---|---|
| `src/dsp/ParamInfo.hpp` | `ParamInfo`: Name, Kurzname (`symbol`), Einheit, Bereich, Standard, Flags (`kFlagLog`, `kFlagBool`, `kFlagInt`, `kFlagOutput`), Auswahl-Namen (`labels`) |
| `src/dsp/Blocks.hpp` | DSP-Bausteine: `Osc` (PolyBLEP-Saw/Square), `Sine`, `Tri`, `Noise`, `Svf` (TPT-State-Variable-Filter), `Smoother`, `Adsr`, `Chorus`, `DenormalGuard` (Flush-to-Zero auf Intel und ARM), Helfer wie `midiToHz`, `decayCoef`, `softLimit` |
| `src/dsp/ParamText.*` | Text ↔ Wert: `parseParamText` (Zahleneingabe am Regler), `applyJson`/`toJson` (KOPIEREN/EINFÜGEN, `--json`, `--preset-json`) |
| `plugin/OffbeatParam.hpp` | `fillParameter`: macht aus einem `ParamInfo` einen DPF-`Parameter` (Hints, Bereich, Auswahl-Werte) |
| `plugin/OffbeatKit.hpp` | `OffbeatKitUI`: Look und Bedienung aller Oberflächen (siehe unten) |
| `tools/Measure.hpp` | Messwerkzeuge der Renderer: WAV lesen/schreiben, Tonhöhe, Pegel, Spektrum, L/R-Korrelation |

### Ein Parameter, viele Stellen: alles kommt aus `kParams`

```
src/dsp/Params.cpp  kParams[]  ──┬─► DSP (Engine/Turbo/Pump liest params[id])
                                 ├─► plugin/*Plugin.cpp: initParameter() → fillParameter() → Host (VST3/AU/CLAP/LV2/…)
                                 ├─► OffbeatKitUI: Name, Bereich, Einheit, Anzeige, Zahleneingabe
                                 ├─► ParamText: JSON-Schlüssel = symbol
                                 └─► Renderer: --set symbol=wert, --list
```

Einen Regler hinzufügen heißt deshalb nur: Eintrag in `ParamId` (Params.hpp) **hinten anhängen**, Zeile in `kParams` (Params.cpp), im DSP benutzen und in der Oberfläche platzieren. Host-Anbindung, JSON und Zahleneingabe funktionieren dann von allein.

> **Nie** bestehende Einträge umsortieren oder löschen. Die Position ist die Parameter-ID in allen Formaten; gespeicherte Projekte würden sonst falsche Werte laden. Ebenso nie die Plugin-URI, VST3/AU-IDs (`DistrhoPluginInfo.h`) oder den Bundle-Namen (`plugin/Makefile: NAME`) ändern.

### OffbeatKitUI (`plugin/OffbeatKit.hpp`)

Die Oberflächen sind ein „Immediate-Mode“-GUI. `onNanoDisplay()` zeichnet bei jedem Bild alles neu. Jeder Baustein, der dabei gezeichnet wird, meldet seine Klickfläche an. Maus und Tastatur arbeiten dann mit den Flächen des zuletzt gezeichneten Bildes. Ein Plugin leitet von `OffbeatKitUI` ab und überschreibt:

| Methode | Zweck |
|---|---|
| `drawContent()` | alles zeichnen (Pflicht) |
| `onButton(id)` | eigene Knöpfe, z. B. Modus-Reiter |
| `controlLabel(p)` | kürzerer Name für einen Regler |

Bausteine:

| Baustein | Was er zeichnet |
|---|---|
| `drawBackground()` | Verlauf und Lochblech (automatisch) |
| `drawLogo(cx, cy)` | Chrom-Plakette mit Swoosh |
| `drawPanel(x, y, w, h, label, farbe)` | Abschnitt im Ticket-Stil |
| `knob(p, x, y, farbe)` | Regler in einer Zelle von 114 × 112 Pixeln; Schalter werden automatisch zur Pille |
| `bigKnob(p, cx, cy, r, farbe)` | großer Regler im Manometer-Look |
| `compact(p, x, y, w, farbe)` | kleiner Regler für Kopfzeilen |
| `pill(id, x, y, w, h, text, farbe, aktiv)` | Knopf |
| `copyPaste(xRechts, y)` | KOPIEREN/EINFÜGEN mit Statuszeile |
| `drawImage(bild, …)` | eingebettetes Bild |
| `drawTag(…)` | kleines Etikett |

Farben und Schriftzug kommen aus einem `kit::Theme`. Die Werte liegen in `values[]` und werden vom Host über `parameterChanged()` aktuell gehalten; `setValue()` schickt eine Änderung an den Host.

**Bedienung:**
- ziehen (mit Shift fein), Mausrad,
- Doppelklick setzt den Standard,
- Klick auf den Wert oder Rechtsklick öffnet die Zahleneingabe,
- Schalter per Klick,
- KOPIEREN/EINFÜGEN tauscht JSON über die Zwischenablage aus; welche Regler darin stehen, legt `jsonParams()` in Params.cpp fest.

**Bilder der Oberfläche** (für die Doku): `make standalone`, dann

```sh
OFFBEAT_SCREENSHOT=ui.ppm OFFBEAT_SCREENSHOT_PRESET=2 bin/<name>.app/Contents/MacOS/<name>
```

Die Oberfläche lädt das Preset, speichert sich als PPM und beendet sich. Die Bilder in `docs/images/` sind so entstanden (PPM → PNG umgewandelt).

### Formate und Build

`plugin/Makefile` nutzt DPF (`dpf/Makefile.plugins.mk`) und baut VST3, VST2, CLAP, LV2 und auf dem Mac AU, bei Turbo und Hydraulics dazu LADSPA. Das Makefile im Hauptordner ruft es auf und baut auf dem Mac ein Universal-Binary (`-arch x86_64 -arch arm64`, `NOOPT=true`). Außerdem erzeugt es die LV2-Beschreibungen (`generate-ttl.sh`) und baut den Offline-Renderer.

Das Bild in `assets/` wird beim Build mit `xxd -i` zu einem C-Array und fest in die Oberfläche eingebaut.

`.github/workflows/build.yml` baut bei jedem Push auf macOS, Windows (MSYS2/MinGW) und Linux und führt `make check` aus. Bei einem Tag `v*` legt es ein Release mit den ZIPs an.

### Echtzeit-Regeln im DSP

- In `process()` kein `new`/`malloc`, keine Locks, keine Datei-Zugriffe. Puffer werden in `setSampleRate()` angelegt.
- Am Anfang jedes `process()` steht `const DenormalGuard denormals;` (Flush-to-Zero).
- Parameter werden in `setParam()` auf ihren Bereich begrenzt; Sprünge glättet `Smoother`.
- Der Ausgang läuft am Ende durch `softLimit()` (Instrumente) bzw. das Ceiling (Turbo); es gibt kein hartes Übersteuern.

### Prüfen

`make check` startet den Offline-Renderer mit `--check-all`. Er rechnet Presets bzw. Testsignale ohne DAW, misst sie (Tonhöhe, Länge, Spektrum, L/R-Korrelation, True-Peak, …) und vergleicht sie mit festen Zielen. Jede Änderung am Klang sollte danach noch grün sein, oder die Ziele werden bewusst angepasst.
