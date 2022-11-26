/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Comms Channel
// Channels for messages from interfaces (BLE, WiFi, etc)
//
// Rob Dobson 2020-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>

class CommsChannelSettings
{
public:
    static const uint32_t INBOUND_BLOCK_LEN_DEFAULT = 1200;
    static const uint32_t INBOUND_BLOCK_MAX_DEFAULT = 5000;
    static const uint32_t INBOUND_QUEUE_LEN_DEFAULT = 20;
    static const uint32_t INBOUND_QUEUE_BYTES_MAX = 20000;
    static const uint32_t OUTBOUND_BLOCK_MAX_DEFAULT = 5000;
    static const uint32_t OUTBOUND_QUEUE_LEN_DEFAULT = 20;

    CommsChannelSettings(uint32_t inboundBlockLen = 0,
                         uint32_t inboundBlockLenMax = 0,
                         uint32_t inboundQueueCountMax = 0,
                         uint32_t inboundQueueBytesMax = 0,
                         uint32_t outboundBlockLen = 0,
                         uint32_t outboundQueueMaxLen = 0)
    {
        this->inboundBlockLen = inboundBlockLen != 0 ? inboundBlockLen : INBOUND_BLOCK_LEN_DEFAULT;
        this->inboundBlockLenMax = inboundBlockLenMax != 0 ? inboundBlockLenMax : INBOUND_BLOCK_MAX_DEFAULT;
        this->inboundQueueCountMax = inboundQueueCountMax != 0 ? inboundQueueCountMax : INBOUND_QUEUE_LEN_DEFAULT;
        this->inboundQueueBytesMax = inboundQueueBytesMax != 0 ? inboundQueueBytesMax : INBOUND_QUEUE_BYTES_MAX;
        this->outboundBlockLen = outboundBlockLen != 0 ? outboundBlockLen : OUTBOUND_BLOCK_MAX_DEFAULT;
        this->outboundQueueMaxLen = outboundQueueMaxLen != 0 ? outboundQueueMaxLen : OUTBOUND_QUEUE_LEN_DEFAULT;
    }

    uint32_t inboundBlockLen = INBOUND_BLOCK_LEN_DEFAULT;
    uint32_t inboundBlockLenMax = INBOUND_BLOCK_MAX_DEFAULT;
    uint32_t inboundQueueCountMax = INBOUND_QUEUE_LEN_DEFAULT;
    uint32_t inboundQueueBytesMax = 20000;
    uint32_t outboundBlockLen = OUTBOUND_BLOCK_MAX_DEFAULT;
    uint32_t outboundQueueMaxLen = OUTBOUND_QUEUE_LEN_DEFAULT;
};
