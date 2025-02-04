/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderOpener
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScaderOpener.h"
#include "RaftArduino.h"
#include "ScaderCommon.h"
#include "RaftUtils.h"
#include "ConfigPinMap.h"
#include "RestAPIEndpointManager.h"
#include "CommsChannelMsg.h"
#include "SysManager.h"
#include "esp_attr.h"
#include "driver/gpio.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderOpener::ScaderOpener(const char* pModuleName, RaftJsonIF& sysConfig)
    : RaftSysMod(pModuleName, sysConfig),
          _scaderCommon(*this, sysConfig, pModuleName),
          _scaderModuleState("scaderOpener"),
          _doorOpener(_scaderModuleState)
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

    // Configure opener
    _doorOpener.setup(getSysManager() ? getSysManager()->getDeviceManager() : nullptr, configGetConfig());

    // Configure UI module
    _uiModule.setup(configGetConfig(), &_doorOpener);

    // HW Now initialised
    _isInitialised = true;

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled scaderUIName %s", 
                _scaderCommon.getUIName().c_str());

    // Setup publisher with callback functions
    SysManager* pSysManager = getSysManager();
    if (pSysManager)
    {
        // Register publish message generator
        pSysManager->registerDataSource("Publish", _scaderCommon.getModuleName().c_str(), 
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
// loop - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderOpener::loop()
{
    // Check initialised
    if (!_isInitialised)
        return;

    // Service door opener
    _doorOpener.loop();

    // Service UI module
    _uiModule.loop();
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

RaftRetCode ScaderOpener::apiControl(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    // Extract params
    std::vector<String> params;
    std::vector<RaftJson::NameValuePair> nameValues;
    RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
    String paramsJSON = RaftJson::getJSONFromNVPairs(nameValues, true);

    // Handle commands
    bool rslt = true;
    String rsltStr;
    if (params.size() > 1)
    {
        // Open
        if (params[1] == "open")
        {
            _doorOpener.startDoorOpening("API-open");
            rsltStr = "Opened";
        }
        // Close
        else if (params[1] == "close")
        {
            _doorOpener.startDoorClosing("API-close");
            rsltStr = "Closed";
        }
        // Stop
        else if (params[1] == "stopanddisable")
        {
            _doorOpener.doorStopAndDisable("API-stopanddisable");
            rsltStr = "Stopped";
        }
        // // Calibrate
        // else if (params[1] == "calibrate")
        // {
        //     _doorOpener.calibrate();
        //     rsltStr = "Calibrating";
        // }
        // Set in-enable
        else if (params[1] == "inenable")
        {
            if (params.size() > 1)
            {
                bool enVal = params[2] == "true" || params[2] == "1";
                _doorOpener.setInEnabled(enVal);
                rsltStr = enVal ? "In enabled" : "In disabled";
            }
        }
        // Set out-enable
        else if (params[1] == "outenable")
        {
            if (params.size() > 1)
            {
                bool enVal = params[2] == "true" || params[2] == "1";
                _doorOpener.setOutEnabled(enVal);
                rsltStr = enVal ? "Out enabled" : "Out disabled";
            }
        }
        // // Set open pos
        // else if (params[1] == "setopenpos")
        // {
        //     _doorOpener.setOpenPosition();
        //     rsltStr = "Open position set";
        // }
        // // Set closed pos
        // else if (params[1] == "setclosedpos")
        // {
        //     _doorOpener.setClosedPosition();
        //     rsltStr = "Closed position set";
        // }
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
                        _doorOpener.debugMoveToAngle(degrees);
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
        LOG_I(MODULE_PREFIX, "apiControl: reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
        return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
    }
    LOG_E(MODULE_PREFIX, "apiControl: FAILED reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
    return Raft::setJsonErrorResult(reqStr.c_str(), respStr, rsltStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderOpener::getStatusJSON() const
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
