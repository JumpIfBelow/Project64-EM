#include "LinuxConfig.h"

#include <Common/IniFile.h>
#include <Project64-core/Settings.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>

namespace
{
int ClampNumber(CIniFile & ini, const char * section, const char * key, int defaultValue, int minimum, int maximum)
{
    return std::clamp(static_cast<int>(ini.GetNumber(section, key, defaultValue)), minimum, maximum);
}

bool LoadBool(CIniFile & ini, const char * section, const char * key, bool defaultValue)
{
    return ini.GetNumber(section, key, defaultValue ? 1 : 0) != 0;
}

void ApplyDirectory(SettingID pathSetting, SettingID useSetting, const std::string & path)
{
    if (g_Settings == nullptr)
    {
        return;
    }
    g_Settings->SaveBool(useSetting, !path.empty());
    if (!path.empty())
    {
        std::filesystem::create_directories(path);
        g_Settings->SaveString(pathSetting, path);
    }
}

void CreateRuntimeDirectories()
{
    static constexpr std::array<SettingID, 4> directories = {{
        Directory_NativeSave,
        Directory_InstantSave,
        Directory_SnapShot,
        Directory_Texture,
    }};
    for (SettingID directory : directories)
    {
        std::error_code error;
        std::filesystem::create_directories(g_Settings->LoadStringVal(directory), error);
    }
}
}

bool LinuxConfig::Load(const std::string & path)
{
    if (!std::filesystem::is_regular_file(path))
    {
        return false;
    }

    CIniFile ini(path.c_str(), false, true);
    windowWidth = ClampNumber(ini, "Window", "Width", windowWidth, 320, 7680);
    windowHeight = ClampNumber(ini, "Window", "Height", windowHeight, 240, 4320);
    fullscreen = LoadBool(ini, "Window", "Fullscreen", fullscreen);
    vsync = LoadBool(ini, "Video", "VSync", vsync);
    aspectRatio = ClampNumber(ini, "Video", "AspectRatio", aspectRatio, 0, 3);
    filtering = ClampNumber(ini, "Video", "Filtering", filtering, 0, 2);
    textureFilter = ClampNumber(ini, "Video", "TextureFilter", textureFilter, 0, 0x20);
    highResolutionTextures = LoadBool(ini, "Video", "HighResolutionTextures", highResolutionTextures);
    anisotropicFiltering = LoadBool(ini, "Video", "AnisotropicFiltering", anisotropicFiltering);
    audioEnabled = LoadBool(ini, "Audio", "Enabled", audioEnabled);
    audioVolume = ClampNumber(ini, "Audio", "Volume", audioVolume, 0, 100);
    audioDevice = ini.GetString("Audio", "Device", "");
    limitFps = LoadBool(ini, "Emulation", "LimitFPS", limitFps);
    saveDirectory = ini.GetString("Directories", "Save", "");
    stateDirectory = ini.GetString("Directories", "State", "");
    screenshotDirectory = ini.GetString("Directories", "Screenshot", "");
    textureDirectory = ini.GetString("Directories", "Texture", "");
    lastRom = ini.GetString("Library", "LastROM", "");

    recentRoms.clear();
    for (int index = 0; index < 10; index++)
    {
        const std::string key = "ROM" + std::to_string(index);
        std::string recent = ini.GetString("Recent", key.c_str(), "");
        if (!recent.empty() && std::find(recentRoms.begin(), recentRoms.end(), recent) == recentRoms.end())
        {
            recentRoms.push_back(std::move(recent));
        }
    }
    return true;
}

