#include "rxd.h"
#include <cmath>

#define CHECK_SDL(value, message) do { if (!(value)) throw std::runtime_error(std::string(message) + " Error:\n" + SDL_GetError()); } while(false)

namespace rxd
{
    static bool initialized = false;

    void Init()
    {
        CHECK_SDL(SDL_Init(SDL_INIT_EVERYTHING) >= 0, "SDL was not initialized properly!");
        initialized = true;
    }

    void Run(Runnable& _runnable)
    {
        if (!initialized) { throw std::runtime_error("RXD is not initialized! Call rxd::Init() to initialize RXD."); }

        SDL_Event event;
        auto thread = _runnable.Start();

        while (_runnable.IsRunning())
        {
            if (SDL_PollEvent(&event))
                _runnable.OnEvent(event);
        }

        thread.join();
    }

    void CleanUp()
    {
        if (!initialized)
            throw std::runtime_error("RXD is not initialized! Call rxd::Init() to initialize RXD.");

        initialized = false;
        SDL_Quit();
    }

#pragma region Runnable
    Runnable::Runnable() : running(false) { }

    std::thread Runnable::Start()
    {
        if (running) { throw std::runtime_error("Runnable is already running!"); }
        else { running = true; }

        OnStart();
        return std::thread(&Runnable::OnRun, this);
    }

    void Runnable::Quit()
    {
        if (!running) { throw std::runtime_error("Runnable is not running!"); }
        else { running = false; }

        OnQuit();
    }

    bool Runnable::IsRunning() const { return running; }
#pragma endregion

#pragma region Bitmap
    Bitmap::Bitmap(size_t _width, size_t _height) : surface(nullptr)
    {
        surface = SDL_CreateRGBSurfaceWithFormat(0, _width, _height, 32, SDL_PIXELFORMAT_ARGB8888);
        CHECK_SDL(surface, "Could not create bitmap!");
    }

    Bitmap::Bitmap(SDL_Surface* _surface) : surface(_surface) { }

    Bitmap::Bitmap() : Bitmap(0, 0) { }

    Bitmap::Bitmap(const Bitmap& _b) : Bitmap(SDL_DuplicateSurface(_b.surface)) { }

    Bitmap::Bitmap(Bitmap&& _b) noexcept : surface(_b.surface)
    {
        SDL_FreeSurface(_b.surface);
        _b.surface = nullptr;
    }

    Bitmap::~Bitmap() { SDL_FreeSurface(surface); }

    void Bitmap::Fill(Color _color) { SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, _color.r, _color.g, _color.b, _color.a)); }
    void Bitmap::Blit(const Bitmap& _bitmap) { SDL_BlitSurface(_bitmap.surface, NULL, surface, NULL); }

    Bitmap& Bitmap::operator=(const Bitmap& _b)
    {
        SDL_FreeSurface(surface);
        surface = SDL_DuplicateSurface(_b.surface);
        return *this;
    }

    Bitmap& Bitmap::operator=(Bitmap&& _b) noexcept
    {
        if (this != &_b)
        {
            SDL_FreeSurface(surface);
            surface = _b.surface;
            _b.surface = nullptr;
        }

        return *this;
    }

    Color Bitmap::GetPixel(int64_t _x, int64_t _y)
    {
        if (_x < 0 || _x >= GetWidth() || _y < 0 || _y >= GetHeight())
            return Color::Clear();

        Color color;
        SDL_GetRGBA(((Uint32*)surface->pixels)[_x + _y * surface->w], surface->format, &color.r, &color.g, &color.b, &color.a);
        return color;
    }

    void Bitmap::SetPixel(int64_t _x, int64_t _y, Color _color)
    {
        if (_x < 0 || _x >= GetWidth() || _y < 0 || _y >= GetHeight())
            return;

        ((Uint32*)surface->pixels)[_x + _y * surface->w] = SDL_MapRGBA(surface->format, _color.r, _color.g, _color.b, _color.a);
    }

    size_t Bitmap::GetWidth() const { return surface->w; }
    size_t Bitmap::GetHeight() const { return surface->h; }

    SDL_Surface* Bitmap::Internal::GetSurface(const Bitmap& _bitmap) { return _bitmap.surface; }
