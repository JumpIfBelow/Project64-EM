#include <SDL.h>

#include "Launcher.h"
#include "LinuxConfig.h"

#include <Common/StdString.h>
#include <Project64-core/AppInit.h>
#include <Project64-core/Multilanguage/Language.h>
#include <Project64-core/N64System/N64System.h>
#include <Project64-core/N64System/Mips/SystemEvents.h>
#include <Project64-core/N64System/SystemGlobals.h>
#include <Project64-core/Plugins/GFXPlugin.h>
#include <Project64-core/Plugins/Plugin.h>
#include <Project64-core/Settings.h>
#include <Project64-input-sdl/InputConfig.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
volatile std::sig_atomic_t g_SignalReceived = 0;

void HandleSignal(int)
{
    g_SignalReceived = 1;
}

class LinuxNotification final : public CNotification
{
public:
    void DisplayError(const char * message) const override
    {
        std::fprintf(stderr, "Error: %s\n", message != nullptr ? message : "Unknown error");
    }

    void DisplayError(LanguageStringID stringId) const override
    {
        DisplayError(LanguageString(stringId));
    }

    void FatalError(const char * message) const override
    {
        DisplayError(message);
        m_StopRequested = true;
    }

    void FatalError(LanguageStringID stringId) const override
    {
        FatalError(LanguageString(stringId));
    }

    void DisplayWarning(const char * message) const override
    {
        std::fprintf(stderr, "Warning: %s\n", message != nullptr ? message : "Unknown warning");
    }

    void DisplayWarning(LanguageStringID stringId) const override
    {
        DisplayWarning(LanguageString(stringId));
    }

    void DisplayMessage(int, const char * message) const override
    {
        if (message != nullptr && message[0] != '\0')
        {
            std::fprintf(stdout, "%s\n", message);
        }
    }

    void DisplayMessage(int displayTime, LanguageStringID stringId) const override
    {
        DisplayMessage(displayTime, LanguageString(stringId));
    }

    void DisplayMessage2(const char * message) const override
    {
        DisplayMessage(0, message);
    }

    bool AskYesNoQuestion(const char * question) const override
    {
        DisplayWarning(question);
        return false;
    }

    void BreakPoint(const char * fileName, int32_t lineNumber) override
    {
        std::fprintf(stderr, "Fatal error at %s:%d\n", fileName != nullptr ? fileName : "unknown", lineNumber);
        m_StopRequested = true;
    }

    void AppInitDone() override
    {
    }

    bool ProcessGuiMessages() const override
    {
        return false;
    }

    void ChangeFullScreen() const override
    {
        m_FullscreenRequested = true;
    }

    bool TakeFullscreenRequest()
    {
        return m_FullscreenRequested.exchange(false);
    }

    bool StopRequested() const
    {
        return m_StopRequested;
    }

private:
    static const char * LanguageString(LanguageStringID stringId)
    {
        return g_Lang != nullptr ? g_Lang->GetString(stringId).c_str() : "Project64-EM error";
    }

    mutable std::atomic_bool m_FullscreenRequested{false};
    mutable std::atomic_bool m_StopRequested{false};
};

class SdlRenderWindow final : public RenderWindow
{
public:
    SdlRenderWindow(SDL_Window * window, SDL_GLContext context, bool vsync) :
        m_Window(window),
        m_Context(context),
        m_Vsync(vsync)
    {
    }

    void GfxThreadInit() override
    {
        if (SDL_GL_MakeCurrent(m_Window, m_Context) != 0)
        {
            std::fprintf(stderr, "Unable to activate the OpenGL context: %s\n", SDL_GetError());
            m_ContextError = true;
            return;
        }
        SDL_GL_SetSwapInterval(m_Vsync ? 1 : 0);
    }

    void GfxThreadDone() override
    {
        SDL_GL_MakeCurrent(m_Window, nullptr);
    }

    void SwapWindow() override
    {
        if (!m_ContextError)
        {
            SDL_GL_SwapWindow(m_Window);
        }
    }

    bool ContextError() const
    {
        return m_ContextError;
    }

private:
    SDL_Window * m_Window;
    SDL_GLContext m_Context;
    bool m_Vsync;
    std::atomic_bool m_ContextError{false};
};

struct Options
{
    bool showHelp = false;
    bool showLauncher = false;
    bool fullscreenRequested = false;
    bool windowedRequested = false;
    std::string inputConfig;
    std::string rom;
};

void PrintUsage(const char * executable)
{
    std::fprintf(stdout,
        "Usage: %s [--configure] [--fullscreen|--windowed] [--input-config FILE] [ROM]\n"
        "\n"
        "Without a ROM, the Linux launcher opens. Direct ROM paths skip the launcher.\n"
        "Runtime: F2 pause, F5 save, F7 load, F8 reset, F9 speed limit,\n"
        "F11 fullscreen, F12 screenshot, Escape exit.\n",
        executable);
}

