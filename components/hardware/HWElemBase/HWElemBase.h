/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware Elem Base
// Base for hardwware elements - like a smart-servo - that can accept and receive messages
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <BusBase.h>
#include <BusRequestInfo.h>
#include <HWElemMsg.h>
#include <HWElemConsts.h>
#include <ConfigBase.h>
#include <list>
#include <UtilsRetCode.h>

class HWElemReq;

typedef std::function<void(bool isDetected, uint32_t safetyCode, uint32_t elemIDNo)> HWElemSafetiesCB;
typedef std::function<void(const char* eventJson)> HWElemEventCB;

static const uint32_t HWELEM_SAFETY_FREEFALL_MASK = 0x01;
static const uint32_t HWELEM_SAFETY_OVER_CURRENT_MASK = 0x02;

enum HWElemAddOnFamily
{
    ADD_ON_FAMILY_ORDINARY_I2C,
    ADD_ON_FAMILY_ROBOTICAL_STD
};

enum HWElemWhoAmITypeCode
{
    WHOAMI_TYPE_CODE_I2CFUELGAUGE_BASE = 0x10000000,
    WHOAMI_TYPE_CODE_I2CIMU_BASE = 0x10010000,
    WHOAMI_TYPE_CODE_BUSPIX_BASE = 0x10020000,
    WHOAMI_TYPE_CODE_POWER_CTRL = 0x10030000,
    WHOAMI_TYPE_CODE_GPIO_BASE = 0x10040000,
    WHOAMI_TYPE_CODE_NONE = 0xffffffff,
};

enum HWElemSafetyAction
{
    SAFETY_ACTION_NONE,
    SAFETY_ACTION_DISABLE,
    SAFETY_ACTION_DISABLE_AND_CLEAR_QUEUES,
    SAFETY_ACTION_CLEAR_QUEUES,
    SAFETY_ACTION_RESUME,
};

class CommsCoreIF;

class HWElemBase
{
    friend class HWElemRSAO;
    friend class HWElemNSAO;
public:
    HWElemBase();
    virtual ~HWElemBase();

    // Setup
    virtual void setup(ConfigBase& config, ConfigBase* pDefaults, const char* pConfigPrefix);

    // Post-Setup - called after any buses have been connected
    virtual void postSetup()
    {
    }

    // Service
    virtual void service();

    // Set CommsChannelManager
    void setCommsCore(CommsCoreIF* pCommsCore)
    {
        _pCommsCore = pCommsCore;
    }

    // Reset
    virtual void reset()
    {
    }

    // Manage hwElem
    virtual void manage(bool pause, bool resume, bool clearQueues, bool stopMoves)
    {
    }

    // Get name
    virtual String getName()
    {
        return _name;
    }

    // Set IDNo
    virtual void setIDNo(uint32_t elemIDNo)
    {
        _IDNo = elemIDNo;
    }

    // Get IDNo
    virtual uint32_t getIDNo()
    {
        return _IDNo;
    }

    // Check if name matches
    virtual bool isNamed(const char* pStr)
    {
        return (_name.equals(pStr));
    }

    // Check if address matches
    virtual bool isAtAddress(uint32_t address)
    {
        if (!_addressIsSet)
            return false;
        return address == _address;
    }

    // Check if busName matches
    virtual bool isOnBus(const char* busName)
    {
        return _busName.equals(busName);
    }

    // Set add-on family
    virtual void setAddOnFamily(HWElemAddOnFamily addOnFamily)
    {
        _addOnFamily = addOnFamily;
    }

    // Check if this is an add-on
    virtual HWElemAddOnFamily getAddOnFamily()
    {
        return _addOnFamily;
    }

    // Get type
    virtual String getType()
    {
        return _type;
    }

    // Get address
    virtual uint32_t getAddress()
    {
        return _address;
    }

    // Address is set
    virtual bool isAddressSet()
    {
        return _addressIsSet;
    }

    // Get version string
    virtual String getVersionStr()
    {
        return _versionStr;
    }

    // Get whoAmI string
    virtual String getWhoAmIStr()
    {
        return _whoAmIStr;
    }

    // Get whoAmITypeCode
    virtual uint32_t getWhoAmITypeCode()
    {
        return _whoAmITypeCode;
    }

    // Get SerialNo string
    virtual String getSerialNo()
    {
        return _serialNo;
    }

    // Is operating ok
    virtual bool isOperatingOk()
    {
        return false;
    }

    // Get bus name
    virtual String getBusName()
    {
        return _busName;
    }

    // Get bus
    virtual BusBase* getBus()
    {
        return _pBus;
    }
    
