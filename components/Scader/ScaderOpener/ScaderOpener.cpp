/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderOpener
//
// Rob Dobson 2013-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderOpener.h"
#include <ArduinoOrAlt.h>
#include <ScaderCommon.h>
#include <RaftUtils.h>
#include <ConfigPinMap.h>
#include <RestAPIEndpointManager.h>
#include <CommsChannelMsg.h>
#include <SysManager.h>
#include <esp_attr.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "ScaderOpener";

#define DEBUG_OPENER_MUTABLE_DATA

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderOpener::ScaderOpener(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig, NULL, true),
          _scaderCommon(*this, pModuleName)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::setup()
{
    // Common setup
    _scaderCommon.setup();

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // Configure
    _doorOpener.setup(configGetConfig());

    // HW Now initialised
    _isInitialised = true;

    // Get mutable data
    bool inEnabled = configGetBool("openerState/inEn", false);
    bool outEnabled = configGetBool("openerState/outEn", false);

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled name %s inEnabled %d outEnabled %d", 
                _scaderCommon.getFriendlyName().c_str(),
                inEnabled, outEnabled);

    // Setup publisher with callback functions
    SysManager* pSysManager = getSysManager();
    if (pSysManager)
    {
        // Register publish message generator
        pSysManager->sendMsgGenCB("Publish", _scaderCommon.getModuleName().c_str(), 
            [this](const char* messageName, CommsChannelMsg& msg) {
                String statusStr = getStatusJSON();
                msg.setFromBuffer((uint8_t*)statusStr.c_str(), statusStr.length());
                return true;
            },
            [this](const char* messageName, std::vector<uint8_t>& stateHash) {
                return getStatusHash(stateHash);
            }
        );
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::service()
{
    // Check initialised
    if (!_isInitialised)
        return;

    // Service door opener
    _doorOpener.service();

    // Check if mutable data changed
    if (_doorOpener.modeHasChanged())
    {
        // Check if min time has passed
        if (Raft::isTimeout(millis(), _mutableDataChangeLastMs, MUTABLE_DATA_SAVE_MIN_MS))
        {
            // Save mutable data
            saveMutableData();
            _doorOpener.modeChangeClear();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// isBusy
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderOpener::isBusy()
{
    return _doorOpener.isBusy();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    // Control shade
    endpointManager.addEndpoint("opener", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&ScaderOpener::apiControl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "Control Opener - /open or /close or /mode/in or /mode/out or /mode/both or /mode/none or mode/open or /test/motoron, /test/motoroff, /test/turnto/<degrees>");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control via API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Extract params
    std::vector<String> params;
    std::vector<RdJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    String paramsJSON = RdJson::getJSONFromNVPairs(nameValues, true);

    // Handle commands
    bool rslt = true;
    String rsltStr;
    if (params.size() > 0)
    {
        // Open
        if (params[1] == "open")
        {
            _doorOpener.motorMoveToPosition(true);
            rsltStr = "Opened";
        }
        // Close
        else if (params[1] == "close")
        {
            _doorOpener.motorMoveToPosition(false);
            rsltStr = "Closed";
        }
        // Stop
        else if (params[1] == "stopanddisable")
        {
            _doorOpener.stopAndDisableDoor();
            rsltStr = "Stopped";
        }
        // Calibrate
        else if (params[1] == "calibrate")
        {
            _doorOpener.calibrate();
            rsltStr = "Calibrating";
        }
        // Set in-enable
        else if (params[1] == "inenable")
        {
            if (params.size() > 1)
            {
                bool enVal = params[2] == "true" || params[2] == "1";
                _doorOpener.setMode(enVal, _doorOpener.isOutEnabled(), true);
                rsltStr = enVal ? "In enabled" : "In disabled";
            }
        }
        // Set out-enable
        else if (params[1] == "outenable")
        {
            if (params.size() > 1)
            {
                bool enVal = params[2] == "true" || params[2] == "1";
                _doorOpener.setMode(_doorOpener.isInEnabled(), enVal, true);
                rsltStr = enVal ? "Out enabled" : "Out disabled";
            }
        }
        // Mode
        else if (params[1] == "mode")
        {
            if (params.size() > 1)
            {
                if (params[2] == "in")
                {
                    _doorOpener.setMode(true, false, true);
                    rsltStr = "Mode set to IN";
                }
                else if (params[2] == "out")
                {
                    _doorOpener.setMode(false, true, true);
                    rsltStr = "Mode set to OUT";
                }
                else if (params[2] == "both")
                {
                    _doorOpener.setMode(true, true, true);
                    rsltStr = "Mode set to BOTH";
                }
                else if (params[2] == "none")
                {
                    _doorOpener.setMode(false, false, true);
                    rsltStr = "Mode set to NONE";
                }
                else
                {
                    rsltStr = "Invalid mode";
                    rslt = false;
                }
            }
            else
            {
                rsltStr = "Mode not specified";
                rslt = false;
            }
        }

        // Test
        else if (params[1] == "test")
        {
            if (params.size() > 1)
            {
                if (params[2] == "turnto")
                {
                    if (params.size() > 2)
                    {
                        int degrees = params[3].toInt();
                        _doorOpener.motorMoveAngle(degrees, true, degrees / 10);
                        rsltStr = "Turned " + String(degrees) + " degrees";
                    }
                    else
                    {
                        rsltStr = "Degrees not specified";
                        rslt = false;
                    }
                }
                else
                {
                    rsltStr = "Invalid test command";
                    rslt = false;
                }
            }
            else
            {
                rsltStr = "Test command not specified";
                rslt = false;
            }
        }
        // Unknown
        else
        {
            rsltStr = "Unknown command";
            rslt = false;
        }
    }
    else
    {
        rsltStr = "No command specified";
        rslt = false;
    }    

    // Result
    if (rslt)
    {
        Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
        LOG_I(MODULE_PREFIX, "apiControl: reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
    }
    else
    {
        Raft::setJsonErrorResult(reqStr.c_str(), respStr, rsltStr.c_str());
        LOG_E(MODULE_PREFIX, "apiControl: FAILED reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderOpener::getStatusJSON()
{
    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + ",\"status\":{" + _doorOpener.getStatusJSON(false) + "}}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::getStatusHash(std::vector<uint8_t>& stateHash)
{
    _doorOpener.getStatusHash(stateHash);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write the mutable config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::saveMutableData()
{
    // Save relay states
    String jsonConfig = R"("openerState":{"inEn":__INEN__,"outEn":__OUTEN__})";
    jsonConfig.replace("__INEN__", String(_doorOpener.isInEnabled()));
    jsonConfig.replace("__OUTEN__", String(_doorOpener.isOutEnabled()));

    // Add outer brackets
    jsonConfig = "{" + jsonConfig + "}";
#ifdef DEBUG_OPENER_MUTABLE_DATA
    LOG_I(MODULE_PREFIX, "saveMutableData %s", jsonConfig.c_str());
#endif
    SysModBase::configSaveData(jsonConfig);
}
