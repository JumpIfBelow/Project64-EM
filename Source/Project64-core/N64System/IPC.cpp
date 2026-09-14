#include "stdafx.h"
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif
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
#ifdef _WIN32
, m_PipeServer(INVALID_HANDLE_VALUE)
#else
, m_ServerFd(-1)
, m_ClientFd(-1)
#endif
, m_PipeCounter(0)
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
#ifdef _WIN32
    if (m_PipeServer != INVALID_HANDLE_VALUE)
    {
        DisconnectNamedPipe(m_PipeServer);
        CloseHandle(m_PipeServer);
        m_PipeServer = INVALID_HANDLE_VALUE;
    }
#else
    CloseClient();
    if (m_ServerFd >= 0)
    {
        close(m_ServerFd);
        m_ServerFd = -1;
    }
    if (!m_SocketPath.empty())
    {
        unlink(m_SocketPath.c_str());
        m_SocketPath.clear();
    }
    m_InputBuffer.clear();
    m_OutputBuffer.clear();
    m_PendingMessage.clear();
#endif
    m_Enabled = false;
    m_Error = false;
    m_RamAddr = 0;
    m_LastMessageSize = 0;
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

#ifdef _WIN32
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
#else
    const char* runtimeDir = getenv("XDG_RUNTIME_DIR");
    if (runtimeDir == nullptr || runtimeDir[0] == '\0')
    {
        m_Error = true;
        return;
    }

    snprintf(buffer, sizeof(buffer), "%s/n64-ipc", runtimeDir);
    if (mkdir(buffer, 0700) != 0 && errno != EEXIST)
    {
        m_Error = true;
        return;
    }
    chmod(buffer, 0700);

    char socketPath[sizeof(sockaddr_un::sun_path)];
    const int pathLength = snprintf(socketPath, sizeof(socketPath), "%s/pj64em-%d-%u.sock", buffer, static_cast<int>(getpid()), m_PipeCounter++);
    if (pathLength < 0 || static_cast<size_t>(pathLength) >= sizeof(socketPath))
    {
        m_Error = true;
        return;
    }

    m_ServerFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (m_ServerFd < 0)
    {
        m_Error = true;
        return;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, socketPath, sizeof(address.sun_path) - 1);
    unlink(socketPath);
    if (bind(m_ServerFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        chmod(socketPath, 0600) != 0 || listen(m_ServerFd, 1) != 0)
    {
        close(m_ServerFd);
        m_ServerFd = -1;
        unlink(socketPath);
        m_Error = true;
        return;
    }
    m_SocketPath = socketPath;
    m_Enabled = true;
#endif
}

uint32_t CIPC::ReadStatus()
{
#ifdef _WIN32
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
#else
    uint32_t status{};
    AcceptClient();
    PumpInput();
    PumpOutput();
    if (m_ClientFd >= 0)
    {
        status |= IPC_STATUS_CONNECTED;
    }
    if (!m_PendingMessage.empty())
    {
        status |= IPC_STATUS_READ_READY;
    }
    if (m_Error)
    {
        status |= IPC_STATUS_ERROR;
    }
    m_Error = false;
    return status;
#endif
}

