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

#define DEBUG_SCADER_BTHOME

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
            [this](RaftDevice& device, const BusAddrStatus& addrStatus) {
                // Debug
#ifdef DEBUG_SCADER_BTHOME
                LOG_I(MODULE_PREFIX, "deviceStatusChangeCB %s %s %s", 
                        device.getDeviceID().toString().c_str(), BusAddrStatus::getOnlineStateStr(addrStatus.onlineState), addrStatus.isNewlyIdentified ? "New" : "");
#endif

                // Register for device data notifications if required
                if (addrStatus.isNewlyIdentified)
                {
                    // Get the decode function
                    DeviceTypeRecord deviceTypeRecord;
                    DeviceTypeIndexType deviceTypeIdx = 0;
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
                            // Add to queue and bump the received counter (drives change detection)
                            _bthomeUpdateQueue.put(BTHomeUpdate{data, millis()});
                            _updatesEnqueued++;

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
            [this](uint16_t topicIndex, CommsChannelMsg& msg) {
                String statusStr = getStatusJSON();
                msg.setFromBuffer((uint8_t*)statusStr.c_str(), statusStr.length());
                return true;
            },
            [this](uint16_t topicIndex, std::vector<uint8_t>& stateHash) {
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
    // Drain all pending updates into the elems array. getStatusJSON is the
    // publish message generator, so each publish flushes the whole queue (the
    // change-detection hash, based on _updatesEnqueued, fires once per new
    // update so queued readings are published promptly rather than backing up).
    String elems;
    uint32_t numElems = 0;
    BTHomeUpdate bthomeUpdate;
    while (_bthomeUpdateQueue.get(bthomeUpdate))
    {
        // Decode data
        poll_BLEBTHome deviceData;
        if (!_pDecodeFn)
            continue;
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

        if (recsDecoded == 0)
            continue;

        // MAC address
        String macAddr = Raft::formatMACAddr(((uint8_t*)&deviceData.MAC), ":", true);

        // Form JSON for this element
        if (numElems > 0)
            elems += ",";
        elems += "{\"timeMs\":" + String(bthomeUpdate.timestampMs) + ",";
        elems += "\"mac\":\"" + macAddr + "\",";
        elems += "\"motion\":" + String(deviceData.motion);
        if (deviceData.battery != 255)
            elems += ",\"batt\":" + String(deviceData.battery);
        if (deviceData.temp < 200.0)
            elems += ",\"temp\":" + String(deviceData.temp);
        if (deviceData.light < 10000000.0)
            elems += ",\"light\":" + String(deviceData.light);
        elems += "}";
        numElems++;
    }

#ifdef DEBUG_SCADER_BTHOME
    LOG_I(MODULE_PREFIX, "getStatusJSON numElems %d", (int)numElems);
#endif

    return "{" + _scaderCommon.getStatusJSON() + ",\"elems\":[" + elems + "]}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check status change
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderBTHome::getStatusHash(std::vector<uint8_t>& stateHash)
{
    // Base the hash on the monotonic count of updates received. Each new update
    // changes the hash (triggering a change-based publish); draining the queue
    // does not change it, so there is no spurious re-publish once it is empty.
    uint32_t count = _updatesEnqueued;
    stateHash.clear();
    stateHash.push_back(count & 0xff);
    stateHash.push_back((count >> 8) & 0xff);
    stateHash.push_back((count >> 16) & 0xff);
    stateHash.push_back((count >> 24) & 0xff);
}
