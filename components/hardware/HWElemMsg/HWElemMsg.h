/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HWElemMsg
//
// Rob Dobson 2016-20
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <vector>

class HWElemMsg
{
public:
    HWElemMsg()
    {
        _formatCode = 0;
        _barAccessForMsAfterSend = 0;
    }

    // The format code is hw-element specific
    uint32_t _formatCode;

    // Bar access to element for N ms after this message is sent
    uint32_t _barAccessForMsAfterSend;

    // Data to send
    std::vector<uint8_t> _data;
};
