#pragma once

#include "SDL2/SDL.h"
#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <concepts>
#include "math.h"

//TODO: Move to cpp file
#define CHECK_SDL(value, message) do { if (!(value)) throw std::runtime_error(std::string(message) + " Error:\n" + SDL_GetError()); } while(false)

namespace rxd
{
    using namespace math;

    class Runnable
    {
        bool running;

    public:
        Runnable();

        std::thread Start();
        void Quit();

        bool IsRunning() const;

        virtual void OnEvent(const SDL_Event& _event) = 0;

    protected:
        virtual void OnStart() = 0;
        virtual void OnRun() = 0;
        virtual void OnQuit() = 0;
    };

    void Init();
    void Run(Runnable& _runnable);
    void Quit();
    void CleanUp();

    void SetConstrainedMouse(bool _constrained);

    namespace utilities
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
    }
    
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

        void Fill(utilities::Color _color);
        void Blit(const Bitmap& _bitmap);

        utilities::Color GetPixel(int64_t _x, int64_t _y);
        void SetPixel(int64_t _x, int64_t _y, utilities::Color _col);

        Bitmap& operator=(const Bitmap& _b);
        Bitmap& operator=(Bitmap&& _b) noexcept;

        uint64_t GetWidth() const;
        uint64_t GetHeight() const;
        double GetAspectRatio() const;

        class Internal
        {
            friend class Window;
            friend class Screen;

            static SDL_Surface* GetSurface(const Bitmap& _bitmap);
        };
    };

    class Window
    {
        SDL_Window* instance;
        SDL_Renderer* renderer;

        SDL_Texture* screen;
        uint64_t screenResolution;

    public:
        Window(std::string _title, uint64_t _x, uint64_t _y, uint64_t _width, uint64_t _height, uint64_t screenResolution);
        virtual ~Window();

        void Show();
        void HandleEvent(const SDL_Event& _event);
        void FlipScreenBuffer(const Bitmap& _buffer);

        uint64_t GetWidth() const;
        uint64_t GetHeight() const;
        uint64_t GetScreenWidth() const;
        uint64_t GetScreenHeight() const;
        double GetAspectRatio() const;

        Vec2UI32 MapToAbsScreenCoords(uint32_t _x, uint32_t _y) const;
        Vec2F64 MapToNormCoords(uint32_t _x, uint32_t _y) const;

    private:
        void UpdateScreen();
    };
}