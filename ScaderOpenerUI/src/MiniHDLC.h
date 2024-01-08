/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HDLC Bit and Bytewise
// This HDLC implementation doesn't completely conform to HDLC
// Currently STX and ETX are not sent
// There is no flow control
// Bit and Byte oriented HDLC is supported with appropriate bit/byte stuffing
// Very loosely based on https://github.com/mengguang/minihdlc
//
// Rob Dobson 2017-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdbool>
#include "SimpleBuffer.h"

// Callback handling
#include <functional>
// Put byte or bit callback function type
typedef std::function<void(uint8_t ch)> MiniHDLCPutChFnType;
// Received frame callback function type
typedef std::function<void(const uint8_t *framebuffer, unsigned framelength)> MiniHDLCFrameFnType;

class MiniHDLCStats
{
public:
    MiniHDLCStats()
    {
        clear();
    }
    void clear()
    {
        _rxFrameCount = 0;
        _frameCRCErrCount = 0;
        _frameTooLongCount = 0;
        _rxBufAllocFail = 0;
    }
    uint32_t _rxFrameCount;
    uint16_t _frameCRCErrCount;
    uint16_t _frameTooLongCount;
    uint16_t _rxBufAllocFail;
};

// MiniHDLC
class MiniHDLC
{
public:
    // Constructor for HDLC with bit/bytewise transmit
    // If bitwise HDLC then the first parameter will receive bits not bytes 
    MiniHDLC(uint32_t rxMsgMaxLen, MiniHDLCPutChFnType putChFn, MiniHDLCFrameFnType frameRxFn,
                uint8_t frameBoundaryOctet, uint8_t controlEscapeOctet,
				bool bigEndianCRC = true, bool bitwiseHDLC = false);

    // Constructor for HDLC with frame-wise transmit
    MiniHDLC(MiniHDLCFrameFnType frameTxFn, MiniHDLCFrameFnType frameRxFn,
            uint8_t frameBoundaryOctet = FRAME_BOUNDARY_OCTET_DEFAULT, 
            uint8_t controlEscapeOctet = CONTROL_ESCAPE_OCTET_DEFAULT,
            uint32_t txMsgMaxLen=1000, uint32_t rxMsgMaxLen=1000, 
            bool bigEndianCRC = true, bool bitwiseHDLC = false);

    // Destructor
    virtual ~MiniHDLC();
    
    // Called by external function that has byte-wise data to process
    void handleChar(uint8_t ch);
    void handleBuffer(const uint8_t* pBuf, unsigned numBytes);

    // Called by external function that has bit-wise data to process
    void handleBit(uint8_t bit);

    // Encode a frame into HDLC
    uint32_t encodeFrame(uint8_t* pEncoded, uint32_t maxEncodedLen, const uint8_t *pFrame, uint32_t frameLen);

    // Encode a frame into HDLC in sections
    uint32_t encodeFrameStart(uint8_t* pEncoded, uint32_t maxEncodedLen, uint16_t& fcs);
    uint32_t encodeFrameAddPayload(uint8_t* pEncoded, uint32_t maxEncodedLen, uint16_t& fcs, uint32_t curPos, const uint8_t *pFrame, uint32_t frameLen);
    uint32_t encodeFrameEnd(uint8_t* pEncoded, uint32_t maxEncodedLen, uint16_t& fcs, uint32_t curPos);

    // Get encoded length of frame
    uint32_t calcEncodedLen(const uint8_t *pFrame, uint32_t frameLen)
    {
        return calcEncodedPayloadLen(pFrame, frameLen) + HDLC_MAX_OVERHEAD_BYTES;
    }

    // Get max encoded length of payload
    uint32_t maxEncodedLen(uint32_t payloadLen)
    {
        // Worst case length is 2 * payloadLen (if every byte is escaped)
        // + 2 * BORDER + 2 * FCS
        return payloadLen * 2 + HDLC_MAX_OVERHEAD_BYTES;
    }

    // Send a frame
    void sendFrame(const uint8_t *pData, unsigned frameLen);

    // Set frame rx max length
    void setFrameRxMaxLen(uint32_t rxMaxLen);

    // Overhead bytes (assumes 1 start, 1 end and both FCS bytes are escaped)
    static constexpr uint32_t HDLC_MAX_OVERHEAD_BYTES = 6;

