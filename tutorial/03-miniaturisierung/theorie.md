# Kapitel 03 — Miniaturisierung, Dennard Scaling und die Power Wall

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/3-Lecture-Miniaturisierung.pdf` (19 Folien)
> **Zeitbedarf:** ca. 15 min

---

## 1. Worum es geht

Zwei Fragen, und die zweite Antwort ist der Grund, warum es diese Vorlesung gibt:

1. **Warum ist Miniaturisierung überhaupt möglich, und was bringt sie?** → Kleinere Rechner
   sind schneller *und* energieeffizienter.
2. **Warum hört das auf?** → Weil die Formeln ab ca. 2005 nicht mehr gelten. Seitdem lässt
   sich Leistung nur noch **unter Aufgabe der seriellen Abstraktion** steigern — also durch
   explizite Parallelität.

---

## 2. Die Kernpunkte

### 2.1 Warum Skalierung erlaubt ist: Information ist keine physikalische Größe

Ein Bit ist mathematisch definiert und kann physikalisch beliebig repräsentiert werden. Das
Bild der Vorlesung: ein **Wassertank**, voll ≙ 1, leer ≙ 0.

Mit Volumen $V$ und Durchflussrate $Q$ dauert ein Bitwechsel $T = V/Q$. Verkleinert man den
Tank auf $v = \alpha V$ mit $0 \le \alpha \le 1$ bei gleichem $Q$:

$$t = \frac{\alpha V}{Q} = \alpha \cdot \frac{V}{Q} = \alpha\, T$$

**Kleinere physikalische Volumina verkürzen den Bitwechsel um denselben Faktor.** Und weil
Information selbst keine physikalische Größe hat, gibt es keinen inhaltlichen Grund, *nicht*
zu skalieren.

### 2.2 Warum kleiner = schneller: die Kapazität hängt an der Länge

Information wird über Spannung übertragen; es gilt $Q = C \cdot U$ mit der Kapazität $C$ als
Materialeigenschaft. Für einen zylindrischen Leiter (Koaxialkabel-Modell) mit
Dielektrizitätskonstante $\varepsilon$ und Länge $L$:

$$C = \frac{2\pi\varepsilon L}{\ln(R/r)} \ \sim\ L$$

**Die Kapazität ist proportional zur Länge.**

Die Taktzykluszeit muss größer sein als die längste nötige Signallaufzeit im Rechner. Modelliert
man den Rechner als Würfel mit Kantenlänge $L$ und Volumen $V = L^3$, ist die längste Strecke
die Raumdiagonale $\sqrt{3}\,L = \sqrt{3}\,V^{1/3}$. Mit der Ausbreitungsgeschwindigkeit $g$:

$$t_{\min} \ge \frac{\sqrt{3}\,V^{1/3}}{g} = \frac{\sqrt{3}\,L}{g} \ \sim\ L,
\qquad f_{\max} \le \frac{g}{\sqrt{3}\,V^{1/3}} \ \sim\ \frac{1}{L}$$

**Taktzeit ist proportional zur Länge, Taktfrequenz umgekehrt proportional.**

Der Realitätscheck aus der Vorlesung:

| | Jahr | Volumen | Taktzeit |
|---|---|---|---|
| ENIAC | 1945 | 65 m³ | 200 · 10⁻⁶ s |
| iPhone | 2018 | 12,5 · 10⁻⁶ m³ | 0,4 · 10⁻⁹ s |
| **Verhältnis** | | **5 · 10⁶** | **5 · 10⁵** |

**Fazit:** Kleinere Rechner sind schneller und erlauben auf gleicher Fläche mehr Transistoren.

### 2.3 Dennard Scaling — die Rechnung, die man können muss

Elektrische Arbeit $W = Q \cdot U$, Ladung $Q = C \cdot U$, Leistung $P = W/T = W \cdot f$:

$$\boxed{P = C \cdot U^2 \cdot f}$$

**Ziel:** Die Leistung über Rechnergenerationen **konstant** halten. Beim Übergang von
Generation 1 auf 2 mit Skalenfaktor $\alpha < 1$:

| Größe | Skalierung | Grund |
|---|---|---|
| Länge | $L_2 = \alpha L_1$ | Miniaturisierung |
| Kapazität | $C_2 = \alpha C_1$ | $C \sim L$ |
| Frequenz | $f_2 = f_1/\alpha$ | $f \sim 1/L$ |
| Spannung | $U_2 = \beta U_1$ | zusätzlicher Freiheitsgrad, $\beta < 1$ |
| Bauelemente | $k \cdot B_1$ | gleiche Chipfläche, kleinere Elemente |

Damit:

$$P_2 = k \cdot C_2 U_2^2 f_2 = k \cdot (\alpha C_1)(\beta U_1)^2 \frac{f_1}{\alpha}
= k\,\beta^2 \cdot C_1 U_1^2 f_1 = k\,\beta^2 \cdot P_1$$

**Das $\alpha$ kürzt sich vollständig heraus.** Für konstante Leistung braucht es also
$k\beta^2 = 1$. Bei einer **Verdopplung** der Transistorzahl ($k = 2$):

$$\beta^2 = \frac{1}{2} \Rightarrow \beta = \frac{1}{\sqrt{2}} \approx \mathbf{0{,}7}$$

**Dennard Scaling:** Senkt man mit jeder Generation die Spannung auf etwa 70 %, verdoppelt
sich die Transistorzahl **und** steigt die Taktfrequenz — bei **gleichbleibender**
Leistungsaufnahme. Kleine Rechner sind energieeffizient.

### 2.4 Das Ende: leaky transistors und die Power Wall

Das Dennard Scaling **endete um 2005**. Der Grund: Zu kleine Transistoren verbrauchen
**dauerhaft** Leistung, auch wenn sie nicht schalten (*Leckströme*). Die Formel
$P = C U^2 f$ erfasst nur die Schaltleistung; der statische Anteil fehlt darin. Zugleich lässt
sich $U$ nicht beliebig senken, ohne dass der Transistor unzuverlässig wird.

**Konsequenz:** Die Taktfrequenz stagniert bei etwa **4 GHz**. Weitere Miniaturisierung liefert
zwar mehr Transistoren, aber keinen höheren Takt mehr.

> **Der Wendepunkt des ganzen Kurses:** Seit ca. 2005 kann Leistung nur noch **unter Aufgabe
> der seriellen Abstraktion** gesteigert werden. Die zusätzlichen Transistoren gehen in mehr
> Kerne, breitere SIMD-Einheiten und GPUs — und die muss der Programmierer **explizit**
> benutzen. Genau deshalb gibt es OpenMP und CUDA.

### 2.5 Der von-Neumann-Flaschenhals

Die von-Neumann-Architektur trennt **CPU** und **Memory**; die ISA ist die passende
Abstraktion dafür, weil Speicherzugriffe nur über Load/Store laufen.

Vorteil: Beide Seiten können sich technologisch **unabhängig** entwickeln und unabhängig
miniaturisiert werden.

Nachteil: Genau diese Trennung begrenzt die Rate der Speicherzugriffe — der
**von-Neumann-Flaschenhals**. Die CPU wurde über Jahrzehnte deutlich schneller als der
Speicher; die Lücke ist der Ausgangspunkt von Kapitel 04.

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Warum darf man Bit-Darstellungen beliebig verkleinern? | Information ist keine physikalische Größe; die Repräsentation ist frei wählbar |
| Wie hängt die Kapazität von der Geometrie ab? | $C \sim L$ — proportional zur Länge |
| Wie hängt die maximale Taktfrequenz von der Größe ab? | $f_{\max} \le g/(\sqrt{3}V^{1/3}) \sim 1/L$ |
| Formel für die elektrische Leistung | $P = C \cdot U^2 \cdot f$ |
| Was besagt Dennard Scaling? | bei $L \to \alpha L$, $C \to \alpha C$, $f \to f/\alpha$, $U \to \beta U$ gilt $P_2 = k\beta^2 P_1$; konstante Leistung für $k\beta^2 = 1$ |
| Welches $\beta$ bei Verdopplung der Transistorzahl? | $\beta = 1/\sqrt{2} \approx 0{,}7$ |
| Warum endete Dennard Scaling um 2005? | Leckströme — zu kleine Transistoren verbrauchen auch im Ruhezustand Leistung; $U$ nicht beliebig senkbar |
| Welche Folge hatte das? | Taktfrequenz stagniert bei ~4 GHz; Leistungssteigerung nur noch durch **explizite Parallelität** |
| Was ist der von-Neumann-Flaschenhals? | die Trennung CPU/Speicher begrenzt die Rate der Speicherzugriffe |

---

## 4. Merkkasten

> - $C \sim L$, $f \sim 1/L$ — **kleiner ist schneller**.
> - $P = C U^2 f$; beim Skalieren kürzt sich $\alpha$ heraus, es bleibt $P_2 = k\beta^2 P_1$.
> - **Dennard:** $k = 2 \Rightarrow \beta \approx 0{,}7$ — doppelt so viele Transistoren bei
>   gleicher Leistungsaufnahme.
> - **Ende ~2005 durch Leckströme** → Power Wall, Takt bei ~4 GHz.
> - Seitdem: Mehr Transistoren ja, mehr Takt nein → **explizite Parallelität** ist der einzige
>   verbleibende Weg.
> - Der **von-Neumann-Flaschenhals** bleibt und motiviert die Cache-Hierarchie.

---

## 5. Verbindung

**Dies ist die Begründung für den Rest der Vorlesung.** Kapitel 04 nimmt den
von-Neumann-Flaschenhals auf (Caches), Kapitel 02 und 05b die Frage, was innerhalb der
seriellen Abstraktion noch geht (ILP, SIMD), und die Kapitel 06–12 das, was danach kommt:
OpenMP, Tasks, GPUs. Kapitel 10 (Amdahl) liefert die Antwort darauf, wie weit man mit
Parallelität überhaupt kommt.
