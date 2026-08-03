// Kapitel 11, Aufgabe 11.5 -- zweidimensionale Indexierung
//
//   B[r][c] = A[r][c] * w[c] + b[r]     (Row-Major, H x W)
//
//   nvcc -std=c++17 -O2 -arch=sm_80 bild2d.cu -o bild2d
//   ./bild2d [H] [W]        Standard: 1080 x 1920
//
// Zwei Fassungen desselben Kernels, die sich NUR darin unterscheiden, welche
// Grid-Dimension die Spalte liefert. Beide sind korrekt -- eine ist deutlich
// schneller. Das ist der Punkt der Aufgabe.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "cuda_check.hpp"

// ---------------------------------------------------------------- Kernels ---

// Richtig: .x liefert die SPALTE. Damit greifen die 32 Lanes eines Warps auf
// 32 aufeinanderfolgende float zu (128 Byte am Stueck) -> coalesced.
__global__ void korrigiere_xc(float* B, const float* A, const float* w,
                              const float* b, int H, int W) {
    int c = blockIdx.x * blockDim.x + threadIdx.x;   // Spalte
    int r = blockIdx.y * blockDim.y + threadIdx.y;   // Zeile
    if (r < H && c < W) {                            // Waechter in BEIDEN Dim.
        int i = r * W + c;                           // Row-Major
        B[i] = A[i] * w[c] + b[r];
    }
}

// Vertauscht: .x liefert die ZEILE. Ergebnis identisch, aber die 32 Lanes eines
// Warps greifen jetzt mit Abstand W*4 Byte zu -> 32 getrennte Transaktionen.
__global__ void korrigiere_xr(float* B, const float* A, const float* w,
                              const float* b, int H, int W) {
    int r = blockIdx.x * blockDim.x + threadIdx.x;   // Zeile aus .x
    int c = blockIdx.y * blockDim.y + threadIdx.y;   // Spalte aus .y
    if (r < H && c < W) {
        int i = r * W + c;
        B[i] = A[i] * w[c] + b[r];
    }
}

// ------------------------------------------------------------- Host-Seite ---

// Testdaten so waehlen, dass das Sollergebnis den Index verraet:
//   A[r][c] = r,  w[c] = 1,  b[r] = c ist nicht moeglich -> stattdessen
//   A[r][c] = r + 0.001*c,  w[c] = 1,  b[r] = 0  =>  B[r][c] = r + 0.001*c.
// Vertauscht man Zeile und Spalte, faellt das sofort auf.
static void fuelle(std::vector<float>& A, std::vector<float>& w,
                   std::vector<float>& b, int H, int W) {
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            A[r * W + c] = static_cast<float>(r) + 0.001f * static_cast<float>(c);
    for (int c = 0; c < W; ++c) w[c] = 1.0f;
    for (int r = 0; r < H; ++r) b[r] = 0.0f;
}

static float soll(int r, int c) {
    return static_cast<float>(r) + 0.001f * static_cast<float>(c);
}

// Aufgabe 11.5g: drei Positionen reichen nicht, wenn eine davon (0,0) ist --
// dort ist r*W+c = 0 fuer JEDE Linearisierung. Deshalb zusaetzlich der Rand.
static int pruefe_stichprobe(const std::vector<float>& B, int H, int W) {
    struct P { int r, c; const char* was; };
    const P p[] = {
        {0,        0,        "Ecke oben links   (blind gegen fast alles)"},
        {H / 2,    W / 2,    "Mitte             (findet r/c vertauscht)"},
        {H - 1,    W - 1,    "Ecke unten rechts (findet Rand-/Rundungsfehler)"},
        {H - 1,    0,        "letzte Zeile      (findet fehlenden y-Waechter)"},
        {0,        W - 1,    "letzte Spalte     (findet fehlenden x-Waechter)"},
    };
    int fehler = 0;
    for (const P& q : p) {
        const float ist = B[static_cast<size_t>(q.r) * W + q.c];
        const bool  ok  = ec::nahe(ist, soll(q.r, q.c), 1.0e-4f);
        if (!ok) ++fehler;
        std::printf("   (%5d,%5d) %-46s ist %12.4f soll %12.4f  %s\n", q.r, q.c,
                    q.was, static_cast<double>(ist),
                    static_cast<double>(soll(q.r, q.c)), ok ? "OK" : "FEHLER");
    }
    return fehler;
}

static int pruefe_alles(const std::vector<float>& B, int H, int W) {
    int fehler = 0;
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            if (!ec::nahe(B[static_cast<size_t>(r) * W + c], soll(r, c), 1.0e-4f))
                ++fehler;
    return fehler;
}

