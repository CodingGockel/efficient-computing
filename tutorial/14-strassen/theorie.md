# Kapitel 14 — Der Strassen-Algorithmus

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/14-Strassen.pdf` (8 Folien)
> **Zeitbedarf:** ca. 12 min
> **Voraussetzungen:** Kapitel 05 (Matrixprodukt, Blocking), Master-Theorem

---

## 1. Worum es geht

Kapitel 05 hat gezeigt, wie man die **Cache-Komplexität** $Q$ des Matrixprodukts senkt, ohne
die Operationszahl $W(n) = \Theta(n^3)$ anzutasten. Strassen greift die andere Seite an: Er
senkt $W(n)$ selbst — von $\Theta(n^3)$ auf $\Theta(n^{2{,}807})$. Und er zeigt, dass die
naive Schranke $n^3$ **kein Naturgesetz** ist.

---

## 2. Die Kernpunkte

### 2.1 Der naive Algorithmus

Drei geschachtelte Schleifen:

$$W(n) = \Theta(n^3)$$

$n^3$ Multiplikationen und $n^3$ Additionen — es sieht so aus, als bräuchte man für jedes der
$n^2$ Ergebnisse ein Skalarprodukt der Länge $n$.

### 2.2 Der blockrekursive Algorithmus

Partitioniere $A$, $B$, $C$ in je vier $\frac n2 \times \frac n2$-Blöcke:

$$\begin{pmatrix} C_{11} & C_{12} \\ C_{21} & C_{22}\end{pmatrix} =
\begin{pmatrix} A_{11} & A_{12} \\ A_{21} & A_{22}\end{pmatrix}
\begin{pmatrix} B_{11} & B_{12} \\ B_{21} & B_{22}\end{pmatrix}$$

$$\begin{aligned}
C_{11} &= A_{11}B_{11} + A_{12}B_{21} & C_{12} &= A_{11}B_{12} + A_{12}B_{22}\\
C_{21} &= A_{21}B_{11} + A_{22}B_{21} & C_{22} &= A_{21}B_{12} + A_{22}B_{22}
\end{aligned}$$

**Beobachtung:** **acht** rekursive Aufrufe pro Ebene und **vier**
$\frac n2 \times \frac n2$-Additionen.

$$T(n) = 8\,T\!\left(\frac n2\right) + \Theta(n^2)
\quad\Longrightarrow\quad T(n) = \Theta\!\left(n^{\log_2 8}\right) = \Theta(n^3)$$

**Rekursion allein bringt also nichts.** Das ist derselbe Algorithmus in anderer
Klammersetzung — nützlich für den Cache (Kapitel 05), nicht für die Operationszahl.

### 2.3 Strassens Beobachtung

Die acht Produkte lassen sich durch **sieben** ersetzen — auf Kosten von mehr Additionen:

$$\begin{aligned}
M_1 &= (A_{11} + A_{22})(B_{11} + B_{22}) \\
M_2 &= (A_{21} + A_{22})\,B_{11} \\
M_3 &= A_{11}\,(B_{12} - B_{22}) \\
M_4 &= A_{22}\,(B_{21} - B_{11}) \\
M_5 &= (A_{11} + A_{12})\,B_{22} \\
M_6 &= (A_{21} - A_{11})(B_{11} + B_{12}) \\
M_7 &= (A_{12} - A_{22})(B_{21} + B_{22})
\end{aligned}$$

$$\begin{aligned}
C_{11} &= M_1 + M_4 - M_5 + M_7 & C_{12} &= M_3 + M_5 \\
C_{21} &= M_2 + M_4 & C_{22} &= M_1 - M_2 + M_3 + M_6
\end{aligned}$$

**Buchführung:** 10 Additionen für die $M_i$, 8 für die $C_{ij}$ — zusammen die **achtzehn**
$\frac n2 \times \frac n2$-Additionen aus der Vorlesung, bei **sieben** rekursiven Aufrufen.

### 2.4 Die Komplexität

$$T(n) = 7\,T\!\left(\frac n2\right) + \Theta(n^2)$$

Master-Theorem, Fall 1 ($\log_2 7 \approx 2{,}807 > 2$, also dominiert die Rekursion):

$$\boxed{T(n) = \Theta\!\left(n^{\log_2 7}\right) = \Theta\!\left(n^{2{,}807}\right)}$$

Der Vergleich der drei Varianten:

| Algorithmus | Rekursive Aufrufe | Additionen | Rekurrenz | Komplexität |
|---|---|---|---|---|
| naiv (Schleifen) | — | — | — | $\Theta(n^3)$ |
| blockrekursiv | 8 | 4 | $8T(n/2) + \Theta(n^2)$ | $\Theta(n^{\log_2 8}) = \Theta(n^3)$ |
| **Strassen** | **7** | **18** | $7T(n/2) + \Theta(n^2)$ | $\Theta(n^{\log_2 7}) = \Theta(n^{2{,}807})$ |

**Die entscheidende Einsicht:** Die Additionen ($\Theta(n^2)$) fallen asymptotisch **nicht ins
Gewicht**. Es zählt allein die Zahl der rekursiven Aufrufe im Exponenten. Deshalb lohnt sich
der Tausch „ein Produkt weniger gegen vierzehn Additionen mehr".

Der Gewinn in Zahlen, gegenüber $n^3$:

| $n$ | $n^3 / n^{2{,}807}$ |
|---|---|
| 128 | 2,6 |
| 1024 | 3,8 |
| 4096 | 5,0 |
| $10^6$ | 14,3 |

Ein Faktor, der langsam wächst — aber unbeschränkt.

### 2.5 Warum trotzdem fast niemand Strassen benutzt

Vier praktische Gründe, die man kennen sollte:

| Problem | Erläuterung |
|---|---|
| **Numerische Stabilität** | Strassen bildet Differenzen von Teilmatrizen ($A_{21} - A_{11}$ usw.). Die Fehlerschranke ist schwächer als beim naiven Algorithmus — er ist nicht *strongly stable*. |
| **Speicherbedarf** | Die sieben $M_i$ und die Zwischensummen müssen zusätzlich gehalten werden. |
| **Konstante und Cutoff** | Die 18 Additionen und die Rekursionsverwaltung machen Strassen für kleine $n$ **langsamer**. In der Praxis rekursiert man nur bis zu einem Cutoff (typisch $n \approx 128$–1024) und schaltet dann auf ein optimiertes GEMM um. |
| **Hardwarefreundlichkeit** | Das naive Produkt ist perfekt blockbar, vektorisierbar und cache-freundlich (Kapitel 05, 12). Strassen zerstört die regelmäßige Struktur teilweise und nutzt SIMD/Tensor-Cores schlechter aus — der asymptotische Gewinn wird von der schlechteren Konstante lange aufgefressen. |

Genau derselbe Konflikt wie bei Barnes-Hut (Kapitel 08): **bessere Komplexität gegen
schlechtere Regularität.**

### 2.6 Ausblick: der Exponent $\omega$

Strassen (1969) war der erste Beweis, dass der Exponent der Matrixmultiplikation **unter 3**
liegt. Seitdem wurde er weiter gedrückt (Coppersmith–Winograd und Nachfolger, heute etwa
$\omega < 2{,}372$), aber diese Verfahren sind *galaktische Algorithmen*: Ihre Konstanten sind
so groß, dass sie erst für absurd große $n$ gewinnen.

Die untere Schranke ist trivialerweise $\omega \ge 2$ (die Ausgabe hat $n^2$ Einträge). Ob
$\omega = 2$ gilt, ist offen.

**Strassen ist das einzige dieser Verfahren, das praktisch eingesetzt wird** — und auch das
nur für große, gutartige Probleme.

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Komplexität des naiven Matrixprodukts | $\Theta(n^3)$ |
| Rekurrenz des blockrekursiven Algorithmus | $T(n) = 8T(n/2) + \Theta(n^2) \Rightarrow \Theta(n^3)$ |
| Warum bringt Blockrekursion allein nichts? | 8 Aufrufe → $\log_2 8 = 3$; der Exponent ändert sich nicht |
| Wie viele Produkte und Additionen bei Strassen? | **7** rekursive Produkte, **18** Additionen von $\frac n2\times\frac n2$-Matrizen |
| Rekurrenz von Strassen | $T(n) = 7T(n/2) + \Theta(n^2)$ |
| Komplexität von Strassen | $\Theta(n^{\log_2 7}) = \Theta(n^{2{,}807})$ |
| Warum fallen die Additionen nicht ins Gewicht? | sie sind $\Theta(n^2)$; im Master-Theorem dominiert der Rekursionsterm $n^{\log_2 7}$ |
| Nenne drei Nachteile in der Praxis | schlechtere numerische Stabilität, mehr Speicher, große Konstante / schlechtere Cache- und SIMD-Ausnutzung |
| Was ist ein Cutoff und wozu? | ab kleinem $n$ auf das naive GEMM umschalten, weil Strassen dort langsamer ist |
| Untere Schranke für den Exponenten | $\omega \ge 2$ — die Ausgabe hat $n^2$ Einträge |

---

## 4. Merkkasten

> - **Blockrekursion allein ändert nichts**: 8 Aufrufe → $\log_2 8 = 3$.
> - **Strassen: 7 Produkte, 18 Additionen** → $T(n) = 7T(n/2) + \Theta(n^2)$
>   → $\Theta(n^{\log_2 7}) \approx \Theta(n^{2{,}807})$.
> - Im Master-Theorem zählt **nur die Zahl der rekursiven Aufrufe** im Exponenten — Additionen
>   sind $\Theta(n^2)$ und asymptotisch gratis.
> - Praktisch: **Cutoff** und optimiertes GEMM darunter; Stabilität und
>   Hardwarefreundlichkeit sind die Gegenargumente.
> - Zwei Hebel, zwei Kapitel: **Kapitel 05 senkt $Q$, Kapitel 14 senkt $W$.**

---

## 5. Verbindung

**Ergänzt Kapitel 05.** Dort wurde $W(n) = \Theta(n^3)$ als gegeben hingenommen und $Q$ von
$\Theta(n^3)$ auf $\Theta(n^3/(B\sqrt M))$ gesenkt. Hier wird $W$ selbst angegriffen. Beide
Hebel sind unabhängig und lassen sich kombinieren — praktische Strassen-Implementierungen
sind unterhalb des Cutoffs geblockt.

**Querverbindung zu Kapitel 08:** Dieselbe Spannung wie bei Barnes-Hut — die asymptotisch
bessere Methode ist die unregelmäßigere, und bei moderaten Größen gewinnt der naive
Algorithmus. Wer optimiert, muss beide Achsen sehen: Komplexitätsklasse **und** Konstante.

**Querverbindung zu Kapitel 12:** Warum cuBLAS bei GEMM praktisch unschlagbar ist — es
investiert vollständig in die Konstante (Tensor-Cores, Register-Tiling, Pipelining) statt in
den Exponenten.
