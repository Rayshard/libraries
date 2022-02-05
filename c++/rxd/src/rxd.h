#pragma once

#include "prelude.h"
#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <concepts>

namespace rxd
{
    namespace window
    {
        class Window;
    }

    extern SDL_PixelFormat* pixelFormat;

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
}