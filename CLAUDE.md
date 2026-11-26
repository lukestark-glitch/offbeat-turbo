# Offbeat – Arbeitsregeln für Claude

Stand und offene Punkte: `offbeat/docs/STAND.md`. Phasen A/B und CI-Regeln: `~/Developer/OFFBEAT_ZUSATZ.md`.

## Sparregel (gilt ab 2026-11-26, ersetzt „Agents ohne Bedenken“)

- Einfache, klar umrissene Jobs (Dateien/Funktionen finden, Doku, Presets eintragen, Tests laufen lassen) an Subagents mit haiku oder sonnet. Opus nur für Entwurf (DSP, Architektur, Server-Design) und das Prüfen der Ergebnisse.
- Höchstens 2 Subagents gleichzeitig.
- Gezielt suchen (graphify-out, sonst grep), nur Ausschnitte lesen, Dateien nicht mehrfach lesen, DPF nicht durchsuchen.
- Nur betroffene Tests und nur das betroffene Repo bauen.
- Kurze Antworten und Commit-Nachrichten, keine Zwischen-Zusammenfassungen.
