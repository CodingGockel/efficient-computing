# Kapitel 08 — n-Body und Barnes-Hut

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/7-Lecture-Openmp-Barneshut.pdf` (13 Folien), Übungsblatt
> `project/excercise7.pdf` (Aufgabe 7.1)
> **Zeitbedarf:** ca. 15 min
> **Voraussetzungen:** Kapitel 07 (Tasks, Baumrekursion)

---

## 1. Worum es geht

Die direkte n-Body-Simulation kostet $\Theta(n^2)$ Operationen **pro Zeitschritt** — bei
$n = 10^6$ Körpern und tausenden Zeitschritten ist das aussichtslos. Barnes-Hut senkt das auf
$\Theta(n \log n)$, indem es weit entfernte Körpergruppen zu **einem** Ersatzkörper
zusammenfasst. Das ist der Punkt, an dem ein besserer **Algorithmus** mehr bringt als jede
Parallelisierung.

---

## 2. Die Kernpunkte

### 2.1 Das Problem

Jeder Körper $i$ ist beschrieben durch Masse $m_i$, Position $c_i = (x_i, y_i)$ und
Geschwindigkeit $v_i = (v_i^x, v_i^y)$. Die Gravitation zwischen zwei Körpern hängt ab von
der Gravitationskonstante $G$, den beiden Massen und dem Abstand $d = \|c_i - c_j\|$.

Für $n$ Körper:

$$\frac{dv_i}{dt} = \sum_{i \ne j} \frac{G\,m_j\,(c_i - c_j)}{\|c_i - c_j\|^3}$$

### 2.2 Zeitintegration: explizites Euler-Verfahren

Mit äquidistanter Diskretisierung $t_k = t_0 + \varepsilon k$:

$$v_i^{[k+1]} = v_i^{[k]} + \varepsilon \sum_{i \ne j} \frac{G\,m_j\,(c_i - c_j)}{\|c_i - c_j\|^3}$$
$$c_i^{[k+1]} = c_i^{[k]} + \varepsilon\, v_i^{[k]}$$

Als Programm — drei geschachtelte Schleifen:

```cpp
for (int k = 0; k < nt; ++k) {            // Zeitschritte
    for (int i = 0; i < n; ++i) {         // jeder Koerper
        for (int j = 0; j < n; ++j) {     // gegen jeden anderen
            if (i != j) { /* Geschwindigkeit aktualisieren */ }
        }
        /* Koordinaten aktualisieren */
    }
}
```

$$\boxed{\Theta(n^2)\ \text{Operationen pro Zeitschritt}}$$

Wichtig für die Parallelisierung: Die **äußere** Schleife über $i$ ist datenparallel (jeder
Körper schreibt nur sein eigenes $v_i$), die Zeitschleife über $k$ dagegen **strikt
sequentiell** — Schritt $k+1$ braucht das Ergebnis von $k$.

### 2.3 Die Idee: Clustering

Statt alle paarweisen Kräfte zu berechnen, fasst man Partikel, die **nahe beieinander, aber
weit vom betrachteten Partikel entfernt** sind, zu **einem Ersatzpartikel** zusammen — mit
der Gesamtmasse des Clusters, angesetzt in dessen Massenschwerpunkt.

Physikalisch ist das eine Näherung: Aus großer Entfernung ist ein Sternhaufen von einem
einzelnen schweren Körper kaum zu unterscheiden.

### 2.4 Quadtree

Das (rechteckige) Gebiet wird **rekursiv in vier gleich große Teilgebiete** zerlegt, bis jedes
Teilgebiet **höchstens einen** Partikel enthält. Diese Zerlegung ist ein **Quadtree**: ein
Baum, dessen Knoten entweder Blätter oder selbst Teilbäume sind.

In jedem inneren Knoten speichert man die **Gesamtmasse** und den **Massenschwerpunkt** des
Teilgebiets. (In 3D heißt dieselbe Struktur *Octree* mit acht Kindern.)

### 2.5 Das Abbruchkriterium

Ein Teilgebiet darf als **ein** Cluster behandelt werden, wenn

$$\boxed{\frac{l}{d} \le \theta}$$

mit

- $l$ = **Kantenlänge des Teilgebiets**,
- $d$ = **Abstand** des aktuellen Partikels zum **Massenschwerpunkt** des Teilgebiets,
- $\theta$ = vorgegebener Schwellwert.

Ist das Kriterium erfüllt, wird die Kraft **einmal** gegen den Ersatzkörper gerechnet.
Andernfalls steigt man in die vier Kinder ab und prüft dort erneut.

| $\theta$ | Wirkung |
|---|---|
| $\theta = 0$ | nie zusammenfassen → exakt, aber wieder $\Theta(n^2)$ |
| $\theta \approx 0{,}5$ | üblicher Kompromiss |
| $\theta$ groß | schnell, aber ungenau |

**$\theta$ ist der Genauigkeits-Geschwindigkeits-Regler des Verfahrens.**

### 2.6 Der Algorithmus und seine Komplexität

```cpp
for (int k = 0; k < nt; ++k) {
    // 1. Quadtree neu aufbauen (Massen und Massenschwerpunkte)
    // 2. Geschwindigkeiten durch rekursive Baumtraversierung aktualisieren
    // 3. Koordinaten aktualisieren
}
```

Für jeden der $n$ Partikel besucht die Traversierung im Mittel $O(\log n)$ Knoten, weil das
$\theta$-Kriterium den Abstieg abbricht, sobald ein Teilgebiet weit genug entfernt ist:

$$\boxed{\Theta(n \log n)\ \text{pro Zeitschritt}}$$

Der Baum muss in **jedem** Zeitschritt neu gebaut werden, weil sich die Partikel bewegen —
das kostet ebenfalls $O(n \log n)$ und fällt damit nicht ins Gewicht.

### 2.7 Was das für die Parallelisierung bedeutet

Die Vorlesung zeigt das **Speicherzugriffsmuster** von Barnes-Hut — und es ist unruhig. Drei
Konsequenzen:

| Eigenschaft | Folge |
|---|---|
| **Irreguläre Zugriffe** — Baumtraversierung ist Pointer Chasing | schlechte Lokalität, kein Prefetching (Kapitel 04) |
| **Ungleiche Arbeit pro Partikel** — in dichten Regionen wird tiefer abgestiegen | Lastungleichgewicht → `schedule(dynamic)` oder **Tasks** |
| **Rekursive, unbekannt tiefe Struktur** | genau der Fall, für den `#pragma omp task` gemacht ist (Kapitel 07) |

