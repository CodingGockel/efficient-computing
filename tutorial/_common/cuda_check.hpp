// Gemeinsame CUDA-Helfer für die Kapitel 11 und 12.
//
//   #include "cuda_check.hpp"
//
// Zwei Dinge, die in jedem CUDA-Programm vorkommen sollten und die man deshalb
// einmal sauber aufschreibt:
//
//   CUDA_CHECK(...)  prueft den Rueckgabewert JEDES CUDA-Aufrufs. Ohne das
//                    scheitern Kernels still: Das Ergebnisarray behaelt seinen
//                    alten Inhalt und das Programm meldet Erfolg.
//   ec::GpuTimer     misst mit cudaEvent_t statt mit einer Host-Uhr. Kernel-
//                    Starts sind asynchron -- eine Host-Uhr um den Aufruf herum
//                    misst nur das Einreihen (wenige Mikrosekunden, unabhaengig
//                    von n).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <cuda_runtime.h>

// do { ... } while (0) macht das Makro zu EINER Anweisung -- sonst zerbricht
// ein "if (x) CUDA_CHECK(...); else ..." am Semikolon.
#define CUDA_CHECK(call)                                                       \
    do {                                                                       \
        cudaError_t err__ = (call);                                            \
        if (err__ != cudaSuccess) {                                            \
            std::fprintf(stderr, "CUDA error at %s:%d: %s\n", __FILE__,        \
                         __LINE__, cudaGetErrorString(err__));                 \
            std::exit(EXIT_FAILURE);                                           \
        }                                                                      \
    } while (0)

// Nach jedem Kernel-Start: erst der Startfehler (synchron gemeldet), dann der
// Laufzeitfehler (nur ueber einen synchronisierenden Aufruf sichtbar).
#define CUDA_CHECK_KERNEL()                                                    \
    do {                                                                       \
        CUDA_CHECK(cudaGetLastError());                                        \
        CUDA_CHECK(cudaDeviceSynchronize());                                   \
    } while (0)

namespace ec {

// Zeitmessung mit CUDA-Events. Die Events werden in den GPU-Strom eingereiht
// und von der GPU selbst gestempelt; Host-Overhead faellt heraus.
// cudaEventElapsedTime liefert MILLISEKUNDEN als float (Aufloesung ~0,5 us).
class GpuTimer {
public:
    GpuTimer() {
        CUDA_CHECK(cudaEventCreate(&a_));
        CUDA_CHECK(cudaEventCreate(&b_));
    }
    ~GpuTimer() {
        cudaEventDestroy(a_);
        cudaEventDestroy(b_);
    }
    GpuTimer(const GpuTimer&)            = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    void start() { CUDA_CHECK(cudaEventRecord(a_)); }

    float stop_ms() {
        CUDA_CHECK(cudaEventRecord(b_));
        CUDA_CHECK(cudaEventSynchronize(b_));  // auf GPU-Fertigstellung warten
        float ms = 0.0f;
        CUDA_CHECK(cudaEventElapsedTime(&ms, a_, b_));
        return ms;
    }

private:
    cudaEvent_t a_{};
    cudaEvent_t b_{};
};

// Erreichte Bandbreite in GB/s aus bewegten Bytes und Zeit in Millisekunden.
inline double gb_pro_s(double bytes, double ms) {
    return bytes / (ms * 1.0e-3) / 1.0e9;
}

// Relativer Vergleich -- niemals auf == pruefen. Die GPU darf a*x+y zu einem
// FMA zusammenziehen, das mit voller Zwischenpraezision rechnet und deshalb ein
// anderes (genaueres) Ergebnis liefert als die getrennte CPU-Rechnung.
inline bool nahe(float a, float b, float tol = 1.0e-5f) {
    const float skala = std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
    return std::fabs(a - b) <= tol * skala;
}

// Geraet identifizieren -- man bekommt auf dem Cluster nicht immer dieselbe Karte.
inline void geraet_ausgeben() {
    int dev = 0;
    CUDA_CHECK(cudaGetDevice(&dev));
    cudaDeviceProp prop{};
    CUDA_CHECK(cudaGetDeviceProperties(&prop, dev));
    std::printf("GPU: %s | CC %d.%d | %d SMs | %.1f GB global | max %d Threads/Block\n\n",
                prop.name, prop.major, prop.minor, prop.multiProcessorCount,
                static_cast<double>(prop.totalGlobalMem) / (1024.0 * 1024.0 * 1024.0),
                prop.maxThreadsPerBlock);
}

}  // namespace ec
