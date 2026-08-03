# Kapitel 13 — PRAM, Pointer Jumping und List Ranking

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/13-Pointerjumping.pdf` (15 Folien)
> **Zeitbedarf:** ca. 15 min
> **Voraussetzungen:** Kapitel 10 (Speedup, Arbeit)

---

## 1. Worum es geht

Bisher ging es um reale Hardware. Dieses Kapitel wechselt auf ein **theoretisches
Maschinenmodell** — die PRAM — und zeigt daran eine Technik, die ein scheinbar unteilbar
sequentielles Problem parallelisiert: **List Ranking** auf einer verketteten Liste. Nebenbei
liefert es den Begriff, mit dem man parallele Algorithmen wirklich bewertet:
**Arbeitseffizienz**.

---

## 2. Die Kernpunkte

### 2.1 Das PRAM-Modell

**Parallel Random Access Machine.** Die Annahmen sind bewusst großzügig — es geht um die
*inhärente* Parallelisierbarkeit eines Problems, nicht um reale Maschinen:

- alle $p$ Prozessoren können **gleichzeitig** aus dem gemeinsamen Speicher lesen und in ihn
  schreiben,
- alle Prozessoren sind **eng synchronisiert** (Gleichtakt),
- **Laufzeit = Anzahl der Speicherzugriffe**,
- **konstante Kosten** pro Speicherzugriff.

Die letzten beiden Punkte sind genau das, was Kapitel 04 als unrealistisch entlarvt hat — im
PRAM-Modell gibt es keine Cache-Hierarchie und keine Latenz. Das ist Absicht: Man will die
untere Schranke der *Parallelität*, nicht die reale Laufzeit.

### 2.2 Die vier Varianten

| | Lesen | Schreiben |
|---|---|---|
| **E**xclusive | keine zwei Prozesse lesen gleichzeitig dieselbe Zelle | keine zwei schreiben gleichzeitig dieselbe Zelle |
| **C**oncurrent | mehrere dürfen gleichzeitig dieselbe Zelle lesen | mehrere dürfen gleichzeitig dieselbe Zelle schreiben |

Daraus: **EREW**, **CREW**, **CRCW** (und theoretisch ERCW). EREW ist am schwächsten, CRCW am
stärksten.

**Bei CRCW muss geregelt werden, was beim gleichzeitigen Schreiben passiert:**

| Strategie | Regel |
|---|---|
| **Common** | alle Prozesse schreiben denselben Wert (sonst undefiniert) |
| **Arbitrary** | gespeichert wird der Wert eines zufällig ausgewählten Prozesses |
| **Priority** | der Wert eines festgelegten Prozesses (z. B. kleinster Index) |
| **Combining** | die **Verknüpfung** aller Werte (Summe, Maximum, …) |

*Combining* ist die stärkste Variante — sie entspricht im Grunde einem `atomicAdd` in Hardware
(Kapitel 12).

### 2.3 Das Problem: verkettete Liste

Bei einer **Linked List** ist die Reihenfolge der Elemente **nicht** durch die Reihenfolge der
Adressen bestimmt, sondern durch die Verweise. Daraus folgt:

- Die Speicheradresse des Elements an Position $i$ ist **nur durch Traversierung** der Kette
  erreichbar.
- Paralleler Zugriff auf einzelne Elemente ist ohne Weiteres nicht möglich.

Genau deshalb ist eine verkettete Liste der Prototyp der „nicht parallelisierbaren"
Datenstruktur — und derselbe Grund, warum sie in Kapitel 07 das Standardmotiv für
`#pragma omp task` ist.

### 2.4 List Ranking

**Aufgabe:** Bestimme für jedes Element seinen **Rang** — den Abstand zum Ende der Liste.
Die Länge $n$ sei **unbekannt** (sonst wäre die Aufgabe trivial).

**Seriell:** einmal durchlaufen und rückwärts aufsummieren:

$$\Theta(n)$$

Das ist offensichtlich optimal — jedes Element muss mindestens einmal angefasst werden.

### 2.5 Pointer Jumping

Die parallele Lösung. Idee: **Jeder Knoten überspringt in jeder Runde seinen Nachfolger** und
addiert dessen Rang zum eigenen. Dadurch **verdoppelt sich die überbrückte Distanz in jedem
Schritt**.

```cpp
// Initialisierung, alle i parallel
rank[i] = (next[i] != nullptr) ? 1 : 0;

// solange es noch Verweise gibt
while (es gibt ein i mit next[i] != nullptr) {
    // alle i parallel
    if (next[i] != nullptr) {
        rank[i] = rank[i] + rank[next[i]];   // Rang des Nachfolgers dazu
        next[i] = next[next[i]];             // den Nachfolger ueberspringen
    }
}
```

Beide Zuweisungen müssen die **alten** Werte lesen — das ist der Punkt, an dem die enge
Synchronisation der PRAM gebraucht wird. Auf realer Hardware braucht es dafür einen
Doppelpuffer oder eine Barriere zwischen Lese- und Schreibphase (dieselbe Race, die in
Kapitel 06 mit `#pragma omp barrier` behandelt wird).

**Analyse:**

| Größe | Wert | Begründung |
|---|---|---|
| **Zeit** (parallele Schritte) | $\Theta(\log n)$ | die Distanz verdoppelt sich pro Runde: $1, 2, 4, \ldots, n$ |
| **Arbeit** (Speicherzugriffe gesamt) | $\Theta(n \log n)$ | $n$ Prozessoren arbeiten in jeder der $\log n$ Runden |
| **Prozessoren** | $n$ | einer pro Listenelement |

### 2.6 Arbeitseffizienz — der Begriff, um den es geht

