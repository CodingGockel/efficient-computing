// Unbalancierter Baum: Tasks vs. "erst sammeln, dann parallel for"
// (Kapitel 07, Aufgabe 7.6).
//
// Der Baum ist bewusst schief: der linke Teilbaum ist drei Ebenen tiefer als
// der rechte. Genau daran scheitert jede statische Zerlegung.
//
//   make && ./bin/tree_tasks [tiefe] [arbeit_pro_knoten]
//   for p in 1 2 4 8; do OMP_NUM_THREADS=$p ./bin/tree_tasks; done
//   ./bin/tree_tasks 30 100     # billigere Knoten -> Cutoff wird entscheidend
#include <omp.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "bench.hpp"

struct Node {
    // unique_ptr = Besitz: der Destruktor raeumt den ganzen Teilbaum ab.
    // Kein freetree() noetig. Bei Tiefe 30 ist die Destruktor-Rekursion unkritisch.
    std::unique_ptr<Node> links, rechts;
    long groesse = 1;  // Teilbaumgroesse -- fuer den groessenbasierten Cutoff
    double wert = 0.0;
};

long g_arbeit = 2000;  // Schleifendurchlaeufe pro Knoten
long g_cutoff = 0;     // Teilbaumgroesse, ab der seriell weitergemacht wird

std::unique_ptr<Node> baue(int tiefe) {
    if (tiefe <= 0) return nullptr;
    auto p = std::make_unique<Node>();
    p->links = baue(tiefe - 1);
    p->rechts = baue(tiefe - 3);
    p->groesse = 1 + (p->links ? p->links->groesse : 0) + (p->rechts ? p->rechts->groesse : 0);
    return p;
}

// echte Arbeit pro Knoten
void bearbeite(Node* p) {
    double s = 0.0;
    for (long k = 1; k <= g_arbeit; ++k) s += 1.0 / static_cast<double>(k);
    p->wert = s;
}

void traverse_seriell(Node* p) {
    if (!p) return;
    traverse_seriell(p->links.get());
    traverse_seriell(p->rechts.get());
    bearbeite(p);
}

void traverse_tasks(Node* p) {
    if (!p) return;
    if (g_cutoff > 0 && p->groesse < g_cutoff) {
        traverse_seriell(p);
        return;
    }
    // p ist firstprivate: jede Task bekommt ihre eigene Kopie des Zeigers.
    // Genau das rettet den Code -- der Erzeuger laeuft naemlich sofort weiter.
    #pragma omp task
    traverse_tasks(p->links.get());
    #pragma omp task
    traverse_tasks(p->rechts.get());
    bearbeite(p);  // eigene Arbeit -- braucht KEINE eigene Task
}

// Variante d): erst seriell einsammeln, dann parallel for
void sammle(Node* p, std::vector<Node*>& aus) {
    if (!p) return;
    aus.push_back(p);
    sammle(p->links.get(), aus);
    sammle(p->rechts.get(), aus);
}

double pruefsumme(Node* p) {
    if (!p) return 0.0;
    return p->wert + pruefsumme(p->links.get()) + pruefsumme(p->rechts.get());
}

int main(int argc, char** argv) {
    const int tiefe = (argc > 1) ? std::atoi(argv[1]) : 30;
    if (argc > 2) g_arbeit = std::atol(argv[2]);

    auto wurzel = baue(tiefe);
    const long n = wurzel->groesse;
    Node* root = wurzel.get();

    std::printf("Baumtiefe=%d  Knoten=%ld  Arbeit/Knoten=%ld  threads=%d\n\n", tiefe, n,
                g_arbeit, omp_get_max_threads());
    std::printf("%-40s %10s %10s %8s\n", "Variante", "T [s]", "Speedup", "Pruefung");

    const double t_ser = ec::once([&] { traverse_seriell(root); });
    const double ref = pruefsumme(root);
    std::printf("%-40s %10.4f %10.2f %8s\n", "seriell rekursiv", t_ser, 1.0, "-");

    auto lauf = [&](const char* name, long cutoff) {
        g_cutoff = cutoff;
        const double t = ec::once([&] {
            #pragma omp parallel
            #pragma omp single
            traverse_tasks(root);
        });
        std::printf("%-40s %10.4f %10.2f %8s\n", name, t, t_ser / t,
                    ec::nahe(pruefsumme(root), ref) ? "OK" : "FEHLER");
        g_cutoff = 0;
    };

    lauf("Tasks (ohne Cutoff)", 0);
    char name[80];
    for (long c = 64; c <= 4096; c *= 8) {
        std::snprintf(name, sizeof name, "Tasks, Cutoff Teilbaumgroesse %ld", c);
        lauf(name, c);
    }

    // sammeln + parallel for
    std::vector<Node*> knoten;
    knoten.reserve(static_cast<std::size_t>(n));
    double t_sammeln = 0.0;
    const double t_arr = ec::once([&] {
        knoten.clear();
        t_sammeln = ec::once([&] { sammle(root, knoten); });  // ZWINGEND seriell
        const int m = static_cast<int>(knoten.size());
        #pragma omp parallel for schedule(dynamic, 64) default(none) shared(knoten, m)
        for (int i = 0; i < m; ++i) bearbeite(knoten[i]);
    });
    std::printf("%-40s %10.4f %10.2f %8s\n", "sammeln + parallel for (dynamic)", t_arr,
                t_ser / t_arr, ec::nahe(pruefsumme(root), ref) ? "OK" : "FEHLER");
    std::printf("   davon seriell zum Einsammeln: %.4f s (%.1f%% -- Amdahl-Anteil)\n",
                t_sammeln, 100.0 * t_sammeln / t_arr);
    return 0;
}
