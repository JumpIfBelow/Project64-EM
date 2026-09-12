#include "Launcher.h"

#include "LinuxConfig.h"
#include <Project64-input-sdl/InputConfig.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <SDL_opengl.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
constexpr size_t PathCapacity = 4096;

enum class BrowserPurpose
{
    None,
    Rom,
    SaveDirectory,
    StateDirectory,
    ScreenshotDirectory,
    TextureDirectory,
};

struct BrowserResult
{
    BrowserPurpose purpose = BrowserPurpose::None;
    std::string path;
};

class FileBrowser
{
public:
    void Open(BrowserPurpose purpose, const std::string & initialPath)
    {
        m_Purpose = purpose;
        std::filesystem::path initial(initialPath);
        std::error_code error;
        if (purpose == BrowserPurpose::Rom && std::filesystem::is_regular_file(initial, error))
        {
            initial = initial.parent_path();
        }
        if (!std::filesystem::is_directory(initial, error))
        {
            const char * home = std::getenv("HOME");
            initial = home != nullptr ? std::filesystem::path(home) : std::filesystem::current_path(error);
        }
        m_Current = initial;
        m_Selected.clear();
        Refresh();
        m_OpenRequested = true;
    }

    BrowserResult Draw()
    {
        BrowserResult result;
        if (m_OpenRequested)
        {
            ImGui::OpenPopup("Choose a file or directory");
            m_OpenRequested = false;
        }
        ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Choose a file or directory", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            return result;
        }

        if (ImGui::Button("Home"))
        {
            const char * home = std::getenv("HOME");
            if (home != nullptr)
            {
                m_Current = home;
                m_Selected.clear();
                Refresh();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Up"))
        {
            const std::filesystem::path parent = m_Current.parent_path();
            if (!parent.empty() && parent != m_Current)
            {
                m_Current = parent;
                m_Selected.clear();
                Refresh();
            }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(m_Current.string().c_str());
        if (!m_Error.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", m_Error.c_str());
        }

        ImGui::BeginChild("Files", ImVec2(0.0f, -72.0f), ImGuiChildFlags_Borders);
        for (const Entry & entry : m_Entries)
        {
            const std::string label = std::string(entry.directory ? "[DIR]  " : "       ") + entry.name;
            const bool selected = m_Selected == entry.path;
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                m_Selected = entry.path;
                if (entry.directory && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    m_Current = entry.path;
                    m_Selected.clear();
                    Refresh();
                }
                else if (!entry.directory && m_Purpose == BrowserPurpose::Rom && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    result = {m_Purpose, entry.path.string()};
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();

        const bool directoryMode = m_Purpose != BrowserPurpose::Rom;
        if (directoryMode)
        {
            if (ImGui::Button("Use this directory"))
            {
                std::error_code error;
                const std::filesystem::path selected = std::filesystem::is_directory(m_Selected, error) ? m_Selected : m_Current;
                result = {m_Purpose, selected.string()};
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            std::error_code error;
            const bool validFile = std::filesystem::is_regular_file(m_Selected, error);
            if (!validFile)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Open"))
            {
                result = {m_Purpose, m_Selected.string()};
                ImGui::CloseCurrentPopup();
            }
            if (!validFile)
            {
                ImGui::EndDisabled();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return result;
    }

private:
    struct Entry
    {
        std::filesystem::path path;
        std::string name;
        bool directory;
    };

    static bool IsRom(const std::filesystem::path & path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return extension == ".z64" || extension == ".n64" || extension == ".v64" ||
            extension == ".zip" || extension == ".7z";
    }

    void Refresh()
    {
        m_Entries.clear();
        m_Error.clear();
        std::error_code error;
        std::filesystem::directory_iterator iterator(
            m_Current,
            std::filesystem::directory_options::skip_permission_denied,
            error);
        if (error)
        {
            m_Error = error.message();
            return;
        }
        for (const auto & item : iterator)
        {
            const bool directory = item.is_directory(error);
            if (error)
            {
                error.clear();
                continue;
            }
            if (!directory && m_Purpose == BrowserPurpose::Rom && !IsRom(item.path()))
            {
                continue;
            }
            if (!directory && m_Purpose != BrowserPurpose::Rom)
            {
                continue;
            }
            m_Entries.push_back({item.path(), item.path().filename().string(), directory});
        }
        std::sort(m_Entries.begin(), m_Entries.end(), [](const Entry & left, const Entry & right) {
            if (left.directory != right.directory)
            {
                return left.directory > right.directory;
            }
            return left.name < right.name;
        });
    }

    BrowserPurpose m_Purpose = BrowserPurpose::None;
    bool m_OpenRequested = false;
    std::filesystem::path m_Current;
    std::filesystem::path m_Selected;
    std::vector<Entry> m_Entries;
    std::string m_Error;
};

template <size_t Size>
void CopyText(std::array<char, Size> & destination, const std::string & value)
{
    std::snprintf(destination.data(), destination.size(), "%s", value.c_str());
}

void HelpMarker(const char * text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void AddUiFont(ImGuiIO & io)
{
    static const std::array<const char *, 5> candidates = {{
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    }};
    for (const char * candidate : candidates)
    {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) &&
            io.Fonts->AddFontFromFileTTF(candidate, 16.0f) != nullptr)
        {
            return;
        }
    }
    io.Fonts->AddFontDefault();
}

void PathSetting(
    const char * label,
    std::array<char, PathCapacity> & buffer,
    BrowserPurpose purpose,
    FileBrowser & browser)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-84.0f);
    ImGui::InputText("##path", buffer.data(), buffer.size());
    ImGui::SameLine();
    if (ImGui::Button("Browse"))
    {
        browser.Open(purpose, buffer.data());
    }
    ImGui::PopID();
}

struct BindingOption
{
    const char * label;
    pj64::input::GamepadBinding value;
};

const std::vector<BindingOption> & BindingOptions()
{
    using Binding = pj64::input::GamepadBinding;
    static const std::vector<BindingOption> options = {
        {"None", {}},
        {"A", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_A}},
        {"B", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_B}},
        {"X", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_X}},
        {"Y", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_Y}},
        {"Back", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_BACK}},
        {"Guide", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_GUIDE}},
        {"Start", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_START}},
        {"Left stick click", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_LEFTSTICK}},
        {"Right stick click", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_RIGHTSTICK}},
        {"Left shoulder", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_LEFTSHOULDER}},
        {"Right shoulder", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}},
        {"D-pad up", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_UP}},
        {"D-pad down", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_DOWN}},
        {"D-pad left", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_LEFT}},
        {"D-pad right", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_RIGHT}},
        {"Left X +", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_LEFTX}},
        {"Left X -", {Binding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_LEFTX}},
        {"Left Y +", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_LEFTY}},
        {"Left Y -", {Binding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_LEFTY}},
        {"Right X +", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_RIGHTX}},
        {"Right X -", {Binding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_RIGHTX}},
        {"Right Y +", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_RIGHTY}},
        {"Right Y -", {Binding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_RIGHTY}},
        {"Left trigger", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_TRIGGERLEFT}},
        {"Right trigger", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_TRIGGERRIGHT}},
    };
    return options;
}

