#pragma once

#include <windows.h>
#include <cstdint>

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

    CMipsMemoryVM& m_Memory;

    bool        m_Enabled;
    bool        m_Error;
    HANDLE      m_PipeServer;
    DWORD       m_PipeCounter;
    uint32_t    m_RamAddr;
    uint32_t    m_LastMessageSize;
};
