# Spieldaten auf die Quest importieren

Du benötigst deine eigene installierte **PC-Vollversion von Colin McRae Rally
2.0** und die OpenCMR2-VR-APK. Die APK enthält keine Strecken, Fahrzeuge, Musik
oder andere Original-Spieldaten.

## Einmalige Einrichtung

1. Installiere die APK auf deiner Quest.
2. Öffne auf dem PC den Installationsordner des Originalspiels. Darin liegen
   unter anderem `Game`, `FrontEnd` und `Sounds`.
3. Erstelle eine **ZIP des gesamten Spielordners**, zum Beispiel über die
   ZIP-Komprimierung im Windows-Explorer. Ein zusätzlicher übergeordneter Ordner
   in der ZIP ist in Ordnung. Verwende eine normale, unverschlüsselte ZIP.
4. Verbinde die Quest per USB mit dem PC und erlaube den Dateizugriff im Headset.
   Kopiere die ZIP in den Ordner **Download** der Quest.
5. Öffne **OpenCMR2 VR** in der App-Bibliothek (bei einer lokal installierten APK
   unter „Unbekannte Quellen“). Die Einrichtung erscheint als Fenster.
6. Drücke **Select game data ZIP**, öffne **Downloads** in der Dateiauswahl
   und wähle deine ZIP. Der Import startet nach der Auswahl.
7. Warte auf **Import complete** und drücke **Start game**.

Die App prüft Pflichtdateien, ZIP-Prüfsummen und freien Speicherplatz. Deine
Dateien bleiben auf deinen Geräten. Nach dem erfolgreichen Import kannst du die
ZIP im Download-Ordner selbst löschen. Beim nächsten normalen App-Start öffnet
sich das Spiel direkt. Ein APK-Update behält Daten und Spielstände; eine
Deinstallation entfernt den privaten App-Speicher.

## Später erneut importieren oder VR testen

Öffne **OpenCMR2 VR · Game data**, den zweiten Eintrag in der App-Bibliothek.
Dort kannst du das Spiel starten, andere Spieldaten importieren oder den
**VR test without game data** öffnen. Beende ein laufendes natives Spiel vor einem
erneuten Import. Die App ersetzt bisherige Daten erst nach erfolgreicher Prüfung;
Spielstände und Einstellungen bleiben erhalten.

## Wenn etwas nicht klappt

- **ZIP nicht sichtbar:** Prüfe, ob die Kopie fertig ist und die Datei im
  Download-Ordner liegt. Wähle in der Dateiauswahl „Downloads“.
- **Fehlende Dateien:** Die Meldung nennt die fehlenden oder leeren Pflichtdateien.
  Packe den gesamten installierten PC-Spielordner ein, nicht nur einzelne Archive.
- **ZIP beschädigt:** Erstelle die ZIP erneut und kopiere sie vollständig auf die
  Quest. Verschlüsselte ZIPs und andere Archivformate werden nicht unterstützt.
- **Nicht genug Speicher:** Die Meldung zeigt benötigten und verfügbaren Platz.
  Während des Imports liegen die ausgewählte ZIP, eine private Arbeitskopie und
  die entpackten Daten gleichzeitig auf der Quest. Bei einem Austausch bleiben
  zudem die bisherigen Daten bis zum erfolgreichen Abschluss erhalten.
- **Import abgebrochen / App geschlossen:** Öffne die Einrichtung erneut. Eine
  unterbrochene Übernahme wird wiederhergestellt; wähle die ZIP danach erneut aus.

Unterstützte Grenzen: ZIP bis 4 GiB, entpackte Daten bis 8 GiB, höchstens 50.000
Archiveinträge. Der Import benötigt zusätzlich 64 MiB freien Reserveplatz.
