# Kapitel 10 — Speedup, Amdahl, Gustafson und die Roofline

> **Kompaktkapitel** — aber das mit Abstand klausurrelevanteste. Zusammenfassung statt
> Lehrbuch, keine eigenen Übungsdateien.
> **Quellen:** `vl/10-Lecture-Ahmdahl-Gustafon.pdf` (13 Folien), Übungsblätter
> `excercises/uebung10.pdf` (Roofline), `excercises/uebung8.pdf` (Aufgaben 8.2/8.3),
> eigene Abgabe `project/REPORT.md` (Amdahl-Fit)
> **Zeitbedarf:** ca. 25 min
> **Voraussetzungen:** keine

> **📖 [Vertiefung](vertiefung.md)** — vollständige Herleitungen, Effizienzkurven,
> Amdahl **mit** Overhead, die Karp-Flatt-Metrik, die Umrechnung $\alpha \leftrightarrow \gamma$
> (und warum beide Gesetze bei festem Problem **dasselbe** liefern), Roofline-Diagramm mit
> Ceilings sowie sechs Aufgabentypen mit Musterlösung.

---

## 1. Worum es geht

Dieses Kapitel liefert die **Sprache**, in der über parallele Performance geredet wird —
und zwei Antworten auf dieselbe Frage „wie viel bringt Parallelisierung?", die sich
scheinbar widersprechen. Sie tun es nicht: Sie beantworten verschiedene Fragen. Dazu kommt
die Roofline als Antwort auf „wodurch ist mein Programm eigentlich begrenzt?".

---

## 2. Die Definitionen

Ein Problem der Größe $n$ wird mit $p$ Prozessoren gelöst.

| Symbol | Bedeutung |
|---|---|
| $T^*(n)$ | Laufzeit des **schnellsten bekannten seriellen** Algorithmus |
| $T(n;p)$ | Laufzeit des **parallelen** Algorithmus auf $p$ Prozessoren |
| $S(n;p)$ | **Speedup** |
| $E(n;p)$ | **Effizienz** |

$$S(n;p) = \frac{T^*(n)}{T(n;p)}, \qquad E(n;p) = \frac{S(n;p)}{p}$$

> **Achtung — die Falle, auf die die Vorlesung ausdrücklich hinweist.** Oft wird stattdessen
> $$S(n;p) = \frac{T(n;1)}{T(n;p)}$$
> benutzt, also der **parallele Algorithmus auf einem Prozessor** als Bezug. Das ist bequem
> (man braucht nur ein Programm), aber es **schmeichelt**: Der parallele Algorithmus trägt auf
> einem Prozessor seinen ganzen Verwaltungsaufwand mit, ist also langsamer als $T^*(n)$, und
> der Speedup fällt entsprechend zu groß aus. Wer $T(n;1)$ verwendet, muss das dazusagen.

**Zerlegung der seriellen Laufzeit:**

$$T^*(n) = T_{\text{ser}}(n) + T_{\text{par}}(n)$$

- $T_{\text{ser}}$ — der **inhärent serielle** Anteil (Ein-/Ausgabe, Steuerlogik, die
  Zeitschleife bei n-Body)
- $T_{\text{par}}$ — der parallelisierbare Anteil

**Zerlegung der parallelen Laufzeit:**

$$T(n;p) \ \ge\ T_{\text{ser}}(n) + \frac{T_{\text{par}}(n)}{p} + T_{\text{over}}(n;p)$$

mit dem **Overhead** $T_{\text{over}}$ (Thread-Erzeugung, Barrieren, Kommunikation,
Lastungleichgewicht). Daraus die allgemeine Schranke:

$$S(n;p) \ \le\ \frac{T_{\text{ser}} + T_{\text{par}}}{T_{\text{ser}} + \dfrac{T_{\text{par}}}{p} + T_{\text{over}}}$$

Amdahl und Gustafson entstehen daraus durch **verschiedene Vereinfachungen**.

---

## 3. Amdahl — Strong Scaling

**Annahmen:** feste Problemgröße $n$, $T_{\text{over}} = 0$, $T_{\text{par}}$ perfekt
parallelisierbar.

$$T(n;p) = T_{\text{ser}}(n) + \frac{T_{\text{par}}(n)}{p}$$