> **Definition.** Ein Algorithmus $A$ heißt **arbeitseffizient** (*work-efficient*) bezüglich
> $B$, wenn seine Arbeit bis auf einen konstanten Faktor mit der von $B$ übereinstimmt.

Damit lautet das Fazit der Vorlesung:

$$\boxed{\text{ListRankingParallel ist } \textbf{nicht arbeitseffizient, aber zeiteffizient.}}$$

- **Zeiteffizient:** $\Theta(\log n)$ statt $\Theta(n)$ — eine exponentielle Verbesserung der
  Tiefe.
- **Nicht arbeitseffizient:** $\Theta(n\log n)$ Gesamtarbeit gegenüber $\Theta(n)$ seriell —
  man verrichtet um den Faktor $\log n$ **mehr** Arbeit.

**Warum das wichtig ist:** Mit $p \ll n$ Prozessoren — also in der Realität — ist die Laufzeit
etwa $\text{Arbeit}/p = n\log n / p$. Gegenüber dem seriellen $\Theta(n)$ lohnt sich das erst
ab $p > \log n$. **Zusätzliche Arbeit muss durch genügend Prozessoren erst wieder
hereingeholt werden.**

Das ist dieselbe Buchführung wie bei Arbeit und Tiefe in Kapitel 07: $T_1$ (Arbeit) gegen
$T_\infty$ (Tiefe), verbunden über die Brent-Schranke
$\max(T_\infty, T_1/p) \le T(p) \le T_1/p + T_\infty$.

### 2.7 Wofür die Technik sonst taugt

Pointer Jumping ist das Grundmuster einer ganzen Familie:

- **Präfixsumme (Scan)** — dieselbe Verdopplungsstruktur auf einem Array: $\Theta(\log n)$
  Zeit, $\Theta(n \log n)$ Arbeit in der naiven Fassung. (Es gibt arbeitseffiziente Varianten
  mit $\Theta(n)$ Arbeit — Blelloch-Scan.)
- **Wurzelfinden in Wäldern**, Zusammenhangskomponenten
- **Baumkontraktion**

Auf der GPU steckt genau dieses Verdopplungsmuster in der Baumreduktion aus Kapitel 12 —
`for (int s = blockDim.x/2; s > 0; s /= 2)` ist Pointer Jumping rückwärts.

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Was sind die Annahmen der PRAM? | $p$ Prozessoren, gemeinsamer Speicher, gleichzeitiges Lesen/Schreiben, enge Synchronisation, Laufzeit = Speicherzugriffe, konstante Zugriffskosten |
| Nenne die PRAM-Varianten | EREW, CREW, CRCW (und ERCW) |
| Welche CW-Strategien gibt es? | Common, Arbitrary, Priority, Combining |
| Warum ist eine verkettete Liste schwer zu parallelisieren? | die Adresse eines Elements ist nur durch Traversierung der Verweise erreichbar |
| Was ist List Ranking? | für jedes Element den Abstand zum Listenende bestimmen |
| Komplexität seriell | $\Theta(n)$ |
| Idee des Pointer Jumping | jeder Knoten überspringt seinen Nachfolger und addiert dessen Rang — die Distanz verdoppelt sich je Runde |
| Zeit und Arbeit des parallelen Algorithmus | Zeit $\Theta(\log n)$, Arbeit $\Theta(n\log n)$ |
| Wann heißt ein Algorithmus arbeitseffizient? | wenn seine Arbeit bis auf einen konstanten Faktor der des Vergleichsalgorithmus entspricht |
| Ist Pointer Jumping arbeitseffizient? | **nein** — $\Theta(n\log n)$ statt $\Theta(n)$; aber zeiteffizient |
| Ab wann lohnt es sich praktisch? | ab $p > \log n$, da die Mehrarbeit durch Prozessoren kompensiert werden muss |

---

## 4. Merkkasten

> - **PRAM** ist ein Modell für *inhärente Parallelität*, nicht für reale Laufzeit — kein
>   Cache, keine Latenz.
> - **EREW / CREW / CRCW**; bei CRCW: Common, Arbitrary, Priority, Combining.
> - **Pointer Jumping:** `rank[i] += rank[next[i]]; next[i] = next[next[i]];` — die überbrückte
>   Distanz **verdoppelt** sich je Runde.
> - **Zeit $\Theta(\log n)$, Arbeit $\Theta(n\log n)$** gegenüber $\Theta(n)$ seriell.
> - **Zeiteffizient, aber nicht arbeitseffizient.** Wer die Tiefe senkt, zahlt oft mit Arbeit —
>   und braucht dann genügend Prozessoren, um es hereinzuholen.

---

## 5. Verbindung

**Querverbindung zu Kapitel 07:** Dort tauchen dieselben Größen unter anderen Namen auf —
Arbeit $T_1$, Tiefe $T_\infty$, Parallelität $T_1/T_\infty$, Brent-Schranke. Die verkettete
Liste ist dort das Standardbeispiel für `#pragma omp task`; hier bekommt sie ein
theoretisches Fundament.

**Querverbindung zu Kapitel 12:** Die Baumreduktion im Shared Memory ist dasselbe
Verdopplungsmuster — in $\log_2(\text{blockDim})$ Schritten von $n$ Werten auf einen.
Dort ist sie allerdings arbeitseffizient ($n-1$ Additionen), weil in jeder Runde nur die
noch aktiven Threads arbeiten.

**Querverbindung zu Kapitel 10:** „Zeiteffizient, aber nicht arbeitseffizient" ist die
theoretische Fassung dessen, was der Overhead-Term $T_{\text{over}}(n;p)$ praktisch beschreibt.
