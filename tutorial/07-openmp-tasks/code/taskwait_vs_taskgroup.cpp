// taskwait wartet auf Kinder, taskgroup auf alle Nachkommen
// (Kapitel 07, Abschnitt 7.1 / Aufgabe 7.9).
//
//   make && ./bin/taskwait_vs_taskgroup
//
// Erwartete Ausgabe: ">>> nach taskwait" erscheint VOR "Enkel fertig",
// ">>> nach taskgroup" dagegen DANACH.
#include <omp.h>

#include <chrono>
#include <cstdio>
#include <thread>

using namespace std::chrono_literals;

void warte(std::chrono::milliseconds ms) { std::this_thread::sleep_for(ms); }

void szene(bool mit_taskgroup) {
    std::printf(mit_taskgroup ? "--- mit taskgroup ---\n" : "--- mit taskwait ---\n");
    #pragma omp parallel num_threads(4)
    #pragma omp single
    {
        if (mit_taskgroup) {
            #pragma omp taskgroup
            {
                #pragma omp task
                {
                    warte(100ms);
                    std::printf("  Kind 1 fertig\n");
                }
                #pragma omp task
                {
                    #pragma omp task
                    {
                        warte(400ms);
                        std::printf("  Enkel fertig\n");
                    }
                    std::printf("  Kind 2 fertig\n");
                }
            }
            std::printf(">>> nach taskgroup\n");
        } else {
            #pragma omp task
            {
                warte(100ms);
                std::printf("  Kind 1 fertig\n");
            }
            #pragma omp task
            {
                #pragma omp task
                {
                    warte(400ms);
                    std::printf("  Enkel fertig\n");
                }
                std::printf("  Kind 2 fertig\n");
            }
            #pragma omp taskwait  // wartet NUR auf die direkten Kinder
            std::printf(">>> nach taskwait\n");
        }
    }
    std::printf("\n");
}

int main() {
    szene(false);
    szene(true);
    return 0;
}
