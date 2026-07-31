// Gemeinsame Mess- und Ausgabe-Helfer für alle Kapitel.
//
//   #include "bench.hpp"        // Build mit: -I../../_common
//
// Warum std::chrono und nicht omp_get_wtime()? Beides misst Wall-Clock-Zeit und
// ist hier gleichwertig. std::chrono ist typsicher (Sekunden lassen sich nicht
// versehentlich mit Millisekunden verwechseln) und funktioniert auch ohne OpenMP.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

namespace ec {

// Führt f() reps-mal aus und gibt die KLEINSTE gemessene Zeit in Sekunden zurück.
// Minimum statt Mittelwert: Störungen (Kontextwechsel, Interrupts, andere Last)
// machen eine Messung immer langsamer, nie schneller.
template <class F>
double best_of(int reps, F&& f) {
    double best = std::numeric_limits<double>::infinity();
    for (int r = 0; r < reps; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        f();
        const auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double>(t1 - t0).count());
    }
    return best;
}

// Einmalige Messung.
template <class F>
double once(F&& f) {
    return best_of(1, std::forward<F>(f));
}

// Kopfzeile einer Ergebnistabelle.
inline void header(std::string_view spalte1 = "Variante") {
    std::printf("%-38s %10s %10s %10s\n", spalte1.data(), "T [s]", "Speedup", "Pruefung");
}

// Eine Ergebniszeile. ok < 0 bedeutet "keine Prüfung".
inline void row(std::string_view name, double t, double t_ref, int ok = -1) {
    std::printf("%-38s %10.5f %10.2f %10s\n", name.data(), t, t_ref / t,
                ok < 0 ? "-" : (ok ? "OK" : "FEHLER"));
}

// Vergleich zweier Gleitkomma-Ergebnisse mit relativer Toleranz.
// Nötig, weil eine Reduktion die Summationsreihenfolge ändert und
// Gleitkommaaddition nicht assoziativ ist -- niemals auf == prüfen.
inline bool nahe(double a, double b, double tol = 1e-9) {
    const double skala = std::max({1.0, std::fabs(a), std::fabs(b)});
    return std::fabs(a - b) <= tol * skala;
}

}  // namespace ec
