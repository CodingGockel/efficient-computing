// Matrix-Vektor-Produkt in drei Varianten (Kapitel 06, Abschnitt 12).
//
//   a) y = A*x                  unabhaengig    -> parallel for
//   b) y = A*x und s = sum(y)   Reduktion      -> reduction(+:s)
//   c) y[i] = y[i-1] + A[i]*x   abhaengig      -> zweiphasig (Praefixsumme)
//
// Jede Variante wird gegen die serielle Referenz geprueft und gemessen.
//
//   make && ./bin/matvec [m] [n] [wiederholungen]
//   OMP_NUM_THREADS=4 ./bin/matvec 4096 4096
#include <omp.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "bench.hpp"

using Vec = std::vector<double>;

// ---------- seriell (Referenz) ----------

void matvec_seriell(const Vec& A, const Vec& x, Vec& y, int m, int n) {
    for (int i = 0; i < m; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += A[static_cast<std::size_t>(i) * n + j] * x[j];
        y[i] = s;
    }
}

double summe_seriell(const Vec& A, const Vec& x, Vec& y, int m, int n) {
    double s_ges = 0.0;
    for (int i = 0; i < m; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += A[static_cast<std::size_t>(i) * n + j] * x[j];
        y[i] = s;
        s_ges += y[i];
    }
    return s_ges;
}

void praefix_seriell(const Vec& A, const Vec& x, Vec& y, int m, int n) {
    y[0] = 0.0;
    for (int i = 1; i < m; ++i) {
        double s = y[i - 1];
        for (int j = 0; j < n; ++j) s += A[static_cast<std::size_t>(i) * n + j] * x[j];
        y[i] = s;
    }
}

// ---------- a) unabhaengig ----------

void matvec_omp(const Vec& A, const Vec& x, Vec& y, int m, int n) {
    // i und j sind INNERHALB der Schleife deklariert -> automatisch private.
    // In C++ braucht man die private(...)-Klausel deshalb praktisch nie.
    #pragma omp parallel for default(none) shared(A, x, y, m, n) schedule(static)
    for (int i = 0; i < m; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += A[static_cast<std::size_t>(i) * n + j] * x[j];
        y[i] = s;
    }
}

// ---------- b) Reduktion ----------

double summe_omp(const Vec& A, const Vec& x, Vec& y, int m, int n) {
    double s_ges = 0.0;
    #pragma omp parallel for default(none) shared(A, x, y, m, n) \
            reduction(+ : s_ges) schedule(static)
    for (int i = 0; i < m; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += A[static_cast<std::size_t>(i) * n + j] * x[j];
        y[i] = s;
        s_ges += y[i];
    }
    return s_ges;
}

// ---------- c) abhaengig: zweiphasig ----------

void praefix_omp(const Vec& A, const Vec& x, Vec& y, Vec& r, int m, int n) {
    // Phase 1: O(m*n), voll parallel
    #pragma omp parallel for default(none) shared(A, x, r, m, n) schedule(static)
    for (int i = 1; i < m; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += A[static_cast<std::size_t>(i) * n + j] * x[j];
        r[i] = s;
    }
    // Phase 2: O(m), seriell -- vernachlaessigbar fuer n >> 1
    y[0] = 0.0;
    for (int i = 1; i < m; ++i) y[i] = y[i - 1] + r[i];
}

// ---------- Hilfsfunktionen ----------

bool gleich(const Vec& a, const Vec& b, double tol) {
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::fabs(a[i] - b[i]) > tol) return false;
    return true;
}

int main(int argc, char** argv) {
    const int m = (argc > 1) ? std::atoi(argv[1]) : 4096;
    const int n = (argc > 2) ? std::atoi(argv[2]) : 4096;
    const int reps = (argc > 3) ? std::atoi(argv[3]) : 5;

    Vec A(static_cast<std::size_t>(m) * n);
    Vec x(n), y(m), ref(m), r(m);

    for (int j = 0; j < n; ++j) x[j] = 1.0 / (j + 1.0);
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < n; ++j)
            A[static_cast<std::size_t>(i) * n + j] = static_cast<double>((i + j) % 7) - 3.0;

    // grosszuegige Toleranz: die Reduktion aendert die Summationsreihenfolge
    const double tol = 1e-9 * m * n;

    std::printf("m=%d n=%d threads=%d reps=%d\n\n", m, n, omp_get_max_threads(), reps);
    ec::header();

    const double t_ser = ec::best_of(reps, [&] { matvec_seriell(A, x, ref, m, n); });
    ec::row("a) seriell (Referenz)", t_ser, t_ser);

    double t = ec::best_of(reps, [&] { matvec_omp(A, x, y, m, n); });
    ec::row("a) parallel for", t, t_ser, gleich(y, ref, tol));

    double s_ref = 0.0, s_par = 0.0;
    const double t_sum_ser = ec::best_of(reps, [&] { s_ref = summe_seriell(A, x, ref, m, n); });
    t = ec::best_of(reps, [&] { s_par = summe_omp(A, x, y, m, n); });
    ec::row("b) reduction(+:s)", t, t_sum_ser, ec::nahe(s_ref, s_par, 1e-9));

    const double t_pre_ser = ec::best_of(reps, [&] { praefix_seriell(A, x, ref, m, n); });
    t = ec::best_of(reps, [&] { praefix_omp(A, x, y, r, m, n); });
    ec::row("c) zweiphasig", t, t_pre_ser, gleich(y, ref, tol));

    return 0;
}
