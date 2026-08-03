// Kapitel 11, Aufgabe 11.3c -- die korrigierte Fassung des fehlerhaften
// Programms aus der Aufgabe:  y[i] = x[i] * x[i]
//
//   nvcc -std=c++17 -O2 -arch=sm_80 fehlersuche.cu -o fehlersuche
//   ./fehlersuche
//
// Die neun Fehler des Originals sind an den betroffenen Zeilen mit ihrer
// Nummer aus der Loesungstabelle markiert.

#include <cstdio>
#include <vector>

#include "cuda_check.hpp"

__global__ void quadrat(float* y, const float* x, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;   // (1) blockIdx gehoert dazu
    if (i < n)                                       // (2) Waechter
        y[i] = x[i] * x[i];
}

int main() {
    ec::geraet_ausgeben();

    const int    n     = 100000;
    const size_t bytes = static_cast<size_t>(n) * sizeof(float);  // (3) sizeof!

    std::vector<float> h_x(n), h_y(n);
    for (int i = 0; i < n; ++i) h_x[i] = static_cast<float>(i);

    float *d_x = nullptr, *d_y = nullptr;
    CUDA_CHECK(cudaMalloc(&d_x, bytes));
    CUDA_CHECK(cudaMalloc(&d_y, bytes));

    // (4) Richtung: die Eingabe muss HostToDevice
    CUDA_CHECK(cudaMemcpy(d_x, h_x.data(), bytes, cudaMemcpyHostToDevice));

    const int threads = 256;
    const int blocks  = (n + threads - 1) / threads;  // (5) aufrunden

    // (6) erst Bloecke, dann Threads   (7) Device-Zeiger, nicht h_x.data()
    quadrat<<<blocks, threads>>>(d_y, d_x, n);
    CUDA_CHECK_KERNEL();                              // (8) beide Fehlerklassen

    CUDA_CHECK(cudaMemcpy(h_y.data(), d_y, bytes, cudaMemcpyDeviceToHost));

    // Aufgabe 11.3d: ueber den GANZEN Vektor pruefen. y[7] allein liegt im
    // ersten Block und ist gegen jeden der Fehler 3, 5 und 6 blind.
    int fehler = 0, erster = -1;
    for (int i = 0; i < n; ++i) {
        const float sollwert = h_x[i] * h_x[i];
        if (!ec::nahe(h_y[i], sollwert)) {
            if (erster < 0) erster = i;
            ++fehler;
        }
    }

    std::printf("blocks = %d, threads = %d, gestartete Threads = %d (n = %d)\n",
                blocks, threads, blocks * threads, n);
    std::printf("y[7] = %.1f, y[n-1] = %.1f\n", static_cast<double>(h_y[7]),
                static_cast<double>(h_y[n - 1]));
    std::printf("Pruefung ueber alle %d Elemente: %s", n, fehler ? "FEHLER" : "OK");
    if (fehler) std::printf(" (%d Abweichungen, erste bei i = %d)", fehler, erster);
    std::printf("\n");

    CUDA_CHECK(cudaFree(d_x));                        // (9) freigeben
    CUDA_CHECK(cudaFree(d_y));
    return fehler == 0 ? 0 : 1;
}
