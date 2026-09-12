# Project64-EM

Project64-EM is a fork of Project64 used by the OoTMM multiplayer client.

## Linux

The Linux target provides a Qt 6 application shell around the SDL2/OpenGL
emulator while retaining the existing Windows frontend. A central game panel
starts ROMs, and the persistent top menu remains available while a ROM is
running. It supports video, audio, keyboard, gamepad and directory settings as
well as the OoTMM multiplayer IPC transport. The Linux frontend currently uses
Project64's interpreter CPU core; the Windows dynamic recompiler is not portable
yet.

Ubuntu build dependencies:

```sh
sudo apt install build-essential cmake ninja-build qt6-base-dev libsdl2-dev libgl1-mesa-dev libssl-dev
```

Configure and build from the repository root:

```sh
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux
```

Qt is required only by the Linux target. No Linux frontend dependency is added
to the Windows target.

Open the launcher:

```sh
./build-linux/bin/project64-em
```

Choose a locally generated ROM in the central panel, adjust settings if needed,
and select `Start Game`. The application includes:

- window, fullscreen and speed-limit settings;
- aspect ratio, texture filtering, VSync and texture-pack settings;
- audio output, enable/disable and volume settings;
- editable keyboard bindings and SDL game-controller mappings;
- optional save, state, screenshot and texture directories.

To bypass the launcher, pass a ROM directly:

```sh
./build-linux/bin/project64-em /path/to/game.z64
```

`--configure ROM` opens the launcher with a particular ROM selected.
`--fullscreen` or `--windowed` overrides the saved mode for a direct launch.
`--input-config FILE` selects an alternate controller configuration.

The File, System, Options and Help menus remain accessible during emulation.
They provide settings, save states, reset, pause, screenshots, speed limiting
and fullscreen controls. F2 pauses or resumes, F5 saves state, F7 loads state,
F8 performs a soft reset, F9 toggles the speed limit, F11 toggles fullscreen,
F12 takes a screenshot, and Escape exits.

Frontend and input settings are stored in
`$XDG_CONFIG_HOME/project64-em`, or `~/.config/project64-em` when
`XDG_CONFIG_HOME` is unset. The supplied `Config/LinuxInput.ini` is copied as
the initial input profile and can also be edited for portable defaults.

Start the emulator before the OoTMM multiplayer client. Release `r4` of the
client detects the emulator through a Unix socket in
`$XDG_RUNTIME_DIR/n64-ipc`. Client release `r5` is not compatible with this
Project64-EM release.

The launcher covers the settings needed for normal OoTMM play. Windows-only
Project64 tools such as its debugger, cheat editor and plugin configuration
dialogs are not part of the Linux frontend.

## Windows

The existing Win32 target remains available through CMake:

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
```
