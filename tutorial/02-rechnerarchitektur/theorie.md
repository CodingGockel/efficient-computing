# Kapitel 02 — Rechnerarchitektur-Prinzipien: ISA, Pipelining, ILP

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/2-Lecture-RechnerarchitekturPrinzipien.pdf` (30 Folien), Übungsblatt
> `excercises/uebung3.pdf` (RISC-V-Pipeline), eigene Abgabe `assignment3/`
> **Zeitbedarf:** ca. 20 min

---

## 1. Worum es geht

Software entwickelt sich **langsam und heterogen**, Hardware **schnell und homogen**. Damit
beide unabhängig voneinander vorankommen, braucht es eine stabile Schnittstelle: die **ISA**.
Und weil die Software eine *sequentielle* Ausführung erwartet, die Hardware aber schneller sein
will, muss sie parallel rechnen und dabei so tun, als täte sie es nicht. Das ist die zentrale
Spannung dieses Kapitels.

---

## 2. Die Kernpunkte

### 2.1 Die ISA als Vertrag

Das **Instruction Set Architecture** ist die Spezifikation der Befehlsmenge, die eine Hardware
korrekt implementieren muss. Sie ist die **Schnittstelle zwischen Hard- und Software** und
garantiert Portabilität und Korrektheit.

Ein Prozessor besteht im Modell aus:

- einer **kleinen, festen Zahl von Registern** (RISC-V: `x0`–`x31`, je 64 Bit),
- einem **großen, byteadressierbaren Speicher**,
- einem **Program Counter (PC)**.

```
add r3, r1, r2        bedeutet        r3 ← r1 + r2
```

Ein C-Ausdruck wird in eine Folge solcher Befehle übersetzt:

```
int res = (((x+y)-z)+x)+y;      // x in x2, y in x3, z in x4
                                 foo:
                                   add x5, x2, x3    # t = x+y
                                   sub x6, x5, x4    # r = t-z
                                   add x6, x6, x2    # r = r+x
                                   add x6, x6, x3    # r = r+y
