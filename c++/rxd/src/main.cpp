#include <iostream>
#include <assert.h>
#include <vector>
#include <functional>
#include "SDL2/SDL.h"

namespace rxd
{
    void Init()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
            throw std::runtime_error(std::string("SDL was not initialized properly! Error:\n") + SDL_GetError());
    }

    void Run(std::function<void(const SDL_Event&, bool&)> _onEvent)
    {
        SDL_Event event;
        bool running = true;

        while (running)
        {
            while (SDL_PollEvent(&event))
            {
                _onEvent(event, running);

                if (event.type == SDL_QUIT)
                    running = false;
            }
        }
    }

    void CleanUp()
    {
        SDL_Quit();
    }

    struct Color { uint8_t a, r, g, b; };

    class Bitmap
    {
        SDL_Surface* surface;

    public:
        Bitmap(size_t _width, size_t _height) : surface(nullptr)
        {
            surface = SDL_CreateRGBSurfaceWithFormat(0, _width, _height, 32, SDL_PIXELFORMAT_ARGB8888);
            if (!surface)
                throw std::runtime_error(std::string("Could not create bitmap! Error:\n") + SDL_GetError());
        }

        Bitmap() : Bitmap(0, 0) { }

        Bitmap(const Bitmap& _b) : Bitmap(_b.GetWidth(), _b.GetHeight()) { SDL_BlitSurface(_b.surface, NULL, surface, NULL); }

        Bitmap(Bitmap&& _b) noexcept : surface(_b.surface)
        {
            std::cout << "moved" <<std::endl;
            SDL_FreeSurface(_b.surface);
            _b.surface = nullptr;
        }

        ~Bitmap()
        {
            SDL_FreeSurface(surface);
        }

        void Fill(Color _color)
        {
            SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, _color.r, _color.g, _color.b, _color.a));
        }

        void Blit(SDL_Surface* _surface)
        {
            SDL_BlitSurface(_surface, NULL, surface, NULL);
        }

        void Blit(const Bitmap& _bitmap) { Blit(_bitmap.GetSurface()); }

        Bitmap& operator=(const Bitmap& _b)
        {
            if (GetWidth() != _b.GetWidth() || GetHeight() != _b.GetHeight())
            {
                SDL_FreeSurface(surface);
                surface = SDL_CreateRGBSurfaceWithFormat(0, _b.surface->w, _b.surface->h, 32, SDL_PIXELFORMAT_ARGB8888);
            }

            Blit(_b.surface);
            return *this;
        }

        Bitmap& operator=(Bitmap&& _b) noexcept
        {
            if (this != &_b)
            {
                SDL_FreeSurface(surface);
                surface = _b.surface;
                _b.surface = nullptr;
            }

            return *this;
        }

        size_t GetWidth() const { return surface->w; }
        size_t GetHeight() const { return surface->h; }
        Color* GetPixels() const { return (Color*)surface->pixels; }
        SDL_Surface* GetSurface() const { return surface; }
    };

    class Window
    {
        SDL_Window* instance;
        SDL_Surface* surface;

    public:
        Window(std::string _title, size_t _x, size_t _y, size_t _width, size_t _height)
        {
            instance = SDL_CreateWindow(_title.c_str(), _x, _y, _width, _height, SDL_WINDOW_RESIZABLE);
            if (!instance)
                throw std::runtime_error(std::string("Could not create window! Error:\n") + SDL_GetError());

            surface = SDL_GetWindowSurface(instance);
        }

        ~Window() { SDL_DestroyWindow(instance); }

        void Show() { SDL_ShowWindow(instance); }

        void Blit(const Bitmap& _bitmap)
        {
            SDL_BlitSurface(_bitmap.GetSurface(), NULL, surface, NULL);
            SDL_UpdateWindowSurface(instance);
        }

        Bitmap Capture()
        {
            Bitmap bitmap(surface->w, surface->h);
            bitmap.Blit(surface);
            return bitmap;
        }

        size_t GetWidth() const { return surface->w; }
        size_t GetHeight() const { return surface->h; }
    };
}

void HandleEvent(const SDL_Event& _event, bool& _running)
{

}

int main()
{
    try
    {
        rxd::Init();

        rxd::Window window("RXD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 600 / 16 * 9);
        window.Show();


        rxd::Bitmap screen(window.GetWidth(), window.GetHeight());
        screen.Fill(rxd::Color{ 255, 0, 0, 255 });

        window.Blit(screen);

        rxd::Run(HandleEvent);
    }
    catch (const std::exception& _e)
    {
        std::cerr << _e.what() << std::endl;
        return EXIT_FAILURE;
    }

    rxd::CleanUp();
    return EXIT_SUCCESS;
}