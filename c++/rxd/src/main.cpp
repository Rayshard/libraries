#include <iostream>
#include "rxd.h"
#include "renderer.h"
#include "input.h"
#include <chrono>
#include <cassert>
#include <optional>

using namespace rxd::renderer;
using namespace rxd::utilities;
using namespace rxd::math;
using namespace rxd::input;

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

std::optional<RayIntersection> CastRay(const Ray& _ray, const std::vector<RayIntersectable*>& _intersectables, bool _findClosest)
{
    if (_findClosest)
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
    else
    {
        for (auto intersectable : _intersectables)
        {
            auto intersection = intersectable->GetIntersection(_ray);
            if (intersection.has_value() && intersection.value() >= 0.0 && intersection.value() <= 1.0)
                return RayIntersection{ intersectable, _ray.GetPoint(intersection.value()) };
        }

        return std::nullopt;
    }
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

        auto intersection = CastRay(pointToLight, _intersectables, false);

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
    Vec3F64 position;
    Quaternion rotation;
    Vec3F64 up, right, forward;
    double fov, zNear, zFar;

    Camera(Vec3F64 _pos = Vec3F64::Zero(), Quaternion _rot = Quaternion(), double _fov = M_PI_2, double _zNear = 1.0, double _zFar = 100.0) :
        position(_pos),
        rotation(_rot),
        fov(_fov),
        zNear(_zNear),
        zFar(_zFar)
    {

    }

    void Capture(rxd::Bitmap& target, const std::vector<RayIntersectable*>& _scene) const
    {
        //Vec3F64 up = Quaternion::Up(rotation), right = Quaternion::Right(rotation), forward = Quaternion::Forward(rotation);
        Vec3F64 up = Rotate(Vec3F64({ 0.0, 1.0, 0.0 }), rotation);
        Vec3F64 right = Rotate(Vec3F64({ 1.0, 0.0, 0.0 }), rotation);
        Vec3F64 forward = Rotate(Vec3F64({ 0.0, 0.0, 1.0 }), rotation);

        int64_t targetWidth = int64_t(target.GetWidth()), targetHeight = int64_t(target.GetHeight());
        int64_t targetWidthM1 = targetWidth - 1, targetHeightM1 = targetHeight - 1;

        double aspectRatio = targetWidth / double(targetHeight);
        double portHalfWidth = zNear * std::tan(fov / 2.0);
        double portHalfHeight = portHalfWidth / aspectRatio;

        for (int64_t yPix = targetHeightM1; yPix >= 0; --yPix)
        {
            double v = yPix / -(double)targetHeightM1 * 2.0 + 1.0;
            Vec3F64 portPointYDir = v * portHalfHeight * up;

            for (int64_t xPix = 0; xPix < targetWidth; ++xPix)
            {
                double u = xPix / (double)targetWidthM1 * 2.0 - 1.0;
                Vec3F64 portPointXDir = u * portHalfWidth * right;

                Color color;
                Vec3F64 portPoint = Normalize(forward + portPointXDir + portPointYDir);
                // double near = std::sqrt(zNear*zNear + LengthSquared(portPointXDir + portPointYDir));
                // double far = std::sqrt(zFar*zFar + LengthSquared(portPointXDir + portPointYDir) * (zFar / zNear));
                // Ray ray = Ray(position + portPoint * near, position + portPoint * far);
                Ray ray = Ray(position + portPoint * zNear, position + portPoint * zFar);
                auto intersection = CastRay(ray, _scene, true);

                if (intersection.has_value()) { color = intersection.value().intersectable->OnIntersect(intersection.value().point, _scene); }
                else { color = Lerp(Vec4F64({ 1.0, 1.0, 1.0, 1.0 }), Vec4F64({ 1.0, 0.5, 0.7, 1.0 }), (ray.GetDirection()[1] + 1.0) * 0.5); }

                target.SetPixel(xPix, yPix, color);
            }
        }
    }
};

class Application : public rxd::Runnable
{
    rxd::Window window;
    Keyboard keyboard;
    Mouse mouse;

    rxd::Bitmap target;

    size_t UPS = 20, FPS = 2000;

