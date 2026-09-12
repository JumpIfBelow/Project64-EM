#pragma once

#include <string>
#include <vector>

struct LinuxConfig
{
    int windowWidth = 960;
    int windowHeight = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool audioEnabled = true;
    int audioVolume = 100;
    std::string audioDevice;
    bool limitFps = true;
    int aspectRatio = 0;
    int filtering = 0;
    int textureFilter = 0;
    bool highResolutionTextures = false;
    bool anisotropicFiltering = false;
    std::string saveDirectory;
    std::string stateDirectory;
    std::string screenshotDirectory;
    std::string textureDirectory;
    std::string lastRom;
    std::vector<std::string> recentRoms;

    bool Load(const std::string & path);
    bool Save(const std::string & path) const;
    void AddRecentRom(const std::string & path);
    void ApplyProjectSettings(const std::string & baseDirectory) const;
    void ApplyEnvironment(const std::string & inputConfigPath) const;
};
