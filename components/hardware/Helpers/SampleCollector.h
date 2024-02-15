/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SampleCollector
// Collects samples and writes them to a file
//
// Rob Dobson 2023
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RaftArduino.h"
#include "RaftSysMod.h"
#include "SpiramAwareAllocator.h"
#include "APISourceInfo.h"
#include "RestAPIEndpointManager.h"
#include "RaftUtils.h"
#include "FileSystem.h"

template <class T>
class SampleCollector : public RaftSysMod
{
public:
    SampleCollector(const char* pModuleName, RaftJsonIF& sysConfig)
        :   RaftSysMod(pModuleName, sysConfig)
    {
    }
    virtual ~SampleCollector()
    {
    }

    // Set sampling info
    void setSamplingInfo(uint32_t sampleRateHz, const char* pSampleHeader,
            const char* sampleAPIName, uint32_t maxSamples, bool allocateAtStart,
            bool dumpWhenFull)
    {
        _sampleRateHz = sampleRateHz;
        _sampleHeader = pSampleHeader;
        _sampleAPIName = sampleAPIName;
        _maxSamples = maxSamples;
        if (allocateAtStart)
            _sampleBuffer.reserve(_maxSamples);
        if (_sampleRateHz > 0)
            _minTimeBetweenSamplesUs = 1000000 / _sampleRateHz;
        _dumpWhenFull = dumpWhenFull;
    }

    // Add sample
    bool addSample(const T& sample)
    {
        // Check if sampling enabled
        if (!_samplingEnabled)
            return false;

        // Check if buffer full
        if (_sampleBuffer.size() >= _maxSamples)
            return false;

        // Check time since last sample
        uint64_t timeNowUs = micros();
        if ((_minTimeBetweenSamplesUs != 0) && (Raft::isTimeout(timeNowUs, _timeSinceLastSampleUs, _minTimeBetweenSamplesUs)))
            return false;

        // // Check previous value
        // if (_sampleBuffer.size() > 0)
        // {
        //     // Get previous value
        //     T prevValue = _sampleBuffer.back()._sampleValue;

        //     // Check if same as previous value
        //     if (abs((int64_t)sample - (int64_t)prevValue) < 1000)
        //         return false;
        // }

        // Create sample
        SampleType sampleWrapped = { (uint32_t)(timeNowUs - _timeSinceLastSampleUs), sample };

        // Add sample to buffer
        _sampleBuffer.push_back(sampleWrapped);

        // Update time since last sample
        _timeSinceLastSampleUs = timeNowUs;

        // Check if buffer full
        if (_sampleBuffer.size() >= _maxSamples)
        {
            // Dump to file
            if (_dumpWhenFull)
            {
                writeToConsole();
                _sampleBuffer.clear();
            }
        }

        return true;
    }

protected:

    // Setup
    virtual void setup() override final
    {}

    // Loop - called frequently
    virtual void loop() override final
    {}

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final
    {
        // Add endpoint for sampling
        pEndpoints.addEndpoint(_sampleAPIName.c_str(), 
                RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET,
                            std::bind(&SampleCollector<T>::apiSample, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                            "handle samples, e.g. sample/start, sample/stop, sample/clear, sample/write/<filename>");
    }

private:
    // Sample API name
    String _sampleAPIName;

    // Sample info
    uint32_t _sampleRateHz = 0;
    String _sampleHeader;
    uint32_t _maxSamples = 0;
    bool _dumpWhenFull = false;

    // Time since last sample
    uint64_t _timeSinceLastSampleUs = 0;
    uint64_t _minTimeBetweenSamplesUs = 0;

    // Enable/disable sampling
    bool _samplingEnabled = true;

    // Sample type
    class SampleType
    {
    public:
        uint32_t _sampleTimeDiffUs = 0;
        T _sampleValue;
    };

    // Sample buffer
    std::vector<SampleType, SpiramAwareAllocator<SampleType>> _sampleBuffer;

    // API
    RaftRetCode apiSample(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
    {
        // Extract params
        std::vector<String> params;
        std::vector<RaftJson::NameValuePair> nameValues;
        RestAPIEndpointManager::getParamsAndNameValues(reqStr.c_str(), params, nameValues);
        String paramsJSON = RaftJson::getJSONFromNVPairs(nameValues, true);

        // Handle commands
        bool rslt = true;
        String rsltStr;
        if (params.size() > 0)
        {
            // Start
            if (params[1].equalsIgnoreCase("start"))
            {
                _samplingEnabled = true;
                rsltStr = "Ok";
            }
            // Stop
            else if (params[1].equalsIgnoreCase("stop"))
            {
                _samplingEnabled = false;
                rsltStr = "Ok";
            }
            // Clear buffer
            else if (params[1].equalsIgnoreCase("clear"))
            {
                _sampleBuffer.clear();
                rsltStr = "Ok";
            }
            // Write to file
            else if (params[1].equalsIgnoreCase("write"))
            {
                rslt = writeToFile(params[2], rsltStr);
            }
        }
        // Result
        if (rslt)
        {
            // LOG_I(MODULE_PREFIX, "apiSample: reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
            return Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
        }
        // LOG_E(MODULE_PREFIX, "apiSample: FAILED reqStr %s rslt %s", reqStr.c_str(), rsltStr.c_str());
        return Raft::setJsonErrorResult(reqStr.c_str(), respStr, rsltStr.c_str());
    }

    // Write to file
    bool writeToFile(const String& filename, String& errMsg)
    {
        // Open file
        FILE* pFile = fileSystem.fileOpen("", filename, true, 0);
        if (!pFile)
        {
            errMsg = "failOpen";
            return false;
        }

        // Write signature code
        const char* pSignatureCode = "RAFTSAMPLES";
        fileSystem.fileWrite(pFile, (uint8_t*)pSignatureCode, strlen(pSignatureCode));
        
        // Write header length (4 bytes)
        uint32_t headerLen = _sampleHeader.length();
        fileSystem.fileWrite(pFile, (uint8_t*)&headerLen, 4);

        // Write header
        fileSystem.fileWrite(pFile, (uint8_t*)_sampleHeader.c_str(), headerLen);

        // Write sample size (4 bytes)
        uint32_t sampleSize = sizeof(SampleType);
        fileSystem.fileWrite(pFile, (uint8_t*)&sampleSize, 4);

        // Write sample rate (4 bytes)
        fileSystem.fileWrite(pFile, (uint8_t*)&_sampleRateHz, 4);

        // Write number of samples (4 bytes)
        uint32_t numSamples = _sampleBuffer.size();
        fileSystem.fileWrite(pFile, (uint8_t*)&numSamples, 4);

        // Write samples
        bool rslt = true;
        uint32_t bytesWritten = fileSystem.fileWrite(pFile, (uint8_t*)_sampleBuffer.data(), numSamples * sizeof(SampleType));
        if (bytesWritten != numSamples * sizeof(SampleType))
        {
            errMsg = "failWrite";
            rslt = false;
        }

        // Close file
        fileSystem.fileClose(pFile, "", filename, true);

        // Clear buffer
        _sampleBuffer.clear();

        // Return
        return rslt;
    }

    void writeToConsole()
    {
        // Write header
        LOG_I("S", "SampleCollector: %s", _sampleHeader.c_str());

        // Write samples
        for (uint32_t i = 0; i < _sampleBuffer.size(); i++)
        {
            LOG_I("S", "%d %d", _sampleBuffer[i]._sampleTimeDiffUs, _sampleBuffer[i]._sampleValue);
        }
    }
};
