// Kapitel 12, Aufgabe 12.7 -- Matrixprodukt C = A*B, naiv und gekachelt
//
//   nvcc -std=c++17 -O2 -arch=sm_80 -I../../_common matmul.cu -o matmul
//   ./matmul [N]           eine Groesse
//   ./matmul               N = 1024, 2048, 4096
//
// Der gekachelte Kernel ist gegenueber der Vorlesungsfassung (Folie 39) um die
// Waechter erweitert, die er fuer N % TILE != 0 braucht -- siehe Aufgabe 12.7b.
//
// Roofline (A100, P = 19500 GFLOP/s, B = 1500 GB/s, Knick bei I = 13):
//   naiv       I = 0,25    -> <=   375 GFLOP/s
//   TILE = 32  I = T/4 = 8 -> <= 12000 GFLOP/s
// Beide Vorhersagen werden von der Messung verfehlt -- nach oben bzw. nach
// unten. Warum, steht in der Loesung zu 12.7d.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "cuda_check.hpp"

#define TILE 32

// ---------------------------------------------------------------- Kernels ---

// Naiv: jedes Element von A und B wird N-mal aus dem globalen Speicher gelesen.
__global__ void matmul_naiv(const float* A, const float* B, float* C, int N) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < N && col < N) {
        float acc = 0.0f;
        for (int k = 0; k < N; ++k)
            acc += A[row * N + k] * B[k * N + col];
        C[row * N + col] = acc;
    }
}

// Gekachelt: jede geladene Kachel wird von TILE Threads benutzt.
__global__ void matmul_tiled(const float* A, const float* B, float* C, int N) {
    __shared__ float As[TILE][TILE];
    __shared__ float Bs[TILE][TILE];

    const int tx  = threadIdx.x;
    const int ty  = threadIdx.y;
    const int row = blockIdx.y * TILE + ty;
    const int col = blockIdx.x * TILE + tx;

    float acc = 0.0f;

    // Aufrunden -- sonst fehlt die letzte, angebrochene Kachel (Aufgabe 12.7b).
    const int kacheln = (N + TILE - 1) / TILE;
    for (int m = 0; m < kacheln; ++m) {
        const int a_col = m * TILE + tx;
        const int b_row = m * TILE + ty;

        // Ausserhalb liegende Eintraege muessen 0 sein, nicht ungeladen bleiben:
        // der alte Kachelinhalt ginge sonst als Muell in die Summe ein.
        As[ty][tx] = (row < N && a_col < N) ? A[row * N + a_col] : 0.0f;
        Bs[ty][tx] = (col < N && b_row < N) ? B[b_row * N + col] : 0.0f;
        __syncthreads();                 // (1) Kachel vollstaendig geladen

        for (int k = 0; k < TILE; ++k)
            acc += As[ty][k] * Bs[k][tx];
        __syncthreads();                 // (2) alle fertig, bevor ueberschrieben wird
    }

    if (row < N && col < N)
        C[row * N + col] = acc;
}

// ------------------------------------------------------------- Host-Seite ---

static void fuelle(std::vector<float>& A, std::vector<float>& B, int N) {
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c) {
            A[static_cast<size_t>(r) * N + c] = static_cast<float>((r + c) % 13) * 0.25f;
            B[static_cast<size_t>(r) * N + c] = static_cast<float>((r * 7 + c) % 11) * 0.5f;
        }
}

// Serielle Referenz -- nur fuer wenige Stichproben, sonst dauert es zu lange.
static int pruefe(const std::vector<float>& C, const std::vector<float>& A,
                  const std::vector<float>& B, int N) {
    const int proben[] = {0, N / 3, N / 2, N - 1};
    int fehler = 0;
    for (int r : proben)
        for (int c : proben) {
            double soll = 0.0;
            for (int k = 0; k < N; ++k)
                soll += static_cast<double>(A[static_cast<size_t>(r) * N + k]) *
                        static_cast<double>(B[static_cast<size_t>(k) * N + c]);
            const float ist = C[static_cast<size_t>(r) * N + c];
            if (std::fabs(ist - soll) > 1e-3 * std::fabs(soll) + 1e-3) ++fehler;
        }
    return fehler;
}

