// Kapitel 12, Aufgabe 12.5 -- Reduktion auf drei Wegen:
//
//   s = sum_i (x_i - y_i)^2
//
//   nvcc -std=c++17 -O2 -arch=sm_80 -I../../_common reduktion.cu -o reduktion
//   ./reduktion [n]        Standard: n = 100000000
//
// Alle drei Fassungen rechnen dasselbe. Der Unterschied ist, WIE die
// Teilergebnisse zusammengefuehrt werden:
//
//   1) ein atomicAdd pro THREAD  -> n Zugriffe auf eine Adresse
//   2) Baumreduktion im Shared Memory, ein atomicAdd pro BLOCK
//   3) Warp-Shuffle in Registern, ein atomicAdd pro BLOCK
//
// Vorhersage (A100, B = 1500 GB/s): I = 3 FLOP / 8 Byte = 0,375
//   -> P <= 562 GFLOP/s, also speichergebunden. Die aussagekraeftige Zahl
//      ist deshalb die erreichte BANDBREITE, nicht GFLOP/s.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "cuda_check.hpp"

// ---------------------------------------------------------------- Kernels ---

// Fassung 1: der naive Weg. Korrekt, aber alle Threads serialisieren an *s.
__global__ void dist_atomic(const float* x, const float* y, float* s, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        float d = x[i] - y[i];
        atomicAdd(s, d * d);
    }
}

// Fassung 2: Grid-Stride-Vorlauf, dann Baumreduktion im Shared Memory.
__global__ void dist_shared(const float* x, const float* y, float* s, int n) {
    extern __shared__ float sm[];
    int tid    = threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    float lokal = 0.0f;
    for (int i = blockIdx.x * blockDim.x + tid; i < n; i += stride) {
        float d = x[i] - y[i];
        lokal += d * d;
    }
    sm[tid] = lokal;
    __syncthreads();                 // (1) alle haben geschrieben

    for (int k = blockDim.x / 2; k > 0; k /= 2) {
        if (tid < k) sm[tid] += sm[tid + k];
        __syncthreads();             // (2) AUSSERHALB des if -- alle muessen ankommen
    }

    if (tid == 0) atomicAdd(s, sm[0]);
}

// Aufgabe 12.5h: dieselbe Rechnung, dieselbe Zahl an Additionen -- aber die
// aktiven Threads liegen ueber alle Warps verstreut statt in den ersten.
__global__ void dist_shared_divergent(const float* x, const float* y, float* s, int n) {
    extern __shared__ float sm[];
    int tid    = threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    float lokal = 0.0f;
    for (int i = blockIdx.x * blockDim.x + tid; i < n; i += stride) {
        float d = x[i] - y[i];
        lokal += d * d;
    }
    sm[tid] = lokal;
    __syncthreads();

    for (int k = 1; k < static_cast<int>(blockDim.x); k *= 2) {
        if (tid % (2 * k) == 0) sm[tid] += sm[tid + k];
        __syncthreads();
    }

    if (tid == 0) atomicAdd(s, sm[0]);
}

// Fassung 3: die ersten fuenf Reduktionsschritte laufen in REGISTERN.
// Kein Shared Memory, keine Barriere, keine Bankkonflikte.
__inline__ __device__ float warp_reduce(float val) {
    for (int offset = 16; offset > 0; offset /= 2)
        val += __shfl_down_sync(0xffffffff, val, offset);
    return val;                      // Lane 0 haelt die Summe des Warps
}

__global__ void dist_shuffle(const float* x, const float* y, float* s, int n) {
    __shared__ float warp_summe[32]; // hoechstens 1024/32 = 32 Warps pro Block
    int tid    = threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    float lokal = 0.0f;
    for (int i = blockIdx.x * blockDim.x + tid; i < n; i += stride) {
        float d = x[i] - y[i];
        lokal += d * d;
    }

    lokal = warp_reduce(lokal);                   // Stufe 1: je Warp
    const int lane = tid % 32;
    const int warp = tid / 32;
    if (lane == 0) warp_summe[warp] = lokal;
    __syncthreads();

    if (warp == 0) {                              // Stufe 2: die Warp-Ergebnisse
        const int n_warps = blockDim.x / 32;
        lokal = (lane < n_warps) ? warp_summe[lane] : 0.0f;
        lokal = warp_reduce(lokal);
        if (lane == 0) atomicAdd(s, lokal);
    }
}

// ------------------------------------------------------------- Host-Seite ---

// Referenz in double: nur so sieht man den Fehler der float-Rechnung ueberhaupt.
static double referenz(const std::vector<float>& x, const std::vector<float>& y) {
    double s = 0.0;
    const int n = static_cast<int>(x.size());
    for (int i = 0; i < n; ++i) {
        const double d = static_cast<double>(x[i]) - static_cast<double>(y[i]);
        s += d * d;
    }
    return s;
}

