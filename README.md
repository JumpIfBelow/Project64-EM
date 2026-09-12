# Project64-EM

Project64-EM is a fork of Project64 used by the OoTMM multiplayer client.

## Linux

The Linux target provides a playable SDL2 frontend while retaining the existing
Windows frontend. It currently uses the interpreter core and supports an SDL2
OpenGL window, fullscreen mode, audio, keyboard/gamepad input, and the OoTMM
multiplayer IPC transport.

Ubuntu build dependencies:

```sh
sudo apt install build-essential cmake ninja-build libsdl2-dev libgl1-mesa-dev libssl-dev
```

Configure and build from the repository root:

```sh
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux
```

Run a locally generated OoTMM ROM:

```sh
./build-linux/bin/project64-em /path/to/game.z64
```

Use `--fullscreen` to start in fullscreen mode. F11 toggles fullscreen and
Escape exits. `--input-config FILE` selects a controller configuration; the
default editable file is `build-linux/bin/Config/LinuxInput.ini`.

Start the emulator before the OoTMM multiplayer client. Release `r4` of the
client detects the emulator through a Unix socket in
`$XDG_RUNTIME_DIR/n64-ipc`. Client release `r5` is not compatible with this
Project64-EM release.

## Windows

The existing Win32 target remains available through CMake:

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
```
