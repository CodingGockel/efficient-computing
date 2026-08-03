# Kapitel 01 — Einführung: Effizienzbegriff, axpy, Zeitmessung

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/1-Lecture-Einführung.pdf` (14 Folien), Übungsblatt `excercises/uebung2.pdf`,
> eigene Abgabe `assignment2/`
> **Zeitbedarf:** ca. 12 min

---

## 1. Worum es geht

Die Vorlesung fragt nach den **Prinzipien**, die den rasanten Leistungszuwachs von Rechnern
möglich gemacht haben — und danach, wo er heute an Grenzen stößt. Die Einführung steckt das
Feld ab: Woran misst man „effizient" überhaupt, und auf welchen Ebenen kann man eingreifen?

---

## 2. Die Kernpunkte

### 2.1 Es gibt nicht *eine* Zielfunktion

Rechner werden gegen ganz verschiedene Größen optimiert:

| Zielfunktion | Einheit | typischer Kontext |
|---|---|---|
| Preis | USD | Massenmarkt, Embedded |
| Volumen | m³ | mobile Geräte, Rechenzentrum |
| elektrische Leistung | W | Akkulaufzeit, Kühlung, Betriebskosten |
| Operationen pro Zeit | FLOP/s | HPC, Supercomputing |

Die **größte Zahl an Computern steckt in Embedded Systems** (Autoschlüssel, Rauchmelder,
Waschmaschine) — dort zählen feste Funktionalität und geringe Produktionskosten, nicht
FLOP/s. „Effizient" ist also immer relativ zur Zielfunktion.

Wer eine Optimierung bewertet, muss deshalb zuerst sagen, **welche** Größe gemeint ist.
Ein GPU-Kernel kann in FLOP/s gewinnen und in FLOP/Joule verlieren.

### 2.2 Die Ebenen, auf denen man ansetzen kann

Von oben nach unten — und der Hebel ist oben am größten:

1. **Problemformulierung** — was genau soll berechnet werden?
2. **Modellierung** (fein- vs. grobgranular)
3. **Algorithmus** — hier liegen die Größenordnungen (Barnes-Hut statt n², Kapitel 08)
4. **Abbildung der Daten auf Ressourcen** — Layout, Verteilung (Kapitel 04, 12)
5. Programmiersprache
6. **Implementierung** — Schleifenordnung, Blocking, Parallelisierung
7. Kompilationssystem — Optimierungsstufen
8. **Rechnerarchitektur** — Pipelining, SIMD, Caches (Kapitel 02, 04, 05b)

Ein besserer Algorithmus schlägt fast jede Mikrooptimierung. Deshalb steht Punkt 3 vor
Punkt 6 — und deshalb ist ein `-O3` selten die Antwort.

### 2.3 Der historische Befund

Preis pro FLOP/s ist über Jahrzehnte um viele Größenordnungen gefallen — der Vergleich
ENIAC (1946) gegen einen ARM Cortex-M3 (2004) macht das drastisch. Die Vorlesung stellt
daran drei Fragen, die den Rest des Semesters strukturieren:

- **Warum** konnte diese Entwicklung stattfinden? → Kapitel 03 (Miniaturisierung)
- **Wohin** geht es? → Power Wall, Parallelität (Kapitel 03, 06 ff.)
- **Welche Prinzipien** liegen darunter? → Kapitel 02, 04

### 2.4 `axpy` — das Arbeitspferd

$$y \leftarrow \alpha x + y, \qquad \alpha \in \mathbb{R},\ x, y \in \mathbb{R}^n$$

```cpp
for (int i = 0; i < n; ++i)
    y[i] = alpha * x[i] + y[i];
