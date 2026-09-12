#pragma once

#include <SDL.h>

#include <csignal>
#include <string>

struct LinuxConfig;

namespace pj64::input
{
struct InputConfig;
}

enum class LauncherResult
{
    Launch,
    Quit,
};

LauncherResult RunLauncher(
    SDL_Window * window,
    SDL_GLContext context,
    LinuxConfig & config,
    pj64::input::InputConfig & input,
    const std::string & frontendConfigPath,
    const std::string & inputConfigPath,
    volatile std::sig_atomic_t * stopSignal);
