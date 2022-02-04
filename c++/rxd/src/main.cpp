#include <iostream>
#include "rxd.h"
#include "renderer.h"
#include <chrono>
#include <cassert>
#include <optional>

using namespace rxd::renderer;
using namespace rxd::utilities;
using namespace rxd::math;

struct Ray
{
    Vec3F64 a, b;

    Ray() : a(), b() { }
    Ray(const Vec3F64& _a, const Vec3F64& _b) : a(_a), b(_b) { }

    Vec3F64 GetPoint(double _t) const { return (1 - _t) * a + _t * b; }
    Vec3F64 GetDirection() const { return Normalize(b - a); }
};

struct RayIntersectable
{
    virtual ~RayIntersectable() { }
    virtual std::optional<double> GetIntersection(const Ray& _ray) = 0;
    virtual Color OnIntersect(const Vec3F64& _point, const std::vector<RayIntersectable*>& _intersectables) = 0;
};


struct RayIntersection
{
    RayIntersectable* intersectable;
    Vec3F64 point;
};

std::optional<RayIntersection> CastRay(const Ray& _ray, const std::vector<RayIntersectable*>& _intersectables)
{
    RayIntersectable* closest = nullptr;
    double t = 1.0;

    for (auto intersectable : _intersectables)
    {
        auto intersection = intersectable->GetIntersection(_ray);
        if (intersection.has_value() && intersection.value() >= 0.0 && intersection.value() <= t)
        {
            closest = intersectable;
            t = intersection.value();
        }
    }

    if (closest) { return RayIntersection{ closest, _ray.GetPoint(t) }; }
    else { return std::nullopt; }
}

struct Sphere : public RayIntersectable
{
    Vec3F64 center;
    double radius;
    Color color;

    Sphere() : center(), radius(1), color(Color::White()) {}
    Sphere(const Vec3F64& _center, double _radius, const Color& _color) : center(_center), radius(_radius), color(_color) { }

    std::optional<double> GetIntersection(const Ray& _ray) override
    {
        Vec3F64 centerToRay = _ray.a - center, rayBMinusA = _ray.b - _ray.a;
        double a = Dot(rayBMinusA, rayBMinusA);
        double b = 2.0 * Dot(centerToRay, rayBMinusA);
        double c = Dot(centerToRay, centerToRay) - radius * radius;
        double discriminant = b * b - 4 * a * c;

        if (discriminant < 0.0)
            return std::nullopt;

        double sqrtDiscriminant = std::sqrt(discriminant), aTimes2 = a * 2;
        return std::min((-b + sqrtDiscriminant) / aTimes2, (-b - sqrtDiscriminant) / aTimes2);
    }

    Color OnIntersect(const Vec3F64& _point, const std::vector<RayIntersectable*>& _intersectables) override
    {
        Vec3F64 lightPos = Vec3F64({ 0.0, 10.0, 0.0 });
        Ray pointToLight = Ray(Ray(_point, lightPos).GetPoint(0.001), lightPos);
        double ambience = 0.1, diffuse;

        auto intersection = CastRay(pointToLight, _intersectables);

        if (!intersection.has_value())
        {
            Vec3F64 normal = Normalize(_point - center), lightToPoint = Normalize(lightPos - _point);
            diffuse = std::min(1.0, std::max(0.0, Dot(normal, lightToPoint)) + ambience);
        }
        else { diffuse = ambience; }

        return Vec4F64({ 1.0, color.r / 255.0 * diffuse, color.g / 255.0 * diffuse, color.b / 255.0 * diffuse });
    }
};

struct Camera
{
    Vec3F64 position, up, right, forward;

    Camera(Vec3F64 _pos = Vec3F64::Zero(), Vec3F64 _up = Vec3F64({ 0.0, 1.0, 0.0 }), Vec3F64 _right = Vec3F64({ 1.0, 0.0, 0.0 }), double _fov = M_PI_2, double _zNear = 1.0, double _zFar = 100.0)
        : position(_pos), up(_up), right(_right), forward(Vec3F64({ 0.0, 0.0, -1.0 })), fov(_fov), aspectRatio(1.0), zNear(_zNear), zFar(_zFar)
    {
        UpdateViewportDimensions();
    }

    double GetFOV() const { return fov; }
    double GetAspectRatio() const { return aspectRatio; }
    double GetZNear() const { return zNear; }
    double GetZFar() const { return zFar; }

    void SetFOV(double _value)
    {
        fov = _value;
        UpdateViewportDimensions();
    }

    void SetAspectRatio(double _value)
    {
        aspectRatio = _value;
        UpdateViewportDimensions();
    }

    void SetZNear(double _value)
    {
        zNear = _value;
        UpdateViewportDimensions();
    }

    void SetZFar(double _value)
    {
        zFar = _value;
        UpdateViewportDimensions();
    }

    Ray GetRay(double _portX, double _portY) const
    {
        Vec3F64 portPoint = Normalize(forward + _portX * portHalfWidth * right + _portY * portHalfHeight * up);
        return Ray(position + portPoint * zNear, position + portPoint * zFar);
    }

private:
    double fov, aspectRatio, zNear, zFar, portHalfWidth, portHalfHeight;

