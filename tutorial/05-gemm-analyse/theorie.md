# Kapitel 05 — GEMM im idealen Cache-Modell: Blocking

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/5-Analysis-GEMM.pdf` (29 Folien),
> `vl/extra-material/Ex-5-IdealCache-II.pdf` (71 Folien)
> **Zeitbedarf:** ca. 20 min
> **Voraussetzungen:** Kapitel 04 ($M$, $B$, $Q(n;M,B)$)

---

## 1. Worum es geht

Das Matrixprodukt braucht $\Theta(n^3)$ Operationen — daran ändert Blocking nichts. Was sich
ändert, ist die **Cache-Komplexität**: von $\Theta(n^3)$ Misses auf $\Theta(n^3/(B\sqrt{M}))$.
Das ist ein Faktor $B\sqrt{M}$ — in typischen Zahlen mehrere Hundert, bei **identischer**
Operationszahl. Dieses Kapitel ist der Beweis dafür, dass $W(n)$ allein nichts über die
Laufzeit sagt.

---

## 2. Die Kernpunkte

### 2.1 Speicherlayout: erst klären, dann analysieren

$$Z \leftarrow Z + X \cdot Y, \qquad X, Y, Z \in \mathbb{R}^{n \times n}$$

Eine Matrix liegt linear im Speicher, und **wie** entscheidet über alles:

| Layout | Formel | benachbart im Speicher | Sprachen |
|---|---|---|---|
| **Row-Major** | `A[i*n + j]` | Elemente **einer Zeile** | C, C++, Python/NumPy |
| **Column-Major** | `A[i + j*n]` | Elemente **einer Spalte** | Fortran, MATLAB, **cuBLAS** |

**Merksatz:** Die Schleife, die am schnellsten läuft, muss über die Dimension laufen, die
im Speicher dicht liegt — bei Row-Major also über den **Spaltenindex**.

### 2.2 Der naive Algorithmus

```cpp
for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j)
        for (int k = 0; k < n; ++k)
            Z[i*n + j] += X[i*n + k] * Y[k*n + j];
```

$$W(n) = \Theta(n^3)$$

**Cache-Analyse** (Row-Major, $n > M/B$, also passt keine ganze Zeile dauerhaft in den Cache):

| Zugriff | Muster in der inneren Schleife | Misses |
|---|---|---|
| `Z[i][j]` | in $k$ **konstant** — bleibt im Register/Cache | $\Theta(n^2/B)$ |
| `X[i][k]` | läuft eine **Zeile** entlang → Stride 1 | $\Theta(n^3/B)$ |
| `Y[k][j]` | läuft eine **Spalte** entlang → Stride $n$ | $\Theta(n^3)$ |

$$\boxed{Q_{\text{naiv}}(n; M, B) = \Theta(n^3)}$$

Die Spaltenzugriffe auf `Y` dominieren: Jeder einzelne ist ein Miss, weil die geladene
Cache-Line 64 Byte enthält, von denen genau 8 gebraucht werden — und bis man den Rest
brauchte, ist sie längst verdrängt.

### 2.3 Der geblockte Algorithmus

Zerlege alle drei Matrizen in $s \times s$-Teilmatrizen und multipliziere blockweise:

```cpp
for (int ii = 0; ii < n; ii += s)
    for (int jj = 0; jj < n; jj += s)
        for (int kk = 0; kk < n; kk += s)
            // s x s Teilprodukt: passt komplett in den Cache
            for (int i = ii; i < ii + s; ++i)
                for (int j = jj; j < jj + s; ++j)
                    for (int k = kk; k < kk + s; ++k)
                        Z[i*n + j] += X[i*n + k] * Y[k*n + j];
