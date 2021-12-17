#include "rxd.h"

#define CHECK_SDL(value, message) do { if (!(value)) throw std::runtime_error(std::string(message) + " Error:\n" + SDL_GetError()); } while(false)

namespace rxd
{
    static bool initialized = false;

    void Init()
    {
        CHECK_SDL(SDL_Init(SDL_INIT_EVERYTHING) >= 0, "SDL was not initialized properly!");
        initialized = true;
    }

    void Run(IRunnable& _runnable)
    {
        if (!initialized) { throw std::runtime_error("RXD is not initialized! Call rxd::Init() to initialize RXD."); }
        else if (_runnable.IsRunning()) { throw std::runtime_error("Specified Runnable is already running!"); }
        
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

    Color Bitmap::GetPixel(size_t _x, size_t _y)
    {
        Color color;
        SDL_GetRGBA(((Uint32*)surface->pixels)[_x + _y * surface->w], surface->format, &color.r, &color.g, &color.b, &color.a);
        return color;
    }

    void Bitmap::SetPixel(size_t _x, size_t _y, Color _color)
    {
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
}