#include <Project64-audio/Audio_1.1.h>

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static AUDIO_INFO g_AudioInfo = {};
static SDL_AudioDeviceID g_Device = 0;
static uint32_t g_Frequency = 33600;
static int g_Volume = 100;
static std::string g_DeviceName;

static void LoadConfiguration()
{
    const char * volume = std::getenv("PROJECT64_EM_AUDIO_VOLUME");
    g_Volume = volume == nullptr ? 100 : std::clamp(std::atoi(volume), 0, 100);
    const char * device = std::getenv("PROJECT64_EM_AUDIO_DEVICE");
    g_DeviceName = device == nullptr ? "" : device;
}

static void CloseDevice()
{
    if (g_Device != 0)
    {
        SDL_ClearQueuedAudio(g_Device);
        SDL_CloseAudioDevice(g_Device);
        g_Device = 0;
    }
}

static void OpenDevice()
{
    CloseDevice();
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        return;
    }

    SDL_AudioSpec desired = {};
    desired.freq = static_cast<int>(g_Frequency);
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    const char * device = g_DeviceName.empty() ? nullptr : g_DeviceName.c_str();
    g_Device = SDL_OpenAudioDevice(device, 0, &desired, nullptr, 0);
    if (g_Device != 0)
    {
        SDL_PauseAudioDevice(g_Device, 0);
    }
}

EXPORT void CALL GetDllInfo(PLUGIN_INFO * info)
{
    info->Version = 0x0101;
    info->Type = PLUGIN_TYPE_AUDIO;
    SDL_strlcpy(info->Name, "Project64-EM SDL audio", sizeof(info->Name));
    info->NormalMemory = false;
    info->MemoryBswaped = true;
}

EXPORT int32_t CALL InitiateAudio(AUDIO_INFO info)
{
    g_AudioInfo = info;
    return true;
}

EXPORT void CALL AiDacrateChanged(int32_t systemType)
{
    uint32_t videoClock = 48681812;
    if (systemType == SYSTEM_PAL) { videoClock = 49656530; }
    else if (systemType == SYSTEM_MPAL) { videoClock = 48628316; }

    uint32_t dacrate = *g_AudioInfo.AI_DACRATE_REG & 0x3fff;
    uint32_t frequency = videoClock / (dacrate + 1);
    if (frequency >= 8000 && frequency <= 192000 && frequency != g_Frequency)
    {
        g_Frequency = frequency;
        OpenDevice();
    }
}

EXPORT void CALL AiLenChanged()
{
    uint32_t length = *g_AudioInfo.AI_LEN_REG & 0x3fff8;
    uint32_t address = *g_AudioInfo.AI_DRAM_ADDR_REG & 0x00fffff8;
    *g_AudioInfo.AI_STATUS_REG = AI_STATUS_DMA_BUSY;

    if (length > 0 && g_Device != 0)
    {
        uint32_t queueLimit = std::max(g_Frequency, 8000u);
        while (SDL_GetQueuedAudioSize(g_Device) > queueLimit)
        {
            SDL_Delay(1);
        }

        const uint8_t * source = g_AudioInfo.RDRAM + address;
        std::vector<uint8_t> samples(length);
        for (uint32_t offset = 0; offset + 3 < length; offset += 4)
        {
            std::memcpy(samples.data() + offset, source + offset + 2, 2);
            std::memcpy(samples.data() + offset + 2, source + offset, 2);
        }
        if (g_Volume < 100)
        {
            for (uint32_t offset = 0; offset + sizeof(int16_t) <= length; offset += sizeof(int16_t))
            {
                int16_t sample;
                std::memcpy(&sample, samples.data() + offset, sizeof(sample));
                sample = static_cast<int16_t>(static_cast<int32_t>(sample) * g_Volume / 100);
                std::memcpy(samples.data() + offset, &sample, sizeof(sample));
            }
        }
        SDL_QueueAudio(g_Device, samples.data(), length);
    }

    *g_AudioInfo.AI_LEN_REG = 0;
    *g_AudioInfo.AI_STATUS_REG = 0;
    *g_AudioInfo.MI_INTR_REG |= MI_INTR_AI;
    g_AudioInfo.CheckInterrupts();
}

EXPORT uint32_t CALL AiReadLength()
{
    return 0;
}

EXPORT void CALL AiUpdate(int32_t wait)
{
    if (wait != 0)
    {
        SDL_Delay(1);
    }
}

EXPORT void CALL RomOpen()
{
    LoadConfiguration();
    OpenDevice();
}

EXPORT void CALL RomClosed()
{
    CloseDevice();
}

EXPORT void CALL CloseDLL()
{
    CloseDevice();
}

EXPORT void CALL ProcessAList() {}
EXPORT void CALL DllAbout(void *) {}
EXPORT void CALL DllConfig(void *) {}
EXPORT void CALL DllTest(void *) {}
