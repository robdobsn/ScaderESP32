/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Serial Console
// Provides serial terminal access to REST API and diagnostics
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include <ProtocolOverAscii.h>
#include <SpiramAwareAllocator.h>

class RestAPIEndpointManager;
class CommsCoreIF;
class CommsChannelMsg;
class ConfigBase;

class SerialConsole : public SysModBase
{
public:
    SerialConsole(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

    // Get
    int getChar();

    // Put
    void putStr(const char* pStr);
    void putStr(const String& str);

    // Get the state of the reception of Commands 
    // Maybe:
    //   idle = 'i' = no command entry in progress,
    //   newChar = XOFF = new command char received since last call - pause transmission
    //   waiting = 'w' = command incomplete but no new char since last call
    //   complete = XON = command completed - resume transmission
    typedef char CommandRxState;
    CommandRxState getXonXoff();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;
    
    // Add comms channels
    virtual void addCommsChannels(CommsCoreIF& commsCoreIF) override final;

    // Handle JSON command
    virtual UtilsRetCode::RetCode receiveCmdJSON(const char* cmdJSON) override final;

public:
    // XON/XOFF hadnling
    static constexpr char ASCII_XOFF = 0x13;
    static constexpr char ASCII_XON = 0x11;

    // State
    static constexpr CommandRxState CommandRx_idle = 'i';
    static constexpr CommandRxState CommandRx_newChar = ASCII_XOFF;
    static constexpr CommandRxState CommandRx_waiting = 'w';
    static constexpr CommandRxState CommandRx_complete = ASCII_XON;

private:
    // isEnabled and isInitialised
    bool _isEnabled;
    bool _isInitialised;

    // CRLF string on tx line end
    bool _crlfOnTx;

    // Serial details
    int _uartNum;
    int _baudRate;

    // Buffer sizes
    uint32_t _rxBufferSize;
    uint32_t _txBufferSize;

    // Bytes to process in service call
    static const uint32_t MAX_BYTES_TO_PROCESS_IN_SERVICE = 100;

    // Procotol
    String _protocol;

    // Line being entered
    String _curLine;

    // Line limits
    static const int MAX_REGULAR_LINE_LEN = 100;
    static const int ABS_MAX_LINE_LEN = 1000;

    // Prev char entered (for line end checks)
    int _prevChar;

    // Cur state (XON/XOFF etc)
    CommandRxState _cmdRxState;

    // EndpointID used to identify this message channel to the CommsChannelManager object
    uint32_t _commsChannelID;

    // ProtocolOverAscii to handle comms through this serial port with MSB set
    static const uint32_t PROTOCOL_OVER_ASCII_MSG_MAX_LEN = 1000;
    ProtocolOverAscii _protocolOverAscii;

    // Helpers
    void showEndpoints();
    bool sendMsg(CommsChannelMsg& msg);
    void processReceivedData(std::vector<uint8_t, SpiramAwareAllocator<uint8_t>>& rxData);
};
