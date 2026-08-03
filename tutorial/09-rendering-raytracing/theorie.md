# Kapitel 09 — 3D-Rendering: Ray Tracing

> **Kompaktkapitel.** Zusammenfassung statt Lehrbuch — keine eigenen Übungsdateien.
> **Quellen:** `vl/9-Render.pdf` (20 Folien), Übungsblatt `project/excercise7.pdf`
> (Aufgabe 7.2), eigene Abgabe `project/`
> **Zeitbedarf:** ca. 18 min
> **Voraussetzungen:** lineare Algebra (Kreuzprodukt, 3×3-Determinanten), Kapitel 06

---

## 1. Worum es geht

Ray Tracing ist ein Musterbeispiel für **embarrassingly parallel**: Jeder Pixel ist völlig
unabhängig von allen anderen. Die Vorlesung zeigt gleichzeitig, dass 3D-Rendering im Kern
„nur Geometrie und lineare Algebra in großem Maßstab" ist — man braucht keine Engine, um es
zu verstehen.

---

## 2. Die Kernpunkte

### 2.1 Warum *inverse* Strahlverfolgung

- **Vorwärts** (physikalisch korrekt): Licht verlässt die Quelle, prallt an Objekten ab und
  trifft irgendwann das Auge. Praktisch unmöglich — die allermeisten Strahlen treffen das
  Auge nie.
- **Invers** (das Verfahren): Man schickt Strahlen **vom Auge durch jeden Pixel** in die
  Szene, sucht das zuerst getroffene Objekt und berechnet dessen Farbe.

Man rechnet also nur die Strahlen, die tatsächlich zum Bild beitragen.

### 2.2 Die Kamerabasis

