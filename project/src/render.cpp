#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// -------- Vec3 --------
struct Vec3
{
    double x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3 &o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3 &o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(double k) const { return Vec3(x * k, y * k, z * k); }

    double dot(const Vec3 &o) const { return x * o.x + y * o.y + z * o.z; }

    Vec3 cross(const Vec3 &o) const
    {
        return Vec3(
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x);
    }

    double length() const { return std::sqrt(dot(*this)); }

    Vec3 normalize() const
    {
        double l = length();
        if (l == 0)
            return Vec3(0, 0, 0);
        return (*this) * (1.0 / l);
    }
};

inline Vec3 operator*(double k, const Vec3 &v) { return v * k; }

// -------- Hit --------
struct Hit
{
    double alpha;
    Vec3 normal;
    Vec3 color;
};

// -------- Triangle --------
struct Triangle
{
    Vec3 T1, T2, T3, color;

    Triangle(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &col)
        : T1(a), T2(b), T3(c), color(col) {}

    std::optional<Hit> intersect(const Vec3 &C, const Vec3 &D) const
    {
        const double eps = 1e-6;
        Vec3 E1 = T2 - T1;
        Vec3 E2 = T3 - T1;
        Vec3 U = D.cross(E2);
        double beta = E1.dot(U);
        if (-eps < beta && beta < eps)
            return std::nullopt;
        double beta_inv = 1.0 / beta;
        Vec3 V = C - T1;
        double lambda2 = V.dot(U) * beta_inv;
        if (lambda2 < 0 || lambda2 > 1)
            return std::nullopt;
        double lambda3 = D.dot(V.cross(E1)) * beta_inv;
        if (lambda3 < 0 || lambda2 + lambda3 > 1)
            return std::nullopt;
        double alpha = E2.dot(V.cross(E1)) * beta_inv;
        if (alpha <= eps)
            return std::nullopt;
        Vec3 normal = E1.cross(E2).normalize();
        return Hit{alpha, normal, color};
    }
};

// -------- Scene --------
struct Scene
{
    std::vector<Triangle> objects;

    void add_triangles(const std::vector<Triangle> &objs)
    {
        objects.insert(objects.end(), objs.begin(), objs.end());
    }

    Vec3 trace(const Vec3 &C, const Vec3 &D, const Vec3 &Light) const
    {
        std::optional<Hit> closest;
        for (const auto &obj : objects)
        {
            auto hit = obj.intersect(C, D);
            if (hit && (!closest || hit->alpha < closest->alpha))
            {
                closest = hit;
            }
        }
        if (!closest)
            return Vec3(1.0, 1.0, 1.0);
        Vec3 hit_point = C + D * closest->alpha;
        Vec3 L = (hit_point - Light).normalize();
        double diffuse = std::max(0.0, -closest->normal.dot(L));
        return closest->color * diffuse;
    }
};

// -------- STL Loader (ASCII) --------
std::vector<Triangle> load_stl(const std::string &path, const Vec3 &color)
{
    std::vector<Triangle> tris;
    std::ifstream f(path);
    if (!f)
        throw std::runtime_error("STL file not found");

    std::vector<Vec3> verts;
    std::string line;
    while (std::getline(f, line))
    {
        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if (tag == "vertex")
        {
            double a, b, c;
            if (iss >> a >> b >> c)
            {
                verts.emplace_back(a, b, c);
                if (verts.size() == 3)
                {
                    tris.emplace_back(verts[0], verts[1], verts[2], color);
                    verts.clear();
                }
            }
        }
    }
    return tris;
}

// -------- Write 2D graphic --------
void write_ppm(const std::string &fname, int width, int height,
               const std::vector<std::array<int, 3>> &pixels)
{
    std::ofstream f(fname);
    f << "P3\n"
      << width << " " << height << "\n255\n";
    for (const auto &p : pixels)
    {
        f << p[0] << " " << p[1] << " " << p[2] << "\n";
    }
}

// -------- Render --------
void render(int width, int height, const std::string &stl_file,
            const Vec3 &C, const Vec3 &cam_lookat, const Vec3 &cam_up,
            const Vec3 &light)
{
    Vec3 forward = (cam_lookat - C).normalize();
    Vec3 right = forward.cross(cam_up).normalize();
    Vec3 actual_up = right.cross(forward).normalize();
    double fov = M_PI / 3;
    double aspect = static_cast<double>(width) / height;

    Scene scene;
    try
    {
        auto tris = load_stl(stl_file, Vec3(0.8, 0.8, 0.8));
        scene.add_triangles(tris);
        std::printf("Loaded %zu triangles from STL\n", scene.objects.size());
    }
    catch (const std::exception &)
    {
        std::printf("STL file not found - rendering empty scene\n");
    }

    std::vector<std::array<int, 3>> pixels;
    pixels.reserve(static_cast<size_t>(width) * height);
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            double x = (2 * (i + 0.5) / (width + 1) - 1) * std::tan(fov / 2) * aspect;
            double y = -(2 * (j + 0.5) / (height + 1) - 1) * std::tan(fov / 2);
            Vec3 D = (right * x + actual_up * y + forward).normalize();
            Vec3 color = scene.trace(C, D, light);
            int r = std::max(0, std::min(255, static_cast<int>(255 * color.x)));
            int g = std::max(0, std::min(255, static_cast<int>(255 * color.y)));
            int b = std::max(0, std::min(255, static_cast<int>(255 * color.z)));
            pixels.push_back({r, g, b});
        }
    }

    write_ppm("output.ppm", width, height, pixels);
}

int main()
{
    int width = 600;
    int height = 600;
    std::string stl_file = "car.stl";
    Vec3 cam_pos(20, -20, 10);
    Vec3 cam_lookat(0, 0, 3);
    Vec3 cam_up(0, 0, 1);
    Vec3 light_source(20, -20, 5);
    render(width, height, stl_file, cam_pos, cam_lookat, cam_up, light_source);
    return 0;
}
