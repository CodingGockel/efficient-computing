# Kapitel 07 — Lösungen: OpenMP Tasks, Abhängigkeiten und kritischer Pfad

> Erst [`uebungen.md`](uebungen.md) selbst bearbeiten.

---

## Lösung 7.1 — Task-Scoping vorhersagen

**a) Datenattribute**

| Variable | in `parallel` | in `task` | Begründung |
|---|---|---|---|
| `s` | shared | **shared** | `static` → shared; nur `shared` wird an die Task vererbt |
| `g` | shared | **shared** | global → shared; ebenso vererbt |
| `p` | shared | **shared** | außerhalb deklariert, nicht in einer Klausel → shared; vererbt |
| `q` | firstprivate | **firstprivate** | `firstprivate` ist eine Form von privat → wird **nicht** vererbt, die Task macht ihre eigene Kopie |
| `r` | private | **firstprivate** | im Block deklariert → private je Thread; die Task kopiert den Wert beim Erzeugen |
| `t` | — | **private** | in der Task selbst deklariert |

**b)** Änderungen des Erzeugers sieht die Task nur bei `s`, `g` und `p` — den drei **shared**
Variablen. Bei `q` und `r` wurde beim Erzeugen eine **Momentaufnahme** kopiert; was der
Erzeuger danach tut, ist für die Task unsichtbar.

Genau das ist der Sinn der Regel: Zwischen Erzeugung und Ausführung können beliebig viele
Anweisungen des Erzeugers liegen. Ohne die Kopie wäre praktisch jede Task in einer Schleife
kaputt.

**c)** Der Unterschied ist Regel 3 aus Abschnitt 5.1:

> Alles, was eine Task von außen benutzt, ist **firstprivate** — außer es war im umgebenden
> Kontext bereits `shared`.

Bei Threads gilt dagegen „außen deklariert → shared". Der Grund für den Unterschied ist die
**zeitliche Entkopplung**: Ein Thread führt seinen Block *sofort* aus, eine Task *irgendwann*.
Die Kopie schützt davor, dass sich der Wert zwischenzeitlich ändert oder der Stackframe des
Erzeugers gar nicht mehr existiert.

**d)** Mit `shared(r)` greift die Task auf die Originalvariable des erzeugenden **Threads** zu.
Das ist ein Fehler, sobald der Erzeuger `r` nach der Task-Erzeugung noch ändert oder den
Gültigkeitsbereich verlässt — dann liest die Task einen falschen Wert oder greift auf einen
zerstörten Stackframe zu. Klassischer Fall:

```cpp
for (int i = 0; i < n; i++) {
    int r = f(i);
    #pragma omp task shared(r)      /* FEHLER: r ist beim Ausführen längst überschrieben */
    use(r);
}
```

`shared` ist nur dann richtig, wenn man das Ergebnis der Task **zurückbekommen** will — und
dann muss ein `taskwait` folgen, bevor der Gültigkeitsbereich endet (wie bei Fibonacci).

---

## Lösung 7.2 — Warum `single`?

**a)** `arbeite()` wird **32-mal** aufgerufen. Der `parallel`-Block wird von jedem der 4 Threads
vollständig ausgeführt — jeder durchläuft die Schleife und erzeugt seine eigenen 8 Tasks. Also
`4 × 8 = 32` Tasks statt 8. Das Ergebnis ist nicht nur langsam, sondern bei schreibenden Tasks
schlicht falsch.

**b)**

```cpp
#pragma omp parallel num_threads(4)
#pragma omp single
for (int i = 0; i < 8; i++) {
    #pragma omp task
    arbeite(i);
}
```

Jetzt entstehen **8** Tasks. Die anderen drei Threads laufen sofort zur impliziten Barriere am
Ende des `single` — und dort ist ein *task scheduling point*: Statt zu warten, holen sie sich
Tasks aus der Queue und arbeiten sie ab. Der erzeugende Thread hilft mit, sobald er fertig ist
mit Erzeugen.

**c)** `#pragma omp for` statt `single` ist **korrekt**: Jede Iteration wird genau einmal
ausgeführt, also entstehen genau 8 Tasks.

| | Vorteil | Nachteil |
|---|---|---|
| `for` | Die Erzeugung selbst ist parallelisiert — relevant, wenn es sehr viele Tasks sind oder das Erzeugen teuer ist | Nur bei einer zählbaren Schleife anwendbar; bei Rekursion oder Listen unbrauchbar |
| `single` | Funktioniert für jede Erzeugungsstruktur, auch rekursiv | Ein Thread erzeugt alles — bei Millionen Tasks kann das der Engpass werden |

Da Tasks gerade für die Fälle gedacht sind, in denen `for` nicht geht, ist `single` das
allgemeine Idiom.

**d)** `nowait` entfernt die **implizite Barriere am Ende des `single`**. Ohne `nowait` müsste
der erzeugende Thread dort warten, bis alle anderen ankommen — mit `nowait` darf er sofort
mithelfen, Tasks abzuarbeiten.

Korrekt bleibt es, weil die Barriere am Ende der `parallel`-Region ohnehin garantiert, dass
**alle** Tasks abgeschlossen sind, bevor es seriell weitergeht. Man verliert also keine
Garantie, sondern nur eine überflüssige Wartezeit.

**e)** Das ist **kein Übersetzungsfehler**. Ohne umgebende `parallel`-Region gibt es ein
implizites Team der Größe 1: Die Tasks werden erzeugt und irgendwann vom einzigen Thread
ausgeführt — das Programm ist korrekt, aber vollständig seriell (plus Task-Overhead). Ein
häufiger Grund für „meine Tasks bringen nichts".

---

## Lösung 7.3 — Kritischer Pfad

**a) Graph**

```
                 ┌──────► B(5) ──────┐
                 │                   ▼
        A(2) ────┼──────► C(3) ────► E(2) ────► G(3) ────┐
                 │          │                            ▼
                 └──────► D(4) ──┴──► F(6) ─────────────► H(2)
```

Kanten: A→B, A→C, A→D, B→E, C→E, C→F, D→F, E→G, F→H, G→H.

**b) Vorwärts- und Rückwärtslauf**

