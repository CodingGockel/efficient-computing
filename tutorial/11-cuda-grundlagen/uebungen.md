# Kapitel 11 — Übungen: CUDA I

Erst selbst bearbeiten, dann mit [`loesungen.md`](loesungen.md) vergleichen.

**Die Aufgaben 11.1, 11.2, 11.3, 11.5a–c, 11.6, 11.7 und 11.9 sind reine Papieraufgaben** —
sie brauchen keine GPU und sind der klausurrelevante Kern. Für die Code-Aufgaben liegen
Gerüst, Makefile und Job-Skript in [`code/`](code/); sie werden auf dem Ara-Cluster übersetzt
und gestartet:

```bash
scp -r code/ <kuerzel>@login1.ara.uni-jena.de:~/kap11/
ssh <kuerzel>@login1.ara.uni-jena.de
cd kap11 && module load nvidia/cuda/12.4 && make
sbatch job.sbatch && squeue -u $USER
```

Tags: `[Rechnen]` `[Code]` `[Analyse]` `[Klausur]`

---

## Aufgabe 11.1 `[Klausur]` — Indexrechnung, hin und zurück

*Zeit: ca. 10 min*

Ein Kernel wird mit `kernel<<<32, 128>>>(d_a, d_b, n)` für `n = 4000` gestartet.

a) Welchen globalen Index `i` bearbeitet der Thread mit `threadIdx.x = 37` in Block
   `blockIdx.x = 11`? Gib den Rechenweg an.
b) Umgekehrt: Welcher Thread bearbeitet das Element `i = 3000`? Nenne `blockIdx.x` und
   `threadIdx.x` und begründe, wie man beide aus `i` gewinnt.
c) Wie viele Threads werden insgesamt gestartet, wie viele davon haben kein Element zu
   bearbeiten?
d) In welchem Warp seines Blocks liegt der Thread aus a), und auf welcher Lane innerhalb
   dieses Warps?
e) In welchem Block und in welchen Warps dieses Blocks stehen die Threads ohne Arbeit? Führt
   der `if (i < n)`-Wächter hier zu Warp-Divergenz? Begründe.

---

## Aufgabe 11.2 `[Rechnen]` — Grid dimensionieren

*Zeit: ca. 15 min*

Ein Vektor der Länge `n = 1 000 000` soll elementweise verarbeitet werden.

a) Bestimme für `blockDim.x = 256` die Blockzahl mit der Aufrundungsformel. Wie viele Threads
   laufen insgesamt, wie viele davon leer?
b) Wie viele Elemente bearbeitet der **letzte** Block? Wie viele Warps dieses Blocks sind
   vollständig aktiv, wie viele vollständig leer, wie viele teilweise aktiv? Was folgt daraus
   für die Divergenzkosten des Wächters?
c) Jemand schlägt `blockDim.x = 250` vor („dann passt es besser"). Bestimme die
   Blockzahl. Wie viele Warps belegt ein solcher Block, und wie viele Lanes sind in **jedem**
   Block dauerhaft ungenutzt? Vergleiche die Gesamtzahl belegter Lanes mit der aus a).
d) Wie viele Blöcke braucht man bei `blockDim.x = 1024`? Ist diese Konfiguration zulässig?
e) Warum scheitert `kernel<<<1, 1000000>>>(…)`, und mit welcher Fehlermeldung würde man das
   bemerken — vorausgesetzt, man prüft überhaupt?
f) Formuliere in einem Satz, warum man `blockDim.x` als Vielfaches von 32 wählt, obwohl jede
   andere Zahl ≤ 1024 auch funktioniert.

---

## Aufgabe 11.3 `[Analyse]` — Fehlersuche

*Zeit: ca. 25 min*

Das folgende Programm soll `y[i] = x[i] * x[i]` für `n = 100 000` berechnen. Es übersetzt
fehlerfrei, meldet keinen Fehler — und liefert trotzdem nicht das Erwartete.