    void UpdateViewportDimensions()
    {
        portHalfWidth = zNear * std::tan(fov / 2.0);
        portHalfHeight = portHalfWidth / aspectRatio;
    }
};

class Application : public rxd::Runnable
{
    rxd::Window window;
    rxd::Bitmap target;
    size_t UPS = 20, FPS = 20;

    Camera camera;

public:
    Application() : window("RXD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 600 / 16 * 9, 300), target()
    {

    }

    ~Application()
    {

    }

    void OnEvent(const SDL_Event& _event) override
    {
        window.HandleEvent(_event);

        switch (_event.type)
        {
        case SDL_EventType::SDL_QUIT: Quit(); break;
        case SDL_EventType::SDL_KEYDOWN:
        {

        } break;
        case SDL_EventType::SDL_KEYUP:
        {

        } break;
        default: break;
        }
    }

protected:
    void OnStart() override
    {
        std::cout << "Application Started" << std::endl;
        window.Show();
    }

    void OnRun() override
    {
        std::cout << "Application Running..." << std::endl;

        auto start = std::chrono::high_resolution_clock::now();
        size_t updates = 0, frames = 0, ups = 0, fps = 0;
        double updateFreq = 1.0 / UPS, renderFreq = 1.0 / FPS, nextUpdate = 0, nextRender = 0;

        while (IsRunning())
        {
            auto delta = (std::chrono::high_resolution_clock::now() - start).count() / 1000000000.0;

            if (delta >= 1.0)
            {
                ups = updates;
                updates = 0;
                nextUpdate = 0;
                updateFreq = 1.0 / UPS;

                fps = frames;
                frames = 0;
                nextRender = 0;
                renderFreq = 1.0 / FPS;

                delta -= 1.0;
                start = std::chrono::high_resolution_clock::now();

                std::cout << "UPS: " << ups << ", FPS: " << fps << std::endl;
            }
            else if (delta >= nextUpdate)
            {
                Update(delta);
                updates++;
                nextUpdate += updateFreq;
            }
            else if (delta >= nextRender)
            {
                Render();
                frames++;
                nextRender += renderFreq;
            }
        }
    }

    void OnQuit() override
    {
        std::cout << "Application Quitted" << std::endl;
    }

private:
    void Update(double _dt)
    {

    }

    void Render()
    {
        using namespace rxd::utilities;

        struct PixelRay
        {
            int64_t pixelX, pixelY;
            Ray ray;
        };

        static std::vector<PixelRay> pixelRays = std::vector<PixelRay>();

        Sphere sphere = Sphere(Vec3F64({ 0.0, 0.0, -5.0 }), 1.0, Color::Red());
        Sphere floor = Sphere(Vec3F64({ 0.0, -1000.0, 0.0 }), 999.0, Color::Green());
        std::vector<RayIntersectable*> world = { &sphere, &floor };

        if (target.GetWidth() != window.GetScreenWidth() || target.GetHeight() != window.GetScreenHeight())
        {
            target = rxd::Bitmap(window.GetScreenWidth(), window.GetScreenHeight());

            int64_t screenWidth = (int64_t)target.GetWidth(), screenHeight = (int64_t)target.GetHeight();
            int64_t screenWidthM1 = screenWidth - 1, screenHeightM1 = screenHeight - 1;

            camera.SetAspectRatio((double)screenWidth / screenHeight);
            pixelRays = std::vector<PixelRay>(screenWidth * screenHeight);

            for (int64_t yPix = screenHeightM1; yPix >= 0; --yPix)
            {
                double v = yPix / -(double)screenHeightM1 * 2.0 + 1.0;

                for (int64_t xPix = 0; xPix < screenWidth; ++xPix)
                {
                    double u = xPix / (double)screenWidthM1 * 2.0 - 1.0;

                    pixelRays[xPix + yPix * screenWidth] = PixelRay{ xPix, yPix, camera.GetRay(u, v) };
                }
            }
        }

        for (auto& pr : pixelRays)
        {
            Color color;

            auto intersection = CastRay(pr.ray, world);

            if (intersection.has_value()) { color = intersection.value().intersectable->OnIntersect(intersection.value().point, world); }
            else { color = Lerp(Vec4F64({ 1.0, 1.0, 1.0, 1.0 }), Vec4F64({ 1.0, 0.5, 0.7, 1.0 }), (pr.ray.GetDirection()[1] + 1.0) * 0.5); }

            target.SetPixel(pr.pixelX, pr.pixelY, color);
        }

        window.FlipScreenBuffer(target);
    }
};


int main()
{
    try
    {
        rxd::Init();

        Application app;

        rxd::Run(app);
    }
    catch (const std::exception& _e)
    {
        std::cerr << _e.what() << std::endl;
        return EXIT_FAILURE;
    }

    rxd::CleanUp();
    return EXIT_SUCCESS;
}