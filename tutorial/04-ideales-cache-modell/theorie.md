# Kapitel 04 — Caches und das ideale Cache-Modell

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/4-Lecture-Cache_de.pdf` (38 Folien), `vl/extra-material/Ex-4-IdealCache.pdf`,
> Übungsblatt `excercises/uebung5.pdf` (Pointer Chasing), eigene Abgabe `assignment5/`
> **Zeitbedarf:** ca. 25 min
> **Voraussetzungen:** Kapitel 03 (von-Neumann-Flaschenhals)

---

## 1. Worum es geht

Die CPU wurde über Jahrzehnte schneller, der Speicher kaum — das ist die **Memory-Processor
Gap**. Das naive Laufzeitmodell „Zeit = Anzahl Operationen" ist deshalb falsch, und zwar um
Größenordnungen. Dieses Kapitel liefert das richtige Modell und die Kennzahl, mit der man
Algorithmen daran misst.

---

## 2. Die Kernpunkte

### 2.1 Warum das Operationen-Zählen nicht reicht

Für $z \leftarrow z + x \cdot y$ mit Skalaren zählt man:

- $F$ = Anzahl CPU-Operationen, $\alpha$ = Zeit pro Operation
- $G$ = Anzahl Speicherbewegungen, $\beta$ = Zeit pro bewegtem Wort

$$T = \alpha F + \beta G$$

Das vereinfachte Modell $T = \alpha F$ ist **nur für $\alpha \gg \beta$** brauchbar. Typische
Werte sind aber

$$\alpha \sim 1\ \text{ns}, \qquad \beta \sim 50\ \text{ns} \qquad \Rightarrow \qquad \alpha \ll \beta$$

**Ein Speicherzugriff kostet 50-mal so viel wie eine Rechenoperation.** Wer Speicherbewegungen
ignoriert, rechnet am dominanten Term vorbei.

### 2.2 Die Speicherhierarchie

| Technologie | Preis/GB | Zugriffszeit | Bandbreite |
|---|---|---|---|
| SRAM (Cache) | $5000 | 0,5 ns | 25+ GB/s |
| DRAM (Hauptspeicher) | $7 | 50–150 ns | 10 GB/s |
| SSD | $0,05 | 25 000–100 000 ns | 0,5 GB/s |

`Register → L1 → L2 → L3 → Hauptspeicher → persistenter Speicher` — je näher an der CPU,
desto schneller, kleiner und teurer. Häufig benutzte Daten wandern nach oben.

- **Cache Hit:** Daten liegen im Cache → schneller Zugriff.
- **Cache Miss:** Daten fehlen → aus der nächsttieferen Ebene nachladen.
- **Hit-/Miss-Ratio:** Anteil erfolgreicher bzw. erfolgloser Zugriffe an allen Anfragen.

### 2.3 Cache-Lines

Der Cache ist in **Cache-Lines** (Blöcke) fester Größe unterteilt — üblich 32, 64 oder
128 Byte. Übertragen wird **immer eine ganze Line**, nie ein einzelnes Byte.

Beispiel: 32 KB Cache mit 64-Byte-Lines hat $32768/64 = 512$ Cache-Lines.

Der Grund ist **räumliche Lokalität**: Wer Adresse $X$ anfordert, braucht mit hoher
Wahrscheinlichkeit bald auch $X+8$ — und die ist dann schon da. Umgekehrt ist genau das der
Grund, warum ein Zugriff mit großer Schrittweite katastrophal ist: Man bezahlt 64 Byte und
benutzt 8.

$$\text{Block-ID} = \left\lfloor \frac{\text{Adresse}}{\text{Line-Größe}} \right\rfloor$$

### 2.4 Zuordnungsstrategien

| Strategie | Wo darf ein Block liegen? | Vorteil | Nachteil |
|---|---|---|---|
| **Voll-assoziativ** | überall im Cache | maximale Flexibilität, keine Konflikte | teure Suche, viel Tag-Speicher |
| **Direkt** | an **genau einer** Stelle (Index = niederwertige Bits der Block-ID) | einfachste Hardware | **Konflikt-Misses**: Blöcke konkurrieren um dieselbe Zeile |
| **$L$-fach set-assoziativ** | in **einer bestimmten Gruppe**, dort beliebig | Kompromiss | — |

$L$-fach set-assoziativ ist der Normalfall. Sonderfälle: $L = 1$ ist direkte Zuordnung,
$L = \#\text{Lines}$ ist voll-assoziativ.

### 2.5 Adressaufteilung — die Rechnung für die Klausur

$$\text{Adresse} = [\ \text{Tag}\ |\ \text{Index}\ |\ \text{Offset}\ ]$$

| Größe | Formel |
|---|---|
| Bits der physischen Adresse | $\log_2(\text{Speichergröße in Byte})$ |
| **Offset**-Bits | $\log_2(\text{Line-Größe in Byte})$ |
| Anzahl Cache-Lines | Cache-Größe / Line-Größe |
| Anzahl Gruppen (Sets) | Anzahl Cache-Lines / $L$ |
| **Index**-Bits | $\log_2(\text{Anzahl Gruppen})$ — bei voll-assoziativ: **0** |
| **Tag**-Bits | Adressbits − Index-Bits − Offset-Bits |
| Anzahl Speicherblöcke | Speichergröße / Line-Größe |

**Beispiel 1 (voll-assoziativ, aus der Vorlesung).** 8 KB Cache, 128-Byte-Lines, 64 KB
Hauptspeicher:

| | Rechnung | Ergebnis |
|---|---|---|
| Adressbits | $\log_2 65\,536$ | 16 |
| Offset | $\log_2 128$ | 7 |
| Tag | $16 - 7$ | 9 |
| Cache-Lines | $8192/128$ | 64 |
| Speicherblöcke | $65\,536/128$ | 512 |

**Beispiel 2 (set-assoziativ).** Cortex-A72 L1-Datencache: 32 KB, 2-fach set-assoziativ,
64-Byte-Lines, 64-Bit-Adressen:

| | Rechnung | Ergebnis |
|---|---|---|
| Cache-Lines | $32\,768/64$ | 512 |
| Gruppen | $512/2$ | 256 |
| Offset | $\log_2 64$ | **6** |
| Index | $\log_2 256$ | **8** |
| Tag | $64 - 8 - 6$ | **50** |

> **Achtung:** Die Vorlesungsfolie gibt für dieses Beispiel „Tag 51 / Index 8 / Offset 5" an.
> Das passt nicht zur ebenfalls dort angegebenen Line-Größe von 64 Byte — 5 Offset-Bits
> entsprächen 32 Byte. Die Rechnung oben ist die konsistente; geprüft wird ohnehin das
> **Verfahren**, nicht die Zahl.

### 2.6 Austauschstrategien

| Strategie | Regel | Preis |
|---|---|---|
| **Random** | zufällige Line ersetzen | trivial, aber ineffizient |
| **FIFO** | die am längsten im Cache liegende | Zähler für den Ladezeitpunkt |
| **LRU** (*least recently used*) | die am längsten nicht benutzte | Nutzungshistorie mitführen — teuer |
| **LFU** (*least frequently used*) | die am seltensten benutzte | Zugriffszähler |

Zu unterscheiden ist: **FIFO** zählt, *wann geladen* wurde; **LRU** zählt, *wann zuletzt
benutzt* wurde; **LFU** zählt, *wie oft* benutzt wurde. Genau diese Unterscheidung wird gern
an einer Zugriffsfolge abgefragt.

### 2.7 Das ideale Cache-Modell

Ein **Modell**, kein realer Cache — es macht Algorithmenanalyse möglich.

**Annahmen:**

- zweistufige Hierarchie: Cache + beliebig großer Hauptspeicher
- die CPU verarbeitet **nur Daten im Cache**
- **Cache-Größe $M$** (in Wörtern), **Line-Größe $B$** (Wörter pro Line)
- **Tall-Cache-Annahme:** $M = \Omega(B^2)$ — es gibt viel mehr Lines als Wörter pro Line
- Übertragung erfolgt **nur in ganzen Lines**
- **idealer Austausch:** voll-assoziativ und *optimal* — ersetzt wird, was am längsten nicht
  gebraucht wird (Belady; in der Realität nicht implementierbar, aber als Schranke nützlich)

**Zwei Bewertungskriterien** für einen Algorithmus mit Problemgröße $n$:

| Kennzahl | Bedeutung |
|---|---|
| **Work** $W(n)$ | Anzahl der Operationen — das klassische Maß |
| **Cache-Komplexität** $Q(n; M, B)$ | **Anzahl der Cache-Misses** |

Diese zweite Größe ist das eigentliche Werkzeug dieses und des nächsten Kapitels: Zwei
Algorithmen mit identischem $W(n)$ können sich in $Q(n;M,B)$ um Größenordnungen unterscheiden
— und genau das entscheidet über die Laufzeit.

### 2.8 Lokalität und Zugriffsmuster

```cpp
for (int i = 0; i < N; ++i)      // Stride 1
    s = s + a[i];
