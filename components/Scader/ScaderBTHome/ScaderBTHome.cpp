/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ScaderBTHome
//
// Rob Dobson 2013-2024
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <time.h>
#include "RaftCore.h"
#include "ScaderBTHome.h"

// #define DEBUG_SCADER_BTHOME

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderBTHome::ScaderBTHome(const char *pModuleName, RaftJsonIF& sysConfig)
        : RaftSysMod(pModuleName, sysConfig),
          _scaderCommon(*this, sysConfig, pModuleName)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderBTHome::setup()
{
    // Common
    _scaderCommon.setup();

    // Check enabled
    if (!_scaderCommon.isEnabled())
    {
        LOG_I(MODULE_PREFIX, "setup disabled");
        return;
    }

    // System manager and device manager
    SysManagerIF* pSysManager = getSysManager();
    DeviceManager* pDevMan = pSysManager ? pSysManager->getDeviceManager() : nullptr;

    // Register for device status updates
    if (pDevMan)
    {
        pDevMan->registerForDeviceStatusChange(
            [this](RaftDevice& device, bool isOnline, bool isNew) {
                // Debug
#ifdef DEBUG_SCADER_BTHOME
                LOG_I(MODULE_PREFIX, "deviceStatusChangeCB %s %s %s", 
                        device.getDeviceName().c_str(), isOnline ? "Online" : "Offline", isNew ? "New" : "");
#endif

                // Register for device data notifications if required
                if (isNew)
                {
                    // Get the decode function
                    DeviceTypeRecord deviceTypeRecord;
                    uint32_t deviceTypeIdx = 0;
                    String deviceTypeName = "BLEBTHome";
                    deviceTypeRecords.getDeviceInfo(deviceTypeName, deviceTypeRecord, deviceTypeIdx);
                    _pDecodeFn = deviceTypeRecord.pollResultDecodeFn;

#ifdef DEBUG_SCADER_BTHOME
                    LOG_I(MODULE_PREFIX, "deviceStatusChangeCB %s devTypeIdx %d (deviceRec %d) %p", 
                                deviceTypeName.c_str(), deviceTypeIdx, device.getDeviceTypeIndex(), _pDecodeFn);
#endif
                    
                    // Register for device data notification with the device
                    device.registerForDeviceData(
                        [this](uint16_t deviceTypeIdx, std::vector<uint8_t> data, const void* pCallbackInfo) {
                            // Add to queue
                            _bthomeUpdateQueue.put(BTHomeUpdate{data, millis()});

                            // Debug
#ifdef DEBUG_SCADER_BTHOME
                            LOG_I(MODULE_PREFIX, "deviceDataChangeCB %d %d", deviceTypeIdx, data.size());
#endif
                        },
                        0,
                        nullptr
                    );
                }
            }
        );
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup pDevMan %p", pDevMan);

    // Setup publisher with callback functions
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

    // HW Now initialised
    _isInitialised = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Loop (called frequently)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderBTHome::loop()
{
    // Check init
    if (!_isInitialised)
        return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ScaderBTHome::getStatusJSON() const
{
    // Check for any updates
    BTHomeUpdate bthomeUpdate;
    if (_bthomeUpdateQueue.get(bthomeUpdate))
    {
        // Debug
#ifdef DEBUG_SCADER_BTHOME
        LOG_I(MODULE_PREFIX, "getStatusJSON %d", bthomeUpdate.msgData.size());
#endif

        // Decode data
        poll_BLEBTHome deviceData;
        if (_pDecodeFn)
        {
            // Decode
            uint32_t recsDecoded = _pDecodeFn(bthomeUpdate.msgData.data(), bthomeUpdate.msgData.size(), 
                        &deviceData, sizeof(deviceData), 1, _decodeState);

#ifdef DEBUG_SCADER_BTHOME
            String hexStr = Raft::getHexStr(bthomeUpdate.msgData);
            LOG_I(MODULE_PREFIX, "getStatusJSON %s tsMs %d decoded recs %d ID %d MAC %llx %d %d %d %f %f", 
                    hexStr.c_str(), (unsigned int)bthomeUpdate.timestampMs,
                    recsDecoded,
                    deviceData.ID,
                    deviceData.MAC, 
                    deviceData.motion, 
                    deviceData.battery, 
                    deviceData.temp, 
                    deviceData.light);
#endif

            if (recsDecoded > 0)
            {
                // MAC address
                String macAddr = Raft::formatMACAddr(((uint8_t*)&deviceData.MAC), ":", true);

                // Form JSON
                String jsonStr;
                jsonStr += "{\"timeMs\":" + String(bthomeUpdate.timestampMs) + ",";
                jsonStr += "\"mac\":\"" + macAddr + "\",";
                jsonStr += "\"motion\":" + String(deviceData.motion);
                if (deviceData.battery != 255)
                    jsonStr += ",\"batt\":" + String(deviceData.battery);
                if (deviceData.temp < 200.0)
                    jsonStr += ",\"temp\":" + String(deviceData.temp);
                if (deviceData.light < 10000000.0)
                    jsonStr += ",\"light\":" + String(deviceData.light);
                jsonStr += "}";

#ifdef DEBUG_SCADER_BTHOME
                LOG_I(MODULE_PREFIX, "getStatusJSON %s", jsonStr.c_str());
#endif

                // Return JSON
                return "{" + _scaderCommon.getStatusJSON() + ",\"elems\":[" + jsonStr + "]}";
            }
        }
    }

    // Nothing to report
    return "{" + _scaderCommon.getStatusJSON() + "}";

    // Get status
    String elemStatus;
    // for (int i = 0; i < _elemNames.size(); i++)
    // {
    //     if (i > 0)
    //         elemStatus += ",";
    //     elemStatus += R"({"name":")" + _elemNames[i] + R"(","state":)" + String(_elemStates[i]) + "}";
    // }

    // Add base JSON
    return "{" + _scaderCommon.getStatusJSON() + ",\"elems\":[" + elemStatus + "]}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderBTHome::getStatusHash(std::vector<uint8_t>& stateHash)
{
    if (_bthomeUpdateQueue.count() == 0)
    {
        stateHash.clear();
        return;
    }

    // Check if state hash already has an element
    if (stateHash.size() == 0)
    {
        stateHash.push_back(0);
        return;
    }

    // Increment state hash
    stateHash[0]++;
}