void CIPC::PerformRead(uint32_t size)
{
#ifdef _WIN32
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
        m_LastMessageSize = 0;
        err = GetLastError();
        if (err == ERROR_NO_DATA)
        {
            /* Nonblocking pipe with nothing queued - not an error */
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
#else
    m_LastMessageSize = 0;
    if (size == 0)
    {
        return;
    }
    if (size > 512 || m_RamAddr + size > 0x800000)
    {
        m_Error = true;
        return;
    }
    PumpInput();
    if (m_PendingMessage.empty())
    {
        return;
    }
    if (m_PendingMessage.size() > size)
    {
        m_Error = true;
        return;
    }

    uint8_t* rdram = m_Memory.Rdram();
    for (size_t i = 0; i < m_PendingMessage.size(); ++i)
    {
        rdram[(m_RamAddr + i) ^ 3] = m_PendingMessage[i];
    }
    m_LastMessageSize = static_cast<uint32_t>(m_PendingMessage.size());
    m_PendingMessage.clear();
    PumpInput();
#endif
}

void CIPC::PerformWrite(uint32_t size)
{
#ifdef _WIN32
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
#else
    if (size == 0)
    {
        return;
    }
    if (size > 512 || m_RamAddr + size > 0x800000 || m_ClientFd < 0)
    {
        m_Error = true;
        return;
    }

    if (m_OutputBuffer.size() + size + 4 > 0x10000)
    {
        m_Error = true;
        return;
    }

    const size_t frameStart = m_OutputBuffer.size();
    m_OutputBuffer.resize(frameStart + size + 4);
    uint8_t * frame = m_OutputBuffer.data() + frameStart;
    frame[0] = static_cast<uint8_t>(size);
    frame[1] = static_cast<uint8_t>(size >> 8);
    frame[2] = static_cast<uint8_t>(size >> 16);
    frame[3] = static_cast<uint8_t>(size >> 24);
    uint8_t* rdram = m_Memory.Rdram();
    for (uint32_t i = 0; i < size; ++i)
    {
        frame[4 + i] = rdram[(m_RamAddr + i) ^ 3];
    }

    PumpOutput();
#endif
}

#ifndef _WIN32
void CIPC::AcceptClient()
{
    if (m_ServerFd < 0 || m_ClientFd >= 0)
    {
        return;
    }
    m_ClientFd = accept4(m_ServerFd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (m_ClientFd < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
    {
        m_Error = true;
    }
}

void CIPC::CloseClient()
{
    if (m_ClientFd >= 0)
    {
        close(m_ClientFd);
        m_ClientFd = -1;
    }
    m_InputBuffer.clear();
    m_OutputBuffer.clear();
    m_PendingMessage.clear();
}

void CIPC::PumpInput()
{
    if (m_ClientFd < 0 || !m_PendingMessage.empty())
    {
        return;
    }

    uint8_t buffer[516];
    for (;;)
    {
        const ssize_t result = recv(m_ClientFd, buffer, sizeof(buffer), 0);
        if (result > 0)
        {
            if (m_InputBuffer.size() + static_cast<size_t>(result) > 0x10000)
            {
                m_Error = true;
                CloseClient();
                return;
            }
            m_InputBuffer.insert(m_InputBuffer.end(), buffer, buffer + result);
            continue;
        }
        if (result == 0)
        {
            CloseClient();
            return;
        }
        if (errno == EINTR)
        {
            continue;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            m_Error = true;
            CloseClient();
        }
        break;
    }

    if (m_InputBuffer.size() < 4)
    {
        return;
    }
    const uint32_t messageSize =
        static_cast<uint32_t>(m_InputBuffer[0]) |
        (static_cast<uint32_t>(m_InputBuffer[1]) << 8) |
        (static_cast<uint32_t>(m_InputBuffer[2]) << 16) |
        (static_cast<uint32_t>(m_InputBuffer[3]) << 24);
    if (messageSize == 0 || messageSize > 512)
    {
        m_Error = true;
        CloseClient();
        return;
    }
    if (m_InputBuffer.size() < messageSize + 4)
    {
        return;
    }
    m_PendingMessage.assign(m_InputBuffer.begin() + 4, m_InputBuffer.begin() + 4 + messageSize);
    m_InputBuffer.erase(m_InputBuffer.begin(), m_InputBuffer.begin() + 4 + messageSize);
}

void CIPC::PumpOutput()
{
    while (m_ClientFd >= 0 && !m_OutputBuffer.empty())
    {
        const ssize_t result = send(m_ClientFd, m_OutputBuffer.data(), m_OutputBuffer.size(), MSG_NOSIGNAL);
        if (result > 0)
        {
            m_OutputBuffer.erase(m_OutputBuffer.begin(), m_OutputBuffer.begin() + result);
            continue;
        }
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return;
        }
        m_Error = true;
        CloseClient();
    }
}
#endif
