#pragma once
#include "prelude.h"
#include "graphics.h"

using namespace rxd::math;
using namespace rxd::graphics;

namespace rxd::window
{
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