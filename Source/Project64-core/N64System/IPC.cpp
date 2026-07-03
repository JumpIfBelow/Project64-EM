#include "stdafx.h"
#include <windows.h>
#include <cstdio>
#include "IPC.h"
#include <Project64-core/N64System/Mips/MemoryVirtualMem.h>

#define IPC_MAGIC_IN    0xae67e45b
#define IPC_MAGIC_OUT   0x64738358

#define IPC_STATUS_CONNECTED    0x01
#define IPC_STATUS_READ_READY   0x02
#define IPC_STATUS_ERROR        0x04

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

CIPC::CIPC(CMipsMemoryVM& memory)
: m_Memory(memory)
, m_Enabled(false)
, m_Error(false)
, m_PipeServer(INVALID_HANDLE_VALUE)
, m_RamAddr(0)
, m_LastMessageSize(0)
{
}

CIPC::~CIPC()
{
    Reset();
}

void CIPC::Reset()
{
    if (m_PipeServer != INVALID_HANDLE_VALUE)
    {
        DisconnectNamedPipe(m_PipeServer);
        CloseHandle(m_PipeServer);
        m_PipeServer = INVALID_HANDLE_VALUE;
    }
    m_Enabled = false;
    m_Error = false;
    m_RamAddr = 0;
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
    case 0x04:
        return ReadStatus();
    case 0x08:
        return m_RamAddr;
    case 0x10:
        return m_LastMessageSize;
    default:
        return 0;
    }
}

void CIPC::Write(uint32_t PAddr, uint32_t Value)
{
    uint32_t off;
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "IPC Write: PAddr=0x%08X, Value=0x%08X", PAddr, Value);
    MessageBoxA(nullptr, buffer, "IPC Write", MB_OK);

    off = PAddr & 0xff;
    if (off == 0x00)
    {
        if (Value == IPC_MAGIC_IN)
            Enable();
        else
            Reset();
        return;
    }

    if (!m_Enabled)
        return;

    switch (off)
    {
    case 0x04:
        m_Error = false;
        break;
    case 0x08:
        m_RamAddr = (Value & 0x1fffffff);
        break;
    case 0x0c:
        PerformWrite(Value);
        break;
    case 0x10:
        PerformRead(Value);
        break;
    }
}

void CIPC::Enable()
{
    char buffer[256];

    if (m_Enabled)
        return;

    snprintf(buffer, sizeof(buffer), "\\\\.\\pipe\\pj64em-ipc.%d.%u", GetCurrentProcessId(), m_PipeCounter++);
    m_PipeServer = CreateNamedPipeA(
        buffer,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1,
        0x10000,
        0x10000,
        0,
        nullptr);
    ConnectNamedPipe(m_PipeServer, nullptr);
    m_Enabled = true;
}

uint32_t CIPC::ReadStatus()
{
    DWORD err;
    DWORD bytesAvail;
    uint32_t status{};

    if (PeekNamedPipe(m_PipeServer, nullptr, 0, nullptr, &bytesAvail, nullptr))
    {
        status |= IPC_STATUS_CONNECTED;
        if (bytesAvail > 0)
            status |= IPC_STATUS_READ_READY;
    }
    else
    {
        err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
        {
            DisconnectNamedPipe(m_PipeServer);
            ConnectNamedPipe(m_PipeServer, nullptr);
        }
    }

    if (m_Error)
        status |= IPC_STATUS_ERROR;
    m_Error = false;
    return status;
}

void CIPC::PerformRead(uint32_t size)
{
    DWORD err;
    DWORD bytesRead;
    char buffer[512];
    uint8_t* rdram;

    /* Bounds check */
    if (size == 0)
        return;
    if (size > 512)
    {
        m_Error = true;
        return;
    }

    /* RAM check */
    if (m_RamAddr + size > 0x800000)
    {
        m_Error = true;
        return;
    }

    /* Try to read a message from the named pipe */
    if (!ReadFile(m_PipeServer, buffer, size, &bytesRead, nullptr))
    {
        err = GetLastError();
        if (err == ERROR_NO_DATA)
        {
            /* Nonblocking pipe with nothing queued - not an error */
            m_LastMessageSize = 0;
            return;
        }
        m_Error = true;
        if (err == ERROR_BROKEN_PIPE)
        {
            DisconnectNamedPipe(m_PipeServer);
            ConnectNamedPipe(m_PipeServer, nullptr);
        }
        return;
    }

    /* Copy from the temp buffer into the RAM */
    rdram = m_Memory.Rdram();
    for (uint32_t i = 0; i < bytesRead; ++i)
        rdram[(m_RamAddr + i) ^ 3] = buffer[i];

    m_LastMessageSize = bytesRead;
}

void CIPC::PerformWrite(uint32_t size)
{
    DWORD err;
    char buffer[512];
    uint8_t* rdram;

    /* Bounds check */
    if (size == 0)
        return;
    if (size > 512)
    {
        m_Error = true;
        return;
    }

    /* RAM check */
    if (m_RamAddr + size > 0x800000)
    {
        m_Error = true;
        return;
    }

    /* Copy from the RAM into the temp buffer */
    rdram = m_Memory.Rdram();
    for (uint32_t i = 0; i < size; ++i)
        buffer[i] = rdram[(m_RamAddr + i) ^ 3];

    /* Try to send the message to the named pipe */
    if (!WriteFile(m_PipeServer, buffer, size, nullptr, nullptr))
    {
        m_Error = true;
        err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
        {
            DisconnectNamedPipe(m_PipeServer);
            ConnectNamedPipe(m_PipeServer, nullptr);
        }
        return;
    }
}