Praktisch: Die Schleife über die Partikel mit `#pragma omp parallel for schedule(dynamic)`
parallelisieren, oder den Baumaufbau/-durchlauf mit Tasks und einem **Cutoff** ab einer
gewissen Tiefe (Aufgabe 7.6 in Kapitel 07 misst genau diesen Effekt).

> **Der Preis der besseren Komplexität:** Der $\Theta(n^2)$-Algorithmus ist *perfekt*
> parallelisierbar, cache-freundlich und vektorisierbar — die GPU-Fassung auf Blatt 11
> erreicht damit sehr hohe Effizienz. Barnes-Hut ist asymptotisch weit besser, aber irregulär
> und schlecht auf SIMD/SIMT abzubilden. Bei kleinem $n$ gewinnt deshalb oft der naive
> Algorithmus.

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Komplexität der direkten n-Body-Simulation | $\Theta(n^2)$ pro Zeitschritt |
| Formel für die Geschwindigkeitsänderung | $\dfrac{dv_i}{dt} = \sum_{i \ne j} \dfrac{G m_j (c_i - c_j)}{\|c_i-c_j\|^3}$ |
| Was macht das explizite Euler-Verfahren? | $v^{[k+1]} = v^{[k]} + \varepsilon \frac{dv}{dt}$, $c^{[k+1]} = c^{[k]} + \varepsilon v^{[k]}$ |
| Grundidee von Barnes-Hut | weit entfernte Partikelgruppen als **einen** Ersatzkörper (Gesamtmasse im Massenschwerpunkt) behandeln |
| Welche Datenstruktur, wie aufgebaut? | Quadtree — rekursive Vierteilung, bis höchstens ein Partikel pro Teilgebiet |
| Was steht in einem inneren Knoten? | Gesamtmasse und Massenschwerpunkt des Teilgebiets |
| Abbruchkriterium | $l/d \le \theta$ ($l$ Gebietsgröße, $d$ Abstand zum Massenschwerpunkt) |
| Komplexität von Barnes-Hut | $\Theta(n\log n)$ pro Zeitschritt |
| Warum muss der Baum jeden Schritt neu gebaut werden? | die Partikel bewegen sich; die Gebietszuordnung ändert sich |
| Welche Schleife ist parallelisierbar, welche nicht? | die Schleife über die Partikel ja, die Zeitschleife nein |
| Warum ist Barnes-Hut schwer zu parallelisieren? | irreguläre Speicherzugriffe und ungleiche Arbeit pro Partikel → Lastungleichgewicht |

---

## 4. Merkkasten

> - Direkt: $\Theta(n^2)$. Barnes-Hut: $\Theta(n\log n)$. **Der Algorithmus schlägt die
>   Parallelisierung** — 8 Kerne bringen Faktor 8, ein besseres Verfahren bringt Faktor
>   $n/\log n$.
> - **Quadtree**: rekursive Vierteilung bis ≤ 1 Partikel; innere Knoten tragen Masse und
>   Massenschwerpunkt.
> - **$l/d \le \theta$** ist das ganze Verfahren in einer Zeile. $\theta$ regelt Genauigkeit
>   gegen Geschwindigkeit.
> - Die Zeitschleife ist **inhärent seriell** — sie ist das $\alpha$ in Amdahl (Kapitel 10).
> - Bessere Komplexität, schlechtere Regularität: Barnes-Hut ist cache- und SIMD-feindlich.

---

## 5. Verbindung

**Baut auf:** Kapitel 07 — rekursive Baumtraversierung mit unbekannter Struktur ist der
Musterfall für `#pragma omp task`; die Cutoff-Frage aus Aufgabe 7.6 ist hier praktisch
relevant. Kapitel 04 erklärt, warum die Zeigerverfolgung im Baum teuer ist.

**Querverbindung:** Blatt 11 (Kapitel 12) macht die Roofline-Analyse des **direkten**
n-Body-Kernels auf der GPU, naiv gegen gekachelt — dort geht es um die Konstante, hier um die
Komplexitätsklasse. Beides zusammen ist die vollständige Antwort auf „wie beschleunige ich
eine n-Body-Simulation".

**Eigene Abgabe:** `project/` enthält die Barnes-Hut-Implementierung zu Blatt 7, Aufgabe 7.1.