```

Warum ausgerechnet dieses Beispiel das ganze Semester durchhält:

- **2 FLOP pro Element**, 3 Speicherzugriffe (12 Byte bei `float`) → arithmetische Intensität
  1/6 FLOP/Byte. Es ist der Prototyp eines **speichergebundenen** Kernels (Kapitel 10, 12).
- Alle Iterationen sind **unabhängig** — der einfachste Fall für OpenMP (Kapitel 06) und CUDA
  (Kapitel 11).
- Es ist BLAS Level 1 und damit in jeder ernsthaften Bibliothek vermessen.

### 2.5 Richtig messen

Die Punkte, die in `assignment2/` praktisch geübt wurden und in jedem späteren Kapitel
wieder auftauchen:

| Regel | Grund |
|---|---|
| **Immer mit Optimierung übersetzen** (`-O2`) | ohne sie misst man den Debug-Code, nicht das Programm |
| **Aufwärmlauf verwerfen** | erster Durchlauf zahlt Cache-Misses und Seitenfehler |
| **Mehrfach messen, Minimum nehmen** | Störungen machen eine Messung immer langsamer, nie schneller |
| **Ergebnis benutzen** | sonst optimiert der Compiler die Schleife weg |
| **Über mehrere Größenordnungen von `n` messen** | ein einzelner Punkt zeigt keinen Trend und kein Cache-Verhalten |
| **Wall-Clock, nicht CPU-Zeit** | bei Parallelität ist CPU-Zeit die Summe über alle Threads |

Die abgeleitete Größe ist meist nicht die Zeit selbst, sondern:

$$\text{FLOP/s} = \frac{\text{Anzahl Operationen}}{T}, \qquad \text{GB/s} = \frac{\text{bewegte Bytes}}{T}$$

Bei `axpy` mit `double`: $2n$ FLOP und $24n$ Byte. **Bei einem speichergebundenen Kernel ist
GB/s die aussagekräftige Zahl**, nicht FLOP/s.

Werkzeug im Tutorial: [`_common/bench.hpp`](../_common/bench.hpp) (`ec::best_of`).

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Nenne vier Zielfunktionen der Rechnerentwicklung | Preis, Volumen, elektrische Leistung, Operationen pro Zeit |
| Auf welchen Ebenen kann man Effizienz beeinflussen? | Problemformulierung, Modellierung, Algorithmus, Datenabbildung, Sprache, Implementierung, Compiler, Architektur |
| Was ist `axpy`? | $y \leftarrow \alpha x + y$, BLAS-1, 2 FLOP pro Element |
| Warum Minimum statt Mittelwert bei Messungen? | Störungen verlängern nur; das Minimum ist die störungsärmste Beobachtung |
| Warum ist die Zeitmessung ohne `-O2` wertlos? | man misst nicht denselben Maschinencode, den man ausliefert |
| Wann ist GB/s die richtige Kennzahl statt FLOP/s? | wenn der Kernel speichergebunden ist (kleine arithmetische Intensität) |

---

## 4. Merkkasten

> - „Effizient" ist ohne Angabe der **Zielfunktion** bedeutungslos.
> - Der Hebel ist **oben** am größten: Algorithmus vor Implementierung vor Compilerflag.
> - `axpy` ist der Prototyp eines **speichergebundenen** Kernels — 2 FLOP auf 12 bzw. 24 Byte.
> - Messen heißt: optimiert übersetzen, aufwärmen, mehrfach messen, Minimum nehmen, Ergebnis
>   benutzen, über mehrere `n` messen.

---

## 5. Verbindung

**Weiter zu:** Kapitel 02 (wie die Hardware Befehle ausführt), Kapitel 03 (warum die
Taktfrequenz stehengeblieben ist), Kapitel 10 (Speedup und Roofline als quantitative
Bewertung). `axpy` kommt in Kapitel 06 (OpenMP), 11 (CUDA) und 12 (Roofline) jeweils wieder.

**Eigene Abgabe:** `assignment2/` — Benchmark über `n` mit Plot je `-O`-Stufe;
`assignment2/plot_benchmark.py` ist die Vorlage für alle späteren Messreihen.
