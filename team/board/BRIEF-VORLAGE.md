# <Karten-ID>: <Titel>

Meilenstein: M<n> · Design: Rev <n>, Abschnitt <…> · Agent: test-writer → implementer

## Fakten

Was der Agent wissen muss, ohne den Chatverlauf zu kennen: Ausgangslage, betroffene Funktionen
(mit `datei:zeile`), fachliche Regeln, bekannte Fallstricke.

## Eigene Dateien

Nur diese Dateien dürfen geändert oder angelegt werden:

- `tests/<…>`
- `<quellcode>/<…>`

## Bewertung

Woran der `reviewer` „fertig“ festmacht:

- [ ] `./scripts/gate.sh` grün
- [ ] Testfälle decken ab: …
- [ ] Verhalten entspricht Design Rev <n>, Abschnitt <…>
- [ ] Keine Änderungen außerhalb der eigenen Dateien
