#pragma once

#include <csignal>
#include <cstdint>
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
    HardReset,
    ToggleSpeedLimit,
    ToggleFullscreen,
    Screenshot,
    Settings,
    VideoSettings,
    AudioSettings,
    InputSettings,
    Quit,
};

enum class SettingsPage
{
    General,
    Video,
    Audio,
    Input,
    Directories,
};

bool ShowSettings(
    LinuxConfig & config,
    pj64::input::InputConfig & input,
    const std::string & frontendConfigPath,
    const std::string & inputConfigPath,
    SettingsPage page = SettingsPage::General);

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
    void GetDrawableSize(uint32_t & width, uint32_t & height) const;
    RuntimeCommand TakeCommand();
    void ToggleFullscreen();
    void SetPaused(bool paused);
    void SetSpeedLimited(bool limited);
    void SetStatus(const std::string & text);

private:
    struct Implementation;
    std::unique_ptr<Implementation> m_Implementation;
};
