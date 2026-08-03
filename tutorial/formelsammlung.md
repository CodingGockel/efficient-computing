# Formelsammlung — Efficient Computing / Parallel Computing 2

Alle rechnerisch verwertbaren Formeln des Kurses an einer Stelle, mit Symbolbedeutung und
Kapitelverweis. Gedacht zum Nachschlagen während des Rechnens — die Herleitungen stehen in
den Kapiteln.

**Legende:** ⚑ = besonders klausurrelevant · ★ = Schwerpunktthema (OpenMP/CUDA)

---

## Inhalt

| # | Abschnitt | |
|---|---|---|
| 1 | [Grundgrößen: Speedup, Effizienz, Raten](#1-grundgrößen) | ⚑ |
| 2 | [Rechnerarchitektur: Pipelining und Peak-Performance](#2-rechnerarchitektur) | ⚑ |
| 3 | [Miniaturisierung und Dennard Scaling](#3-miniaturisierung) | ⚑ |
| 4 | [Cache: Adressen, Misses, Lokalität](#4-cache) | ⚑ |
| 5 | [GEMM im idealen Cache-Modell](#5-gemm-im-idealen-cache-modell) | ⚑ |
| 6 | [Amdahl, Gustafson, Roofline](#6-amdahl-gustafson-roofline) | ⚑ |
| 7 | [Parallele Algorithmen: Arbeit, Tiefe, Task-Graph](#7-parallele-algorithmen) | ★ |
| 8 | [n-Body und Barnes-Hut](#8-n-body-und-barnes-hut) | |
| 9 | [Ray Tracing](#9-ray-tracing) | |
| 10 | [CUDA: Index, Grid, Occupancy](#10-cuda--index-grid-occupancy) | ★⚑ |
| 11 | [CUDA: Speicher, Coalescing, Bankkonflikte](#11-cuda--speicher-coalescing-bankkonflikte) | ★⚑ |
| 12 | [Pipelines und Streams (allgemein)](#12-pipelines-und-streams) | |
| 13 | [Pointer Jumping / PRAM](#13-pointer-jumping--pram) | |
| 14 | [Strassen und Rekurrenzen](#14-strassen-und-rekurrenzen) | |
| — | [Referenzwerte und Einheiten](#referenzwerte-und-einheiten) | |
| — | [Welche Formel für welche Frage?](#welche-formel-für-welche-frage) | |

---

## 1. Grundgrößen

| Größe | Formel | Symbole | Kap. |
|---|---|---|---|
| **Speedup** ⚑ | $S(n;p) = \dfrac{T^*(n)}{T(n;p)}$ | $T^*$ = schnellster **serieller** Algorithmus, $T(n;p)$ = parallel auf $p$ Prozessoren | 10 |
| Speedup (schwächere Variante) | $S(n;p) = \dfrac{T(n;1)}{T(n;p)}$ | paralleler Algorithmus auf 1 Prozessor — **schmeichelt**, weil $T(n;1) > T^*(n)$ | 10 |
| **Effizienz** ⚑ | $E(n;p) = \dfrac{S(n;p)}{p}$ | $E = 1$ heißt perfekte Skalierung | 10 |
| Serielle Laufzeit, zerlegt | $T^*(n) = T_{\text{ser}}(n) + T_{\text{par}}(n)$ | inhärent seriell + parallelisierbar | 10 |
| Parallele Laufzeit, zerlegt | $T(n;p) \ge T_{\text{ser}} + \dfrac{T_{\text{par}}}{p} + T_{\text{over}}(n;p)$ | $T_{\text{over}}$ = Overhead (Barrieren, Kommunikation, Lastungleichgewicht) | 10 |
| Allgemeine Speedup-Schranke | $S \le \dfrac{T_{\text{ser}} + T_{\text{par}}}{T_{\text{ser}} + T_{\text{par}}/p + T_{\text{over}}}$ | Amdahl und Gustafson sind Spezialfälle | 10 |
| **Rechenrate** | $\text{FLOP/s} = \dfrac{\text{Anzahl Operationen}}{T}$ | bei rechengebundenen Kerneln aussagekräftig | 01 |
| **Bandbreite** | $\text{GB/s} = \dfrac{\text{bewegte Bytes}}{T}$ | bei **speichergebundenen** Kerneln die richtige Kennzahl | 01 |
| Laufzeitmodell mit Speicher | $T = \alpha F + \beta G$ | $F$ = CPU-Operationen, $G$ = Speicherbewegungen, $\alpha \approx 1$ ns, $\beta \approx 50$ ns | 04 |

> **Merke:** $\alpha \ll \beta$. Das vereinfachte $T = \alpha F$ ist nur brauchbar, wenn
> Speicherbewegungen vernachlässigbar sind — was praktisch nie zutrifft.

---

## 2. Rechnerarchitektur

### Pipelining ⚑

$n$ Instanzen, $k$ Teilaufgaben je Instanz, Zeit $t$ pro Teilaufgabe:

| Größe | Formel |
|---|---|
| ohne Pipelining | $T(n,k,t) = n\,k\,t$ |
| **mit Pipelining** | $T_{\text{Pipe}}(n,k,t) = \underbrace{k\,t}_{\text{Füllen}} + \underbrace{(n-1)\,t}_{\text{Fließbetrieb}} = (n+k-1)\,t$ |
| **Speedup** | $S = \dfrac{n\,k}{n+k-1} = \dfrac{k}{1 + \frac{k-1}{n}} \ \xrightarrow{n\to\infty}\ k$ |

**Der maximale Speedup ist die Pipeline-Tiefe $k$.** (Kap. 02)

### Peak-Performance ⚑

$$P_{\text{peak}} = \text{Takt} \times \frac{\text{elem. FLOP}}{\text{Instruktion}} \times \text{Vektorlänge} \times \text{Throughput}$$

| Faktor | typischer Wert |
|---|---|
| Takt | z. B. $1{,}5 \cdot 10^9$ Hz |
| FLOP/Instruktion | **2** bei FMA (Multiplikation + Addition) |
| Vektorlänge | 4 bei `fmla v0.4s` (4 × `float`), 2 bei `.2d` |
| Throughput | $1/\text{CPI}$; 1 bei einer Einheit, 2 bei superskalar |

Beispiel: $1{,}5\cdot10^9 \times 2 \times 4 \times 1 = 12$ GFLOP/s. (Kap. 05b)

**Gilt nur bei:** keine RaW-Abhängigkeiten, gleichartige Instruktionskette, warme Pipeline,
kein Speicherengpass. Es ist eine **obere Schranke**.

---

## 3. Miniaturisierung

| Größe | Formel | Bedeutung | Kap. |
|---|---|---|---|
| Bitwechselzeit | $T = V/Q$, skaliert: $t = \alpha T$ | Volumen $V$ auf $\alpha V$ verkleinert → Zeit um $\alpha$ kürzer | 03 |
| **Kapazität** | $C = \dfrac{2\pi\varepsilon L}{\ln(R/r)} \ \sim\ L$ | proportional zur **Länge** | 03 |
| längste Strecke im Würfel | $\sqrt{3}\,L = \sqrt{3}\,V^{1/3}$ | Raumdiagonale bei Volumen $V = L^3$ | 03 |
| **minimale Taktzeit** | $t_{\min} \ge \dfrac{\sqrt{3}\,V^{1/3}}{g} \ \sim\ L$ | $g$ = Ausbreitungsgeschwindigkeit | 03 |
| **maximale Taktrate** | $f_{\max} \le \dfrac{g}{\sqrt{3}\,V^{1/3}} \ \sim\ \dfrac{1}{L}$ | kleiner ⇒ schneller | 03 |
| elektrische Arbeit | $W = Q \cdot U$, mit $Q = C \cdot U$ | | 03 |
| **elektrische Leistung** ⚑ | $P = \dfrac{W}{T} = W f = C \cdot U^2 \cdot f$ | die zentrale Formel | 03 |

### Dennard Scaling ⚑

Generation 1 → 2 mit Skalenfaktor $\alpha < 1$, Spannungsfaktor $\beta < 1$, $k$-fache
Bauelementezahl:

$$L_2 = \alpha L_1, \quad C_2 = \alpha C_1, \quad f_2 = \frac{f_1}{\alpha}, \quad U_2 = \beta U_1$$

$$P_2 = k\,C_2 U_2^2 f_2 = k\,(\alpha C_1)(\beta U_1)^2 \frac{f_1}{\alpha} = \boxed{k\,\beta^2 \cdot P_1}$$

**$\alpha$ kürzt sich heraus.** Konstante Leistung ⇔ $k\beta^2 = 1$.

$$k = 2 \ \Longrightarrow\ \beta = \frac{1}{\sqrt 2} \approx 0{,}7$$

---

## 4. Cache

### Adressaufteilung ⚑

$$\text{Adresse} = [\ \text{Tag}\ |\ \text{Index}\ |\ \text{Offset}\ ]$$

| Größe | Formel |
|---|---|
| Bits der physischen Adresse | $\log_2(\text{Speichergröße in Byte})$ |
| **Offset**-Bits | $\log_2(\text{Line-Größe in Byte})$ |
| Anzahl Cache-Lines | $\dfrac{\text{Cache-Größe}}{\text{Line-Größe}}$ |
| Anzahl Gruppen (Sets) | $\dfrac{\text{Anzahl Cache-Lines}}{L}$ &nbsp; ($L$ = Assoziativität) |
| **Index**-Bits | $\log_2(\text{Anzahl Gruppen})$ — voll-assoziativ: **0** |
| **Tag**-Bits | Adressbits − Index-Bits − Offset-Bits |
| Anzahl Speicherblöcke | $\dfrac{\text{Speichergröße}}{\text{Line-Größe}}$ |
| Block-ID einer Adresse | $\left\lfloor \dfrac{\text{Adresse}}{\text{Line-Größe}} \right\rfloor$ |
| Gruppe eines Blocks | $\text{Block-ID} \bmod \text{Anzahl Gruppen}$ |

**Sonderfälle:** $L = 1$ → direkte Zuordnung; $L = \#\text{Lines}$ → voll-assoziativ
(dann Index-Bits $= 0$).

### Trefferquoten

$$\text{Hit-Ratio} = \frac{\#\text{Hits}}{\#\text{Zugriffe}}, \qquad
\text{Miss-Ratio} = \frac{\#\text{Misses}}{\#\text{Zugriffe}} = 1 - \text{Hit-Ratio}$$

### Ideales Cache-Modell ⚑

| Parameter | Bedeutung |
|---|---|
| $M$ | Cache-Größe in **Wörtern** |
| $B$ | Wörter pro Cache-Line |
| **Tall-Cache-Annahme** | $M = \Omega(B^2)$ |
| $W(n)$ | **Work** — Anzahl der Operationen |
| $Q(n; M, B)$ | **Cache-Komplexität** — Anzahl der Cache-Misses |

### Cache-Misses nach Zugriffsmuster

| Muster | Code | $Q$ |
|---|---|---|
| Stride 1 | `a[i]` | $\approx N/B$ |
| Stride $k < B$ | `a[k*i]` | $\approx kN/B$ |
| Stride $k \ge B$ | `a[k*i]` | $\approx N$ |
| zufällig / Pointer Chasing | `a[Index(i)]` | $\approx N$ |

**Faktor $B$ zwischen bestem und schlechtestem Fall — bei identischer Operationszahl.**

---

## 5. GEMM im idealen Cache-Modell

$Z \leftarrow Z + X\cdot Y$ mit $X,Y,Z \in \mathbb{R}^{n\times n}$:

| Variante | $W(n)$ | $Q(n;M,B)$ | Kap. |
|---|---|---|---|
| naiv (Row-Major, $n > M/B$) | $\Theta(n^3)$ | $\boldsymbol{\Theta(n^3)}$ | 05 |
| **geblockt**, $s = \Theta(\sqrt M)$ | $\Theta(n^3)$ | $\boldsymbol{\Theta\!\left(\dfrac{n^3}{B\sqrt M}\right)}$ | 05 |
| untere Schranke (Hong & Kung) | — | $\Omega\!\left(\dfrac{n^3}{B\sqrt M}\right)$ | 05 |

**Blockgröße:** drei $s\times s$-Blöcke müssen in den Cache passen:

$$3s^2 \le M \quad\Longrightarrow\quad s = \Theta(\sqrt M)$$

**Gewinn durch Blocking:**

$$\frac{Q_{\text{naiv}}}{Q_{\text{block}}} = B\sqrt{M}$$

Beispiel: 32 KB Cache, `double`, 64-B-Lines → $B = 8$, $M = 4096$, $\sqrt M = 64$,
**Faktor 512**.

### Matrix-Vektor-Produkt, $A \in \mathbb{R}^{m\times n}$ in Row-Major

| Fall | $Q$ |
|---|---|
| alles passt ($mn + m + n \ll M$) | $\approx m\frac nB + \frac nB + \frac mB \approx \dfrac{mn}{B}$ |
| $y \leftarrow Ax$, passt nicht | $\approx \dfrac{mn}{B}$ (zeilenweiser Durchlauf) |
| $y \leftarrow A^{\mathsf T}x$, passt nicht | $\approx mn$ (spaltenweiser Durchlauf) |

**Faktor $B$ zwischen $Ax$ und $A^{\mathsf T}x$.**

### Speicherlayout

$$\text{Row-Major: } A[i\cdot n + j] \qquad\qquad \text{Column-Major: } A[i + j\cdot n]$$

---

## 6. Amdahl, Gustafson, Roofline

### Amdahl — Strong Scaling ⚑

Festes $n$, $T_{\text{over}} = 0$, $T_{\text{ser}} = \alpha\,T(n;1)$:

$$\boxed{S(n;p) = \frac{1}{\alpha + \dfrac{1-\alpha}{p}}} \qquad\qquad
\lim_{p\to\infty} S = \boxed{\frac{1}{\alpha}}$$

Mit einem **endlichen Beschleunigungsfaktor $s$** für den parallelen Teil (z. B. GPU):

$$S = \frac{1}{\alpha + \dfrac{1-\alpha}{s}}$$

| $\alpha$ | 0,50 | 0,10 | 0,05 | 0,01 | 0,001 |
|---|---|---|---|---|---|
| $S_{\max}$ | 2 | 10 | 20 | 100 | 1000 |

### Gustafson — Weak Scaling ⚑

Feste **parallele** Laufzeit, $T_{\text{ser}} = \gamma\,T(n;p)$:

$$\boxed{S(n;p) = p + \gamma\,(1-p)}$$

Probe: $\gamma = 0 \Rightarrow S = p$; $\gamma = 1 \Rightarrow S = 1$.

| | Amdahl | Gustafson |
|---|---|---|
| fest | Problemgröße $n$ | Laufzeit $T(n;p)$ |
| Bezug von $\alpha$ / $\gamma$ | $T(n;1)$ (seriell) | $T(n;p)$ (parallel) |
| Grenzwert | $1/\alpha$ (beschränkt) | linear in $p$ |

### Roofline ⚑

$$I = \frac{\text{FLOPs}}{\text{aus dem DRAM bewegte Bytes}} \quad [\text{FLOP/Byte}]$$

$$\boxed{P \le \min\bigl(\pi,\ \beta \cdot I\bigr)} \qquad\qquad
\boxed{I^\star = \frac{\pi}{\beta}}$$

$\pi$ = Spitzenrechenleistung [GFLOP/s], $\beta$ = Spitzenbandbreite [GB/s],
$I^\star$ = **Knickpunkt** (*ridge point*).

| Bereich | Diagnose | Hebel |
|---|---|---|
| $I < I^\star$ | **speichergebunden** | Bytes sparen: Lokalität, Blocking, Fusion, kleinerer Datentyp |
| $I > I^\star$ | **rechengebunden** | Operationen: FMA, SIMD, Tensor-Cores, Bibliotheken |

**Zählregeln für $I$:** nur DRAM-Bytes (Cache-Treffer zählen nicht) · Schreiben zählt wie
Lesen · Register zählen nicht · FMA = 2 FLOP · `double` halbiert $I$.

### Arithmetische Intensität typischer Kernel (`float`)

| Kernel | FLOP | Byte | $I$ |
|---|---|---|---|
| Kopie `y = x` | 0 | 8 | 0 |
| `x[i] += y[i]` | 1 | 12 | 0,083 |
| **SAXPY** `y = a·x + y` | 2 | 12 | 0,167 |
| Skalarprodukt | 2 | 8 | 0,25 |
| `t += x[i]*x[i]` | 2 | 4 | 0,5 |
| `d = a*b*c` | 2 | 16 | 0,125 |
| $\sum (x_i-y_i)^2$ | 3 | 8 | 0,375 |
| Horner, Grad $d$ | $2d$ | 8 | $d/4$ |
| 5-Punkt-Stencil, naiv | 4 | 20 | 0,2 |
| 5-Punkt-Stencil, ideal | 4 | 8 | 0,5 |
| GEMM naiv (je $k$-Schritt) | 2 | 8 | 0,25 |
| **GEMM gekachelt** | — | — | $T/4$ |

---

## 7. Parallele Algorithmen

### Arbeit, Tiefe, Parallelität ★

| Größe | Bedeutung |
|---|---|
| $T_1$ | **Arbeit** — Gesamtzeit auf einem Prozessor (Summe aller Knotengewichte) |
| $T_\infty$ | **Tiefe** / kritischer Pfad — Zeit bei unbegrenzt vielen Prozessoren |
| $T_1/T_\infty$ | **Parallelität** — maximal sinnvolle Prozessorzahl |

**Brent-Schranke** ⚑

$$\boxed{\max\!\left(T_\infty,\ \frac{T_1}{p}\right) \ \le\ T(p) \ \le\ \frac{T_1}{p} + T_\infty}$$

### Task-Graph: Zeitfenster und Slack ⚑

Für jeden Knoten mit Dauer $d$:

| Größe | Formel | Richtung |
|---|---|---|
| **ES** (earliest start) | $\text{ES} = \max_{\text{Vorgänger}} \text{EF}$ | Vorwärts (**max**) |
| **EF** (earliest finish) | $\text{EF} = \text{ES} + d$ | Vorwärts |
| **LF** (latest finish) | $\text{LF} = \min_{\text{Nachfolger}} \text{LS}$ | Rückwärts (**min**) |
| **LS** (latest start) | $\text{LS} = \text{LF} - d$ | Rückwärts |
| **Slack** (Puffer) | $\text{Slack} = \text{LS} - \text{ES} = \text{LF} - \text{EF}$ | |

**Kritischer Pfad** = alle Knoten mit $\text{Slack} = 0$. Seine Länge ist $T_\infty$.

Startwerte: $\text{ES} = 0$ bei Quellen, $\text{LF} = T_\infty$ bei Senken.

### Bernstein-Bedingungen ⚑

Zwei Anweisungen $S_1, S_2$ mit Eingaben $I_j$ und Ausgaben $O_j$ dürfen vertauscht bzw.
parallel ausgeführt werden, genau wenn

$$I_1 \cap O_2 = \emptyset \quad\wedge\quad I_2 \cap O_1 = \emptyset \quad\wedge\quad O_1 \cap O_2 = \emptyset$$

| Verletzung | Abhängigkeit |
|---|---|
| $I_2 \cap O_1 \ne \emptyset$ | **RAW** (*true dependence*) — nicht auflösbar |
| $I_1 \cap O_2 \ne \emptyset$ | **WAR** (*anti dependence*) — durch Umbenennen auflösbar |
| $O_1 \cap O_2 \ne \emptyset$ | **WAW** (*output dependence*) — durch Umbenennen auflösbar |

### Aufrundungsdivision (überall gebraucht) ⚑

$$\left\lceil \frac{n}{t} \right\rceil = \frac{n + t - 1}{t} \quad \text{(Ganzzahldivision)}$$

---

## 8. n-Body und Barnes-Hut

| Größe | Formel | Kap. |
|---|---|---|
| Beschleunigung | $\dfrac{dv_i}{dt} = \displaystyle\sum_{i\ne j} \frac{G\,m_j\,(c_i - c_j)}{\|c_i-c_j\|^3}$ | 08 |
| Euler, Geschwindigkeit | $v_i^{[k+1]} = v_i^{[k]} + \varepsilon \displaystyle\sum_{i\ne j} \frac{G m_j (c_i-c_j)}{\|c_i-c_j\|^3}$ | 08 |
| Euler, Position | $c_i^{[k+1]} = c_i^{[k]} + \varepsilon\, v_i^{[k]}$ | 08 |
| Zeitgitter | $t_k = t_0 + \varepsilon k$ | 08 |
| **Clustering-Kriterium** ⚑ | $\dfrac{l}{d} \le \theta$ | 08 |
| Komplexität direkt | $\Theta(n^2)$ pro Zeitschritt | 08 |
| **Komplexität Barnes-Hut** | $\Theta(n \log n)$ pro Zeitschritt | 08 |

$l$ = Kantenlänge des Teilgebiets, $d$ = Abstand zum **Massenschwerpunkt** des Gebiets,
$\theta$ = Schwellwert ($\theta = 0$ → exakt und $\Theta(n^2)$; $\theta \approx 0{,}5$ üblich).

---

## 9. Ray Tracing

### Kamerabasis (orthonormal, rechtshändig)

$$\boldsymbol f = \frac{V_{la} - C}{\|V_{la} - C\|}, \qquad
\boldsymbol r = \frac{\boldsymbol f \times V_{up}}{\|\boldsymbol f \times V_{up}\|}, \qquad
\boldsymbol u = \boldsymbol r \times \boldsymbol f$$

### Pixel → Strahlrichtung

$$x = \left(\frac{2(i+0{,}5)}{W} - 1\right) a \tan\frac{\theta}{2}, \qquad
y = -\left(\frac{2(j+0{,}5)}{H} - 1\right)\tan\frac{\theta}{2}$$

$$D = \operatorname{normalize}(x\,\boldsymbol r + y\,\boldsymbol u + \boldsymbol f), \qquad
\text{Strahl} = C + \alpha D$$

$W\times H$ = Auflösung, $\theta$ = Öffnungswinkel (FOV), $a$ = Seitenverhältnis,
$+0{,}5$ trifft die Pixelmitte.

### Dreieck und Schnittpunkt

| Größe | Formel |
|---|---|
| Normale | $N = (T_2 - T_1) \times (T_3 - T_1)$ |
| Schnitt eindeutig ⇔ | $\beta = \langle N, D\rangle \ne 0$ |
| **Lineares System** | $\bigl[\,T_2-T_1 \mid T_3-T_1 \mid -D\,\bigr] \begin{pmatrix}\lambda_2\\\lambda_3\\\alpha\end{pmatrix} = C - T_1$ |
| **Treffer** ⇔ | $\lambda_2 \ge 0,\ \lambda_3 \ge 0,\ \lambda_2 + \lambda_3 \le 1,\ \alpha \ge 0$ |
| Schnittpunkt | $H = (1-\lambda_2-\lambda_3)T_1 + \lambda_2 T_2 + \lambda_3 T_3$ |

### Cramersche Regel (3×3)

$$\det M = \det[M_1|M_2|M_3] = \langle M_1,\ M_2 \times M_3\rangle$$

$$x_1 = \frac{\det[b|A_2|A_3]}{\det A}, \qquad
x_2 = \frac{\det[A_1|b|A_3]}{\det A}, \qquad
x_3 = \frac{\det[A_1|A_2|b]}{\det A}$$

### Shading und Komplexität

$$\gamma = \max\bigl(0,\ \langle N, -L\rangle\bigr) \qquad\qquad
\text{Aufwand} = O(\text{Pixel} \times \text{Dreiecke})$$

Nächstes Dreieck: das mit dem **kleinsten nichtnegativen $\alpha$**.

---

## 10. CUDA — Index, Grid, Occupancy

### Globaler Index ⚑ ★

$$\boxed{i = \text{blockIdx.x} \cdot \text{blockDim.x} + \text{threadIdx.x}}$$

**Umkehrung** (aus $i$ zurück):

$$\text{blockIdx.x} = \left\lfloor \frac{i}{\text{blockDim.x}} \right\rfloor, \qquad
\text{threadIdx.x} = i \bmod \text{blockDim.x}$$

**Warp und Lane innerhalb des Blocks:**

$$\text{Warp} = \left\lfloor \frac{\text{threadIdx.x}}{32} \right\rfloor, \qquad
\text{Lane} = \text{threadIdx.x} \bmod 32$$

### Grid-Dimensionierung ⚑

| Größe | Formel |
|---|---|
| Blöcke (1D) | $\left\lceil \dfrac{n}{t} \right\rceil = \dfrac{n + t - 1}{t}$ |
| gestartete Threads | $\text{blocks} \cdot t$ |
| Threads ohne Arbeit | $\text{blocks}\cdot t - n$ |
| Elemente im letzten Block | $n - (\text{blocks}-1)\cdot t$ |
| Warps pro Block | $\left\lceil \dfrac{t}{32} \right\rceil$ |
| belegte Lanes gesamt | $\text{blocks} \cdot \left\lceil \frac{t}{32}\right\rceil \cdot 32$ |

**Immer zusammen:** Aufrunden beim Grid **und** `if (i < n)` im Kernel.

### 2D-Indexierung ⚑

```cpp
int c = blockIdx.x * blockDim.x + threadIdx.x;   // .x = SPALTE
int r = blockIdx.y * blockDim.y + threadIdx.y;   // .y = ZEILE
if (r < H && c < W) { int i = r * W + c; ... }   // Row-Major
```

| Größe | Formel |
|---|---|
| Linearisierung (Row-Major) | $i = r \cdot W + c$ |
| Umkehrung | $r = \lfloor i / W \rfloor$, $c = i \bmod W$ |
| Grid | $\text{blocks.x} = \lceil W/\text{bd.x}\rceil$, $\text{blocks.y} = \lceil H/\text{bd.y}\rceil$ |

### Grid-Stride-Loop

```cpp
int i      = blockIdx.x * blockDim.x + threadIdx.x;
int stride = blockDim.x * gridDim.x;             // = Gesamtzahl Threads T
for (; i < n; i += stride) { ... }
```

| Größe | Formel |
|---|---|
| Gesamtzahl Threads | $T = \text{gridDim.x} \cdot \text{blockDim.x}$ |
| Elemente je Thread | $\lfloor n/T \rfloor$ oder $\lceil n/T \rceil$ |
| Threads mit $\lceil n/T\rceil$ Elementen | $n - \lfloor n/T\rfloor \cdot T$ |

### Occupancy ⚑ ★

$$\text{Occupancy} = \frac{\text{aktive Warps pro SM}}{\text{max. Warps pro SM}}$$

**Residente Blöcke pro SM** = Minimum über alle vier Grenzen:

| Grenze | Formel |
|---|---|
| Register | $\left\lfloor \dfrac{\text{Register pro SM}}{\text{blockDim} \cdot \text{Register pro Thread}} \right\rfloor$ |
| Shared Memory | $\left\lfloor \dfrac{\text{Shared pro SM}}{\text{Shared pro Block}} \right\rfloor$ |
| Warps | $\left\lfloor \dfrac{\text{max. Warps pro SM}}{\lceil \text{blockDim}/32\rceil} \right\rfloor$ |
| Blöcke | Hardware-Limit (A100: 32) |

$$\text{aktive Warps} = \text{Blöcke} \cdot \left\lceil \frac{\text{blockDim}}{32}\right\rceil$$

**Untere Schranke für 100 %:** $\text{blockDim} \ge \dfrac{\text{max. Threads pro SM}}{\text{max. Blöcke pro SM}}$ — auf der A100 also $2048/32 = 64$.

### Little's Law — wie viele Threads braucht die GPU?

$$\text{Bytes gleichzeitig unterwegs} = \text{Bandbreite} \times \text{Latenz}$$

$$\text{Latenz [s]} = \frac{\text{Latenz [Zyklen]}}{\text{Takt [Hz]}}, \qquad
\#\text{Threads} = \frac{\text{Bytes unterwegs}}{\text{Byte pro Thread}}$$

A100-Beispiel: $500/1{,}41\cdot10^9 = 355$ ns; $1555\cdot10^9 \cdot 355\cdot10^{-9} \approx 551$ kB;
$/4$ B $\approx$ **138 000 Threads**.

---

## 11. CUDA — Speicher, Coalescing, Bankkonflikte

### Coalescing ⚑ ★

Ein Warp = 32 Threads × 4 Byte = **128 Byte Nutzdaten**; Sektorgröße 32 Byte.

$$\text{Effizienz} = \frac{\text{gebrauchte Bytes}}{\text{geholte Bytes}} = \frac{128}{32 \cdot \#\text{Sektoren}}$$

Für Schrittweite $s$ (in `float`):

$$\#\text{Sektoren} = \min(32,\ 4s), \qquad
\text{Effizienz}(s) = \frac{4}{\min(32,\ 4s)} = \max\!\left(\frac18,\ \min\!\left(1,\ \frac1s\right)\right)$$

| $s$ | 1 | 2 | 4 | 8 | ≥ 8 |
|---|---|---|---|---|---|
| Sektoren | 4 | 8 | 16 | 32 | 32 |
| Effizienz | 100 % | 50 % | 25 % | 12,5 % | 12,5 % |

**Sättigung ab $s = 8$**, weil 32 Threads höchstens 32 Sektoren anfordern können.

| Sonderfall | Sektoren | Effizienz |
|---|---|---|
| um 1 Element fehlausgerichtet | 5 | 80 % |
| Permutation im selben Segment | 4 | 100 % |
| alle lesen dieselbe Adresse | 1 | Broadcast |
| AoS `struct{float x,y,z;}`, `.x` gelesen | 12 | 33 % |

### Bankkonflikte ⚑ ★

32 Bänke, Wort $w$ liegt in Bank $w \bmod 32$.

$$\boxed{\text{Konfliktgrad für } \texttt{sm[s*t]} = \gcd(s, 32)}$$

| $s$ | 1 | 2 | 3 | 4 | 8 | 16 | 32 |
|---|---|---|---|---|---|---|---|
| Grad | 1 | 2 | **1** | 4 | 8 | 16 | **32** |

**Jede ungerade Schrittweite ist konfliktfrei.** Dasselbe **Wort** von mehreren Threads →
Broadcast, kein Konflikt.

**2D-Tile** `tile[R][Cp]`, Wortindex $C_p \cdot r + c$:

| Deklaration | Bank bei Spaltenzugriff ($r$ variiert) | Grad |
|---|---|---|
| `tile[32][32]` | $(32r + c)\bmod 32 = c$ | **32** |
| `tile[32][33]` | $(33r + c)\bmod 32 = (r+c)\bmod 32$ | **1** |
| `tile[32][34]` | $(34r + c)\bmod 32 = (2r+c)\bmod 32$ | 2 |

**Padding muss ungerade sein.** Kosten von `+1`: $R$ zusätzliche Wörter.

### Arithmetische Intensität gekachelter Kernel ⚑

| Kernel | $I(T)$ | Grenzwert | Klasse wechselbar? |
|---|---|---|---|
| **GEMM**, Kachel $T\times T$ | $\dfrac{T}{4}$ | $\to \infty$ | **ja** |
| **5-Punkt-Stencil**, Kachel $T\times T$ + Halo | $\dfrac{T^2}{(T+2)^2 + T^2}$ | $\to \dfrac12$ | nein |

Herleitung GEMM: globaler Verkehr $2n^3/T$ Wörter für $2n^3$ FLOP ⇒ $I = T/4$ (bei `float`).

**Kriterium:** Kacheln kann die Klasse nur ändern, wenn die **Wiederverwendung pro geladenem
Element mit $T$ wächst** (GEMM: $T$-fach; Stencil: höchstens 5-fach, konstant).

### Speichergrößen

$$\text{Bytes} = \text{Anzahl Elemente} \times \texttt{sizeof(T)}$$

Shared Memory eines Tile-Paars: $2 \cdot T^2 \cdot \texttt{sizeof(float)}$
(bei $T=32$: 8 kB).

---

## 12. Pipelines und Streams

Dieselbe Formel taucht dreimal auf (Instruktionspipeline, CUDA-Streams, jede Fließbandarbeit).
$c$ Elemente durchlaufen $k$ Stufen mit Dauern $d_1, \ldots, d_k$:

$$\boxed{T(c) = \underbrace{\sum_{j=1}^{k} d_j}_{\text{Füllen + Leeren}} \ +\ \underbrace{(c-1)\cdot \max_j d_j}_{\text{Fließbetrieb}}}$$

$$S = \frac{c \cdot \sum_j d_j}{T(c)}, \qquad
\lim_{c\to\infty} \frac{T(c)}{c} = \max_j d_j$$

**Anwendung 1 — Instruktionspipeline** ($k$ Stufen à $t$, $c = n$ Instanzen):

$$T = kt + (n-1)t = (n+k-1)t, \qquad S \to k$$

**Anwendung 2 — CUDA-Streams** (Gesamtzeiten $T_{h2d}, T_{k}, T_{d2h}$, in $c$ Stücke geteilt,
also $d_j = T_j/c$):

$$T(c) = \frac{T_{h2d} + T_k + T_{d2h}}{c} + (c-1)\cdot\frac{\max(T_{h2d}, T_k, T_{d2h})}{c}$$

$$T(\infty) = \max(T_{h2d}, T_k, T_{d2h}), \qquad
S_{\max} = \frac{T_{h2d} + T_k + T_{d2h}}{\max(\cdots)}$$

Beispiel (80 / 60 / 40 ms): $T(c) = \frac{100}{c} + 80$, $S_{\max} = 180/80 = 2{,}25$.

---

## 13. Pointer Jumping / PRAM

| Größe | Wert | Kap. |
|---|---|---|
| List Ranking **seriell** | $\Theta(n)$ | 13 |
| Pointer Jumping — **Zeit** | $\Theta(\log n)$ | 13 |
| Pointer Jumping — **Arbeit** | $\Theta(n \log n)$ | 13 |
| Prozessoren | $n$ | 13 |

**Iteration** (alle $i$ parallel, Distanz verdoppelt sich je Runde):

```
rank[i] = rank[i] + rank[next[i]];
next[i] = next[next[i]];
```

> **Arbeitseffizienz.** $A$ heißt *work-efficient* bezüglich $B$, wenn die Arbeit von $A$ bis
> auf einen konstanten Faktor der von $B$ entspricht. Pointer Jumping ist **zeiteffizient,
> aber nicht arbeitseffizient** — es lohnt sich erst ab $p > \log n$.

---

## 14. Strassen und Rekurrenzen

| Algorithmus | Rekursive Aufrufe | Additionen | Rekurrenz | Komplexität |
|---|---|---|---|---|
| naiv (Schleifen) | — | — | — | $\Theta(n^3)$ |
| blockrekursiv | 8 | 4 | $T(n) = 8T(n/2) + \Theta(n^2)$ | $\Theta(n^{\log_2 8}) = \Theta(n^3)$ |
| **Strassen** | **7** | **18** | $T(n) = 7T(n/2) + \Theta(n^2)$ | $\boldsymbol{\Theta(n^{\log_2 7}) \approx \Theta(n^{2{,}807})}$ |

**Master-Theorem** für $T(n) = a\,T(n/b) + \Theta(n^d)$:

$$T(n) = \begin{cases}
\Theta(n^{\log_b a}) & \text{falls } d < \log_b a \quad \text{(Rekursion dominiert)}\\
\Theta(n^d \log n) & \text{falls } d = \log_b a\\
\Theta(n^d) & \text{falls } d > \log_b a \quad \text{(Aufteilung dominiert)}
\end{cases}$$

Hier: $a = 7$, $b = 2$, $d = 2$; $\log_2 7 \approx 2{,}807 > 2$ ⇒ erster Fall.

**Die sieben Produkte:**

$$\begin{aligned}
M_1 &= (A_{11}+A_{22})(B_{11}+B_{22}) & M_5 &= (A_{11}+A_{12})B_{22}\\
M_2 &= (A_{21}+A_{22})B_{11} & M_6 &= (A_{21}-A_{11})(B_{11}+B_{12})\\
M_3 &= A_{11}(B_{12}-B_{22}) & M_7 &= (A_{12}-A_{22})(B_{21}+B_{22})\\
M_4 &= A_{22}(B_{21}-B_{11})
\end{aligned}$$

$$C_{11} = M_1+M_4-M_5+M_7, \quad C_{12} = M_3+M_5, \quad
C_{21} = M_2+M_4, \quad C_{22} = M_1-M_2+M_3+M_6$$

(10 Additionen für die $M_i$ + 8 für die $C_{ij}$ = 18.)

---

## Referenzwerte und Einheiten

### Hardware-Konstanten

| Größe | Wert |
|---|---|
| Cache-Line (CPU) | **64 Byte** = 8 `double` = 16 `float` |
| Warp (GPU) | **32 Threads** |
| Shared-Memory-Bänke | **32**, je 4 Byte |
| Sektorgröße GPU-Speicher | 32 Byte |
| max. Threads pro Block | **1024** |
| max. `gridDim.x` | $2^{31}-1$; `.y`, `.z`: 65 535 |
| Speicherlatenz GPU (global) | 400–800 Zyklen |
| Shared-Memory-Latenz | ~20–30 Zyklen |

### Compute Capability

| GPU | Architektur | Flag |
|---|---|---|
| Tesla P100 | Pascal | `sm_60` |
| Tesla V100 | Volta | `sm_70` |
| Tesla A100 | Ampere | `sm_80` |

### A100 (die Zahlen aus Blatt 11 und VL 12)

| Größe | Wert |
|---|---|
| $\pi$ (FP32) | 19,5 TFLOP/s |
| $\pi$ (FP64) | 9,7 TFLOP/s |
| $\beta$ (HBM2) | 1,5 TB/s (Datenblatt 1555 GB/s) |
| **Knickpunkt $I^\star$** | $19500/1500 = \mathbf{13}$ FLOP/Byte = **52 FLOP pro `float`** |
| $I^\star$ (FP64) | $9700/1500 = 6{,}47$ FLOP/Byte |
| SMs | 108 |
| FP32-Einheiten | 6912 |
| Register pro SM | 65 536 (32 Bit) |
| Shared Memory pro SM | 164 kB nutzbar |
| max. Warps pro SM | **64** (= 2048 Threads) |
| max. Blöcke pro SM | 32 |
| Boost-Takt | ~1,41 GHz |

### Speichertechnologien (VL 4)

| Technologie | Preis/GB | Zugriffszeit | Bandbreite |
|---|---|---|---|
| SRAM | $5000 | 0,5 ns | 25+ GB/s |
| DRAM | $7 | 50–150 ns | 10 GB/s |
| SSD | $0,05 | 25–100 µs | 0,5 GB/s |

### PCIe (Größenordnung für Aufgaben)

| | Bandbreite |
|---|---|
| PCIe 3.0 ×16 | ~12 GB/s praktisch |
| Verhältnis GPU-Speicher : PCIe | ~125 : 1 |

### Umrechnungen

$$1\ \text{GFLOP/s} = 10^9\ \text{FLOP/s} \qquad 1\ \text{GB} = 10^9\ \text{Byte (hier)}$$
$$1\ \text{ms} = 10^{-3}\ \text{s} \qquad 1\ \mu\text{s} = 10^{-6}\ \text{s} \qquad 1\ \text{ns} = 10^{-9}\ \text{s}$$
$$\text{Zeit [s]} = \frac{\text{Zyklen}}{\text{Takt [Hz]}} \qquad
\log_2 7 = 2{,}807 \qquad \log_2 10 \approx 3{,}32$$

---

## Welche Formel für welche Frage?

| Aufgabenstellung | Formel | Abschnitt |
|---|---|---|
| „Wie viel schneller wird es mit $p$ Prozessoren?" | $S = 1/(\alpha + (1-\alpha)/p)$ | [6](#6-amdahl-gustafson-roofline) |
| „Wie groß darf das Problem werden?" | $S = p + \gamma(1-p)$ | [6](#6-amdahl-gustafson-roofline) |
| „Ist mein Kernel speicher- oder rechengebunden?" | $I$ vs. $I^\star = \pi/\beta$ | [6](#6-amdahl-gustafson-roofline) |
| „Wie schnell kann er höchstens sein?" | $P \le \min(\pi, \beta I)$ | [6](#6-amdahl-gustafson-roofline) |
| „Wie viele Tag-/Index-/Offset-Bits?" | Adressaufteilung | [4](#4-cache) |
| „Wie viele Cache-Misses?" | $Q$ nach Zugriffsmuster; GEMM $\Theta(n^3)$ vs. $\Theta(n^3/(B\sqrt M))$ | [4](#4-cache), [5](#5-gemm-im-idealen-cache-modell) |
| „Wie groß soll der Block sein?" | $3s^2 \le M \Rightarrow s = \Theta(\sqrt M)$ | [5](#5-gemm-im-idealen-cache-modell) |
| „Wie viel bringt Pipelining?" | $T = (n+k-1)t$, $S \to k$ | [2](#2-rechnerarchitektur) |
| „Wie hoch ist die Peak-Performance?" | Takt × FLOP/Instr × Vektorlänge × Throughput | [2](#2-rechnerarchitektur) |
| „Warum steigt der Takt nicht mehr?" | $P = CU^2f$, $P_2 = k\beta^2P_1$ | [3](#3-miniaturisierung) |
| „Welcher Thread bearbeitet Element $i$?" | $i = \text{bIdx}\cdot\text{bDim} + \text{tIdx}$, Umkehrung per Division mit Rest | [10](#10-cuda--index-grid-occupancy) |
| „Wie viele Blöcke starte ich?" | $\lceil n/t\rceil = (n+t-1)/t$ | [10](#10-cuda--index-grid-occupancy) |
| „Wie hoch ist die Occupancy?" | Minimum über 4 Ressourcengrenzen | [10](#10-cuda--index-grid-occupancy) |
| „Wie viele Speichertransaktionen?" | $\#\text{Sektoren} = \min(32, 4s)$ | [11](#11-cuda--speicher-coalescing-bankkonflikte) |
| „Wie stark ist der Bankkonflikt?" | $\gcd(s, 32)$ | [11](#11-cuda--speicher-coalescing-bankkonflikte) |
| „Wie lang ist der kritische Pfad?" | ES/EF vorwärts (max), LF/LS rückwärts (min), Slack = 0 | [7](#7-parallele-algorithmen) |
| „Darf ich das parallelisieren?" | Bernstein: $I_1\cap O_2 = I_2\cap O_1 = O_1\cap O_2 = \emptyset$ | [7](#7-parallele-algorithmen) |
| „Wie viel bringt Überlappen?" | $T(c) = \sum d_j + (c-1)\max d_j$ | [12](#12-pipelines-und-streams) |
| „Wann darf ich Partikel zusammenfassen?" | $l/d \le \theta$ | [8](#8-n-body-und-barnes-hut) |
| „Trifft der Strahl das Dreieck?" | 3×3-System + $\lambda_2,\lambda_3\ge0$, $\lambda_2+\lambda_3\le1$, $\alpha\ge0$ | [9](#9-ray-tracing) |
| „Was ergibt diese Rekurrenz?" | Master-Theorem | [14](#14-strassen-und-rekurrenzen) |

---

**Zurück:** [Tutorial-Übersicht](README.md)
