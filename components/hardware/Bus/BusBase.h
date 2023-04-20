/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Bus Base-class
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ArduinoOrAlt.h>
#include "BusConsts.h"
#include "BusStats.h"
#include <vector>
#include <functional>

class ConfigBase;
class BusRequestInfo;
class BusBase;

typedef std::function<void(BusBase& bus, const std::vector<BusElemAddrAndStatus>& statusChanges)> BusElemStatusCB;
typedef std::function<void(BusBase& bus, BusOperationStatus busOperationStatus)> BusOperationStatusCB;

class BusBase
{
public:
    // Constructor
    BusBase(BusElemStatusCB busElemStatusCB, BusOperationStatusCB busOperationStatusCB)
        : _busElemStatusCB(busElemStatusCB), _busOperationStatusCB(busOperationStatusCB)
    {
    }
    virtual ~BusBase()
    {
    }

    // Setup
    virtual bool setup(ConfigBase& config, const char* pConfigPrefix)
    {
        return false;
    }

    // Close
    virtual void close()
    {
    }

    // Service
    virtual void service()
    {
    }

    // Clear
    virtual void clear(bool incPolling)
    {
    }

    // Pause
    virtual void pause(bool pause)
    {
    }

    // IsPaused
    virtual bool isPaused()
    {
        return false;
    }

    // Hiatus for period of ms
    virtual void hiatus(uint32_t forPeriodMs)
    {
    }

    // IsHiatus
    virtual bool isHiatus()
    {
        return false;
    }

    // isOperatingOk
    virtual BusOperationStatus isOperatingOk() const
    {
        return BusOperationStatus::BUS_OPERATION_OK;
    }

    // isReady (for new requests)
    virtual bool isReady()
    {
        return false;
    }

    // Get Name
    virtual String getBusName() const
    {
        return "";
    }

    // Request bus action
    virtual bool addRequest(BusRequestInfo& busReqInfo)
    {
        return false;
    }

    // Get stats
    virtual String getBusStatsJSON()
    {
        return _busStats.getStatsJSON(getBusName());
    }

    // Check bus element responding
    virtual bool isElemResponding(uint32_t address, bool* pIsValid = nullptr)
    {
        if (pIsValid)
            *pIsValid = false;
        return true;
    }

    // Request (or suspend) slow scanning and optionally request a fast scan
    virtual void requestScan(bool enableSlowScan, bool requestFastScan)
    {
    }

    // Clear receive buffer
    virtual void rxDataClear()
    {
    }
    
    // Received data bytes available
    virtual uint32_t rxDataBytesAvailable()
    {
        return 0;
    }

    // Get rx data - returns number of bytes placed in pData buffer
    virtual uint32_t rxDataGet(uint8_t* pData, uint32_t maxLen)
    {
        return 0;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Get the bus operation status as a string
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static const char *busOperationStatusToString(BusOperationStatus busOperationStatus)
    {
        switch (busOperationStatus)
        {
        case BUS_OPERATION_OK: return "Ok";
        case BUS_OPERATION_FAILING: return "Failing";
        default: return "Unknown";
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Get the bus element address as a string
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static String busElemAddrAndStatusToString(BusElemAddrAndStatus busElemAddr)
    {
        return "0x" + String(busElemAddr.address, 16) + ":" +
                                (busElemAddr.isChangeToOnline ? "Online" : "Offline");
    }    

protected:
    BusStats _busStats;
    BusElemStatusCB _busElemStatusCB;
    BusOperationStatusCB _busOperationStatusCB;
};
