#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

#if defined(__GNUC__)
#define EXPORT extern "C" __attribute__((visibility("default")))
#else
#define EXPORT extern "C"
#endif

enum
{
    PLUGIN_TYPE_CONTROLLER = 4,
    PLUGIN_NONE = 1,
    PLUGIN_MEMPAK = 2,
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

struct InputConfig
{
    SDL_Scancode a = SDL_SCANCODE_X;
    SDL_Scancode b = SDL_SCANCODE_C;
    SDL_Scancode z = SDL_SCANCODE_Z;
    SDL_Scancode start = SDL_SCANCODE_RETURN;
    SDL_Scancode dpadUp = SDL_SCANCODE_UP;
    SDL_Scancode dpadDown = SDL_SCANCODE_DOWN;
    SDL_Scancode dpadLeft = SDL_SCANCODE_LEFT;
    SDL_Scancode dpadRight = SDL_SCANCODE_RIGHT;
    SDL_Scancode cUp = SDL_SCANCODE_I;
    SDL_Scancode cDown = SDL_SCANCODE_K;
    SDL_Scancode cLeft = SDL_SCANCODE_J;
    SDL_Scancode cRight = SDL_SCANCODE_L;
    SDL_Scancode triggerL = SDL_SCANCODE_Q;
    SDL_Scancode triggerR = SDL_SCANCODE_E;
    SDL_Scancode analogUp = SDL_SCANCODE_W;
    SDL_Scancode analogDown = SDL_SCANCODE_S;
    SDL_Scancode analogLeft = SDL_SCANCODE_A;
    SDL_Scancode analogRight = SDL_SCANCODE_D;
    int deadzone = 8000;
    int sensitivity = 80;
};

static InputConfig g_Config;
static SDL_GameController * g_Controller = nullptr;

static std::string Trim(std::string value)
{
    auto space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) { return !space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) { return !space(ch); }).base(), value.end());
    return value;
}

static void SetKey(const std::string & name, const std::string & value)
{
    SDL_Scancode code = SDL_GetScancodeFromName(value.c_str());
    if (code == SDL_SCANCODE_UNKNOWN)
    {
        return;
    }
    if (name == "a") { g_Config.a = code; }
    else if (name == "b") { g_Config.b = code; }
    else if (name == "z") { g_Config.z = code; }
    else if (name == "start") { g_Config.start = code; }
    else if (name == "dpad_up") { g_Config.dpadUp = code; }
    else if (name == "dpad_down") { g_Config.dpadDown = code; }
    else if (name == "dpad_left") { g_Config.dpadLeft = code; }
    else if (name == "dpad_right") { g_Config.dpadRight = code; }
    else if (name == "c_up") { g_Config.cUp = code; }
    else if (name == "c_down") { g_Config.cDown = code; }
    else if (name == "c_left") { g_Config.cLeft = code; }
    else if (name == "c_right") { g_Config.cRight = code; }
    else if (name == "l") { g_Config.triggerL = code; }
    else if (name == "r") { g_Config.triggerR = code; }
    else if (name == "analog_up") { g_Config.analogUp = code; }
    else if (name == "analog_down") { g_Config.analogDown = code; }
    else if (name == "analog_left") { g_Config.analogLeft = code; }
    else if (name == "analog_right") { g_Config.analogRight = code; }
}

static void LoadConfig()
{
    const char * configPath = std::getenv("PROJECT64_EM_INPUT_CONFIG");
    if (configPath == nullptr || configPath[0] == '\0')
    {
        return;
    }
    std::ifstream config(configPath);
    std::string line;
    while (std::getline(config, line))
    {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[')
        {
            continue;
        }
        size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }
        std::string name = Trim(line.substr(0, equals));
        std::string value = Trim(line.substr(equals + 1));
        if (name == "deadzone")
        {
            g_Config.deadzone = std::clamp(std::atoi(value.c_str()), 0, 32767);
        }
        else if (name == "sensitivity")
        {
            g_Config.sensitivity = std::clamp(std::atoi(value.c_str()), 1, 127);
        }
        else
        {
            SetKey(name, value);
        }
    }
}

static void OpenController()
{
    if (g_Controller != nullptr)
    {
        return;
    }
    for (int index = 0; index < SDL_NumJoysticks(); index++)
    {
        if (SDL_IsGameController(index))
        {
            g_Controller = SDL_GameControllerOpen(index);
            if (g_Controller != nullptr)
            {
                break;
            }
        }
    }
}