#pragma endregion

#pragma region Window
    Window::Window(std::string _title, size_t _x, size_t _y, size_t _width, size_t _height)
    {
        instance = SDL_CreateWindow(_title.c_str(), _x, _y, _width, _height, SDL_WINDOW_RESIZABLE);
        CHECK_SDL(instance, "Could not create window!");
    }

    Window::~Window() { SDL_DestroyWindow(instance); }

    void Window::Show() { SDL_ShowWindow(instance); }

    Bitmap Window::Capture() { return Bitmap(SDL_DuplicateSurface(SDL_GetWindowSurface(instance))); }

    void Window::UpdateBuffer(const Bitmap& _buffer)
    {
        SDL_BlitSurface(Bitmap::Internal::GetSurface(_buffer), NULL, SDL_GetWindowSurface(instance), NULL);
        SDL_UpdateWindowSurface(instance);
    }

    size_t Window::GetWidth() const { return SDL_GetWindowSurface(instance)->w; }
    size_t Window::GetHeight() const { return SDL_GetWindowSurface(instance)->h; }
#pragma endregion

    namespace Renderer
    {
        void Draw(Bitmap& _target, const EdgeScan& _scan)
        {
            double lX = _scan.lStart, rX = _scan.rStart;

            for (int64_t y = _scan.start; y < _scan.end; y++)
            {
                int64_t minX = std::ceil(lX), maxX = std::ceil(rX);

                if (maxX < minX)
                    std::swap(minX, maxX);

                for (int64_t x = std::ceil(minX); x <= std::ceil(maxX); x++)
                    _target.SetPixel(x, y, rxd::Color::Red());

                lX += _scan.lStep;
                rX += _scan.rStep;
            }
        }

        void DrawLine(Bitmap& _target, const Vertex& _v1, const Vertex& _v2)
        {
            const Vertex* left = &_v1, * right = &_v2;
            if (left->coords.x > right->coords.x)
                std::swap(left, right);

            if (left->coords.y == right->coords.y) // Horizontal line
            {
                int64_t y = std::ceil(left->coords.y);
                int64_t xStart = std::ceil(left->coords.x), xEnd = std::ceil(right->coords.x);

                for (int64_t x = xStart; x < xEnd; x++)
                    _target.SetPixel(x, y, rxd::Color::Red());
            }
            else if (left->coords.y < right->coords.y)
            {
                int64_t yStart = std::ceil(left->coords.y), yEnd = std::ceil(right->coords.y);

                double xStep = (right->coords.x - left->coords.x) / (right->coords.y - left->coords.y);
                double scanlineX0 = left->coords.x, scanlineX1 = scanlineX0 + xStep;

                for (int64_t y = yStart; y < yEnd; y++)
                {
                    int64_t xStart = std::ceil(scanlineX0);
                    int64_t xEnd = std::max(xStart + 1, (int64_t)std::ceil(scanlineX1));

                    for (int64_t x = xStart; x < xEnd; x++)
                        _target.SetPixel(x, y, rxd::Color::Red());

                    scanlineX0 = scanlineX1;
                    scanlineX1 += xStep;
                }
            }
            else
            {
                int64_t yStart = std::ceil(right->coords.y), yEnd = std::ceil(left->coords.y);

                double xStep = (right->coords.x - left->coords.x) / (right->coords.y - left->coords.y);
                double scanlineX0 = right->coords.x + xStep, scanlineX1 = right->coords.x;

                for (int64_t y = yStart; y < yEnd; y++)
                {
                    int64_t xStart = std::ceil(scanlineX0);
                    int64_t xEnd = std::max(xStart + 1, (int64_t)std::ceil(scanlineX1));

                    for (int64_t x = xStart; x < xEnd; x++)
                        _target.SetPixel(x, y, rxd::Color::Red());

                    scanlineX1 = scanlineX0;
                    scanlineX0 += xStep;
                }
            }
        }

        void DrawTriangle(Bitmap& _target, const Vertex& _v1, const Vertex& _v2, const Vertex& _v3)
        {
            //Sort vertices
            const Vertex* top = &_v1, * middle = &_v2, * bottom = &_v3;
            if (bottom->coords.y < middle->coords.y) { std::swap(bottom, middle); }
            if (middle->coords.y < top->coords.y) { std::swap(middle, top); }
            if (bottom->coords.y < middle->coords.y) { std::swap(bottom, middle); }

            auto topToBottom = bottom->coords - top->coords, topToMiddle = middle->coords - top->coords;
            bool rightHanded = topToMiddle.x * topToBottom.y - topToBottom.x * topToMiddle.y >= 0;

            if (rightHanded)
            {
                int64_t yStart = std::ceil(top->coords.y), yEnd = std::ceil(middle->coords.y);
                double topToBottomXStep = (bottom->coords.x - top->coords.x) / (bottom->coords.y - top->coords.y);
                double topToMiddleXStep = (middle->coords.x - top->coords.x) / (middle->coords.y - top->coords.y);
                double middleToBottomXStep = (bottom->coords.x - middle->coords.x) / (bottom->coords.y - middle->coords.y);
                double scanlineX0 = top->coords.x, scanlineX1 = top->coords.x;
                int64_t y = yStart;

                while (y < yEnd)
                {
                    int64_t xStart = std::ceil(scanlineX0), xEnd = std::ceil(scanlineX1);

                    for (int64_t x = xStart; x < xEnd; x++)
                        _target.SetPixel(x, y, rxd::Color::Green());

                    scanlineX0 += topToBottomXStep;
                    scanlineX1 += topToMiddleXStep;
                    y++;
                }

                yEnd = std::ceil(bottom->coords.y);

                while (y < yEnd)
                {
                    int64_t xStart = std::ceil(scanlineX0), xEnd = std::ceil(scanlineX1);

                    for (int64_t x = xStart; x < xEnd; x++)
                        _target.SetPixel(x, y, rxd::Color::White());

                    scanlineX0 += topToBottomXStep;
                    scanlineX1 += middleToBottomXStep;
                    y++;
                }
            }
            else
            {
                int64_t yStart = std::ceil(top->coords.y), yEnd = std::ceil(middle->coords.y);
                double topToBottomXStep = (bottom->coords.x - top->coords.x) / (bottom->coords.y - top->coords.y);
                double topToMiddleXStep = (middle->coords.x - top->coords.x) / (middle->coords.y - top->coords.y);
                double middleToBottomXStep = (bottom->coords.x - middle->coords.x) / (bottom->coords.y - middle->coords.y);
                double scanlineX0 = top->coords.x, scanlineX1 = top->coords.x;
                int64_t y = yStart;

                while (y < yEnd)
                {
                    int64_t xStart = std::ceil(scanlineX0), xEnd = std::ceil(scanlineX1);

                    for (int64_t x = xStart; x < xEnd; x++)
                        _target.SetPixel(x, y, rxd::Color::Green());

                    scanlineX0 += topToMiddleXStep;
                    scanlineX1 += topToBottomXStep;
                    y++;
                }

                yEnd = std::ceil(bottom->coords.y);

                while (y < yEnd)
                {
                    int64_t xStart = std::ceil(scanlineX0), xEnd = std::ceil(scanlineX1);

                    for (int64_t x = xStart; x < xEnd; x++)
                        _target.SetPixel(x, y, rxd::Color::Green());

                    scanlineX0 += middleToBottomXStep;
                    scanlineX1 += topToBottomXStep;
                    y++;
                }
            }

            DrawLine(_target, *top, *bottom);
            DrawLine(_target, *top, *middle);
            DrawLine(_target, *middle, *bottom);
        }
    }
}