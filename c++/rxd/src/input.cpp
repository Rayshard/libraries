#include "input.h"
#include <iostream> //TODO: remove

namespace rxd::input
{
    Keyboard::Keyboard()
    {

    }

    void Keyboard::HandleEvent(const SDL_Event& _event)
    {
        switch (_event.type)
        {
        case SDL_EventType::SDL_KEYDOWN: downKeysBuffer.insert((Key)_event.key.keysym.sym); break;
        case SDL_EventType::SDL_KEYUP: downKeysBuffer.erase((Key)_event.key.keysym.sym); break;
        default: break;
        }
    }

    void Keyboard::Update()
    {
        releasedKeys.clear();
        pressedKeys.clear();

        for(auto& key : downKeys)
        {
            if (!downKeysBuffer.contains(key))
                releasedKeys.insert(key);
        }

        for(auto& key : downKeysBuffer)
        {
            if (!downKeys.contains(key))
                pressedKeys.insert(key);
        }

        downKeys = downKeysBuffer;
    }

    bool Keyboard::IsKeyDown(const Key& _key) const { return downKeys.contains(_key); }
    bool Keyboard::WasKeyReleased(const Key& _key) const { return releasedKeys.contains(_key); }
    bool Keyboard::WasKeyPressed(const Key& _key) const { return pressedKeys.contains(_key); }


#pragma region Mouse
    Mouse::Mouse()
    {

    }

    void Mouse::HandleEvent(const SDL_Event& _event)
    {
        switch (_event.type)
        {
        case SDL_EventType::SDL_MOUSEBUTTONDOWN: downButtonsBuffer.insert((MouseButton)_event.button.button); break;
        case SDL_EventType::SDL_MOUSEBUTTONUP: downButtonsBuffer.erase((MouseButton)_event.button.button); break;
        case SDL_EventType::SDL_MOUSEMOTION: 
        {
            positionBuffer[0] = _event.motion.x;
            positionBuffer[1] = _event.motion.y;
            deltaBuffer[0] = _event.motion.xrel;
            deltaBuffer[1] = _event.motion.yrel;
        } break;
        default: break;
        }
    }

    void Mouse::Update()
    {
        releasedButtons.clear();
        pressedButtons.clear();

        for(auto& button : downButtons)
        {
            if (!downButtonsBuffer.contains(button))
                releasedButtons.insert(button);
        }

        for(auto& button : downButtonsBuffer)
        {
            if (!downButtons.contains(button))
                pressedButtons.insert(button);
        }

        downButtons = downButtonsBuffer;
        position = positionBuffer;
        delta = deltaBuffer;
    }

    bool Mouse::IsButtonDown(const MouseButton& _button) const { return downButtons.contains(_button); }
    bool Mouse::WasButtonReleased(const MouseButton& _button) const { return releasedButtons.contains(_button); }
    bool Mouse::WasButtonPressed(const MouseButton& _button) const { return pressedButtons.contains(_button); }
    
    Vec2UI32 Mouse::GetPosition() const { return position; }
    Vec2I32 Mouse::GetDelta() const { return delta; }
    uint32_t Mouse::GetX() const { return position[0]; }
    uint32_t Mouse::GetY() const { return position[1]; }
    int32_t Mouse::GetDX() const { return delta[0]; }
    int32_t Mouse::GetDY() const { return delta[1]; }
#pragma endregion
}