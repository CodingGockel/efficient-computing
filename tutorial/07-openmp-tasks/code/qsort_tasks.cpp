// Quicksort mit OpenMP-Tasks (Kapitel 07, Aufgabe 7.9 d/e).
//
// Die beiden Haelften nach dem Partitionieren sind disjunkt -- die
// Bernstein-Bedingung ist erfuellt, also duerfen sie parallel laufen.
// Geprueft wird gegen std::sort.
//
//   make && ./bin/qsort_tasks [n] [cutoff]
//   for p in 1 2 4 8; do OMP_NUM_THREADS=$p ./bin/qsort_tasks; done
#include <omp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "bench.hpp"

long g_cutoff = 10'000;  // darunter seriell sortieren

// Median-of-three, damit sortierte Eingaben nicht entarten
long partitioniere(int* a, long lo, long hi) {
    const long mitte = lo + (hi - lo) / 2;
    if (a[mitte] < a[lo]) std::swap(a[lo], a[mitte]);
    if (a[hi] < a[lo]) std::swap(a[lo], a[hi]);
    if (a[hi] < a[mitte]) std::swap(a[mitte], a[hi]);
    const int pivot = a[mitte];
    std::swap(a[mitte], a[hi - 1]);
    long i = lo, j = hi - 1;
    for (;;) {
        while (a[++i] < pivot) {}
        while (a[--j] > pivot) {}
        if (i >= j) break;
        std::swap(a[i], a[j]);
    }
    std::swap(a[i], a[hi - 1]);
    return i;
}

void qsort_seriell(int* a, long lo, long hi) {
    while (hi - lo > 16) {
        const long p = partitioniere(a, lo, hi);
        // kleinere Haelfte rekursiv, groessere iterativ -> Stacktiefe O(log n)
        if (p - lo < hi - p) {
            qsort_seriell(a, lo, p - 1);
            lo = p + 1;
        } else {
            qsort_seriell(a, p + 1, hi);
            hi = p - 1;
        }
    }
    for (long i = lo + 1; i <= hi; ++i) {  // insertion sort
        const int v = a[i];
        long j = i - 1;
        while (j >= lo && a[j] > v) {
            a[j + 1] = a[j];
            --j;
        }
        a[j + 1] = v;
    }
}

void qsort_tasks(int* a, long lo, long hi) {
    if (hi - lo < g_cutoff) {
        qsort_seriell(a, lo, hi);
        return;
    }
    const long p = partitioniere(a, lo, hi);  // seriell
    // a ist ein Zeiger -> firstprivate (die Kopie ist genau richtig),
    // die Daten dahinter sind geteilt. Die beiden Bereiche sind disjunkt.
    #pragma omp task
    qsort_tasks(a, lo, p - 1);
    #pragma omp task
    qsort_tasks(a, p + 1, hi);
    #pragma omp taskwait  // begrenzt die Zahl gleichzeitig lebender Tasks
}

std::vector<int> zufallsdaten(long n, unsigned seed) {
    std::mt19937 rng(seed);
    std::vector<int> v(static_cast<std::size_t>(n));
    for (int& x : v) x = static_cast<int>(rng());
    return v;
}

int main(int argc, char** argv) {
    const long n = (argc > 1) ? std::atol(argv[1]) : 20'000'000L;
    if (argc > 2) g_cutoff = std::atol(argv[2]);

    std::printf("n=%ld  cutoff=%ld  threads=%d\n\n", n, g_cutoff, omp_get_max_threads());
    std::printf("%-30s %10s %10s %10s\n", "Variante", "T [s]", "Speedup", "korrekt");

    const std::vector<int> original = zufallsdaten(n, 42);

    std::vector<int> ref = original;
    const double t_std = ec::once([&] { std::sort(ref.begin(), ref.end()); });
    std::printf("%-30s %10.4f %10s %10s\n", "std::sort (Referenz)", t_std, "-", "-");

    std::vector<int> a = original;
    const double t_ser = ec::once([&] { qsort_seriell(a.data(), 0, n - 1); });
    std::printf("%-30s %10.4f %10.2f %10s\n", "eigener Quicksort seriell", t_ser,
                t_std / t_ser, (a == ref) ? "ja" : "NEIN");

    a = original;
    const double t_par = ec::once([&] {
        #pragma omp parallel
        #pragma omp single
        qsort_tasks(a.data(), 0, n - 1);
    });
    std::printf("%-30s %10.4f %10.2f %10s\n", "Quicksort mit Tasks", t_par, t_ser / t_par,
                (a == ref) ? "ja" : "NEIN");

    return 0;
}
