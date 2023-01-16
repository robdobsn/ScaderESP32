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
#include <SysManager.h>
#include <esp_attr.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "ScaderOpener";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderOpener::ScaderOpener(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig),
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

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled name %s", _scaderCommon.getFriendlyName().c_str());

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
    if (!_isInitialised)
        return;
    _doorOpener.service();
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
                            "Control Opener - /open or /close or /mode/in or /mode/out or /mode/both or /mode/none or mode/open - /setmotorontime/<secs>, /setopenpos, /setclosedpos - /test/motoron, /testmotoroff, /test/turnto/<degrees>");
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
            _doorOpener.moveDoor(true);
            rsltStr = "Opened";
        }
        // Close
        else if (params[1] == "close")
        {
            _doorOpener.moveDoor(false);
            rsltStr = "Closed";
        }
        // Stop
        else if (params[1] == "stop")
        {
            _doorOpener.stopDoor();
            rsltStr = "Stopped";
        }
        // Mode
        else if (params[1] == "mode")
        {
            if (params.size() > 1)
            {
                if (params[2] == "in")
                {
                    _doorOpener.setMode(true, false);
                    rsltStr = "Mode set to IN";
                }
                else if (params[2] == "out")
                {
                    _doorOpener.setMode(false, true);
                    rsltStr = "Mode set to OUT";
                }
                else if (params[2] == "both")
                {
                    _doorOpener.setMode(true, true);
                    rsltStr = "Mode set to BOTH";
                }
                else if (params[2] == "none")
                {
                    _doorOpener.setMode(false, false);
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
        // else if (params[1] == "setopenangle")
        // {
        //     // Get value passed in
        //     uint32_t openPos = DoorOpener::DEFAULT_DOOR_OPEN_ANGLE;
        //     if (params.size() > 2)
        //     {
        //         openPos = params[2].toInt();
        //     }

        //     // Send to opener
        //     _doorOpener.setOpenAngleDegrees(openPos);
            
        // }
        // else if (params[1] == "setmotorontime")
        // {
        //     // Get value passed in
        //     uint32_t motorOnTime = DoorOpener::DEFAULT_MOTOR_ON_TIME_SECS;
        //     if (params.size() > 2)
        //     {
        //         motorOnTime = params[2].toInt();
        //     }

        //     // Send to opener
        //     _doorOpener.setMotorOnTimeAfterMoveSecs(motorOnTime);
        // }
        // else if (params[1] == "setdooropentime")
        // {
        //     // Get value passed in
        //     uint32_t doorOpenTime = DoorOpener::DEFAULT_DOOR_OPEN_TIME_SECS;
        //     if (params.size() > 2)
        //     {
        //         doorOpenTime = params[2].toInt();
        //     }

        //     // Send to opener
        //     _doorOpener.setDoorOpenTimeSecs(doorOpenTime);
        // }
        // Test
        else if (params[1] == "test")
        {
            if (params.size() > 1)
            {
                if (params[2] == "motoron")
                {
                    _doorOpener.testMotorEnable(true);
                    rsltStr = "Motor on";
                }
                else if (params[2] == "motoroff")
                {
                    _doorOpener.testMotorEnable(false);
                    rsltStr = "Motor off";
                }
                else if (params[2] == "turnto")
                {
                    if (params.size() > 2)
                    {
                        int degrees = params[3].toInt();
                        _doorOpener.testMotorTurnTo(degrees);
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

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Write the mutable config
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void ScaderOpener::saveMutableData()
// {
//     // Save relay states
//     String jsonConfig = "\"DoorOpenAngle\":" + String(_doorOpener.getOpenAngleDegrees());
//     jsonConfig += ",\"MotorOnTimeAfterMoveSecs\":" + String(_doorOpener.getMotorOnTimeAfterMoveSecs());
//     jsonConfig += ",\"DoorOpenTimeSecs\":" + String(_doorOpener.getDoorOpenTimeSecs());

//     // Add outer brackets
//     jsonConfig = "{" + jsonConfig + "}";
//     LOG_I(MODULE_PREFIX, "saveMutableData %s", jsonConfig.c_str());
//     SysModBase::configSaveData(jsonConfig);
// }