bool LinuxConfig::Save(const std::string & path) const
{
    std::error_code error;
    const std::filesystem::path configPath(path);
    std::filesystem::create_directories(configPath.parent_path(), error);
    CIniFile ini(path.c_str());
    if (!ini.IsFileOpen())
    {
        return false;
    }

    ini.SaveNumber("Window", "Width", windowWidth);
    ini.SaveNumber("Window", "Height", windowHeight);
    ini.SaveNumber("Window", "Fullscreen", fullscreen ? 1 : 0);
    ini.SaveNumber("Video", "VSync", vsync ? 1 : 0);
    ini.SaveNumber("Video", "AspectRatio", aspectRatio);
    ini.SaveNumber("Video", "Filtering", filtering);
    ini.SaveNumber("Video", "TextureFilter", textureFilter);
    ini.SaveNumber("Video", "HighResolutionTextures", highResolutionTextures ? 1 : 0);
    ini.SaveNumber("Video", "AnisotropicFiltering", anisotropicFiltering ? 1 : 0);
    ini.SaveNumber("Audio", "Enabled", audioEnabled ? 1 : 0);
    ini.SaveNumber("Audio", "Volume", audioVolume);
    ini.SaveString("Audio", "Device", audioDevice.c_str());
    ini.SaveNumber("Emulation", "LimitFPS", limitFps ? 1 : 0);
    ini.SaveString("Directories", "Save", saveDirectory.c_str());
    ini.SaveString("Directories", "State", stateDirectory.c_str());
    ini.SaveString("Directories", "Screenshot", screenshotDirectory.c_str());
    ini.SaveString("Directories", "Texture", textureDirectory.c_str());
    ini.SaveString("Library", "LastROM", lastRom.c_str());
    for (int index = 0; index < 10; index++)
    {
        const std::string key = "ROM" + std::to_string(index);
        ini.SaveString("Recent", key.c_str(), index < static_cast<int>(recentRoms.size()) ? recentRoms[index].c_str() : nullptr);
    }
    ini.FlushChanges();
    return true;
}

void LinuxConfig::AddRecentRom(const std::string & path)
{
    lastRom = path;
    recentRoms.erase(std::remove(recentRoms.begin(), recentRoms.end(), path), recentRoms.end());
    recentRoms.insert(recentRoms.begin(), path);
    if (recentRoms.size() > 10)
    {
        recentRoms.resize(10);
    }
}

void LinuxConfig::ApplyProjectSettings(const std::string & baseDirectory) const
{
    if (g_Settings == nullptr)
    {
        const std::filesystem::path projectConfig = std::filesystem::path(baseDirectory) / "Config" / "Project64.cfg";
        std::error_code error;
        std::filesystem::create_directories(projectConfig.parent_path(), error);
        CIniFile ini(projectConfig.string().c_str());
        ini.SaveNumber("Video", "vsync", vsync ? 1 : 0);
        ini.SaveNumber("Video", "aspect", aspectRatio);
        ini.SaveNumber("Video", "filtering", filtering);
        ini.SaveNumber("Video", "ghq_fltr", textureFilter);
        ini.SaveNumber("Video", "ghq_hirs", highResolutionTextures ? 0x00020000 : 0);
        ini.SaveNumber("Video", "texenh_options", textureFilter != 0 || highResolutionTextures ? 1 : 0);
        ini.SaveNumber("Video", "wrpAnisotropic", anisotropicFiltering ? 1 : 0);
        ini.FlushChanges();
        return;
    }
    g_Settings->SaveBool(Setting_ForceInterpreterCPU, true);
    g_Settings->SaveBool(Plugin_EnableAudio, audioEnabled);
    g_Settings->SaveBool(GameRunning_LimitFPS, limitFps);
    ApplyDirectory(Directory_NativeSaveSelected, Directory_NativeSaveUseSelected, saveDirectory);
    ApplyDirectory(Directory_InstantSaveSelected, Directory_InstantSaveUseSelected, stateDirectory);
    ApplyDirectory(Directory_SnapShotSelected, Directory_SnapShotUseSelected, screenshotDirectory);
    ApplyDirectory(Directory_TextureSelected, Directory_TextureUseSelected, textureDirectory);
    CreateRuntimeDirectories();
}

void LinuxConfig::ApplyEnvironment(const std::string & inputConfigPath) const
{
    setenv("PROJECT64_EM_INPUT_CONFIG", inputConfigPath.c_str(), 1);
    const std::string volume = std::to_string(audioVolume);
    setenv("PROJECT64_EM_AUDIO_VOLUME", volume.c_str(), 1);
    if (audioDevice.empty())
    {
        unsetenv("PROJECT64_EM_AUDIO_DEVICE");
    }
    else
    {
        setenv("PROJECT64_EM_AUDIO_DEVICE", audioDevice.c_str(), 1);
    }
}
