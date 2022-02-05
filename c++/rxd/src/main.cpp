#include <iostream>
#include "rxd.h"
#include "window.h"
#include "renderer.h"
#include "input.h"
#include <chrono>
#include <cassert>
#include <optional>

using namespace rxd::window;
using namespace rxd::renderer;
using namespace rxd::math;
using namespace rxd::math::raytracing;
using namespace rxd::input;
using namespace rxd::graphics;

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

    void Capture(Bitmap& target, const std::vector<Renderable*>& _scene) const
    {
        std::vector<Intersectable*> intersectables;
        intersectables.reserve(_scene.size());

        for (auto& renderable : _scene)
            intersectables.push_back(renderable);

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
                auto intersection = TryIntersect(ray, intersectables, true);

                if (intersection.has_value())
                {
                    Vec3F64 point = intersection.value().point;
                    Renderable* intersectable = (Renderable*)intersection.value().intersectable;
                    Color intersectableColor = intersectable->GetColor(point);

                    Vec3F64 lightPos = Vec3F64({ 0.0, 10.0, 0.0 });
                    Ray pointToLight = Ray(Ray(point, lightPos).GetPoint(0.001), lightPos);
                    double ambience = 0.1, diffuse = 0.1;

                    auto lightRayIntersection = TryIntersect(pointToLight, intersectables, false);

                    if (!lightRayIntersection.has_value())
                    {
                        Vec3F64 normal = intersectable->GetNormal(point), lightToPoint = Normalize(lightPos - point);
                        diffuse = std::min(1.0, std::max(0.0, Dot(normal, lightToPoint)) + ambience);
                    }
                    else { diffuse = ambience; }

                    color = Vec4F64({ 1.0, intersectableColor.r / 255.0 * diffuse, intersectableColor.g / 255.0 * diffuse, intersectableColor.b / 255.0 * diffuse });
                }
                else { color = Lerp(Vec4F64({ 1.0, 1.0, 1.0, 1.0 }), Vec4F64({ 1.0, 0.5, 0.7, 1.0 }), (ray.GetDirection()[1] + 1.0) * 0.5); }

                target.SetPixel(xPix, yPix, color);
            }
        }
    }
};

class Application : public rxd::Runnable
{
    Window window;
    Keyboard keyboard;
    Mouse mouse;

    Bitmap target;

    size_t UPS = 20, FPS = 30;

    Camera camera;
    bool lockedMouse = false;

    std::vector<Renderable*> scene;

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
    }

protected:
    void OnStart() override
    {
        std::cout << "Application Started" << std::endl;
        window.Show();

        scene.push_back(new Sphere(Vec3F64({ 0.0, 0.0, 5.0 }), 1.0, Color::Red()));
        scene.push_back(new Plane(Vec3F64({ 0.0, 1.0, 0.0 }), -2.0, Color::Green()));
        scene.push_back(new Triangle(Vec3F64({ -3.0, 1.0, 5.0 }), Vec3F64({ -2.0, 1.0, 5.0 }), Vec3F64({ -2.5, 0.0, 5.0 })));
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

        for(auto renderable : scene)
            delete renderable;
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
        if (target.GetWidth() != window.GetScreenWidth() || target.GetHeight() != window.GetScreenHeight())
            target = Bitmap(window.GetScreenWidth(), window.GetScreenHeight());

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