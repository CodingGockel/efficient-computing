# Kapitel 05b — SIMD, Pipelining und Peak-Performance

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/extra-material/Ex-3-SIMD.pdf` (13 Folien), Übungsblatt
> `excercises/uebung4.pdf`, eigene Abgabe `assignment4/` (RISC-V / K230)
> **Zeitbedarf:** ca. 20 min
> **Voraussetzungen:** Kapitel 02 (Pipelining, ISA)

---

## 1. Worum es geht

Die Leitfrage des Foliensatzes lautet: **„Warum ist die SIMD-Variante nicht viermal schneller
als die skalare Variante?"** Die Antwort erklärt gleichzeitig, wie moderne Prozessoren
wirklich arbeiten — und warum eine Peak-Performance-Angabe fast nie erreicht wird.

---

## 2. Die Kernpunkte

### 2.1 SIMD-Instruktionen

Ein Vektorregister enthält mehrere Skalare, eine Instruktion bearbeitet alle gleichzeitig.
ARM-NEON-Notation (`assignment4/`):

| Instruktion | Register | Vektorlänge | Ergebnisse/Instr. | Rechnungen/Instr. |
|---|---|---|---|---|
| `fmla v2.4s, v0.4s, v1.4s` | 4 × `float` | 4 | 4 | **8** (4 MUL + 4 ADD) |
| `fmla v2.2d, v0.2d, v1.2d` | 2 × `double` | 2 | 2 | 4 |
| `fmla v2.2s, v0.2s, v1.2s` | 2 × `float` | 2 | 2 | 4 |

`fmla` ist ein **FMA** (*fused multiply-add*): $v_2 \leftarrow v_2 + v_0 \cdot v_1$, also
**zwei FLOP pro Element in einer Instruktion**.

Auf x86 heißt das SSE/AVX/AVX2/AVX-512 (Kapitel 02, Tabelle) mit 128 bis 512 Bit Breite.

### 2.2 Die drei Kennzahlen der Pipeline

Die Begriffe muss man sauber trennen — sie werden gern verwechselt:

| Begriff | Bedeutung |
|---|---|
| **Latency** | wie viele Taktzyklen die Ausführung einer Instruktion **insgesamt** dauert (Fetch bis Ergebnis) |
| **Execution Latency** | wie lange es dauert, bis eine Instruktion starten kann, die auf dem **Ergebnis** dieser Instruktion beruht — die relevante Größe bei RaW-Abhängigkeiten |
| **Throughput** | wie viele Instruktionen desselben Typs pro Takt maximal ein Ergebnis liefern |

Bei ausgelasteter Pipeline und **Throughput = 1** liefert jeder Takt ein Ergebnis, obwohl
jede einzelne Instruktion mehrere Takte braucht. Genau das ist der Pipelining-Gewinn aus
Kapitel 02.

**Superskalare Prozessoren** haben Throughput > 1 (z. B. 2): zwei Einheiten desselben Typs,
also zwei Ergebnisse pro Takt.

### 2.3 RaW-Abhängigkeiten — der eigentliche Bremsklotz

*Read after Write*: Eine Instruktion liest ein Register, das die vorige gerade schreibt.

```asm
fmla v0.4s, v0.4s, v1.4s     ; RaW auf v0:
fmla v0.4s, v0.4s, v1.4s     ; die Multiplikation muss warten, bis v0 geschrieben ist

fmla v2.4s, v0.4s, v1.4s     ; RaW auf v2:
fmla v2.4s, v0.4s, v1.4s     ; die Multiplikation kann starten, nur die Addition wartet

fmla v2.4s, v0.4s, v1.4s     ; keine RaW-Abhaengigkeit
fmla v3.4s, v0.4s, v1.4s     ; verschiedene Zielregister -> volle Pipeline
```

**Der dritte Fall ist die Lehre:** Verschiedene Akkumulatoren zu benutzen, kostet nichts und
löst die Abhängigkeitskette auf. Deshalb entrollen Compiler Schleifen und führen mehrere
Teilsummen mit.

Bei **In-Order-Execution** blockiert eine wartende Instruktion alle nachfolgenden (*stall*),
auch die unabhängigen. **Out-of-Order-Execution** zieht unabhängige Instruktionen vor und
füllt die Lücke — dieselbe Instruktionsfolge wird dadurch messbar schneller, ohne dass sich
am Code etwas ändert.

### 2.4 Die Peak-Performance-Formel

$$\text{FLOP/s} = \underbrace{\text{Takt}}_{\text{Hz}}
\times \underbrace{\frac{\text{elem. FLOP}}{\text{Instruktion}}}_{\text{FMA} = 2}
\times \underbrace{\text{Vektorlänge}}_{\text{SIMD}}
\times \underbrace{\text{Throughput}}_{= 1/\text{CPI}}$$

Beispiel aus der Vorlesung (K230, `fmla v0.4s`):

$$1{,}5 \cdot 10^9\ \text{Hz} \times 2 \times 4 \times 1 = 12 \cdot 10^9 = \mathbf{12\ \text{GFLOP/s}}$$

**Die Voraussetzungen, unter denen dieser Wert gilt** — und die in echtem Code fast nie alle
erfüllt sind:

- keine RaW-Abhängigkeiten
- eine ununterbrochene Kette von Instruktionen desselben Typs
- aufgewärmte Pipeline
- keine Speicherengpässe

Deshalb ist Peak-Performance eine **obere Schranke**, kein Versprechen — genau wie die
Roofline in Kapitel 10 und 12.

### 2.5 Die Antwort auf die Leitfrage

Beide Schleifen rechnen dasselbe:

```asm
while:                          while:
  cmp  x4, xzr                    cmp  x4, xzr
  b.eq finish                     b.eq finish
  fmadd s9,  s0,s1,s9             fmla v2.4s, v0.4s, v1.4s
  fmadd s10, s0,s1,s10
  fmadd s11, s0,s1,s11
  fmadd s12, s0,s1,s12
  sub  x4, x4, #4                 sub  x4, x4, #4
  b    while                      b    while
