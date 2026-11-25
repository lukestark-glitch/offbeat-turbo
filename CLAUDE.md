# CLAUDE.md – Offbeat Turbo (Repo offbeat-turbo)

Projekt: Master-Lautheit (Glue-Kompressor → Soft-Clipper → True-Peak-Limiter) der Offbeat-Familie (LV2 + VST3 mit Oberfläche, dazu LADSPA für LMMS; DPF) für Hard Bounce / Bounce Techno. Läuft in LMMS 1.2.2 (LADSPA direkt, LV2/VST3 über Carla Rack 2.5.10) auf einem Intel-Mac (MacBook Pro 2019, x86_64). Ziele, Aufbau und Abnahmekriterien stehen in `docs/PLAN.md`: zuerst lesen.

## Arbeitsweise

- **Suchen:** Wenn `graphify-out/` existiert, zuerst den Graphen nutzen (deckt Code und Doku ab). Gezieltes grep nur für das, was der Graph nicht zeigt. Solange es noch keinen Graphen gibt, gezielt mit grep/glob suchen. Große Dateien nicht komplett lesen, wenn ein Ausschnitt reicht. `graphify-out/` nicht bearbeiten und nicht committen. Den DPF-Submodule nicht durchsuchen, außer für eine konkrete API-Frage.
- **Agents:** Bis auf Weiteres dürfen Subagents ohne Bedenken genutzt werden, auch parallel und mit jedem Modell, wenn das die Bauzeit verkürzt. **Sobald der Nutzer sagt, sparsamer zu sein, gilt ab dann:** einfache, klar umrissene Jobs (Dateien und Funktionen finden, Doku aktualisieren, Presets eintragen) gehen an Subagents mit Modell haiku oder sonnet; Opus nur für den Entwurf (DSP-Design, Architektur) und das Prüfen des Ergebnisses. Die übrigen Regeln gelten in beiden Fällen.
- **Erst schauen, dann bauen.** Vorhandenes wiederverwenden (DSP-Bausteine wie Oszillatoren, Hüllkurven, Filter nur einmal schreiben und von allen Modi nutzen). So wenig neuer Text wie möglich, keine Extra-Features über die Spezifikation hinaus.
- **Abhängigkeiten:** Einzige externe Abhängigkeit ist DPF (Git-Submodule). Keine weiteren Bibliotheken.
- **Tests:** Nur die betroffenen Tests laufen lassen. Ändert sich der DSP, mit `turbo-render --check-all` (= `make check`) gegen die Ziele in `docs/PLAN.md` prüfen; eigene WAVs mit `--in`. Reine Doku-Änderungen brauchen keinen Testlauf.

## Offbeat-Familie

- Geschwister: offbeat-engine (Kick/Bass/Hat/Stab/Pad/Riser), offbeat-topline (Melodie), offbeat-turbo (Master-Lautheit), offbeat-hydraulics (Pumpen). Übersicht: `docs/FAMILIE.md` in offbeat-engine.
- **Gemeinsamer Code** ist eine bewusste Kopie aus offbeat-engine (Quelle steht im Dateikopf): `src/dsp/ParamInfo.hpp`, `src/dsp/ParamText.*`, `src/dsp/Blocks.hpp`, `plugin/OffbeatKit.hpp`, `plugin/OffbeatParam.hpp`, `tools/Measure.hpp`. Änderungen daran zuerst in offbeat-engine machen und dann hierher kopieren.
- **Nie ändern:** LV2-URI `https://github.com/lukestark-glitch/offbeat-turbo`, VST3-IDs `Ofbt`/`OfbU`, Bundle-Name `offbeat-turbo`, LADSPA-Datei `offbeat-turbo-ladspa.dylib` (LADSPA-ID kommt aus der Unique-ID). Sonst laden gespeicherte LMMS-Projekte nicht mehr. Parameter nur **hinten anhängen**.
- **LMMS:** Für den Master die LADSPA-Version nehmen (Carla Rack ist in LMMS ein Instrument, Effekte darin wirken nur auf denselben Carla). LADSPA hat keine eigene Oberfläche und kein Tempo; Turbo braucht keins.
- Arbeit auf Branches, PR öffnen, nach grünen Checks selbst mergen und pushen. `docs/HANDOFF.md` kurz aktuell halten. Nach jeder fertigen Änderung `make install`.

## Regeln

- **Keine Screenshots, keine Browser-Tests.** Klang wird über `turbo-render` und Messwerte geprüft. Was nur der Nutzer prüfen kann (Laden in Carla, Hören in LMMS), kommt als kurze Checkliste in den PR.
- **Entscheidungen, die beim Nutzer liegen:** die vorsichtigere, leicht änderbare Option wählen, unter „Offene Fragen“ im Plan notieren und weitermachen. Nicht warten.
- **Installation:** Die eigenen Offbeat-Plugins in `~/Library/Audio/Plug-Ins/VST3/`, `LV2/` und `LADSPA/` sowie die eigene LADSPA-Datei im LMMS-LADSPA-Ordner (`laddir` aus `~/.lmmsrc.xml`, vom Nutzer freigegeben) dürfen ohne Nachfrage überschrieben werden. Andere Plugins und Dateien außerhalb des Projektordners nicht anfassen.
- **Komplett blockiert:** Push-Benachrichtigung senden und, wenn möglich, an einem anderen Teil weiterarbeiten.
- **Sichern:** Nach getaner Arbeit committen (mit den üblichen Attribution-Zeilen) und pushen, damit nichts verloren geht. Wenn `docs/HANDOFF.md` existiert, kurz aktualisieren (was fertig ist, was als Nächstes kommt); sonst überspringen.

## Benachrichtigungen (PushNotification)

- Eine, wenn eine Frage an den Nutzer offen ist: Frage und gewählter Standard (1–2 Sätze), dann weitermachen.
- Eine, wenn alles fertig ist, mit allen PR- bzw. Repo-Links.
- Eine, wenn du blockiert bist oder wegen des Session-Limits aufhörst, mit Stand und nächstem Schritt.
- Keine anderen Fortschrittsmeldungen.