int main(int argc, char** argv) {
    const int n = (argc >= 2) ? std::atoi(argv[1]) : 100000000;
    if (n <= 0) { std::fprintf(stderr, "n muss positiv sein\n"); return 1; }

    ec::geraet_ausgeben();

    const size_t bytes = static_cast<size_t>(n) * sizeof(float);
    std::vector<float> h_x(n), h_y(n);
    for (int i = 0; i < n; ++i) {
        h_x[i] = static_cast<float>((i % 1000)) * 0.001f;
        h_y[i] = static_cast<float>((i % 997)) * 0.001f;
    }
    const double soll = referenz(h_x, h_y);

    float *d_x = nullptr, *d_y = nullptr, *d_s = nullptr;
    CUDA_CHECK(cudaMalloc(&d_x, bytes));
    CUDA_CHECK(cudaMalloc(&d_y, bytes));
    CUDA_CHECK(cudaMalloc(&d_s, sizeof(float)));
    CUDA_CHECK(cudaMemcpy(d_x, h_x.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_y, h_y.data(), bytes, cudaMemcpyHostToDevice));

    int sms = 0;
    CUDA_CHECK(cudaDeviceGetAttribute(&sms, cudaDevAttrMultiProcessorCount, 0));

    const int threads = 256;
    const int blocks_voll   = (n + threads - 1) / threads;  // Fassung 1: ein Elem/Thread
    const int blocks_stride = 4 * sms;                      // Fassung 2/3: ausgelastet
    const size_t sm_bytes   = threads * sizeof(float);

    // Vorhersage aus Aufgabe 12.5a
    const double gelesen = 8.0 * static_cast<double>(n);    // x und y, je 4 Byte
    std::printf("n = %d  ->  %.2f GB gelesen, I = 3/8 = 0,375 FLOP/Byte\n", n,
                gelesen / 1e9);
    std::printf("Roofline-Schranke bei B = 1500 GB/s:  T_min = %.3f ms\n\n",
                gelesen / 1500e9 * 1e3);

    std::printf("%-40s %10s %9s %12s %11s\n", "Fassung", "T [ms]", "GB/s",
                "Ergebnis", "rel. Fehler");
    std::printf("%s\n", std::string(88, '-').c_str());

    ec::GpuTimer t;

    auto messe = [&](const char* name, auto starte, int wdh) {
        // Aufwaermlauf
        CUDA_CHECK(cudaMemset(d_s, 0, sizeof(float)));
        starte();
        CUDA_CHECK_KERNEL();

        float best = 1e30f;
        float ist  = 0.0f;
        for (int r = 0; r < wdh; ++r) {
            CUDA_CHECK(cudaMemset(d_s, 0, sizeof(float)));   // *s vorher nullen!
            t.start();
            starte();
            const float ms = t.stop_ms();
            CUDA_CHECK_KERNEL();
            if (ms < best) best = ms;
            CUDA_CHECK(cudaMemcpy(&ist, d_s, sizeof(float), cudaMemcpyDeviceToHost));
        }
        const double fehler = std::fabs(ist - soll) / std::fabs(soll);
        std::printf("%-40s %10.3f %9.1f %12.4e %11.2e %s\n", name, best,
                    ec::gb_pro_s(gelesen, best), static_cast<double>(ist), fehler,
                    fehler < 1e-3 ? "OK" : "FEHLER");
    };

    // Fassung 1 ist so langsam, dass eine Wiederholung reicht.
    messe("1) atomicAdd pro Thread", [&] {
        dist_atomic<<<blocks_voll, threads>>>(d_x, d_y, d_s, n);
    }, 1);

    messe("2) Shared Memory, if (tid < k)", [&] {
        dist_shared<<<blocks_stride, threads, sm_bytes>>>(d_x, d_y, d_s, n);
    }, 5);

    messe("2b) Shared Memory, if (tid % (2k) == 0)", [&] {
        dist_shared_divergent<<<blocks_stride, threads, sm_bytes>>>(d_x, d_y, d_s, n);
    }, 5);

    messe("3) Warp-Shuffle", [&] {
        dist_shuffle<<<blocks_stride, threads>>>(d_x, d_y, d_s, n);
    }, 5);

    std::printf("\nReferenz (CPU, double): %.6e\n", soll);
    std::printf("Warum keine Pruefung auf ==: die GPU addiert in anderer Reihenfolge,\n"
                "und atomicAdd auf float ist zusaetzlich nichtdeterministisch.\n");

    CUDA_CHECK(cudaFree(d_x));
    CUDA_CHECK(cudaFree(d_y));
    CUDA_CHECK(cudaFree(d_s));
    return 0;
}
