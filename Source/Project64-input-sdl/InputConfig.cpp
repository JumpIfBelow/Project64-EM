#include "InputConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace pj64::input
{
namespace
{
using Button = SDL_GameControllerButton;
using Axis = SDL_GameControllerAxis;

constexpr size_t ToIndex(N64Button button)
{
    return static_cast<size_t>(button);
}

constexpr size_t ToIndex(KeyboardAction action)
{
    return static_cast<size_t>(action);
}

std::string Trim(std::string value)
{
    auto isSpace = [](unsigned char character) { return std::isspace(character) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char character) { return !isSpace(character); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char character) { return !isSpace(character); }).base(), value.end());
    return value;
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool ParseBool(const std::string & value, bool defaultValue)
{
    const std::string lowered = Lower(Trim(value));
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on")
    {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off")
    {
        return false;
    }
    return defaultValue;
}

Button ParseButton(const std::string & value)
{
    const std::string lowered = Lower(value);
    static const std::array<std::pair<const char *, Button>, 15> buttons = {{
        {"a", SDL_CONTROLLER_BUTTON_A}, {"b", SDL_CONTROLLER_BUTTON_B},
        {"x", SDL_CONTROLLER_BUTTON_X}, {"y", SDL_CONTROLLER_BUTTON_Y},
        {"back", SDL_CONTROLLER_BUTTON_BACK}, {"guide", SDL_CONTROLLER_BUTTON_GUIDE},
        {"start", SDL_CONTROLLER_BUTTON_START}, {"leftstick", SDL_CONTROLLER_BUTTON_LEFTSTICK},
        {"rightstick", SDL_CONTROLLER_BUTTON_RIGHTSTICK}, {"leftshoulder", SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
        {"rightshoulder", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}, {"dpad_up", SDL_CONTROLLER_BUTTON_DPAD_UP},
        {"dpad_down", SDL_CONTROLLER_BUTTON_DPAD_DOWN}, {"dpad_left", SDL_CONTROLLER_BUTTON_DPAD_LEFT},
        {"dpad_right", SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
    }};
    for (const auto & item : buttons)
    {
        if (lowered == item.first)
        {
            return item.second;
        }
    }
    return SDL_CONTROLLER_BUTTON_INVALID;
}

const char * ButtonName(Button button)
{
    switch (button)
    {
    case SDL_CONTROLLER_BUTTON_A: return "A";
    case SDL_CONTROLLER_BUTTON_B: return "B";
    case SDL_CONTROLLER_BUTTON_X: return "X";
    case SDL_CONTROLLER_BUTTON_Y: return "Y";
    case SDL_CONTROLLER_BUTTON_BACK: return "Back";
    case SDL_CONTROLLER_BUTTON_GUIDE: return "Guide";
    case SDL_CONTROLLER_BUTTON_START: return "Start";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "Left Stick";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "Right Stick";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "Left Shoulder";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "Right Shoulder";
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return "D-pad Up";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "D-pad Down";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "D-pad Left";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "D-pad Right";
    default: return "None";
    }
}

const char * ButtonValue(Button button)
{
    switch (button)
    {
    case SDL_CONTROLLER_BUTTON_A: return "a";
    case SDL_CONTROLLER_BUTTON_B: return "b";
    case SDL_CONTROLLER_BUTTON_X: return "x";
    case SDL_CONTROLLER_BUTTON_Y: return "y";
    case SDL_CONTROLLER_BUTTON_BACK: return "back";
    case SDL_CONTROLLER_BUTTON_GUIDE: return "guide";
    case SDL_CONTROLLER_BUTTON_START: return "start";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "leftstick";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "rightstick";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "leftshoulder";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "rightshoulder";
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return "dpad_up";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "dpad_down";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "dpad_left";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "dpad_right";
    default: return "none";
    }
}
}

InputConfig::InputConfig()
{
    keyboard = {{
        SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_Z, SDL_SCANCODE_RETURN,
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
        SDL_SCANCODE_Q, SDL_SCANCODE_E,
        SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D,
    }};
    gamepad[ToIndex(N64Button::A)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_A};
    gamepad[ToIndex(N64Button::B)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_X};
    gamepad[ToIndex(N64Button::Z)] = {GamepadBinding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_TRIGGERLEFT};
    gamepad[ToIndex(N64Button::Start)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_START};
    gamepad[ToIndex(N64Button::DpadUp)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_UP};
    gamepad[ToIndex(N64Button::DpadDown)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_DOWN};
    gamepad[ToIndex(N64Button::DpadLeft)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_LEFT};
    gamepad[ToIndex(N64Button::DpadRight)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
    gamepad[ToIndex(N64Button::CUp)] = {GamepadBinding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_RIGHTY};
    gamepad[ToIndex(N64Button::CDown)] = {GamepadBinding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_RIGHTY};
    gamepad[ToIndex(N64Button::CLeft)] = {GamepadBinding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_RIGHTX};
    gamepad[ToIndex(N64Button::CRight)] = {GamepadBinding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_RIGHTX};
    gamepad[ToIndex(N64Button::L)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_LEFTSHOULDER};
    gamepad[ToIndex(N64Button::R)] = {GamepadBinding::Kind::Button, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER};
}

bool InputConfig::Load(const std::string & path)
{
    std::ifstream input(path);
    if (!input)
    {
        return false;
    }

    std::string section;
    std::string line;
    while (std::getline(input, line))
    {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            section = Lower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }
        const std::string key = Lower(Trim(line.substr(0, equals)));
        const std::string value = Trim(line.substr(equals + 1));
        if (section == "keyboard")
        {
            for (size_t index = 0; index < keyboard.size(); index++)
            {
                if (key == KeyboardActionName(static_cast<KeyboardAction>(index)))
                {
                    const SDL_Scancode scancode = SDL_GetScancodeFromName(value.c_str());
                    if (scancode != SDL_SCANCODE_UNKNOWN)
                    {
                        keyboard[index] = scancode;
                    }
                    break;
                }
            }
        }
        else if (section == "gamepad")
        {
            bool matched = false;
            for (size_t index = 0; index < gamepad.size(); index++)
            {
                if (key == N64ButtonName(static_cast<N64Button>(index)))
                {
                    gamepad[index] = ParseBinding(value);
                    matched = true;
                    break;
                }
            }
            if (matched) { continue; }
            if (key == "analog_x")
            {
                const SDL_GameControllerAxis axis = ParseAxis(value);
                if (axis != SDL_CONTROLLER_AXIS_INVALID) { analogX = axis; }
            }
            else if (key == "analog_y")
            {
                const SDL_GameControllerAxis axis = ParseAxis(value);
                if (axis != SDL_CONTROLLER_AXIS_INVALID) { analogY = axis; }
            }
            else if (key == "invert_x") { invertAnalogX = ParseBool(value, invertAnalogX); }
            else if (key == "invert_y") { invertAnalogY = ParseBool(value, invertAnalogY); }
            else if (key == "deadzone") { deadzone = std::clamp(std::atoi(value.c_str()), 0, 32767); }
            else if (key == "sensitivity") { sensitivity = std::clamp(std::atoi(value.c_str()), 1, 127); }
            else if (key == "controller_guid") { controllerGuid = value; }
        }
    }
    return true;
}

bool InputConfig::Save(const std::string & path) const
{
    std::error_code error;
    const std::filesystem::path destination(path);
    std::filesystem::create_directories(destination.parent_path(), error);
    const std::filesystem::path temporary = destination.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output)
    {
        return false;
    }
    output << "[keyboard]\n";
    for (size_t index = 0; index < keyboard.size(); index++)
    {
        output << KeyboardActionName(static_cast<KeyboardAction>(index)) << '=' << SDL_GetScancodeName(keyboard[index]) << '\n';
    }
    output << "\n[gamepad]\n";
    for (size_t index = 0; index < gamepad.size(); index++)
    {
        output << N64ButtonName(static_cast<N64Button>(index)) << '=' << BindingValue(gamepad[index]) << '\n';
    }
    output << "analog_x=" << AxisName(analogX) << '\n';
    output << "analog_y=" << AxisName(analogY) << '\n';
    output << "invert_x=" << (invertAnalogX ? "true" : "false") << '\n';
    output << "invert_y=" << (invertAnalogY ? "true" : "false") << '\n';
    output << "deadzone=" << deadzone << '\n';
    output << "sensitivity=" << sensitivity << '\n';
    output << "controller_guid=" << controllerGuid << '\n';
    output.close();
    if (!output)
    {
        std::filesystem::remove(temporary, error);
        return false;
    }
    std::filesystem::rename(temporary, destination, error);
    if (error)
    {
        std::filesystem::remove(destination, error);
        error.clear();
        std::filesystem::rename(temporary, destination, error);
    }
    return !error;
}

const char * N64ButtonName(N64Button button)
{
    static constexpr std::array<const char *, static_cast<size_t>(N64Button::Count)> names = {{
        "a", "b", "z", "start", "dpad_up", "dpad_down", "dpad_left", "dpad_right",
        "c_up", "c_down", "c_left", "c_right", "l", "r",
    }};
    return names[ToIndex(button)];
}

const char * KeyboardActionName(KeyboardAction action)
{
    static constexpr std::array<const char *, static_cast<size_t>(KeyboardAction::Count)> names = {{
        "a", "b", "z", "start", "dpad_up", "dpad_down", "dpad_left", "dpad_right",
        "c_up", "c_down", "c_left", "c_right", "l", "r",
        "analog_up", "analog_down", "analog_left", "analog_right",
    }};
    return names[ToIndex(action)];
}

std::string BindingName(const GamepadBinding & binding)
{
    if (binding.kind == GamepadBinding::Kind::Button)
    {
        return ButtonName(static_cast<Button>(binding.value));
    }
    if (binding.kind == GamepadBinding::Kind::PositiveAxis || binding.kind == GamepadBinding::Kind::NegativeAxis)
    {
        return AxisName(static_cast<Axis>(binding.value)) + (binding.kind == GamepadBinding::Kind::PositiveAxis ? " +" : " -");
    }
    return "None";
}

std::string BindingValue(const GamepadBinding & binding)
{
    if (binding.kind == GamepadBinding::Kind::Button)
    {
        return std::string("button:") + ButtonValue(static_cast<Button>(binding.value));
    }
    if (binding.kind == GamepadBinding::Kind::PositiveAxis || binding.kind == GamepadBinding::Kind::NegativeAxis)
    {
        return std::string("axis:") + (binding.kind == GamepadBinding::Kind::PositiveAxis ? '+' : '-') + AxisName(static_cast<Axis>(binding.value));
    }
    return "none";
}

GamepadBinding ParseBinding(const std::string & value)
{
    const std::string lowered = Lower(Trim(value));
    if (lowered.rfind("button:", 0) == 0)
    {
        const Button button = ParseButton(lowered.substr(7));
        if (button != SDL_CONTROLLER_BUTTON_INVALID)
        {
            return {GamepadBinding::Kind::Button, button};
        }
    }
    if (lowered.rfind("axis:", 0) == 0 && lowered.size() > 6)
    {
        const bool positive = lowered[5] != '-';
        const Axis axis = ParseAxis(lowered.substr(6));
        if (axis != SDL_CONTROLLER_AXIS_INVALID)
        {
            return {positive ? GamepadBinding::Kind::PositiveAxis : GamepadBinding::Kind::NegativeAxis, axis};
        }
    }
    return {};
}

std::string AxisName(SDL_GameControllerAxis axis)
{
    switch (axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX: return "leftx";
    case SDL_CONTROLLER_AXIS_LEFTY: return "lefty";
    case SDL_CONTROLLER_AXIS_RIGHTX: return "rightx";
    case SDL_CONTROLLER_AXIS_RIGHTY: return "righty";
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return "triggerleft";
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "triggerright";
    default: return "leftx";
    }
}

SDL_GameControllerAxis ParseAxis(const std::string & value)
{
    const std::string lowered = Lower(Trim(value));
    if (lowered == "leftx") { return SDL_CONTROLLER_AXIS_LEFTX; }
    if (lowered == "lefty") { return SDL_CONTROLLER_AXIS_LEFTY; }
    if (lowered == "rightx") { return SDL_CONTROLLER_AXIS_RIGHTX; }
    if (lowered == "righty") { return SDL_CONTROLLER_AXIS_RIGHTY; }
    if (lowered == "triggerleft") { return SDL_CONTROLLER_AXIS_TRIGGERLEFT; }
    if (lowered == "triggerright") { return SDL_CONTROLLER_AXIS_TRIGGERRIGHT; }
    return SDL_CONTROLLER_AXIS_INVALID;
}

bool BindingPressed(SDL_GameController * controller, const GamepadBinding & binding, int threshold)
{
    if (controller == nullptr)
    {
        return false;
    }
    if (binding.kind == GamepadBinding::Kind::Button)
    {
        return SDL_GameControllerGetButton(controller, static_cast<Button>(binding.value)) != 0;
    }
    if (binding.kind == GamepadBinding::Kind::PositiveAxis)
    {
        return SDL_GameControllerGetAxis(controller, static_cast<Axis>(binding.value)) > threshold;
    }
    if (binding.kind == GamepadBinding::Kind::NegativeAxis)
    {
        return SDL_GameControllerGetAxis(controller, static_cast<Axis>(binding.value)) < -threshold;
    }
    return false;
}
}