Definiere $\alpha$ über den **seriellen** Bezug: $T_{\text{ser}}(n) = \alpha \cdot T(n;1)$,
also $T_{\text{par}}(n) = (1-\alpha)\,T(n;1)$:

$$\boxed{S(n;p) = \frac{1}{\alpha + \dfrac{1-\alpha}{p}}}
\qquad\qquad \lim_{p \to \infty} S(n;p) = \boxed{\frac{1}{\alpha}}$$

**Die Aussage:** Der serielle Anteil deckelt alles. Bei $\alpha = 0{,}05$ ist bei Speedup 20
Schluss — mit **beliebig vielen** Prozessoren.

| $\alpha$ | $S_{\max} = 1/\alpha$ |
|---|---|
| 0,50 | 2 |
| 0,10 | 10 |
| 0,05 | 20 |
| 0,01 | 100 |
| 0,001 | 1000 |

**Strong Scaling:** Die Problemgröße bleibt bei wachsendem $p$ **fest**. Ziel ist, dasselbe
Problem **schneller** zu lösen. Perfekte Skalierung heißt $p$-mal schneller.

---

## 4. Gustafson — Weak Scaling

**Annahmen:** feste **parallele** Laufzeit $T(n;p)$, $T_{\text{over}} = 0$, $T_{\text{par}}$
perfekt parallelisierbar.

Definiere $\gamma$ über den **parallelen** Bezug: $T_{\text{ser}}(n) = \gamma \cdot T(n;p)$,
also $T_{\text{par}}(n)/p = (1-\gamma)\,T(n;p)$:

$$\boxed{S(n;p) = p + \gamma\,(1 - p)}$$

Probe: $\gamma = 0 \Rightarrow S = p$ (perfekt); $\gamma = 1 \Rightarrow S = 1$ (alles
seriell). ✓

**Weak Scaling:** Die Problemgröße **pro Prozessor** bleibt fest, $n$ wächst also mit $p$.
Ziel ist, ein **größeres** Problem in gleicher Zeit zu lösen. Perfekte Skalierung heißt: ein
$p$-mal größeres Problem wird gelöst.

### Der Vergleich, den man verstanden haben muss

$\alpha = \gamma = 0{,}1$ und $p = 16$:

| | Rechnung | $S$ | $E = S/p$ |
|---|---|---|---|
| **Amdahl** | $1/(0{,}1 + 0{,}9/16)$ | **6,4** | 0,40 |
| **Gustafson** | $16 + 0{,}1(1-16)$ | **14,5** | 0,91 |

Derselbe Zahlenwert, völlig verschiedene Ergebnisse — weil er **verschiedene Dinge** misst:

|  | Amdahl | Gustafson |
|---|---|---|
| Was ist fest? | die **Problemgröße** $n$ | die **Laufzeit** $T(n;p)$ |
| $\alpha$ bzw. $\gamma$ bezogen auf | $T(n;1)$ — die **serielle** Laufzeit | $T(n;p)$ — die **parallele** Laufzeit |
| Ziel | dasselbe Problem schneller | ein größeres Problem gleich schnell |
| Name | **Strong Scaling** | **Weak Scaling** |
| Grenzwert $p\to\infty$ | $1/\alpha$ — **beschränkt** | $\to \infty$ — **linear in $p$** |
| Stimmung | pessimistisch | optimistisch |

**Der eigentliche Punkt:** In der Praxis wächst mit der Rechenleistung meist auch der
Anspruch — man simuliert feiner, nicht dasselbe schneller. Und typischerweise wächst
$T_{\text{par}}$ schneller in $n$ als $T_{\text{ser}}$ (etwa $O(n^3)$ gegen $O(n)$), sodass
$\alpha$ mit wachsendem $n$ **kleiner** wird. Deshalb ist Gustafson für den HPC-Alltag oft
das ehrlichere Modell — Amdahl bleibt aber die harte Schranke für feste Problemgröße.

### Bemerkungen für die Klausur

- **Superlinearer Speedup** ($S > p$) ist möglich, obwohl beide Gesetze ihn ausschließen —
  weil sie den Cache ignorieren: Bei $p$ Prozessoren passt der Teildatensatz jedes
  Prozessors womöglich in den L2-Cache, bei einem nicht (Kapitel 04).
