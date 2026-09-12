#include "InputConfig.h"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

#if defined(__GNUC__)
#define EXPORT extern "C" __attribute__((visibility("default")))
#else
#define EXPORT extern "C"
#endif

enum
{
    PLUGIN_TYPE_CONTROLLER = 4,
    PLUGIN_NONE = 1,
};

struct PLUGIN_INFO
{
    uint16_t Version;
    uint16_t Type;
    char Name[100];
    int32_t NormalMemory;
    int32_t MemoryBswaped;
};

struct CONTROL
{
    int32_t Present;
    int32_t RawData;
    int32_t Plugin;
};

struct CONTROL_INFO
{
    void * hMainWindow;
    void * hinst;
    int32_t MemoryBswaped;
    uint8_t * HEADER;
    CONTROL * Controls;
};

union BUTTONS
{
    uint32_t Value;
    struct
    {
        unsigned R_DPAD : 1;
        unsigned L_DPAD : 1;
        unsigned D_DPAD : 1;
        unsigned U_DPAD : 1;
        unsigned START_BUTTON : 1;
        unsigned Z_TRIG : 1;
        unsigned B_BUTTON : 1;
        unsigned A_BUTTON : 1;
        unsigned R_CBUTTON : 1;
        unsigned L_CBUTTON : 1;
        unsigned D_CBUTTON : 1;
        unsigned U_CBUTTON : 1;
        unsigned R_TRIG : 1;
        unsigned L_TRIG : 1;
        unsigned Reserved1 : 1;
        unsigned Reserved2 : 1;
        signed X_AXIS : 8;
        signed Y_AXIS : 8;
    };
};

namespace
{
using pj64::input::InputConfig;
using pj64::input::KeyboardAction;
using pj64::input::N64Button;

InputConfig g_Config;
SDL_GameController * g_Controller = nullptr;
std::filesystem::file_time_type g_ConfigWriteTime{};
std::chrono::steady_clock::time_point g_NextConfigCheck{};

constexpr size_t ToIndex(N64Button button)
{
    return static_cast<size_t>(button);
}

constexpr size_t ToIndex(KeyboardAction action)
{
    return static_cast<size_t>(action);
}

void LoadConfig()
{
    g_Config = InputConfig{};
    const char * configPath = std::getenv("PROJECT64_EM_INPUT_CONFIG");
    if (configPath != nullptr && configPath[0] != '\0')
    {
        g_Config.Load(configPath);
        std::error_code error;
        g_ConfigWriteTime = std::filesystem::last_write_time(configPath, error);
    }
}

void CloseController()
{
    if (g_Controller == nullptr)
    {
        return;
    }
    SDL_GameControllerRumble(g_Controller, 0, 0, 0);
    SDL_GameControllerClose(g_Controller);
    g_Controller = nullptr;
}

void ReloadConfigIfChanged()
{
    const auto now = std::chrono::steady_clock::now();
    if (now < g_NextConfigCheck)
    {
        return;
    }
    g_NextConfigCheck = now + std::chrono::milliseconds(250);

    const char * configPath = std::getenv("PROJECT64_EM_INPUT_CONFIG");
    if (configPath == nullptr || configPath[0] == '\0')
    {
        return;
    }
    std::error_code error;
    const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(configPath, error);
    if (error || writeTime == g_ConfigWriteTime)
    {
        return;
    }
    InputConfig updated;
    if (!updated.Load(configPath))
    {
        return;
    }
    const bool controllerChanged = updated.controllerGuid != g_Config.controllerGuid;
    g_Config = std::move(updated);
    g_ConfigWriteTime = writeTime;
    if (controllerChanged)
    {
        CloseController();
    }
}

std::string DeviceGuid(int index)
{
    char text[64] = {};
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), text, sizeof(text));
    return text;
}

void OpenController()
{
    if (g_Controller != nullptr && !SDL_GameControllerGetAttached(g_Controller))
    {
        SDL_GameControllerClose(g_Controller);
        g_Controller = nullptr;
    }
    if (g_Controller != nullptr)
    {
        return;
    }

    int fallback = -1;
    for (int index = 0; index < SDL_NumJoysticks(); index++)
    {
        if (!SDL_IsGameController(index))
        {
            continue;
        }
        if (fallback < 0)
        {
            fallback = index;
        }
        if (!g_Config.controllerGuid.empty() && DeviceGuid(index) != g_Config.controllerGuid)
        {
            continue;
        }
        g_Controller = SDL_GameControllerOpen(index);
        if (g_Controller != nullptr)
        {
            return;
        }
    }
    if (g_Controller == nullptr && fallback >= 0)
    {
        g_Controller = SDL_GameControllerOpen(fallback);
    }
}

bool Pressed(const uint8_t * keys, KeyboardAction action)
{
    const SDL_Scancode code = g_Config.keyboard[ToIndex(action)];
    return keys != nullptr && code > SDL_SCANCODE_UNKNOWN && code < SDL_NUM_SCANCODES && keys[code] != 0;
}

bool GamepadPressed(N64Button button)
{
    return pj64::input::BindingPressed(g_Controller, g_Config.gamepad[ToIndex(button)]);
}