static bool Pressed(const uint8_t * keys, SDL_Scancode code)
{
    return keys != nullptr && keys[code] != 0;
}

static int8_t ScaleAxis(int16_t value)
{
    if (std::abs(static_cast<int>(value)) <= g_Config.deadzone)
    {
        return 0;
    }
    int scaled = value * g_Config.sensitivity / 32767;
    return static_cast<int8_t>(std::clamp(scaled, -g_Config.sensitivity, g_Config.sensitivity));
}

EXPORT void CloseDLL()
{
    if (g_Controller != nullptr)
    {
        SDL_GameControllerClose(g_Controller);
        g_Controller = nullptr;
    }
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
        info.Controls[index].Plugin = index == 0 ? PLUGIN_MEMPAK : PLUGIN_NONE;
    }
}

EXPORT void GetKeys(int control, BUTTONS * buttons)
{
    buttons->Value = 0;
    if (control != 0)
    {
        return;
    }

    const uint8_t * keys = SDL_GetKeyboardState(nullptr);
    buttons->A_BUTTON = Pressed(keys, g_Config.a);
    buttons->B_BUTTON = Pressed(keys, g_Config.b);
    buttons->Z_TRIG = Pressed(keys, g_Config.z);
    buttons->START_BUTTON = Pressed(keys, g_Config.start);
    buttons->U_DPAD = Pressed(keys, g_Config.dpadUp);
    buttons->D_DPAD = Pressed(keys, g_Config.dpadDown);
    buttons->L_DPAD = Pressed(keys, g_Config.dpadLeft);
    buttons->R_DPAD = Pressed(keys, g_Config.dpadRight);
    buttons->U_CBUTTON = Pressed(keys, g_Config.cUp);
    buttons->D_CBUTTON = Pressed(keys, g_Config.cDown);
    buttons->L_CBUTTON = Pressed(keys, g_Config.cLeft);
    buttons->R_CBUTTON = Pressed(keys, g_Config.cRight);
    buttons->L_TRIG = Pressed(keys, g_Config.triggerL);
    buttons->R_TRIG = Pressed(keys, g_Config.triggerR);

    int x = (Pressed(keys, g_Config.analogRight) - Pressed(keys, g_Config.analogLeft)) * g_Config.sensitivity;
    int y = (Pressed(keys, g_Config.analogUp) - Pressed(keys, g_Config.analogDown)) * g_Config.sensitivity;

    OpenController();
    if (g_Controller != nullptr && SDL_GameControllerGetAttached(g_Controller))
    {
        buttons->A_BUTTON |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_A);
        buttons->B_BUTTON |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_X);
        buttons->START_BUTTON |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_START);
        buttons->U_DPAD |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
        buttons->D_DPAD |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        buttons->L_DPAD |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        buttons->R_DPAD |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        buttons->L_TRIG |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        buttons->R_TRIG |= SDL_GameControllerGetButton(g_Controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        buttons->Z_TRIG |= SDL_GameControllerGetAxis(g_Controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000;
        buttons->U_CBUTTON |= SDL_GameControllerGetAxis(g_Controller, SDL_CONTROLLER_AXIS_RIGHTY) < -16000;
        buttons->D_CBUTTON |= SDL_GameControllerGetAxis(g_Controller, SDL_CONTROLLER_AXIS_RIGHTY) > 16000;
        buttons->L_CBUTTON |= SDL_GameControllerGetAxis(g_Controller, SDL_CONTROLLER_AXIS_RIGHTX) < -16000;
        buttons->R_CBUTTON |= SDL_GameControllerGetAxis(g_Controller, SDL_CONTROLLER_AXIS_RIGHTX) > 16000;
        x = ScaleAxis(SDL_GameControllerGetAxis(g_Controller, SDL_CONTROLLER_AXIS_LEFTX));
        y = -ScaleAxis(SDL_GameControllerGetAxis(g_Controller, SDL_CONTROLLER_AXIS_LEFTY));
    }

    buttons->X_AXIS = static_cast<int8_t>(x);
    buttons->Y_AXIS = static_cast<int8_t>(y);
}

EXPORT void ControllerCommand(int, uint8_t *) {}
EXPORT void ReadController(int, uint8_t *) {}
EXPORT void RomOpen() {}
EXPORT void RomClosed() {}
EXPORT void WM_KeyDown(uint32_t, uint32_t) {}
EXPORT void WM_KeyUp(uint32_t, uint32_t) {}
EXPORT void DllConfig(void *) {}
EXPORT void DllAbout(void *) {}
