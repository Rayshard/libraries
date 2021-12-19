#include <iostream>
#include "rxd.h"
#include <chrono>

struct ColorVertex : public rxd::Renderer::Vertex
{
    rxd::Utilities::Vec2F64 position;
    rxd::Utilities::Vec4F64 color;

    ColorVertex(rxd::Utilities::Vec2F64 _pos, rxd::Utilities::Vec4F64 _col) : position(_pos), color(_col) { }
    ColorVertex() : ColorVertex(rxd::Utilities::Vec2F64(), rxd::Utilities::Vec4F64(0, 0, 0, 1)) {}

    double GetX() const override { return position.x; }
    double GetY() const override { return position.y; }

    static ColorVertex Lerp(const ColorVertex& _start, const ColorVertex& _end, double _amt)
    {
        auto position = rxd::Utilities::Vec2F64::Lerp(_start.position, _end.position, _amt);
        auto color = rxd::Utilities::Vec4F64::Lerp(_start.color, _end.color, _amt);
        return ColorVertex(position, color);
    }
};

class Application : public rxd::Runnable
{
    rxd::Window window;
    rxd::Screen* screen;
    size_t UPS = 20, FPS = 10000;

    bool wireframe = false;

public:
    Application() : window("RXD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 600 / 16 * 9), screen(nullptr)
    {
        screen = new rxd::Screen(window, window.GetWidth() / 3, window.GetHeight() / 3);
    }

    ~Application()
    {
        delete screen;
    }

    void OnEvent(const SDL_Event& _event) override
    {
        switch (_event.type)
        {
        case SDL_EventType::SDL_QUIT: Quit(); break;
        case SDL_EventType::SDL_KEYDOWN:
        {
            if (_event.key.keysym.sym == SDL_KeyCode::SDLK_w)
                wireframe = true;
        } break;
        case SDL_EventType::SDL_KEYUP:
        {
            if (_event.key.keysym.sym == SDL_KeyCode::SDLK_w)
                wireframe = false;
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
        using namespace rxd::Utilities;

        screen->Fill(Color{ 128, 128, 128, 255 });

        rxd::Renderer::VertexShader<ColorVertex, ColorVertex> vs = [](const ColorVertex& _v) { return _v; };
        rxd::Renderer::PixelShader<ColorVertex> ps = [](const ColorVertex& _v) { return _v.color; };

        // auto v1 = ColorVertex({ 0, 0 }, Color::Red().ToVec4F64());
        // auto v2 = ColorVertex({ (double)screen->GetWidth() - 1, 0 }, Color::Green().ToVec4F64());
        // auto v3 = ColorVertex({ (double)screen->GetWidth() - 1, (double)screen->GetHeight() - 1 }, Color::Blue().ToVec4F64());
        // auto v4 = ColorVertex({ 0, (double)screen->GetHeight() }, Color::White().ToVec4F64());

        auto v1 = ColorVertex({ 0.25, 0.25 }, Color::Red().ToVec4F64());
        auto v2 = ColorVertex({ 0.75, 0.25 }, Color::Green().ToVec4F64());
        auto v3 = ColorVertex({ 0.75, 0.75 }, Color::Blue().ToVec4F64());
        auto v4 = ColorVertex({ 0.25, 0.25 }, Color::Blue().ToVec4F64());
        auto v5 = ColorVertex({ 0.75, 0.75 }, Color::Green().ToVec4F64());
        auto v6 = ColorVertex({ 0.25, 0.75 }, Color::Red().ToVec4F64());

        for(int i = 0; i < 50; i++)
        rxd::Renderer::Rasterize(screen, v1, v2, v3, vs, ps);

        if(wireframe)
        rxd::Renderer::Rasterize(screen, v4, v5, v6, vs, ps);

        screen->Update();
        window.FlipScreen(*screen);
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