// Fibonacci mit Tasks: das Granularitaetsproblem (Kapitel 07, Aufgabe 7.5).
//
//   1) seriell rekursiv
//   2) Tasks ohne Cutoff        -> um Groessenordnungen LANGSAMER als seriell
//   3) nur final()              -> immer noch langsamer als seriell!
//   4) final() + omp_in_final() -> schneller als seriell
//   5) Cutoff-Sweep             -> Optimum sichtbar machen
//
//   make && ./bin/fib_tasks [n] [max_sweep_tiefe]
//   OMP_NUM_THREADS=8 ./bin/fib_tasks 32
//
// Achtung: Variante 2 bei n > 33 dauert sehr lange -- das ist der Punkt der Aufgabe.
#include <omp.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "bench.hpp"

long fib_seriell(int n) {
    if (n < 2) return n;
    return fib_seriell(n - 1) + fib_seriell(n - 2);
}

// iterativ, nur um die Task-Anzahl abzuschaetzen
double fib_iter(int n) {
    double a = 0.0, b = 1.0;
    for (int k = 0; k < n; ++k) {
        const double c = a + b;
        a = b;
        b = c;
    }
    return a;
}

// ohne Cutoff: eine Task pro Additionsknoten
long fib_naiv(int n) {
    if (n < 2) return n;
    long i = 0, j = 0;
    // shared(i) ist noetig: Task-Variablen sind sonst firstprivate, und der
    // Erzeuger saehe das Ergebnis nie.
    #pragma omp task shared(i)
    i = fib_naiv(n - 1);
    #pragma omp task shared(j)
    j = fib_naiv(n - 2);
    #pragma omp taskwait  // ohne das: Unsinn UND ein zerstoerter Stackframe
    return i + j;
}

// Cutoff nach Rekursionstiefe -- die einfachste und lesbarste Variante
long fib_cutoff(int n, int tiefe) {
    if (n < 2) return n;
    if (tiefe <= 0) return fib_seriell(n);
    long i = 0, j = 0;
    #pragma omp task shared(i)
    i = fib_cutoff(n - 1, tiefe - 1);
    #pragma omp task shared(j)
    j = fib_cutoff(n - 2, tiefe - 1);
    #pragma omp taskwait
    return i + j;
}

// Cutoff ueber final(): der Teilbaum wird undeferred ausgefuehrt -- die
// task-Konstrukte werden aber weiterhin ANGETROFFEN und kosten trotzdem.
long fib_final(int n, int k) {
    if (n < 2) return n;
    long i = 0, j = 0;
    #pragma omp task shared(i) final(n < k)
    i = fib_final(n - 1, k);
    #pragma omp task shared(j) final(n < k)
    j = fib_final(n - 2, k);
    #pragma omp taskwait
    return i + j;
}

// final + omp_in_final(): im finalen Teilbaum wird gar kein Konstrukt mehr
// durchlaufen. Erst DAS macht final schneller als die serielle Version.
long fib_final_opt(int n, int k) {
    if (n < 2) return n;
    if (omp_in_final()) return fib_seriell(n);
    long i = 0, j = 0;
    #pragma omp task shared(i) final(n < k)
    i = fib_final_opt(n - 1, k);
    #pragma omp task shared(j) final(n < k)
    j = fib_final_opt(n - 2, k);
    #pragma omp taskwait
    return i + j;
}

// Das parallel+single-Idiom einmal zentral: einer erzeugt, alle arbeiten.
template <class F>
long in_region(F&& erzeuge) {
    long ergebnis = 0;
    #pragma omp parallel
    #pragma omp single
    ergebnis = erzeuge();
    return ergebnis;
}

int main(int argc, char** argv) {
    const int n = (argc > 1) ? std::atoi(argv[1]) : 32;
    const int tiefe_max = (argc > 2) ? std::atoi(argv[2]) : 14;

    std::printf("n=%d  threads=%d\n\n", n, omp_get_max_threads());

    long r = 0;
    const double t_ser = ec::once([&] { r = fib_seriell(n); });
    const long r_ser = r;
    std::printf("%-34s %10.4f s   fib=%ld\n", "1) seriell rekursiv", t_ser, r_ser);
    std::printf("   (ohne Cutoff entstehen ca. %.2e Tasks fuer je EINE Addition)\n",
                2.0 * fib_iter(n + 1));

    double t = ec::once([&] { r = in_region([&] { return fib_naiv(n); }); });
    std::printf("%-34s %10.4f s   fib=%ld   %6.1fx langsamer\n", "2) Tasks OHNE Cutoff", t, r,
                t / t_ser);

    t = ec::once([&] { r = in_region([&] { return fib_final(n, n - 10); }); });
    std::printf("%-34s %10.4f s   fib=%ld   %6.2fx\n", "3) nur final(n < n-10)", t, r,
                t_ser / t);

    t = ec::once([&] { r = in_region([&] { return fib_final_opt(n, n - 10); }); });
    std::printf("%-34s %10.4f s   fib=%ld   %6.2fx\n", "4) final + omp_in_final()", t, r,
                t_ser / t);
    std::printf("   -> final schaltet die Verzoegerung ab, omp_in_final die Erzeugung.\n");

    std::printf("\n5) Cutoff-Sweep ueber die Rekursionstiefe:\n");
    std::printf("   %6s %12s %10s %12s\n", "Tiefe", "T [s]", "S vs. ser", "#Tasks ca.");
    double best = 1e30;
    int best_d = 0;
    for (int d = 0; d <= tiefe_max; ++d) {
        t = ec::once([&] { r = in_region([&] { return fib_cutoff(n, d); }); });
        if (r != r_ser) {
            std::printf("   FEHLER bei Tiefe %d\n", d);
            return 1;
        }
        if (t < best) {
            best = t;
            best_d = d;
        }
        std::printf("   %6d %12.5f %10.2f %12.0f%s\n", d, t, t_ser / t, std::exp2(d),
                    (d == 0) ? "   (= seriell)" : "");
    }
    std::printf("\n   Optimum bei Tiefe %d (%.5f s, %.2fx schneller als seriell)\n", best_d,
                best, t_ser / best);
    std::printf("   Faustregel: 2^d ~ 10*p  ->  d ~ log2(10*%d) = %.1f\n",
                omp_get_max_threads(), std::log2(10.0 * omp_get_max_threads()));
    return 0;
}
