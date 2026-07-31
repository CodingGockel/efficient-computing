// Der Task-Graph aus Aufgabe 7.3 -- ausgefuehrt mit depend-Klauseln.
//
//   Task    A  B  C  D  E   F   G  H
//   Gewicht 2  5  3  4  2   6   3  2      Summe T1 = 27
//   Vorg.   -  A  A  A  B,C C,D E  F,G    kritischer Pfad T_inf = 14
//
// Jede Task rechnet w * EINHEIT Sekunden. Gemessen wird die Gesamtlaufzeit
// ("Makespan") in Zeiteinheiten -- sie muss bei p >= 3 gegen 14 gehen und
// bei p = 1 gegen 27.
//
//   make && ./bin/taskgraph [ms_pro_einheit]
//   for p in 1 2 3 4 8; do OMP_NUM_THREADS=$p ./bin/taskgraph; done
#include <omp.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

double g_einheit = 0.05;  // Sekunden pro Gewichtseinheit

// Aktiv warten, NICHT schlafen: der Thread soll wirklich belegt sein.
// Mit sleep() wuerde ein einziger Thread alle Tasks "gleichzeitig" erledigen.
void rechne(int w, const char* name, std::chrono::steady_clock::time_point t0) {
    const auto jetzt = [] { return std::chrono::steady_clock::now(); };
    const auto sek = [t0](std::chrono::steady_clock::time_point t) {
        return std::chrono::duration<double>(t - t0).count();
    };

    const auto start = jetzt();
    const double ziel = sek(start) + w * g_einheit;
    volatile double s = 0.0;
    while (sek(jetzt()) < ziel)
        for (int k = 0; k < 1000; ++k) s += 1.0;

    #pragma omp critical
    std::printf("  %s: Start %5.1f  Ende %5.1f  (Thread %d)\n", name, sek(start) / g_einheit,
                sek(jetzt()) / g_einheit, omp_get_thread_num());
}

int main(int argc, char** argv) {
    if (argc > 1) g_einheit = std::atof(argv[1]) / 1000.0;

    // Je eine Variable pro Task -- die depend-Klauseln bilden damit exakt
    // die Kanten des Graphen ab (alles echte RAW-Abhaengigkeiten).
    int a, b, c, d, e, f, g, h;
    // Nur die depend-Klauseln benutzen sie -- das zaehlt fuer g++ nicht als Zugriff.
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h;

    const int p = omp_get_max_threads();
    std::printf("threads=%d   T1=27   T_inf=14   (Zeiten in Gewichtseinheiten)\n", p);

    const auto t0 = std::chrono::steady_clock::now();
    #pragma omp parallel
    #pragma omp single
    {
        #pragma omp task depend(out : a)
        rechne(2, "A", t0);

        #pragma omp task depend(in : a) depend(out : b)
        rechne(5, "B", t0);

        #pragma omp task depend(in : a) depend(out : c)
        rechne(3, "C", t0);

        #pragma omp task depend(in : a) depend(out : d)
        rechne(4, "D", t0);

        #pragma omp task depend(in : b, c) depend(out : e)
        rechne(2, "E", t0);

        #pragma omp task depend(in : c, d) depend(out : f)
        rechne(6, "F", t0);

        #pragma omp task depend(in : e) depend(out : g)
        rechne(3, "G", t0);

        #pragma omp task depend(in : f, g) depend(out : h)
        rechne(2, "H", t0);
    }
    const double T =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() /
        g_einheit;

    std::printf("\n  Makespan T(%d) = %.2f Einheiten\n", p, T);
    std::printf("  Speedup  S      = %.2f      Effizienz E = %.2f\n", 27.0 / T, 27.0 / T / p);
    std::printf("  Schranken: max(T_inf, T1/p) = %.2f   Brent: T1/p + T_inf = %.2f\n",
                std::max(14.0, 27.0 / p), 27.0 / p + 14.0);
    return 0;
}