bool SameBinding(const pj64::input::GamepadBinding & left, const pj64::input::GamepadBinding & right)
{
    return left.kind == right.kind && left.value == right.value;
}

void DrawKeyboardSettings(pj64::input::InputConfig & input, int & captureIndex)
{
    ImGui::TextWrapped("Click a binding, then press a key. Escape cancels capture. These keys control N64 controller port 1.");
    if (ImGui::BeginTable("Keyboard", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
    {
        for (size_t index = 0; index < input.keyboard.size(); index++)
        {
            if ((index % 2) == 0)
            {
                ImGui::TableNextRow();
            }
            ImGui::TableSetColumnIndex(static_cast<int>((index % 2) * 2));
            ImGui::TextUnformatted(pj64::input::KeyboardActionName(static_cast<pj64::input::KeyboardAction>(index)));
            ImGui::TableSetColumnIndex(static_cast<int>((index % 2) * 2 + 1));
            ImGui::PushID(static_cast<int>(index));
            const std::string label = captureIndex == static_cast<int>(index)
                ? "Press a key..."
                : SDL_GetScancodeName(input.keyboard[index]);
            if (ImGui::Button(label.empty() ? "Unassigned" : label.c_str(), ImVec2(-1.0f, 0.0f)))
            {
                captureIndex = static_cast<int>(index);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void DrawGamepadSettings(pj64::input::InputConfig & input)
{
    std::vector<std::pair<std::string, std::string>> controllers;
    for (int index = 0; index < SDL_NumJoysticks(); index++)
    {
        if (!SDL_IsGameController(index))
        {
            continue;
        }
        char guid[64] = {};
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
        const char * name = SDL_GameControllerNameForIndex(index);
        controllers.emplace_back(name == nullptr ? "Unnamed controller" : name, guid);
    }

    const char * preview = "Any controller";
    for (const auto & controller : controllers)
    {
        if (controller.second == input.controllerGuid)
        {
            preview = controller.first.c_str();
            break;
        }
    }
    if (ImGui::BeginCombo("Preferred controller", preview))
    {
        if (ImGui::Selectable("Any controller", input.controllerGuid.empty()))
        {
            input.controllerGuid.clear();
        }
        for (const auto & controller : controllers)
        {
            if (ImGui::Selectable(controller.first.c_str(), controller.second == input.controllerGuid))
            {
                input.controllerGuid = controller.second;
            }
        }
        ImGui::EndCombo();
    }
    if (controllers.empty())
    {
        ImGui::TextDisabled("No SDL game controller is currently connected.");
    }

    const auto & options = BindingOptions();
    if (ImGui::BeginTable("GamepadBindings", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
    {
        for (size_t index = 0; index < input.gamepad.size(); index++)
        {
            if ((index % 2) == 0)
            {
                ImGui::TableNextRow();
            }
            ImGui::TableSetColumnIndex(static_cast<int>((index % 2) * 2));
            ImGui::TextUnformatted(pj64::input::N64ButtonName(static_cast<pj64::input::N64Button>(index)));
            ImGui::TableSetColumnIndex(static_cast<int>((index % 2) * 2 + 1));
            ImGui::PushID(static_cast<int>(index));
            const std::string currentName = pj64::input::BindingName(input.gamepad[index]);
            if (ImGui::BeginCombo("##binding", currentName.c_str()))
            {
                for (const auto & option : options)
                {
                    const bool selected = SameBinding(option.value, input.gamepad[index]);
                    if (ImGui::Selectable(option.label, selected))
                    {
                        input.gamepad[index] = option.value;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    static const std::array<std::pair<const char *, SDL_GameControllerAxis>, 4> stickAxes = {{
        {"Left X", SDL_CONTROLLER_AXIS_LEFTX}, {"Left Y", SDL_CONTROLLER_AXIS_LEFTY},
        {"Right X", SDL_CONTROLLER_AXIS_RIGHTX}, {"Right Y", SDL_CONTROLLER_AXIS_RIGHTY},
    }};
    auto axisCombo = [&](const char * label, SDL_GameControllerAxis & axis) {
        const std::string current = pj64::input::AxisName(axis);
        if (ImGui::BeginCombo(label, current.c_str()))
        {
            for (const auto & item : stickAxes)
            {
                if (ImGui::Selectable(item.first, item.second == axis))
                {
                    axis = item.second;
                }
            }
            ImGui::EndCombo();
        }
    };
    axisCombo("Analog X axis", input.analogX);
    axisCombo("Analog Y axis", input.analogY);
    ImGui::Checkbox("Invert analog X", &input.invertAnalogX);
    ImGui::SameLine();
    ImGui::Checkbox("Invert analog Y", &input.invertAnalogY);
    ImGui::SliderInt("Deadzone", &input.deadzone, 0, 20000);
    ImGui::SliderInt("Analog sensitivity", &input.sensitivity, 1, 127);
}

void ApplyBrowserResult(
    const BrowserResult & result,
    std::array<char, PathCapacity> & rom,
    std::array<char, PathCapacity> & save,
    std::array<char, PathCapacity> & state,
    std::array<char, PathCapacity> & screenshot,
    std::array<char, PathCapacity> & texture)
{
    switch (result.purpose)
    {
    case BrowserPurpose::Rom: CopyText(rom, result.path); break;
    case BrowserPurpose::SaveDirectory: CopyText(save, result.path); break;
    case BrowserPurpose::StateDirectory: CopyText(state, result.path); break;
    case BrowserPurpose::ScreenshotDirectory: CopyText(screenshot, result.path); break;
    case BrowserPurpose::TextureDirectory: CopyText(texture, result.path); break;
    default: break;
    }
}
}

LauncherResult RunLauncher(
    SDL_Window * window,
    SDL_GLContext context,
    LinuxConfig & config,
    pj64::input::InputConfig & input,
    const std::string & frontendConfigPath,
    const std::string & inputConfigPath,
    volatile std::sig_atomic_t * stopSignal)
{
    SDL_SetWindowTitle(window, "Project64-EM for Linux");
    SDL_SetWindowSize(window, 960, 700);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    AddUiFont(io);
    const std::string imguiConfigPath = (std::filesystem::path(frontendConfigPath).parent_path() / "LinuxFrontendLayout.ini").string();
    io.IniFilename = imguiConfigPath.c_str();
    ImGui::StyleColorsDark();
    ImGuiStyle & style = ImGui::GetStyle();
    style.ScaleAllSizes(1.08f);
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.42f, 0.64f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.52f, 0.77f, 1.0f);

    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 130");

    std::array<char, PathCapacity> rom = {};
    std::array<char, PathCapacity> save = {};
    std::array<char, PathCapacity> state = {};
    std::array<char, PathCapacity> screenshot = {};
    std::array<char, PathCapacity> texture = {};
    CopyText(rom, config.lastRom);
    CopyText(save, config.saveDirectory);
    CopyText(state, config.stateDirectory);
    CopyText(screenshot, config.screenshotDirectory);
    CopyText(texture, config.textureDirectory);

    FileBrowser browser;
    std::string status;
    int captureIndex = -1;
    bool running = true;
    LauncherResult result = LauncherResult::Quit;
    while (running)
    {
        if (stopSignal != nullptr && *stopSignal != 0)
        {
            running = false;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            else if (captureIndex >= 0 && event.type == SDL_KEYDOWN && !event.key.repeat)
            {
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                {
                    captureIndex = -1;
                }
                else
                {
                    input.keyboard[static_cast<size_t>(captureIndex)] = event.key.keysym.scancode;
                    captureIndex = -1;
                }
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
            {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Project64-EM", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        ImGui::Text("Project64-EM");
        ImGui::SameLine();
        ImGui::TextDisabled("Linux frontend - OoTMM multiplayer client r4");
        ImGui::Separator();
        ImGui::TextUnformatted("ROM");
        ImGui::SetNextItemWidth(-84.0f);
        ImGui::InputText("##rom", rom.data(), rom.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse##rom"))
        {
            browser.Open(BrowserPurpose::Rom, rom.data());
        }

        if (!config.recentRoms.empty() && ImGui::BeginCombo("Recent ROMs", "Select a recent ROM"))
        {
            for (const std::string & recent : config.recentRoms)
            {
                if (ImGui::Selectable(recent.c_str()))
                {
                    CopyText(rom, recent);
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginTabBar("Settings"))
        {
            if (ImGui::BeginTabItem("General"))
            {
                ImGui::Checkbox("Start in fullscreen", &config.fullscreen);
                ImGui::SetNextItemWidth(180.0f);
                ImGui::InputInt("Window width", &config.windowWidth, 16, 160);
                ImGui::SetNextItemWidth(180.0f);
                ImGui::InputInt("Window height", &config.windowHeight, 16, 120);
                config.windowWidth = std::clamp(config.windowWidth, 320, 7680);
                config.windowHeight = std::clamp(config.windowHeight, 240, 4320);
                ImGui::Checkbox("Limit emulation speed", &config.limitFps);
                HelpMarker("Disabling this runs the emulator as fast as the interpreter and graphics plugin allow.");
                ImGui::Spacing();
                ImGui::TextWrapped("Runtime: F2 pause/resume, F5 save state, F7 load state, F8 soft reset, F9 speed limit, F11 fullscreen, F12 screenshot, Escape exit.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Video"))
            {
                static const char * aspects[] = {"4:3", "16:9", "Stretch", "Original"};
                static const char * filtering[] = {"Automatic", "Force bilinear", "Force point sampled"};
                static const std::array<std::pair<const char *, int>, 7> textureFilters = {{
                    {"None", 0x00}, {"Smooth 1", 0x01}, {"Smooth 2", 0x02},
                    {"Smooth 3", 0x03}, {"Smooth 4", 0x04}, {"Sharp 1", 0x10}, {"Sharp 2", 0x20},
                }};
                ImGui::Checkbox("Vertical synchronization", &config.vsync);
                ImGui::Combo("Aspect ratio", &config.aspectRatio, aspects, IM_ARRAYSIZE(aspects));
                ImGui::Combo("N64 texture filtering", &config.filtering, filtering, IM_ARRAYSIZE(filtering));
                const char * texturePreview = "None";
                for (const auto & item : textureFilters) { if (item.second == config.textureFilter) { texturePreview = item.first; } }
                if (ImGui::BeginCombo("Texture enhancement filter", texturePreview))
                {
                    for (const auto & item : textureFilters)
                    {
                        if (ImGui::Selectable(item.first, item.second == config.textureFilter)) { config.textureFilter = item.second; }
                    }
                    ImGui::EndCombo();
                }
                ImGui::Checkbox("Load high-resolution texture packs", &config.highResolutionTextures);
                ImGui::Checkbox("Anisotropic filtering", &config.anisotropicFiltering);
                ImGui::TextDisabled("The bundled Glide64/OpenGL plugin remains selected for Linux compatibility.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Audio"))
            {
                ImGui::Checkbox("Enable audio", &config.audioEnabled);
                ImGui::SliderInt("Volume", &config.audioVolume, 0, 100, "%d%%");
                const char * audioPreview = config.audioDevice.empty() ? "System default" : config.audioDevice.c_str();
                if (ImGui::BeginCombo("Output device", audioPreview))
                {
                    if (ImGui::Selectable("System default", config.audioDevice.empty())) { config.audioDevice.clear(); }
                    for (int index = 0; index < SDL_GetNumAudioDevices(0); index++)
                    {
                        const char * device = SDL_GetAudioDeviceName(index, 0);
                        if (device != nullptr && ImGui::Selectable(device, config.audioDevice == device)) { config.audioDevice = device; }
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Keyboard"))
            {
                DrawKeyboardSettings(input, captureIndex);
                if (ImGui::Button("Restore keyboard defaults"))
                {
                    const pj64::input::InputConfig defaults;
                    input.keyboard = defaults.keyboard;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Gamepad"))
            {
                DrawGamepadSettings(input);
                if (ImGui::Button("Restore gamepad defaults"))
                {
                    const pj64::input::InputConfig defaults;
                    input.gamepad = defaults.gamepad;
                    input.analogX = defaults.analogX;
                    input.analogY = defaults.analogY;
                    input.invertAnalogX = defaults.invertAnalogX;
                    input.invertAnalogY = defaults.invertAnalogY;
                    input.deadzone = defaults.deadzone;
                    input.sensitivity = defaults.sensitivity;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Directories"))
            {
                ImGui::TextWrapped("Leave a directory empty to use the portable folders beside the executable.");
                PathSetting("Native saves", save, BrowserPurpose::SaveDirectory, browser);
                PathSetting("Save states", state, BrowserPurpose::StateDirectory, browser);
                PathSetting("Screenshots", screenshot, BrowserPurpose::ScreenshotDirectory, browser);
                PathSetting("Texture packs", texture, BrowserPurpose::TextureDirectory, browser);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        const BrowserResult browserResult = browser.Draw();
        if (browserResult.purpose != BrowserPurpose::None)
        {
            ApplyBrowserResult(browserResult, rom, save, state, screenshot, texture);
        }

        config.saveDirectory = save.data();
        config.stateDirectory = state.data();
        config.screenshotDirectory = screenshot.data();
        config.textureDirectory = texture.data();
        std::error_code pathError;
        const bool validRom = std::filesystem::is_regular_file(rom.data(), pathError);
        if (!status.empty())
        {
            ImGui::TextWrapped("%s", status.c_str());
        }
        if (!validRom)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Launch ROM", ImVec2(150.0f, 36.0f)))
        {
            config.AddRecentRom(rom.data());
            const bool frontendSaved = config.Save(frontendConfigPath);
            const bool inputSaved = input.Save(inputConfigPath);
            if (frontendSaved && inputSaved)
            {
                result = LauncherResult::Launch;
                running = false;
            }
            else
            {
                status = "Unable to save one or more configuration files.";
            }
        }
        if (!validRom)
        {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save settings", ImVec2(150.0f, 36.0f)))
        {
            config.lastRom = rom.data();
            status = config.Save(frontendConfigPath) && input.Save(inputConfigPath)
                ? "Settings saved."
                : "Unable to save one or more configuration files.";
        }
        ImGui::SameLine();
        if (ImGui::Button("Quit", ImVec2(100.0f, 36.0f)))
        {
            running = false;
        }
        ImGui::End();

        ImGui::Render();
        int displayWidth = 0;
        int displayHeight = 0;
        SDL_GL_GetDrawableSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.055f, 0.067f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_SetWindowTitle(window, "Project64-EM");
    return result;
}
