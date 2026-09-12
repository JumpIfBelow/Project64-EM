#include <SDL.h>

#include <Common/StdString.h>
#include <Project64-core/AppInit.h>
#include <Project64-core/Multilanguage/Language.h>
#include <Project64-core/N64System/N64System.h>
#include <Project64-core/N64System/SystemGlobals.h>
#include <Project64-core/Plugins/Plugin.h>
#include <Project64-core/Settings.h>

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
    SdlRenderWindow(SDL_Window * window, SDL_GLContext context) :
        m_Window(window),
        m_Context(context)
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
        SDL_GL_SetSwapInterval(1);
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
    std::atomic_bool m_ContextError{false};
};

struct Options
{
    bool showHelp = false;
    bool fullscreen = false;
    std::string inputConfig;
    std::string rom;
};

void PrintUsage(const char * executable)
{
    std::fprintf(stdout,
        "Usage: %s [--fullscreen] [--input-config FILE] ROM\n"
        "\n"
        "F11 toggles fullscreen. Escape exits.\n",
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
            options.fullscreen = true;
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

    if (options.rom.empty())
    {
        PrintUsage(argv[0]);
        return false;
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

void SetInputConfig(const Options & options, const std::string & baseDirectory)
{
    std::filesystem::path configPath = options.inputConfig.empty()
        ? std::filesystem::path(baseDirectory) / "Config" / "LinuxInput.ini"
        : std::filesystem::path(options.inputConfig);
    setenv("PROJECT64_EM_INPUT_CONFIG", configPath.c_str(), 1);
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
    if (!std::filesystem::is_regular_file(options.rom))
    {
        std::fprintf(stderr, "ROM does not exist or is not a regular file: %s\n", options.rom.c_str());
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

    const uint32_t windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
        (options.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_Window * window = SDL_CreateWindow(
        "Project64-EM",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640,
        480,
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

    const std::string baseDirectory = ExecutableDirectory();
    SetInputConfig(options, baseDirectory);
    LinuxNotification notification;
    SdlRenderWindow renderWindow(window, context);

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
        g_Settings->SaveBool(Setting_ForceInterpreterCPU, true);
        g_Plugins->SetRenderWindows(&renderWindow, nullptr);
        SDL_GL_MakeCurrent(window, nullptr);
        romStarted = CN64System::RunFileImage(options.rom.c_str());
    }

    bool fullscreen = options.fullscreen;
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
                else if (event.key.keysym.sym == SDLK_F11)
                {
                    fullscreen = !fullscreen;
                    SetFullscreen(window, fullscreen);
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
