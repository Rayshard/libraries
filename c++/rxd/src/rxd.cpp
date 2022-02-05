#include "rxd.h"
#include "SDL2_image/SDL_image.h"
#include <filesystem>
#include <iostream> //TODO: Remove

namespace rxd
{
    bool initialized = false;
    SDL_PixelFormat* pixelFormat;

    void Init()
    {
        CHECK_SDL(SDL_Init(SDL_INIT_EVERYTHING) >= 0, "SDL was not initialized properly!");

        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF | IMG_INIT_WEBP);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

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
            {
                _runnable.OnEvent(event);

                if (event.type == SDL_EventType::SDL_QUIT)
                    _runnable.Quit();
            }
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
}