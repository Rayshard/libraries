#pragma once

#include "SDL2/SDL.h"
#include <string>
#include <functional>
#include <thread>

namespace rxd
{
    //TODO: strip and put in cpp
    class IRunnable
    {
        bool running;
    public:
        IRunnable() : running(false) { }

        std::thread Start()
        {
            running = true;

            OnStart();
            return std::thread(&IRunnable::OnRun, this);
        }

        void Quit()
        {
            OnQuit();
            running = false;
        }

        virtual void OnEvent(const SDL_Event& _event) = 0;

        bool IsRunning() const { return running; }

    protected:
        virtual void OnStart() = 0;
        virtual void OnRun() = 0;
        virtual void OnQuit() = 0;
    };

    void Init();
    void Run(IRunnable& _runnable);
    void Quit();
    void CleanUp();

    struct Color { uint8_t r, g, b, a; };

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

        Color GetPixel(size_t _x, size_t _y);
        void SetPixel(size_t _x, size_t _y, Color _col);

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
}