Vorwärts (`ES = max EF der Vorgänger`, `EF = ES + w`):

| Task | w | Vorgänger | ES | EF |
|---|---|---|---|---|
| A | 2 | — | 0 | 2 |
| B | 5 | A | 2 | 7 |
| C | 3 | A | 2 | 5 |
| D | 4 | A | 2 | 6 |
| E | 2 | B, C | max(7, 5) = 7 | 9 |
| F | 6 | C, D | max(5, 6) = 6 | 12 |
| G | 3 | E | 9 | 12 |
| H | 2 | F, G | max(12, 12) = 12 | **14** |

`T∞ = 14`.

Rückwärts (`LF = min LS der Nachfolger`, `LS = LF − w`), Start bei `LF(H) = 14`:

| Task | Nachfolger | LF | LS | Slack |
|---|---|---|---|---|
| H | — | 14 | 12 | **0** |
| G | H | 12 | 9 | **0** |
| F | H | 12 | 6 | **0** |
| E | G | 9 | 7 | **0** |
| D | F | 6 | 2 | **0** |
| C | E, F | min(7, 6) = 6 | 3 | **1** |
| B | E | 7 | 2 | **0** |
| A | B, C, D | min(2, 3, 2) = 2 | 0 | **0** |

Gesamttabelle:

| Task | w | ES | EF | LS | LF | Slack |
|---|---|---|---|---|---|---|
| A | 2 | 0 | 2 | 0 | 2 | 0 |
| B | 5 | 2 | 7 | 2 | 7 | 0 |
| C | 3 | 2 | 5 | 3 | 6 | **1** |
| D | 4 | 2 | 6 | 2 | 6 | 0 |
| E | 2 | 7 | 9 | 7 | 9 | 0 |
| F | 6 | 6 | 12 | 6 | 12 | 0 |
| G | 3 | 9 | 12 | 9 | 12 | 0 |
| H | 2 | 12 | 14 | 12 | 14 | 0 |

**c) Kritische Pfade** — alle Tasks außer C haben Slack 0. Es gibt **zwei** kritische Pfade:

$$ A \to B \to E \to G \to H = 2+5+2+3+2 = 14 $$
$$ A \to D \to F \to H = 2+4+6+2 = 14 $$

Nur `C` hat Puffer: Der Pfad `A → C → F → H = 2+3+6+2 = 13` ist eine Zeiteinheit kürzer,
und `A → C → E → G → H = 12` ist noch unkritischer. C darf also eine Zeiteinheit später
starten, ohne dass sich etwas verzögert.

> Dass es mehr als einen kritischen Pfad gibt, ist der Normalfall und keine Ausnahme. Wer
> einen Pfad optimiert, gewinnt dann **gar nichts** — der andere begrenzt weiterhin.

**d)**

$$ T_1 = 2+5+3+4+2+6+3+2 = \mathbf{27}, \qquad T_\infty = \mathbf{14}, \qquad
\frac{T_1}{T_\infty} = \frac{27}{14} \approx \mathbf{1{,}93} $$

**e) Minimale Prozessorzahl für `T∞`** — Tasks bei ihrem `ES` eintragen:

```
Zeit:  0    2    4    6    8   10   12   14
A      ████
B           ██████████
C           ██████
D           ████████
E                        ████
F                   ████████████
G                             ██████
H                                   ████
            ↑↑↑
       hier laufen B, C, D gleichzeitig → 3 Prozessoren
```

Das Maximum gleichzeitig laufender Tasks ist **3** (im Intervall [2, 5): B, C, D). Mit
`p = 3` ist `T∞ = 14` tatsächlich erreichbar:

| Prozessor | Zeitplan |
|---|---|
| P1 | A (0–2), B (2–7), E (7–9), G (9–12), H (12–14) |
| P2 | — (0–2), C (2–5), — (5–6), F (6–12) |
| P3 | — (0–2), D (2–6) |

Probe: F startet bei 6, weil C (5) und D (6) fertig sein müssen ✓. H startet bei 12, weil F
und G beide dort fertig sind ✓.

Dass 3 hier wirklich das **Minimum** ist (und nicht nur das Maximum im ES-Diagramm), zeigt
Teilaufgabe g): Mit 2 Prozessoren ist `T∞` nachweislich nicht erreichbar. Im Allgemeinen ist
das ES-Maximum nur eine obere Schranke — Tasks mit Slack lassen sich in Lücken verschieben
(vgl. Aufgabe 7.4d).

**f)**

$$ S(3) = \frac{T_1}{T(3)} = \frac{27}{14} = 1{,}93, \qquad E(3) = \frac{1{,}93}{3} = 0{,}64 $$

Trotz drei Prozessoren nur Faktor 1,93 — der kritische Pfad lässt nicht mehr zu. Ein vierter
Prozessor bringt **exakt nichts**, weil zu keinem Zeitpunkt vier Tasks bereit sind.

**g) `p = 2`**

$$ \max\left(T_\infty, \frac{T_1}{p}\right) = \max\left(14,\ \frac{27}{2}\right) = \max(14,\ 13{,}5) = \mathbf{14} $$

Diese Schranke ist **nicht erreichbar**. Zwei Argumente:

*Leerlaufargument.* A hat keine Vorgänger und alle anderen Tasks hängen von A ab — während der
2 Zeiteinheiten von A ist der zweite Prozessor zwingend untätig. Dasselbe gilt für H: Alles
hängt von H ab, nichts läuft parallel dazu (2 weitere Leerlaufeinheiten). Also

$$ 2 \cdot T(2) \ \ge \ T_1 + 4 = 31 \quad \Longrightarrow \quad T(2) \ge 15{,}5 \ \Rightarrow \ T(2) \ge 16 $$

*Aufteilungsargument.* Auch 16 geht nicht. Die Kette `B → E → G` (10 Zeiteinheiten) ist
sequentiell; liegt sie auf einem Prozessor, bleiben `C, D, F` (13 Zeiteinheiten) für den
anderen — der wäre damit erst bei `2 + 13 = 15` fertig, und H könnte frühestens bei 15 starten.
Verschiebt man eine Task auf die andere Seite, überlastet man dort oder verzögert F, weil F
erst nach C **und** D starten darf. Jede Aufteilung endet bei ≥ 17.

