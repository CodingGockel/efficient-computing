// Scoping-Selbsttest (Kapitel 06, Abschnitt 6 / Aufgaben 6.1 und 6.2).
//
// Erst die Ausgabe auf Papier vorhersagen, DANN ausfuehren:
//
//   make && OMP_NUM_THREADS=6 ./bin/scoping
//
// Mehrfach starten -- die Teile 1 und 4 sind absichtlich nicht deterministisch.
#include <omp.h>

#include <cstdio>

int g = 0;  // global -> shared

int main() {
    // ---- Teil 1: Default-Scoping (Aufgabe 6.1) ----
    int h = 0;  // ausserhalb deklariert -> shared
    omp_set_num_threads(4);
    #pragma omp parallel
    {
        int u = 0;         // im Block deklariert -> private
        static int v = 0;  // static -> SHARED, trotz Deklaration im Block
        ++g;
        ++h;
        ++u;
        ++v;
        #pragma omp critical
        std::printf("  Thread %d: u=%d v=%d\n", omp_get_thread_num(), u, v);
    }
    std::printf("Teil 1: g=%d h=%d   (erwartet je 1..4, nicht deterministisch)\n\n", g, h);

    // ---- Teil 2: Threadzahl und Rangfolge (Aufgabe 6.2) ----
    // Achtung: In Aufgabe 6.2 steht dieser Code allein in einem Programm, dort
    // zeigt Zeile A den Wert von OMP_NUM_THREADS. Hier hat Teil 1 die ICV
    // bereits mit omp_set_num_threads(4) ueberschrieben -- deshalb steht in
    // A und B eine 4 statt der 6. Genau das ist die Rangfolge aus Abschnitt 4.2.
    std::printf("A: %d %d\n", omp_get_num_threads(), omp_get_max_threads());
    #pragma omp parallel num_threads(2)
    {
        #pragma omp master
        std::printf("B: %d %d\n", omp_get_num_threads(), omp_get_max_threads());
    }
    omp_set_num_threads(3);
    std::printf("C: %d %d\n", omp_get_num_threads(), omp_get_max_threads());
    #pragma omp parallel
    {
        #pragma omp single
        std::printf("D: %d %d\n", omp_get_num_threads(), omp_get_max_threads());
    }
    std::printf("\n");

    // ---- Teil 3: private / firstprivate / lastprivate ----
    int i = -1, j = -1;
    #pragma omp parallel private(i) num_threads(2)
    {
        // i ist hier UNINITIALISIERT -- nicht -1
        i = omp_get_thread_num();
    }
    std::printf("Teil 3a: nach private(i):        i=%d   (erwartet -1)\n", i);

    i = 10;
    #pragma omp parallel firstprivate(i) num_threads(2)
    {
        #pragma omp critical
        std::printf("  firstprivate: Thread %d sieht i=%d\n", omp_get_thread_num(), i);
        i = 1000 + omp_get_thread_num();
    }
    std::printf("Teil 3b: nach firstprivate(i):   i=%d   (erwartet 10)\n", i);

    #pragma omp parallel for lastprivate(j) num_threads(2)
    for (int k = 0; k < 8; ++k) j = k;
    std::printf("Teil 3c: nach lastprivate(j):    j=%d   (erwartet 7)\n\n", j);

    // ---- Teil 4: Race Condition ----
    int a = 0;
    #pragma omp parallel num_threads(8)
    {
        int b = a;
        b = b + 1;
        a = b;
    }
    std::printf("Teil 4: a=%d   (erwartet 8, tatsaechlich 1..8 -- race condition)\n", a);

    return 0;
}
