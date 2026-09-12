#pragma once

#include <SDL.h>

#include <array>
#include <cstddef>
#include <string>

namespace pj64::input
{
enum class N64Button : size_t
{
    A,
    B,
    Z,
    Start,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    CUp,
    CDown,
    CLeft,
    CRight,
    L,
    R,
    Count,
};

enum class KeyboardAction : size_t
{
    A,
    B,
    Z,
    Start,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    CUp,
    CDown,
    CLeft,
    CRight,
    L,
    R,
    AnalogUp,
    AnalogDown,
    AnalogLeft,
    AnalogRight,
    Count,
};

struct GamepadBinding
{
    enum class Kind
    {
        None,
        Button,
        PositiveAxis,
        NegativeAxis,
    };

    Kind kind = Kind::None;
    int value = 0;
};

struct InputConfig
{
    std::array<SDL_Scancode, static_cast<size_t>(KeyboardAction::Count)> keyboard;
    std::array<GamepadBinding, static_cast<size_t>(N64Button::Count)> gamepad;
    SDL_GameControllerAxis analogX = SDL_CONTROLLER_AXIS_LEFTX;
    SDL_GameControllerAxis analogY = SDL_CONTROLLER_AXIS_LEFTY;
    bool invertAnalogX = false;
    bool invertAnalogY = true;
    int deadzone = 8000;
    int sensitivity = 80;
    std::string controllerGuid;

    InputConfig();
    bool Load(const std::string & path);
    bool Save(const std::string & path) const;
};

const char * N64ButtonName(N64Button button);
const char * KeyboardActionName(KeyboardAction action);
std::string BindingName(const GamepadBinding & binding);
std::string BindingValue(const GamepadBinding & binding);
GamepadBinding ParseBinding(const std::string & value);
std::string AxisName(SDL_GameControllerAxis axis);
SDL_GameControllerAxis ParseAxis(const std::string & value);
bool BindingPressed(SDL_GameController * controller, const GamepadBinding & binding, int threshold = 16000);
}