Bester Zeitplan mit `T(2) = 17`:

| Prozessor | Zeitplan |
|---|---|
| P1 | A (0–2), B (2–7), E (7–9), G (9–12), — (12–15), H (15–17) |
| P2 | — (0–2), C (2–5), D (5–9), F (9–15) |

Also `S(2) = 27/17 = 1{,}59`, `E(2) = 0{,}79`.

> **Die Lehre:** `max(T∞, T₁/p)` ist eine **untere Schranke**, keine erreichbare Laufzeit. Die
> optimale Zuordnung von Tasks auf Prozessoren ist im Allgemeinen NP-schwer; deshalb verwenden
> Laufzeitsysteme gierige Heuristiken, für die dann die Brent-Schranke `T₁/p + T∞` gilt
> (hier: `13,5 + 14 = 27,5`).

---

## Lösung 7.4 — Bernstein-Bedingung

**a) Input- und Output-Mengen**

| Task | Anweisung | `Iᵢ` | `Oᵢ` |
|---|---|---|---|
| S1 (2) | `t = a + b` | {a, b} | {t} |
| S2 (3) | `u = t * c` | {t, c} | {u} |
| S3 (4) | `v = d - e` | {d, e} | {v} |
| S4 (1) | `z = t / 2` | {t} | {z} |
| S5 (2) | `t = u + v` | {u, v} | {t} |

**b) Alle 10 Paare** (in Programmreihenfolge, `i < j`)

| Paar | `Iᵢ ∩ Oⱼ` | `Iⱼ ∩ Oᵢ` | `Oᵢ ∩ Oⱼ` | verletzt | Typ | Kante |
|---|---|---|---|---|---|---|
| S1, S2 | ∅ | {t} | ∅ | `Iⱼ ∩ Oᵢ` | **RAW** | S1 → S2 |
| S1, S3 | ∅ | ∅ | ∅ | — | — | keine |
| S1, S4 | ∅ | {t} | ∅ | `Iⱼ ∩ Oᵢ` | **RAW** | S1 → S4 |
| S1, S5 | ∅ | ∅ | {t} | `Oᵢ ∩ Oⱼ` | **WAW** | S1 → S5 |
| S2, S3 | ∅ | ∅ | ∅ | — | — | keine |
| S2, S4 | ∅ | ∅ | ∅ | — | — | keine |
| S2, S5 | ∅ | {u} | ∅ | `Iⱼ ∩ Oᵢ` | **RAW** | S2 → S5 |
| S3, S4 | ∅ | ∅ | ∅ | — | — | keine |
| S3, S5 | ∅ | {v} | ∅ | `Iⱼ ∩ Oᵢ` | **RAW** | S3 → S5 |
| S4, S5 | {t} | ∅ | ∅ | `Iᵢ ∩ Oⱼ` | **WAR** | S4 → S5 |

**c) Graph**

```
      S1(2)                      S3(4)
     ╱  │  ╲ (WAW)                 │
    ▼   ▼    ╲                     │
  S2(3) S4(1) ╲                    │
    │     │(WAR)╲                  │
    └─────┴──────┴──► S5(2) ◄──────┘
```

Echt parallel laufen können: **S1 und S3** (Bernstein vollständig erfüllt), sowie **S2 und S4**
(untereinander unabhängig, beide nur nach S1). S3 ist zu allem außer S5 unabhängig und kann von
Anfang an mitlaufen.

**d)** `T₁ = 2 + 3 + 4 + 1 + 2 = 12`.

| Task | ES | EF |
|---|---|---|
| S1 | 0 | 2 |
| S3 | 0 | 4 |
| S2 | 2 | 5 |
| S4 | 2 | 3 |
| S5 | max(2, 5, 4, 3) = 5 | **7** |

`T∞ = 7`, kritischer Pfad `S1 → S2 → S5 = 2 + 3 + 2 = 7`.
Parallelität `T₁/T∞ = 12/7 ≈ 1,71`.

**Minimale Prozessorzahl — hier lohnt sich genaues Hinsehen.** Trägt man alle Tasks bei ihrem
`ES` ein, laufen im Intervall [2, 3) drei Tasks gleichzeitig (S2, S3, S4) — das legt `p = 3`
nahe. Tatsächlich genügen aber **2 Prozessoren**, denn S4 hat Slack und lässt sich verschieben:

| Prozessor | Zeitplan |
|---|---|
| P1 | S1 (0–2), S2 (2–5), S5 (5–7) |
| P2 | S3 (0–4), S4 (4–5) |

S4 startet bei 4 statt bei 2 — erlaubt, weil sein einziger Vorgänger S1 schon bei 2 fertig ist
und S5 ohnehin erst bei 5 beginnt. Damit ist `T(2) = 7 = T∞`.

> **Merke:** Das Maximum im ES-Diagramm ist nur eine **obere Schranke** für die nötige
> Prozessorzahl. Tasks mit Slack darf man in Lücken schieben. In Aufgabe 7.3 war die Schranke
> scharf, hier nicht.

**e) Als OpenMP-Tasks**

```cpp
#pragma omp parallel
#pragma omp single
{
    #pragma omp task depend(out : t)                  /* S1 */
    t = a + b;

    #pragma omp task depend(in : t) depend(out : u)   /* S2 */
    u = t * c;

    #pragma omp task depend(out : v)                  /* S3 */
    v = d - e;

    #pragma omp task depend(in : t) depend(out : z)   /* S4 */
    z = t / 2;

    #pragma omp task depend(in : u, v) depend(out : t) /* S5 */
    t = u + v;
}
```

Die Laufzeit leitet daraus genau die Kanten aus b) ab — einschließlich der WAW-Kante S1 → S5
und der WAR-Kante S4 → S5, die man beim Hinschauen leicht übersieht.

**f) Umbenennung**

```
S5': t2 = u + v          /* statt t = u + v */
```

Damit gilt `O₅ = {t2}` und es verschwinden **zwei Kanten**:

- die WAW-Kante S1 → S5 (`O₁ ∩ O₅ = {t} ∩ {t2} = ∅`),
- die WAR-Kante S4 → S5 (`I₄ ∩ O₅ = {t} ∩ {t2} = ∅`).

