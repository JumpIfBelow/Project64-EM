#include <windows.h>
#include <cstdio>
#include "IPC.h"

#define IPC_MAGIC_IN    0xae67e45b
#define IPC_MAGIC_OUT   0x64738358

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
    switch (PAddr & 0xff)
    {
    case 0x00:
        return IPC_MAGIC_OUT;
    default:
        return 0;
    }
}

void CIPC::Write(uint32_t PAddr, uint32_t Value)
{
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "IPC Write: PAddr=0x%08X, Value=0x%08X", PAddr, Value);
    MessageBoxA(nullptr, buffer, "IPC Write", MB_OK);

    switch (PAddr & 0xff)
    {
    case 0x00:
        if (Value == IPC_MAGIC_IN)
            m_Enabled = true;
        else
            Reset();
        break;
    }
}
