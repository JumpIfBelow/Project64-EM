#pragma once

#include <csignal>
#include <memory>
#include <string>

struct LinuxConfig;
struct SDL_Window;

namespace pj64::input
{
struct InputConfig;
}

enum class LauncherResult
{
    Launch,
    Quit,
};

enum class RuntimeCommand
{
    Idle,
    PauseResume,
    SaveState,
    LoadState,
    SoftReset,
    ToggleSpeedLimit,
    ToggleFullscreen,
    Screenshot,
    Settings,
    Quit,
};

bool ShowSettings(
    LinuxConfig & config,
    pj64::input::InputConfig & input,
    const std::string & frontendConfigPath,
    const std::string & inputConfigPath);

class RuntimeWindow
{
public:
    RuntimeWindow(
        LinuxConfig & config,
        pj64::input::InputConfig & input,
        const std::string & frontendConfigPath,
        const std::string & inputConfigPath);
    ~RuntimeWindow();

    RuntimeWindow(const RuntimeWindow &) = delete;
    RuntimeWindow & operator=(const RuntimeWindow &) = delete;

    LauncherResult SelectRom(volatile std::sig_atomic_t * stopSignal);
    bool AttachRenderWindow(SDL_Window * renderWindow, const std::string & title, int width, int height);
    bool IsEmbedded() const;
    bool IsOpen() const;
    void ProcessEvents();
    RuntimeCommand TakeCommand();
    void ToggleFullscreen();
    void SetPaused(bool paused);
    void SetSpeedLimited(bool limited);
    void SetStatus(const std::string & text);

private:
    struct Implementation;
    std::unique_ptr<Implementation> m_Implementation;
};
