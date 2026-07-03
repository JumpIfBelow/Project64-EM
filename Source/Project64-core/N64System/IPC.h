#pragma once

#include <cstdint>

class CIPC
{
public:
    CIPC();
    ~CIPC();

    void Reset();
    bool Enabled() const;

    uint32_t Read(uint32_t PAddr);
    void     Write(uint32_t PAddr, uint32_t Value);

private:
    bool m_Enabled;
};
