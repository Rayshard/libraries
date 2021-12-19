#include <iostream>
#include "rxd.h"
#include <chrono>

struct ColorVertex : public rxd::Renderer::Vertex<6>
{
    ColorVertex(rxd::Utilities::Vec2F64 _pos, rxd::Utilities::Vec4F64 _col) : Vertex(_pos)
    {
        SetColor(_col);
    }

    ColorVertex() : ColorVertex(rxd::Utilities::Vec2F64(), rxd::Utilities::Vec4F64(0, 0, 0, 1)) { }

    ColorVertex(const Vertex& _v) : Vertex(_v) { }

    rxd::Utilities::Vec4F64& GetColor() { return GetComponent<rxd::Utilities::Vec4F64, 2>(); }
    const rxd::Utilities::Vec4F64& GetColor() const { return GetComponent<rxd::Utilities::Vec4F64, 2>(); }
    void SetColor(const rxd::Utilities::Vec4F64& _value) { return SetComponent<rxd::Utilities::Vec4F64, 2>(_value); }

    static PixelShader DefaultPS() { return [](const ColorVertex& _v) { return _v.GetColor(); }; }
};

struct TextureVertex : public rxd::Renderer::Vertex<4>
{
    TextureVertex(rxd::Utilities::Vec2F64 _pos, rxd::Utilities::Vec2F64 _texCoords) : Vertex(_pos)
    {
        SetTexCoords(_texCoords);
    }

    TextureVertex() : TextureVertex(rxd::Utilities::Vec2F64(), rxd::Utilities::Vec2F64()) { }

    TextureVertex(const Vertex& _v) : Vertex(_v) { }

    rxd::Utilities::Vec2F64& GetTexCoords() { return GetComponent<rxd::Utilities::Vec2F64, 2>(); }
    const rxd::Utilities::Vec2F64& GetTexCoords() const { return GetComponent<rxd::Utilities::Vec2F64, 2>(); }
    void SetTexCoords(const rxd::Utilities::Vec2F64& _value) { return SetComponent<rxd::Utilities::Vec2F64, 2>(_value); }

    static PixelShader DefaultPS(rxd::Image* _texture)
    {
        return [_texture](const TextureVertex& _v)
        {
            auto coords = _v.GetTexCoords();
            int64_t x = std::ceil(coords.x * (_texture->GetWidth() - 1));
            int64_t y = std::ceil(coords.y * (_texture->GetHeight() - 1));
            return _texture->GetPixel(x, y);
        };
    }
};

class Application : public rxd::Runnable
{
    rxd::Window window;
    rxd::Renderer::Target* screen;
    size_t UPS = 20, FPS = 10000;

    bool wireframe = false;

    rxd::Bitmap* texture;
    TextureVertex::PixelShader ps;

public:
    Application() : window("RXD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 600 / 16 * 9)
    {
        screen = new rxd::Renderer::Target(new rxd::Screen(window, window.GetWidth() / 3, window.GetHeight() / 3), true);
    }

    ~Application()
    {
        delete screen;
        delete texture;
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

        texture = new rxd::Bitmap("/Users/rayshardthompson/Documents/GitHub/libraries/c++/rxd/texture.png");
        texture->SetPixel(0, 0, rxd::Utilities::Color::Red());

        ps = TextureVertex::DefaultPS(texture);
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

        screen->ClearDepthBuffer();
        screen->As<rxd::Screen>().Fill(Color{ 128, 128, 128, 255 });

        //ColorVertex::VertexShader<ColorVertex> vs = [](const ColorVertex& _v) { return _v; };
        TextureVertex::VertexShader<TextureVertex> vs = [](const TextureVertex& _v) { return _v; };

        // auto v1 = ColorVertex({ 0.25, 0.25 }, Color::Red().ToVec4F64());
        // auto v2 = ColorVertex({ 0.75, 0.25 }, Color::Green().ToVec4F64());
        // auto v3 = ColorVertex({ 0.75, 0.75 }, Color::Blue().ToVec4F64());
        // auto v4 = ColorVertex({ 0.25, 0.75 }, Color::White().ToVec4F64());

        auto v1 = TextureVertex({ 0.25, 0.25 }, { 0.0, 0.0 });
        auto v2 = TextureVertex({ 0.75, 0.25 }, { 1.0, 0.0 });
        auto v3 = TextureVertex({ 0.75, 0.75 }, { 1.0, 1.0 });
        auto v4 = TextureVertex({ 0.25, 0.75 }, { 0.0, 1.0 });


        for (int i = 0; i < 1; i++)
        {
            rxd::Renderer::Rasterize(screen, v1, v2, v3, vs, ps);
            rxd::Renderer::Rasterize(screen, v1, v3, v4, vs, ps);
        }

        screen->As<rxd::Screen>().Update();
        window.FlipScreen(screen->As<rxd::Screen>());
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