Der kritische Pfad **ändert sich nicht**: Er bleibt `S1 → S2 → S5 = 7`, denn er besteht
ausschließlich aus echten (RAW-)Kanten.

**Was allgemein daraus folgt:** WAR und WAW sind *Namenskonflikte*, keine Datenflüsse — sie
lassen sich durch Umbenennung immer auflösen. Nach vollständiger Umbenennung (jede Variable
wird genau einmal geschrieben; das ist die *Static Single Assignment*-Form) bleiben **nur
RAW-Kanten** übrig, und der kritische Pfad ist dann rein durch den Datenfluss bestimmt. Ob das
Umbenennen etwas *bringt*, hängt davon ab, ob die unechten Kanten überhaupt kritisch waren —
hier waren sie es nicht. Genau dieselbe Technik nutzen Prozessoren als *register renaming*
(Kapitel 02) und Compiler bei der Registerallokation.

---

## Lösung 7.5 — Fibonacci und das Granularitätsproblem

**a) Die drei Varianten** — siehe [`code/fib_tasks.c`](code/fib_tasks.c)

```cpp
long fib_serial(int n) {
    if (n < 2) return n;
    return fib_serial(n - 1) + fib_serial(n - 2);
}

long fib_naiv(int n) {                        /* ohne Cutoff */
    if (n < 2) return n;
    long i, j;
    #pragma omp task shared(i)
    i = fib_naiv(n - 1);
    #pragma omp task shared(j)
    j = fib_naiv(n - 2);
    #pragma omp taskwait
    return i + j;
}

long fib_cutoff(int n, int depth) {           /* mit Cutoff */
    if (n < 2) return n;
    if (depth <= 0) return fib_serial(n);
    long i, j;
    #pragma omp task shared(i)
    i = fib_cutoff(n - 1, depth - 1);
    #pragma omp task shared(j)
    j = fib_cutoff(n - 2, depth - 1);
    #pragma omp taskwait
    return i + j;
}
```

**b) Messung** (`n = 32`, 8 Threads, i7-1165G7 — eigene Zahlen können abweichen)

| Variante | T [s] | relativ zu seriell |
|---|---|---|
| seriell | 0,0031 | 1,0 |
| Tasks ohne Cutoff | 3,25 | **1050× langsamer** |
| Tasks, Cutoff-Tiefe 4 | 0,0011 | 3,1× **schneller** |

Quantitativ: Die Zahl der Aufrufe von `fib(n)` ist `2·F(n+1) − 1`. Für `n = 32` sind das

$$ 2 \cdot F(33) - 1 = 2 \cdot 3\,524\,578 - 1 \approx 7 \cdot 10^6 \text{ Aufrufe} $$

also rund **7 Millionen Tasks**. In jeder Task steckt genau **eine Addition** (≈ 1 ns). Eine
Task kostet an Erzeugung, Einreihen, Datenkontext und Scheduling aber grob **300–500 ns**.
Gegenprobe: `7 \cdot 10^6 \cdot 450\,\text{ns} \approx 3{,}2\,\text{s}` — exakt die gemessene
Zeit. Das Verhältnis Nutzarbeit zu Overhead liegt bei etwa **1 : 1000**.

**c) Cutoff-Tiefe variieren**

Gemessen (`n = 32`, 8 Threads):

| Tiefe | 0 | 1 | 2 | 3 | **4** | 5 | 6 | 8 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| T [ms] | 3,73 | 2,56 | 1,44 | 1,28 | **1,06** | 1,07 | 1,23 | 1,28 | 1,36 | 1,99 | 3,70 |
| #Tasks | 1 | 2 | 4 | 8 | **16** | 32 | 64 | 256 | 1024 | 2048 | 4096 |

Die Kurve ist eine **Badewanne**: steiler Abfall bis Tiefe 4, ein breites flaches Optimum von
Tiefe 3 bis 10, dann wieder Anstieg. Bei Tiefe 12 ist man schon wieder so langsam wie seriell.

Beide Flanken haben verschiedene Ursachen:

- **Links (zu flach):** Bei Tiefe 0 entsteht genau eine Task — das Programm ist seriell. Bei
  Tiefe 2 gibt es 4 Tasks für 8 Threads: Die Hälfte der Threads hat nichts zu tun, und weil der
  Fibonacci-Baum unbalanciert ist (`fib(n-1)` ist deutlich teurer als `fib(n-2)`), ist die Last
  zusätzlich ungleich verteilt.
- **Rechts (zu tief):** Ab etwa Tiefe 10 gibt es mehr als 1000 Tasks, ab Tiefe 20 über eine
  Million. Der Verwaltungsaufwand wächst exponentiell mit der Tiefe, die Nutzarbeit pro Task
  schrumpft exponentiell.

**d) Faustregel**

> Wähle die Tiefe so, dass etwa **8 bis 50 Tasks pro Thread** entstehen — genug für
> Lastausgleich bei unbalancierten Bäumen, wenig genug, dass der Overhead nicht zählt.

Bei einem binären Rekursionsbaum sind das `2^d ≈ 10·p`, also

$$ d \approx \log_2(10 \cdot p) $$

Für `p = 8`: `d ≈ log₂(80) ≈ 6`. Gemessen liegt das Minimum bei 4, aber der Bereich 3–10 ist
praktisch gleich schnell — die Faustregel landet also sicher im flachen Teil der Badewanne.
Genau das ist ihr Zweck: nicht das exakte Optimum treffen, sondern die beiden steilen Flanken
zuverlässig vermeiden. Dass der Fibonacci-Baum unbalanciert ist (`fib(n-1)` kostet rund 1,6×
so viel wie `fib(n-2)`), verschiebt das Optimum zusätzlich leicht nach oben, weil man etwas
mehr Tasks für den Lastausgleich braucht.

**e) Mit `final` — und warum `final` allein nicht reicht**

```cpp
#pragma omp task final(n < k) shared(i)
i = fib(n - 1, k);
```

