#include <windows.h>
#include <cstdio>
#include "IPC.h"

/**
 * Custom IPC subsystem.
 *
 * IPC_BASE = 0x1FE00000
 * +0x00 | IPC_KEY
 * +0x04 | IPC_STATUS
 * +0x08 | IPC_RAM_ADDR
 * +0x0C | IPC_WRITE_LEN
 * +0x10 | IPC_READ_LEN
 */

CIPC::CIPC()
: m_Enabled(false)
{
}

CIPC::~CIPC()
{
}

void CIPC::Reset()
{
    m_Enabled = false;
}

bool CIPC::Enabled() const
{
    return m_Enabled;
}

uint32_t CIPC::Read(uint32_t PAddr)
{
    return 0;
}

void CIPC::Write(uint32_t PAddr, uint32_t Value)
{
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "IPC Write: PAddr=0x%08X, Value=0x%08X", PAddr, Value);
    MessageBoxA(nullptr, buffer, "IPC Write", MB_OK);
}