    // Get frame rx max len
    uint32_t getFrameRxMaxLen()
    {
        return _rxBufferMaxLen;
    }

    // Get frame tx buffer
    uint8_t* getFrameTxBuf()
    {
        return _txBuffer.data();
    }

    // Clear tx buffer
    void clearTxBuf()
    {
        _txBuffer.clear();
    }

    // Get frame tx len
    uint32_t getFrameTxLen()
    {
        if (_txBuffer.size() < _txBufferPos)
            return _txBuffer.size();
        return _txBufferPos;
    }

    // Get stats
    MiniHDLCStats* getStats()
    {
        return &_stats;
    }

    // Compute CCITT CRC16
    static unsigned computeCRC16(const uint8_t* pData, unsigned len);
    static uint16_t crcInitCCITT()
    {
        return CRC16_CCITT_INIT_VAL;
    }
    static uint16_t crcUpdateCCITT(unsigned short fcs, unsigned char value);
    static uint16_t crcUpdateCCITT(unsigned short fcs, const unsigned char* pBuf, unsigned bufLen);

    // Calculate encoded length for payload
    uint32_t calcEncodedPayloadLen(const uint8_t *pFrame, uint32_t frameLen);

    // Frame boundary and escape values
    static const uint8_t FRAME_BOUNDARY_OCTET_DEFAULT = 0xE7;
    static const uint8_t CONTROL_ESCAPE_OCTET_DEFAULT = 0xD7;

private:
    // If either of the following two octets appears in the transmitted data, an escape octet is sent,
    // followed by the original data octet with bit 5 inverted

    // The frame boundary octet
    uint8_t _frameBoundaryOctet = 0;

    // Control escape octet
    uint8_t _controlEscapeOctet = 0;

    // Invert octet explained above
    static constexpr uint8_t INVERT_OCTET = 0x20;

    // The frame check sequence (FCS) is a 16-bit CRC-CCITT
    // AVR Libc CRC function is _crc_ccitt_update()
    // Corresponding CRC function in Qt (www.qt.io) is qChecksum()
    static constexpr uint16_t CRC16_CCITT_INIT_VAL = 0xFFFF;

    // CRC table
    static const uint16_t _CRCTable[256];

    // Callback functions for PutCh/PutBit and FrameRx
    MiniHDLCPutChFnType _putChFn;
    MiniHDLCFrameFnType _frameRxFn;
    MiniHDLCFrameFnType _frameTxFn;

    // Bitwise HDLC flag (otherwise byte-wise)
    bool _bitwiseHDLC = false;

    // Send FCS (CRC) big-endian - i.e. high byte first
    bool _bigEndianCRC = false;

    // State vars
    uint16_t _framePos = 0;
    uint16_t _frameCRC = CRC16_CCITT_INIT_VAL;
    bool _inEscapeSeq = false;

    // Bitwise state
    uint8_t _bitwiseLast8Bits = 0;
    uint8_t _bitwiseByte = 0;
    uint8_t _bitwiseBitCount = 0;
    uint8_t _bitwiseSendOnesCount = 0;

    // Receive buffer
    SimpleBuffer _rxBuffer;
    uint16_t _rxBufferMaxLen = 0;

    // Transmit buffer
    SimpleBuffer _txBuffer;
    uint16_t _txBufferMaxLen = 0;
    uint16_t _txBufferPos = 0;
    uint16_t _txBufferBitPos = 0;

    // Stats
    MiniHDLCStats _stats;

private:
    void sendChar(uint8_t ch);
    void sendCharWithStuffing(uint8_t ch);
    void sendEscaped(uint8_t ch);
    uint32_t putEscaped(uint8_t ch, uint8_t* pBuf, uint32_t pos);
    void clear();
    void putCharToFrame(uint8_t ch);

    // Get CRC from buffer
    uint16_t getCRCFromBuffer()
    {
        if (_bigEndianCRC)
            return _rxBuffer.getAt(_framePos - 1) | (((uint16_t)_rxBuffer.getAt(_framePos-2)) << 8);
        return _rxBuffer.getAt(_framePos - 2) | (((uint16_t)_rxBuffer.getAt(_framePos-1)) << 8);
    }
};