`final(true)` macht die Task **und ihren gesamten Nachkommenteilbaum** *undeferred*: Sie werden
zu *included tasks*, die der antreffende Thread sofort selbst abarbeitet. Einreihen, Scheduling
und Datenkontext entfallen.

`if(n >= k)` könnte das nicht: Es führt nur **diese eine** Task sofort aus; ihre rekursiven
Aufrufe erzeugen danach wieder ganz normale, verzögerte Tasks. `if` schaltet die Verzögerung
für eine Task ab, `final` für einen ganzen Teilbaum.

**Aber die Messung zeigt, dass `final` allein nicht genügt** (`fib(32)`, 8 Threads):

| Variante | T [s] | vs. seriell |
|---|---|---|
| seriell | 0,0031 | 1,0 |
| nur `final(n < 22)` | 0,0335 | **10× langsamer** |
| `final(n < 22)` + `omp_in_final()` | 0,0018 | **1,7× schneller** |

Der Grund: Die `task`-Konstrukte werden im finalen Teilbaum weiterhin **angetroffen**, und auch
eine included task kostet pro Antreffpunkt noch etwas. Bei `fib(32)` sind das rund 7 Millionen
Antreffpunkte für je eine Addition.

Die Lösung ist, im finalen Teilbaum gar nicht mehr in den Task-Code zu laufen:

```cpp
long fib(int n, int k) {
    if (n < 2) return n;
    if (omp_in_final()) return fib_serial(n);   /* ab hier keine Konstrukte mehr */
    long i, j;
    #pragma omp task shared(i) final(n < k)
    i = fib(n - 1, k);
    #pragma omp task shared(j) final(n < k)
    j = fib(n - 2, k);
    #pragma omp taskwait
    return i + j;
}
```

> **Merksatz:** `final` schaltet die *Verzögerung* ab, `omp_in_final()` die *Erzeugung*. Für
> Rekursions-Cutoffs braucht man beides — oder gleich den expliziten Tiefenzähler aus a), der
> dasselbe leistet und deutlich leichter zu lesen ist.

---

## Lösung 7.6 — Unbalancierter Baum

**a) Zwei Gründe gegen `parallel for`**

1. **Kein zählbarer Iterationsraum.** `#pragma omp for` verlangt eine kanonische Schleife,
   deren Iterationszahl beim Betreten feststeht. Die Baumstruktur ist erst zur Laufzeit
   bekannt, die Knoten sind über Zeiger verkettet, es gibt keinen Index — man kann nicht einmal
   sagen, wie viele Knoten es gibt, ohne vorher den ganzen Baum zu durchlaufen.
2. **Statische Aufteilung passt nicht zur Last.** Selbst wenn man die Teilbäume der Wurzel auf
   Threads verteilte: Der linke Teilbaum ist um Größenordnungen größer als der rechte. Jede
   statische Zerlegung führt dazu, dass ein Thread fast alles macht und die übrigen leerlaufen.

**b) Traversierung mit Tasks** — siehe [`code/tree_tasks.c`](code/tree_tasks.c)

```cpp
void traverse(node *p) {
    if (!p) return;
    #pragma omp task                /* p ist firstprivate -> eigene Kopie */
    traverse(p->left);
    #pragma omp task
    traverse(p->right);
    process(p);                     /* eigene Arbeit dieses Knotens */
}

/* Aufruf */
#pragma omp parallel
#pragma omp single
traverse(root);
```

`process(p)` braucht **keine eigene Task**: Es ist die Arbeit *dieser* Task. Würde man sie in
eine eigene Task packen, hätte der aktuelle Thread danach nichts mehr zu tun und müsste
ohnehin warten — man erzeugt Overhead, ohne zusätzliche Parallelität zu gewinnen. Die
Parallelität steckt in den **Geschwisterteilbäumen**, nicht im Knoten selbst.

**c) Messung** (125 490 Knoten, 2000 Schleifendurchläufe je Knoten ≈ 2 µs)

| p | T [s] | Speedup | Effizienz |
|---|---|---|---|
| 1 | 0,2204 | 1,01 | 1,01 |
| 2 | 0,1150 | 1,89 | 0,95 |
| 4 | 0,0729 | 3,05 | 0,76 |
| 8 | 0,0636 | 3,50 | 0,44 |

Der Speedup ist **weitgehend unabhängig von der Unbalanciertheit** — genau das ist der Punkt:
Jeder Thread holt sich seine Arbeit selbst aus der Queue, die Last gleicht sich automatisch
aus. Der Knick zwischen 4 und 8 Threads kommt von der Hardware (4 physische Kerne, darüber
nur noch SMT), nicht von der Baumstruktur.

Bemerkenswert ist die Zeile `p = 1`: Die Task-Version ist dort **genauso schnell wie die
serielle**. Bei 2 µs Arbeit pro Knoten fallen die ~300 ns Task-Overhead kaum ins Gewicht.
Genau das ändert sich in Teilaufgabe f).

**d) Vergleich mit „erst sammeln, dann `parallel for`"**

| | Task-Variante | Sammeln + `parallel for` |
|---|---|---|
| Zusatzspeicher | O(Tiefe) für den Task-Stack | **O(n)** für das Zeigerarray |
| Vorlauf | keiner | ein **vollständiger serieller Durchlauf** — nicht parallelisierbar, das ist ein Amdahl-Anteil |
| Lastausgleich | automatisch über die Queue | nur mit `schedule(dynamic)`, und nur wenn die Knotenkosten variieren |
| Anwendbarkeit | auch wenn die Struktur sich während der Traversierung ändert | nur bei statischer Struktur |
| Overhead pro Knoten | Task-Erzeugung (~200 ns) | nahezu null |

**Die Array-Variante lohnt sich, wenn** derselbe Baum **mehrfach** traversiert wird (die
Sammelkosten amortisieren sich), die Struktur statisch ist und die Arbeit pro Knoten klein ist —
dann schlägt der fehlende Task-Overhead alles andere. Bei einmaliger Traversierung oder
dynamischer Struktur gewinnen die Tasks.

**e) Cutoff nach Teilbaumgröße**

```cpp
void traverse(node *p) {
    if (!p) return;
    if (p->size < CUTOFF) { traverse_serial(p); return; }   /* size beim Bauen mitführen */
    #pragma omp task
    traverse(p->left);
    #pragma omp task
    traverse(p->right);
    process(p);
}
```

