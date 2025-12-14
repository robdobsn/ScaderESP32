// ScaderLEDPixelsCustom.cpp
// Custom LEDPixels implementation that uses SimpleRMTLeds driver instead of ESP32RMTLedStrip
// Rob Dobson 2024

#include "ScaderLEDPixelsCustom.h"
#include "RaftUtils.h"
#include "RaftJsonIF.h"
#include "Logger.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScaderLEDPixelsCustom::ScaderLEDPixelsCustom()
{
}

ScaderLEDPixelsCustom::~ScaderLEDPixelsCustom()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderLEDPixelsCustom::setup(const RaftJsonIF& config)
{
    // LED pixels config
    LEDPixelConfig ledPixelConfig;
    if (!ledPixelConfig.setup(config))
    {
        LOG_E(MODULE_PREFIX, "setup failed to get LED pixel config");
        return false;
    }

    // Setup
    return setup(ledPixelConfig);
}

bool ScaderLEDPixelsCustom::setup(LEDPixelConfig& config)
{
    // Setup pixels
    _pixels.resize(config.totalPixels);

    // Setup hardware drivers using SimpleRMTLedsWrapper
    _ledStripDrivers.resize(config.stripConfigs.size());
    bool rslt = false;
    uint32_t pixelCount = 0;
    for (uint32_t ledStripIdx = 0; ledStripIdx < _ledStripDrivers.size(); ledStripIdx++)
    {
        rslt = _ledStripDrivers[ledStripIdx].setup(config.stripConfigs[ledStripIdx], pixelCount);
        if (!rslt)
            break;
        pixelCount += config.stripConfigs[ledStripIdx].numPixels;
    }

    // Check if any segments are specified, if not create a single segment from the entire pixel array
    if (config.segmentConfigs.size() == 0)
    {
        LEDSegmentConfig segCfg;
        segCfg.startOffset = 0;
        segCfg.numPixels = config.totalPixels;
        segCfg.name = "All";
        segCfg.pixelBrightnessFactor = config.globalBrightnessFactor;
        _segments.resize(1);
        _segments[0].setNamedValueProvider(_pDefaultNamedValueProvider, true);
        _segments[0].setup(segCfg, &_pixels, &_ledPatterns);
    }
    else
    {
        // Setup segments
        _segments.resize(config.segmentConfigs.size());

        // If there is exactly one segment and it does not specify a numPixels, assume it covers all pixels
        if ((config.segmentConfigs.size() == 1) && (config.segmentConfigs[0].numPixels == 0))
        {
            config.segmentConfigs[0].numPixels = config.totalPixels;
        }
        for (uint32_t segIdx = 0; segIdx < _segments.size(); segIdx++)
        {
            _segments[segIdx].setNamedValueProvider(_pDefaultNamedValueProvider, true);
            _segments[segIdx].setup(config.segmentConfigs[segIdx], &_pixels, &_ledPatterns);
        }
    }

    // Log
    LOG_I(MODULE_PREFIX, "setup %s numStrips %d numSegments %d totalPixels %d (using SimpleRMTLeds)", 
                rslt ? "OK" : "FAILED",
                (int)_ledStripDrivers.size(), 
                (int)_segments.size(),
                config.totalPixels);
    return rslt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixelsCustom::loop()
{
    // Loop over LED strips
    for (auto& ledStrip : _ledStripDrivers)
    {
        ledStrip.loop();
    }

    // Loop over segments
    for (auto& segment : _segments)
    {
        if (segment.loop())
        {
            show();
        }
        if (segment.isStopRequested())
        {
            segment.stopPattern(true);
            show();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add a pattern
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixelsCustom::addPattern(const String& patternName, LEDPatternCreateFn createFn)
{
    // Check for existing pattern with same name and remove if so
    for (auto it = _ledPatterns.begin(); it != _ledPatterns.end(); ++it)
    {
        if ((*it).name.equalsIgnoreCase(patternName))
        {
            _ledPatterns.erase(it);
            break;
        }
    }

    // Add the pattern
    LEDPatternBase::LEDPatternListItem item;
    item.name = patternName;
    item.createFn = createFn;
    _ledPatterns.push_back(item);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get pattern names
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixelsCustom::getPatternNames(std::vector<String>& patternNames) const
{
    for (const auto& pattern : _ledPatterns)
    {
        patternNames.push_back(pattern.name);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get segment index from name
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32_t ScaderLEDPixelsCustom::getSegmentIdx(const String& segmentName) const
{
    for (uint32_t i = 0; i < _segments.size(); i++)
    {
        if (_segments[i].getName().equalsIgnoreCase(segmentName))
            return i;
    }
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear all pixels
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixelsCustom::clear(bool showAfterClear)
{
    // Clear all segments
    for (auto& segment : _segments)
    {
        segment.clear();
    }

    // Show
    if (showAfterClear)
        show();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Show pixels
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScaderLEDPixelsCustom::show()
{
    // Call show callback if set
    if (_showCB)
    {
        for (uint32_t segmentIdx = 0; segmentIdx < _segments.size(); segmentIdx++)
        {
            _showCB(segmentIdx, false, _pixels);
        }
    }

    // Show on all strips
    for (auto& ledStrip : _ledStripDrivers)
    {
        ledStrip.showPixels(_pixels);
    }

    // Call post-show callback if set
    if (_showCB)
    {
        for (uint32_t segmentIdx = 0; segmentIdx < _segments.size(); segmentIdx++)
        {
            _showCB(segmentIdx, true, _pixels);
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait until show complete
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScaderLEDPixelsCustom::waitUntilShowComplete()
{
    for (auto& ledStrip : _ledStripDrivers)
    {
        ledStrip.waitUntilShowComplete();
    }
}