static void lauf(int N) {
    const size_t elem  = static_cast<size_t>(N) * static_cast<size_t>(N);
    const size_t bytes = elem * sizeof(float);

    std::vector<float> h_A(elem), h_B(elem), h_C(elem);
    fuelle(h_A, h_B, N);

    float *d_A = nullptr, *d_B = nullptr, *d_C = nullptr;
    CUDA_CHECK(cudaMalloc(&d_A, bytes));
    CUDA_CHECK(cudaMalloc(&d_B, bytes));
    CUDA_CHECK(cudaMalloc(&d_C, bytes));
    CUDA_CHECK(cudaMemcpy(d_A, h_A.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_B, h_B.data(), bytes, cudaMemcpyHostToDevice));

    dim3 threads(TILE, TILE);
    dim3 blocks((N + TILE - 1) / TILE, (N + TILE - 1) / TILE);

    const double flops = 2.0 * static_cast<double>(N) * N * N;
    ec::GpuTimer t;

    auto messe = [&](const char* name, auto starte, double schranke) {
        CUDA_CHECK(cudaMemset(d_C, 0, bytes));
        starte();                                    // Aufwaermlauf
        CUDA_CHECK_KERNEL();

        float best = 1e30f;
        for (int r = 0; r < 3; ++r) {
            t.start();
            starte();
            const float ms = t.stop_ms();
            CUDA_CHECK_KERNEL();
            if (ms < best) best = ms;
        }
        CUDA_CHECK(cudaMemcpy(h_C.data(), d_C, bytes, cudaMemcpyDeviceToHost));
        const int fehler  = pruefe(h_C, h_A, h_B, N);
        const double gfps = flops / (best * 1e-3) / 1e9;
        std::printf("%-26s %10.2f %12.1f %13.1f %9.0f%% %9s\n", name, best, gfps,
                    schranke, gfps / schranke * 100.0, fehler ? "FEHLER" : "OK");
    };

    std::printf("N = %d   (%.2f GFLOP)\n", N, flops / 1e9);
    std::printf("%-26s %10s %12s %13s %10s %9s\n", "Kernel", "T [ms]", "GFLOP/s",
                "Roofline", "davon", "Pruefung");
    std::printf("%s\n", std::string(86, '-').c_str());

    messe("naiv (global)", [&] { matmul_naiv<<<blocks, threads>>>(d_A, d_B, d_C, N); }, 375.0);
    messe("gekachelt (TILE = 32)", [&] { matmul_tiled<<<blocks, threads>>>(d_A, d_B, d_C, N); }, 12000.0);
    std::printf("\n");

    CUDA_CHECK(cudaFree(d_A));
    CUDA_CHECK(cudaFree(d_B));
    CUDA_CHECK(cudaFree(d_C));
}

int main(int argc, char** argv) {
    ec::geraet_ausgeben();
    std::printf("Roofline-Schranken: naiv I = 0,25 -> 375 GFLOP/s ; "
                "gekachelt I = TILE/4 = 8 -> 12000 GFLOP/s\n\n");

    if (argc >= 2) {
        const int N = std::atoi(argv[1]);
        if (N <= 0) { std::fprintf(stderr, "N muss positiv sein\n"); return 1; }
        lauf(N);
        return 0;
    }
    for (int N : {1024, 2048, 4096}) lauf(N);

    // Aufgabe 12.7b: der gekachelte Kernel muss auch fuer N % TILE != 0 stimmen.
    std::printf("Gegenprobe mit N = 1000 (kein Vielfaches von TILE = 32):\n");
    lauf(1000);
    return 0;
}
