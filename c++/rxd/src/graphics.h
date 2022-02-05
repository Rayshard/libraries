#pragma once
#include "prelude.h"
#include "math.h"

using namespace rxd::math;

namespace rxd::window
{
    class Window;
}

namespace rxd::graphics
{
    struct Color
    {
        uint8_t a, r, g, b;

        Color(uint8_t _a, uint8_t _r, uint8_t _g, uint8_t _b) : a(_a), r(_r), g(_g), b(_b) { }
        Color(Vec4F64 _v) : Color(_v[0] * 255, _v[1] * 255, _v[2] * 255, _v[3] * 255) { }
        Color(uint32_t _value) : Color((_value >> 24) & 255, (_value >> 16) & 255, (_value >> 8) & 255, _value & 255) { }
        Color() : Color(255, 0, 0, 0) { }

        Vec4F64 ToVec4F64() { return Vec4F64({ a / 255.0, r / 255.0, g / 255.0, b / 255.0 }); }

        inline static Color Red() { return { 255, 255, 0, 0 }; }
        inline static Color Green() { return { 255, 0, 255, 0 }; }
        inline static Color Blue() { return { 255, 0, 0, 255 }; }
        inline static Color White() { return { 255, 255, 255, 255 }; }
        inline static Color Black() { return { 255, 0, 0, 0 }; }
        inline static Color Clear() { return { 0, 0, 0, 0 }; }
    };

    class Bitmap
    {
    protected:
        SDL_Surface* surface;

        Bitmap(SDL_Surface* _surface);

    public:
        Bitmap();
        Bitmap(uint64_t _width, uint64_t _height);
        Bitmap(const std::string& _path);

        Bitmap(const Bitmap& _b);
        Bitmap(Bitmap&& _b) noexcept;

        virtual ~Bitmap();

        void Fill(Color _color);
        void Blit(const Bitmap& _bitmap);

        Color GetPixel(int64_t _x, int64_t _y);
        void SetPixel(int64_t _x, int64_t _y, Color _col);

        Bitmap& operator=(const Bitmap& _b);
        Bitmap& operator=(Bitmap&& _b) noexcept;

        uint64_t GetWidth() const;
        uint64_t GetHeight() const;
        double GetAspectRatio() const;

        class Internal
        {
            friend class rxd::window::Window;

            static SDL_Surface* GetSurface(const Bitmap& _bitmap);
        };
    };
}