Gegeben Kameraposition $C$, Zielpunkt $V_{la}$ („look at") und eine grobe Aufwärtsrichtung
$V_{up}$. Daraus wird eine **orthonormale Rechtsbasis** konstruiert:

$$\boldsymbol{f} = \frac{V_{la} - C}{\|V_{la} - C\|} \qquad \text{(Blickrichtung, optische Achse)}$$
$$\boldsymbol{r} = \frac{\boldsymbol{f} \times V_{up}}{\|\boldsymbol{f} \times V_{up}\|} \qquad \text{(rechts, horizontale Bildachse)}$$
$$\boldsymbol{u} = \boldsymbol{r} \times \boldsymbol{f} \qquad \text{(oben, senkrecht auf beiden)}$$

Der Sinn: Nach dieser Konstruktion ist der Ray-Tracer **invariant gegen die Orientierung des
Weltkoordinatensystems**. Man rechnet in Kamerakoordinaten und dreht am Ende einmal.

> Die Folie schreibt bei $\boldsymbol{r}$ im Nenner $\|f - V_{up}\|$; gemeint ist die Norm des
> **Kreuzprodukts**, sonst wäre $\boldsymbol{r}$ nicht normiert.

### 2.3 Vom Pixel zur Strahlrichtung

Diskrete Pixelkoordinaten $(i, j)$ eines $W \times H$-Sensors werden auf kontinuierliche
Koordinaten $(x,y) \in [-1,1]^2$ auf einer virtuellen Bildebene im Abstand $d = 1$ abgebildet.
Dabei gehen **Öffnungswinkel** $\theta$ (field of view) und **Seitenverhältnis** $a$ ein:

$$x = \left(\frac{2(i + 0{,}5)}{W} - 1\right) \cdot a \cdot \tan\frac{\theta}{2}, \qquad
y = -\left(\frac{2(j + 0{,}5)}{H} - 1\right) \cdot \tan\frac{\theta}{2}$$

Das `+0,5` trifft die **Pixelmitte** statt der Ecke. Das Minuszeichen bei $y$ dreht die
Bildzeilenrichtung um (Zeile 0 ist oben, $y$ zeigt nach oben) — es steht im Vorlesungscode,
auf der Formelfolie fehlt es.

Zusammengesetzt zur Weltrichtung:

$$\boxed{D = \operatorname{normalize}(x\,\boldsymbol{r} + y\,\boldsymbol{u} + \boldsymbol{f})}$$

Der Strahl ist damit $C + \alpha D$ mit $\alpha \ge 0$.

### 2.4 Objekte: Dreiecksnetze

3D-Objekte werden als **Netz von Dreiecken** beschrieben. Ein Dreieck ist durch seine Ecken
$T_1, T_2, T_3 \in \mathbb{R}^3$ gegeben, seine Orientierung durch die **Normale**

$$N = (T_2 - T_1) \times (T_3 - T_1)$$

(Die Reihenfolge der Faktoren legt nur die Richtung fest — vertauscht man sie, zeigt $N$ nach
innen statt nach außen.)

Das übliche Dateiformat ist **STL**, eine schlichte Liste von Dreiecken mit Normalen:

```
facet normal 0.70675 -0.70746 0
  outer loop
    vertex 1000 0 0
    vertex 0 -1000 0
    vertex 0 -999 -52
  endloop
endfacet
```

### 2.5 Der Schnittpunkt — der rechnerische Kern

Ein Schnittpunkt $H$ liegt genau dann eindeutig vor, wenn der Strahl nicht parallel zum
Dreieck verläuft:

$$\beta = \langle N, D\rangle \ne 0$$

$H$ liegt im Dreieck genau dann, wenn er eine **Konvexkombination** der Ecken ist:

$$H = \lambda_1 T_1 + \lambda_2 T_2 + \lambda_3 T_3, \quad \lambda_i \ge 0,\ \textstyle\sum \lambda_i = 1$$

Mit $\lambda_1 = 1 - \lambda_2 - \lambda_3$ eliminiert man einen Parameter. Gleichsetzen mit
dem Strahl:

$$C + \alpha D = (1 - \lambda_2 - \lambda_3)T_1 + \lambda_2 T_2 + \lambda_3 T_3$$

Umstellen liefert ein **lineares 3×3-System**:

$$\begin{pmatrix} \vdots & \vdots & \vdots \\ T_2 - T_1 & T_3 - T_1 & -D \\ \vdots & \vdots & \vdots \end{pmatrix}
\begin{pmatrix} \lambda_2 \\ \lambda_3 \\ \alpha \end{pmatrix} = C - T_1$$

**Treffer-Bedingung:**

$$\lambda_2 \ge 0, \quad \lambda_3 \ge 0, \quad \lambda_2 + \lambda_3 \le 1, \quad \alpha \ge 0$$

(Die letzte Bedingung schließt Dreiecke **hinter** der Kamera aus.)

### 2.6 Cramersche Regel

Für $[A_1 | A_2 | A_3]\,x = b$ gilt mit der praktischen Determinantenform

$$\det M = \langle M_1,\ M_2 \times M_3 \rangle$$

$$x_1 = \frac{\det[b\,|\,A_2\,|\,A_3]}{\det A}, \qquad
x_2 = \frac{\det[A_1\,|\,b\,|\,A_3]}{\det A}, \qquad
x_3 = \frac{\det[A_1\,|\,A_2\,|\,b]}{\det A}$$

Für ein 3×3-System ist das schneller als eine Gauß-Elimination und braucht **keine
Verzweigungen** — ein wichtiger Punkt, wenn der Kernel später auf einer GPU laufen soll
(Kapitel 11: Divergenz).

### 2.7 Möller–Trumbore und Shading

**Nächstes Dreieck finden:** Für einen Strahl den Schnittpunkt mit **allen** Dreiecken
berechnen und dasjenige mit dem **kleinsten nichtnegativen $\alpha$** wählen.

**Lambertsche (diffuse) Beleuchtung:** Die Helligkeit hängt davon ab, wie gut die
Oberflächennormale und die Lichtrichtung ausgerichtet sind:

$$\gamma = \max\bigl(0,\ \langle N, -L\rangle\bigr)$$

| $\langle N, -L\rangle$ | Bedeutung |
|---|---|
| $= 1$ | Fläche zeigt direkt zum Licht → maximal hell |
| $= 0$ | streifender Einfall |
| $< 0$ | vom Licht abgewandt → durch $\max(0,\cdot)$ auf 0 gesetzt |

### 2.8 Die Pipeline und ihre Komplexität

1. Szene definieren, STL-Datei einlesen
2. **Für jeden Pixel $P_{i,j}$:**
   1. **Richtung** $D$ berechnen
   2. **Trace:** über alle Dreiecke den nächsten Schnittpunkt suchen (Cramer)
   3. **Shade:** Farbe aus Normale und Lichtrichtung
   4. **Store:** Float-Farbe auf Integer 0–255 abbilden
3. Als **PPM** ausgeben (`P3`, Breite, Höhe, Maximalwert, dann RGB-Tripel als Text)

$$\boxed{O(\text{Pixel} \times \text{Dreiecke})}$$

**Der Engpass** ist genau dieses Produkt: Für jedes Pixel jedes Dreieck zu prüfen, ist bei
detaillierten Modellen hoffnungslos.

### 2.9 Parallelisierung

**Die Schleife über die Pixel ist perfekt parallel** — jeder Strahl ist unabhängig, es gibt
keine gemeinsamen Schreibzugriffe:

```cpp
#pragma omp parallel for collapse(2) schedule(dynamic)
for (int j = 0; j < height; ++j)
    for (int i = 0; i < width; ++i)
        pixels[j * width + i] = trace(C, richtung(i, j), light);
```

Zwei Details, die den Unterschied machen:

- **`collapse(2)`**, damit auch bei wenigen Bildzeilen genug Iterationen zum Verteilen da sind.
- **`schedule(dynamic)`**, weil die Strahlen **ungleich teuer** sind: Ein Strahl, der ins
  Leere geht, prüft trotzdem alle Dreiecke; einer, der früh trifft, ebenfalls — aber die
  Verzweigungen und Cache-Effekte streuen deutlich.

Der algorithmische Hebel ist derselbe wie in Kapitel 08: **nicht alle Dreiecke prüfen**,
sondern eine räumliche Datenstruktur benutzen (*Spatial Partitioning*, **BVH** — Bounding
Volume Hierarchy). Das senkt den Trace-Schritt von $O(\text{Dreiecke})$ auf
$O(\log \text{Dreiecke})$ — dieselbe Idee wie der Quadtree bei Barnes-Hut.

---

## 3. Klausur-Schnellcheck

| Frage | Antwort |
|---|---|
| Warum inverse statt vorwärts gerichteter Strahlverfolgung? | vorwärts erreichen fast alle Strahlen das Auge nie; invers rechnet man nur die bildwirksamen |
| Wie konstruiert man die Kamerabasis? | $f = (V_{la}-C)/\|\cdot\|$, $r = (f \times V_{up})/\|\cdot\|$, $u = r \times f$ |
| Wie kommt man vom Pixel zur Strahlrichtung? | $(i,j) \to (x,y)$ über FOV und Seitenverhältnis, dann $D = \text{normalize}(xr + yu + f)$ |
| Wozu das `+0,5` in der Pixelformel? | trifft die Pixelmitte statt der Ecke |
| Wie ist ein Dreieck beschrieben? | drei Ecken $T_1,T_2,T_3$; Normale $N = (T_2-T_1)\times(T_3-T_1)$ |
| Wann ist der Schnittpunkt eindeutig? | wenn $\langle N, D\rangle \ne 0$, der Strahl also nicht parallel zum Dreieck liegt |
| Wie lautet das lineare System? | $[T_2-T_1 \mid T_3-T_1 \mid -D]\,(\lambda_2,\lambda_3,\alpha)^{\mathsf T} = C - T_1$ |
| Wann liegt der Treffer im Dreieck? | $\lambda_2,\lambda_3 \ge 0$, $\lambda_2+\lambda_3 \le 1$, $\alpha \ge 0$ |
| Determinantenformel für Cramer | $\det M = \langle M_1, M_2 \times M_3\rangle$ |
| Wie wählt man das nächste Dreieck? | kleinstes nichtnegatives $\alpha$ |
| Lambertsche Beleuchtung | $\gamma = \max(0, \langle N, -L\rangle)$ |
| Komplexität | $O(\text{Pixel} \times \text{Dreiecke})$ |
| Wie parallelisiert man? | über die Pixel — unabhängig, `collapse(2)` + `schedule(dynamic)` |
| Wie senkt man die Komplexität? | räumliche Datenstruktur (BVH/Spatial Partitioning) statt aller Dreiecke |

---

## 4. Merkkasten

> - **Invers**: Strahlen vom Auge durch die Pixel, nicht vom Licht ins Auge.
> - Kamerabasis $f, r, u$ macht den Tracer unabhängig vom Weltkoordinatensystem.
> - Der Schnitt ist ein **3×3-System**, gelöst mit **Cramer** — verzweigungsfrei und damit
>   GPU-tauglich.
> - Treffer $\iff \lambda_2, \lambda_3 \ge 0$, $\lambda_2 + \lambda_3 \le 1$, $\alpha \ge 0$.
> - $\gamma = \max(0, \langle N, -L\rangle)$ ist das ganze Shading.
> - $O(\text{Pixel} \times \text{Dreiecke})$ — **embarrassingly parallel** über die Pixel,
>   aber algorithmisch verbesserbar über eine BVH.

---

## 5. Verbindung

**Baut auf:** Kapitel 06 (`parallel for`, `collapse`, `schedule(dynamic)`).

**Querverbindung:** Die Struktur ist dieselbe wie in Kapitel 08 — ein naives $O(N \cdot M)$
gegen eine hierarchische Raumzerlegung (Quadtree dort, BVH hier). Und wie in Kapitel 11:
Ein Pixel pro Thread ist genau die Denkfigur „ein Kernel ersetzt eine datenparallele
Schleife"; die Vorlesung nennt in Kapitel 11 selbst das Beispiel „1 Million Pixel →
1 Million Threads".

**Eigene Abgabe:** `project/` enthält den OpenMP-Ray-Tracer zu Blatt 7, Aufgabe 7.2,
inklusive Skalierungsmessung und Amdahl-Fit in `project/REPORT.md`.