- $T_{\text{over}}$ wächst meist **mit** $p$ (mehr Barrieren, mehr Kommunikation). Deshalb hat
  eine reale Speedup-Kurve ein **Maximum** und fällt danach wieder — beide Gesetze sagen das
  nicht voraus.
- Aus einer Messreihe $\alpha$ zu bestimmen (Fit an die Amdahl-Kurve) ist eine typische
  Aufgabe; die Vorlage steht in `project/REPORT.md`.

---

## 5. Die Roofline

Amdahl und Gustafson beantworten „wie viel bringt mehr Parallelität?". Die Roofline
beantwortet die andere Frage: **„Wodurch ist mein Kernel begrenzt — Rechenwerk oder
Speicher?"**

**Arithmetische (operationale) Intensität:**

$$I = \frac{\text{FLOPs}}{\text{aus dem DRAM bewegte Bytes}} \qquad [\text{FLOP/Byte}]$$

**Erreichbare Leistung**, mit Spitzenrechenleistung $\pi$ [GFLOP/s] und Spitzenbandbreite
$\beta$ [GB/s]:

$$\boxed{P \le \min\bigl(\pi,\ \beta \cdot I\bigr)}$$

Im doppelt-logarithmischen Diagramm ($I$ auf der x-Achse, $P$ auf der y-Achse) ist das eine
Gerade der Steigung 1, die in eine Waagerechte übergeht. Der Übergang ist der **Knickpunkt**:

$$I^\star = \frac{\pi}{\beta}$$

| Bereich | Diagnose | Was hilft |
|---|---|---|
| $I < I^\star$ (links) | **speichergebunden** | weniger Bytes bewegen: Lokalität, Blocking, Kernel-Fusion, kleinere Datentypen |
| $I > I^\star$ (rechts) | **rechengebunden** | bessere Operationen: FMA, SIMD, Tensor-Cores, Bibliotheken |

### Durchgerechnet (Zahlen von Blatt 10)

$\pi = 3$ GFLOP/s, $\beta = 8$ GB/s:

$$I^\star = \frac{3}{8} = 0{,}375\ \text{FLOP/Byte}$$

**Beispiel 1:**

```c
float x[N], y[N];
for (int i = 0; i < N; i++)
    x[i] = x[i] + y[i];
```

- FLOP pro Iteration: **1** (eine Addition)
- Bytes: `x` lesen, `y` lesen, `x` schreiben = **12**

$$I = \frac{1}{12} = 0{,}083 < 0{,}375 \Rightarrow \textbf{speichergebunden}$$
$$P \le 8 \cdot 0{,}083 = 0{,}67\ \text{GFLOP/s} \quad (22\ \% \text{ von } \pi)$$

**Beispiel 2:**

```c
float t = 0.0, x[N];
for (int i = 0; i < N; i++)
    t = t + x[i] * x[i];
```

- FLOP pro Iteration: **2** (Multiplikation + Addition)
- Bytes: nur `x` lesen = **4** (`t` liegt in einem Register)

$$I = \frac{2}{4} = 0{,}5 > 0{,}375 \Rightarrow \textbf{rechengebunden}$$
$$P \le \min(3,\ 8 \cdot 0{,}5) = \min(3, 4) = 3\ \text{GFLOP/s} \quad (100\ \% \text{ von } \pi)$$

**Die beiden Schleifen sehen fast gleich aus und liegen auf verschiedenen Seiten des Knicks.**
Der Unterschied ist, dass die zweite ihr Ergebnis in einem Register akkumuliert, statt es in
den Speicher zurückzuschreiben.

### Wie man $I$ richtig zählt

| Regel | Begründung |
|---|---|
| Zähle **DRAM**-Bytes, nicht alle Zugriffe | was im Cache liegt, kostet keine Bandbreite — deshalb erhöht Blocking $I$ |
| Ein Schreibzugriff zählt genauso wie ein Lesezugriff | er belegt dieselbe Bandbreite |
| Skalare in Registern zählen nicht | sie werden nicht bewegt |
| Datentyp beachten | `double` verdoppelt die Bytes und **halbiert $I$** |
| Bei FMA zählt eine Instruktion als **2 FLOP** | Multiplikation und Addition |

**Die Roofline ist eine obere Schranke, kein Versprechen.** Ein Kernel kann sie aus vielen
Gründen verfehlen (schlechte Lokalität, Divergenz, zu wenig Parallelität) — aber er kann sie
nie überschreiten. Wer 80 % erreicht, ist fertig.