    // Connect to bus
    virtual void connectToBus(BusBase* pBus)
    {
        _pBus = pBus;
    }

    // Set safe mode
    virtual void setSafety(HWElemSafetyAction safetyAction)
    {
    }

    // Get safety status
    virtual uint32_t getSafetyStatus()
    {
        return 0;
    }

    // Set a named value
    virtual void setNamedValue(const char* name, double value)
    {
    }

    // Get a named value
    virtual double getNamedValue(const char* param, bool& isValid)
    {
        return 0;
    }

    // Get a set of data
    virtual String getNamedValueJson(const char* param, bool& isValid)
    {
        return "";
    }

    // Send a binary-encoded command - format specific to hardware
    virtual UtilsRetCode::RetCode sendCmdBinary(uint32_t formatCode, const uint8_t* pData, uint32_t dataLen)
    {
        return UtilsRetCode::INVALID_OPERATION;
    }

    // Send a binary-encoded command - format specific to hardware
    virtual UtilsRetCode::RetCode sendCmdBinary(HWElemMsg& msg)
    {
        return sendCmdBinary(msg._formatCode, msg._data.data(), msg._data.size());
    }

    // Get values binary = format specific to hardware
    virtual uint32_t getValsBinary(uint32_t formatCode, uint8_t* pBuf, uint32_t bufMaxLen)
    {
        return 0;
    }

    // Send JSON command
    virtual UtilsRetCode::RetCode sendCmdJSON(const char* jsonCmd);

    // Set callback on events
    virtual bool setEventCallback(const char* eventParamsJSON, HWElemEventCB callback)
    {
        return false;
    }

    virtual bool hasCapability(const char* pCapabilityStr)
    {
        return false;
    }

    // Get info JSON
    virtual String getInfoJSON(bool includeStatus, bool includedOuterBraces, HWElemStatusLevel_t dataLevel = ELEM_STATUS_LEVEL_NONE)
    {
        String jsonStr;
        if (includeStatus)
        {
            char jsonInfoStr[400];
            bool isValid = false;
            bool isOnline = isElemResponding(&isValid);
            snprintf(jsonInfoStr, sizeof(jsonInfoStr),
                    R"("name":"%s","type":"%s","busName":"%s","addr":"0x%02x","addrValid":%d,"IDNo":%d,)"
                    R"("whoAmI":"%s","whoAmITypeCode":"%08x","SN":"%s","versionStr":"%s","commsOk":"%c")",
                    _name.c_str(), _type.c_str(), _busName.c_str(), _address, _addressIsSet,
                    _IDNo, _whoAmIStr.c_str(), _whoAmITypeCode, _serialNo.c_str(), _versionStr.c_str(),
                    isValid ? (isOnline ? 'Y' : 'N') : 'X');
            jsonStr += jsonInfoStr;
        }
        if (dataLevel != ELEM_STATUS_LEVEL_NONE)
        {
            String dataJson = getDataJSON(dataLevel);
            if (dataJson.length() > 0)
            {
                if (jsonStr.length() > 0)
                    jsonStr += ",";
                jsonStr += dataJson;
            }
        }
        if (includedOuterBraces)
            return "{" + jsonStr + "}";
        return jsonStr;
    }

    // Request reinitialisation of element
    virtual void requestReinit()
    {
    }

    // Check if elem responding
    virtual bool isElemResponding(bool* pIsValid = nullptr)
    {
        // Return true for elements that don't use a bus
        if (pIsValid)
            *pIsValid = true;
        if (!_pBus)
            return true;

        // Check valid
        if (pIsValid)
            *pIsValid = false;
        if (!_addressIsSet)
            return false;

        // Check if responding
        return _pBus->isElemResponding(_address, pIsValid);
    }

    // Enable hardware if required
    virtual void enableHardware(bool enable)
    {
    }

    // Send default parameters to element
    virtual void setDefaultParams()
    {
    }

    // Set default name for element if name is empty
    virtual void setDefaultNameIfEmpty()
    {
        // Check name
        if (_name.length() != 0)
            return;

        // Setup name from address
        char tmpStr[50];
        snprintf(tmpStr, sizeof(tmpStr), "AddOn_%s_%02X", _busName.c_str(), _address);
        _name = tmpStr;
    }

    // Set bus name if valid
    virtual void setBusNameIfValid(const char* pBusNameOrNull)
    {
        if (pBusNameOrNull)
            _busName = pBusNameOrNull;
    }

    // Non-virtual Helpers
    // Make a request on the connected bus
    bool makeBusRequest(const HWElemReq& hwElemReq, BusRequestCallbackType callback, void* callbackParam,
                    BusReqType reqType = BUS_REQ_TYPE_STD);

