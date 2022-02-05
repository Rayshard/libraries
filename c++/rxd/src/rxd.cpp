#include "rxd.h"
#include <filesystem>
#include <iostream> //TODO: Remove
#include "SDL2_image/SDL_image.h"

namespace rxd
{
    static bool initialized = false;
    static SDL_PixelFormat* pixelFormat;

    void Init()
    {
        CHECK_SDL(SDL_Init(SDL_INIT_EVERYTHING) >= 0, "SDL was not initialized properly!");

        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF | IMG_INIT_WEBP);

        pixelFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
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

        SDL_FreeFormat(pixelFormat);
        IMG_Quit();
        SDL_Quit();

        initialized = false;
    }

    void SetConstrainedMouse(bool _constrained) { CHECK_SDL(SDL_SetRelativeMouseMode(_constrained ? SDL_TRUE : SDL_FALSE) >= 0, "Unable to set mouse constraint!"); }

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
    Bitmap::Bitmap(uint64_t _width, uint64_t _height) : surface(nullptr)
    {
        surface = SDL_CreateRGBSurfaceWithFormat(0, _width, _height, 32, pixelFormat->format);
        CHECK_SDL(surface, "Could not create bitmap!");
    }

    Bitmap::Bitmap(SDL_Surface* _surface) : surface(_surface)
    {
        if (surface->format->format != pixelFormat->format)
        {
            auto convertedSurface = SDL_ConvertSurface(surface, pixelFormat, 0);
            CHECK_SDL(convertedSurface, "Could not create bitmap!");

            SDL_FreeSurface(surface);
            surface = convertedSurface;
        }
    }

    Bitmap::Bitmap(const std::string& _path)
    {
        std::string extension = std::filesystem::path(_path).extension();

        SDL_Surface* original = extension == "bmp" ? SDL_LoadBMP(_path.c_str()) : IMG_Load(_path.c_str());
        CHECK_SDL(original, "Cannot load bitmap from file: " + _path);

        if (original->format->format != pixelFormat->format)
        {
            auto convertedSurface = SDL_ConvertSurface(original, pixelFormat, 0);
            CHECK_SDL(convertedSurface, "Could not create bitmap!");

            surface = convertedSurface;
        }
        else { surface = original; }
    }

    Bitmap::Bitmap() : Bitmap(0, 0) { }

    Bitmap::Bitmap(const Bitmap& _b) : Bitmap(SDL_DuplicateSurface(_b.surface)) { }

    Bitmap::Bitmap(Bitmap&& _b) noexcept : surface(_b.surface)
    {
        SDL_FreeSurface(_b.surface);
        _b.surface = nullptr;
    }

    Bitmap::~Bitmap() { SDL_FreeSurface(surface); }

    void Bitmap::Fill(utilities::Color _color) { SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, _color.r, _color.g, _color.b, _color.a)); }
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

    utilities::Color Bitmap::GetPixel(int64_t _x, int64_t _y)
    {
        if (_x < 0 || _x >= (int64_t)GetWidth() || _y < 0 || _y >= (int64_t)GetHeight())
            return utilities::Color::Clear();

        utilities::Color color;
        SDL_GetRGBA(((Uint32*)surface->pixels)[_x + _y * surface->w], surface->format, &color.r, &color.g, &color.b, &color.a);
        return color;
    }

    void Bitmap::SetPixel(int64_t _x, int64_t _y, utilities::Color _color)
    {
        if (_x < 0 || _x >= (int64_t)GetWidth() || _y < 0 || _y >= (int64_t)GetHeight())
            return;

        ((Uint32*)surface->pixels)[_x + _y * surface->w] = SDL_MapRGBA(surface->format, _color.r, _color.g, _color.b, _color.a);
    }

    uint64_t Bitmap::GetWidth() const { return surface->w; }
    uint64_t Bitmap::GetHeight() const { return surface->h; }
    double Bitmap::GetAspectRatio() const { return GetWidth() / (double)GetHeight(); }

    SDL_Surface* Bitmap::Internal::GetSurface(const Bitmap& _bitmap) { return _bitmap.surface; }
#pragma endregion

#pragma region Window
    Window::Window(std::string _title, uint64_t _x, uint64_t _y, uint64_t _width, uint64_t _height, uint64_t _screenResolution)
        : screen(nullptr), screenResolution(_screenResolution)
    {
        instance = SDL_CreateWindow(_title.c_str(), _x, _y, _width, _height, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        CHECK_SDL(instance, "Could not create window!");

        renderer = SDL_CreateRenderer(instance, -1, SDL_RENDERER_ACCELERATED);
        CHECK_SDL(renderer, "Could not create window renderer!");

        UpdateScreen();
    }

    Window::~Window()
    {
        SDL_DestroyTexture(screen);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(instance);
    }

    void Window::Show() { SDL_ShowWindow(instance); }

    void Window::FlipScreenBuffer(const Bitmap& _buffer)
    {
        SDL_Surface* surface = Bitmap::Internal::GetSurface(_buffer);

        SDL_UpdateTexture(screen, NULL, surface->pixels, surface->pitch);
        SDL_RenderCopy(renderer, screen, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    uint64_t Window::GetWidth() const
    {
        int width;
        SDL_GetWindowSize(instance, &width, NULL);
        return width;
    }

    uint64_t Window::GetHeight() const
    {
        int height;
        SDL_GetWindowSize(instance, NULL, &height);
        return height;
    }

    uint64_t Window::GetScreenWidth() const
    {
        int width;
        SDL_QueryTexture(screen, NULL, NULL, &width, NULL);
        return width;
    }

    uint64_t Window::GetScreenHeight() const
    {
        int height;
        SDL_QueryTexture(screen, NULL, NULL, NULL, &height);
        return height;
    }

    double Window::GetAspectRatio() const { return GetWidth() / (double)GetHeight(); }

    void Window::HandleEvent(const SDL_Event& _event)
    {
        if (_event.type != SDL_WINDOWEVENT)
            return;

        switch (_event.window.event)
        {
        case SDL_WINDOWEVENT_RESIZED: UpdateScreen(); break;
        }
    }

    void Window::UpdateScreen()
    {
        SDL_DestroyTexture(screen);

        screen = SDL_CreateTexture(renderer, pixelFormat->format, SDL_TEXTUREACCESS_STREAMING, screenResolution * GetAspectRatio(), screenResolution);
        CHECK_SDL(screen, "Could not create window screen!");
    }

    Vec2UI32 Window::MapToAbsScreenCoords(uint32_t _x, uint32_t _y) const
    {
        return Vec2UI32({
            (uint32_t)(_x / (GetWidth() - 1.0) * GetScreenWidth()),
            (uint32_t)(_y / (GetHeight() - 1.0) * GetScreenHeight())
            });
    }

    Vec2F64 Window::MapToNormCoords(uint32_t _x, uint32_t _y) const { return Vec2F64({ _x / (GetWidth() - 1.0), _y / (GetHeight() - 1.0) }); }
#pragma endregion
}