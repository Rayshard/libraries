#include "window.h"
#include "rxd.h"

namespace rxd::window
{
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
}