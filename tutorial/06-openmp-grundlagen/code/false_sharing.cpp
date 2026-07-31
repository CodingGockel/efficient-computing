// False Sharing sichtbar machen (Kapitel 06, Abschnitt 9.2 / Aufgabe 6.4).
//
// Drei Varianten berechnen DASSELBE korrekte Ergebnis, unterscheiden sich aber
// um Groessenordnungen in der Laufzeit:
//
//   1) zaehler[id]        -- alle Zaehler in einer Cache-Line -> Ping-Pong
//   2) zaehler[id][8]     -- Padding auf 64 Byte              -> kein Konflikt
//   3) lokale Variable    -- gar kein geteilter Schreibzugriff -> optimal
//
//   make && ./bin/false_sharing [n]
#include <omp.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "bench.hpp"

// 64 Byte Cache-Line = 8 double. Ein Zaehler pro Cache-Line.
constexpr int kCacheLine = 64;
constexpr int kPad = kCacheLine / sizeof(double);  // = 8

inline double arbeit(long i) { return static_cast<double>(i & 7); }

// Die Zaehler sind volatile, damit der Compiler sie nicht im Register haelt und
// die Schreibzugriffe aus der Schleife herauszieht -- sonst waere das Phaenomen
// wegoptimiert und gar nicht messbar. Variante 1 und 2 sind dadurch exakt
// gleich teuer bis auf EINEN Unterschied: das Speicherlayout.
using Zaehler = volatile double;

int main(int argc, char** argv) {
    const long n = (argc > 1) ? std::atol(argv[1]) : 50'000'000L;
    const int p = omp_get_max_threads();

    std::vector<double> flach(p);
    std::vector<double> gepolstert(static_cast<std::size_t>(p) * kPad);

    std::printf("n=%ld threads=%d  (sizeof(double)=%zu, Cache-Line=%d Byte)\n\n", n, p,
                sizeof(double), kCacheLine);
    std::printf("%-40s %10s %10s %14s\n", "Variante", "T [s]", "rel.", "Summe");

    Zaehler* f = flach.data();
    Zaehler* g = gepolstert.data();
    double summe = 0.0;

    // 1) false sharing: alle p Zaehler liegen in einer einzigen Cache-Line
    const double t1 = ec::once([&] {
        #pragma omp parallel num_threads(p)
        {
            const int id = omp_get_thread_num();
            #pragma omp for schedule(static)
            for (long i = 0; i < n; ++i) f[id] += arbeit(i);
        }
    });
    summe = 0.0;
    for (int k = 0; k < p; ++k) summe += flach[k];
    std::printf("%-40s %10.4f %10.2f %14.1f\n", "1) zaehler[id]  (false sharing)", t1, 1.0,
                summe);

    // 2) Padding: jeder Zaehler bekommt seine eigene Cache-Line
    const double t2 = ec::once([&] {
        #pragma omp parallel num_threads(p)
        {
            const int id = omp_get_thread_num();
            #pragma omp for schedule(static)
            for (long i = 0; i < n; ++i) g[id * kPad] += arbeit(i);
        }
    });
    summe = 0.0;
    for (int k = 0; k < p; ++k) summe += gepolstert[static_cast<std::size_t>(k) * kPad];
    std::printf("%-40s %10.4f %10.2f %14.1f\n", "2) zaehler[id*8]  (Padding)", t2, t1 / t2,
                summe);

    // 3) lokale Akkumulation == das, was reduction intern tut
    double s3 = 0.0;
    const double t3 = ec::once([&] {
        s3 = 0.0;
        #pragma omp parallel num_threads(p) reduction(+ : s3)
        {
            #pragma omp for schedule(static)
            for (long i = 0; i < n; ++i) s3 += arbeit(i);
        }
    });
    std::printf("%-40s %10.4f %10.2f %14.1f\n", "3) reduction(+:s)  (lokal)", t3, t1 / t3, s3);

    std::printf(
        "\nAlle drei Summen sind gleich -- nur die Zeit nicht.\n"
        "Variante 1 und 2 unterscheiden sich AUSSCHLIESSLICH im Speicherlayout.\n");
    return 0;
}
