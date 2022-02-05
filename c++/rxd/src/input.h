#pragma once

#include "SDL2/SDL.h"
#include <set>

#include "math.h"

using namespace rxd::math;

namespace rxd::input
{
    typedef SDL_KeyCode Key;

    enum class MouseButton
    {
        LEFT = 1,
        MIDDLE = 2,
        RIGHT = 3,
        X1 = 4,
        X2 = 5
    };

    class Keyboard
    {
        std::set<Key> downKeys, releasedKeys, pressedKeys, downKeysBuffer;

    public:
        Keyboard();

        void HandleEvent(const SDL_Event& _event);
        void Update();

        bool IsKeyDown(const Key& _key) const;
        bool WasKeyReleased(const Key& _key) const;
        bool WasKeyPressed(const Key& _key) const;
    };

    class Mouse
    {
        Vec2UI32 position, positionBuffer;
        Vec2I32 delta, deltaBuffer;
        std::set<MouseButton> downButtons, releasedButtons, pressedButtons, downButtonsBuffer;

    public:
        Mouse();

        void HandleEvent(const SDL_Event& _event);
        void Update();

        bool IsButtonDown(const MouseButton& _button) const;
        bool WasButtonReleased(const MouseButton& _button) const;
        bool WasButtonPressed(const MouseButton& _button) const;

        Vec2UI32 GetPosition() const;
        Vec2I32 GetDelta() const;
        uint32_t GetX() const;
        uint32_t GetY() const;
        int32_t GetDX() const;
        int32_t GetDY() const;
    };
}