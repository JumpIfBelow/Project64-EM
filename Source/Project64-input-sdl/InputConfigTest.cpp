#include "InputConfig.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
bool Check(bool condition, const char * message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
    }
    return condition;
}
}

int main()
{
    using namespace pj64::input;
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / ("project64-em-input-test-" + std::to_string(unique));
    const std::filesystem::path path = directory / "LinuxInput.ini";

    InputConfig expected;
    expected.keyboard[static_cast<size_t>(KeyboardAction::A)] = SDL_SCANCODE_SPACE;
    expected.gamepad[static_cast<size_t>(N64Button::B)] = {
        GamepadBinding::Kind::NegativeAxis,
        SDL_CONTROLLER_AXIS_RIGHTX,
    };
    expected.analogX = SDL_CONTROLLER_AXIS_RIGHTY;
    expected.analogY = SDL_CONTROLLER_AXIS_LEFTX;
    expected.invertAnalogX = true;
    expected.invertAnalogY = false;
    expected.deadzone = 4321;
    expected.sensitivity = 97;
    expected.controllerGuid = "03000000123400005678000000000000";

    bool passed = Check(expected.Save(path.string()), "could not save input configuration");
    InputConfig actual;
    passed &= Check(actual.Load(path.string()), "could not load input configuration");
    passed &= Check(actual.keyboard[static_cast<size_t>(KeyboardAction::A)] == SDL_SCANCODE_SPACE,
        "keyboard binding did not round-trip");
    const GamepadBinding & binding = actual.gamepad[static_cast<size_t>(N64Button::B)];
    passed &= Check(binding.kind == GamepadBinding::Kind::NegativeAxis &&
        binding.value == SDL_CONTROLLER_AXIS_RIGHTX, "gamepad binding did not round-trip");
    passed &= Check(actual.analogX == expected.analogX && actual.analogY == expected.analogY,
        "analog axes did not round-trip");
    passed &= Check(actual.invertAnalogX == expected.invertAnalogX &&
        actual.invertAnalogY == expected.invertAnalogY, "axis inversion did not round-trip");
    passed &= Check(actual.deadzone == expected.deadzone && actual.sensitivity == expected.sensitivity,
        "analog tuning did not round-trip");
    passed &= Check(actual.controllerGuid == expected.controllerGuid, "controller selection did not round-trip");

    {
        std::ofstream malformed(path, std::ios::trunc);
        malformed << "[gamepad]\nanalog_x=not-an-axis\nanalog_y=also-invalid\n";
    }
    InputConfig defaults;
    defaults.Load(path.string());
    passed &= Check(defaults.analogX == SDL_CONTROLLER_AXIS_LEFTX &&
        defaults.analogY == SDL_CONTROLLER_AXIS_LEFTY, "invalid axes replaced safe defaults");

    std::error_code error;
    std::filesystem::remove_all(directory, error);
    return passed ? 0 : 1;
}