finish:                         finish:
```

Die skalare Variante braucht **vier** `fmadd`, die SIMD-Variante **eine** `fmla` — man würde
Faktor 4 erwarten. Gemessen wird deutlich weniger.

**Der Grund:** Die vier skalaren `fmadd` benutzen **verschiedene Zielregister** (`s9`–`s12`)
und sind damit **untereinander unabhängig**. Sie laufen vollständig durch die Pipeline
gestaffelt und kosten zusammen kaum mehr als eine einzelne. Was in beiden Fällen die
Iterationsdauer bestimmt, ist der **Schleifen-Overhead plus die RaW-Abhängigkeit über die
Akkumulatoren hinweg** — in beiden Varianten sind es rund **10 Zyklen**, bis eine neue
Iteration beginnen kann.

> **Die verallgemeinerte Lehre:** SIMD spart **Instruktionen**, nicht automatisch **Zeit**.
> Solange die Pipeline durch Abhängigkeiten oder Sprünge und nicht durch die Zahl der
> Recheninstruktionen begrenzt ist, bringt Vektorisierung wenig. Erst entrollen und
> Abhängigkeiten auflösen, dann vektorisieren.

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Was leistet `fmla v2.4s, v0.4s, v1.4s`? | FMA auf 4 `float`: 4 Ergebnisse, 8 FLOP in einer Instruktion |
| Unterschied Latency / Execution Latency / Throughput | Gesamtdauer einer Instruktion / Wartezeit für abhängige Folgeinstruktion / Ergebnisse pro Takt |
| Was ist eine RaW-Abhängigkeit? | eine Instruktion liest ein Register, das die vorige schreibt |
| Wie löst man sie auf? | verschiedene Zielregister, mehrere Akkumulatoren, Schleife entrollen |
| Unterschied In-Order / Out-of-Order | bei In-Order blockiert eine wartende Instruktion alle folgenden; OoO zieht unabhängige vor |
| Peak-Performance-Formel | Takt × FLOP/Instruktion × Vektorlänge × Throughput |
| Peak für 1,5 GHz, FMA, 4× `float`, Throughput 1 | $1{,}5\cdot10^9 \cdot 2 \cdot 4 \cdot 1 = 12$ GFLOP/s |
| Warum ist SIMD nicht automatisch $k$-mal schneller? | die Laufzeit wird von Abhängigkeiten und Schleifen-Overhead bestimmt, nicht von der Instruktionszahl |
| Voraussetzungen für Peak-Performance | keine RaW, gleichartige Instruktionskette, warme Pipeline, kein Speicherengpass |

---

## 4. Merkkasten

> - **SIMD spart Instruktionen, nicht zwangsläufig Zeit.** Was zählt, ist die Auslastung der
>   Pipeline.
> - **RaW-Abhängigkeiten sind der häufigste Bremsklotz** — mehrere Akkumulatoren lösen sie auf.
> - **Peak = Takt × FLOP/Instr. × Vektorlänge × Throughput**, gültig nur unter idealen
>   Bedingungen.
> - `fmla` auf 4 `float` = **8 FLOP pro Instruktion** (FMA zählt doppelt).
> - Reihenfolge beim Optimieren: erst Abhängigkeiten auflösen, dann vektorisieren.

---

## 5. Verbindung

**Baut auf:** Kapitel 02 (Pipelining, ILP, ISA-Tabelle der SIMD-Erweiterungen).

**Ist der CPU-Verwandte von Kapitel 11:** SIMD und SIMT sind dieselbe Hardware-Idee mit
verschiedenen Programmiermodellen. Bei SIMD sieht man **ein Vektorregister** und rechnet die
Indizes selbst; bei SIMT sieht man **32 Threads** mit eigener Identität. Die Kosten einer
datenabhängigen Verzweigung sind in beiden Fällen dieselben — nur heißt sie auf der GPU
*warp divergence*.

**Die Peak-Performance-Formel** liefert das $P_{\text{peak}}$, das in der Roofline (Kapitel 10
und 12) das waagerechte Dach bildet.

**Eigene Abgabe:** `assignment4/` — Inline-Assembly und externe Kernel auf RISC-V / K230
(Blatt 4).