    // Queue bus requests for sending
    void queuedBusReqClear();
    bool queuedBusReqAdd(const HWElemReq& hwElemReq, BusReqType reqType = BUS_REQ_TYPE_STD);
    bool queuedBusReqStart(BusRequestCallbackType callback, void* callbackParam);

    // Poll rate
    double getPollRateHz()
    {
        return _pollRateHz;
    }

    // Timeout on polling
    uint32_t getPollTimeoutMs()
    {
        return _pollTimeoutMs;
    }

    // Set poll rate and timeout
    void setPollRateAndTimeout(double pollRate)
    {
        _pollRateHz = pollRate;
        // Calculate timeout to be somewhat greater than time expected for poll to be received
        _pollTimeoutMs = (pollRate != 0) ? 1100 / pollRate + 250 : 10000;
    }

protected:
    // Get data JSON
    virtual String getDataJSON(HWElemStatusLevel_t level = ELEM_STATUS_LEVEL_MIN)
    {
        // Empty string is valid and ignored
        return "";
    }

    // Get string with defaults
    String getStringWithDefault(const char* key, const char* fallbackDefault,
                                ConfigBase& config, ConfigBase* pDefs, const char* pConfigPrefix)
    {
        return config.getString(key, pDefs ? pDefs->getString(key, fallbackDefault) : fallbackDefault, pConfigPrefix);
    }

    // Get double with defaults
    double getDoubleWithDefault(const char* key, double fallbackDefault,
                                ConfigBase& config, ConfigBase* pDefs, const char* pConfigPrefix)
    {
        return config.getDouble(key, pDefs ? pDefs->getDouble(key, fallbackDefault) : fallbackDefault, pConfigPrefix);
    }

    // Get long with defaults
    long getLongWithDefault(const char* key, long fallbackDefault,
                            ConfigBase& config, ConfigBase* pDefs, const char* pConfigPrefix)
    {
        return config.getLong(key, pDefs ? pDefs->getLong(key, fallbackDefault) : fallbackDefault, pConfigPrefix);
    }

    // Get bool with defaults
    bool getBoolWithDefault(const char* key, bool fallbackDefault,
                            ConfigBase& config, ConfigBase* pDefs, const char* pConfigPrefix)
    {
        return config.getBool(key, pDefs ? pDefs->getBool(key, fallbackDefault) : fallbackDefault, pConfigPrefix);
    }

    // Set type
    void setType(const char* type)
    {
        _type = type;
    }

    // Get pollFor
    String getPollFor()
    {
        return _pollFor;
    }

    // Set whoAmITypeCode
    void setWhoAmITypeCode(uint32_t whoAmITypeCode)
    {
        _whoAmITypeCode = whoAmITypeCode;
    }

    // Set whoAmIStr
    void setWhoAmIStr(const char* whoAmIStr)
    {
        _whoAmIStr = whoAmIStr;
    }

    // Set serial number
    void setSerialNo(const char* serialNo)
    {
        _serialNo = serialNo;
    }

    // Set version string
    void setVersionStr(const char* versionStr)
    {
        _versionStr = versionStr;
    }

    // Bus this element is connected to
    BusBase* _pBus = nullptr;

    // Settings
    String _name;
    String _type;
    String _busName;
    uint16_t _address = 0;
    bool _addressIsSet = false;
    int32_t _IDNo = 0;
    HWElemAddOnFamily _addOnFamily = ADD_ON_FAMILY_ORDINARY_I2C;

    // Polling
    String _pollFor;
    float _pollRateHz;
    uint32_t _pollTimeoutMs;

    // WhoAmI and serial
    String _whoAmIStr;
    uint32_t _whoAmITypeCode = WHOAMI_TYPE_CODE_NONE;
    String _serialNo;

    // Version
    String _versionStr = "0.0.0";

    // Queue of bus requests
    static const uint32_t MAX_QUEUED_BUS_REQS = 10;
    std::list<HWElemReq> _queuedBusReqs;
    bool _queuedBusReqsActive = false;
    uint32_t _queuedBusReqLastStartMs = 0;
    uint32_t _queuedBusReqHoldOffMs = 0;

    // Command response message key
    String _cmdResponseMsgKey;

    // Comms channel manager - for comms with system
    CommsCoreIF* _pCommsCore = nullptr;

    // Consts
    static const int MAX_RAW_WRITE_BYTES = 64;
    static const int SEND_CMD_JSON_REQ = 100;

    // Helpers
    static void cmdResultCallbackStatic(void* pCallbackData, BusRequestResult& reqResult);
    void cmdResultCallback(BusRequestResult& reqResult);
};