int8_t ScaleAxis(SDL_GameControllerAxis axis, bool invert)
{
    if (g_Controller == nullptr || axis == SDL_CONTROLLER_AXIS_INVALID)
    {
        return 0;
    }
    int value = SDL_GameControllerGetAxis(g_Controller, axis);
    if (std::abs(value) <= g_Config.deadzone)
    {
        return 0;
    }
    if (invert)
    {
        value = -value;
    }
    const int scaled = value * g_Config.sensitivity / 32767;
    return static_cast<int8_t>(std::clamp(scaled, -g_Config.sensitivity, g_Config.sensitivity));
}
}

EXPORT void CloseDLL()
{
    CloseController();
}

EXPORT void GetDllInfo(PLUGIN_INFO * info)
{
    info->Version = 0x0101;
    info->Type = PLUGIN_TYPE_CONTROLLER;
    SDL_strlcpy(info->Name, "Project64-EM SDL input", sizeof(info->Name));
    info->NormalMemory = 1;
    info->MemoryBswaped = 1;
}

EXPORT void InitiateControllers(CONTROL_INFO info)
{
    LoadConfig();
    if ((SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER) == 0)
    {
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
    OpenController();
    for (int index = 0; index < 4; index++)
    {
        info.Controls[index].Present = index == 0 ? 1 : 0;
        info.Controls[index].RawData = 0;
        info.Controls[index].Plugin = index == 0 ? static_cast<int32_t>(g_Config.controllerPak) : PLUGIN_NONE;
    }
}

EXPORT void GetKeys(int control, BUTTONS * buttons)
{
    buttons->Value = 0;
    if (control != 0)
    {
        return;
    }

    ReloadConfigIfChanged();
    const uint8_t * keys = SDL_GetKeyboardState(nullptr);
    OpenController();
    SDL_GameControllerUpdate();
    buttons->A_BUTTON = Pressed(keys, KeyboardAction::A) || GamepadPressed(N64Button::A);
    buttons->B_BUTTON = Pressed(keys, KeyboardAction::B) || GamepadPressed(N64Button::B);
    buttons->Z_TRIG = Pressed(keys, KeyboardAction::Z) || GamepadPressed(N64Button::Z);
    buttons->START_BUTTON = Pressed(keys, KeyboardAction::Start) || GamepadPressed(N64Button::Start);
    buttons->U_DPAD = Pressed(keys, KeyboardAction::DpadUp) || GamepadPressed(N64Button::DpadUp);
    buttons->D_DPAD = Pressed(keys, KeyboardAction::DpadDown) || GamepadPressed(N64Button::DpadDown);
    buttons->L_DPAD = Pressed(keys, KeyboardAction::DpadLeft) || GamepadPressed(N64Button::DpadLeft);
    buttons->R_DPAD = Pressed(keys, KeyboardAction::DpadRight) || GamepadPressed(N64Button::DpadRight);
    buttons->U_CBUTTON = Pressed(keys, KeyboardAction::CUp) || GamepadPressed(N64Button::CUp);
    buttons->D_CBUTTON = Pressed(keys, KeyboardAction::CDown) || GamepadPressed(N64Button::CDown);
    buttons->L_CBUTTON = Pressed(keys, KeyboardAction::CLeft) || GamepadPressed(N64Button::CLeft);
    buttons->R_CBUTTON = Pressed(keys, KeyboardAction::CRight) || GamepadPressed(N64Button::CRight);
    buttons->L_TRIG = Pressed(keys, KeyboardAction::L) || GamepadPressed(N64Button::L);
    buttons->R_TRIG = Pressed(keys, KeyboardAction::R) || GamepadPressed(N64Button::R);

    int x = (Pressed(keys, KeyboardAction::AnalogRight) - Pressed(keys, KeyboardAction::AnalogLeft)) * g_Config.sensitivity;
    int y = (Pressed(keys, KeyboardAction::AnalogUp) - Pressed(keys, KeyboardAction::AnalogDown)) * g_Config.sensitivity;
    if (g_Controller != nullptr && SDL_GameControllerGetAttached(g_Controller))
    {
        const int controllerX = ScaleAxis(g_Config.analogX, g_Config.invertAnalogX);
        const int controllerY = ScaleAxis(g_Config.analogY, g_Config.invertAnalogY);
        if (controllerX != 0) { x = controllerX; }
        if (controllerY != 0) { y = controllerY; }
    }
    buttons->X_AXIS = static_cast<int8_t>(x);
    buttons->Y_AXIS = static_cast<int8_t>(y);
}

EXPORT void ControllerCommand(int, uint8_t *) {}
EXPORT void ReadController(int, uint8_t *) {}
EXPORT void RumbleCommand(int32_t control, int32_t enabled)
{
    if (control != 0)
    {
        return;
    }
    OpenController();
    if (g_Controller == nullptr || !SDL_GameControllerGetAttached(g_Controller))
    {
        return;
    }
    const Uint16 strength = enabled != 0 ? 0xFFFF : 0;
    SDL_GameControllerRumble(g_Controller, strength, strength, enabled != 0 ? SDL_HAPTIC_INFINITY : 0);
}
EXPORT void RomOpen() {}
EXPORT void RomClosed() {}
EXPORT void WM_KeyDown(uint32_t, uint32_t) {}
EXPORT void WM_KeyUp(uint32_t, uint32_t) {}
EXPORT void DllConfig(void *) {}
EXPORT void DllAbout(void *) {}