```

**Die Wahl von $s$** ist der ganze Trick: Es müssen **drei** $s \times s$-Blöcke gleichzeitig
in den Cache passen.

$$3 s^2 \le M \qquad \Longrightarrow \qquad s = \Theta(\sqrt{M})$$

**Cache-Analyse:**

- Es gibt $(n/s)^3$ Block-Tripel.
- Pro Tripel werden 3 Blöcke geladen: $\Theta(s^2/B)$ Misses (jede Blockzeile liegt
  zusammenhängend, $s \ge B$ vorausgesetzt).
- Innerhalb des Tripels ist danach **jeder** Zugriff ein Hit.

$$Q_{\text{block}} = \left(\frac{n}{s}\right)^3 \cdot \Theta\!\left(\frac{s^2}{B}\right)
= \Theta\!\left(\frac{n^3}{s\,B}\right)
= \boxed{\Theta\!\left(\frac{n^3}{B\sqrt{M}}\right)}$$

### 2.4 Der Gewinn, in Zahlen

$$\frac{Q_{\text{naiv}}}{Q_{\text{block}}} = B\sqrt{M}$$

Beispiel: 32 KB L1-Cache, `double` (8 Byte), 64-Byte-Lines:

$$B = \frac{64}{8} = 8\ \text{Wörter}, \qquad M = \frac{32\,768}{8} = 4096\ \text{Wörter}, \qquad \sqrt{M} = 64$$

$$B\sqrt{M} = 8 \cdot 64 = \mathbf{512}$$

**Faktor 512 weniger Cache-Misses — bei exakt derselben Zahl an Multiplikationen und
Additionen.** In der Praxis erreicht man diesen Faktor nicht ganz (der reale Cache ist nicht
voll-assoziativ und nicht optimal), aber Faktor 5–20 in der **Laufzeit** ist ein normales
Ergebnis.

Blockgröße konkret: $s \le \sqrt{4096/3} \approx 37$, praktisch nimmt man 32.

### 2.5 Untere Schranke — Blocking ist optimal

$$Q(n; M, B) = \Omega\!\left(\frac{n^3}{B\sqrt{M}}\right)$$

(Hong & Kung, 1981 — I/O-Komplexität der Matrixmultiplikation.) Der geblockte Algorithmus
erreicht diese Schranke asymptotisch. **Er ist also nicht nur besser, sondern optimal**;
weitere Umsortierungen können die Größenordnung nicht mehr verbessern.

### 2.6 Der einfachere Fall: Matrix-Vektor-Produkt

Aus `Ex-5-IdealCache-II.pdf`, mit $A \in \mathbb{R}^{m \times n}$ in Row-Major:

**Fall 1 — alles passt** ($mn + m + n \ll M$):

$$Q \approx m\frac{n}{B} + \frac{n}{B} + \frac{m}{B} \approx \frac{mn}{B}$$

Jedes Element von $A$ wird genau einmal geholt, und die Line wird voll ausgenutzt. Das ist
das Optimum — weniger als $mn/B$ geht nicht, denn $A$ muss einmal gelesen werden.

**Fall 2 — $y \leftarrow A x$ passt nicht** ($mn \gg M$): $A$ wird weiterhin **zeilenweise**
durchlaufen, also bleibt es bei $Q \approx mn/B$. Der Vektor $x$ wird $m$-mal neu gelesen,
trägt aber nur $\Theta(mn/B)$ bei — dieselbe Größenordnung.

**Fall 3 — $y \leftarrow A^{\mathsf T} x$** (transponiert, $A$ weiterhin Row-Major): Jetzt wird
$A$ **spaltenweise** durchlaufen. Jeder Zugriff ein Miss:

$$Q \approx mn$$

$$\boxed{\text{Faktor } B \text{ Unterschied zwischen } Ax \text{ und } A^{\mathsf T}x —
\text{bei identischer Operationszahl.}}$$

Genau darauf verweist auch die CUDA-Vorlesung, wenn sie schreibt, dass sich $Ax$ und
$A^{\mathsf T}x$ in der Performance unterscheiden (Kapitel 12).

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Row-Major vs. Column-Major | `A[i*n+j]` (Zeilen zusammenhängend, C/C++) vs. `A[i+j*n]` (Spalten, Fortran/cuBLAS) |
| $W(n)$ des Matrixprodukts | $\Theta(n^3)$ — für beide Varianten gleich |
| $Q$ des naiven Algorithmus | $\Theta(n^3)$, dominiert durch die Spaltenzugriffe auf $Y$ |
| Wie wählt man die Blockgröße? | so, dass **drei** $s\times s$-Blöcke in den Cache passen: $3s^2 \le M$, also $s = \Theta(\sqrt M)$ |
| $Q$ des geblockten Algorithmus | $\Theta\!\left(n^3/(B\sqrt M)\right)$ |
| Wie groß ist der Gewinn? | Faktor $B\sqrt M$ — z. B. 512 bei $B=8$, $M=4096$ |
| Ist Blocking optimal? | ja — die untere Schranke ist $\Omega(n^3/(B\sqrt M))$ (Hong & Kung) |
| $Q$ für $Ax$ und $A^{\mathsf T}x$ bei Row-Major | $mn/B$ bzw. $mn$ — Faktor $B$ |
| Warum ändert Blocking $W(n)$ nicht? | es werden dieselben Produkte gebildet, nur in anderer Reihenfolge |

---

## 4. Merkkasten

> - **$W(n)$ und $Q(n;M,B)$ sind zwei verschiedene Dinge.** Blocking lässt $W$ unverändert und
>   senkt $Q$ um $B\sqrt{M}$.
> - **Naiv: $Q = \Theta(n^3)$. Geblockt: $Q = \Theta(n^3/(B\sqrt M))$.** Diese zwei Formeln
>   sind der Kern des Kapitels.
> - Die Blockgröße folgt aus **drei Blöcken im Cache**: $s = \Theta(\sqrt M)$.
> - Die untere Schranke $\Omega(n^3/(B\sqrt M))$ macht Blocking **optimal**.
> - Immer der Speicherordnung folgen: bei Row-Major läuft die **innerste** Schleife über den
>   Spaltenindex.

---

## 5. Verbindung

**Baut auf:** Kapitel 04 ($M$, $B$, ideales Cache-Modell).

**Ist die CPU-Fassung von Kapitel 12.** Das gekachelte CUDA-Matrixprodukt ist derselbe
Algorithmus mit demselben Argument — nur mit einem Unterschied: Hier *hofft* man, dass die
Kachel im Cache bleibt; dort legt man sie mit `__shared__` **explizit** hinein und bestimmt
$s$ selbst (dort `TILE`). Auch die Kennzahl ist dieselbe, nur anders benannt: Die
arithmetische Intensität $I = T/4$ in Kapitel 12 ist die Kehrseite von $Q = \Theta(n^3/(sB))$.

**Kapitel 14** (Strassen) greift den anderen Hebel an: nicht $Q$, sondern $W(n)$ selbst —
von $\Theta(n^3)$ auf $\Theta(n^{2{,}807})$.