bool ParseOptions(int argc, char ** argv, Options & options)
{
    for (int index = 1; index < argc; index++)
    {
        const std::string argument(argv[index]);
        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
            return true;
        }
        if (argument == "--fullscreen")
        {
            options.fullscreenRequested = true;
            continue;
        }
        if (argument == "--windowed")
        {
            options.windowedRequested = true;
            continue;
        }
        if (argument == "--configure")
        {
            options.showLauncher = true;
            continue;
        }
        if (argument == "--input-config")
        {
            if (++index >= argc)
            {
                std::fprintf(stderr, "--input-config requires a file path\n");
                return false;
            }
            options.inputConfig = argv[index];
            continue;
        }
        if (!argument.empty() && argument[0] == '-')
        {
            std::fprintf(stderr, "Unknown option: %s\n", argument.c_str());
            return false;
        }
        if (!options.rom.empty())
        {
            std::fprintf(stderr, "Only one ROM can be opened at a time\n");
            return false;
        }
        options.rom = argument;
    }

    return true;
}

std::string ExecutableDirectory()
{
    char * basePath = SDL_GetBasePath();
    if (basePath == nullptr)
    {
        return std::filesystem::current_path().string();
    }
    std::string result(basePath);
    SDL_free(basePath);
    return result;
}

std::filesystem::path UserConfigDirectory(const std::string & baseDirectory)
{
    const char * xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig != nullptr && xdgConfig[0] != '\0' && std::filesystem::path(xdgConfig).is_absolute())
    {
        return std::filesystem::path(xdgConfig) / "project64-em";
    }
    const char * home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0')
    {
        return std::filesystem::path(home) / ".config" / "project64-em";
    }
    return std::filesystem::path(baseDirectory) / "Config";
}

std::string InputConfigPath(const Options & options, const std::filesystem::path & userConfigDirectory)
{
    return (options.inputConfig.empty()
        ? userConfigDirectory / "LinuxInput.ini"
        : std::filesystem::path(options.inputConfig)).string();
}

bool SetFullscreen(SDL_Window * window, bool fullscreen)
{
    const uint32_t flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(window, flags) != 0)
    {
        std::fprintf(stderr, "Unable to change fullscreen mode: %s\n", SDL_GetError());
        return false;
    }
    return true;
}
}

