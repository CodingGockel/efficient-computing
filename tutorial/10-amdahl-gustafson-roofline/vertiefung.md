# Vertiefung — Amdahl, Gustafson und Roofline im Detail

> Ergänzung zu [`theorie.md`](theorie.md). Dort steht die kompakte Fassung zum Nachschlagen —
> hier die vollständigen Herleitungen, die Grenzfälle und die Frage, warum Amdahl und
> Gustafson sich **nicht** widersprechen.
> **Zeitbedarf:** ca. 45 min
> **Formeln kompakt:** [Formelsammlung, Abschnitt 6](../formelsammlung.md#6-amdahl-gustafson-roofline)

---

## Inhalt

1. [Der gemeinsame Ausgangspunkt](#1-der-gemeinsame-ausgangspunkt)
2. [Amdahl Schritt für Schritt](#2-amdahl-schritt-für-schritt)
3. [Gustafson Schritt für Schritt](#3-gustafson-schritt-für-schritt)
4. [Der aufgelöste Widerspruch](#4-der-aufgelöste-widerspruch) ⚑
5. [Roofline Schritt für Schritt](#5-roofline-schritt-für-schritt)
6. [Alles zusammen: der GPU-Fall](#6-alles-zusammen-der-gpu-fall)
7. [Aufgabentypen mit Musterlösung](#7-aufgabentypen-mit-musterlösung)

---

## 1. Der gemeinsame Ausgangspunkt

Beide Gesetze folgen aus **derselben** Zerlegung. Man muss sie nicht getrennt auswendig
lernen — man muss wissen, welche Größe man festhält.

Die serielle Laufzeit zerfällt in zwei Teile:

$$T^*(n) = T_{\text{ser}}(n) + T_{\text{par}}(n)$$

- $T_{\text{ser}}$ — **inhärent seriell**: lässt sich prinzipiell nicht aufteilen. Einlesen
  einer Datei, Ausgabe, Steuerlogik, die Zeitschleife einer Simulation.
- $T_{\text{par}}$ — **parallelisierbar**: lässt sich im Idealfall auf $p$ Prozessoren
  gleichmäßig verteilen.

Auf $p$ Prozessoren wird daraus:

$$T(n;p) \ \ge\ T_{\text{ser}}(n) + \frac{T_{\text{par}}(n)}{p} + T_{\text{over}}(n;p)$$

Das „$\ge$" steht da, weil noch mehr schiefgehen kann, als $T_{\text{over}}$ erfasst
(Lastungleichgewicht, Speicherbandbreite, …).

**Beide Gesetze setzen $T_{\text{over}} = 0$** und nehmen an, dass $T_{\text{par}}$ perfekt
teilbar ist:

$$T(n;p) = T_{\text{ser}}(n) + \frac{T_{\text{par}}(n)}{p}$$

Ab hier trennen sich die Wege — und zwar **nur** durch die Wahl des Bezugspunkts:

| | Amdahl | Gustafson |
|---|---|---|
| Bezugsgröße | $T(n;1)$ — die **serielle** Laufzeit | $T(n;p)$ — die **parallele** Laufzeit |
| Definierter Anteil | $\alpha = \dfrac{T_{\text{ser}}}{T(n;1)}$ | $\gamma = \dfrac{T_{\text{ser}}}{T(n;p)}$ |
| Festgehalten wird | die **Problemgröße** $n$ | die **Laufzeit** $T(n;p)$ |

> **Das ist die ganze Geschichte.** Alles Weitere ist Algebra. Wer sich merkt, dass $\alpha$
> auf die serielle und $\gamma$ auf die parallele Laufzeit bezogen ist, kann beide Formeln in
> zwei Zeilen herleiten und wird sie nie verwechseln.

---

## 2. Amdahl Schritt für Schritt

### 2.1 Die Herleitung

Normiere $T(n;1) = 1$ (das ist keine Einschränkung, nur eine Zeiteinheit). Dann:

$$T_{\text{ser}} = \alpha, \qquad T_{\text{par}} = 1 - \alpha$$

Eingesetzt:

$$T(n;p) = \alpha + \frac{1-\alpha}{p}$$

$$S(n;p) = \frac{T(n;1)}{T(n;p)} = \frac{1}{\alpha + \dfrac{1-\alpha}{p}}$$

**Ausführlicher, ohne Normierung** — so sieht es in der Klausur oft aus:

$$S = \frac{T_{\text{ser}} + T_{\text{par}}}{T_{\text{ser}} + \frac{T_{\text{par}}}{p}}
= \frac{\alpha T_1 + (1-\alpha)T_1}{\alpha T_1 + \frac{(1-\alpha)T_1}{p}}
= \frac{T_1\bigl[\alpha + 1 - \alpha\bigr]}{T_1\bigl[\alpha + \frac{1-\alpha}{p}\bigr]}
= \frac{1}{\alpha + \frac{1-\alpha}{p}}$$

### 2.2 Was $\alpha$ wirklich ist — und was nicht

$\alpha$ ist **kein** Maß für „schlechte Programmierung". Es ist der Anteil an der
**seriellen Laufzeit**, den man auch mit perfektem Code nicht verteilen kann.

| Beispiel | $T_{\text{ser}}$ |
|---|---|
| Ray Tracer (Kap. 09) | STL-Datei einlesen, PPM schreiben |
| n-Body (Kap. 08) | die **Zeitschleife** — Schritt $k+1$ braucht Schritt $k$ |
| GPU-Programm (Kap. 11) | Kernel-Start, PCIe-Transfer, Ergebnisprüfung |
| jedes Programm | `main()`-Start, Speicher allokieren, Ergebnis ausgeben |

**Zwei Dinge, die man leicht verwechselt:**

- $\alpha$ ist ein **Zeitanteil**, kein Codeanteil. Fünf Zeilen serieller Code können 30 %
  der Laufzeit sein.
- $\alpha$ hängt von $n$ ab. Typischerweise wächst $T_{\text{par}}$ schneller in $n$ als
  $T_{\text{ser}}$ — etwa $O(n^3)$ gegen $O(n)$. **Also sinkt $\alpha$ mit wachsendem $n$.**
  Genau darauf beruht Gustafson (Abschnitt 4).

### 2.3 Der Grenzwert und die Effizienz

$$\lim_{p\to\infty} S = \lim_{p\to\infty} \frac{1}{\alpha + \frac{1-\alpha}{p}} = \frac{1}{\alpha}$$

Für die Effizienz gilt

$$E(p) = \frac{S}{p} = \frac{1}{p\left(\alpha + \frac{1-\alpha}{p}\right)} = \frac{1}{\alpha p + 1 - \alpha}
\qquad\xrightarrow{p\to\infty}\qquad 0$$

**Die Effizienz geht gegen null.** Bei $\alpha = 0{,}05$:

| $p$ | $T(p)/T_1$ | $S$ | $E$ | verschenkte Prozessoren |
|---|---|---|---|---|
| 1 | 1,000 | 1,00 | 100 % | 0 |
| 2 | 0,525 | 1,90 | 95 % | 0,1 |
| 4 | 0,288 | 3,48 | 87 % | 0,5 |
| 8 | 0,169 | 5,93 | 74 % | 2,1 |
| 16 | 0,109 | 9,14 | 57 % | 6,9 |
| 32 | 0,080 | 12,55 | 39 % | 19,5 |
| 64 | 0,065 | 15,42 | 24 % | 48,6 |
| 128 | 0,057 | 17,42 | 14 % | 110,6 |
| 1024 | 0,051 | 19,64 | 2 % | 1004,4 |
| $\infty$ | 0,050 | **20,00** | 0 % | — |

Bei 1024 Prozessoren arbeiten effektiv **20**. Die restlichen 1004 warten auf die 5 % seriellen
Code.

### 2.4 Wie viele Prozessoren lohnen sich?

Eine praktisch sehr nützliche Frage. Setze $E(p) = \tfrac12$:

$$\frac{1}{\alpha p + 1 - \alpha} = \frac12 \iff \alpha p + 1 - \alpha = 2
\iff p = \frac{1 + \alpha}{\alpha} = \frac{1}{\alpha} + 1$$

$$\boxed{E(p) \ge \tfrac12 \iff p \le \tfrac1\alpha + 1}$$

Bei $\alpha = 0{,}05$ also $p \le 21$. Faustregel: **Ab etwa $1/\alpha$ Prozessoren
verschwendet man mehr als die Hälfte der Maschine.**

### 2.5 Amdahl mit Overhead — die realistische Kurve

Reine Amdahl-Kurven sind **monoton steigend**: mehr Prozessoren sind nie schlechter. Reale
Messkurven haben ein **Maximum** und fallen danach. Der Grund ist $T_{\text{over}}$, das mit
$p$ wächst (mehr Barrieren, mehr Kommunikation, mehr Cache-Kohärenzverkehr).

Modelliere linear wachsenden Overhead, $T_{\text{over}} = c\,p\,T_1$:

$$\frac{T(p)}{T_1} = \alpha + \frac{1-\alpha}{p} + c\,p$$

Das **Optimum** findet man durch Ableiten:

$$\frac{d}{dp}\left[\alpha + \frac{1-\alpha}{p} + cp\right] = -\frac{1-\alpha}{p^2} + c \overset{!}{=} 0
\qquad\Longrightarrow\qquad \boxed{p^* = \sqrt{\frac{1-\alpha}{c}}}$$

Mit $\alpha = 0{,}05$ und $c = 0{,}002$: $p^* = \sqrt{0{,}95/0{,}002} = 21{,}8$, also
$p^* \approx 22$.

| $p$ | ohne Overhead $S$ | **mit** Overhead $S$ |
|---|---|---|
| 2 | 1,90 | 1,89 |
| 4 | 3,48 | 3,38 |
| 8 | 5,93 | 5,41 |
| 16 | 9,14 | 7,07 |
| **22** | 10,6 | **7,29** ← Maximum |
| 32 | 12,55 | 6,96 |
| 64 | 15,42 | 5,19 |

**Ab 32 Prozessoren wird es wieder langsamer.** Genau das sieht man in echten
Skalierungsmessungen — und genau das sagt reines Amdahl nicht voraus.

### 2.6 Karp-Flatt: $\alpha$ aus Messwerten bestimmen ⚑

Man hat eine Messreihe $S(p)$ und will wissen: **Liegt es am seriellen Anteil oder am
Overhead?** Dafür löst man die Amdahl-Formel nach $\alpha$ auf. Das Ergebnis heißt
**experimentell bestimmter serieller Anteil** (Karp-Flatt-Metrik):

$$\boxed{e = \frac{\dfrac{1}{S(p)} - \dfrac{1}{p}}{1 - \dfrac{1}{p}}}$$

**Herleitung:** Aus $S = 1/(\alpha + (1-\alpha)/p)$ folgt $\tfrac1S = \alpha + \tfrac{1-\alpha}{p}
= \alpha\left(1 - \tfrac1p\right) + \tfrac1p$, also $\alpha = \dfrac{1/S - 1/p}{1 - 1/p}$.

**Und jetzt der Trick:** Man berechnet $e$ für **jedes** gemessene $p$ und schaut, ob es
konstant bleibt.

| $p$ | $S$ (nur Amdahl) | $e$ | $S$ (mit Overhead) | $e$ |
|---|---|---|---|---|
| 2 | 1,905 | **0,0500** | 1,890 | 0,0580 |
| 4 | 3,478 | **0,0500** | 3,384 | 0,0607 |
| 8 | 5,926 | **0,0500** | 5,413 | 0,0683 |
| 16 | 9,143 | **0,0500** | 7,073 | 0,0841 |
| 32 | 12,549 | **0,0500** | 6,960 | 0,1161 |
| 64 | 15,422 | **0,0500** | 5,186 | 0,1800 |

**Die Diagnose:**

| Beobachtung | Ursache | Was tun |
|---|---|---|
| $e$ bleibt **konstant** | reiner serieller Anteil — Amdahl gilt exakt | den seriellen Code angreifen; mehr Prozessoren bringen wenig |
| $e$ **wächst** mit $p$ | Overhead (Barrieren, Kommunikation, Lastungleichgewicht) | Synchronisation reduzieren, Granularität vergrößern |
| $e$ **fällt** mit $p$ | superlinearer Effekt, meist Cache | nichts — genießen, aber im Bericht erwähnen |

Das ist erheblich aussagekräftiger, als nur eine Amdahl-Kurve durch die Messpunkte zu fitten,
und mit zwei Zeilen Python erledigt.

### 2.7 Superlinearer Speedup

Amdahl sagt $S \le p$. Gemessen wird trotzdem manchmal $S > p$. **Kein Widerspruch, sondern
eine falsche Modellannahme:** Beide Gesetze nehmen an, dass ein Prozessor bei
$n$-fach kleinerem Teilproblem exakt $n$-fach schneller ist. Das stimmt nicht, wenn Caches
im Spiel sind.

Beispiel: Ein Datensatz von 30 MB passt nicht in einen 8-MB-L3-Cache. Auf 8 Prozessoren mit
je 3,75 MB Anteil passt er — jeder Kern arbeitet plötzlich aus dem Cache statt aus dem DRAM,
und das ist ein Faktor 10 pro Zugriff (Kapitel 04).

Weitere Quellen: Suchprobleme, bei denen ein Prozessor die Lösung früh findet und alle
abbricht; oder mehr Register/TLB-Einträge insgesamt.

### 2.8 Typische Fehler

| Fehler | Warum falsch |
|---|---|
| $\alpha$ als Anteil der **Codezeilen** lesen | es ist ein Anteil der **Laufzeit** |
| $\alpha$ als konstant über $n$ annehmen | $\alpha$ sinkt meist mit wachsendem $n$ |
| $T(n;1)$ statt $T^*(n)$ als Bezug, ohne es zu sagen | $T(n;1)$ enthält den parallelen Overhead → Speedup wirkt besser |
| aus $S(p) < p$ auf schlechten Code schließen | bei $\alpha > 0$ ist $S < p$ **zwingend** |
| Amdahl auf skalierende Probleme anwenden | dann ist Gustafson die richtige Frage |
| Overhead vergessen | reale Kurven haben ein Maximum, Amdahl-Kurven nicht |

---

## 3. Gustafson Schritt für Schritt

### 3.1 Die andere Frage

Amdahl fragt: *„Ich habe ein Problem fester Größe. Wie viel schneller wird es?"*

Gustafson fragt: *„Ich habe ein Zeitbudget. Wie viel größer darf mein Problem werden?"*

Das ist keine Spitzfindigkeit, sondern die Realität im HPC: Niemand kauft einen Cluster, um
dieselbe Wettervorhersage in einer Sekunde statt in einer Stunde zu rechnen. Man rechnet
**eine Stunde lang ein feineres Gitter**.

### 3.2 Die Herleitung

Jetzt wird die **parallele** Laufzeit normiert: $T(n;p) = 1$. Der serielle Anteil daran ist
$\gamma$:

$$T_{\text{ser}} = \gamma, \qquad \frac{T_{\text{par}}}{p} = 1 - \gamma
\qquad\Longrightarrow\qquad T_{\text{par}} = p\,(1-\gamma)$$

Die **hypothetische serielle Laufzeit** desselben (großen) Problems wäre:

$$T(n;1) = T_{\text{ser}} + T_{\text{par}} = \gamma + p(1-\gamma)$$

Also

$$S = \frac{T(n;1)}{T(n;p)} = \frac{\gamma + p(1-\gamma)}{1} = \gamma + p - p\gamma$$

$$\boxed{S(n;p) = p + \gamma\,(1-p)}$$

### 3.3 Zwei äquivalente Formen

Beide Schreibweisen stehen in der Literatur und sind identisch:

$$S = p + \gamma(1-p) = \gamma + p(1-\gamma)$$

Die zweite ist die anschaulichere: **„der serielle Teil bleibt, der parallele wird $p$-mal
größer"**. Probe:

| $\gamma$ | $S$ | Deutung |
|---|---|---|
| 0 | $p$ | nichts seriell → perfekt |
| 1 | 1 | alles seriell → kein Gewinn |
| 0,1, $p=16$ | $16 + 0{,}1 \cdot (-15) = 14{,}5$ | |

### 3.4 Was $\gamma$ wirklich ist

$\gamma$ ist der Anteil der seriellen Arbeit an der **beobachteten parallelen** Laufzeit —
also an dem, was man mit einer Stoppuhr am laufenden parallelen Programm messen würde.

**Das ist der leichter messbare Wert**: Man profiliert den parallelen Lauf und liest ab,
welcher Zeitanteil in seriellen Abschnitten steckt. Für $\alpha$ müsste man den seriellen
Lauf desselben Problems haben — bei einem Problem, das nur auf 1024 Knoten in den Speicher
passt, ist das unmöglich.

### 3.5 Ein konkretes Szenario

Wetterlauf, Zeitbudget **1 Stunde**, $\gamma = 0{,}1$ (6 Minuten Ein-/Ausgabe und Steuerung).

| $p$ | Gitterpunkte (relativ) | $S$ | $E$ |
|---|---|---|---|
| 1 | 1,0 | 1,00 | 100 % |
| 2 | 1,8 | 1,90 | 95,0 % |
| 4 | 3,6 | 3,70 | 92,5 % |
| 8 | 7,2 | 7,30 | 91,2 % |
| 16 | 14,4 | 14,50 | 90,6 % |
| 64 | 57,6 | 57,70 | 90,2 % |

**Die Effizienz fällt nicht gegen null, sondern gegen $1 - \gamma = 0{,}9$.** Denn:

$$E = \frac{S}{p} = \frac{p + \gamma(1-p)}{p} = 1 - \gamma + \frac{\gamma}{p}
\ \xrightarrow{p\to\infty}\ 1 - \gamma$$

Das ist der optimistische Kern von Gustafson: **Weak Scaling skaliert unbegrenzt**, solange
das Problem mitwächst.

---

## 4. Der aufgelöste Widerspruch ⚑

Hier steckt der eigentliche Erkenntnisgewinn — und eine beliebte Prüfungsfrage.

### 4.1 Der scheinbare Widerspruch

Für $\alpha = \gamma = 0{,}1$ und $p = 16$:

$$S_{\text{Amdahl}} = \frac{1}{0{,}1 + 0{,}9/16} = 6{,}4 \qquad\qquad
S_{\text{Gustafson}} = 16 + 0{,}1(1-16) = 14{,}5$$

Faktor 2,3 Unterschied bei „demselben" Zahlenwert. Wie kann das sein?

### 4.2 Die Umrechnung

**Weil $\alpha$ und $\gamma$ nicht dasselbe messen.** Sie lassen sich ineinander umrechnen.
Mit $T_{\text{ser}} = \alpha$, $T_{\text{par}} = 1-\alpha$ (Normierung $T(n;1)=1$):

$$\gamma = \frac{T_{\text{ser}}}{T_{\text{ser}} + T_{\text{par}}/p}
= \frac{\alpha}{\alpha + \frac{1-\alpha}{p}}
= \frac{\alpha p}{\alpha p + 1 - \alpha}$$

$$\boxed{\gamma = \frac{\alpha p}{\alpha p + 1 - \alpha}
\qquad\qquad
\alpha = \frac{\gamma}{\gamma + p(1-\gamma)} = \frac{\gamma}{S}}$$

Die zweite Form ist besonders hübsch: **$\alpha = \gamma / S$.**

### 4.3 Bei festem Problem sind beide Gesetze identisch

Nimm $\alpha = 0{,}1$ und $p = 16$. Dann ist das **zugehörige** $\gamma$:

$$\gamma = \frac{0{,}1 \cdot 16}{0{,}1 \cdot 16 + 0{,}9} = \frac{1{,}6}{2{,}5} = 0{,}64$$

Und damit:

$$S_{\text{Gustafson}} = 16 + 0{,}64\,(1-16) = 16 - 9{,}6 = \mathbf{6{,}4}$$

**Exakt der Amdahl-Wert.** Das gilt allgemein:

| $\alpha$ | $p$ | $S_{\text{Amdahl}}$ | zugehöriges $\gamma$ | $S_{\text{Gustafson}}$ |
|---|---|---|---|---|
| 0,05 | 4 | 3,478 | 0,1739 | **3,478** |
| 0,05 | 16 | 9,143 | 0,4571 | **9,143** |
| 0,05 | 64 | 15,422 | 0,7711 | **15,422** |
| 0,10 | 16 | 6,400 | 0,6400 | **6,400** |
| 0,20 | 64 | 4,706 | 0,9412 | **4,706** |

> **Die Auflösung:** Amdahl und Gustafson sind **dieselbe Gleichung**, nur nach verschiedenen
> Variablen aufgelöst. Für ein **festes** Problem liefern sie **immer denselben** Speedup.
>
> Der Unterschied entsteht erst durch das **Szenario**: Bei Gustafson hält man $\gamma$
> konstant, während $p$ wächst — und das bedeutet, dass man das Problem vergrößert. Denn
> $\gamma$ konstant zu halten heißt nach der Umrechnung, dass $\alpha$ **sinkt**:

| $p$ | $\gamma$ (fest) | $S$ | äquivalentes $\alpha$ |
|---|---|---|---|
| 1 | 0,1 | 1,0 | 0,1000 |
| 2 | 0,1 | 1,9 | 0,0526 |
| 4 | 0,1 | 3,7 | 0,0270 |
| 8 | 0,1 | 7,3 | 0,0137 |
| 16 | 0,1 | 14,5 | 0,0069 |
| 64 | 0,1 | 57,7 | 0,0017 |

**Ein festes $\gamma$ bei wachsendem $p$ ist ein Problem, dessen serieller Anteil $\alpha$
immer kleiner wird** — weil der parallele Teil mitwächst. Genau das passiert, wenn man das
Gitter verfeinert: $T_{\text{par}} \sim n^3$, $T_{\text{ser}} \sim n$.

### 4.4 Wann welches Gesetz?

| Frage | Gesetz | Messvorschrift |
|---|---|---|
| „Wird meine Simulation mit 64 Kernen schneller fertig?" | **Amdahl** | $n$ konstant halten, $p$ variieren |
| „Lohnt sich der größere Cluster für feinere Modelle?" | **Gustafson** | $n/p$ konstant halten, $p$ variieren |
| „Ist mein Code gut parallelisiert?" | **Karp-Flatt** | $e(p)$ berechnen und auf Konstanz prüfen |
| „Lohnt sich die GPU?" | **Amdahl** mit Transfer als zusätzlichem seriellen Anteil | Kap. 11, Aufgabe 11.7 |

**Für die Klausur:** Wenn in der Aufgabe steht „*die Problemgröße bleibt gleich*" → Amdahl.
Steht dort „*jeder Prozessor bearbeitet gleich viele Elemente*" oder „*in derselben Zeit*"
→ Gustafson. Wenn nichts dasteht, gilt die Konvention **Amdahl** (Strong Scaling).

---

## 5. Roofline Schritt für Schritt

### 5.1 Die Herleitung

Amdahl und Gustafson fragen nach der **Prozessorzahl**. Roofline fragt nach etwas ganz
anderem: **Wodurch ist ein einzelner Kernel begrenzt?**

Ein Kernel leistet $W_{\text{flop}}$ Gleitkommaoperationen und bewegt dabei
$W_{\text{byte}}$ Byte aus dem Hauptspeicher. Für die Laufzeit gibt es **zwei untere
Schranken**:

$$T \ \ge\ \frac{W_{\text{flop}}}{\pi} \qquad \text{(die ALUs schaffen nicht mehr)}$$
$$T \ \ge\ \frac{W_{\text{byte}}}{\beta} \qquad \text{(der Speicherbus schafft nicht mehr)}$$

Beides muss gelten, also gilt das Maximum — und **im besten Fall überlappen** Rechnen und
Laden vollständig:

$$T \ \ge\ \max\!\left(\frac{W_{\text{flop}}}{\pi},\ \frac{W_{\text{byte}}}{\beta}\right)$$

Die erreichte Leistung ist $P = W_{\text{flop}}/T$, also:

$$P \ \le\ \frac{W_{\text{flop}}}{\max\!\left(\frac{W_{\text{flop}}}{\pi}, \frac{W_{\text{byte}}}{\beta}\right)}
= \min\!\left(\pi,\ \frac{W_{\text{flop}}}{W_{\text{byte}}}\,\beta\right)$$

Mit der **arithmetischen Intensität** $I = W_{\text{flop}}/W_{\text{byte}}$:

$$\boxed{P \ \le\ \min\bigl(\pi,\ \beta \cdot I\bigr)}$$

Man beachte: $I$ hängt **nur vom Algorithmus** ab, $\pi$ und $\beta$ **nur von der Hardware**.
Das ist der Grund, warum das Modell so nützlich ist — es trennt beides sauber.

### 5.2 Warum ein log-log-Diagramm

Trägt man $P$ über $I$ auf, besteht die Schranke aus zwei Teilen:

- $P = \beta \cdot I$ — im **linearen** Plot eine Gerade durch den Ursprung mit Steigung $\beta$
- $P = \pi$ — eine Waagerechte

Logarithmiert man beide Achsen, wird aus dem ersten Teil:

$$\log P = \log \beta + \log I$$

— eine Gerade mit **Steigung 1**, unabhängig von $\beta$. $\beta$ verschiebt sie nur nach
oben oder unten. Der zweite Teil bleibt waagerecht. Damit sind beide Teile Geraden, und
Kernel über viele Größenordnungen von $I$ passen ins selbe Bild.

```
   P [GFLOP/s]
   (log)
      ^
      |
   π ─┼─ ─ ─ ─ ─ ─ ─ ─ ─ ┌─────────────────────  Rechen-Dach:  P = π
      |                  │
      |                 ╱│
      |               ╱  │
      |             ╱    │        rechengebunden
      |           ╱      │        (compute-bound)
      |         ╱        │
      |       ╱   Speicher-Dach:  P = β·I   (Steigung 1)
      |     ╱            │
      |   ╱  speicher-   │
      | ╱    gebunden    │
      +──────────────────┼─────────────────────>  I [FLOP/Byte] (log)
                       I* = π/β
                    (Knickpunkt / ridge point)
```

### 5.3 Das Diagramm lesen — in vier Schritten

1. **Knickpunkt bestimmen:** $I^\star = \pi/\beta$. Das ist eine reine Hardware-Eigenschaft.
2. **$I$ des Kernels bestimmen:** FLOPs zählen, DRAM-Bytes zählen, dividieren.
3. **Senkrecht nach oben gehen**, bis man das Dach trifft — das ist $P_{\max}$.
4. **Gemessenes $P$ eintragen** und den Abstand zum Dach ansehen.

Der **Abstand zum Dach** ist die eigentliche Diagnose:

| Abstand | Deutung |
|---|---|
| $P \approx P_{\max}$ | fertig — die Hardware gibt nicht mehr her |
| $P \approx \tfrac12 P_{\max}$ | typisch für gut optimierten Code; weiter lohnt selten |
| $P \ll P_{\max}$, $I < I^\star$ | Datenbewegung ineffizient: schlechtes Coalescing/Layout, zu wenig Parallelität |
| $P \ll P_{\max}$, $I > I^\star$ | Instruktionen ineffizient: keine SIMD/FMA, Divergenz, Abhängigkeiten |

### 5.4 FLOPs und Bytes richtig zählen

Das ist der Teil, an dem Klausuraufgaben tatsächlich scheitern. Die Regeln:

**FLOPs zählen:**

| Regel | Beispiel |
|---|---|
| $+$, $-$, $\times$ zählen je **1** | `a*b + c` = 2 FLOP |
| **FMA zählt als 2** | `fma(a,b,c)` = 2 FLOP |
| Division und `sqrt` — nach Aufgabenstellung | Blatt 11 zählt `rsqrtf` als **1** |
| Vergleiche, Indexrechnung, Schleifenzähler | zählen **nicht** |

**Bytes zählen:**

| Regel | Begründung |
|---|---|
| nur **DRAM**-Verkehr | was aus dem Cache kommt, belegt keine Speicherbandbreite |
| **Schreiben zählt wie Lesen** | belegt dieselbe Bandbreite |
| Skalare in Registern zählen **nicht** | `s += ...` bewegt nichts |
| Datentyp beachten | `double` verdoppelt Bytes und **halbiert $I$** |
| Jedes Element **einmal**, wenn es wiederverwendet wird | genau hier wirkt Blocking |

**Der häufigste Fehler:** die Ausgabe vergessen. Bei `y[i] = a*x[i] + y[i]` werden **drei**
Wörter bewegt (x lesen, y lesen, y schreiben), nicht zwei.

**Der zweithäufigste:** ein Skalar wie `t` in `t += x[i]*x[i]` als Speicherzugriff zählen.
Der Compiler hält ihn in einem Register — es sind 4 Byte pro Iteration, nicht 8.

### 5.5 Algorithmische vs. gemessene Intensität

Ein wichtiger Punkt, der die Verwirrung aus Kapitel 12 auflöst.

$I$ lässt sich auf zwei Arten bestimmen:

| | Wie | Ergebnis |
|---|---|---|
| **Algorithmisch (Worst Case)** | jeden Zugriff im Quelltext als DRAM-Zugriff zählen | untere Schranke für $I$ |
| **Algorithmisch (Compulsory)** | jedes Datum nur einmal zählen (perfekter Cache) | obere Schranke für $I$ |
| **Gemessen** | Profiler liest die tatsächlichen DRAM-Transaktionen | der wahre Wert, liegt dazwischen |

Beispiel naives GEMM: Rechnet man jeden Zugriff als Miss, kommt $I = 0{,}25$ heraus, also
$P \le 375$ GFLOP/s auf einer A100. Gemessen wird **mehr** — kein Widerspruch, sondern der
**L2-Cache**, der einen Teil der Wiederverwendung gratis liefert. Die Zahl 0,25 ist eine
pessimistische Abschätzung, nicht der wahre Verkehr.

Umgekehrt: Beim gekachelten GEMM mit $I = 8$ und $P \le 12\,000$ GFLOP/s wird die Schranke
**deutlich verfehlt** — weil $\pi$ lauter FMA-Instruktionen ohne Adressrechnung,
Shared-Memory-Zugriffe und `__syncthreads()` unterstellt.

> **Die Roofline ist eine Schranke, kein Ziel.** Sie sagt zuverlässig, was *nicht* geht.
> Was tatsächlich geht, sagt erst die Messung.

### 5.6 Mehrere Dächer (Ceilings)

Das vollständige Roofline-Modell hat nicht ein Dach, sondern mehrere — jedes entspricht einer
Optimierung, die man **nicht** gemacht hat:

```
   P
   ^
 π ┼─ ─ ─ ─ ─ ─ ┌──────────────  mit FMA + SIMD  (volles π)
   |            │
π/2┼─ ─ ─ ─ ─ ┌─┘                ohne FMA
   |          │
π/8┼─ ─ ─ ─ ┌─┘                  ohne SIMD (skalar)
   |      ╱ │
   |    ╱   │                    Speicher-Dächer analog:
   |  ╱     │                      - volle Bandbreite (Prefetch, NUMA korrekt)
   +────────┴──────────────>       - ohne Prefetching
                          I        - ohne NUMA-Bindung
```

Der praktische Nutzen: Liegt der Messpunkt genau auf dem „ohne-SIMD"-Dach, weiß man sofort,
woran es liegt — der Compiler hat nicht vektorisiert (Kapitel 05b).

Für die Klausur reicht das einfache Modell mit einem Dach; die Ceilings sind der Grund, warum
ein Kernel „unter der Roofline, aber genau auf einer Linie" liegen kann.

### 5.7 Was die Roofline nicht sagt

| Nicht erfasst | Warum es trotzdem bremst |
|---|---|
| **Latenz** | zu wenige gleichzeitige Zugriffe → Bandbreite nicht ausgeschöpft (Little's Law, Kap. 11) |
| **Occupancy / Parallelität** | ein Thread erreicht nie $\beta$, egal wie gut sein $I$ ist |
| **Divergenz, Bankkonflikte** | senken den effektiven Durchsatz, nicht die Schranke |
| **Lastungleichgewicht** | Roofline betrachtet einen Kernel, nicht die Verteilung |
| **PCIe-Transfer** | steht außerhalb — muss separat gerechnet werden (Abschnitt 6) |

**Roofline ist ein Modell für den stationären Durchsatz eines Kernels.** Alles, was mit
Anlaufen, Verteilen oder Warten zu tun hat, gehört zu Amdahl.

---

## 6. Alles zusammen: der GPU-Fall

Auf der GPU greifen alle drei Modelle gleichzeitig ineinander — das ist der beste Test dafür,
ob man sie verstanden hat.

**Die Frage:** Lohnt sich ein Kernel auf der GPU?

**Schritt 1 — Roofline: Wie schnell kann der Kernel überhaupt sein?**

$$I = \frac{\text{FLOP}}{\text{Byte}}, \qquad P_{\text{GPU}} \le \min(\pi_{\text{GPU}}, \beta_{\text{GPU}} \cdot I)$$

**Schritt 2 — Amdahl: Der Transfer ist zusätzlicher serieller Anteil.** Er kommt in der
CPU-Fassung gar nicht vor, ist also reiner Zusatzaufwand:

$$T_{\text{GPU,gesamt}} = \underbrace{T_{\text{ser}}}_{\text{CPU-Rest}} + \underbrace{T_{\text{transfer}}}_{\text{neu!}} + \underbrace{\frac{T_{\text{par}}}{s}}_{\text{Kernel}}$$

$$S_{\max} = \frac{T_{\text{ser}} + T_{\text{par}}}{T_{\text{ser}} + T_{\text{transfer}}}
\qquad \text{(selbst bei unendlich schnellem Kernel)}$$

**Schritt 3 — Die Break-Even-Rechnung.** Ein Kernel verarbeitet $n$ Elemente, überträgt
$b$ Byte pro Element über PCIe und rechnet $q$ FLOP pro Element. Die GPU gewinnt, wenn

$$\underbrace{\frac{bn}{\beta_{\text{PCIe}}}}_{\text{Transfer}} + \underbrace{\frac{qn}{\pi_{\text{GPU}}}}_{\text{Kernel}}
\ <\ \underbrace{\frac{qn}{\pi_{\text{CPU}}}}_{\text{CPU}}$$

**$n$ kürzt sich heraus** — die Schwelle hängt **nicht von der Problemgröße ab**, nur von der
Arbeit pro Element:

$$q \ >\ \frac{b/\beta_{\text{PCIe}}}{\dfrac{1}{\pi_{\text{CPU}}} - \dfrac{1}{\pi_{\text{GPU}}}}$$

Mit $b = 8$ Byte, $\beta_{\text{PCIe}} = 12$ GB/s, $\pi_{\text{GPU}} = 15$ TFLOP/s,
$\pi_{\text{CPU}} = 100$ GFLOP/s:

$$q > \frac{6{,}667\cdot10^{-10}}{10^{-11} - 6{,}667\cdot10^{-14}} = \mathbf{67{,}1\ \text{FLOP pro Element}}$$

Da der GPU-Term meist vernachlässigbar ist, geht auch die Kopfrechnung:

$$q \gtrsim \frac{b \cdot \pi_{\text{CPU}}}{\beta_{\text{PCIe}}} = \frac{8 \cdot 10^{11}}{12 \cdot 10^9} = 66{,}7$$

*„Die CPU muss mindestens so lange rechnen, wie das Kopieren dauert."*

**Die drei Zahlen für SAXPY auf einer A100:**

| Frage | Modell | Antwort |
|---|---|---|
| Wie schnell kann der Kernel sein? | Roofline | $I = 0{,}167 \Rightarrow P \le 250$ GFLOP/s = 1,3 % von $\pi$ |
| Lohnt sich der Transfer? | Break-Even | $q = 2 \ll 67$ → **nein** |
| Was wäre der beste Gesamtgewinn? | Amdahl | begrenzt durch $T_{\text{ser}} + T_{\text{transfer}}$ |

**Und die Konsequenz, die in der Praxis alles entscheidet:** Wenn 50 Kernels nacheinander auf
demselben Puffer arbeiten, fällt der Transfer **einmal** statt 50-mal an — die effektive
Schwelle sinkt um Faktor 50. Deshalb gewinnen iterative Löser auf der GPU, obwohl jeder
einzelne ihrer `axpy`-Schritte es für sich genommen nicht täte.

---

## 7. Aufgabentypen mit Musterlösung

### Typ A — Amdahl vorwärts

> *Ein Programm läuft 200 s. 15 % der Zeit sind seriell. Wie schnell läuft es auf 8 Kernen?
> Was ist maximal erreichbar?*

$$S(8) = \frac{1}{0{,}15 + \frac{0{,}85}{8}} = \frac{1}{0{,}15 + 0{,}10625} = \frac{1}{0{,}25625} = 3{,}90$$

$$T(8) = \frac{200}{3{,}90} = 51{,}2\ \text{s}, \qquad E = \frac{3{,}90}{8} = 48{,}8\ \%$$

$$S_{\max} = \frac{1}{0{,}15} = 6{,}67 \qquad\Rightarrow\qquad T_{\min} = 30\ \text{s}$$

**Probe über die Zeiten:** $0{,}15 \cdot 200 = 30$ s seriell, $170/8 = 21{,}25$ s parallel,
zusammen 51,25 s. ✓ *(Diese Probe sollte man immer machen — sie deckt Formelfehler sofort auf.)*

### Typ B — Amdahl rückwärts

> *Auf 16 Kernen wird ein Speedup von 8 gemessen. Wie groß ist der serielle Anteil?*

Karp-Flatt:

$$e = \frac{\frac18 - \frac1{16}}{1 - \frac1{16}} = \frac{0{,}125 - 0{,}0625}{0{,}9375} = \frac{0{,}0625}{0{,}9375} = 0{,}0667$$

Also **$\alpha \approx 6{,}7\,\%$** — sofern kein Overhead im Spiel ist. Maximal erreichbar
wären $1/0{,}0667 = 15$.

### Typ C — Gustafson

> *Ein Programm läuft auf 32 Prozessoren 10 Minuten, davon 1 Minute seriell. Wie groß ist der
> skalierte Speedup?*

$$\gamma = \frac{1}{10} = 0{,}1$$

$$S = 32 + 0{,}1\,(1 - 32) = 32 - 3{,}1 = \mathbf{28{,}9}$$

**Deutung:** Dasselbe Problem seriell zu rechnen würde $28{,}9 \cdot 10 = 289$ Minuten dauern.
Probe: 1 min seriell + 9 min × 32 = 288 min parallelisierbar → 289 min. ✓

### Typ D — Roofline einordnen

> *Maschine: $\pi = 3$ GFLOP/s, $\beta = 8$ GB/s. Ordne die beiden Schleifen ein.*

$$I^\star = \frac{3}{8} = 0{,}375\ \text{FLOP/Byte}$$

| Schleife | FLOP | Byte | $I$ | $P \le$ | Klasse |
|---|---|---|---|---|---|
| `x[i] = x[i] + y[i]` | 1 | 12 (x lesen, y lesen, x schreiben) | 0,083 | $8 \cdot 0{,}083 = 0{,}67$ GFLOP/s | **speichergebunden** (22 % von $\pi$) |
| `t = t + x[i]*x[i]` | 2 | 4 (nur x; `t` im Register) | 0,500 | $\min(3, 4) = 3$ GFLOP/s | **rechengebunden** (100 % von $\pi$) |

### Typ E — Roofline mit Optimierung

> *Ein Kernel hat $I = 0{,}25$ auf einer A100. Nach Kacheln steigt er auf $I = 8$. Wie ändert
> sich die Schranke, und welche Klasse hat er jeweils?*

$$I^\star = \frac{19\,500}{1500} = 13$$

| | $I$ | $P \le \min(19500, 1500 I)$ | Klasse |
|---|---|---|---|
| vorher | 0,25 | 375 GFLOP/s (1,9 % von $\pi$) | speichergebunden |
| nachher | 8 | 12 000 GFLOP/s (62 % von $\pi$) | **noch immer** speichergebunden, aber knapp |

Die Schranke steigt um **Faktor 32** — genau der Faktor, um den $I$ gestiegen ist, weil man
links vom Knick bleibt. Erst ab $I > 13$ (also `TILE` $> 52$) würde der Kernel die Klasse
wechseln, und dann bringt weiteres Kacheln nichts mehr.

### Typ F — kombiniert

> *Ein Programm hat 5 % seriellen Anteil. Der parallele Teil läuft auf der GPU 60× schneller,
> der Transfer kostet zusätzlich 20 % der ursprünglichen Laufzeit. Gesamtspeedup? Was ist
> maximal erreichbar?*

Mit $T_1 = 100$:

$$T_{\text{GPU}} = \underbrace{5}_{\text{seriell}} + \underbrace{\frac{95}{60}}_{= 1{,}58} + \underbrace{20}_{\text{Transfer}} = 26{,}58$$

$$S = \frac{100}{26{,}58} = \mathbf{3{,}76}$$

**Ohne** Transfer wäre es $100/6{,}58 = 15{,}2$ — der Transfer kostet Faktor **4**.

Maximal erreichbar, selbst bei unendlich schnellem Kernel:

$$S_{\max} = \frac{100}{5 + 20} = \mathbf{4{,}0}$$

**Interpretation:** Der Transfer ist zum neuen, dominanten „seriellen Anteil" geworden — er
ist viermal größer als der echte. Eine schnellere GPU hilft **gar nicht**; helfen würde nur,
den Transfer loszuwerden.

---

## 8. Merkkasten

> **Amdahl**
> - $S = \dfrac{1}{\alpha + \frac{1-\alpha}{p}} \to \dfrac1\alpha$, &nbsp;
>   $E = \dfrac{1}{\alpha p + 1 - \alpha} \to 0$
> - $\alpha$ bezieht sich auf die **serielle** Laufzeit und ist ein **Zeit**anteil.
> - Lohnend bis etwa $p \approx 1/\alpha$; darüber ist $E < \tfrac12$.
> - Mit Overhead $cp$: Optimum bei $p^* = \sqrt{(1-\alpha)/c}$ — reale Kurven haben ein Maximum.
> - **Karp-Flatt** $e = \dfrac{1/S - 1/p}{1 - 1/p}$: konstant → serieller Anteil, wachsend →
>   Overhead.
>
> **Gustafson**
> - $S = p + \gamma(1-p) = \gamma + p(1-\gamma)$, &nbsp; $E \to 1 - \gamma$
> - $\gamma$ bezieht sich auf die **parallele** Laufzeit und ist leichter zu messen.
>
> **Der Zusammenhang** ⚑
> - $\gamma = \dfrac{\alpha p}{\alpha p + 1 - \alpha}$, &nbsp; $\alpha = \dfrac{\gamma}{S}$
> - Bei **festem Problem liefern beide denselben Speedup.** Der Unterschied liegt im
>   Szenario: festes $\gamma$ bei wachsendem $p$ heißt sinkendes $\alpha$ — also ein
>   **mitwachsendes Problem**.
>
> **Roofline**
> - $T \ge \max\!\left(\frac{W_{\text{flop}}}{\pi}, \frac{W_{\text{byte}}}{\beta}\right)
>   \Rightarrow P \le \min(\pi, \beta I)$, Knick bei $I^\star = \pi/\beta$.
> - $I$ gehört zum **Algorithmus**, $\pi$ und $\beta$ zur **Hardware** — das ist die Stärke
>   des Modells.
> - Zählen: DRAM-Bytes, Schreiben zählt mit, Register nicht, FMA = 2 FLOP, `double` halbiert $I$.
> - Es ist eine **Schranke**, kein Ziel; Latenz, Occupancy und Divergenz stehen nicht drin.
>
> **Zusammen**
> - Roofline sagt, wie schnell ein Kernel sein **kann**; Amdahl, wie viel davon beim
>   **Programm** ankommt.
> - Ein PCIe-Transfer wirkt wie zusätzlicher **serieller** Anteil und deckelt den Speedup
>   unabhängig von der GPU-Geschwindigkeit.

---

**Zurück:** [Kapitel 10](theorie.md) · [Tutorial-Übersicht](../README.md) ·
[Formelsammlung](../formelsammlung.md)
