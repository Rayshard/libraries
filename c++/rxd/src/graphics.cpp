#include "graphics.h"
#include "rxd.h"
#include "SDL2_image/SDL_image.h"
#include <filesystem>

namespace rxd::graphics
{
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
        if (_x < 0 || _x >= (int64_t)GetWidth() || _y < 0 || _y >= (int64_t)GetHeight())
            return Color::Clear();

        Color color;
        SDL_GetRGBA(((Uint32*)surface->pixels)[_x + _y * surface->w], surface->format, &color.r, &color.g, &color.b, &color.a);
        return color;
    }

    void Bitmap::SetPixel(int64_t _x, int64_t _y, Color _color)
    {
        if (_x < 0 || _x >= (int64_t)GetWidth() || _y < 0 || _y >= (int64_t)GetHeight())
            return;

        ((Uint32*)surface->pixels)[_x + _y * surface->w] = SDL_MapRGBA(surface->format, _color.r, _color.g, _color.b, _color.a);
    }

    uint64_t Bitmap::GetWidth() const { return surface->w; }
    uint64_t Bitmap::GetHeight() const { return surface->h; }
    double Bitmap::GetAspectRatio() const { return GetWidth() / (double)GetHeight(); }

    SDL_Surface* Bitmap::Internal::GetSurface(const Bitmap& _bitmap) { return _bitmap.surface; }
}