int main(int argc, char** argv) {
    const int H = (argc >= 2) ? std::atoi(argv[1]) : 1080;
    const int W = (argc >= 3) ? std::atoi(argv[2]) : 1920;
    if (H <= 0 || W <= 0) { std::fprintf(stderr, "H und W muessen positiv sein\n"); return 1; }

    ec::geraet_ausgeben();

    const size_t pixel = static_cast<size_t>(H) * static_cast<size_t>(W);
    const size_t bytes = pixel * sizeof(float);

    std::vector<float> h_A(pixel), h_B(pixel), h_w(W), h_b(H);
    fuelle(h_A, h_w, h_b, H, W);

    float *d_A = nullptr, *d_B = nullptr, *d_w = nullptr, *d_b = nullptr;
    CUDA_CHECK(cudaMalloc(&d_A, bytes));
    CUDA_CHECK(cudaMalloc(&d_B, bytes));
    CUDA_CHECK(cudaMalloc(&d_w, W * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_b, H * sizeof(float)));

    CUDA_CHECK(cudaMemcpy(d_A, h_A.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_w, h_w.data(), W * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_b, h_b.data(), H * sizeof(float), cudaMemcpyHostToDevice));

    // Aufgabe 11.5a -- Grid dimensionieren. .x deckt die Spalten ab.
    dim3 threads(16, 16);
    dim3 blocks_xc((W + threads.x - 1) / threads.x, (H + threads.y - 1) / threads.y);
    dim3 blocks_xr((H + threads.x - 1) / threads.x, (W + threads.y - 1) / threads.y);

    const long gestartet = static_cast<long>(blocks_xc.x) * threads.x *
                           static_cast<long>(blocks_xc.y) * threads.y;
    std::printf("Bild %d x %d = %zu Pixel\n", H, W, pixel);
    std::printf("Block  16 x 16 = 256 Threads\n");
    std::printf("Grid   %u x %u = %ld Threads, davon %ld ohne Pixel\n\n",
                blocks_xc.x, blocks_xc.y, gestartet,
                gestartet - static_cast<long>(pixel));

    ec::GpuTimer t;
    const double bytes_kernel = 8.0 * static_cast<double>(pixel);  // A lesen, B schreiben

    std::printf("%-42s %10s %9s %10s\n", "Fassung", "T [ms]", "GB/s", "Pruefung");
    std::printf("%s\n", std::string(76, '-').c_str());

    // --- Fassung 1: Spalte aus .x (coalesced) --------------------------------
    korrigiere_xc<<<blocks_xc, threads>>>(d_B, d_A, d_w, d_b, H, W);  // Aufwaermlauf
    CUDA_CHECK_KERNEL();

    float t_xc = 1e30f;
    for (int r = 0; r < 10; ++r) {
        t.start();
        korrigiere_xc<<<blocks_xc, threads>>>(d_B, d_A, d_w, d_b, H, W);
        const float ms = t.stop_ms();
        CUDA_CHECK_KERNEL();
        if (ms < t_xc) t_xc = ms;
    }
    CUDA_CHECK(cudaMemcpy(h_B.data(), d_B, bytes, cudaMemcpyDeviceToHost));
    const int f_xc = pruefe_alles(h_B, H, W);
    std::printf("%-42s %10.4f %9.1f %10s\n", "Spalte aus .x  (coalesced)", t_xc,
                ec::gb_pro_s(bytes_kernel, t_xc), f_xc == 0 ? "OK" : "FEHLER");

    std::printf("\n   Stichproben (Aufgabe 11.5g):\n");
    pruefe_stichprobe(h_B, H, W);
    std::printf("\n");

    // --- Fassung 2: Zeile aus .x (nicht coalesced) ---------------------------
    CUDA_CHECK(cudaMemset(d_B, 0, bytes));
    korrigiere_xr<<<blocks_xr, threads>>>(d_B, d_A, d_w, d_b, H, W);  // Aufwaermlauf
    CUDA_CHECK_KERNEL();

    float t_xr = 1e30f;
    for (int r = 0; r < 10; ++r) {
        t.start();
        korrigiere_xr<<<blocks_xr, threads>>>(d_B, d_A, d_w, d_b, H, W);
        const float ms = t.stop_ms();
        CUDA_CHECK_KERNEL();
        if (ms < t_xr) t_xr = ms;
    }
    CUDA_CHECK(cudaMemcpy(h_B.data(), d_B, bytes, cudaMemcpyDeviceToHost));
    const int f_xr = pruefe_alles(h_B, H, W);
    std::printf("%-42s %10.4f %9.1f %10s\n", "Zeile  aus .x  (nicht coalesced)",
                t_xr, ec::gb_pro_s(bytes_kernel, t_xr), f_xr == 0 ? "OK" : "FEHLER");

    std::printf("\nVerhaeltnis (vertauscht / richtig): %.2f x\n", t_xr / t_xc);
    std::printf("Erwartung aus Aufgabe 11.5e: rund Faktor 8 (32 Byte geholt je 4 gebraucht),\n"
                "durch den L2-Cache in der Praxis oft weniger.\n");

    CUDA_CHECK(cudaFree(d_A));
    CUDA_CHECK(cudaFree(d_B));
    CUDA_CHECK(cudaFree(d_w));
    CUDA_CHECK(cudaFree(d_b));
    return (f_xc == 0 && f_xr == 0) ? 0 : 1;
}
