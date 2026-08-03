// Kapitel 11, Aufgaben 11.4 und 11.8 -- axpy auf der GPU:  y <- alpha*x + y
//
//   nvcc -std=c++17 -O2 -arch=sm_80 axpy.cu -o axpy
//
//   ./axpy              Groessen-Sweep: H->D, Kernel und D->H getrennt messen
//   ./axpy <n>          eine Groesse ausfuehrlich
//   ./axpy config <n>   Aufgabe 11.8b: vier Grid-Konfigurationen vergleichen
//
// Die Rechenkerne sind bewusst rohe Indexschleifen -- so stehen sie in der
// Klausur. Modernes C++ nur drumherum (std::vector, RAII fuer die Events).

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "cuda_check.hpp"

// ---------------------------------------------------------------- Kernels ---

// Aufgabe 11.4a: ein Thread bearbeitet genau ein Element.
__global__ void axpy(float* y, const float* x, float alpha, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        y[i] = alpha * x[i] + y[i];
}

// Aufgabe 11.8a: Grid-Stride-Loop -- entkoppelt n von der Startkonfiguration.
// Laeuft korrekt mit <<<1,1>>> genauso wie mit <<<40000,256>>>.
__global__ void axpy_stride(float* y, const float* x, float alpha, int n) {
    int i      = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;   // = Gesamtzahl der Threads im Grid
    for (; i < n; i += stride)
        y[i] = alpha * x[i] + y[i];
}

// ------------------------------------------------------------- Host-Seite ---

static void fuelle(std::vector<float>& x, std::vector<float>& y) {
    const int n = static_cast<int>(x.size());
    for (int i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i % 1000) * 0.5f;
        y[i] = static_cast<float>(i % 7);
    }
}

// Serielle Referenz auf dem Host -- die einzige verlaessliche Pruefung.
static int pruefe(const std::vector<float>& y_gpu, const std::vector<float>& x,
                  const std::vector<float>& y0, float alpha) {
    const int n = static_cast<int>(x.size());
    int fehler = 0;
    for (int i = 0; i < n; ++i) {
        const float soll = alpha * x[i] + y0[i];
        if (!ec::nahe(y_gpu[i], soll)) ++fehler;
    }
    return fehler;
}

struct Zeiten {
    float h2d = 0.0f;   // Host -> Device, in ms
    float ker = 0.0f;   // Kernel,         in ms
    float d2h = 0.0f;   // Device -> Host, in ms
};