---

## 6. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Definiere Speedup und Effizienz | $S = T^*(n)/T(n;p)$, $E = S/p$ |
| Warum ist $S = T(n;1)/T(n;p)$ problematisch? | $T(n;1)$ enthält den parallelen Overhead und ist größer als $T^*(n)$ → Speedup fällt zu günstig aus |
| Zerlegung der parallelen Laufzeit | $T(n;p) \ge T_{\text{ser}} + T_{\text{par}}/p + T_{\text{over}}$ |
| Amdahl-Formel und Annahmen | $S = 1/(\alpha + (1-\alpha)/p)$; festes $n$, kein Overhead, perfekt parallelisierbar |
| Amdahl-Grenzwert | $1/\alpha$ |
| Gustafson-Formel und Annahmen | $S = p + \gamma(1-p)$; feste **parallele** Laufzeit, kein Overhead |
| Worauf beziehen sich $\alpha$ und $\gamma$? | $\alpha$ auf $T(n;1)$, $\gamma$ auf $T(n;p)$ |
| Strong vs. Weak Scaling | $n$ fest, schneller / $n/p$ fest, größeres Problem in gleicher Zeit |
| Wie ist superlinearer Speedup möglich? | Cache-Effekte — der Teildatensatz passt bei $p$ Prozessoren in den Cache, bei einem nicht |
| Definition der arithmetischen Intensität | FLOP pro aus dem DRAM bewegtem Byte |
| Roofline-Schranke | $P \le \min(\pi, \beta I)$ |
| Knickpunkt | $I^\star = \pi/\beta$ |
| $I$ von `x[i] = x[i] + y[i]` (float) | $1/12 = 0{,}083$ |
| $I$ von `t += x[i]*x[i]` (float) | $2/4 = 0{,}5$ |
| Was tun bei $I < I^\star$? | Bytes sparen: Lokalität, Blocking, Fusion, kleinere Datentypen |

---

## 7. Merkkasten

> - $S = T^*/T(n;p)$, $E = S/p$. **Bezug angeben** — $T^*$ oder $T(n;1)$ macht einen
>   Unterschied.
> - **Amdahl (strong):** $S = \dfrac{1}{\alpha + (1-\alpha)/p} \to \dfrac{1}{\alpha}$.
>   Festes $n$, harte Schranke.
> - **Gustafson (weak):** $S = p + \gamma(1-p)$. Feste Zeit, wachsendes $n$, linear in $p$.
> - Beide widersprechen sich nicht — sie halten **verschiedene Größen** fest.
> - **Roofline:** $I = \text{FLOP}/\text{DRAM-Byte}$, $P \le \min(\pi, \beta I)$,
>   Knick bei $I^\star = \pi/\beta$.
> - Links vom Knick hilft **nur**, Bytes zu sparen. Rechenoptimierung bringt dort nichts.

---

## 8. Verbindung

**Wird gebraucht in:** Kapitel 11 (lohnt sich die GPU trotz PCIe-Transfer?) und
Kapitel 12 (Knickpunkt der A100: $I^\star = 13$ FLOP/Byte; Roofline-Analyse von Stencil und
GEMM). Kapitel 02 liefert mit dem Pipeline-Speedup $S = nk/(n+k-1)$ dieselbe Denkfigur auf
Instruktionsebene, Kapitel 05b mit der Peak-Performance-Formel das $\pi$.

**Querverbindung:** Der serielle Anteil $\alpha$ ist in konkreten Programmen greifbar — die
Zeitschleife bei Barnes-Hut (Kapitel 08), das Einlesen der STL-Datei beim Ray Tracer
(Kapitel 09), der PCIe-Transfer bei CUDA (Kapitel 11). In Kapitel 11, Aufgabe 11.7 wird
vorgerechnet, dass ein Transfer **wie ein zusätzlicher serieller Anteil** wirkt und den
erreichbaren Speedup unabhängig von der GPU-Geschwindigkeit deckelt.

**Eigene Abgabe:** `project/REPORT.md` enthält Speedup-/Effizienztabelle und Amdahl-Fit für
den Ray Tracer; `project/src/util/benchmark.py` ist die Vorlage.