```cpp
#include <cstdio>
#include <vector>

__global__ void quadrat(float* y, const float* x, int n) {
    int i = threadIdx.x;
    y[i] = x[i] * x[i];
}

int main() {
    const int n = 100000;
    std::vector<float> h_x(n), h_y(n);
    for (int i = 0; i < n; ++i) h_x[i] = static_cast<float>(i);

    float *d_x, *d_y;
    cudaMalloc(&d_x, n);
    cudaMalloc(&d_y, n * sizeof(float));

    cudaMemcpy(d_x, h_x.data(), n * sizeof(float), cudaMemcpyDeviceToHost);

    int threads = 256;
    int blocks  = n / threads;
    quadrat<<<threads, blocks>>>(d_y, h_x.data(), n);

    cudaMemcpy(h_y.data(), d_y, n * sizeof(float), cudaMemcpyDeviceToHost);
    std::printf("y[7] = %f\n", h_y[7]);

    return 0;
}
```

a) Finde **alle** Fehler. Für jeden: die Zeile, die Art des Fehlers und was er konkret
   bewirkt (falsches Ergebnis? Absturz? stilles Nichtstun?). Es sind mehr als fünf.
b) Welche dieser Fehler würde ein `CUDA_CHECK`-Makro um jeden Aufruf sofort sichtbar machen,
   welche nicht? Begründe für jeden Fehler einzeln.
c) Schreibe das Programm korrekt auf.
d) Ein Kommilitone repariert nur den Kernel-Index und sagt: „Bei mir kommt jetzt
   `y[7] = 49` heraus, also stimmt es." Erkläre in zwei Sätzen, warum diese Schlussfolgerung
   nicht trägt, und nenne eine Prüfung, die die verbleibenden Fehler zuverlässig aufdeckt.

---

## Aufgabe 11.4 `[Code]` — axpy auf der GPU

*Zeit: ca. 40 min*

Aus Kapitel 01 bekannt: `y ← αx + y` mit `α ∈ ℝ` und `x, y ∈ ℝⁿ`. Auf der CPU wurde das in
`assignment2/` schon vermessen — jetzt auf der GPU.

a) Schreibe den Kernel `axpy`. Ein Thread bearbeitet **ein** Element. Verwende `float`.
b) Wähle eine 1D-Konfiguration, die auch für `n` funktioniert, das kein Vielfaches der
   Blockgröße ist. `n` soll als Kommandozeilenargument übergeben werden.
c) Schreibe das Host-Programm nach den fünf Schritten. Prüfe **jeden** CUDA-Aufruf mit einem
   `CUDA_CHECK`-Makro und den Kernel-Start zusätzlich mit `cudaGetLastError()` **und** einem
   synchronisierenden Aufruf.
d) Verifiziere das Ergebnis auf dem Host gegen eine serielle Referenz. Prüfe dabei nicht nur
   ein Element — begründe kurz, welche Positionen du prüfst und warum.
e) Miss mit `cudaEvent_t` **drei** Zeiten getrennt: Host→Device, Kernel, Device→Host. Lass
   `n` über mehrere Größenordnungen laufen (z. B. 2¹⁰ bis 2²⁶) und stelle die drei Anteile
   als Tabelle dar.
f) Werte aus: Welcher Anteil dominiert? Ab welchem `n` ist die Kernel-Zeit überhaupt messbar
   größer als der Messfehler? Berechne aus deiner Kernel-Zeit die erreichte
   **Speicherbandbreite** in GB/s und vergleiche sie mit dem Datenblattwert der Karte, die du
   bekommen hast (`nvidia-smi` im Job-Skript verrät sie).
g) Vergleiche mit der CPU-Messung aus `assignment2/`. Ist die GPU schneller — mit und ohne
   Transfer gerechnet? Erkläre das Ergebnis mit dem Argument aus Abschnitt 9 der Theorie.

---

## Aufgabe 11.5 `[Rechnen]` `[Code]` — Zweidimensionale Indexierung

*Zeit: ca. 30 min*

Ein Bild der Größe `H × W = 1080 × 1920` (Row-Major, `float`, ein Kanal) soll
spalten- und zeilenweise korrigiert werden:

$$B[r][c] = A[r][c] \cdot w[c] + b[r]$$