int main(int argc, char ** argv)
{
    Options options;
    if (!ParseOptions(argc, argv, options))
    {
        return EXIT_FAILURE;
    }
    if (options.showHelp)
    {
        PrintUsage(argv[0]);
        return EXIT_SUCCESS;
    }
    if (!options.showLauncher && !options.rom.empty() && !std::filesystem::is_regular_file(options.rom))
    {
        std::fprintf(stderr, "ROM does not exist or is not a regular file: %s\n", options.rom.c_str());
        return EXIT_FAILURE;
    }
    if (options.fullscreenRequested && options.windowedRequested)
    {
        std::fprintf(stderr, "--fullscreen and --windowed cannot be used together\n");
        return EXIT_FAILURE;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        std::fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    const std::string baseDirectory = ExecutableDirectory();
    const std::filesystem::path userConfigDirectory = UserConfigDirectory(baseDirectory);
    const std::string frontendConfigPath = (userConfigDirectory / "LinuxFrontend.ini").string();
    const std::string inputConfigPath = InputConfigPath(options, userConfigDirectory);
    LinuxConfig config;
    config.Load(frontendConfigPath);
    pj64::input::InputConfig input;
    if (!input.Load(inputConfigPath) && options.inputConfig.empty())
    {
        input.Load((std::filesystem::path(baseDirectory) / "Config" / "LinuxInput.ini").string());
        input.Save(inputConfigPath);
    }
    if (!options.rom.empty())
    {
        config.lastRom = options.rom;
    }
    if (options.fullscreenRequested)
    {
        config.fullscreen = true;
    }
    if (options.windowedRequested)
    {
        config.fullscreen = false;
    }

    const bool showLauncher = options.showLauncher || options.rom.empty();
    const uint32_t windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
        (!showLauncher && config.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_Window * window = SDL_CreateWindow(
        "Project64-EM",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        showLauncher ? 960 : config.windowWidth,
        showLauncher ? 700 : config.windowHeight,
        windowFlags);
    if (window == nullptr)
    {
        std::fprintf(stderr, "Unable to create the SDL window: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr)
    {
        std::fprintf(stderr, "Unable to create an OpenGL context: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (showLauncher && RunLauncher(window, context, config, input, frontendConfigPath, inputConfigPath, &g_SignalReceived) != LauncherResult::Launch)
    {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_SUCCESS;
    }
    options.rom = showLauncher ? config.lastRom : options.rom;
    if (!std::filesystem::is_regular_file(options.rom))
    {
        std::fprintf(stderr, "ROM does not exist or is not a regular file: %s\n", options.rom.c_str());
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    if (!showLauncher)
    {
        config.AddRecentRom(options.rom);
        config.Save(frontendConfigPath);
    }
    config.ApplyEnvironment(inputConfigPath);
    config.ApplyProjectSettings(baseDirectory);

    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0)
    {
        SetFullscreen(window, false);
    }
    SDL_SetWindowSize(window, config.windowWidth, config.windowHeight);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    if (config.fullscreen)
    {
        SetFullscreen(window, true);
    }
    SDL_SetWindowTitle(window, std::filesystem::path(options.rom).filename().string().c_str());

    LinuxNotification notification;
    SdlRenderWindow renderWindow(window, context, config.vsync);

    std::vector<char *> coreArguments;
    coreArguments.push_back(argv[0]);
    coreArguments.push_back(options.rom.data());

    bool appInitialized = AppInit(
        &notification,
        baseDirectory.c_str(),
        static_cast<int>(coreArguments.size()),
        coreArguments.data());
    bool romStarted = false;
    if (appInitialized)
    {
        config.ApplyProjectSettings(baseDirectory);
        g_Plugins->SetRenderWindows(&renderWindow, nullptr);
        SDL_GL_MakeCurrent(window, nullptr);
        romStarted = CN64System::RunFileImage(options.rom.c_str());
    }

    bool fullscreen = config.fullscreen;
    bool sawRunningCpu = false;
    bool quit = !appInitialized || !romStarted;
    while (!quit)
    {
        if (g_SignalReceived != 0)
        {
            quit = true;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN && !event.key.repeat)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    quit = true;
                }
                else if (event.key.keysym.sym == SDLK_F2 && g_BaseSystem != nullptr)
                {
                    const bool paused = g_Settings->LoadBool(GameRunning_CPU_Paused);
                    g_BaseSystem->ExternalEvent(paused ? SysEvent_ResumeCPU_FromMenu : SysEvent_PauseCPU_FromMenu);
                }
                else if (event.key.keysym.sym == SDLK_F5 && g_BaseSystem != nullptr)
                {
                    g_BaseSystem->ExternalEvent(SysEvent_SaveMachineState);
                }
                else if (event.key.keysym.sym == SDLK_F7 && g_BaseSystem != nullptr)
                {
                    g_BaseSystem->ExternalEvent(SysEvent_LoadMachineState);
                }
                else if (event.key.keysym.sym == SDLK_F8 && g_BaseSystem != nullptr)
                {
                    g_BaseSystem->ExternalEvent(SysEvent_ResetCPU_Soft);
                }
                else if (event.key.keysym.sym == SDLK_F9 && g_Settings != nullptr)
                {
                    g_Settings->SaveBool(GameRunning_LimitFPS, !g_Settings->LoadBool(GameRunning_LimitFPS));
                }
                else if (event.key.keysym.sym == SDLK_F11)
                {
                    fullscreen = !fullscreen;
                    SetFullscreen(window, fullscreen);
                }
                else if (event.key.keysym.sym == SDLK_F12 && g_Settings != nullptr &&
                    g_Plugins != nullptr && g_Plugins->Gfx() != nullptr && g_Plugins->Gfx()->CaptureScreen != nullptr)
                {
                    const std::string screenshotDirectory = g_Settings->LoadStringVal(Directory_SnapShot);
                    g_Plugins->Gfx()->CaptureScreen(screenshotDirectory.c_str());
                }
            }
        }

        if (notification.TakeFullscreenRequest())
        {
            fullscreen = !fullscreen;
            SetFullscreen(window, fullscreen);
        }
        if (notification.StopRequested() || renderWindow.ContextError())
        {
            quit = true;
        }

        const bool cpuRunning = g_Settings != nullptr && g_Settings->LoadBool(GameRunning_CPU_Running);
        const bool loading = g_Settings != nullptr && g_Settings->LoadBool(GameRunning_LoadingInProgress);
        sawRunningCpu = sawRunningCpu || cpuRunning;
        if (sawRunningCpu && !cpuRunning && !loading)
        {
            quit = true;
        }
        SDL_Delay(1);
    }

    if (g_BaseSystem != nullptr)
    {
        g_BaseSystem->CloseCpu();
        delete g_BaseSystem;
        g_BaseSystem = nullptr;
    }
    if (appInitialized)
    {
        AppCleanup();
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return appInitialized && romStarted ? EXIT_SUCCESS : EXIT_FAILURE;
}