```

- `s` hat hohe **temporale Lokalität** (immer wieder dieselbe Stelle)
- `a` hat hohe **räumliche Lokalität** (benachbarte Adressen)

Misses: $Q \approx N/B$ — nur jeder $B$-te Zugriff ist ein Miss.

**Stride-$k$-Zugriff** (`a[k*i]`): Ab $k \ge B$ ist **jeder** Zugriff ein Miss,
$Q \approx N$ — Faktor $B$ schlechter, bei gleicher Operationszahl.

**Zufälliger Zugriff** (`a[Index(i)]`): weder räumliche noch zeitliche Lokalität, $Q \approx N$.

**Pointer Chasing** (Blatt 5, `assignment5/`) ist der Extremfall: Die nächste Adresse steht
im gerade geladenen Datum, also lässt sich der Zugriff nicht vorhersagen und nicht
vorausladen. Jeder Schritt kostet die volle Latenz. Das ist die übliche Methode, die
**Latenz** der einzelnen Cache-Stufen zu vermessen: Man legt eine zyklische Zeigerkette an,
variiert die Arbeitsmenge und liest an den Sprüngen der Zeitkurve die Cache-Größen ab.

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Warum ist $T = \alpha F$ kein gutes Modell? | $\beta \approx 50\alpha$ — Speicherbewegungen dominieren; richtig ist $T = \alpha F + \beta G$ |
| Was ist eine Cache-Line und warum gibt es sie? | Übertragungseinheit fester Größe (32/64/128 B); nutzt räumliche Lokalität aus |
| Drei Zuordnungsstrategien | voll-assoziativ, direkt, $L$-fach set-assoziativ |
| Wie viele Offset-Bits? | $\log_2(\text{Line-Größe in Byte})$ |
| Wie viele Index-Bits? | $\log_2(\text{Anzahl Gruppen})$, Gruppen = Lines/$L$; voll-assoziativ: 0 |
| Wie viele Tag-Bits? | Adressbits − Index − Offset |
| Unterschied FIFO / LRU / LFU | Ladezeitpunkt / letzter Zugriff / Zugriffshäufigkeit |
| Annahmen des idealen Cache-Modells | 2 Ebenen, nur Cache rechenbar, Größe $M$, Lines $B$, $M = \Omega(B^2)$, optimaler Austausch |
| Was ist die Tall-Cache-Annahme? | $M = \Omega(B^2)$ — deutlich mehr Lines als Wörter pro Line |
| Was misst $Q(n;M,B)$? | die Anzahl der Cache-Misses |
| $Q$ bei Stride 1 und bei Stride $\ge B$? | $N/B$ bzw. $N$ |
| Warum ist Pointer Chasing der schlechteste Fall? | die nächste Adresse ist erst nach dem Laden bekannt — kein Prefetching, volle Latenz pro Schritt |

---

## 4. Merkkasten

> - Ein Speicherzugriff kostet **~50 Operationen**. Deshalb ist $Q(n;M,B)$ neben $W(n)$ die
>   zweite, oft entscheidende Kennzahl.
> - **Adresse = Tag | Index | Offset.** Offset aus der Line-Größe, Index aus der Gruppenzahl,
>   Tag ist der Rest.
> - **Ideales Cache-Modell:** $M$ Wörter, $B$ pro Line, $M = \Omega(B^2)$, optimaler Austausch.
> - **Stride 1 → $Q \approx N/B$; Stride $\ge B$ oder zufällig → $Q \approx N$.** Gleiche
>   Operationszahl, Faktor $B$ Unterschied.
> - Räumliche Lokalität nutzt die Line aus, zeitliche Lokalität den Cache-Inhalt.

---

## 5. Verbindung

**Baut darauf auf:** Kapitel 05 wendet $Q(n;M,B)$ auf das Matrixprodukt an und zeigt, dass
**Blocking** die Cache-Komplexität um den Faktor $B\sqrt{M}$ senkt. Kapitel 12 ist die
GPU-Fassung derselben Idee — nur heißt der Cache dort Shared Memory und man befüllt ihn
selbst.

**Querverbindung:** *False sharing* (Kapitel 06) ist ein Cache-Line-Effekt: Zwei Threads
schreiben in dieselbe Line, ohne dieselben Daten anzufassen. *Coalescing* (Kapitel 12) ist
dasselbe Prinzip auf der GPU — nur über die Threads eines Warps statt über die Zeit.

**Eigene Abgabe:** `assignment5/` — Pointer Chasing (Blatt 5).