// Ein vollstaendiger Durchlauf fuer eine Groesse. Gibt die drei Zeiten zurueck
// und schreibt die Zahl der Abweichungen nach *fehler.
static Zeiten lauf(int n, float alpha, int* fehler, int wiederholungen = 5) {
    const size_t bytes = static_cast<size_t>(n) * sizeof(float);

    std::vector<float> h_x(n), h_y(n), h_y0(n), h_erg(n);
    fuelle(h_x, h_y);
    h_y0 = h_y;

    // 1. allokieren
    float *d_x = nullptr, *d_y = nullptr;
    CUDA_CHECK(cudaMalloc(&d_x, bytes));
    CUDA_CHECK(cudaMalloc(&d_y, bytes));

    const int threads = 256;
    const int blocks  = (n + threads - 1) / threads;   // aufrunden!

    // Aufwaermlauf: der erste CUDA-Aufruf im Prozess initialisiert den Kontext
    // (mehrere hundert ms) -- der gehoert nicht in die Messung.
    CUDA_CHECK(cudaMemcpy(d_x, h_x.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_y, h_y.data(), bytes, cudaMemcpyHostToDevice));
    axpy<<<blocks, threads>>>(d_y, d_x, alpha, n);
    CUDA_CHECK_KERNEL();

    ec::GpuTimer t;
    Zeiten best{1e30f, 1e30f, 1e30f};

    for (int r = 0; r < wiederholungen; ++r) {
        // 2. kopieren Host -> Device
        t.start();
        CUDA_CHECK(cudaMemcpy(d_x, h_x.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(d_y, h_y.data(), bytes, cudaMemcpyHostToDevice));
        const float h2d = t.stop_ms();

        // 3. Kernel starten
        t.start();
        axpy<<<blocks, threads>>>(d_y, d_x, alpha, n);
        const float ker = t.stop_ms();
        CUDA_CHECK_KERNEL();

        // 4. kopieren Device -> Host
        t.start();
        CUDA_CHECK(cudaMemcpy(h_erg.data(), d_y, bytes, cudaMemcpyDeviceToHost));
        const float d2h = t.stop_ms();

        // Minimum statt Mittelwert: Stoerungen machen eine Messung immer
        // langsamer, nie schneller.
        if (h2d < best.h2d) best.h2d = h2d;
        if (ker < best.ker) best.ker = ker;
        if (d2h < best.d2h) best.d2h = d2h;
    }

    *fehler = pruefe(h_erg, h_x, h_y0, alpha);

    // 5. freigeben
    CUDA_CHECK(cudaFree(d_x));
    CUDA_CHECK(cudaFree(d_y));
    return best;
}

static void kopf() {
    std::printf("%12s %10s %10s %10s %10s %9s %8s\n", "n", "H->D [ms]",
                "Kernel[ms]", "D->H [ms]", "gesamt", "Kern GB/s", "Pruefung");
    std::printf("%s\n", std::string(76, '-').c_str());
}

static void zeile(int n, const Zeiten& z, int fehler) {
    // axpy bewegt pro Element 12 Byte im Device-Speicher: x lesen, y lesen,
    // y schreiben.
    const double bytes_kernel = 12.0 * static_cast<double>(n);
    const double gesamt       = z.h2d + z.ker + z.d2h;
    std::printf("%12d %10.3f %10.4f %10.3f %10.3f %9.1f %8s\n", n, z.h2d, z.ker,
                z.d2h, gesamt, ec::gb_pro_s(bytes_kernel, z.ker),
                fehler == 0 ? "OK" : "FEHLER");
}

// Aufgabe 11.8b: derselbe Grid-Stride-Kernel mit vier Konfigurationen.
static void konfig_vergleich(int n, float alpha) {
    const size_t bytes = static_cast<size_t>(n) * sizeof(float);

    std::vector<float> h_x(n), h_y(n), h_y0(n), h_erg(n);
    fuelle(h_x, h_y);
    h_y0 = h_y;

    float *d_x = nullptr, *d_y = nullptr;
    CUDA_CHECK(cudaMalloc(&d_x, bytes));
    CUDA_CHECK(cudaMalloc(&d_y, bytes));
    CUDA_CHECK(cudaMemcpy(d_x, h_x.data(), bytes, cudaMemcpyHostToDevice));

    int sms = 0;
    CUDA_CHECK(cudaDeviceGetAttribute(&sms, cudaDevAttrMultiProcessorCount, 0));

    struct Konfig { const char* name; int blocks; int threads; };
    const Konfig k[] = {
        {"<<<1, 1>>>            1 Thread",        1,               1},
        {"<<<1, 256>>>          1 Block",         1,             256},
        {"<<<4*SM, 256>>>       ausgelastet",     4 * sms,       256},
        {"<<<ceil(n/256), 256>>> 1 Elem/Thread", (n + 255) / 256, 256},
    };

    std::printf("%-34s %10s %9s %8s %10s\n", "Konfiguration", "T [ms]",
                "GB/s", "Pruefung", "Elem/Thr");
    std::printf("%s\n", std::string(76, '-').c_str());

    ec::GpuTimer t;
    for (const Konfig& c : k) {
        // <<<1,1>>> braucht bei n = 10^7 mehrere Sekunden -- einmal reicht.
        CUDA_CHECK(cudaMemcpy(d_y, h_y.data(), bytes, cudaMemcpyHostToDevice));
        t.start();
        axpy_stride<<<c.blocks, c.threads>>>(d_y, d_x, alpha, n);
        const float ms = t.stop_ms();
        CUDA_CHECK_KERNEL();

        CUDA_CHECK(cudaMemcpy(h_erg.data(), d_y, bytes, cudaMemcpyDeviceToHost));
        const int fehler = pruefe(h_erg, h_x, h_y0, alpha);

        const double gesamt_threads = static_cast<double>(c.blocks) * c.threads;
        std::printf("%-34s %10.3f %9.1f %8s %10.1f\n", c.name, ms,
                    ec::gb_pro_s(12.0 * n, ms), fehler == 0 ? "OK" : "FEHLER",
                    static_cast<double>(n) / gesamt_threads);
    }

    CUDA_CHECK(cudaFree(d_x));
    CUDA_CHECK(cudaFree(d_y));
}

int main(int argc, char** argv) {
    const float alpha = 2.5f;
    ec::geraet_ausgeben();

    if (argc >= 2 && std::string(argv[1]) == "config") {
        const int n = (argc >= 3) ? std::atoi(argv[2]) : 10000000;
        std::printf("Aufgabe 11.8b -- Grid-Stride-Loop, n = %d\n\n", n);
        konfig_vergleich(n, alpha);
        return 0;
    }

    if (argc >= 2) {
        const int n = std::atoi(argv[1]);
        if (n <= 0) { std::fprintf(stderr, "n muss positiv sein\n"); return 1; }
        kopf();
        int fehler = 0;
        zeile(n, lauf(n, alpha, &fehler), fehler);
        return fehler == 0 ? 0 : 1;
    }

    // Aufgabe 11.4e: Sweep ueber mehrere Groessenordnungen.
    std::printf("Aufgabe 11.4e -- H->D, Kernel und D->H getrennt (Minimum aus 5 Laeufen)\n\n");
    kopf();
    int fehler_gesamt = 0;
    // Obergrenze 2^26: das sind 4 Host-Vektoren a 268 MB plus 537 MB auf dem
    // Device -- mehr passt nicht in die im Job-Skript angeforderten 8 GB.
    for (int e = 10; e <= 26; ++e) {
        const int n = 1 << e;
        int fehler  = 0;
        const Zeiten z = lauf(n, alpha, &fehler);
        zeile(n, z, fehler);
        fehler_gesamt += fehler;
    }
    return fehler_gesamt == 0 ? 0 : 1;
}
