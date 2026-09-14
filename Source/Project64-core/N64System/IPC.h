#pragma once

#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#else
#include <string>
#include <vector>
#endif

class CMipsMemoryVM;
class CIPC
{
public:
    CIPC(CMipsMemoryVM& memory);
    ~CIPC();

    void Reset();
    bool Enabled() const;

    uint32_t Read(uint32_t PAddr);
    void     Write(uint32_t PAddr, uint32_t Value);

private:
    void        Enable();
    uint32_t    ReadStatus();
    void        PerformRead(uint32_t size);
    void        PerformWrite(uint32_t size);
#ifndef _WIN32
    void        AcceptClient();
    void        CloseClient();
    void        PumpInput();
    void        PumpOutput();
#endif

    CMipsMemoryVM& m_Memory;

    bool        m_Enabled;
    bool        m_Error;
#ifdef _WIN32
    HANDLE      m_PipeServer;
#else
    int         m_ServerFd;
    int         m_ClientFd;
    std::string m_SocketPath;
    std::vector<uint8_t> m_InputBuffer;
    std::vector<uint8_t> m_OutputBuffer;
    std::vector<uint8_t> m_PendingMessage;
#endif
    uint32_t    m_PipeCounter;
    uint32_t    m_RamAddr;
    uint32_t    m_LastMessageSize;
};