```

### 2.2 Die sieben Teilschritte eines Befehls

1. **Fetch** — Befehl aus dem Speicher laden
2. **Decode** — Befehlstyp feststellen
3. **Read** — Operanden laden
4. **Compute** — Operation ausführen
5. **Read** — aus dem Speicher lesen, falls nötig
6. **Write** — Ergebnis schreiben
7. **Update PC** — Befehlszähler setzen

Für jeden Schritt gibt es **eigene Hardware**. Genau das macht Pipelining möglich: Während
Befehl 1 in der Execute-Stufe steckt, kann Befehl 2 schon dekodiert werden.

Ein einzelner Befehl ändert nur einen kleinen Teil des Prozessorzustands. Die **sequentielle
Abstraktion** — die Befehle wirken, als liefen sie streng nacheinander — ist die Grundlage
für Komposition, Modularität und Korrektheitsargumente. Die Hardware darf sie beschleunigen,
aber nicht sichtbar brechen.

### 2.3 Die vier Wege zur Beschleunigung

**(a) Die Zeit pro Teilaufgabe minimieren.** Verletzt die sequentielle Abstraktion nicht,
stößt aber an physikalische Grenzen (Kapitel 03).

**(b) Instruction-Level Parallelism (ILP)** — der Software die Illusion der Sequenz lassen,
in der Hardware aber parallel arbeiten:

| Technik | Idee |
|---|---|
| **Pipelining** | unabhängige *Teilaufgaben verschiedener Instanzen* gleichzeitig ausführen |
| **Multiple Issue** | mehrere *unabhängige Befehle* gleichzeitig starten (superskalar) |
| **Out-of-Order Execution** | Befehle umsortieren, wenn Abhängigkeiten es erlauben |
| **Speculative Execution** | bei Sprüngen raten und den geratenen Pfad vorab rechnen |

### 2.4 Pipelining — die Formel, die man können muss

**Modell:** $n$ Instanzen eines Ablaufs mit $k$ Teilaufgaben; jede Teilaufgabe braucht die
Zeit $t$. Verschiedene Teilaufgaben verschiedener Instanzen dürfen parallel laufen, **dieselbe**
Teilaufgabe verschiedener Instanzen nicht.

$$T(n,k,t) = n\,k\,t \qquad \text{(ohne Pipelining)}$$

$$T_{\text{Pipe}}(n,k,t) = \underbrace{k\,t}_{\text{Füllen}} + \underbrace{(n-1)\,t}_{\text{Fließbetrieb}} = (n + k - 1)\,t$$

$$S = \frac{T}{T_{\text{Pipe}}} = \frac{n\,k}{n + k - 1} = \frac{k}{1 + \frac{k-1}{n}} \xrightarrow{n \to \infty} \boxed{k}$$

**Interpretation:** Der maximale Speedup ist die **Pipeline-Tiefe** $k$ — aber nur im Grenzwert.
Der Term $k t$ ist das Füllen der Pipeline und amortisiert sich erst über viele Instanzen.

Zahlenbeispiel ($k = 5$):

| $n$ | $S$ |
|---|---|
| 1 | 1,00 |
| 5 | 2,78 |
| 100 | 4,81 |
| $\infty$ | 5,00 |

> Dieselbe Struktur wie beim Stream-Pipelining in Kapitel 12: **Füllen + Leeren gegen
> Fließbetrieb.** Wer die eine Formel kann, kann beide.

**Was die Pipeline stört** (Details in Kapitel 05b): Datenabhängigkeiten (RaW), Sprünge,
mehrtaktige Operationen. Dann entstehen *stalls*, und $S$ bleibt unter $k$.

### 2.5 Multiple Issue und SIMD

**Multiple Issue:** Gibt es unabhängige Befehle und genügend Hardware-Einheiten, startet der
Prozessor mehrere gleichzeitig. Voraussetzung ist Unabhängigkeit — dieselbe Frage wie bei den
Bernstein-Bedingungen in Kapitel 07.

**SIMD** (*Single Instruction Multiple Data*): Eine Folge identischer Befehle auf
verschiedenen Skalardaten **ohne Datenabhängigkeit** wird durch **einen Vektorbefehl** ersetzt.

```
x1 ← ADD(y1, z1)
x2 ← ADD(y2, z2)          ⟹      x[1:4] ← VECADD(y[1:4], z[1:4])
x3 ← ADD(y3, z3)
x4 ← ADD(y4, z4)
```

Entwicklung der x86-Vektorerweiterungen:

| Name | Breite | Jahr | Register |
|---|---|---|---|
| MMX | 64 Bit | 1997 | `mm0`–`mm7` |
| SSE | 128 Bit | 1999 | `xmm0`–`xmm7` |
| SSE2 | 128 Bit | 2000 | `xmm0`–`xmm15` |
| AVX | 128 Bit | 2011 | `xmm0`–`xmm15` |
| AVX2 | 256 Bit | 2013 | `ymm0`–`ymm15` |
| AVX-512 | 512 Bit | 2017 | `zmm0`–`zmm31` |

Details, Pipelining-Diagramme und die Peak-Performance-Formel: **Kapitel 05b**.

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Was ist eine ISA und wozu dient sie? | Spezifikation der Befehlsmenge; Schnittstelle Hard-/Software, garantiert Portabilität und Korrektheit |
| Nenne die Teilschritte der Befehlsausführung | Fetch, Decode, Read, Compute, Read, Write, PC setzen |
| Formel für die Pipeline-Zeit | $T_{\text{Pipe}} = (n + k - 1)\,t$ |
| Maximaler Pipeline-Speedup | $S = nk/(n+k-1) \to k$ für $n \to \infty$ |
| Warum erreicht man $k$ nie ganz? | das Füllen der Pipeline ($kt$) kostet einmalig; dazu stalls durch Abhängigkeiten und Sprünge |
| Nenne vier ILP-Techniken | Pipelining, Multiple Issue, Out-of-Order, Speculative Execution |
| Was ist die Voraussetzung für SIMD? | identische Operation, verschiedene Daten, **keine** Datenabhängigkeit |
| Warum bleibt die sequentielle Abstraktion erhalten? | sie ist Voraussetzung für Komposition, Modularität und Korrektheitsargumente der Software |

---

## 4. Merkkasten

> - **ISA** = Vertrag zwischen Hardware und Software; sie entkoppelt zwei unterschiedlich
>   schnelle Entwicklungen.
> - Die Hardware **simuliert** Sequentialität und rechnet darunter parallel — das ist ILP.
> - **Pipelining:** $T_{\text{Pipe}} = (n+k-1)t$, $S \to k$. Tiefe ist die Obergrenze,
>   Füllen und stalls sind der Abzug.
> - **Multiple Issue** braucht unabhängige Befehle, **SIMD** braucht unabhängige Daten.
> - Sobald die Taktfrequenz nicht mehr steigt (Kapitel 03), ist ILP der einzige verbleibende
>   Hebel innerhalb der sequentiellen Abstraktion — und danach nur noch **explizite**
>   Parallelität.

---

## 5. Verbindung

**Baut darauf auf:** Kapitel 05b vertieft SIMD samt Pipeline-Diagrammen, Latency/Throughput
und der Peak-Performance-Formel. Kapitel 03 erklärt, warum ILP überhaupt nötig wurde.
Kapitel 07 verallgemeinert „unabhängig" zu den Bernstein-Bedingungen. Die SIMT-Ausführung
der GPU (Kapitel 11) ist die konsequente Weiterführung des SIMD-Gedankens.

**Eigene Abgabe:** `assignment3/` — RISC-V-Pipeline (Blatt 3).