Weil `size` die tatsächliche Arbeit im Teilbaum misst (und nicht nur die Rekursionstiefe
schätzt), ist dieser Cutoff **robust gegen Unbalanciertheit** — bei einem schiefen Baum wäre
ein tiefenbasierter Cutoff auf der flachen Seite zu grob und auf der tiefen Seite zu fein. Wann
immer man die Teilbaumgröße billig kennt, ist sie das bessere Kriterium.

**f) Der Cutoff wirkt erst, wenn die Knoten billig werden** (8 Threads, alle Zeiten in ms)

| Arbeit/Knoten | ≈ Zeit/Knoten | seriell | Tasks ohne Cutoff | Cutoff 64 | Cutoff 512 | Cutoff 4096 |
|---|---|---|---|---|---|---|
| 2000 | ≈ 2 µs | 222,7 | 63,6 (**3,50×**) | 61,9 (3,60×) | 62,0 (3,59×) | 68,6 (3,25×) |
| 100 | ≈ 100 ns | 15,6 | 7,1 (2,21×) | 3,6 (4,33×) | 3,1 (**4,97×**) | 7,9 (1,97×) |
| 20 | ≈ 20 ns | 2,3 | 5,6 (**0,42×**) | 2,3 (1,00×) | 0,8 (2,77×) | 0,7 (**3,52×**) |

Drei klar unterscheidbare Regime:

- **2 µs pro Knoten:** Der Cutoff bringt nichts. Der Task-Overhead von ~300 ns ist nur etwa
  15 % der Knotenarbeit — mehr als diese 15 % kann ein Cutoff gar nicht einsparen.
- **100 ns pro Knoten:** Overhead und Nutzarbeit sind etwa gleich groß. Der Cutoff **halbiert**
  die Laufzeit (2,21× → 4,97×). Hier entscheidet er über gut und mittelmäßig.
- **20 ns pro Knoten:** Ohne Cutoff ist die Task-Version **langsamer als seriell** (0,42×) —
  der Overhead ist das 15-Fache der Nutzarbeit. Der Cutoff entscheidet über Erfolg und
  Misserfolg, und je größer, desto besser.

Die quantitative Grenze: Ein Cutoff lohnt sich, sobald

$$ \frac{\text{Overhead pro Task} \ (\approx 300\,\text{ns})}{\text{Arbeit pro Knoten}} $$

nicht mehr klein gegen 1 ist. Umgekehrt gilt die Faustregel: **Ein Arbeitspaket sollte
mindestens 10 µs Arbeit enthalten**, dann ist der Task-Overhead unter 3 % — bei 2000
Durchläufen pro Knoten ist das mit einem Cutoff von 8 Knoten schon erreicht.

**g) Die Sammelphase der Array-Variante**

| Arbeit/Knoten | Zeit gesamt | davon seriell gesammelt | Anteil |
|---|---|---|---|
| 2000 | 63,9 ms | 0,9 ms | **1,4 %** |
| 100 | 4,2 ms | 0,7 ms | **15,8 %** |
| 20 | 1,2 ms | 0,6 ms | **47,8 %** |

Die Sammelphase ist ein reiner **Amdahl-Anteil**: Sie ist prinzipiell nicht parallelisierbar
(man muss die Zeiger verfolgen, um sie zu kennen) und kostet unabhängig von der Knotenarbeit
immer dieselben ~0,7 ms. Bei billigen Knoten wächst ihr Anteil auf fast die Hälfte — nach
Amdahl ist der Speedup dann auf gut 2 gedeckelt, egal wie viele Kerne man einsetzt.

**Fazit für d):** Die Array-Variante ist genau dann sinnvoll, wenn die Sammelkosten sich über
**mehrere Traversierungen amortisieren**. Bei einmaliger Traversierung mit billigen Knoten ist
sie die schlechteste aller Varianten — bei teuren Knoten spielt sie dagegen mit den Tasks
gleichauf (63,9 ms vs. 63,6 ms).

---

## Lösung 7.7 — `depend` vorhersagen

**a) Kanten**

| Kante | Grund | Typ |
|---|---|---|
| T1 → T3 | T1 `out:a`, T3 `in:a` | **RAW** |
| T1 → T4 | T1 `out:a`, T4 `inout:a` (schreibt) | **WAW** (und RAW über den gelesenen Teil) |
| T3 → T4 | T3 `in:a`, T4 schreibt `a` | **WAR** |
| T2 → T4 | T2 `out:b`, T4 `in:b` | **RAW** |
| T4 → T5 | T4 schreibt `a`, T5 `in:a` | **RAW** |
| T3 → T5 | T3 `out:c`, T5 `in:c` | **RAW** |

T1 und T2 sind untereinander unabhängig (verschiedene Variablen).

**b) Graph**

```
    T1 (out a)          T2 (out b, sleep 1s)
      │  ╲                    │
 RAW  │   ╲ WAW               │ RAW
      ▼    ╲                  │
    T3 (in a, out c)          │
      │  ╲                    │
 RAW  │   ╲──── WAR ──► T4 (in b, inout a) ◄──┘
      │                       │
      └──────────► T5 (in a, c) ◄── RAW
```

**c) Ausgabe**

`T1: a = 2` → `T3: c = 2·10 = 20` → `T2: b = 3` (nach 1 s) → `T4: a = 2 + 3 = 5` →
`T5` gibt aus:

```
5 20
```

**d)** Die Gesamtlaufzeit beträgt **etwa 1 Sekunde** — sie wird vollständig vom `sleep(1)` in T2
bestimmt. Gleichzeitig laufen können **T1 und T2** (unabhängig) sowie **T2 und T3** (T3 hängt
nur von T1 ab). Während T2 schläft, sind T1 und T3 also längst fertig; T4 und T5 laufen danach
in Nullzeit. Mehr als 2 der 4 Threads werden nie gleichzeitig gebraucht.

**e)** Mit `depend(in : a)` statt `depend(inout : a)` ist das Programm **fehlerhaft**.

