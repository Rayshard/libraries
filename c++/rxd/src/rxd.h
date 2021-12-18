#pragma once

#include "SDL2/SDL.h"
#include <string>
#include <functional>
#include <thread>

namespace rxd
{
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

    struct Color
    {
        uint8_t r, g, b, a;

        inline static Color Red() { return { 255, 0, 0, 255 }; }
        inline static Color Green() { return { 0, 255, 0, 255 }; }
        inline static Color Blue() { return { 0, 0, 255, 255 }; }
        inline static Color White() { return { 255, 255, 255, 255 }; }
        inline static Color Black() { return { 0, 0, 0, 255 }; }
        inline static Color Clear() { return { 0, 0, 0, 0 }; }
    };

    class Bitmap
    {
        SDL_Surface* surface;

    public:
        Bitmap();
        Bitmap(size_t _width, size_t _height);
        Bitmap(SDL_Surface* _surface);

        Bitmap(const Bitmap& _b);
        Bitmap(Bitmap&& _b) noexcept;

        ~Bitmap();

        void Fill(Color _color);
        void Blit(const Bitmap& _bitmap);

        Color GetPixel(int64_t _x, int64_t _y);
        void SetPixel(int64_t _x, int64_t _y, Color _col);

        Bitmap& operator=(const Bitmap& _b);
        Bitmap& operator=(Bitmap&& _b) noexcept;

        size_t GetWidth() const;
        size_t GetHeight() const;

        class Internal
        {
            friend class Window;

            static SDL_Surface* GetSurface(const Bitmap& _bitmap);
        };
    };

    class Window
    {
        SDL_Window* instance;

    public:
        Window(std::string _title, size_t _x, size_t _y, size_t _width, size_t _height);
        ~Window();

        void Show();
        void UpdateBuffer(const Bitmap& _buffer);
        Bitmap Capture();

        size_t GetWidth() const;
        size_t GetHeight() const;
    };

    struct EdgeScan
    {
        int64_t start, end;
        double lStart, lStep, rStart, rStep;
    };

    namespace Utilities
    {
        template<typename T>
        struct Vec2
        {
            T x, y;

            Vec2(T _x = 0, T _y = 0) : x(_x), y(_y) { }
        };

        template<typename T>
        Vec2<T> operator-(const Vec2<T>& _left, const Vec2<T>& _right) { return Vec2<T>(_left.x - _right.x, _left.y - _right.y); }

        typedef Vec2<int64_t> Vec2I64;
        typedef Vec2<double> Vec2F64;
    };


    namespace Renderer
    {
        struct Vertex
        {
            Utilities::Vec2F64 coords;
        };

        void Draw(Bitmap& _target, const EdgeScan& _scan);
        void DrawLine(Bitmap& _target, const Vertex& _v1, const Vertex& _v2);
        void DrawTriangle(Bitmap& _target, const Vertex& _v1, const Vertex& _v2, const Vertex& _v3);
    }
}