    Camera camera;
    bool lockedMouse = true;

public:
    Application() :
        window("RXD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 600 / 16 * 9, 300),
        keyboard(),
        target()
    {
        
    }

    ~Application()
    {

    }

    void OnEvent(const SDL_Event& _event) override
    {
        window.HandleEvent(_event);
        keyboard.HandleEvent(_event);
        mouse.HandleEvent(_event);

        switch (_event.type)
        {
        case SDL_EventType::SDL_QUIT: Quit(); break;
        default: break;
        }
    }

protected:
    void OnStart() override
    {
        std::cout << "Application Started" << std::endl;
        window.Show();
        rxd::SetConstrainedMouse(true);
    }

    void OnRun() override
    {
        std::cout << "Application Running..." << std::endl;

        auto start = std::chrono::high_resolution_clock::now();
        auto lastUpdate = start, lastRender = start;
        size_t updates = 0, frames = 0, ups = 0, fps = 0;
        double updateFreq = 1.0 / UPS, renderFreq = 1.0 / FPS;

        while (IsRunning())
        {
            auto now = std::chrono::high_resolution_clock::now();
            double timeDelta = (now - start).count() / 1000000000.0;
            double updateDelta = (now - lastUpdate).count() / 1000000000.0;
            double renderDelta = (now - lastRender).count() / 1000000000.0;

            if (timeDelta >= 1.0)
            {
                ups = updates;
                updates = 0;
                updateFreq = 1.0 / UPS;

                fps = frames;
                frames = 0;
                renderFreq = 1.0 / FPS;

                start = std::chrono::high_resolution_clock::now();

                std::cout << "UPS: " << ups << ", FPS: " << fps << std::endl;
            }
            else if (updateDelta >= updateFreq)
            {
                lastUpdate = now;

                Update(updateDelta);
                updates++;
            }
            else if (renderDelta >= renderFreq)
            {
                lastRender = now;

                Render();
                frames++;
            }
        }
    }

    void OnQuit() override
    {
        std::cout << "Application Quitted" << std::endl;
        rxd::SetConstrainedMouse(false);
    }

private:
    void Update(double _dt)
    {
        keyboard.Update();
        mouse.Update();

        if (keyboard.IsKeyDown(Key::SDLK_ESCAPE))
        {
            Quit();
            return;
        }

        if (mouse.WasButtonPressed(MouseButton::MIDDLE))
            rxd::SetConstrainedMouse(lockedMouse = !lockedMouse);

        static Vec2UI32 lastMosPos;
        static double yaw, pitch;

        double cameraSpeed = 2.0, cameraSensitivity = 0.01;

        if (mouse.GetPosition() != lastMosPos)
        {
            if (lockedMouse)
            {
                yaw += mouse.GetDY() * cameraSensitivity;
                pitch += mouse.GetDX() * cameraSensitivity;
            }

            lastMosPos = mouse.GetPosition();
        }

        camera.rotation = Quaternion::FromEulerAngles(0.0, pitch, yaw);

        Vec3F64 right = Rotate(Vec3F64({ 1.0, 0.0, 0.0 }), camera.rotation);
        Vec3F64 forward = Rotate(Vec3F64({ 0.0, 0.0, 1.0 }), camera.rotation);
        Vec3F64 offset;

        if (keyboard.IsKeyDown(Key::SDLK_a))
            offset -= right;
        if (keyboard.IsKeyDown(Key::SDLK_w))
            offset += forward;
        if (keyboard.IsKeyDown(Key::SDLK_s))
            offset -= forward;
        if (keyboard.IsKeyDown(Key::SDLK_d))
            offset += right;
        if (keyboard.IsKeyDown(Key::SDLK_SPACE))
            offset[1]++;
        if (keyboard.IsKeyDown(Key::SDLK_LSHIFT) || keyboard.IsKeyDown(Key::SDLK_RSHIFT))
            offset[1]--;

        if (offset != Vec3F64::Zero())
            camera.position += Normalize(offset) * _dt * cameraSpeed;
    }

    void Render()
    {
        using namespace rxd::utilities;

        Sphere sphere = Sphere(Vec3F64({ 0.0, 0.0, 5.0 }), 1.0, Color::Red());
        Sphere floor = Sphere(Vec3F64({ 0.0, -1000.0, 0.0 }), 999.0, Color::Green());
        std::vector<RayIntersectable*> scene = { &sphere, &floor };

        if (target.GetWidth() != window.GetScreenWidth() || target.GetHeight() != window.GetScreenHeight())
            target = rxd::Bitmap(window.GetScreenWidth(), window.GetScreenHeight());

        camera.Capture(target, scene);
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