`depend` beschreibt der Laufzeit, *was die Task mit den Daten tut* — der Code schreibt aber
weiterhin `a = a + b`. Die Deklaration wäre also schlicht gelogen. Konkrete Folgen:

- Die WAR-Kante T3 → T4 verschwindet (zwei `in` kollidieren nicht) → T3 und T4 dürfen
  **gleichzeitig** laufen. T3 liest `a`, während T4 es schreibt → **Race Condition**, `c` kann
  20 oder 50 sein.
- Die Kante T4 → T5 verschwindet ebenfalls (`in` gegen `in`) → T5 kann `a` lesen, bevor T4 es
  geschrieben hat. Ausgabe dann `2 20` statt `5 20`.

Merksatz: `depend` ersetzt keine Synchronisation, es **beschreibt** sie. Eine falsche
Beschreibung ist genauso schlimm wie gar keine.

**f)** T5 hängt über zwei Wege am `sleep`: über `a` (T2 → T4 → T5) und über den Wert von `b`
selbst. Der Grund ist, dass `a = a + b` beides zusammenführt. Will man T5 früher haben, muss
man die Ergebnisse **entkoppeln** — statt `a` zu überschreiben, schreibt T4 in eine neue
Variable (**Umbenennung**, siehe Aufgabe 7.4):

```cpp
#pragma omp task depend(in : b) depend(in : a) depend(out : a2)   /* T4' */
{ a2 = a + b; }

#pragma omp task depend(in : a, c)                                /* T5' */
{ std::printf("%d %d\n", a, c); }        /* liest jetzt das ALTE a */
```

Jetzt hängt T5' nur noch von T1 und T3 ab und läuft sofort — allerdings gibt es dann das
**ursprüngliche** `a = 2` aus. Man muss sich also entscheiden: Entweder T5 sieht das Ergebnis
von T4 (dann muss es warten), oder es sieht es nicht (dann kann es früher laufen). Die
Abhängigkeit lässt sich nur auflösen, wenn man die Semantik ändert — echte RAW-Kanten sind
nicht wegoptimierbar, im Gegensatz zu den WAR/WAW-Kanten aus Aufgabe 7.4.

---

## Lösung 7.8 — Arbeit und Tiefe von Mergesort

**a) Arbeit**

$$ T_1(n) = 2\,T_1(n/2) + \Theta(n), \qquad T_1(1) = \Theta(1) $$

Master-Theorem, Fall 2 (`a = 2`, `b = 2`, `f(n) = n = n^{\log_b a}`):

$$ T_1(n) = \Theta(n \log n) $$

**b) Tiefe**

Die beiden rekursiven Aufrufe laufen parallel, also zählt nur **einer** von ihnen. Das Mischen
bleibt seriell und kostet `Θ(n)`:

$$ T_\infty(n) = T_\infty(n/2) + \Theta(n) $$

Ausrollen: `n + n/2 + n/4 + … = 2n`, also

$$ T_\infty(n) = \Theta(n) $$

**c) Parallelität**

$$ \frac{T_1}{T_\infty} = \frac{\Theta(n \log n)}{\Theta(n)} = \Theta(\log n) $$

Für `n = 2²⁰` mit `T₁ = n \log_2 n` und `T∞ = 2n`:

$$ T_1 = 20 \cdot 2^{20} \approx 2{,}10 \cdot 10^7, \qquad
   T_\infty = 2 \cdot 2^{20} \approx 2{,}10 \cdot 10^6 $$

$$ \frac{T_1}{T_\infty} = \frac{20}{2} = \mathbf{10} $$

**d) Interpretation**

- **8 Kerne:** Parallelität 10 > 8 — die Maschine lässt sich noch auslasten. Brauchbar, aber
  ohne Reserve: Man ist bereits nah an der Grenze, und die Effizienz wird spürbar unter 1
  liegen.
- **256 Kerne:** Der Speedup ist bei **10** gedeckelt, egal wie viele Kerne dazukommen. 246
  Kerne bleiben nutzlos. Die Ursache ist das serielle Mischen auf der obersten Ebene: Der
  letzte Merge über alle `n` Elemente läuft auf **einem** Thread und kostet allein schon `n`.

**e) Mit parallelem Mischen**

$$ T_\infty(n) = T_\infty(n/2) + \Theta(\log n) \;\Longrightarrow\; T_\infty(n) = \Theta(\log^2 n) $$

(Die Rekursion hat `log n` Ebenen, jede kostet `Θ(log n)`.)

$$ \frac{T_1}{T_\infty} = \frac{\Theta(n \log n)}{\Theta(\log^2 n)} = \Theta\!\left(\frac{n}{\log n}\right) $$

Für `n = 2²⁰`: `T∞ ≈ log₂²(2²⁰) = 400`, also

$$ \frac{T_1}{T_\infty} \approx \frac{2{,}10 \cdot 10^7}{400} \approx \mathbf{52\,000} $$

Damit ist die Parallelität praktisch unbegrenzt — jede reale Maschine lässt sich auslasten.

**f) Brent-Schranke für `p = 8`**

| Variante | `T₁/p` | `T∞` | `T(8) ≤ T₁/p + T∞` | Overhead |
|---|---|---|---|---|
| b) serielles Mischen | 2,62 · 10⁶ | 2,10 · 10⁶ | **4,72 · 10⁶** | Tiefe verdoppelt die Schranke fast |
| e) paralleles Mischen | 2,62 · 10⁶ | 400 | **2,62 · 10⁶** | Tiefe vernachlässigbar |

Die Aussage in einem Satz: Solange `T∞ ≪ T₁/p` gilt, ist die Brent-Schranke praktisch gleich
dem perfekten Speedup. Bei serieller Mischphase ist diese Bedingung schon bei 8 Kernen
verletzt — die Schranke ist fast doppelt so groß wie das Ideal, was gut zur Parallelität von
nur 10 passt.

---

## Lösung 7.9 — `taskwait` vs. `taskgroup`

**a) und b)** — siehe [`code/taskwait_vs_taskgroup.c`](code/taskwait_vs_taskgroup.c)