mit einem Spaltengewicht `w ∈ ℝ^W` und einem Zeilenoffset `b ∈ ℝ^H`.

a) Es wird mit `dim3 threads(16,16)` gestartet. Bestimme `blocks.x` und `blocks.y`. Wie viele
   Threads laufen insgesamt, wie viele haben kein Pixel?
b) Welcher Block und welcher Thread darin bearbeiten das Pixel `(r, c) = (500, 1000)`? Welchen
   linearen Index hat dieses Pixel im Speicher?
c) Umgekehrt: Welches Pixel `(r, c)` liegt am linearen Index 1 000 000?
d) Schreibe den Kernel. Achte auf den zweidimensionalen Wächter und darauf, welche der beiden
   Dimensionen die Spalte ist.
e) Eine Variante vertauscht die Rollen: `int r = blockIdx.x * blockDim.x + threadIdx.x;` und
   `int c = blockIdx.y * blockDim.y + threadIdx.y;` — bei entsprechend vertauschtem
   `blocks`-Aufbau. Ist das Ergebnis korrekt? Ist es gleich schnell? Begründe über die
   Adressen, die die 32 Lanes eines Warps in beiden Fassungen anfassen.
f) Führe beide Fassungen aus und vergleiche die Zeiten. Passt das Verhältnis zu deiner
   Vorhersage aus e)?
g) Prüfe das Ergebnis an mindestens drei Positionen. Welche wählst du, und warum reicht eine
   nicht?

---

## Aufgabe 11.6 `[Analyse]` — Warp-Divergenz erkennen

*Zeit: ca. 15 min*

Ein Kernel läuft mit `blockDim.x = 256`. Sei `i = blockIdx.x * blockDim.x + threadIdx.x`.
Gib für jedes Fragment an, **wie viele Durchläufe** die Hardware pro Warp braucht (also den
Faktor gegenüber dem divergenzfreien Fall), und begründe.

```cpp
// (a)
if (i % 2 == 0) A(); else B();

// (b)
if (threadIdx.x < 64) A(); else B();

// (c)
if ((threadIdx.x / 32) % 2 == 0) A(); else B();

// (d)
for (int k = 0; k < threadIdx.x; ++k) A();

// (e)
switch (threadIdx.x % 4) { case 0: A(); break; case 1: B(); break;
                           case 2: C(); break; default: D(); }

// (f)
if (i < n) A();          // der uebliche Waechter, n = 100000, Grid genau aufgerundet
```

g) Formuliere `A()`/`B()` aus (a) so um, dass keine Divergenz mehr entsteht, ohne die
   Semantik zu ändern — also so, dass weiterhin die geraden Indizes `A` und die ungeraden `B`
   erhalten. Was hast du dafür bezahlt?
h) Erkläre, warum in (d) der Warp so lange braucht wie sein *langsamster* Thread, und nenne
   das allgemeine Prinzip dahinter in einem Satz.

---

## Aufgabe 11.7 `[Rechnen]` — Lohnt sich die GPU?

*Zeit: ca. 20 min*

Ein Programm braucht auf der CPU 100 s. Davon sind **5 %** unvermeidlich seriell (Einlesen,
Ausgabe, Steuerlogik).

a) Der ausgelagerte Teil läuft auf der GPU 60-mal schneller. Welchen Gesamtspeedup erreicht
   das Programm? Wie groß wäre der Speedup bei einer unendlich schnellen GPU?
b) Der Datentransfer über PCIe kostet zusätzlich 20 s, die es in der CPU-Fassung nicht gab.
   Wie groß ist der Speedup jetzt? Um welchen Faktor hat der Transfer den Gewinn aus a)
   reduziert?
c) Welchen Kernel-Speedup bräuchte man **mit** diesem Transfer, um insgesamt Faktor 10 zu
   erreichen? Interpretiere das Ergebnis.

Jetzt der allgemeine Fall. Ein Kernel verarbeitet `n` `float`-Elemente, liest sie einmal und
schreibt einmal zurück (also 8n Byte über PCIe) und rechnet dabei `q` Flop **pro Element**.
Rechne mit:

| Größe | Wert |
|---|---|
| PCIe-Bandbreite | 12 GB/s |
| GPU-Rechenleistung (FP32) | 15 TFLOP/s |
| CPU-Rechenleistung | 100 GFLOP/s |

d) Stelle die Gesamtzeit der GPU-Fassung (Transfer + Rechnung) und der CPU-Fassung als
   Funktion von `n` und `q` auf.
e) Ab welchem `q` ist die GPU-Fassung schneller? Rechne den Zahlenwert aus.
f) Wie groß ist `q` bei `y ← αx + y`? Ordne das Ergebnis in e) ein.
g) Nenne zwei konkrete Maßnahmen, mit denen man die Schwelle aus e) in der Praxis unterläuft,
   ohne den Kernel selbst zu ändern.

---

## Aufgabe 11.8 `[Code]` `[Analyse]` — Grid-Stride-Loop

*Zeit: ca. 25 min*

a) Schreibe den `axpy`-Kernel aus 11.4 als Grid-Stride-Loop.
b) Starte ihn mit vier Konfigurationen bei `n = 10⁷`: `<<<1,1>>>`, `<<<1,256>>>`,
   `<<<432,256>>>` und `<<<(n+255)/256, 256>>>`. Welche liefern ein **korrektes** Ergebnis?
   Miss die Zeiten und erkläre die Abstufung.
c) Wie viele Elemente bearbeitet ein Thread bei der Konfiguration `<<<4096, 256>>>` und
   `n = 10⁷`? Bearbeiten alle Threads gleich viele? Rechne beides aus.
d) Eine naheliegende Alternative lautet: „Thread `t` bekommt den zusammenhängenden Block von
   `k = ⌈n/T⌉` Elementen ab Position `t·k`" (`T` = Gesamtzahl Threads). Das ist ebenfalls
   korrekt. Warum ist es auf der GPU trotzdem die schlechtere Aufteilung? Argumentiere über
   die Adressen, die die 32 Lanes eines Warps in einem Schritt anfassen.
e) Auf der CPU ist genau diese Blockaufteilung die **bessere** Wahl (`schedule(static)` in
   OpenMP). Erkläre den Widerspruch.
f) Nenne einen praktischen Vorteil des Grid-Stride-Loops beim **Debuggen**.

---

## Aufgabe 11.9 `[Klausur]` — Kurzfragen

*Zeit: ca. 15 min*

Beantworte in je ein bis drei Sätzen. Bei Richtig/Falsch-Aussagen ist die Begründung der
eigentliche Teil der Antwort.

a) Erkläre den Unterschied zwischen SIMD und SIMT. Warum sagt man, die GPU sei „SIMD-Hardware
   mit SIMT-Programmiermodell"?
b) Richtig oder falsch: „Zwei Blöcke desselben Grids können sich über Shared Memory
   verständigen."
c) Richtig oder falsch: „`cudaMemcpy` muss nach einem Kernel-Start nicht extra synchronisiert
   werden."
d) Richtig oder falsch: „Ein Kernel mit `__global__` darf einen `float` zurückgeben, wenn nur
   ein Thread schreibt."
e) Warum ist eine Zeitmessung mit `std::chrono` direkt um den Kernel-Aufruf herum irreführend,
   und woran erkennt man das an den Messwerten?
f) Nenne die fünf Schritte des Host-Ablaufs in der richtigen Reihenfolge.
g) Was bewirkt `-arch=sm_80`, und was passiert, wenn man damit erzeugten Code auf einer V100
   startet?
h) Warum gibt es in CUDA keine Barriere über alle Threads eines Grids? Nenne den
   architektonischen Grund, nicht nur die Tatsache.
i) Die GPU hat 6912 Recheneinheiten, die CPU 8 Kerne. Warum ist der Speedup trotzdem fast nie
   864?
j) In einem Job-Skript fehlt `#SBATCH --gres=gpu:1`. Der Code ist fehlerfrei. Was passiert?

---

**Lösungen:** [`loesungen.md`](loesungen.md) — erst danach aufschlagen.