```cpp
#pragma omp parallel
#pragma omp single
{
    #pragma omp task                              /* Kind 1 */
    { msleep(100); std::printf("Kind 1 fertig\n"); }

    #pragma omp task                              /* Kind 2 */
    {
        #pragma omp task                          /* Enkel */
        { msleep(400); std::printf("Enkel fertig\n"); }
        std::printf("Kind 2 fertig\n");
    }

    #pragma omp taskwait
    std::printf(">>> nach taskwait\n");
}
```

Ausgabe:

```
Kind 2 fertig
Kind 1 fertig
>>> nach taskwait          ← kommt VOR dem Enkel
Enkel fertig
```

`taskwait` wartet nur auf die **direkten Kinder** der erzeugenden Task. Der Enkel gehört zu
Kind 2, nicht zum `single`-Block — also wird er nicht abgewartet.

Mit `taskgroup` um die beiden Tasks herum:

```
Kind 2 fertig
Kind 1 fertig
Enkel fertig
>>> nach taskgroup         ← jetzt garantiert nach ALLEN Nachkommen
```

**c)** Fibonacci funktioniert trotzdem, weil die Garantie sich **rekursiv fortpflanzt**. Der
Beweis geht per Induktion über die Rekursionstiefe:

- *Basis:* `fib(0)` und `fib(1)` erzeugen keine Tasks und sind bei ihrer Rückkehr fertig.
- *Schritt:* Kehrt ein Aufruf `fib(k)` zurück, hat er sein eigenes `taskwait` passiert; seine
  beiden Kinder sind also fertig. Nach Induktionsannahme haben diese Kinder ihrerseits ihre
  gesamten Teilbäume abgewartet.

Wenn also das `taskwait` auf Ebene `k` zurückkehrt, ist der **komplette Teilbaum** fertig.
`taskgroup` bräuchte man nur, wenn eine Task Nachkommen erzeugt und zurückkehrt, **ohne** auf
sie zu warten — genau das tut Fibonacci nicht.

**d) Quicksort mit Tasks**

```cpp
static void qsort_tasks(int *a, long lo, long hi) {
    if (hi - lo < CUTOFF) { insertion_sort(a, lo, hi); return; }
    long m = partition(a, lo, hi);          /* seriell */
    #pragma omp task                        /* a ist ein Zeiger -> firstprivate,
                                               die Daten dahinter sind geteilt */
    qsort_tasks(a, lo, m - 1);
    #pragma omp task
    qsort_tasks(a, m + 1, hi);
    #pragma omp taskwait
}

/* Aufruf */
#pragma omp parallel
#pragma omp single
qsort_tasks(a, 0, n - 1);
```

Korrektheitsprüfung gegen die Standardbibliothek:

```cpp
std::vector<int> ref = original;
std::sort(ref.begin(), ref.end());

std::vector<int> a = original;
qsort_tasks(a.data(), 0, n - 1);

assert(a == ref);          // std::vector vergleicht elementweise
```

Messung (`n = 2 · 10⁷` zufällige `int`, Cutoff 10 000):

| p | eigener Quicksort seriell | mit Tasks | Speedup |
|---|---|---|---|
| 1 | 1,300 s | 1,313 s | 0,99 |
| 4 | 1,300 s | 0,484 s | **2,69** |
| 8 | 1,300 s | 0,439 s | **2,96** |

> **C++-Nebenbemerkung, die man gesehen haben sollte:** Als Referenz misst das Programm
> `std::sort` mit **1,26 s** — genauso schnell wie der handgeschriebene serielle Quicksort.
> Dasselbe mit C's `qsort()` aus `<stdlib.h>` braucht **2,31 s**, also fast das Doppelte.
>
> Der Grund ist kein besserer Algorithmus, sondern die Schnittstelle: `qsort()` bekommt einen
> **Funktionszeiger** und muss ihn bei *jedem* Vergleich indirekt aufrufen — das lässt sich
> nicht inlinen. `std::sort` bekommt den Vergleich als **Typ** (Template-Parameter), sodass der
> Compiler ihn direkt einsetzt. Bei ~5·10⁸ Vergleichen macht dieser eine Unterschied den
> Faktor 2. Merksatz: In C++ kosten Abstraktionen über Templates nichts, Abstraktionen über
> Funktionszeiger schon.

Der Speedup bleibt deutlich unter `p`, und zwar aus einem strukturellen Grund: Das **erste
`partition` läuft seriell über alle `n` Elemente**, das zweite über je `n/2` und so weiter. Der
Rekursionsbaum hat Tiefe `log n`, aber die oberste Ebene allein kostet `Θ(n)` — genau dieselbe
Situation wie beim seriellen Mischen in Aufgabe 7.8. Die Tiefe ist `T∞ = Θ(n)`, die Parallelität
also nur `Θ(log n)`. Bei `n = 2·10⁷` sind das rund 24 — theoretisch mehr als 8, aber
Speicherbandbreite und die ungleichen Pivot-Schnitte fressen den Rest.

**e)** Zwischen den beiden rekursiven Aufrufen ist **keine** Synchronisation nötig: `partition`
teilt das Array in zwei **disjunkte** Bereiche, die Bernstein-Bedingung ist erfüllt
(`O₁ ∩ O₂ = ∅`, und gelesen wird jeweils nur der eigene Bereich).

Das `taskwait` am Ende ist trotzdem sinnvoll, aus einem anderen Grund: Es begrenzt die Zahl
gleichzeitig lebender Tasks und hält den Speicherbedarf der Task-Queue klein. Streng
notwendig ist es hier nicht — die Barriere am Ende der `parallel`-Region garantiert ohnehin,
dass alles fertig ist, bevor der serielle Code weiterläuft.

Anders sieht es aus, wenn der Aufrufer **innerhalb** der parallelen Region direkt nach dem
Sortieren auf das Array zugreifen will:

```cpp
#pragma omp parallel
#pragma omp single
{
    #pragma omp taskgroup           /* jetzt zwingend */
    { qsort_tasks(a, 0, n - 1); }
    std::printf("%d\n", a[0]);           /* ohne taskgroup: Race */
}
```

Hier braucht man **`taskgroup`**, nicht `taskwait`: Die Sortierung erzeugt beliebig tiefe
Nachkommen, und `taskwait` würde nur die oberste Ebene abwarten.
